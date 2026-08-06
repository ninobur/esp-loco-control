# CTO3 resource record

This folder gives Claude Code, Codex, and future maintainers direct access to
the primary material used to reconstruct CTO3 intent.

## Authority and interpretation

Sources are not equally authoritative. Use this order:

1. David Brown's explicit current decisions, recorded in
   `../CTO3_INTENT_BASELINE.md`.
2. `CTO_OPERATIONAL_PRINCIPLES_JULY_2026.md` where it has not been superseded.
3. Contemporary field observations and failure records.
4. Historical design discussions and prototype notes.
5. AI-generated diagnoses and proposed fixes, which are hypotheses unless field
   evidence confirms them.

Historical documents intentionally disagree. Do not silently harmonize them.
In particular, centralized dispatcher control, block logic, symmetric marker
bubbles, mandatory AUTO peer participation, and some pairing proposals have
been superseded.

## Included sources

- `OPERATOR_DECISIONS_2026-08-05.md` — current operator decisions that govern
  when historical sources disagree.
- `CTO_OPERATIONAL_PRINCIPLES_JULY_2026.md` — text extraction of the agreed
  July operational-principles Word document.
- `CTO2_BUBBLE_PRINCIPLE.txt` — concise CTO2 bubble behavior and physical-bubble
  proposal; its old dimensions and dispatcher/config assumptions are obsolete.
- `CTO2_FAILURE_ANALYSIS.txt` — revised account of the SOLO/TRAFFIC_CLEAR
  near-miss. Logged symptoms are evidence; causes and fixes are hypotheses from
  a superseded paradigm.
- `MANUAL_VS_AUTO.txt` — source for bicameral Manual/AUTO authority.
- `CTO2_DISPATCHER_MODEL.txt` — historical centralized dispatcher model,
  retained as an explicit non-model for CTO3.
- `LOCOMOTIVE_SITUATIONAL_AWARENESS.txt` — persistent onboard operational
  picture.
- `ANALOGOUS_SYSTEMS.txt` — expectation-based navigation exploration preceding
  QUORUM.
- `SPEED_CONTROL_DISCUSSION.txt` — speed as controlled variable, PWM as actuator,
  and offline learning boundary.
- `PHYSICAL_ENVELOPE_NOTE.txt` — transition from Hall-point separation to full
  consist occupancy. Current dimensions are recorded in the intent baseline.
- `NEW_PARADIGM.txt` — short contemporary reframing note.
- `CTO2_R1_BUILD_NOTES.md` — early 171-marker CTO2 construction record.
- `ROAD_TO_CTO.md` — development chronology and staging record.

## Preservation and sanitization

The iCloud originals were not moved or modified. Repository filenames were
normalized for stable links. The selected text sources contained no credential
assignments or authentication secrets, so their textual content was preserved.
The Word source was converted to Markdown for accessibility; the original DOCX
remains in iCloud.

Firmware/config directories containing Wi-Fi passwords or service tokens were
not copied. Historical source code should be imported only through a separate
sanitization and provenance pass.

`MANIFEST.csv` records repository hashes and source provenance.
