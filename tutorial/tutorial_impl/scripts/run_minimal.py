"""
Tutorial module: tutorial_impl/scripts/run_minimal.py

Purpose:
    Provide the first runnable tutorial command: a hardware-free deployment
    dry-run and source-contract validator for NanoCockpit Frontnet.

Original references:
    - Paper: NanoCockpit Frontnet deployment; TODO: verify section numbers.
    - Original code: README.md, src/gap/README.md, src/nina/README.md,
      src/stm32/README.md, src/client/README.md.

Modernization:
    - Original stack: manually executed multi-component firmware deployment.
    - Tutorial stack: one Python command that validates constants and prints an
      ordered dry-run command plan.
    - Preserved: component order, dimensions, CPX constants, and host topics.
    - Changed: no firmware build, flash, or network connection by default.
"""

import argparse
import json
from pathlib import Path

from tutorial.tutorial_impl.actions_or_control.reply import make_inference_reply
from tutorial.tutorial_impl.evaluation.source_audit import (
    assert_expected_constants,
    load_cpx_constants,
    load_frontnet_contract,
)
from tutorial.tutorial_impl.execution.deployment_plan import build_frontnet_deployment_plan, dry_run_lines
from tutorial.tutorial_impl.sensing_or_observations.contracts import StreamerMetadata


def load_config(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_repo_root(config_path: Path, configured_root: str) -> Path:
    root = Path(configured_root)
    if root.is_absolute():
        return root
    return (config_path.parent / root).resolve()


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate and dry-run NanoCockpit Frontnet deployment")
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--include-hardware", action="store_true", help="Print hardware flash steps as RUN instead of SKIP")
    args = parser.parse_args()

    config = load_config(args.config)
    repo_root = resolve_repo_root(args.config, config["repository_root"])
    expected = config["expected"]

    contract = load_frontnet_contract(repo_root, config["network_name"])
    assert_expected_constants(
        {
            "network_input_count": contract.input_count,
            "network_output_count": contract.output_count,
            "network_l2_buffer_size": contract.l2_buffer_size,
        },
        {
            "network_input_count": expected["network_input_count"],
            "network_output_count": expected["network_output_count"],
            "network_l2_buffer_size": expected["network_l2_buffer_size"],
        },
    )

    cpx = load_cpx_constants(repo_root)
    assert_expected_constants(
        {
            "cpx_version": cpx["CPX_VERSION"],
            "cpx_streamer_function": cpx["CPX_F_STREAMER"],
            "cpx_gap_target": cpx["CPX_T_GAP"],
            "cpx_wifi_host_target": cpx["CPX_T_WIFI_HOST"],
        },
        {
            "cpx_version": expected["cpx_version"],
            "cpx_streamer_function": expected["cpx_streamer_function"],
            "cpx_gap_target": expected["cpx_gap_target"],
            "cpx_wifi_host_target": expected["cpx_wifi_host_target"],
        },
    )

    metadata = StreamerMetadata(
        metadata_version=1,
        frame_id=7,
        frame_gap8_timestamp_us=105000,
        state_gap8_timestamp_us=104200,
        state_stm32_timestamp=4242,
    )
    reply = make_inference_reply(metadata, [0.2, -0.1, 1.4, 0.05])

    print("NanoCockpit Frontnet tutorial dry-run")
    print(f"repo_root: {repo_root}")
    print(f"frontnet_input_count: {contract.input_count}")
    print(f"frontnet_output_count: {contract.output_count} -> [x, y, z, phi]")
    print(f"frontnet_l2_buffer_size: {contract.l2_buffer_size} bytes")
    print(f"cpx_version: {cpx['CPX_VERSION']}")
    print(f"toy_reply_stm32_timestamp: {reply.inference.stm32_timestamp}")
    print(f"toy_state_age_ms: {metadata.state_age_ms:.3f}")
    print("")

    steps = build_frontnet_deployment_plan(
        repo_root=repo_root,
        host=config["host"],
        port=int(config["port"]),
        radio_uri=config["radio_uri"],
    )
    for line in dry_run_lines(steps, include_hardware=args.include_hardware):
        print(line)


if __name__ == "__main__":
    main()
