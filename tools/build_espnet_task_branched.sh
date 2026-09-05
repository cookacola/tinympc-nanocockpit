#!/usr/bin/env bash
# Native a2r GAP SDK build. Default is compile/link only.
set -eo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
source /home/cchen/miniconda3/etc/profile.d/conda.sh
conda activate frontnet
export PATH=/home/cchen/gap_riscv_toolchain_ubuntu_18/bin:$PATH
export GAP_RISCV_GCC_TOOLCHAIN=/home/cchen/gap_riscv_toolchain_ubuntu_18
export GAP_RISCV_GCC_TOOLCHAIN_BASE=
source /home/cchen/gap_sdk_dory/configs/ai_deck.sh
cd "$repo_dir/src/gap/examples/espnet-task-branched"
if [ "$#" -eq 0 ]; then set -- build; fi
exec make "$@"
