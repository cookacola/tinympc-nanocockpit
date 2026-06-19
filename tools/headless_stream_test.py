#!/usr/bin/env python3
"""
Headless smoke test for the NanoCockpit AI-deck streamer.

Connects to the NINA streamer (default 192.168.4.1:5000, AP mode), pulls a few
frames over CPX, saves them as PNGs, and prints per-frame metadata. No GUI /
display required -- useful on a headless box where plt_viewer can't open a
window. Run plt_viewer on a machine with a display for the live view.
"""
import argparse
import os
import sys

import cv2

# Import the client package straight from the source tree (no install needed).
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "src/client/aideck_cpx_streamer"))
from aideck_cpx_streamer.cpx import StreamerClient  # noqa: E402


def main():
    p = argparse.ArgumentParser(description="Headless AI-deck stream test")
    p.add_argument("-host", default="192.168.4.1", help="AI-deck host/IP")
    p.add_argument("-port", type=int, default=5000, help="AI-deck port")
    p.add_argument("-n", type=int, default=10, help="frames to capture then exit")
    p.add_argument("-out", default=os.path.join(HERE, "stream_test_out"),
                   help="output dir for PNGs")
    args = p.parse_args()

    os.makedirs(args.out, exist_ok=True)
    print(f"Connecting to {args.host}:{args.port} ...", flush=True)
    client = StreamerClient(host=args.host, port=args.port, udp_send=True)

    got = 0
    for frame, tof_frame, metadata in client.receive():
        client.send_reply(metadata, None)
        got += 1
        path = os.path.join(args.out, f"frame_{got:03d}.png")
        cv2.imwrite(path, frame)
        info = f"  frame {got}/{args.n}: shape={frame.shape}"
        if metadata is not None:
            info += (f" id={metadata.frame_id}"
                     f" state=({metadata.state.x/1000:+.2f},"
                     f"{metadata.state.y/1000:+.2f},"
                     f"{metadata.state.z/1000:+.2f})m")
        if tof_frame is not None:
            info += f" tof={tof_frame.shape}"
        print(info, flush=True)
        if got >= args.n:
            break

    client.shutdown()
    print(f"OK: saved {got} frames to {args.out}", flush=True)


if __name__ == "__main__":
    main()
