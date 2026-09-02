"""tinympc-perception adapter for tools/neural_net_viewtester.py.

This runs the newer GAP8-oriented multi-task perception model from
https://github.com/cookacola/tinympc-perception when supplied with a compatible
PyTorch checkpoint, such as the v4 pipeline's best_total.pt or last.pt.

The model predicts:
- four 40x40 corner heatmaps,
- 20x20 nominal collision probability,
- 20x20 inverse range,
- 20x20 uncertainty,
- optional 40x40 gate-opening probability.

The adapter returns overlay masks for the viewer: red unsafe regions, green
safe/gate regions, corner points, confidence, and summary labels.
"""

from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np
import torch
from torch import nn


FRAME_SIZE = 160
CORNER_THRESHOLD = 0.20
DANGER_THRESHOLD = 0.35
GATE_THRESHOLD = 0.40
DEFAULT_SPEED_MPS = 1.0
DEFAULT_HORIZON_S = 1.0
DEFAULT_LATENCY_S = 0.08


class ConvBNReLU(nn.Sequential):
    def __init__(self, cin: int, cout: int, kernel: int = 3, stride: int = 1, groups: int = 1):
        super().__init__(
            nn.Conv2d(
                cin,
                cout,
                kernel,
                stride=stride,
                padding=kernel // 2,
                groups=groups,
                bias=False,
            ),
            nn.BatchNorm2d(cout),
            nn.ReLU(inplace=True),
        )


class DSConv(nn.Module):
    def __init__(self, cin: int, cout: int, stride: int = 1):
        super().__init__()
        self.depthwise = ConvBNReLU(cin, cin, 3, stride, groups=cin)
        self.pointwise = ConvBNReLU(cin, cout, 1)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.pointwise(self.depthwise(x))


class ElementwiseAdd(nn.Module):
    def forward(self, first: torch.Tensor, second: torch.Tensor) -> torch.Tensor:
        return first + second


class ResidualDS(nn.Module):
    def __init__(self, channels: int):
        super().__init__()
        self.block = DSConv(channels, channels)
        self.add = ElementwiseAdd()
        self.relu = nn.ReLU(inplace=False)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.relu(self.add(x, self.block(x)))


class Gap8MultiTaskNet(nn.Module):
    """160x160 mono -> corners 4x40x40, danger/range/uncertainty 1x20x20."""

    def __init__(self, gate_head: bool = True, state_dim: int = 8):
        super().__init__()
        self.gate_head_enabled = gate_head
        self.state_dim = state_dim
        self.stem = nn.Sequential(ConvBNReLU(1, 8, 3, 2), DSConv(8, 12))
        self.e1_down = DSConv(12, 20, 2)
        self.e1_refine = ResidualDS(20)
        self.geometry40 = nn.Sequential(
            ConvBNReLU(20, 16, 1),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
            ResidualDS(16),
        )
        self.packed_channels = 8 if gate_head else 7
        self.packed_head = nn.Sequential(DSConv(16, 12), nn.Conv2d(12, self.packed_channels, 1))

    def forward_packed(self, x: torch.Tensor) -> torch.Tensor:
        x = self.stem(x)
        e1 = self.e1_refine(self.e1_down(x))
        return self.packed_head(self.geometry40(e1))

    def forward_image(self, x: torch.Tensor) -> dict[str, torch.Tensor]:
        packed = self.forward_packed(x)
        outputs = {
            "corners": packed[:, 0:4],
            "danger": torch.nn.functional.avg_pool2d(packed[:, 4:5], kernel_size=2, stride=2),
            "urgency": torch.nn.functional.avg_pool2d(packed[:, 5:6], kernel_size=2, stride=2),
            "uncertainty": torch.nn.functional.avg_pool2d(packed[:, 6:7], kernel_size=2, stride=2),
        }
        if self.gate_head_enabled:
            outputs["gate"] = packed[:, 7:8]
        return outputs

    def forward(self, x: torch.Tensor, vehicle_state: torch.Tensor | None = None) -> dict[str, torch.Tensor]:
        return self.forward_image(x)


def load_model(weights_path: str | None):
    if not weights_path:
        default_path = _default_checkpoint_path()
        if default_path.exists():
            weights_path = str(default_path)
    if not weights_path:
        raise FileNotFoundError(
            "tinympc-perception requires a compatible checkpoint. Pass "
            "--nn-weights /path/to/best_total.pt or /path/to/last.pt from the v4 pipeline."
        )
    checkpoint_path = Path(weights_path).expanduser()
    if not checkpoint_path.exists():
        raise FileNotFoundError(f"Checkpoint not found: {checkpoint_path}")

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    state = torch.load(checkpoint_path, map_location=device, weights_only=False)
    if not isinstance(state, dict) or "model" not in state:
        raise ValueError(f"{checkpoint_path} is not a tinympc-perception checkpoint with a 'model' state dict.")

    model = Gap8MultiTaskNet(state.get("gate_head", True), state.get("state_dim", 8)).to(device)
    try:
        model.load_state_dict(state["model"], strict=True)
    except RuntimeError as exc:
        raise ValueError(
            f"{checkpoint_path} does not match Gap8MultiTaskNet. "
            "The local hm01b0_segmenter_best.pt is a semantic segmenter, not the newer multi-task model."
        ) from exc
    model.eval()
    return {
        "model": model,
        "device": device,
        "checkpoint": str(checkpoint_path),
        "gate_head": bool(state.get("gate_head", True)),
    }


def _default_checkpoint_path() -> Path:
    here = Path(__file__).resolve()
    candidates = [
        here.parents[2] / "gap8_download_bundle_hm01b0_v4" / "checkpoints" / "float_best.pt",
        here.parents[1] / "gap8_download_bundle_hm01b0_v4" / "checkpoints" / "float_best.pt",
    ]
    for path in candidates:
        if path.exists():
            return path
    return candidates[0]


def predict(frame: np.ndarray, metadata, tof_frame, model=None) -> dict:
    if model is None:
        model = load_model(None)
    gray160 = _to_gray160(frame)
    tensor = torch.from_numpy(gray160.astype(np.float32) / 255.0)[None, None].to(model["device"])
    vehicle_state = _vehicle_state(metadata).to(model["device"])

    with torch.no_grad():
        outputs = model["model"](tensor, vehicle_state)
        corners_prob = outputs["corners"].sigmoid()[0].cpu().numpy()
        base_danger = outputs["danger"].sigmoid()[0, 0].cpu().numpy()
        inv_range = outputs["urgency"].sigmoid()[0, 0].cpu().numpy()
        uncertainty = outputs["uncertainty"].sigmoid()[0, 0].cpu().numpy()
        gate = outputs.get("gate")
        gate_prob = None if gate is None else gate.sigmoid()[0, 0].cpu().numpy()

    corners_px, corner_scores, corners_valid = _decode_corners(corners_prob)
    speed_mps = _speed_from_metadata(metadata)
    danger_prob, ttc_s = _collision_probability_from_range(
        inv_range,
        base_danger,
        uncertainty,
        body_speed_mps=speed_mps,
        horizon_s=DEFAULT_HORIZON_S,
        latency_s=DEFAULT_LATENCY_S,
    )
    safe_mask_20 = danger_prob < DANGER_THRESHOLD
    unsafe_mask_160 = cv2.resize(danger_prob, (FRAME_SIZE, FRAME_SIZE), interpolation=cv2.INTER_LINEAR)
    safe_mask_160 = cv2.resize(safe_mask_20.astype(np.float32), (FRAME_SIZE, FRAME_SIZE), interpolation=cv2.INTER_NEAREST)
    if gate_prob is not None:
        gate_mask_160 = cv2.resize((gate_prob > GATE_THRESHOLD).astype(np.float32), (FRAME_SIZE, FRAME_SIZE),
                                   interpolation=cv2.INTER_NEAREST)
        safe_mask_160 = np.maximum(safe_mask_160, gate_mask_160)

    heatmaps = {
        "corner_max_40": np.max(corners_prob, axis=0).astype(np.float32),
        "corner_tl_40": corners_prob[0].astype(np.float32),
        "corner_tr_40": corners_prob[1].astype(np.float32),
        "corner_br_40": corners_prob[2].astype(np.float32),
        "corner_bl_40": corners_prob[3].astype(np.float32),
        "danger_20": danger_prob.astype(np.float32),
        "nominal_danger_20": base_danger.astype(np.float32),
        "inverse_range_20": inv_range.astype(np.float32),
        "uncertainty_20": uncertainty.astype(np.float32),
    }
    if gate_prob is not None:
        heatmaps["gate_40"] = gate_prob.astype(np.float32)

    max_danger = float(np.max(danger_prob))
    mean_danger = float(np.mean(danger_prob))
    min_ttc = float(np.min(ttc_s))
    confidence = float(np.clip(np.mean(corner_scores), 0.0, 1.0))
    dangerous = bool(max_danger >= DANGER_THRESHOLD)
    label = "unsafe" if dangerous else "safe"
    if not corners_valid:
        label = "no gate lock"

    return {
        "label": label,
        "dangerous": dangerous,
        "confidence": confidence,
        "corners_px": corners_px,
        "safe_mask": safe_mask_160,
        "unsafe_mask": unsafe_mask_160,
        "danger_threshold": DANGER_THRESHOLD,
        "max_danger": max_danger,
        "mean_danger": mean_danger,
        "min_ttc_s": min_ttc,
        "corner_scores": corner_scores,
        "heatmaps": heatmaps,
        "reply": [max_danger, mean_danger, min_ttc, confidence],
        "debug": {
            "checkpoint": model["checkpoint"],
            "speed_mps": speed_mps,
            "frame_id": None if metadata is None else int(metadata.frame_id),
        },
    }


def _to_gray160(frame: np.ndarray) -> np.ndarray:
    gray = frame
    if gray.ndim == 3:
        gray = cv2.cvtColor(gray, cv2.COLOR_BGR2GRAY)
    if gray.dtype != np.uint8:
        gray = cv2.normalize(gray, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    if gray.shape != (FRAME_SIZE, FRAME_SIZE):
        side = min(gray.shape[:2])
        y0 = (gray.shape[0] - side) // 2
        x0 = (gray.shape[1] - side) // 2
        gray = cv2.resize(gray[y0 : y0 + side, x0 : x0 + side], (FRAME_SIZE, FRAME_SIZE),
                          interpolation=cv2.INTER_AREA)
    return gray


def _vehicle_state(metadata) -> torch.Tensor:
    # Training kept this argument for compatibility; the DORY model is image-only.
    speed = _speed_from_metadata(metadata)
    values = [speed, 0.0, 0.0, 0.0, 0.0, 0.0, DEFAULT_HORIZON_S, DEFAULT_LATENCY_S]
    return torch.tensor(values, dtype=torch.float32)[None]


def _speed_from_metadata(metadata) -> float:
    if metadata is None:
        return DEFAULT_SPEED_MPS
    state = getattr(metadata, "state", None)
    if state is None:
        return DEFAULT_SPEED_MPS
    values = []
    for name in ("vx", "vy", "vz"):
        if hasattr(state, name):
            values.append(float(getattr(state, name)) / 1000.0)
    if not values:
        return DEFAULT_SPEED_MPS
    speed = float(np.linalg.norm(values))
    return speed if speed > 1e-3 else DEFAULT_SPEED_MPS


def _decode_corners(heatmaps: np.ndarray, threshold: float = CORNER_THRESHOLD):
    corners = []
    confidence = []
    for channel in heatmaps:
        y, x = np.unravel_index(np.argmax(channel), channel.shape)
        score = float(channel[y, x])
        confidence.append(score)
        y0, y1 = max(0, y - 2), min(channel.shape[0], y + 3)
        x0, x1 = max(0, x - 2), min(channel.shape[1], x + 3)
        patch = channel[y0:y1, x0:x1].clip(1e-8)
        yy, xx = np.mgrid[y0:y1, x0:x1]
        corners.append([(patch * xx).sum() / patch.sum(), (patch * yy).sum() / patch.sum()])
    confidence_arr = np.asarray(confidence, dtype=np.float32)
    return np.asarray(corners, dtype=np.float32) * 4.0, confidence_arr, bool(np.all(confidence_arr >= threshold))


def _collision_probability_from_range(
    inverse_range: np.ndarray,
    base_hazard_probability: np.ndarray,
    uncertainty: np.ndarray,
    body_speed_mps: float,
    horizon_s: float,
    latency_s: float,
    max_range_m: float = 6.0,
    transition_m: float = 0.15,
    nominal_target_speed_mps: float = 1.0,
) -> tuple[np.ndarray, np.ndarray]:
    inv = np.clip(np.asarray(inverse_range, np.float32), 0.0, 1.0)
    base = np.clip(np.asarray(base_hazard_probability, np.float32), 0.0, 1.0)
    sigma = np.clip(np.asarray(uncertainty, np.float32), 0.0, 1.0)
    range_m = (1.0 - inv) * max_range_m
    speed = max(float(body_speed_mps), 1e-3)
    reachable_m = speed * max(float(horizon_s) + float(latency_s), 0.0)
    nominal_reachable_m = max(float(nominal_target_speed_mps), 1e-3) * max(float(horizon_s) + 0.08, 0.0)
    softened_margin = transition_m * (1.0 + 2.0 * sigma)
    geometric = 1.0 / (1.0 + np.exp(np.clip((range_m - reachable_m) / softened_margin, -30, 30)))
    base_logit = np.log(np.clip(base, 1e-5, 1 - 1e-5) / np.clip(1 - base, 1e-5, 1))
    adjusted = 1.0 / (
        1.0 + np.exp(np.clip(-(base_logit + (reachable_m - nominal_reachable_m) / softened_margin), -30, 30))
    )
    probability = np.maximum(adjusted, geometric)
    effective_range = np.maximum(0.0, range_m - speed * float(latency_s))
    ttc_s = effective_range / speed
    return probability.astype(np.float32), ttc_s.astype(np.float32)
