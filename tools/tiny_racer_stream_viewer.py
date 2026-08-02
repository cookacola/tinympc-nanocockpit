#!/usr/bin/env python3
"""Display the annotated video stream from the Tiny Racer GAP8 firmware.

The sequential model runs on GAP8 and the firmware sends its annotated
grayscale 160x160 frame through the NanoCockpit CPX streamer. The four legacy
inference floats temporarily contain the four gate-corner positions as
``y * frame_width + x`` in TL, TR, BR, BL order. Metadata version 12 also
carries fixed-normal clearance/confidence summaries and input/output CRCs.

This viewer does not run a neural network locally and never sends inference
back to the Crazyflie.  It only returns the normal per-frame streamer reply so
the firmware can measure round-trip time.
"""

import argparse
import csv
import sys
from pathlib import Path

import cv2
import numpy as np


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "src/client/aideck_cpx_streamer"))

from aideck_cpx_streamer.cpx import StreamerClient  # noqa: E402


CORNER_NAMES = ("TL", "TR", "BR", "BL")
CORNER_COLORS = (
    (0, 255, 255),
    (255, 255, 0),
    (255, 0, 255),
    (0, 255, 0),
)


def decode_packed_corners(metadata, width, height):
    """Return Tiny Racer's four encoded corners, or ``None`` if absent."""
    if metadata is None:
        return None

    values = (
        metadata.inference.x,
        metadata.inference.y,
        metadata.inference.z,
        metadata.inference.phi,
    )
    if not all(np.isfinite(value) for value in values):
        return None

    # An untouched metadata structure is all zeroes.  A real four-corner
    # detection cannot encode four positions as zero simultaneously.
    if not any(values):
        return None

    corners = []
    for value in values:
        encoded = int(round(value))
        if encoded < 0 or encoded >= width * height:
            return None
        corners.append((encoded % width, encoded // width))
    return corners


def sequential_values(metadata):
    """Return the sequential summary, or ``None`` for a v10 legacy stream."""
    sequential = getattr(metadata, "sequential", None)
    if sequential is None:
        return None
    return {
        "gate_valid": bool(sequential.gate_valid),
        "input_crc32": getattr(sequential, "input_crc32", None),
        "output_crc32": getattr(sequential, "output_crc32", None),
        "corner_peak_scores": tuple(
            float(value) for value in sequential.corner_peak_scores
        ),
        "corner_ambiguity": tuple(
            float(value) for value in sequential.corner_ambiguity
        ),
        "clearance_m": tuple(float(value) for value in sequential.clearance_m),
        "clearance_confidence": tuple(
            float(value) for value in sequential.clearance_confidence
        ),
    }


def draw_sequential_summary(display, height, summary):
    """Draw the sequential summary without obscuring the gate overlay."""
    if summary is None:
        return
    clearance = "/".join(f"{value:.2f}" for value in summary["clearance_m"])
    confidence = "/".join(
        f"{value:+.2f}" for value in summary["clearance_confidence"]
    )
    cv2.putText(display, f"clearance m: {clearance}", (4, height - 20),
                cv2.FONT_HERSHEY_SIMPLEX, 0.34, (255, 255, 255), 1,
                cv2.LINE_AA)
    cv2.putText(display, f"confidence: {confidence}", (4, height - 6),
                cv2.FONT_HERSHEY_SIMPLEX, 0.34, (190, 190, 190), 1,
                cv2.LINE_AA)
    if summary["input_crc32"] is not None:
        cv2.putText(display, "crc in/out: %08x/%08x" % (
            summary["input_crc32"], summary["output_crc32"]),
            (4, height - 36), cv2.FONT_HERSHEY_SIMPLEX, 0.29,
            (190, 190, 190), 1, cv2.LINE_AA)


def annotate_frame(frame, metadata):
    """Add host-side labels and decoded corner markers to a streamed frame."""
    if frame.dtype != np.uint8:
        frame = cv2.normalize(frame, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    display = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
    height, width = frame.shape[:2]

    cv2.rectangle(display, (0, 0), (width, 20), (0, 0, 0), -1)
    frame_id = "?" if metadata is None else metadata.frame_id
    cv2.putText(display, f"Tiny Racer onboard overlay  frame {frame_id}",
                (4, 14), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (255, 255, 255), 1,
                cv2.LINE_AA)

    summary = sequential_values(metadata) if metadata is not None else None
    corners = decode_packed_corners(metadata, width, height)
    if summary is not None and not summary["gate_valid"]:
        corners = None
    if corners is None:
        cv2.putText(display, "corners: unavailable", (4, height - 52),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.40, (160, 160, 160), 1,
                    cv2.LINE_AA)
        draw_sequential_summary(display, height, summary)
        return display, None, summary

    polygon = np.asarray(corners, dtype=np.int32).reshape((-1, 1, 2))
    cv2.polylines(display, [polygon], True, (255, 255, 255), 1, cv2.LINE_AA)
    for name, color, point in zip(CORNER_NAMES, CORNER_COLORS, corners):
        cv2.drawMarker(display, point, color, markerType=cv2.MARKER_CROSS,
                       markerSize=10, thickness=1, line_type=cv2.LINE_AA)
        cv2.putText(display, name, (point[0] + 4, point[1] - 4),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.35, color, 1, cv2.LINE_AA)
    draw_sequential_summary(display, height, summary)
    return display, corners, summary


def parse_args():
    parser = argparse.ArgumentParser(
        description="Display annotated Tiny Racer frames sent by the AI-deck")
    parser.add_argument("-n", "--host", default="192.168.4.1",
                        help="AI-deck IP address (default: %(default)s)")
    parser.add_argument("-p", "--port", type=int, default=5000,
                        help="AI-deck CPX port (default: %(default)s)")
    parser.add_argument("--no-udp-send", action="store_false", dest="udp_send",
                        help="send the normal streamer reply through TCP instead")
    parser.add_argument("--save-dir", type=Path, default=None,
                        help="save annotated PNG frames and corners.csv here")
    parser.add_argument("--no-display", action="store_true",
                        help="do not open a window; use with --frames or Ctrl-C")
    parser.add_argument("--frames", type=int, default=0,
                        help="stop after this many frames (0 means run until Ctrl-C)")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.frames < 0:
        raise SystemExit("--frames must be non-negative")

    csv_file = None
    csv_writer = None
    if args.save_dir is not None:
        args.save_dir.mkdir(parents=True, exist_ok=True)
        csv_file = (args.save_dir / "corners.csv").open("w", newline="",
                                                         encoding="utf-8")
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(("frame", "frame_timestamp_us", "gate_valid",
                             "tl_x", "tl_y", "tr_x", "tr_y", "br_x", "br_y",
                             "bl_x", "bl_y", "clearance_0_m", "clearance_1_m",
                             "clearance_2_m", "clearance_3_m", "confidence_0",
                             "confidence_1", "confidence_2", "confidence_3",
                             "corner_peak_0", "corner_peak_1", "corner_peak_2",
                             "corner_peak_3", "corner_ambiguity_0",
                             "corner_ambiguity_1", "corner_ambiguity_2",
                             "corner_ambiguity_3", "input_crc32", "output_crc32"))

    client = StreamerClient(host=args.host, port=args.port,
                            udp_send=args.udp_send)
    shown = 0
    try:
        for frame, _tof_frame, metadata in client.receive():
            # This carries no offboard inference. It only acknowledges this
            # frame so GAP8 can retain its round-trip timing statistics.
            client.send_reply(metadata, None)

            display, corners, summary = annotate_frame(frame, metadata)
            shown += 1

            if csv_writer is not None:
                row = [shown, metadata.frame_timestamp,
                       "" if summary is None else int(summary["gate_valid"])]
                if corners is None:
                    row.extend(("",) * 8)
                else:
                    row.extend(np.asarray(corners).flat)
                if summary is None:
                    row.extend(("",) * 18)
                else:
                    row.extend(summary["clearance_m"])
                    row.extend(summary["clearance_confidence"])
                    row.extend(summary["corner_peak_scores"])
                    row.extend(summary["corner_ambiguity"])
                    row.extend((summary["input_crc32"], summary["output_crc32"]))
                csv_writer.writerow(row)
                csv_file.flush()
                cv2.imwrite(str(args.save_dir / f"frame_{shown:06d}.png"), display)

            if args.no_display:
                if shown == 1 or shown % 30 == 0:
                    print(f"frame {shown}: {frame.shape}, corners={corners}")
            else:
                cv2.imshow("Tiny Racer stream", display)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break

            if args.frames and shown >= args.frames:
                break
    except KeyboardInterrupt:
        pass
    finally:
        client.shutdown()
        if csv_file is not None:
            csv_file.close()
        if not args.no_display:
            cv2.destroyAllWindows()
        print(f"Done. Received {shown} frames.")


if __name__ == "__main__":
    main()
