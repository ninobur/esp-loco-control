# NGR — Standing AI Agent Operating Policy

This policy governs all work in this repository by Claude, Codex, and any other
AI agent. It is the durable record of how agents work on the NGR. It takes
precedence over other instructions in this repository, including
`docs/CLAUDE.md`, and over an agent's or environment's default working habits,
wherever they conflict.

For NGR project context — hardware, the MQTT integration constraint, firmware
organization, and decision records — read `docs/CLAUDE.md`.

---

## 1. Human architectural authority

David is the system architect and final decision-maker.

Agents may analyze evidence, identify problems, develop alternatives, and make
recommendations. They must clearly distinguish recommendations from established
architectural decisions.

Agents must not make material architectural or design decisions on David's
behalf.

If an approved design or implementation does not work, STOP and report what
happened. Failure does not authorize the agent to substitute another
architecture, algorithm, behavior, or implementation strategy.

## 2. Analysis does not authorize implementation

Reading, analysis, review, compilation, simulation, and testing do not
constitute permission to modify project code.

Before modifying code, the agent must explain:

- what it found;
- what it proposes changing;
- why;
- which files would change;
- expected behavior;
- significant risks or uncertainties;
- how the change will be tested.

The agent must then obtain explicit approval before implementation.

**Scope control.** Approval to implement a specific change authorizes only that
change. Discovery of an adjacent problem, improvement, cleanup opportunity, or
additional required change does not expand the authorized scope. Report it
separately and obtain explicit approval before acting on it.

**Deployment authority.** Permission to modify code does not imply permission
to deploy it. Flashing locomotives or other ESP32 devices, modifying running
Raspberry Pi services, changing live MQTT or Blynk configuration, or otherwise
deploying changes to operating NGR hardware or services requires separate
explicit authorization.

## 3. Repository and branch discipline

Before beginning any task that may modify the repository, establish which
branch contains the current project state. Do not assume `main` is current
merely because it is named `main`.

Claude Cloud must create and work from its own task branch based on the branch
containing the current project state. It must not modify the
authoritative/current branch directly unless David explicitly authorizes that
operation.

Do not reorganize, repair, merge, rebase, reset, force-push, rewrite, delete,
or otherwise clean up historical Git structure unless David specifically
requests repository maintenance.

Imperfect historical repository organization is not itself a problem requiring
correction. If the repository is sufficient to perform the engineering task
consistently, leave unrelated history and structure alone.

If an unexpected repository condition is encountered, report it and its
practical consequences rather than autonomously fixing it.

**Rollback.** Before implementing an approved change, identify the known-good
starting commit/build and preserve a straightforward rollback path. Do not make
an implementation unnecessarily difficult to reverse.

## 4. Resource discipline

Work as one agent by default.

Do not spawn subagents, parallel agents, agent teams, or broad parallel
investigations unless David explicitly authorizes them.

Conserve compute, tokens, context, and paid usage. Prefer targeted inspection
of relevant files and history over broad repository exploration.

Do not repeat analyses already established in repository documentation or
supplied context unless verification is necessary for the current task.

If a task appears likely to require substantial additional exploration or
compute, stop and explain what is needed and why before proceeding.

## 5. Evidence must remain independent of expectation

Preserve physical observations, measurements, logs, and other evidence even
when they contradict the expected system state or current theory.

Do not reinterpret an observation merely to make it agree with NAVI, MQTT
context, a map, an existing algorithm, or an architectural expectation.

Observation, expectation, interpretation, and decision should remain
distinguishable.

**Field evidence.** Do not alter algorithms, thresholds, tests, acceptance
criteria, or interpretation merely to make observed results conform to expected
behavior. Unexpected field evidence must be preserved and reported as evidence.
A failed test is information, not authorization to change the design or the
test.

## 6. Ask rather than assume

When uncertainty could materially affect architecture, locomotive behavior,
navigation authority, safety behavior, experimental validity, or repository
history, stop and ask David rather than making the decision independently.

## 7. The repository is the durable project record

Decisions intended to govern future development must be recorded in the
repository. Conversation history alone is not an authoritative or durable
project record.

Important architectural decisions, experimental findings, operating rules, and
implementation decisions that future agents will need must be preserved in
appropriate repository documentation.
