# Candidate inventory

| Candidate | Source recovery | Evidence class | Replay status |
|---|---|---|---|
| DNA | v2.0/v2.1 lineage; exact final flashed image not established | field-bounded | unverified pending common raw sessions |
| QUORUM historical | `31c4898`, `ca27f1c`; original asymmetric fence is preserved as historical | field-demonstrated, safety-rejected | source recovered; adapter not yet parity-proven |
| QUORUM symmetric | new, explicitly separate `QUORUM_SYMMETRIC_EXPERIMENTAL_VARIANT` | replay hypothesis only | contract-tested, not historically faithful |
| NAVI_SIMPLIFIED historical | reconstruction document and untracked build artifacts | failed implementation / design | unverified exact flash equivalence |
| NAVI_SIMPLIFIED experimental | detection → judgment reconstruction | experiment only | contract-tested, not historically faithful |
| RVM continuity manual | `8986c5f`, source plus parity corpus | field-proven + parity replay | recovered; requires raw-stream adapter for competition |
| Continuous landscape | frozen `aligner.py`, October 16 package | replay only | provenance documented; Otto-specific templates |
| CPR | `f9cc9493`–`fca9e5b8`, self-contained package | replay only | portable native data and byte audit recovered |

NAVI_ONE and EWO remain controls only: their available records do not yet make
a fair common causal replay possible. No historical candidate is silently
replaced by an experimental reconstruction.
