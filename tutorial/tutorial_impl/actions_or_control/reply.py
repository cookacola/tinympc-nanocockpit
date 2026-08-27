"""
Tutorial module: tutorial_impl/actions_or_control/reply.py

Purpose:
    Model the reply sent from the host or GAP8 inference path back toward
    NanoCockpit so novices can understand what must reach STM32.

Original references:
    - Paper: NanoCockpit Frontnet follow-me / human pose experiment;
      TODO: verify section numbers from publisher PDF.
    - Original code:
      src/gap/examples/pulp-frontnet/main.c::inference_task
      src/client/aideck_cpx_streamer/aideck_cpx_streamer/cpx/streamer.py::send_reply
      src/stm32/app/src/frontnet_appchannel.c

Modernization:
    - Original stack: packed C unions and CPX streamer buffers.
    - Tutorial stack: explicit Python function with validation.
    - Preserved: four-output inference vector and STM32 timestamp association.
    - Changed: no radio transmission or low-level packet packing.
"""

from dataclasses import dataclass
from typing import Optional, Sequence

from tutorial.tutorial_impl.sensing_or_observations.contracts import InferenceStamped, StreamerMetadata


@dataclass(frozen=True)
class Reply:
    """A compact representation of the streamer reply buffer."""

    reply_frame_gap8_timestamp_us: int
    reply_frame_id: int
    inference: InferenceStamped


def make_inference_reply(metadata: StreamerMetadata, network_output: Optional[Sequence[float]]) -> Reply:
    """Create the reply semantics used by the original host-side streamer client."""

    metadata.validate()
    if network_output is None:
        inference = InferenceStamped(0, 0.0, 0.0, 0.0, 0.0)
    else:
        if len(network_output) != 4:
            raise ValueError(f"Frontnet reply must contain four values, got {len(network_output)}")
        inference = InferenceStamped(metadata.state_stm32_timestamp, *[float(v) for v in network_output])
    inference.validate()
    return Reply(
        reply_frame_gap8_timestamp_us=metadata.frame_gap8_timestamp_us,
        reply_frame_id=metadata.frame_id,
        inference=inference,
    )
