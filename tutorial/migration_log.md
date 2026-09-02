# Migration Log

| Date | Component | Change | Rationale | Verification |
|---|---|---|---|---|
| 2026-06-26 | Tutorial scaffold | Added required tutorial docs and maps | Provide novice-facing deployment path | File presence |
| 2026-06-26 | Minimal implementation | Added CPU-runnable dry-run validator | Teach Frontnet deployment contracts without hardware | `python -m tutorial.tutorial_impl.scripts.run_minimal --config tutorial/tutorial_impl/configs/minimal.json` |
| 2026-06-26 | Tests | Added unittest coverage for constants, replies, and deployment order | Encode tutorial invariants | `python -m unittest discover tutorial/tests` |
| 2026-06-26 | Modernization docs | Documented dry-run, dataclass, and command-plan deviations | Make simplifications explicit | `modernization_notes.md`, `deviations_from_original.md` |
