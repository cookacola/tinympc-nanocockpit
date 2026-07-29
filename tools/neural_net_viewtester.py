#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#  NanoCockpit AI-deck neural-net view tester.
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
import importlib
import importlib.util
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

parser = argparse.ArgumentParser(description="View the NanoCockpit AI-deck stream for laptop neural-net testing")
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
parser.add_argument("--nn-module", default=os.path.join(_HERE, "stdc_shared_onnx_adapter.py"),
                    help="Python module or .py file with predict(frame, metadata, tof_frame, model=None)")
parser.add_argument("--nn-weights", default=None,
                    help="optional model/checkpoint path passed to module load_model(path)")
parser.add_argument("--send-nn-output", action="store_true",
                    help="send result['reply'] or result['network_output'] back to the deck")
parser.add_argument("--view", choices=("heatmaps", "overlay", "both"), default="heatmaps",
                    help="display mode: raw NN heatmaps, postprocessed overlay, or both")
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


def load_nn_runner(module_name_or_path, weights_path):
    if not module_name_or_path:
        return None

    if module_name_or_path.endswith(".py") or os.path.exists(module_name_or_path):
        path = os.path.abspath(module_name_or_path)
        name = os.path.splitext(os.path.basename(path))[0]
        spec = importlib.util.spec_from_file_location(name, path)
        if spec is None or spec.loader is None:
            raise RuntimeError(f"Could not load NN module from {path}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    else:
        module = importlib.import_module(module_name_or_path)

    predict = getattr(module, "predict", None)
    if predict is None:
        raise RuntimeError("NN module must define predict(frame, metadata, tof_frame, model=None)")

    load_model = getattr(module, "load_model", None)
    model = load_model(weights_path) if load_model is not None else None
    return predict, model


def run_nn(nn_runner, gray, metadata, tof_frame):
    if nn_runner is None:
        return None
    predict, model = nn_runner
    start = time.perf_counter()
    result = predict(gray, metadata, tof_frame, model=model)
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    if result is None:
        result = {}
    if not isinstance(result, dict):
        result = {"network_output": result}
    result.setdefault("inference_ms", elapsed_ms)
    return result


def reply_from_result(result):
    if not result:
        return None
    output = result.get("reply", result.get("network_output"))
    if output is None:
        return None
    values = np.asarray(output, dtype=np.float32).reshape(-1)
    if values.size < 4:
        raise ValueError("NN reply/network_output must contain at least 4 floats")
    return values[:4].tolist()


def draw_nn_overlay(disp, result):
    if not result:
        cv2.putText(disp, "NN: not loaded", (4, 32), cv2.FONT_HERSHEY_SIMPLEX,
                    0.45, (180, 180, 180), 1, cv2.LINE_AA)
        return

    draw_region_overlay(disp, result)

    dangerous = result.get("dangerous")
    confidence = result.get("confidence")
    label = result.get("label")
    if label is None:
        if dangerous is True:
            label = "danger"
        elif dangerous is False:
            label = "safe"
        else:
            label = "NN"
    color = (0, 0, 255) if dangerous is True else (0, 220, 0)
    if confidence is None:
        text = f"{label}  {result.get('inference_ms', 0.0):.1f} ms"
    else:
        text = f"{label} {float(confidence):.2f}  {result.get('inference_ms', 0.0):.1f} ms"
    range_m = result.get("range_m")
    if range_m is not None:
        text += f"  {float(range_m):.2f}m"
    cv2.putText(disp, text, (4, 32), cv2.FONT_HERSHEY_SIMPLEX,
                0.45, color, 1, cv2.LINE_AA)

    plane_points = result.get("plane_points_px", result.get("corners_px"))
    if plane_points is not None:
        points = np.asarray(plane_points, dtype=np.int32).reshape(-1, 2)
        if len(points) >= 2:
            cv2.polylines(disp, [points], isClosed=len(points) > 2, color=(255, 200, 0),
                          thickness=1, lineType=cv2.LINE_AA)

    normal = result.get("plane_normal")
    if normal is not None:
        try:
            nx, ny, nz = [float(v) for v in np.asarray(normal).reshape(-1)[:3]]
            cv2.putText(disp, f"plane n=({nx:+.2f},{ny:+.2f},{nz:+.2f})",
                        (4, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.4,
                        (255, 200, 0), 1, cv2.LINE_AA)
        except (TypeError, ValueError):
            pass

    max_danger = result.get("max_danger")
    min_ttc = result.get("min_ttc_s")
    if max_danger is not None:
        extra = f"danger max={float(max_danger):.2f}"
        if min_ttc is not None:
            extra += f"  ttc={float(min_ttc):.2f}s"
        cv2.putText(disp, extra, (4, 68), cv2.FONT_HERSHEY_SIMPLEX, 0.4,
                    (0, 220, 255), 1, cv2.LINE_AA)


def build_display(gray, metadata, result):
    if args.view == "overlay":
        disp = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
        draw_metadata(disp, metadata)
        draw_nn_overlay(disp, result)
        return disp

    heatmaps = draw_heatmap_view(gray, metadata, result)
    if args.view == "heatmaps":
        return heatmaps

    overlay = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
    draw_metadata(overlay, metadata)
    draw_nn_overlay(overlay, result)
    # Keep the camera's native aspect ratio. The heatmap grid is three panels
    # wide, so pad the overview row instead of stretching it to that width.
    overview = np.zeros((gray.shape[0], heatmaps.shape[1], 3), dtype=np.uint8)
    x0 = max(0, (overview.shape[1] - overlay.shape[1]) // 2)
    overview[:, x0:x0 + overlay.shape[1]] = overlay
    return np.vstack([overview, heatmaps])


def draw_metadata(disp, metadata):
    if metadata is None:
        return
    txt = (f"#{metadata.frame_id}  "
           f"X{metadata.state.x/1000:+.2f} "
           f"Y{metadata.state.y/1000:+.2f} "
           f"Z{metadata.state.z/1000:+.2f}m")
    cv2.putText(disp, txt, (4, 14), cv2.FONT_HERSHEY_SIMPLEX,
                0.4, (0, 255, 0), 1, cv2.LINE_AA)


def draw_heatmap_view(gray, metadata, result):
    raw = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
    height, width = gray.shape[:2]
    draw_metadata(raw, metadata)
    _label_panel(raw, _summary_label(result), color=(255, 255, 255))
    _draw_corner_points(raw, result)

    heatmaps = {} if not result else result.get("heatmaps", {}) or {}
    panels = [
        raw,
        _heatmap_panel(heatmaps.get("corner_max_40"), "corner max", cv2.COLORMAP_TURBO, width, height),
        _heatmap_panel(heatmaps.get("gate_40"), "gate opening", cv2.COLORMAP_VIRIDIS, width, height),
        _heatmap_panel(heatmaps.get("danger_20"), "danger", cv2.COLORMAP_TURBO, width, height),
        _heatmap_panel(heatmaps.get("inverse_range_20"), "inverse range", cv2.COLORMAP_TURBO, width, height),
        _heatmap_panel(heatmaps.get("uncertainty_20"), "uncertainty", cv2.COLORMAP_MAGMA, width, height),
    ]
    return np.vstack([np.hstack(panels[:3]), np.hstack(panels[3:])])


def _summary_label(result):
    if not result:
        return "NN not loaded"
    label = result.get("label", "NN")
    confidence = result.get("confidence")
    max_danger = result.get("max_danger")
    text = str(label)
    if confidence is not None:
        text += f"  corner={float(confidence):.2f}"
    if max_danger is not None:
        text += f"  danger={float(max_danger):.2f}"
    text += f"  {result.get('inference_ms', 0.0):.1f} ms"
    return text


def _heatmap_panel(values, label, colormap, width, height):
    if values is None:
        panel = np.zeros((height, width, 3), dtype=np.uint8)
        _label_panel(panel, f"{label}: none", color=(180, 180, 180))
        return panel

    arr = np.asarray(values, dtype=np.float32)
    if arr.ndim == 3:
        arr = np.max(arr, axis=0)
    arr = np.nan_to_num(arr, nan=0.0, posinf=1.0, neginf=0.0)
    clipped = np.clip(arr, 0.0, 1.0)
    scaled = (clipped * 255.0).astype(np.uint8)
    scaled = cv2.resize(scaled, (width, height), interpolation=cv2.INTER_NEAREST)
    panel = cv2.applyColorMap(scaled, colormap)
    _label_panel(panel, f"{label}  max={float(np.max(clipped)):.2f}", color=(255, 255, 255))
    return panel


def _label_panel(panel, label, color=(255, 255, 255)):
    cv2.rectangle(panel, (0, 0), (panel.shape[1], 18), (0, 0, 0), -1)
    cv2.putText(panel, label[:35], (4, 13), cv2.FONT_HERSHEY_SIMPLEX,
                0.36, color, 1, cv2.LINE_AA)


def _draw_corner_points(panel, result):
    if not result:
        return
    points = result.get("corners_px")
    scores = result.get("corner_scores")
    if points is None:
        return
    pts = np.asarray(points, dtype=np.int32).reshape(-1, 2)
    scores_arr = None if scores is None else np.asarray(scores, dtype=np.float32).reshape(-1)
    for idx, (x, y) in enumerate(pts):
        score = 0.0 if scores_arr is None or idx >= scores_arr.size else float(scores_arr[idx])
        color = (0, 255, 255) if score >= 0.20 else (0, 120, 255)
        cv2.drawMarker(panel, (int(x), int(y)), color, markerType=cv2.MARKER_CROSS,
                       markerSize=10, thickness=1, line_type=cv2.LINE_AA)


def draw_region_overlay(disp, result):
    unsafe = result.get("unsafe_mask")
    safe = result.get("safe_mask")
    if unsafe is None and safe is None:
        return

    overlay = disp.copy()
    if unsafe is not None:
        unsafe_arr = np.asarray(unsafe, dtype=np.float32)
        if unsafe_arr.shape[:2] != disp.shape[:2]:
            unsafe_arr = cv2.resize(unsafe_arr, (disp.shape[1], disp.shape[0]), interpolation=cv2.INTER_LINEAR)
        threshold = float(result.get("danger_threshold", 0.35))
        mask = unsafe_arr >= threshold
        overlay[mask] = (0, 0, 255)

    if safe is not None:
        safe_arr = np.asarray(safe, dtype=np.float32)
        if safe_arr.shape[:2] != disp.shape[:2]:
            safe_arr = cv2.resize(safe_arr, (disp.shape[1], disp.shape[0]), interpolation=cv2.INTER_NEAREST)
        mask = safe_arr > 0.5
        overlay[mask] = (0, 180, 0)

    cv2.addWeighted(overlay, 0.28, disp, 0.72, 0, dst=disp)


def main():
    client = StreamerClient(host=args.n, port=args.p, udp_send=args.udp_send)
    nn_runner = load_nn_runner(args.nn_module, args.nn_weights)
    if nn_runner is None:
        print("NN module not provided; streaming frames without laptop inference.")
    else:
        print(f"Loaded NN module: {args.nn_module}")

    rx = threading.Thread(target=receiver, args=(client,), daemon=True)
    rx.start()

    save_dir = os.path.join(_HERE, "stream_out")
    count = 0
    if args.save:
        os.makedirs(save_dir, exist_ok=True)
        existing = [f for f in os.listdir(save_dir)
                    if f.startswith("img_") and f.endswith(".png")]
        count = len(existing)

    win = "NanoCockpit neural-net viewtester"
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
            nn_result = run_nn(nn_runner, gray, metadata, tof_frame)

            try:
                network_output = reply_from_result(nn_result) if args.send_nn_output else None
                client.send_reply(metadata, network_output)
            except Exception as e:  # noqa: BLE001 - keep the viewer alive on bad model output
                print(f"Could not send NN reply: {e}", flush=True)
                client.send_reply(metadata, None)

            if args.save:
                count += 1
                cv2.imwrite(os.path.join(save_dir, f"img_{count:06d}.png"), gray)

            shown += 1
            if not args.no_display:
                now = time.time()
                if args.render_fps <= 0 or (now - last_render) >= 1.0 / args.render_fps:
                    last_render = now
                    disp = build_display(gray, metadata, nn_result)
                    cv2.imshow(win, disp)
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        break
            else:
                if shown == 1 or shown % 30 == 0:
                    fid = metadata.frame_id if metadata is not None else "?"
                    nn_text = ""
                    if nn_result:
                        nn_text = (f"  nn={nn_result.get('label', nn_result.get('dangerous', '?'))}"
                                   f"  {nn_result.get('inference_ms', 0.0):.1f}ms")
                    print(f"  frame {shown}  id={fid}  shape={frame.shape}"
                          + nn_text
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
