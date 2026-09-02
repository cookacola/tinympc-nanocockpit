#!/usr/bin/env python3
"""Display the annotated video stream from the Tiny Racer GAP8 firmware.

The firmware sends a grayscale 160x160 frame through the NanoCockpit CPX
streamer.  Its 10x8 danger map has already darkened the corresponding image
regions on GAP8.  The four legacy inference floats temporarily contain the
four corner positions as ``y * frame_width + x`` in TL, TR, BR, BL order.

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

    corners = decode_packed_corners(metadata, width, height)
    if corners is None:
        cv2.putText(display, "corners: unavailable", (4, height - 6),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.40, (160, 160, 160), 1,
                    cv2.LINE_AA)
        return display, None

    polygon = np.asarray(corners, dtype=np.int32).reshape((-1, 1, 2))
    cv2.polylines(display, [polygon], True, (255, 255, 255), 1, cv2.LINE_AA)
    for name, color, point in zip(CORNER_NAMES, CORNER_COLORS, corners):
        cv2.drawMarker(display, point, color, markerType=cv2.MARKER_CROSS,
                       markerSize=10, thickness=1, line_type=cv2.LINE_AA)
        cv2.putText(display, name, (point[0] + 4, point[1] - 4),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.35, color, 1, cv2.LINE_AA)
    return display, corners


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
        csv_writer.writerow(("frame", "frame_timestamp_us", "tl_x", "tl_y",
                             "tr_x", "tr_y", "br_x", "br_y", "bl_x", "bl_y"))

    client = StreamerClient(host=args.host, port=args.port,
                            udp_send=args.udp_send)
    shown = 0
    try:
        for frame, _tof_frame, metadata in client.receive():
            # This carries no offboard inference. It only acknowledges this
            # frame so GAP8 can retain its round-trip timing statistics.
            client.send_reply(metadata, None)

            display, corners = annotate_frame(frame, metadata)
            shown += 1

            if csv_writer is not None:
                row = [shown, metadata.frame_timestamp]
                if corners is None:
                    row.extend(("",) * 8)
                else:
                    row.extend(np.asarray(corners).flat)
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
