# DDND GAP8 live deployment

Final INT8 model is flashed through tinympc-nanocockpit on the VM. Three resumed flash-boot samples show sequences 2, 5, 8, approximately 1.45 s inference, and camera I2C flags=1. The LED is set during inference and cleared before UART transmission. Temporary per-core diagnostics are removed.

OpenOCD resets and halts during init, so verification resumes in the same session. Do not use separate init/read/shutdown as passive observation. An untouched power-cycle and host UART decode are not independently verified. No STM32 changes or flight.

Earlier stalls cleared after adapter reattachment; no established software root cause. Model and kernel hashes match the prior exact-output hardware release. Board init uses direct frequency calls at150MHz FC/100MHz CL, with no voltage change or delay; workspace32KiB and stacks2KiB per core.
