"""
Tutorial module: tutorial_impl/execution/deployment_plan.py

Purpose:
    Generate the ordered build, simulation, flash, and verification commands
    used to deploy NanoCockpit Frontnet.

Original references:
    - Paper: NanoCockpit deployment on Crazyflie + AI-deck; TODO: verify
      section numbers from publisher PDF.
    - Original code:
      README.md
      src/stm32/README.md
      src/gap/README.md
      src/gap/gap8.sh
      src/nina/README.md
      src/nina/esp.sh
      src/nina/flash-jtag.sh

Modernization:
    - Original stack: README snippets plus shell aliases and Docker wrappers.
    - Tutorial stack: declarative, testable command plan.
    - Preserved: component order and command semantics.
    - Changed: hardware flashing is listed as dry-run text unless explicitly
      requested by a human outside this tutorial.
"""

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List


@dataclass(frozen=True)
class DeploymentStep:
    name: str
    component: str
    command: str
    requires_hardware: bool
    purpose: str


def build_frontnet_deployment_plan(repo_root: Path, host: str, port: int, radio_uri: str) -> List[DeploymentStep]:
    repo = str(repo_root)
    return [
        DeploymentStep(
            "prepare-python-client",
            "host",
            "python3.9 -m venv venv && source venv/bin/activate && "
            "pip install -r requirements.txt -e src/client/aideck_cpx_streamer -e src/client/crazyflie-clients-python",
            False,
            "Install the Python packages used for cfclient and CPX stream viewing.",
        ),
        DeploymentStep(
            "build-stm32-controller",
            "stm32",
            f"cd {repo}/src/stm32/app && make clean all",
            False,
            "Compile the Crazyflie STM32 follow-me controller and UART inference receiver.",
        ),
        DeploymentStep(
            "flash-stm32-controller",
            "stm32",
            f"cd {repo}/src/stm32/app && make cload CLOAD_ARGS=\"-w {radio_uri}\"",
            True,
            "Flash the custom STM32 app over Crazyradio.",
        ),
        DeploymentStep(
            "validate-gap8-frontnet-gvsoc",
            "gap8",
            f"cd {repo}/src/gap && ./gap8.sh examples/pulp-frontnet clean all run platform=gvsoc",
            False,
            "Run the Frontnet example in GVSOC when NETWORK_TEST_INPUT is enabled.",
        ),
        DeploymentStep(
            "flash-gap8-frontnet",
            "gap8",
            f"cd {repo}/src/gap && ./gap8.sh examples/pulp-frontnet clean all flash",
            True,
            "Flash the PULP-Frontnet application to the AI-deck GAP8.",
        ),
        DeploymentStep(
            "build-nina-cpx-bridge",
            "nina",
            f"cd {repo}/src/nina && ./esp.sh build",
            False,
            "Build the ESP32 NINA CPX Wi-Fi bridge.",
        ),
        DeploymentStep(
            "flash-nina-cpx-bridge",
            "nina",
            f"cd {repo}/src/nina && ./flash-jtag.sh",
            True,
            "Flash the ESP32 NINA CPX Wi-Fi bridge over JTAG.",
        ),
        DeploymentStep(
            "verify-stream",
            "host",
            f"python tools/headless_stream_test.py -host {host} -port {port} -frames 5",
            True,
            "Confirm the AI-deck streams frames and metadata to the host.",
        ),
        DeploymentStep(
            "open-crazyflie-client",
            "host",
            "cfclient",
            True,
            "Connect to the Crazyflie, check parameters, and command the demo safely.",
        ),
    ]


def dry_run_lines(steps: Iterable[DeploymentStep], include_hardware: bool) -> List[str]:
    lines = []
    for index, step in enumerate(steps, start=1):
        if step.requires_hardware and not include_hardware:
            prefix = "SKIP hardware"
        else:
            prefix = "RUN"
        lines.append(f"{index}. [{prefix}] {step.name} ({step.component})")
        lines.append(f"   purpose: {step.purpose}")
        lines.append(f"   command: {step.command}")
    return lines
