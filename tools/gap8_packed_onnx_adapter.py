"""Display adapter for the deployed GAP8 packed integer network.

This adapter runs the exact ``gap8_packed_int.onnx`` artifact delivered in
``gap8_download_bundle_hm01b0_v4``.  Its output is represented as float by
ONNX Runtime, but every value is an integer in the uint8 deployment domain.
The affine dequantization below is the runtime contract published with the
bundle, so the rendered maps agree with the GAP8 model rather than its float
teacher.
"""

from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np


FRAME_SIZE = 160
OUTPUT_SIZE = 40
CHANNELS = 8
CORNER_THRESHOLD = 0.20
DANGER_THRESHOLD = 0.40
GATE_THRESHOLD = 0.50
UNCERTAINTY_THRESHOLD = 0.50

# Values generated into controller/perception_model_qparams.h by the v4
# packaging step.  Keep these beside the display code so a copied ONNX file
# remains usable without parsing C headers at runtime.
OUTPUT_EPSILON = 0.19889970123767853
SPATIAL_OFFSET = np.array(
    [8.042428834533691, 11.111829621887207, 11.572567803955078,
     8.653360231018066, 12.788898332214355, 7.22682176361084,
     5.082098348236084, 36.62235341796875], dtype=np.float32)
LEARNED_BIAS = np.array(
    [-5.684194087982178, -0.36083489656448364, -0.45318108797073364,
     -3.4574155807495117, 0.17458303272724152, 1.1917307376861572,
     1.3860822916030884, -1.4064135551452637], dtype=np.float32)


def _default_model_path() -> Path:
    here = Path(__file__).resolve()
    candidates = (
        # Standard layout: TinyMPC/{tinympc-nanocockpit,bundle}.
        here.parents[2] / "gap8_download_bundle_hm01b0_v4" / "export" / "gap8_packed_int.onnx",
        # Also permit a bundle copied into this checkout.
        here.parents[1] / "gap8_download_bundle_hm01b0_v4" / "export" / "gap8_packed_int.onnx",
    )
    return next((path for path in candidates if path.exists()), candidates[0])


def load_model(weights_path: str | None = None) -> dict:
    """Open the deployed ONNX graph; ``weights_path`` may override its path."""
    try:
        import onnxruntime as ort
    except ImportError as exc:
        raise ImportError(
            "The deployed GAP8 viewer needs onnxruntime. Install dependencies with "
            "python3 -m pip install -r requirements.txt."
        ) from exc

    model_path = Path(weights_path).expanduser() if weights_path else _default_model_path()
    if not model_path.is_file():
        raise FileNotFoundError(
            f"GAP8 ONNX model not found: {model_path}. Pass --nn-weights with the "
            "bundle's export/gap8_packed_int.onnx path."
        )
    session = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
    model_input = session.get_inputs()[0]
    model_output = session.get_outputs()[0]
    if model_input.shape != [1, 1, FRAME_SIZE, FRAME_SIZE] or model_output.shape != [1, CHANNELS, OUTPUT_SIZE, OUTPUT_SIZE]:
        raise ValueError(
            f"{model_path} has incompatible I/O: {model_input.shape} -> {model_output.shape}; "
            "expected [1, 1, 160, 160] -> [1, 8, 40, 40]."
        )
    return {"session": session, "input_name": model_input.name, "checkpoint": str(model_path)}


def predict(frame: np.ndarray, metadata, tof_frame, model=None) -> dict:
    """Run the deployed network and return panels/overlays for the viewer."""
    if model is None:
        model = load_model()
    gray = _to_gray160(frame)
    packed = model["session"].run(
        None, {model["input_name"]: gray.astype(np.float32)[None, None] / 255.0}
    )[0]
    quantized = np.clip(np.rint(packed[0]), 0, 255).astype(np.uint8)
    logits = quantized.astype(np.float32) * OUTPUT_EPSILON
    logits -= SPATIAL_OFFSET[:, None, None]
    logits += LEARNED_BIAS[:, None, None]
    probabilities = _sigmoid(logits)

    corners = probabilities[:4]
    collision_40, inverse_range_40, uncertainty_40, gate_40 = probabilities[4:]
    danger_20 = _pool_2x2(collision_40, np.max)
    inverse_range_20 = _pool_2x2(inverse_range_40, np.max)
    uncertainty_20 = _pool_2x2(uncertainty_40, np.max)
    gate_20 = _pool_2x2(gate_40, np.min)
    corners_px, corner_scores, corners_valid = _decode_corners(corners)

    unsafe_mask = cv2.resize(danger_20, (FRAME_SIZE, FRAME_SIZE), interpolation=cv2.INTER_NEAREST)
    # A gate opening is only green when it is not also an unsafe or uncertain
    # cell. This avoids hiding a detected obstacle behind a gate overlay.
    permitted = ((gate_20 >= GATE_THRESHOLD) & (danger_20 < DANGER_THRESHOLD)
                 & (uncertainty_20 < UNCERTAINTY_THRESHOLD)).astype(np.float32)
    safe_mask = cv2.resize(permitted, (FRAME_SIZE, FRAME_SIZE), interpolation=cv2.INTER_NEAREST)
    max_danger = float(np.max(danger_20))
    confidence = float(np.mean(corner_scores))
    dangerous = max_danger >= DANGER_THRESHOLD
    label = "unsafe" if dangerous else "safe"
    if not corners_valid:
        label = "no gate lock"

    return {
        "label": label,
        "dangerous": dangerous,
        "confidence": confidence,
        "corners_px": corners_px,
        "corner_scores": corner_scores,
        "safe_mask": safe_mask,
        "unsafe_mask": unsafe_mask,
        "danger_threshold": DANGER_THRESHOLD,
        "max_danger": max_danger,
        "mean_danger": float(np.mean(danger_20)),
        "heatmaps": {
            "corner_max_40": np.max(corners, axis=0),
            "corner_tl_40": corners[0], "corner_tr_40": corners[1],
            "corner_br_40": corners[2], "corner_bl_40": corners[3],
            "danger_20": danger_20, "inverse_range_20": inverse_range_20,
            "uncertainty_20": uncertainty_20, "gate_40": gate_40,
            "gate_permission_20": gate_20,
        },
        "debug": {"checkpoint": model["checkpoint"], "frame_id": getattr(metadata, "frame_id", None)},
    }


def _to_gray160(frame: np.ndarray) -> np.ndarray:
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY) if frame.ndim == 3 else frame
    if gray.dtype != np.uint8:
        gray = cv2.normalize(gray, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    if gray.shape != (FRAME_SIZE, FRAME_SIZE):
        gray = cv2.resize(gray, (FRAME_SIZE, FRAME_SIZE), interpolation=cv2.INTER_AREA)
    return gray


def _sigmoid(values: np.ndarray) -> np.ndarray:
    return 1.0 / (1.0 + np.exp(-np.clip(values, -60.0, 60.0)))


def _pool_2x2(values: np.ndarray, reducer) -> np.ndarray:
    return reducer(values.reshape(20, 2, 20, 2), axis=(1, 3))


def _decode_corners(heatmaps: np.ndarray):
    corners, scores = [], []
    for channel in heatmaps:
        y, x = np.unravel_index(np.argmax(channel), channel.shape)
        score = float(channel[y, x])
        scores.append(score)
        y0, y1 = max(0, y - 2), min(OUTPUT_SIZE, y + 3)
        x0, x1 = max(0, x - 2), min(OUTPUT_SIZE, x + 3)
        patch = channel[y0:y1, x0:x1].clip(1e-8)
        yy, xx = np.mgrid[y0:y1, x0:x1]
        corners.append(((patch * xx).sum() / patch.sum(), (patch * yy).sum() / patch.sum()))
    scores = np.asarray(scores, dtype=np.float32)
    return np.asarray(corners, dtype=np.float32) * 4.0, scores, bool(np.all(scores >= CORNER_THRESHOLD))
