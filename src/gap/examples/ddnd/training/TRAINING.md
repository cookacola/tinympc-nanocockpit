# Float training pipeline (2026-09-09)

Implemented train_dense_gate.py without modifying baseline.py. All writes occurred on a2r-lab.

Data: sequence-disjoint DroNetV3 train68055/val4141; cached Depth Pro dense inverse labels; gates_10cm train100000/validation5000. Test split never opened.

Dense labels are physical inverse metres: prediction .01 + 9.99 * sigmoid disparity. No median scaling; masked physical inverse MSE. Validation also measures inverse MAE, metric depth MAE, absolute relative depth error, and <=2m metric MAE. These are teacher-agreement metrics, not sensor ground-truth metrics. Gate head uses existing heatmap, offset and per-corner visibility loss, weighted 1,1,.1. Joint loss uses independently batch-averaged domain losses; shorter train domain cycles; each validation set traversed once.

Stages: dense LR1e-4; frozen-depth gate LR3e-4; joint LR3e-5. AdamW weight decay1e-5, gradient norm clip5. 30epochs maximum/stage, validation early stop patience5/min_delta1e-4. Checkpoints best.pt/last.pt preserve optimizer/RNG/epoch for resume. Stage switches use --init (fresh optimizer); --resume requires same stage. Checkpoint model keys are exact baseline.py keys.

Verification: Slurm11144 dense/gate/joint plus joint resume smoke all real samples completed0:0. Float gate stage depth weights AND BN buffers bit exact; gate parameters changed. Smoke metrics not quality evidence.

Full chain: 11145 dense ->11146 gate ->11147 joint (afterok dependencies). Artifact dirs artifacts/dense, artifacts/gate, artifacts/joint. scripts slurm/train_*.sbatch auto-resume last.pt. Signed integer QAT is a subsequent step maintained by the root agent; these scripts deliberately do not fake an unverified QAT mapping.

## Integer graph QAT
train_qat.py loads GraphNet model/nodes/outputs and enables signed fixed-scale fake quantization. Convolution/BN already folded; exponents stay fixed, slopes use Q15. Max10epochs, lr2e-5, patience5. Checkpoints contain graph and optimizer/RNG. Reports weight clipping fraction, rejects nonrepresentable Q15 slopes. --evaluate val/test produces separately held-out metrics; these are fake-quant graph metrics, not exact C-kernel dataset evaluation. Exact integer vs fake-quant accuracy gap remains to be measured independently.

Real-data QAT smoke11150 completed0:0 including resume and host integer whole-node verification after export. Chain11147 ->11152 calibration/hostverify ->11153 QAT ->11154 final export + complete val/test evaluation submitted. Root owns export_verify.py and mixed-domain calibration coverage. Final exports in artifacts/final_export, eval in artifacts/final_evaluation.

## Exact integer evaluation
Evaluator evaluate_integer.py calls all C kernels with per-node independent integer reference checks, while preserving export binaries (SHA256 asserted). Float host sigmoid/depth/corner postprocessing remains explicit. Sampled evaluation stratifies sequence/group, 64 per domain on each val/test split; records every chosen identifier. Reports teacher depth agreement, near overestimate rate, gate accuracy and C-vs-QAT discrepancies. Smoke11159 passed. Full11160 depends on finalexport11154; report artifacts/integer_evaluation/report.json.

Objective caveat: global inverse MSE checkpoint selection does not minimize near-range metric MAE. Early dense epoch4 inverseMSE.02042/nearMAE.4848m, epoch6 inverseMSE.02175/nearMAE.4135m. These represent a real metric tradeoff, not necessarily monotonic degradation. Current task preserves requested objective; do not claim near-range suitability from global validation alone.
