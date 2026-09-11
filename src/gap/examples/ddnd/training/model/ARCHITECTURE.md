# DDND released-code replication audit

Source: https://github.com/UAV-Centre-ITC/drone_depth_DDND (pinned vendor/DDND checkout).
Paper: https://arxiv.org/html/2303.10386 (Section III-A, IV-C).

## Reproduced graph

model/student.py reproduces public CNN/networks/my_encoder.py DroneMono2, with
CNN/layers.py ConvBlock and Conv3x3. The only intended numerical change is
resolution-independent bilinear output sizes, replacing hard-coded128x160 sizes.
At128x160 all three output maps and four encoder features are bit-exact under
identical state dicts. Input is grayscale float[0,1]; source normalization is disabled.

Encoder:
- stem1: 1->32 stride2,32->32 stride1,32->32 stride1; each3x3/biasfalse/BN/ReLU.
- stem2:32->32 3x3 stride2, noBN/activation.
- stage0:3 depthwise3x3 dilation1,2,3, each followed byBN only.
- downsample32->64 stride2; stage1 identical dilation1,2,3.
- downsample64->80 stride2; stage2 dilation1,2,3,2,4,6.
No CDC residual, pointwise MLP, attention, GELU, or CDC nonlinearity is active.
Four feature tensors are32x64x80,32x32x40,64x16x20,80x8x10.

Decoder:
-80->40 conv/PReLU; bilinear2x; concatenate64channel skip;104->40 conv/PReLU.
-40->32 conv/PReLU; bilinear2x; concatenate32channel skip;64->32 conv/PReLU.
-32->16 conv/PReLU; bilinear2x;16->16 conv/PReLU.
All conv3x3 zero padding1 biasfalse. Each PReLU has one learned scalar.
Each scale has3x3 1channel logits, bilinear2x resize then sigmoid.
All bilinear operations align_corners=True. Final inference prunes unused auxiliaryheads.

## Audit

204990 parameters:101088 encoder,103902 decoder. Conv weights203256.
Final-only inference179228160 convolution MACs for128x160; interpolation/BN/activation
arithmetic excluded. Full state dict has two auxiliaryheads which are unused by
forward() but trained through forward_train().
See audit.json and audit_student.py. The audit used a Slurm CPU allocation.

## Reproducibility limits

Paper says310K parameters, and747.6KB float weights/201.3KB8bit. Released source
counts differ; no released authoritative trained student checkpoint or generated
GAP8 graph identifies how to reconcile them. Do not invent layers to reach310K
or claim bit-exact reproduction of the authors' final flashed network.

Upstream DroneMono2_onnx wrapper uses224x320 hard-coded sizes, converts sigmoid
to depth1/(0.1+9.9*disp), then pools center20rows into16values. The paper describes
128x160 plus10x10 pooling. Our forward() intentionally returns final sigmoid;
postprocessing is a separately documented deployment decision.

## CADiT

The implementation follows paper channel-correlation formula: align student's
four feature channel widths to[48,48,80,128] using1x1 projections, reshapeN*C,
CCM=softmax(S^T*T) across teacher channels, reconstruct S+S*CCM and penalize MSE.
Teacher features are detached. Correlation/loss run float32 under mixed precision.
Paper does not fully specify layer aggregation or L2 mean/sum convention: we
explicitly choose average MSE over layers. This is documented rather than
represented as a recovered author hyperparameter.

Paper training: grayscaleKITTI192x640,39180triplets/4421validation/697test,
30epochs; Gray Campus Indoor9140images17sequences,128x160,100epochs.
Teacher ImageNet-pretrainedLite-Mono thenKITTI, frozen duringdistillation.
AdamWlr1e-4, weightdecay1e-4, cosineLR; SSIM0.85/L10.15, smoothness1e-3,
CADiTweight0.1, three-scale outputL1weight1. Dataset/teacher availability may
limit exact experimental reproduction and must be stated in the run manifest.
