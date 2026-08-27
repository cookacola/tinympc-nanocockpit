# Modernization Notes

## Deprecated or stale infrastructure found

No simulator migration such as IsaacGym to Isaac Lab is involved. The stale or heavy parts for novice learning are embedded toolchains, Dockerized GAP SDK / ESP-IDF builds, OpenOCD flashing, ROS2 setup, and physical hardware dependencies.

## Modern replacements used

- Python dataclasses replace packed C structs for the tutorial's first runnable path.
- A JSON config replaces scattered command snippets for the dry-run.
- Unit tests validate constants and semantics before firmware builds.
- Deployment commands are represented as structured data in `build_frontnet_deployment_plan()`.

## Semantics that must remain unchanged

- Frontnet input count is `15360`.
- Frontnet output count is `4`, interpreted as `[x, y, z, phi]`.
- The inference reply preserves the relevant STM32 timestamp when carrying a non-empty network output.
- CPX STREAMER packets use CPX version `0`, function `0x06`, GAP target `0x04`, and Wi-Fi host target `0x03`.
- Deployment order remains STM32 controller, GAP8 Frontnet, NINA bridge, host verification.

## Known unresolved migration questions

- TODO: verify exact paper section and figure references.
- TODO: confirm whether `frontnet-160x32-bgaug` is the only intended deployment network or one of several supported Frontnet variants.
- TODO: confirm the current safest Wi-Fi default for the user's hardware: STA with `aideck.local` or AP with `192.168.4.1`.
- TODO: decide whether tutorial hardware tests should call Docker wrappers directly or remain manual to avoid accidental flashing.
