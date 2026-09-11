#!/usr/bin/env bash
set -eo pipefail
source /home/cchen/miniconda3/etc/profile.d/conda.sh
conda activate frontnet
export GAP_RISCV_GCC_TOOLCHAIN=/home/cchen/gap_riscv_toolchain_ubuntu_18
export GAP_RISCV_GCC_TOOLCHAIN_BASE=""
source /home/cchen/gap_sdk_dory/configs/ai_deck.sh
cd "$(dirname "$0")"
make all run platform=gvsoc "$@"
