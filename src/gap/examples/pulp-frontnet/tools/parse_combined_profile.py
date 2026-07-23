#!/usr/bin/env python3
"""Convert GAP8 combined CNN/LK JTAG output into simulation-friendly CSV/JSON."""

import argparse
import csv
import json
import re
import statistics
from pathlib import Path


FLOW_FIELDS = (
    "sequence",
    "frame_ts_us",
    "frame_dt_us",
    "total_us",
    "pyramid_us",
    "select_us",
    "track_us",
    "aggregate_us",
    "corner_max_score",
    "selected",
    "accepted",
    "rejected",
    "mean_error_milli",
    "max_error_milli",
    "valid_sector_mask",
)

PROFILE_FIELDS = (
    "flow_count",
    "flow_total_mean_us", "flow_total_std_us",
    "flow_total_min_us", "flow_total_max_us",
    "flow_track_mean_us", "flow_track_std_us",
    "flow_track_min_us", "flow_track_max_us",
    "frame_dt_mean_us", "frame_dt_std_us",
    "frame_dt_min_us", "frame_dt_max_us",
    "selected_mean", "selected_std",
    "accepted_mean", "accepted_std",
    "photometric_error_mean_milli", "photometric_error_std_milli",
    "photometric_error_min_milli", "photometric_error_max_milli",
    "valid_sectors_mean", "valid_sectors_std",
    "cnn_count", "cnn_mean_us", "cnn_std_us", "cnn_min_us", "cnn_max_us",
)

QUANTILE_FIELDS = (
    "flow_count",
    "flow_total_p95_us_upper",
    "flow_total_p99_us_upper",
    "cnn_count",
    "cnn_p95_us_upper",
    "cnn_p99_us_upper",
)


def percentile(values, probability):
    if not values:
        return None
    ordered = sorted(values)
    index = probability * (len(ordered) - 1)
    lower = int(index)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = index - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def stats(values):
    if not values:
        return {"count": 0}
    return {
        "count": len(values),
        "mean": statistics.fmean(values),
        "min": min(values),
        "p50": percentile(values, 0.50),
        "p95": percentile(values, 0.95),
        "p99": percentile(values, 0.99),
        "max": max(values),
    }


def parse_flow(line):
    if not line.startswith("flowdiag,"):
        return None
    values = line.strip().split(",")[1:]
    if len(values) != len(FLOW_FIELDS):
        return None
    row = {}
    for name, value in zip(FLOW_FIELDS, values):
        row[name] = int(value, 16) if name == "valid_sector_mask" else int(value)
    row["mean_error_intensity"] = row["mean_error_milli"] / 1000.0
    row["max_error_intensity"] = row["max_error_milli"] / 1000.0
    row["valid_sector_count"] = row["valid_sector_mask"].bit_count()
    return row


def parse_heartbeat(line):
    if not line.startswith("hb "):
        return None
    pairs = dict(re.findall(r"([a-z_]+)=([^ ]+)", line))
    required = ("t_us", "cam", "flow", "fd", "uart", "cnn")
    if any(key not in pairs for key in required):
        return None

    flow = [int(value) for value in pairs["flow"].split("/")]
    drops = [int(value) for value in pairs["fd"].split("/")]
    uart = [int(value) for value in pairs["uart"].split("/")]
    cnn = [int(value) for value in pairs["cnn"].split("/")]
    return {
        "t_us": int(pairs["t_us"]),
        "camera_captures": int(pairs["cam"]),
        "flow_snapshots": flow[0],
        "flow_processed": flow[1],
        "flow_transmitted": flow[2],
        "flow_snapshot_drops": drops[0],
        "flow_tx_drops": drops[1],
        "uart_queued": uart[0],
        "uart_completed": uart[1],
        "uart_errors": uart[2],
        "cnn_invocations": cnn[0],
        "cnn_completions": cnn[1],
        "cnn_last_us": cnn[2],
        "cnn_max_us": cnn[3],
    }


def parse_profile(line):
    if not line.startswith("profile,"):
        return None
    values = line.strip().split(",")[1:]
    if len(values) != len(PROFILE_FIELDS):
        return None
    return {name: int(value) for name, value in zip(PROFILE_FIELDS, values)}


def parse_quantile(line):
    if not line.startswith("quantile,"):
        return None
    values = line.strip().split(",")[1:]
    if len(values) != len(QUANTILE_FIELDS):
        return None
    return {name: int(value) for name, value in zip(QUANTILE_FIELDS, values)}


def rate_windows(heartbeats):
    rows = []
    counter_fields = (
        "camera_captures",
        "flow_snapshots",
        "flow_processed",
        "flow_transmitted",
        "cnn_invocations",
        "cnn_completions",
    )
    for previous, current in zip(heartbeats, heartbeats[1:]):
        elapsed = (current["t_us"] - previous["t_us"]) & 0xFFFFFFFF
        if elapsed == 0:
            continue
        row = {"start_us": previous["t_us"], "end_us": current["t_us"],
               "elapsed_s": elapsed / 1_000_000.0}
        for field in counter_fields:
            delta = current[field] - previous[field]
            row[field + "_hz"] = delta * 1_000_000.0 / elapsed
        row["flow_snapshot_drops"] = (
            current["flow_snapshot_drops"] - previous["flow_snapshot_drops"]
        )
        row["flow_tx_drops"] = current["flow_tx_drops"] - previous["flow_tx_drops"]
        row["uart_errors"] = current["uart_errors"] - previous["uart_errors"]
        rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path, help="raw output captured from gap8.sh run")
    parser.add_argument("--out-prefix", type=Path, default=Path("combined_profile"))
    args = parser.parse_args()

    flows = []
    heartbeats = []
    profiles = []
    quantiles = []
    for line in args.log.read_text(errors="replace").splitlines():
        flow = parse_flow(line)
        if flow:
            flows.append(flow)
        heartbeat = parse_heartbeat(line)
        if heartbeat:
            heartbeats.append(heartbeat)
        profile = parse_profile(line)
        if profile:
            profiles.append(profile)
        quantile = parse_quantile(line)
        if quantile:
            quantiles.append(quantile)

    windows = rate_windows(heartbeats)
    flow_csv = args.out_prefix.with_suffix(".flow.csv")
    rate_csv = args.out_prefix.with_suffix(".rates.csv")
    profile_csv = args.out_prefix.with_suffix(".profiles.csv")
    quantile_csv = args.out_prefix.with_suffix(".quantiles.csv")
    summary_json = args.out_prefix.with_suffix(".summary.json")

    flow_columns = list(FLOW_FIELDS) + [
        "mean_error_intensity", "max_error_intensity", "valid_sector_count"
    ]
    with flow_csv.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=flow_columns)
        writer.writeheader()
        writer.writerows(flows)

    with rate_csv.open("w", newline="") as stream:
        columns = list(windows[0]) if windows else ["start_us", "end_us", "elapsed_s"]
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(windows)

    with profile_csv.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=PROFILE_FIELDS)
        writer.writeheader()
        writer.writerows(profiles)

    with quantile_csv.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=QUANTILE_FIELDS)
        writer.writeheader()
        writer.writerows(quantiles)

    summary = {
        "flow_total_us": stats([row["total_us"] for row in flows]),
        "flow_track_us": stats([row["track_us"] for row in flows]),
        "frame_dt_us": stats([row["frame_dt_us"] for row in flows]),
        "accepted_tracks": stats([row["accepted"] for row in flows]),
        "mean_photometric_error": stats(
            [row["mean_error_intensity"] for row in flows if row["accepted"]]
        ),
        "valid_sector_count": stats([row["valid_sector_count"] for row in flows]),
        "flow_processed_hz": stats([row["flow_processed_hz"] for row in windows]),
        "cnn_completed_hz": stats([row["cnn_completions_hz"] for row in windows]),
        "aggregate_profiles": profiles,
        "quantile_windows": quantiles,
        "notes": [
            "Photometric residual is not ground-truth optical-flow error.",
            "Discard the first CNN inference when analyzing warm-up latency.",
            "Firmware p95/p99 values are conservative 250 us histogram-bin upper bounds.",
        ],
    }
    summary_json.write_text(json.dumps(summary, indent=2) + "\n")
    print(flow_csv)
    print(rate_csv)
    print(profile_csv)
    print(quantile_csv)
    print(summary_json)


if __name__ == "__main__":
    main()
