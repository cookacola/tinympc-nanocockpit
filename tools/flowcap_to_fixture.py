#!/usr/bin/env python3
"""Convert one GAP8 FLOWCAP diagnostic block to PGM, JSON, and CSV."""

from __future__ import annotations

import argparse
import csv
import json
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class Track:
    index: int
    x: float
    y: float
    nx: float
    ny: float
    lk_error: float
    fb_error: float


def parse_capture(text: str) -> tuple[dict[str, int], dict[str, bytes], list[Track]]:
    metadata: dict[str, int] | None = None
    frames: dict[str, bytes] = {}
    tracks: list[Track] = []
    ended = False
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if line.startswith("FLOWCAP_BEGIN,"):
            fields = line.split(",")
            if len(fields) != 7 or fields[1] != "1":
                raise ValueError("unsupported FLOWCAP_BEGIN record")
            if metadata is not None:
                raise ValueError("input contains more than one capture")
            metadata = {
                "version": 1,
                "prev_ts_us": int(fields[2]),
                "cur_ts_us": int(fields[3]),
                "width": int(fields[4]),
                "height": int(fields[5]),
                "reported_track_count": int(fields[6]),
            }
        elif metadata is not None and line.startswith("FLOWCAP_FRAME,"):
            fields = line.split(",", 3)
            if len(fields) != 4 or fields[1] not in {"prev", "cur"}:
                raise ValueError("invalid FLOWCAP_FRAME record")
            size = int(fields[2])
            data = bytes.fromhex(fields[3])
            expected = metadata["width"] * metadata["height"]
            if size != expected or len(data) != expected:
                raise ValueError("frame size does not match capture dimensions")
            frames[fields[1]] = data
        elif metadata is not None and line.startswith("FLOWCAP_TRACK,"):
            fields = line.split(",")
            if len(fields) != 8:
                raise ValueError("invalid FLOWCAP_TRACK record")
            tracks.append(Track(int(fields[1]), *(float(v) for v in fields[2:])))
        elif metadata is not None and line == "FLOWCAP_END,1":
            ended = True
            break
    if metadata is None or not ended:
        raise ValueError("complete FLOWCAP version-1 block not found")
    if set(frames) != {"prev", "cur"}:
        raise ValueError("capture must contain prev and cur frames")
    if len(tracks) != metadata["reported_track_count"]:
        raise ValueError("track count does not match FLOWCAP_BEGIN")
    return metadata, frames, tracks


def write_fixture(prefix: Path, metadata: dict[str, int],
                  frames: dict[str, bytes], tracks: list[Track]) -> None:
    prefix.parent.mkdir(parents=True, exist_ok=True)
    pgm_header = f"P5\n{metadata['width']} {metadata['height']}\n255\n".encode()
    for name, data in frames.items():
        prefix.with_name(f"{prefix.name}_{name}.pgm").write_bytes(pgm_header + data)
    document = dict(metadata)
    document["dt_us"] = (
        metadata["cur_ts_us"] - metadata["prev_ts_us"]
    ) & 0xFFFFFFFF
    document["tracks"] = [asdict(track) for track in tracks]
    prefix.with_suffix(".json").write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n")
    with prefix.with_suffix(".csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(asdict(tracks[0]).keys())
                                if tracks else list(Track.__annotations__))
        writer.writeheader()
        writer.writerows(asdict(track) for track in tracks)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="serial text containing FLOWCAP")
    parser.add_argument("output_prefix", type=Path)
    args = parser.parse_args()
    write_fixture(args.output_prefix, *parse_capture(
        args.input.read_text(errors="replace")))


if __name__ == "__main__":
    main()
