"""
Tutorial module: tutorial_impl/evaluation/source_audit.py

Purpose:
    Read the original NanoCockpit source files and validate the deployment
    constants that the tutorial depends on.

Original references:
    - Paper: TODO: verify exact source-package link.
    - Original code:
      src/gap/examples/pulp-frontnet/app/app.mk
      src/gap/examples/pulp-frontnet/app/networks/frontnet-160x32-bgaug/inc/network.h
      src/nina/main/cpx_types.h
      src/client/aideck_cpx_streamer/aideck_cpx_streamer/cpx/cpx.py

Modernization:
    - Original stack: C preprocessor definitions and Python ctypes enums.
    - Tutorial stack: small parser that checks named constants only.
    - Preserved: values that affect deployment compatibility.
    - Changed: avoids compiling firmware during the minimal CPU-runnable path.
"""

import re
from pathlib import Path
from typing import Dict

from tutorial.tutorial_impl.sensing_or_observations.contracts import FrontnetContract


DEFINE_RE = re.compile(r"^\s*#define\s+(?P<name>[A-Za-z0-9_]+)\s+\(?(?P<value>0x[0-9A-Fa-f]+|[0-9]+)", re.MULTILINE)


def parse_c_defines(path: Path) -> Dict[str, int]:
    text = path.read_text(encoding="utf-8")
    values: Dict[str, int] = {}
    for match in DEFINE_RE.finditer(text):
        values[match.group("name")] = int(match.group("value"), 0)
    return values


def load_frontnet_contract(repo_root: Path, network_name: str) -> FrontnetContract:
    network_header = (
        repo_root
        / "src/gap/examples/pulp-frontnet/app/networks"
        / network_name
        / "inc/network.h"
    )
    defines = parse_c_defines(network_header)
    contract = FrontnetContract(
        input_count=defines["NETWORK_INPUT_COUNT"],
        output_count=defines["NETWORK_OUTPUT_COUNT"],
        l2_buffer_size=defines["NETWORK_L2_BUFFER_SIZE"],
    )
    contract.validate()
    return contract


def load_cpx_constants(repo_root: Path) -> Dict[str, int]:
    cpx_header = repo_root / "src/nina/main/cpx_types.h"
    defines = parse_c_defines(cpx_header)
    text = cpx_header.read_text(encoding="utf-8")

    enum_values = {
        "CPX_T_STM32": 0x01,
        "CPX_T_ESP32": 0x02,
        "CPX_T_WIFI_HOST": 0x03,
        "CPX_T_GAP": 0x04,
        "CPX_F_STREAMER": 0x06,
    }
    for name, expected in enum_values.items():
        pattern = rf"{name}\s*=\s*0x{expected:02X}|{name}\s*=\s*0x{expected:01X}"
        if not re.search(pattern, text):
            raise ValueError(f"Could not verify {name}={expected:#x} in {cpx_header}")
    return {
        "CPX_VERSION": defines["CPX_VERSION"],
        **enum_values,
    }


def assert_expected_constants(actual: Dict[str, int], expected: Dict[str, int]) -> None:
    for key, expected_value in expected.items():
        actual_value = actual[key]
        if actual_value != expected_value:
            raise ValueError(f"{key}: expected {expected_value}, got {actual_value}")
