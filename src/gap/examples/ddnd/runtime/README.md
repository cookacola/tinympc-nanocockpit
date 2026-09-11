# DDND portable signed INT8 runtime

This runtime preserves the operation types in DDND without NNTool/AutoTiler.
It is a correctness reference with explicit HWC buffers, not a DORY-generated
or PULP-NN optimized implementation. Real-time suitability is unverified.

Compile ddnd_runtime.c with C99. Define DDND_GAP8 for PMSIS cluster execution;
fork all cluster cores and call each primitive on every core, in the same order.
Each primitive ends with a cluster barrier. Host builds execute serially.

Activations use symmetric signed INT8 [-127,127], implicit zero point 0.
Convolution weights are OIHW, with input channels per group. Padded values are
zero. Weight and input scales must be powers of two; output shifts equal
output_scale_exponent - input_scale_exponent - weight_scale_exponent.
Bias is INT32 at input_scale * weight_scale, and accumulation is INT64.
Right shifts round nearest, ties away from zero. Negative shifts multiply.
Saturation occurs after requantization. Shifts are per output channel.

PReLU slopes are INT32 Q15 per channel (expand a scalar before export);
input/output scales are equal. Bilinear resize uses align_corners=True, floored
Q16 coordinates, and one final Q32 rounded sum. Concat shifts each input to
the output scale; its values can saturate. Optional sigmoid consumes an
exported 256-entry uint8 lookup table, index input + 128.

Callers validate positive dimensions, divisibility of groups, tensor sizes,
allocation success, and scale ranges. Kernels do not allocate. Conv, resize
and concat must have nonoverlapping input/output storage. PReLU is in-place
safe. Do not recycle a graph activation until all skip consumers complete.

Validation:
    /home/cchen/miniconda3/envs/frontnet-train/bin/python runtime/test_runtime.py

The test compiles with -Wall -Wextra -Werror and compares independently
computed integer fixtures. This establishes host primitive parity only,
not whole-graph, GAP8, timing, memory, QAT or physical flight validation.

The additional ddnd_conv_i8_ohwi entry point accepts OHWI weights and uses
signed four-way dot-product instructions on GAP8. It checks each output bias
plus K*16129 before using an INT32 accumulator, otherwise takes an INT64
fallback. Both layouts produce identical outputs. Inputs/weights must exclude
-128, consistent with the exporter contract. Group/channel tails and unaligned
channel starts are supported. These are hand-written PMSIS kernels, not DORY
generated code; target timing still needs measurement.

The ddnd_conv_i8_ohwi_tiled API adds shared, four-byte aligned workspace
and its byte size. Full weights, bias and shifts are cached there once; each
output row stages its clipped receptive input-row span and output row. It
falls back to direct OHWI when the workspace cannot accommodate all buffers.
With 49152 bytes, 31 of the 32 current graph convolution nodes fit. down2
requires 51360 bytes and uses the direct path. Workspace lifetime must span
all team calls, and it must not alias any graph tensor or payload. This uses
cooperative CPU copies and barriers rather than DMA; measure target timing.
