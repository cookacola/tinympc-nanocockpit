#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#  NanoCockpit AI-deck stream viewer.
#
#  Adapted from Bitcraze's esp_color_object/tools/color_stream_viewer.py, but the
#  frame-parsing guts are replaced: the original speaks the wifi-img-streamer
#  protocol (0xBC image header + JPEG framed by SOF/EOF), which is NOT what the
#  NanoCockpit NINA firmware sends. NanoCockpit uses the STREAMER CPX protocol
#  (BUFFER_BEGIN/DATA, CRC32, raw grayscale pixels + pose/ToF metadata), so this
#  viewer drives the maintained `aideck_cpx_streamer.StreamerClient` instead and
#  keeps the original's UX: a receiver thread feeding a freshest-frame queue,
#  --save, --no-display headless capture, --render-fps cap.
#
#  Run plt_viewer for the upstream minimal viewer; this one mirrors the
#  color_stream_viewer ergonomics.

import argparse
import os
import sys
import threading
import queue
import time

import numpy as np
import cv2

# Import the NanoCockpit client straight from the source tree, so this works
# whether or not the package is pip-installed.
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_HERE, "..", "src/client/aideck_cpx_streamer"))
from aideck_cpx_streamer.cpx import StreamerClient  # noqa: E402

parser = argparse.ArgumentParser(description="View the NanoCockpit AI-deck stream")
parser.add_argument("-n", default="192.168.4.1", metavar="ip", help="AI-deck IP (AP mode default)")
parser.add_argument("-p", type=int, default=5000, metavar="port", help="AI-deck port")
parser.add_argument("--save", action="store_true", help="save streamed frames to stream_out/")
parser.add_argument("--no-udp-send", action="store_false", dest="udp_send",
                    help="don't send replies over UDP (RTT measurement)")
parser.add_argument("--render-fps", type=float, default=0.0,
                    help="cap on-screen refresh rate (0 = unlimited); lower it if the "
                         "link backs up, the receiver still drains every frame")
parser.add_argument("--no-display", action="store_true",
                    help="headless: don't open a window. Use with --save over SSH; stop with Ctrl-C")
parser.add_argument("--frames", type=int, default=0,
                    help="exit after N frames (0 = run until Ctrl-C); handy for scripted checks")
args = parser.parse_args()

# Freshest-frame hand-off: maxsize=1 so the display always gets the newest frame
# and a slow renderer drops stale frames instead of building latency.
frame_queue = queue.Queue(maxsize=1)
stop_event = threading.Event()


def push_frame(item):
    try:
        frame_queue.put_nowait(item)
    except queue.Full:
        try:
            frame_queue.get_nowait()
        except queue.Empty:
            pass
        try:
            frame_queue.put_nowait(item)
        except queue.Full:
            pass


def receiver(client):
    """Thread 1: pull decoded (frame, tof, metadata) from StreamerClient."""
    try:
        for frame, tof_frame, metadata in client.receive():
            if stop_event.is_set():
                break
            # Reply so the deck can compute round-trip time (matches plt_viewer).
            client.send_reply(metadata, None)
            push_frame((frame, tof_frame, metadata))
    except Exception as e:  # noqa: BLE001 - surface any stream/socket error, then stop
        if not stop_event.is_set():
            print(f"Stream ended: {e}")
    finally:
        stop_event.set()
        push_frame(None)  # unblock the consumer


def to_display_gray(frame):
    """NanoCockpit frames are raw grayscale (uint8, or uint16 for >8bpp)."""
    if frame.dtype != np.uint8:
        frame = cv2.normalize(frame, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    return frame


def main():
    client = StreamerClient(host=args.n, port=args.p, udp_send=args.udp_send)

    rx = threading.Thread(target=receiver, args=(client,), daemon=True)
    rx.start()

    save_dir = os.path.join(_HERE, "stream_out")
    count = 0
    if args.save:
        os.makedirs(save_dir, exist_ok=True)
        existing = [f for f in os.listdir(save_dir)
                    if f.startswith("img_") and f.endswith(".png")]
        count = len(existing)

    win = "NanoCockpit stream"
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
            frame, tof_frame, metadata = item
            gray = to_display_gray(frame)

            if args.save:
                count += 1
                cv2.imwrite(os.path.join(save_dir, f"img_{count:06d}.png"), gray)

            shown += 1
            if not args.no_display:
                now = time.time()
                if args.render_fps <= 0 or (now - last_render) >= 1.0 / args.render_fps:
                    last_render = now
                    disp = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
                    if metadata is not None:
                        txt = (f"#{metadata.frame_id}  "
                               f"X{metadata.state.x/1000:+.2f} "
                               f"Y{metadata.state.y/1000:+.2f} "
                               f"Z{metadata.state.z/1000:+.2f}m")
                        cv2.putText(disp, txt, (4, 14), cv2.FONT_HERSHEY_SIMPLEX,
                                    0.4, (0, 255, 0), 1, cv2.LINE_AA)
                    cv2.imshow(win, disp)
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        break
            else:
                if shown == 1 or shown % 30 == 0:
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
