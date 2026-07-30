# Deviations from Original Implementation

| Area | Original | Tutorial | Why | Risk | Validation |
|---|---|---|---|---|---|
| GAP8 inference | Runs PULP-NN/DORY kernels on GAP8 cluster | Reads `network.h` and models output shape | CPU-runnable first step | Does not validate numerical inference | GVSOC command documented separately |
| Streamer wire format | Packed C structs over CPX/SPI/Wi-Fi | Python dataclasses for metadata and inference reply | Easier to inspect and test | Struct packing bugs are not caught | TODO: add struct-size parser |
| Hardware flashing | `make cload`, `gap8.sh ... flash`, `flash-jtag.sh` | Printed dry-run plan | Avoid accidental hardware mutation | User still must execute commands manually | Deployment plan unit test |
| ROS2 viewer | Publishes ROS messages and supports offboard replies | Not required for minimal path | ROS2 setup is heavy for first tutorial run | ROS topic issues not caught | Manual ROS2 command documented |
| Paper references | Full publication and experiments | `TODO: verify` section / figure placeholders | Avoid inventing claims | Docs need one pass after paper source review | `01_paper_to_original_code_map.md` |
| Wi-Fi configuration | ESP-IDF `menuconfig` | Config only stores host and port for dry-run | Build-time Wi-Fi settings remain in original project | Misconfigured NINA not caught offline | Headless stream test |
