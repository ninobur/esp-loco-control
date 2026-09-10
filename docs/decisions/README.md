# Decision records

## Purpose

This is a diary, not a rulebook. It exists so that people and future AI
sessions working on this railway — the operator himself later, Sam/CODEX, a
future Claude — can remember what was done and why, without re-deriving it
from scratch. It has one job: let a reader answer, for any piece of the
railway's behavior, *why does it work this way* and *who is actually
accountable for that being true.*

It is not a source of authority. Nothing recorded here binds future action
by itself, and no entry — however it's worded, however confidently it reads
— is a substitute for asking the operator about the situation in front of
you now.

## The rule that matters most

**No entry in this log authorizes an agent to act.** A record describes what
happened. It does not grant permission to do something similar next time.

Before using any past entry to justify, guide, or gate a current decision —
and especially before treating an already-agreed plan as changeable because
a past record seems to point elsewhere — an agent must ask the operator
directly, in this form:

> "Previously we made a decision that was recorded. This is the situation
> and the decision. I face a similar decision. May I use this as a guide to
> make the current decision?"

Ask every time, even when the precedent looks obviously applicable. Citing a
decision back to the operator as if it settles a new question — without his
agreement that it applies here — is exactly the failure this rule exists to
stop. It has happened more than once (see the 2026-08-30 audit, decisions
0002, 0011, 0027).

## Attribution

Say plainly who actually decided or found each thing. Three honest shapes:

- **The operator ruled it.** Use his own words when you have them (a direct
  quote beats a paraphrase). If you don't have a quote, don't invent the
  sound of one — describe what he actually did (e.g. "the operator measured
  X three times") rather than asserting "the operator decided" with nothing
  behind it.
- **An engineer or agent found or fixed it.** Say so. Most of the founding
  batch (0004, 0005, 0006, 0007, 0009 among them) are this: real, dated,
  evidenced field/engineering work, correctly attributed to whoever actually
  wrote the fix — including naming Sam/CODEX when a finding was theirs (0007
  does this correctly).
- **An agent drafted language and presented it as the operator's ruling
  without one.** This is the failure mode. If you can't point to a quote, a
  "Decided by" field, or an action only the operator could have taken, don't
  write it as his decision.

When a record states a principle, separate what was actually decided from
any technique chosen to implement it (see decision 0002, revised 2026-08-30,
for the worked example) — a policy and the engineering mechanism used to
enforce it are not the same thing, and only one of them may be the
operator's.

## Principles carry risk, not just benefit

State a decision's known cost, failure mode, or maintenance burden in the
same entry — not just what it accomplishes. A principle that sounds settled
can still fail quietly if nobody wrote down what it depends on continuing to
be true. When a later entry invokes an earlier one, briefly re-examine
whether that risk still applies in the new context; don't just cite the
number.

## Mechanics (unchanged)

- One record per significant decision or finding. Numbered sequentially,
  never renumbered. **Numbering is branch-dependent** — this repo runs many
  concurrent `agent/*` branches, each capable of its own unmerged run of
  numbers. Check the actual highest number across `main` and any branch
  you'll eventually share history with before assigning one.
- A record is never edited away. When something changes, add a NEW record
  that supersedes the old one, and mark the old one "Superseded by NNNN."
  The old record's Decision/Context/Consequences stay intact; only its
  Status line points forward. (In-place correction of a record's own
  misattribution — as happened to 0002 on 2026-08-30 — requires the
  operator's explicit approval each time; it is not the default.)
- Keep them short — half a page. Reasoning, not transcript.
- Write the record when the decision or finding is made, not reconstructed
  long after.

## Known numbering collision: two records numbered 0075

Two files carry the number 0075:

- `0075-a-rising-reading-is-not-a-false-start.md`
- `0075-publishing-every-passage-is-gated-on-measurement.md`

This is the branch-dependent numbering above behaving exactly as warned:
concurrent `agent/*` runs each allocated 0075 without seeing the other, and both
were kept. **Neither is to be renumbered.** The rule one line up — "numbered
sequentially, never renumbered" — outranks the tidiness of a unique index, and a
renumber would break every existing reference to whichever file lost.

Recorded 2026-09-10, on operator instruction, as documentation only. No
renumbering was attempted and none is proposed.

**When citing 0075, cite the filename, not the number.** 0076 refers to "0075
(the discard fix)", which is
`0075-publishing-every-passage-is-gated-on-measurement.md`. A reader who resolves
"0075" to the other file will follow a chain of reasoning that was never written.

Numbering continued from 0076 with 0077, 0078 and 0079 on 2026-09-10.

## Template

```
# NNNN — Title (stated as a sentence)

Status: Proposed | Accepted | Superseded by NNNN  (date)
Decided by: operator (quote if available) | engineer/agent finding

## Decision
What was decided or found.

## Context
The situation and forces that made this necessary.

## Alternatives considered
What else was on the table, and why each was not chosen.

## Consequences
What follows — including the known risk or maintenance cost, not benefit
only.

## References
Specs, commits, reports, resource files.
```

## Review

A record is re-examined, not just cited, when: new field evidence
contradicts it, the hardware or target it was written for changes (as
happened between 0008 and 0022), or the operator proposes a direction it
would seem to block — that last case is the one this log has to get right,
and the rule above is how.
