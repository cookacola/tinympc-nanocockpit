"""Host viewer adapter for ``shared_dory_frozen_real_v1``.

The deployed STDC DORY graph is three integer ONNX components: encoder,
corner head, and danger head.  It consumes the central 160x120 HM01B0 crop.
This adapter executes those exact components and decodes their published
integer-affine outputs for ``neural_net_viewtester.py``.
"""

from __future__ import annotations

import json
from pathlib import Path

import cv2
import numpy as np


INPUT_WIDTH, INPUT_HEIGHT = 160, 120
SOURCE_HEIGHT = 160
CORNER_SHAPE = (30, 40, 4)
DANGER_SHAPE = (8, 10)
# These are the integer thresholds used by the deployed GAP8 output decoder.
CORNER_Q_THRESHOLDS = np.array([136, 141, 187, 131], dtype=np.float32)


def _release_root() -> Path:
    here = Path(__file__).resolve()
    candidates = (
        here.parents[1] / "gap8_stdc_release_shared_real_v1",
        here.parents[2] / "gap8_stdc_release_shared_real_v1",
    )
    return next((path for path in candidates if path.is_dir()), candidates[0])


def load_model(weights_path: str | None = None) -> dict:
    """Load the three ONNX components from a release root or integer directory."""
    try:
        import onnxruntime as ort
    except ImportError as exc:
        raise ImportError("Install onnxruntime with: python3 -m pip install -r requirements.txt") from exc

    requested = Path(weights_path).expanduser() if weights_path else _release_root()
    integer_dir = requested / "integer" if (requested / "integer").is_dir() else requested
    manifest_path = integer_dir.parent / "nanocockpit" / "manifest.json"
    if not manifest_path.is_file():
        raise FileNotFoundError(
            f"STDC release manifest not found under {requested}. Pass --nn-weights "
            "with gap8_stdc_release_shared_real_v1 or its integer directory."
        )
    with manifest_path.open() as file:
        manifest = json.load(file)
    paths = {name: integer_dir / f"{name}_int.onnx" for name in ("encoder", "corner_head", "danger_head")}
    missing = [str(path) for path in paths.values() if not path.is_file()]
    if missing:
        raise FileNotFoundError("Missing STDC ONNX component(s): " + ", ".join(missing))
    sessions = {name: ort.InferenceSession(str(path), providers=["CPUExecutionProvider"])
                for name, path in paths.items()}
    _check_io(sessions)
    return {"sessions": sessions, "manifest": manifest, "checkpoint": str(integer_dir.parent)}


def predict(frame: np.ndarray, metadata, tof_frame, model=None) -> dict:
    if model is None:
        model = load_model()
    crop, display_y_offset = _center_crop(frame)
    sessions = model["sessions"]
    # The released integer graph consumes the camera's *uint8 pixel domain*
    # (see nanocockpit/manifest.json), not a [0, 1] normalized image.  Keep
    # float32 solely because ONNX Runtime declares the graph input as float.
    # Dividing by 255 here drives the integer encoder far outside the domain
    # used by both the GAP8 DORY network and the release calibration.
    encoder = _run(sessions["encoder"], crop.astype(np.float32)[None, None])
    corner_q = _quantized(_run(sessions["corner_head"], encoder)[0])
    danger_q = _quantized(_run(sessions["danger_head"], encoder)[0, 0])

    affine = model["manifest"]["integer_affine"]
    corners_prob = _probability(corner_q, affine["corner"])
    danger_prob = _probability(danger_q, affine["danger"])
    threshold = float(model["manifest"]["danger_probability_threshold"])
    corners_px, corner_scores = _decode_corners(corners_prob, display_y_offset)
    corner_confident = np.max(corner_q, axis=(1, 2)) >= CORNER_Q_THRESHOLDS
    corners_px, recovered_corner, gate_reason = _validate_or_recover_gate(
        corners_px, corner_confident, display_y_offset
    )
    gate_locked = gate_reason.startswith("accepted")
    source_h, source_w = frame.shape[:2]
    unsafe = _danger_overlay(danger_prob, source_w, source_h, display_y_offset)
    max_danger = float(np.max(danger_prob))

    return {
        "label": "unsafe" if max_danger >= threshold else "safe",
        "dangerous": max_danger >= threshold,
        "confidence": float(np.mean(corner_scores)),
        "corners_px": corners_px,
        # Keep the individual argmax markers for debugging, but do not join
        # low-confidence peaks into a fictitious gate polygon.
        "plane_points_px": corners_px if gate_locked else None,
        "corner_scores": corner_scores,
        "unsafe_mask": unsafe,
        "danger_threshold": threshold,
        "max_danger": max_danger,
        "mean_danger": float(np.mean(danger_prob)),
        "heatmaps": {
            "corner_max_40": np.max(corners_prob, axis=0),
            "corner_tl_40": corners_prob[0], "corner_tr_40": corners_prob[1],
            "corner_br_40": corners_prob[2], "corner_bl_40": corners_prob[3],
            "danger_20": danger_prob,
        },
        "debug": {
            "checkpoint": model["checkpoint"],
            "frame_id": getattr(metadata, "frame_id", None),
            "gate_locked": gate_locked,
            "gate_reason": gate_reason,
            "recovered_corner": recovered_corner,
        },
    }


def _check_io(sessions: dict) -> None:
    expected = {
        "encoder": ([1, 1, 120, 160], [1, 32, 30, 40]),
        "corner_head": ([1, 32, 30, 40], [1, 4, 30, 40]),
        "danger_head": ([1, 32, 30, 40], [1, 1, 8, 10]),
    }
    for name, (input_shape, output_shape) in expected.items():
        session = sessions[name]
        if session.get_inputs()[0].shape != input_shape or session.get_outputs()[0].shape != output_shape:
            raise ValueError(f"Unexpected {name} ONNX I/O; expected {input_shape} -> {output_shape}.")


def _run(session, values: np.ndarray) -> np.ndarray:
    return session.run(None, {session.get_inputs()[0].name: values})[0]


def _quantized(values: np.ndarray) -> np.ndarray:
    return np.clip(np.rint(values), 0, 255).astype(np.float32)


def _probability(values: np.ndarray, affine: dict) -> np.ndarray:
    offsets = np.asarray(affine["offset"], dtype=np.float32)
    bias = np.asarray(affine["learned_bias"], dtype=np.float32)
    if values.ndim == 3:
        logits = values * float(affine["epsilon"])
        logits -= offsets[:, None, None]
        logits += bias[:, None, None]
    else:
        logits = values * float(affine["epsilon"]) - offsets[0] + bias[0]
    return 1.0 / (1.0 + np.exp(-np.clip(logits, -60.0, 60.0)))


def _center_crop(frame: np.ndarray) -> tuple[np.ndarray, int]:
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY) if frame.ndim == 3 else frame
    if gray.dtype != np.uint8:
        gray = cv2.normalize(gray, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    if gray.shape == (INPUT_HEIGHT, INPUT_WIDTH):
        return gray, 0
    if gray.shape == (SOURCE_HEIGHT, INPUT_WIDTH):
        return gray[20:140], 20
    raise ValueError(f"STDC viewer needs a 160x120 stream or 160x160 source; received {gray.shape[1]}x{gray.shape[0]}.")


def _decode_corners(heatmaps: np.ndarray, y_offset: int) -> tuple[np.ndarray, np.ndarray]:
    points, scores = [], []
    for channel in heatmaps:
        y, x = np.unravel_index(np.argmax(channel), channel.shape)
        points.append(((x + 0.5) * 4.0, (y + 0.5) * 4.0 + y_offset))
        scores.append(float(channel[y, x]))
    return np.asarray(points, dtype=np.float32), np.asarray(scores, dtype=np.float32)


def _validate_or_recover_gate(
    corners: np.ndarray, confident: np.ndarray, y_offset: int
) -> tuple[np.ndarray, int | None, str]:
    """Mirror the deployed GAP8 confidence and geometry gate."""
    points = np.asarray(corners, dtype=np.float32).copy()
    confident = np.asarray(confident, dtype=bool)
    count = int(confident.sum())
    recovered = None
    if count < 3:
        return points, recovered, "confidence"
    if count == 3:
        recovered = int(np.flatnonzero(~confident)[0])
        opposite = (recovered + 2) & 3
        previous = (recovered + 3) & 3
        following = (recovered + 1) & 3
        points[recovered] = points[previous] + points[following] - points[opposite]
        x, y = points[recovered]
        if not (0.0 <= x < INPUT_WIDTH and y_offset <= y < y_offset + INPUT_HEIGHT):
            return np.asarray(corners, dtype=np.float32), None, "recovered_out_of_bounds"

    # Semantic corner order is TL, TR, BR, BL.
    if not (
        points[0, 0] < points[1, 0]
        and points[3, 0] < points[2, 0]
        and points[0, 1] < points[3, 1]
        and points[1, 1] < points[2, 1]
    ):
        return np.asarray(corners, dtype=np.float32), None, "ordering"
    contour = points.reshape(-1, 1, 2)
    if not cv2.isContourConvex(contour):
        return np.asarray(corners, dtype=np.float32), None, "nonconvex"
    area = abs(float(cv2.contourArea(contour)))
    if area < 128.0 or area > 23000.0:
        return np.asarray(corners, dtype=np.float32), None, "area"
    width = 0.5 * ((points[1, 0] - points[0, 0]) + (points[2, 0] - points[3, 0]))
    height = 0.5 * ((points[3, 1] - points[0, 1]) + (points[2, 1] - points[1, 1]))
    if width <= 0.0 or height <= 0.0 or not (0.35 <= width / height <= 2.85):
        return np.asarray(corners, dtype=np.float32), None, "aspect"
    return points, recovered, "accepted_three_corners" if recovered is not None else "accepted"


def _danger_overlay(danger: np.ndarray, width: int, height: int, y_offset: int) -> np.ndarray:
    cropped = cv2.resize(danger, (INPUT_WIDTH, INPUT_HEIGHT), interpolation=cv2.INTER_NEAREST)
    if height == INPUT_HEIGHT:
        return cropped
    # The 160x160 camera frame contains 20 rows above and below the STDC
    # crop. They are conservatively unsafe for the flight controller, but are
    # not a neural-net prediction and should not paint the host preview red.
    full = np.zeros((height, width), dtype=np.float32)
    full[y_offset:y_offset + INPUT_HEIGHT] = cropped
    return full
