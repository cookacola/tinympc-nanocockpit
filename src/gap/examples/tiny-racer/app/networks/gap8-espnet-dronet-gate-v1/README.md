# ESPNet DroNet gate v1

This is the selected GAP8 deployment package for two-frame navigation and
NewBeeDrone gate perception. It contains five NeMO/DORY integer graphs: a
shared ESPNet-style encoder, four-corner heatmaps, a gate mask, gate presence,
and DroNet-compatible yaw/collision outputs.

Input is a `160 x 160 x 2` HWC `uint8` tensor ordered as previous frame then
current frame. `manifest.json` records the complete output layout,
quantization, held-out metrics, and MAC-aware architecture comparison. The
helpers in `inc/gap8_perception_output.h` decode corners, presence, yaw, and
collision probability.

Build and run the Tiny Racer firmware with:

```sh
make all NETWORK_NAME=gap8-espnet-dronet-gate-v1 platform=gvsoc CORE=7
```

The generated package occupies about 173 kB of L2 in the NanoCockpit build.
All five individual graphs passed exact DORY/GVSOC checksum validation.

The selected network uses 26,275,200 MACs per frame pair. It was chosen over
the 21,102,992-MAC joint DroNet control because the exact integer ESPNet
deployment retained 32.5% higher gate IoU and 44.4% lower synthetic corner
error for a 24.5% MAC increase. Integer collision AUROC remains the principal
known limitation; see `manifest.json` for the measured value.
