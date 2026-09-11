# Recovered DDND depth + gate GAP8 application

Recovered from `a2r-main:/home/cchen/ddnd-gap8-gates-20260908/releases/final-11174-liveled-20260909` on 2026-09-10. The signed int8 network, runtime and HM01B0 camera setup are preserved. `gap8_app/hex/weights.bin` is 240872 bytes, SHA256 `2be0691fb77bc2109933e3f0b1ec1fa1f7e5b0b82e15028cd944bbbcaf61aa76`. `training/best.pt` is the final QAT best checkpoint; training architecture, supporting data/loss code, graph and export metadata are included. Checkpoint loading and portable training imports were checked with PyTorch 2.12.1 (the old SDK frontnet environment has an incompatible older Torch). Data paths in historical configuration refer to the original lab datasets, which are not bundled.

After sourcing GAP SDK's `configs/ai_deck.sh`, build from `gap8_app`:

```
make clean
make all LIVE=1 platform=gvsoc
```

This builds the camera app and flash image without connecting to a board. Use the repository's existing AI-deck flashing procedure for the image. `make all run platform=gvsoc` with `LIVE=0` runs both stored full-vector fixtures. `build_gvsoc.sh` records the working lab SDK environment; elsewhere source your SDK directly.

Default live UART is **115200 baud, 80-byte DepthGate packets**, compatible with TinyMPC's `depthgate_packet.h`. Header is `90 19 08 44`; CRC32 IEEE covers the first 76 bytes. Status bit 0 means valid camera inference, bit 1 denotes the GAP-local capture timestamp in milliseconds. The sequence wraps at 16 bits. The timestamp accumulates across the underlying microsecond timer wrap.

Depth uses the exact training transform `inverse_metres = .01 + 9.99 * sigmoid(q * 2^-4)`. Each left/center/right value is the maximum inverse depth over all rows and column ranges `[0,53)`, `[53,106)`, `[106,160)` respectively. This represents the nearest predicted optical depth in each third, including floor pixels; it is conservative relative to a percentile but is not a calibrated safety bound. The controller approximates each sector by its center ray.

Corners retain LT/RT/LB/RB order, heatmap argmax with 16-pixel grid offsets clamped to [0,1], and coordinates clamped to the 160x128 image. Visibility logits include the trained **minus 8** offset. These are decoded from the second head, not placeholder values.

UART transmit buffers are allocated in L2 memory for GAP8 uDMA access; the compact packet and raw CRC must not reside on the FC stack.

For raw 21488-byte DDN1 diagnostic frames, clean and rebuild with `LIVE=1 RAW_UART=1`; baud becomes 921600. `tools/read_ddnd.py` reads that diagnostic format. Clean when changing modes because SDK make rules do not track changed compile flags.

Verification on recovery: host kernel tests passed; `tools/test_depthgate.py` matched the recovered Python decoder on both fixtures including CRC and all 15 floats; GAP8 GVSOC matched all 42888 output bytes exactly. The compact live app compiled and linked with the lab GAP SDK. No new physical flight/camera test was performed. Historical physical tests measured approximately **1.49 seconds per inference**; controller integration must account for this delay and remain explicitly opt-in. Depth metrics are teacher agreement, not range sensor ground truth.

`release_manifest.json`, `DEPLOYMENT_STATUS.md`, `hardware_configuration.json`, `live_verification.json`, and `evidence/` are historical release evidence. Their source hashes describe the original release; recovery intentionally changes live packet formatting and adds portable training imports. `recovery_manifest.json` records the recovered files separately. Build products and large training datasets are excluded.
