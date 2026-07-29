# PLAN — GitHub setup for esp-loco-control

**Status:** CORRECTED v2 — reviewed by Codex and Claude Code (both read-only,
2026-07-24). Verdict was **NO-GO as written**; this version folds in every
must-fix finding. Re-review or execute against *this* version, not v1.
**Owner decision:** David. Claude drafted; Codex + Claude Code reviewed.

For overall project state see `STATUS.md` §6 (GitHub is the top priority item,
ahead of writing HL-Auto code). This doc is the detailed plan for that step.

> **Secrets note:** this document deliberately contains **no real credentials**.
> Earlier drafts printed the WiFi password inline — that has been removed. Real
> values live only in the (git-ignored) `credentials.h`, never in any tracked
> file including this one.

---

## 1. Goal

Get the `esp-loco-control` folder into a **private** GitHub repo, without ever
committing plaintext secrets — not even once, because git history is permanent
and a later fix does not remove a secret from history. Git currently has
**zero commits** (verified: `git log` reports no commits yet), so the slate is
clean — everything below happens before the first snapshot.

Option B from STATUS: a fresh private repo for `esp-loco-control`, borrowing
the `credentials.h` / `credentials_template.h` split pattern from the archived
`NGR-Automated-Train-Control` repo. The old Blynk-era repo is left alone as an
archive; nothing is merged in.

---

## 2. The secret problem — what must not be committed

Both loco config headers mix committable settings with secrets:

| In the header | Committable? | Notes |
|---|---|---|
| Motor pins, PWM, direction, hall thresholds, voltage thresholds | Yes | Per-loco tuning, safe and useful in git |
| `WIFI_SSID` / `WIFI_PASS` | **No — secret** | Same shared value across all three headers |
| `BLYNK_TEMPLATE_ID` / `BLYNK_TEMPLATE_NAME` | Borderline | Not a credential by itself |
| `BLYNK_AUTH_TOKEN` | **No — secret** | **Per-loco** — Otto and Toby have different tokens |

### Complete secret inventory (verified against the real files)

Five files carry real secrets. This is the authoritative list — the reviews
found the v1 plan had missed two of them and had the wrong path on a third.

| File | Secret | Status in v1 plan |
|---|---|---|
| `LL_LocoConfig_9950011.h` | Otto Blynk token + WiFi pass | listed ✅ |
| `LL_LocoConfig_9950012.h` | Toby Blynk token + WiFi pass | listed ✅ |
| `LL_LocoConfig_Dispatcher.h` **(repo ROOT)** | WiFi pass | listed but **WRONG PATH** — v1 said `reference/`; the file is at the repo root |
| `Reference/LL_MQ_CO_GC_DI_1_2.ino` | commented-out Blynk token | **missed** |
| `PLAN-github.md` (v1) | WiFi pass printed in prose | **missed** — now sanitized (see note above) |

> **Path correction (highest-risk finding).** The dispatcher header is
> `LL_LocoConfig_Dispatcher.h` at the **repo root**. The `Reference/` folder
> (capital R) contains only the dispatcher `.ino`. Any `.gitignore` rule or
> split step written against the v1 path `reference/LL_LocoConfig_Dispatcher.h`
> would match **nothing**, and the real header — WiFi pass inline — would be
> committed. All rules below use the correct root path.

### Files verified CLEAN (safe to commit as-is)

`ngr_app_v1_9_5.py` (Flask console — localhost only, no creds),
`STATUS.md`, `SPEC.md`, `SPEC-voltage.md`, `CLAUDE.md`, `AGENTS.md`,
`CODEX_QA.md` (dummy placeholders only), and the main firmware `.ino`
(contains only the broker LAN IP `192.168.68.x`, a private RFC1918 address
that must ship in the firmware — not a secret).

> **Flask filename note:** the file in the folder is **`ngr_app_v1_9_5.py`**,
> not the `v1_9_3` named in CLAUDE.md / SPEC. v1.9.5 is a later revision that
> (per David) dropped some earlier features — this is known, not a bug, and no
> agent should "restore" it. Fix the stale v1_9_3 references in the docs.

### Two kinds of secret

One **shared** (WiFi, identical everywhere) and one **per-loco** (the Blynk
token, different per loco). A single shared `credentials.h` covers WiFi; the
per-loco tokens need per-loco handling (resolved in §5 Q1).

---

## 2a. What moves, what stays — exact per-symbol mapping

The concrete change the credentials split makes, symbol by symbol, so the
compile-time consistency check has something exact to verify. **Every symbol
below must remain defined and reachable by the `.ino` after the split** —
nothing is deleted, only relocated behind an `#include`.

### Otto (`LL_LocoConfig_9950011.h`) and Toby (`LL_LocoConfig_9950012.h`)

| Symbol | Today | After split | Kind |
|---|---|---|---|
| `LOCO_NAME`, `LOCO_ID` | inline | **stays** in per-loco header | identity, not secret |
| `HALL_POLARITY_INVERTED` | inline | **stays** | per-loco, not secret |
| `WIFI_SSID`, `WIFI_PASS` | inline | **moves → `credentials.h`** (shared) | secret, shared |
| `BLYNK_AUTH_TOKEN` | inline | **moves → `credentials.h`** (per-loco symbol, see §5 Q1) | secret, per-loco |
| `BLYNK_TEMPLATE_ID`, `BLYNK_TEMPLATE_NAME` | inline | **stays** | not a credential alone |
| `VPIN_*` (7 pins) | inline | **stays** | not secret |
| `MOTOR_PWM_PIN`, `MOTOR_DIR_PIN`, `PWM_*` | inline | **stays** | not secret |
| `DIRECTION_*`, `SAFE_DIRECTION_CHANGE_PWM` | inline | **stays** | not secret |
| `NORMAL_PWM`, `RAMP_*` | inline | **stays** | not secret |
| voltage thresholds (`SHUTDOWN_VOLTAGE`, etc.) | inline | **stays** | not secret |
| `HALL_*` counts (Toby only) | inline | **stays** | measured tuning, not secret |

### Dispatcher (`LL_LocoConfig_Dispatcher.h` — repo ROOT, not `reference/`)

| Symbol | Today | After split | Kind |
|---|---|---|---|
| `LOCO_NAME`, `LOCO_ID` | inline | **stays** | identity, not secret |
| `WIFI_SSID`, `WIFI_PASS` | inline | **moves → `credentials.h`** (shared) | secret, shared |

The dispatcher carries **no Blynk token** — WiFi is its only secret.

### Include mechanics (the part that must not break the build)

- Each header that loses secrets gains `#include "credentials.h"` **near the
  top, before any symbol it now depends on is used.**
- `credentials.h` (git-ignored) holds the real values. `credentials_template.h`
  (committed) holds the same `#define` names with dummy values, so a fresh
  clone documents exactly what to fill in.
- **Consistency invariant:** after the split, compiling any one loco profile
  must still see identical values for every symbol it saw before. The only
  difference is *where* a define lives, never *whether* it resolves.

---

## 3. Blynk decision

Blynk is **live**, not dead. The locos can be driven in **manual mode** via
Blynk; it is not used in auto mode.

**Decision:** do **not** retire Blynk as part of this repo setup. Retiring a
working manual-mode fallback is its own work item. Leave Blynk as-is; the
credentials split keeps the (per-loco) tokens out of git. Blynk can be cleanly
removed later, in git, where the change is visible and reversible.

---

## 4. Rotation decision (David, 2026-07-24)

The WiFi password and both Blynk tokens have existed in plaintext across
several local files. The reviews correctly flag that sanitizing docs does not
un-expose a value that was already in the clear locally.

**David's decision: do NOT rotate.** This is a private repo on a home LAN; the
exposure risk is low and the cost is real (rotating WiFi means reflashing both
locos *and* the dispatcher). Blynk-token rotation was also declined. Recorded
here so it is not re-raised each review — it is a settled, deliberate call, not
an oversight. Revisit only if the repo's privacy changes or the LAN is
otherwise compromised.

---

## 5. Resolved decisions (were open questions)

1. **Per-loco Blynk tokens → one shared `credentials.h`, per-loco symbols.**
   `credentials.h` holds shared `WIFI_SSID`/`WIFI_PASS` plus
   `BLYNK_AUTH_TOKEN_9950011` and `BLYNK_AUTH_TOKEN_9950012`; each per-loco
   header maps its own `BLYNK_AUTH_TOKEN` to the right one. One ignored file,
   no per-loco secret files, flashing workflow unchanged. (Both reviews
   endorsed this shape.)
2. **`.gitignore` coverage** — must include `credentials.h`, `.DS_Store` (a
   6 KB one is present at root), and build artifacts (`build/`, `*.bin`,
   `*.elf`, `*.map`). Written first, before any staging.
3. **Sequencing** — see §6; explicit staging + staged-snapshot verification
   closes the "does a secret reach the first commit" path.
4. **`LocoConfig.h` r22-vs-r12 label** — provenance confusion, not a leak.
   The selector references `LL_Auto_r22.ino` while current firmware is r12.
   Resolve when convenient; not a gate for repo creation.

---

## 6. Corrected sequence (nothing permanent until step 7)

Reviews changed this from a NO-GO to a GO **only if these are done in order.**

1. **Write `.gitignore` FIRST** — `credentials.h`, `.DS_Store`, `build/`,
   `*.bin`, `*.elf`, `*.map`. (None exists yet.)
2. **Sanitize the two missed leaks:** strip the Blynk token from
   `Reference/LL_MQ_CO_GC_DI_1_2.ino` (dummy it out); confirm this plan file
   carries no real secret (done). 
3. **Apply the credentials split to all THREE headers** — both loco headers
   **and the root dispatcher header** — per §2a. Real values → `credentials.h`;
   `credentials_template.h` committed with dummies.
4. **Stage explicitly — never `git add .` or `git add -A`.** Add only the
   files intended for commit, so a stray `.DS_Store` or an un-sanitized file
   can't ride along.
5. **Verify the STAGED snapshot** (not just the working tree):
   `git diff --cached`, `git ls-files --stage`, grep the staged content for
   the known WiFi pass and both Blynk tokens (expect zero hits), and
   `git check-ignore -v credentials.h` (expect it ignored).
6. **Read the staged list together** — confirm specs, STATUS, both reviews,
   and the sanitized headers are present; confirm `credentials.h` is absent.
7. **First commit** — first permanent step. Only after 1–6 are clean.
8. Create a new **private** GitHub repo and push.

Steps 1–6 change nothing in git history and upload nothing.

> **Strategy reconciliation (review finding):** STATUS §6 says git-ignore "the
> real credential headers" wholesale; this plan instead **edits and commits**
> the headers sanitized (secrets pulled into `credentials.h`). Follow THIS
> plan — ignoring the headers wholesale would drop all the useful per-loco
> tuning (pins, PWM, thresholds) out of git. STATUS §6 wording is superseded
> on this point.

---

## 7. Out of scope for this step

- Retiring Blynk (§3), WiFi/token rotation (§4 — declined).
- Renaming version-in-the-name files (STATUS §6 item 3 — after the repo
  exists, so history holds the old→new mapping).
- Fixing the r22/r12 label and the v1_9_3→v1_9_5 doc references (housekeeping;
  do with the rename pass).
- Any HL-Auto code.
- The Flask console's no-auth / bind-all-interfaces posture (Claude Code
  noted it; it is a LAN-exposure question, not a committed-secret one).

---

## 8. Review notes — Codex + Claude Code (2026-07-24)

Both reviews ran read-only against the real files. Verdict: **NO-GO as v1
written**, architecture sound, three defects each of which would leak a secret
into the first commit. All are fixed in this v2:

1. **Dispatcher path wrong** (`reference/` vs actual repo root) — highest risk;
   corrected throughout (§2, §2a, §6).
2. **PLAN v1 leaked the WiFi password** in prose — sanitized.
3. **Reference `.ino` had a commented Blynk token** — added to inventory (§2),
   sanitize step added (§6.2).
4. **No `.gitignore` existed** and a `.DS_Store` was present — §6.1.
5. **STATUS-vs-PLAN strategy clash** — reconciled in favour of edit-and-commit
   (§6 note).
6. **Flask file is clean** — confirmed no credentials; it is `v1_9_5`, not the
   `v1_9_3` in the docs.

Confirmed sound: the credentials split shape (§5 Q1), the HL-Auto architecture
(out of scope here — see STATUS §4), and that git has zero commits so no secret
is yet in history.
