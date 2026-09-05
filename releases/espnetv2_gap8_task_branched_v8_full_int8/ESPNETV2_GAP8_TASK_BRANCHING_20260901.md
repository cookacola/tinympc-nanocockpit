# ESPNetV2 GAP8 task branching (2026-09-01)

## Outcome

`v8_task_branched` implements an obstacle-first split after the first shared
stem. Collision retraining can update only its private stage and three-output
head. The gate stage, five-output affordance/visibility head, corner head, and
the shared stem are frozen, including BatchNorm running statistics.

The collision deployment graph is valid NeMO INT8, lowers through DORY to
PULP-NN, and passes every layer checksum in GVSOC. It was promoted as the
official deployable on explicit user authorization. The promotion records a
quality override because its exact-INT8 collision parity gates do not pass.

## Architecture and ABI

```text
two-frame signed difference, 3x160x160
                 |
       shared stem, 16x80x80 (frozen)
              /                         \
 collision stage, 32x40x40       gate stage, 32x40x40
              |                         |          |
  collision head: 3 logits       gate head: 5    corners: 4x40x40
    left / center / right         affordance +    LT, RT, LB, RB
                                  visibility
```

- Branch point: `after_shared_stem_16x80x80`.
- The Python evaluation ABI remains collision3 + gate5 plus four corner maps.
- The GAP8 obstacle graph emits only three uint8 values; unrelated gate
  channels no longer share its output quantizer.
- Migration from the v6 packed head duplicates the old stage and shared head
  features, then slices the terminal projection at collision3/gate5. A unit
  test verifies that this migration is bit-exact before retraining.
- Firmware-v4 action packets and the TinyMPC `u0` reference-action boundary
  are unchanged.

The bridge exposes five explicit partitions: `encoder` and `global_head` are
the collision encoder/head compatibility names; `gate_encoder`, `gate_head`,
and `corner_head` are the isolated gate path.

## Obstacle-priority retraining

The model was initialized from the gate-preserving v6 checkpoint, with the
previous obstacle-priority v6 stage/head overlaid only onto the collision
branch. Training used 65,900 balanced original obstacle pairs plus 3,360 gate
pairs. The collision objective was ground-truth BCE weight 1.0 plus teacher
soft BCE weight 0.25. Checkpoint selection maximized macro AP on all 28,200
natural original validation pairs. Epoch 11 of 15 was selected.

| Model | Macro AP | Left / center / right AP | Any-risk precision / recall / F1 |
|---|---:|---:|---:|
| Float teacher | 0.8360 | 0.8114 / 0.8771 / 0.8194 | 0.6941 / 0.8353 / 0.7582 |
| Previous unbranched obstacle float | 0.8081 | 0.7767 / 0.8559 / 0.7917 | 0.7402 / 0.7800 / 0.7596 |
| **Branched v8 float** | **0.7917** | **0.7569 / 0.8442 / 0.7741** | **0.7381 / 0.7592 / 0.7485** |
| **Branched v8 exact INT8** | **0.7227** | **0.6954 / 0.7741 / 0.6988** | **0.6829 / 0.6958 / 0.6893** |

The float branch retains 94.7% of teacher macro AP. Exact INT8 retains 91.3%
of the branched float result. It is slightly worse than the prior unbranched
v6 model. The user accepted this tradeoff in favor of the isolated task
architecture, so the strict quality failure is an explicit promotion override.

The actual gate outputs stayed constant throughout collision retraining:

- affordance probability MAE 0.04713 and argmax agreement 98.86%;
- visibility probability MAE 0.06299 and threshold agreement 92.93%;
- corner mean error 5.741 px and p95 error 14.647 px.

These are the migrated gate checkpoint's original validation values. The
legacy combined `collision_probability_mae` gate-check field changes by
design because collision is now its own branch; it is not a gate regression.

## INT8 and GVSOC

- Collision encoder: symmetric signed W8 QAT, uint8 activations.
- Collision head: asymmetric signed W7 QAT, uint8 activations.
- Full validation exact-INT8 versus branched float collision MAE: 0.08931;
  sector decision agreement: 92.85%; any-danger agreement: 89.46%.
- DORY parsed and tiled both graphs and generated PULP-NN C successfully.

| Partition | DORY/PULP-NN nodes | MACs | GVSOC cycles | Max L1 tile | Checksums |
|---|---:|---:|---:|---:|---:|
| Collision encoder | 11 | 10,572,800 | 4,990,211 | 36,352 B | 11/11 |
| Collision head | 7 | 37,785,888 | 4,020,999 | 32,032 B | 7/7 |
| Total | 18 | 48,358,688 | 9,011,210 | - | 18/18 |

At 175 MHz this is 51.49 ms, or 19.42 Hz, and passes the 35-million-cycle
timing budget. Physical GAP8/HM01B0 execution and the sealed test split were
not evaluated.

The generated arithmetic and timing acceptance checks pass. The overall
acceptance remains false because all four frozen exact-INT8-vs-float parity
thresholds fail. The principal remaining compression problem is encoder
activation distortion, not cross-task output-range contention.

## Artifacts and verification

- Float checkpoint, reports, bridge, DORY reports, GVSOC logs, and acceptance:
  `outputs/espnetv2_imav22_gate_rail/gap8_obstacle_gate_branched_v8/`
- QAT checkpoints: `gap8_obstacle_gate_branched_v8/qat_symw8_gw7/`
- Exact NeMO graphs: `gap8_obstacle_gate_branched_v8/integer_symw8_gw7/`
- Full original-validation score:
  `gap8_obstacle_gate_branched_v8/original_obstacle_exact_int8_validation.json`

Verification completed with 45 focused ESPNet/GAP8 tests and the project
quick validator's 3 tests. The immutable promoted bundle is selected by
`outputs/espnetv2_imav22_gate_rail/current_gap8_deployable`; the separate
`current_candidate` float-teacher pointer is unchanged.

## Gate-branch QAT and GVSOC (2026-09-02)

The isolated gate encoder was trained with symmetric signed W8 QAT. The
five-output affordance/visibility head and dense corner head use asymmetric
signed W7 QAT, conditioned on the exact integer gate encoder. Exact NeMO
parity on all 1,400 gate validation pairs is:

- affordance probability MAE 0.02027 and argmax agreement 99.07%;
- visibility probability MAE 0.03291 and threshold agreement 98.50%;
- visible-corner peak mean error 0.356 and p95 1.414 heatmap pixels.

Corner parity uses only manifest-visible endpoints, matching training and the
deployment contract. Unsupervised invisible endpoint heatmaps are retained in
the raw diagnostic (mean 1.732, p95 11.045) but are not gate geometry outputs.

| Gate partition | DORY nodes | MACs | GVSOC cycles | Checksums |
|---|---:|---:|---:|---:|
| Gate encoder | 11 | 10,572,800 | 4,976,448 | 11/11 |
| Affordance/visibility head | 7 | 37,786,080 | 4,022,556 | 7/7 |
| Corner head | 5 | 51,712,000 | 4,143,558 | 5/5 |
| Total | 23 | 100,070,880 | 13,142,562 | 23/23 |

The gate path runs in 75.10 ms at 175 MHz (13.32 Hz). All six frozen gate
parity checks, DORY/PULP-NN checks, layer checksums, and timing checks pass.
The official immutable bundle was revised to include these gate artifacts.
