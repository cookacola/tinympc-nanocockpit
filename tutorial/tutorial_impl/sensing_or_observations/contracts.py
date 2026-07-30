"""
Tutorial module: tutorial_impl/sensing_or_observations/contracts.py

Purpose:
    Define the minimal data contracts used to teach NanoCockpit Frontnet
    deployment without requiring a Crazyflie, AI-deck, ROS, GAP SDK, or ESP-IDF.

Original references:
    - Paper: NanoCockpit paper system description and Human pose estimation
      experiment; TODO: verify section numbers from publisher PDF.
    - Original code:
      src/gap/examples/pulp-frontnet/main.c::camera_callback
      src/gap/lib/streamer.h::streamer_metadata_t
      src/gap/lib/uart_protocol.h::inference_stamped_msg_t

Modernization:
    - Original stack: GAP8 C firmware, packed C structs, CPX over SPI/Wi-Fi.
    - Tutorial stack: Python dataclasses for CPU-runnable validation.
    - Preserved: image tensor size, state timestamp linkage, four-value
      Frontnet output, and reply timestamps.
    - Changed: no real camera, UART, SPI, ROS, or neural-network kernels.
"""

from dataclasses import dataclass
from typing import Tuple


@dataclass(frozen=True)
class FrontnetContract:
    """Static dimensions exported by the deployed Frontnet header."""

    input_count: int
    output_count: int
    l2_buffer_size: int
    input_shape: Tuple[int, int, int] = (160, 32, 3)

    @property
    def input_size_bytes(self) -> int:
        return self.input_count

    def validate(self) -> None:
        if self.input_count != self.input_shape[0] * self.input_shape[1] * self.input_shape[2]:
            raise ValueError(
                "Frontnet input_count must match width * height * channels "
                f"({self.input_shape}), got {self.input_count}"
            )
        if self.output_count != 4:
            raise ValueError(f"Frontnet must output [x, y, z, phi], got {self.output_count} values")
        if self.l2_buffer_size <= self.input_size_bytes:
            raise ValueError("L2 buffer must be larger than one input frame")


@dataclass(frozen=True)
class InferenceStamped:
    """The four Frontnet outputs paired with the STM32 state timestamp."""

    stm32_timestamp: int
    x: float
    y: float
    z: float
    phi: float

    def as_tuple(self) -> Tuple[float, float, float, float]:
        return (self.x, self.y, self.z, self.phi)

    def validate(self) -> None:
        if self.stm32_timestamp < 0:
            raise ValueError("STM32 timestamp must be non-negative")
        if len(self.as_tuple()) != 4:
            raise ValueError("Inference output must contain exactly four values")


@dataclass(frozen=True)
class StreamerMetadata:
    """Subset of streamed metadata needed to teach deployment verification."""

    metadata_version: int
    frame_id: int
    frame_gap8_timestamp_us: int
    state_gap8_timestamp_us: int
    state_stm32_timestamp: int
    reply_frame_gap8_timestamp_us: int = 0
    reply_recv_gap8_timestamp_us: int = 0

    def validate(self) -> None:
        if self.metadata_version != 1:
            raise ValueError(f"Expected streamer metadata version 1, got {self.metadata_version}")
        if not 0 <= self.frame_id <= 255:
            raise ValueError("frame_id is an unsigned 8-bit hardware frame counter")
        if self.frame_gap8_timestamp_us < self.state_gap8_timestamp_us:
            raise ValueError("state timestamp should not be newer than the frame timestamp in this toy contract")

    @property
    def state_age_ms(self) -> float:
        return (self.frame_gap8_timestamp_us - self.state_gap8_timestamp_us) / 1000.0
