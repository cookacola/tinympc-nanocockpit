# DDND GAP8 graph harness

This is an AutoTiler-free PMSIS application. It executes the original DDND graph,
including dilated grouped convolution, PReLU, align-corners bilinear resize and
skip concatenation, using the signed integer kernels in ../runtime.

Package a calibrated export:
```sh
cd /home/cchen/ddnd-gap8-gates-20260908
python3 package_app.py --export artifacts/export
bash gap8_app/build_gvsoc.sh
```

Weights are held in HyperFlash ReadFS and streamed into a single layer-sized L2
buffer on the fabric controller. All intermediate activations share the
exporter's liveness-planned L2 arena. Eight cluster cores execute each node.
This implementation requires neither a GreenWaves AutoTiler binary/license nor
a DORY-supported substitute for the original architecture.

The fixture app reads input.bin and checks every output byte against independent
golden files. A passing final line is DDND FULL_VECTOR_PASS. Raw output tensors
are signed logits; sigmoid and the physical inverse-depth mapping are downstream
postprocessing, not a replacement of the original mathematical output.

This directory is currently a numerical deployment fixture, not a camera/UART
application or a flight-qualified collision-avoidance release. Clock configuration
is 1200 mV, 246 MHz FC / 175 MHz cluster. Runtime measurements from GVSOC are simulated,
not hardware measurements. No flashing is performed by build_gvsoc.sh.

## Live camera product

Build with `make all LIVE=1 build_dir_ext=_live` under the same SDK environment.
Only weights.bin is put in ReadFS. Camera setup is taken from the previously
flown NanoCockpit HM01B0 source/configuration in ../vendor/live_nanocockpit.
Capture is synchronous: no next CPI capture exists during graph execution or
UART transmission. The camera buffer and layer-weight buffer share storage
after image input has been copied/quantized into the activation arena.
The 160x160 crop is identical to the previous application; rows16..143 are the
128x160 network input. Pixel quantization is round(pixel*128/255), capped127.

UART is transmit-only, 921600baud. Packets are a new DDN1 protocol, incompatible
with the old three-sector DepthGate firmware by design. It sends raw signed
depth logits128x160x1, corner logits8x10x12 and visibility logits1x1x4.
Consumers must apply the declared power-of-two scales and the correct output
activation/mapping. Do not interpret these tensors as three-sector distances.

Header is packed little endian40bytes:
`<4sHHIIIIHHBBBBbbbBI`.
Fields: magicDDN1, version1, header_bytes40, sequence, capture_us, inference_us,
payload_bytes21444, depth_h128, depth_w160, corner_h8, corner_w10, corner_c12,
visibility_count4, signed depth/corner/visibility scale exponents, flags,
model_tag (first8hex of weights SHA256).
Flags bit0 means camera configuration checks passed. Capture time is GAP-local
end-of-frame microseconds. Payload follows in depth/corner/visibility order,
then little-endian IEEECRC32 over header+payload. Total21488bytes per frame.
No arming, flight command, or old DG packet is emitted.

Optional `TILED=1 build_dir_ext=_tiled` allocates a 48 KiB L1 workspace after opening the cluster for supported
convolution nodes, retaining directL2 fallback. Each core has a1KiB task stack;
cluster dispatch errors terminate instead of comparing uncomputed outputs.
Node printf is disabled by default, to avoid corrupting timing with JTAG I/O.
Define DDND_NODE_TIMING to enable diagnostic per-node printing.



Optional gate fixtures are discovered as input_gate.bin and all three
golden_gate_*.bin files. The same arena runs both inputs sequentially;
FULL_VECTOR_PASS is printed only when every output of every fixture matches.
The generated manifest records fixture inputs and golden filenames.

Both modes explicitly configure 1200 mV before changing clocks, matching the
previously flown NanoCockpit application. Dynamic L1 workspace allocation
avoids a large cluster-memory initialization image during startup.

Use LIVE_DEBUG=1 in a fresh build directory for short JTAG diagnostics:
DDND_LIVE_START, DDND_LIVE_READY, and one summary per frame. Input CRC is over
the signed quantized image. Inference timing excludes diagnostic printing.

Hardware-fix release 2026-09-09: defaults150MHzFC/100MHzCL, no PMU change,32KiB dynamicL1 workspace,2KiBper-core stacks, tiling enabled. Two physical fixtures match42888bytes exactly. Live camera was validated over11frames; persistent image flashed successfully, cold boot remains unverified. UART was transmitted but not independently decoded by a host.
