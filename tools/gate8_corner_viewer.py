#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#  gate8_corner_viewer.py
#  NanoCockpit AI-deck stream viewer with LIVE gate8 corner overlay.
#
#  Streams the AI-deck camera over WiFi (same STREAMER CPX transport as
#  color_stream_viewer.py) and, for each frame, runs the *same* trained gate8
#  network on the host (gate8_deploy/deploy_bundle/Frontnet.onnx) to draw the four
#  predicted gate corners on top of the image. This lets you see what the camera
#  sees AND where the net places the corners, without touching GAP8 firmware.
#
#  The host inference path is bit-exact with the GAP8: running the ONNX on the
#  bundle's golden input.txt reproduces out_layer8.txt (verified). The preprocessing
#  and dequant below mirror the firmware (examples/pulp-frontnet/main.c) exactly:
#    - camera 160x160 -> vertical INTER_AREA resize to 160x96 (width unchanged)
#    - net input is the raw 0-255 pixel intensity (eps_in = 1.0, no input bias)
#    - output[i] (int) -> pixel = output[i]*EPS_OUT + BIAS[i], in the 160x96 frame
#    - corner order is IPPE_SQUARE: TL, TR, BR, BL
#
#  Usage:
#    python3 gate8_corner_viewer.py                       # AP mode default 192.168.4.1:5000
#    python3 gate8_corner_viewer.py -n 192.168.4.1 -p 5000
#    python3 gate8_corner_viewer.py --onnx /path/to/Frontnet.onnx
#    python3 gate8_corner_viewer.py --save                # dump overlaid frames to stream_out/
#    python3 gate8_corner_viewer.py --no-display --save    # headless capture (Ctrl-C to stop)
#
#  Deps (host): onnxruntime, numpy, opencv-python, and the aideck_cpx_streamer
#  package (src/client/aideck_cpx_streamer). Verified against onnxruntime 1.27,
#  numpy 2.4, opencv 4.13.

import argparse
import os
import queue
import sys
import threading
import time

import numpy as np

try:
    import cv2
except ImportError:
    sys.exit("opencv-python is required: pip install opencv-python")

try:
    import onnxruntime as ort
except ImportError:
    sys.exit("onnxruntime is required: pip install onnxruntime")

from aideck_cpx_streamer.cpx import StreamerClient  # noqa: E402

_HERE = os.path.dirname(os.path.abspath(__file__))
# gate8_deploy sits beside tinympc-nanocockpit in the workspace.
_DEFAULT_ONNX = os.path.normpath(
    os.path.join(_HERE, "..", "..", "gate8_deploy", "deploy_bundle", "Frontnet.onnx"))

# Net input frame the corners live in (matches firmware main.c IMG_W x IMG_H_NET).
NET_W = 160
NET_H = 96

# Dequant, from gate8_deploy/deploy_bundle/output_dequant.txt (== firmware GATE8_*).
EPS_OUT = 1.61093718e-04
BIAS = np.array([0.767035, 0.421470, 0.818451, 0.712229,
                 0.845009, 0.705781, 0.806453, 0.380487], dtype=np.float64)
# IPPE_SQUARE order of the 4 (x,y) pairs.
CORNER_NAMES = ["TL", "TR", "BR", "BL"]
CORNER_COLORS = [(0, 0, 255), (0, 255, 255), (0, 255, 0), (255, 128, 0)]  # BGR

parser = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("-n", default="192.168.4.1", metavar="ip",
                    help="AI-deck IP (AP mode default 192.168.4.1)")
parser.add_argument("-p", type=int, default=5000, metavar="port", help="AI-deck port")
parser.add_argument("--onnx", default=_DEFAULT_ONNX,
                    help="path to Frontnet.onnx (default: gate8_deploy bundle)")
parser.add_argument("--save", action="store_true",
                    help="save overlaid frames to stream_out/")
parser.add_argument("--no-udp-send", action="store_false", dest="udp_send",
                    help="disable the UDP round-trip reply")
parser.add_argument("--render-fps", type=float, default=0.0,
                    help="cap display refresh rate (0 = uncapped)")
parser.add_argument("--no-display", action="store_true",
                    help="headless: don't open a window (use with --save)")
parser.add_argument("--frames", type=int, default=0, help="stop after N frames (0 = run forever)")
parser.add_argument("--print", action="store_true", dest="print_corners",
                    help="print the 8 corner pixel coords each frame")
args = parser.parse_args()

stop_event = threading.Event()
frame_queue = queue.Queue(maxsize=1)


def push_frame(item):
    """Freshest-frame queue: drop the stale frame so the overlay never lags."""
    try:
        frame_queue.get_nowait()
    except queue.Empty:
        pass
    try:
        frame_queue.put_nowait(item)
    except queue.Full:
        pass


def receiver(client):
    try:
        for frame, tof_frame, metadata in client.receive():
            if stop_event.is_set():
                break
            client.send_reply(metadata, None)
            push_frame((frame, tof_frame, metadata))
    except Exception as e:  # noqa: BLE001
        if not stop_event.is_set():
            print(f"Stream ended: {e}")
    finally:
        stop_event.set()
        push_frame(None)


def to_gray_u8(frame):
    """NanoCockpit frames are raw grayscale (uint8, or uint16 for >8bpp)."""
    if frame.dtype != np.uint8:
        frame = cv2.normalize(frame, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    return frame


def net_input_from(gray):
    """Replicate the firmware preprocessing: width -> 160, then vertical INTER_AREA
    resize of the height to 96. cv2.resize((160,96), INTER_AREA) does both in one
    step (area interpolation matches the firmware's 5-row -> 3-row averaging)."""
    return cv2.resize(gray, (NET_W, NET_H), interpolation=cv2.INTER_AREA)


def run_corners(sess, in_name, gray):
    """gray (any HxW u8) -> 4 (x,y) corner points in the 160x96 net frame."""
    net = net_input_from(gray).astype(np.float32)[None, None, :, :]  # NCHW [1,1,96,160]
    out = sess.run(None, {in_name: net})[0].ravel().astype(np.float64)
    px = out * EPS_OUT + BIAS                     # 8 values, pixels in 160x96
    return px.reshape(4, 2)                        # rows: TL, TR, BR, BL


def draw_overlay(gray, corners_net):
    """Scale net-frame corners to the display frame and draw the gate quad."""
    disp = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
    h, w = gray.shape[:2]
    sx, sy = w / float(NET_W), h / float(NET_H)
    pts = np.array([[c[0] * sx, c[1] * sy] for c in corners_net], dtype=np.int32)
    # Gate outline TL->TR->BR->BL->TL.
    cv2.polylines(disp, [pts.reshape(-1, 1, 2)], isClosed=True, color=(0, 200, 0),
                  thickness=1, lineType=cv2.LINE_AA)
    for i, (x, y) in enumerate(pts):
        cv2.circle(disp, (int(x), int(y)), 3, CORNER_COLORS[i], -1, cv2.LINE_AA)
        cv2.putText(disp, CORNER_NAMES[i], (int(x) + 4, int(y) - 4),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.35, CORNER_COLORS[i], 1, cv2.LINE_AA)
    return disp


def main():
    if not os.path.isfile(args.onnx):
        sys.exit(f"ONNX model not found: {args.onnx}\n"
                 f"Pass --onnx /path/to/Frontnet.onnx")
    sess = ort.InferenceSession(args.onnx, providers=["CPUExecutionProvider"])
    in_name = sess.get_inputs()[0].name
    print(f"gate8 model: {args.onnx}")

    client = StreamerClient(host=args.n, port=args.p, udp_send=args.udp_send)
    rx = threading.Thread(target=receiver, args=(client,), daemon=True)
    rx.start()

    save_dir = os.path.join(_HERE, "stream_out")
    count = 0
    if args.save:
        os.makedirs(save_dir, exist_ok=True)
        count = len([f for f in os.listdir(save_dir)
                     if f.startswith("gate8_") and f.endswith(".png")])

    win = "gate8 corner overlay"
    last_render = 0.0
    shown = 0
    try:
        while not stop_event.is_set():
            try:
                item = frame_queue.get(timeout=1.0)
            except queue.Empty:
                continue
            if item is None:
                break
            frame, _tof, metadata = item
            gray = to_gray_u8(frame)

            corners = run_corners(sess, in_name, gray)
            if args.print_corners:
                flat = " ".join(f"{v:6.1f}" for v in corners.ravel())
                print(f"  corners(px@160x96) {flat}", flush=True)

            shown += 1
            disp = draw_overlay(gray, corners)
            if metadata is not None:
                cv2.putText(disp, f"#{metadata.frame_id}", (4, 14),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1, cv2.LINE_AA)

            if args.save:
                count += 1
                cv2.imwrite(os.path.join(save_dir, f"gate8_{count:06d}.png"), disp)

            if not args.no_display:
                now = time.time()
                if args.render_fps <= 0 or (now - last_render) >= 1.0 / args.render_fps:
                    last_render = now
                    cv2.imshow(win, disp)
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        break
            elif shown == 1 or shown % 30 == 0:
                fid = metadata.frame_id if metadata is not None else "?"
                print(f"  frame {shown}  id={fid}  shape={frame.shape}"
                      + (f"  saved->{count}" if args.save else ""), flush=True)

            if args.frames and shown >= args.frames:
                break
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        client.shutdown()
        if not args.no_display:
            cv2.destroyAllWindows()
        print(f"Done. {shown} frames" + (f", {count} saved to {save_dir}" if args.save else ""))


if __name__ == "__main__":
    main()
