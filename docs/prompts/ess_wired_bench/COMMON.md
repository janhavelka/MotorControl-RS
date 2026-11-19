# Common execution contract

Every prompt in this set incorporates this contract and root
[AGENTS.md](../../../AGENTS.md). The machine inventory is
[SEQUENCE.json](SEQUENCE.json); the single status record is the status table in
[README.md](README.md). Prompts run in ascending numeric order among ready work.
Each prompt is one complete work unit with its own code, tests, bench runs,
documentation, report and commit. Writing or repairing these prompts does not
start implementation.

## Read and prepare

1. Read root AGENTS, this contract, the active prompt, the reports of the
   prerequisites it names, its entries in [audit.md](audit.md) once that file
   exists, and the [backlog](../../backlog.md) rows the prompt names. The report
   says how each audit finding was applied, carried or rejected.
2. Fetch, inspect the branch, upstream and working tree. Work on the existing
   branch (`main`); create no branch or worktree. Other agents may have uncommitted work: never stash,
   discard, overwrite or commit it. When a file you must change has foreign
   uncommitted hunks, stage only your own hunks.
3. Read the code before the documents that describe it. For every behavior the
   prompt changes, read the implementation, its callers and its tests in this
   repository: core under `include/` and `src/`, the standalone application
   under `examples/probe_cli/` and `examples/common/`, the harness under
   `scripts/`. A guide or an older report is a pointer, not proof.
4. Check that each prerequisite artifact the prompt consumes exists and that its
   checks passed (**usable prerequisite**). A report alone does not make
   unfinished code usable. Name the consumed artifacts in the report.
5. State the prompt's gate (below) and, for bench work, the image, the drives
   and the terminals it will use.

## Check facts and correct errors

The original PDFs in [docs/vendor/](../../vendor/) are authoritative for
documented drive behavior; text extracts can scramble tables. For every
register, bit, unit, range, default, sequence or timing a prompt relies on,
open the PDF page and record the claim as **documented**, **inferred**,
**contradicted**, **unknown** or **measured** (with model `0x4EEA`, firmware
`0x0029` and the date). Keep conflicting claims side by side. The register
ledger `docs/reference/ess_rs_registers.json` stays the address source; regenerate
its tables with `scripts/generate_ess_registers.py` and check with `--check`.

A measurement is evidence for the recorded model, firmware, image, fixture and
settings only. Do not generalize one drive's result to the family, or a
finite free-shaft run to loaded mechanics or electrical timing.

When the bench disagrees with a guide, a ledger, the operation inventory or a
contract, correct that document first with the old claim, the evidence and the
reason, then fix the code, tests and guides in the same checked change. Never
tune a test, a timeout or an admission limit to hide a failure.

## Implement the bounded result

Use the prompt's objective and files as the scope. Keep code, meaningful native
tests, Doxygen, affected guides, the [CLI contract](../../cli_contract.md), the
[operation inventory](../../reference/ess_rs_operations.json) and the
CHANGELOG together.

- The core stays framework-independent, ESS-first and free of heap, clocks,
  retries and I/O. Applications own transport, timing, scheduling and fixture
  control. The `bench` fixture ([guide](../../bench_io.md)) is example code.
- Supported motor behavior belongs in the regular API and ordinary firmware.
  Console, programmatic API and Python harness call the same routes; a bench
  scenario never introduces separate motor semantics or an admission flag.
- No missing feature is a stub that reports success. An unsupported or
  unresolved capability is refused before transmission with a reason.
- Keep the evidence levels separate: sent, acknowledged, read back, drive-
  reported (status/position), controller-timestamped (`bench` edges, runner
  traces) and independently observed (owner's instrument or eyes). An echo is
  not a confirmed write; an acknowledgement is not completed motion; a bench
  output state is not proof that a drive input switched.
- Never automatically replay an uncertain write, motion or enable. Cancel is
  not stop. Loss of communication is not a stop.
- Never silently disable inputs, limits or stops to admit a test. Assign
  function 0 only as an explicit, read-back configuration step.

Subagents work on disjoint files or do read-only review, with a complete brief
that names this contract. The lead owns Git and required CI.

## Bench rules

**Fixture.** COM13, USB `303A:1001`, serial `3C:0F:02:CD:6B:98`; never open
COM15 or any other device. Three ESS23-RS20 drives at addresses 1, 2 and 4 on
one RS485 bus, clamped to the bench with free shafts, 24 V shared supply.
Wired I/O since 18 November ([bench I/O](../../bench_io.md#recommended-wiring)):
`mosfet1` → X0, `relay2` → X1, `relay1` → X2, `mosfet2` → X3, each wired in
parallel to all three drives; drive 4's Y0 is wired sinking to the board
input `aux1`, read **inverted** (Y0 on → `aux1` inactive). Every X terminal of
every drive and drive 4's Y0 are connected; declare them so.

**Authorization.** The owner's statement of 18 November, recorded in
[AGENTS.md](../../../AGENTS.md) and [hardware_bench.md](../../hardware_bench.md),
authorizes every test the work needs on these three drives, unattended
included, at the owner's responsibility: velocity, JOG, input-started motion,
wired I/O functions, homing, limits, segments, long runs, link-loss
experiments, communication changes and persistence writes. Do not stop to ask
before a run a prompt needs. A power cycle, a cable pull or a rewiring needs
the owner's hands (gate G3): ask for it once, with the exact action, and keep
the rest of the prompt moving.

**One operator.** Exactly one process owns COM13 at a time. A bench-running
subagent is the only bench user until it reports; others work read-only.

**Before each live step**, write a scenario record into the run folder: image
and SHA-256, drives, terminals, the exact commands, the write list (every
register and value that will change), bounds (travel, speed, duration, hold
times), prepared stop and restoration commands, expected evidence at each
level, abort conditions and cleanup. Start from a read-only baseline, then
enable and stop, then the smallest motion.

**Shared outputs.** Every bench output drives the same X pin on all three
drives. Before any output turns on, read fresh assignments, polarity and state
of every drive on that line and refuse a combination that can command motion
on a drive outside the case. Software that switches an output for an armed
operation checks the non-members the same way.

**Before every flash or controller reset.** Read back on all three drives that
they are stopped and that no input has a start, trigger, speed, JOG, PV, PT,
home or selector function (7-17) assigned, or have the owner remove the I/O
feed. The board's strapping-pin outputs are not measured during reset, so this
holds for the first flash of a prompt, every reflash and every planned reset.
If the check cannot be established, do not flash.

**Experiments and production.** D4, D8 and D9 authorize bounded experiments;
they do not change production admission. Production support follows the ledger
correction and the evidence, and a new FC10 window needs D10. An experiment
names its exact target, candidate words, bounds, independent stop and
restoration before its first write, and keeps unresolved behavior out of
general admission.

**Host-failure exposure.** A case that deliberately removes the controller,
the harness or the link names what ends motion without them (a finite move's
own end, a drive input, the owner, a later controller recovery) and the longest
exposure. On the clamped free-shaft bench, rotation until recovery is accepted
under D1 and D11; it is stated as exposure, never described as a bound, and
standstill is verified afterwards from fresh reads.

**Restoration.** The baseline snapshot from prompt 01 is the reference state.
Restoration is generated as a per-field difference against it, never from
positional shorthand or assumed defaults. Before a case's first write, show an
admissible restoration route for every field it changes; a case without one
stays NOT RUN.
Each run ends with: bench outputs off; every changed setting restored and read
back, or listed as left changed with its storage class (RAM, saved) and reason;
drives stopped, alarm-free and enabled or as recorded; COM13 closed. A run
whose test passed but whose restoration failed keeps its measurements and is
not a successful session.

**Deviations.** When motion continues unexpectedly, send the prepared
independent stop at once, keep the in-flight evidence and confirm fresh
stopped state on every affected drive; if that cannot be confirmed, end live
work and tell the owner. Diagnosis starts only from a verified safe state. On
any other unexpected result, stop, keep the evidence, and reproduce
the case deliberately with bounded diagnostics that distinguish competing
causes. At most three explicit recovery attempts per scenario; the same error
twice ends the scenario. Write a failing native regression where the failure
can be represented, fix the owning code, rerun the failing case and a short
regression. A later pass never erases failure evidence. If the cause stays
unknown, record it as unresolved with the missing measurement.

**Evidence.** One folder per run: `build/bench/<YYYY-MM-DD>_wb<NN>_<slug>/`
with the scenario record, the harness JSONL, console logs, raw traffic where
captured, the baseline and ending reads, and a `result.json` naming the image,
the commit, the cases and their verdicts. `build/` is not committed: the dated
report carries the summary, the tables and the evidence paths. Never delete
bench evidence when cleaning build caches.

**Timing.** Timestamps bracket events; they do not order them. A drive's
internal transition lies between the last sample that showed the old state
(from its request start) and the first that showed the new one (to its reply
end). `aux1` edges are interrupt times after the board's input filter. Report
every interval on the controller clock; an overlapping or reversed order is a
measured relationship or indeterminate, never a drive fault without a
supported timing requirement.

**Verdicts.** PASS, FAIL, NOT RUN or BLOCKED, per case and per evidence level.
A case whose samples cannot separate competing causes reports
**indeterminate** with what is missing; it counts as not passed. NOT RUN
states why. A simulated, compiled or planned case is never PASS.
Owner observations are recorded as such, word for word where they decide a
result.

## Gates

| Gate | Meaning |
| --- | --- |
| G0 | Software only: no traffic to the bench |
| G1 | Bench reads, settings writes and bench outputs; no commanded motion |
| G2 | Bench motion of any kind the prompt needs |
| G3 | Needs the owner's hands: power cycle, cable pull, rewiring or observation |

A prompt names its highest required gate. Optional owner steps (rewiring for
extra coverage) and conditional ones (a power cycle only if a recovery fails)
are marked as such in the prompt and do not raise its gate. Required G3 steps
wait for the owner; the prompt continues with its other cases meanwhile and
stays Partial until they run. A conditional step that becomes necessary makes
the prompt Partial the same way.

## Owner absence, 20 November to about 4 December 2026

Decision D15. The owner is away and nobody can touch the bench. A drive that
becomes unreachable or needs a restart stays lost for the rest of the
absence and blocks later prompts. Until the owner's return is recorded here,
these rules override the G3 text above:

1. **No waiting.** Do not ask the owner for anything and do not wait for an
   answer. A G3 step (power cycle, cable pull, rewiring, DIP switch, observation,
   instrument, owner question) goes into the README table
   [Deferred to owner return](README.md#deferred-to-owner-return) with the exact
   action and the cases it unlocks; those cases are NOT RUN with the reason
   "deferred: owner away". Continue with the rest; the prompt stays Partial.
2. **Nothing persistent, no communication changes.** No save, no factory
   restore, no write to `0x0013` (address), `0x0014`/`0x0015` (serial
   settings) or any other setting that changes how a drive answers. All other
   writes are RAM only, so the owner's power cycle on return reloads the stored
   values.
3. **No states that only a restart clears.** Do not provoke a fault on a drive
   that has not yet cleared that fault with `alarm-clear`: alarm 5 only on
   drives 2 and 4, no Err1-Err3, no HW-01 reproduction (16 R1), no Q04 (16 R3).
   If a drive still ends in such a state, stop motion on it, record it, leave it
   out of later cases and continue on the other drives.
4. **Snapshot discipline.** Before changing a homing, velocity, JOG or profile
   word, take a fresh snapshot through that field's own restore route and
   restore it on the same drive before selecting another drive or flashing.
5. **Before-flash check without the owner.** "Have the owner remove the I/O
   feed" is not available. If one drive does not answer, a flash may proceed
   only when that drive's last checked read in the same session showed it
   stopped, alarm-free and with no input function 7-17, and no write reached it
   since; record that. Otherwise do not flash.
6. **Relay wear.** Each report states how many relay operations it added (the
   budget in 20 is 10,000 per relay). A stuck relay is worked around by setting
   that input's function to 0 on every drive over RS485.

Prompt-specific safeguards:

- **12:** collision methods stay unresolved; X3 = 11 is never left assigned
  between H10 runs.
- **13:** W0b runs only if a drive-side stop besides the harness path guard is
  available on a free terminal; otherwise W0b is NOT RUN and PT stays
  non-negative only.
- **14:** phase C runs on drive 2, not drive 1.
- **15:** T3 and T9 repeats on drives 2 and 4 only; T5/T6 on one drive,
  decreases first, and tuning stops at any alarm that `alarm-clear` does not
  clear; T12 (encoder resolution change) is deferred.
- **16:** F4, R1, R3, the step 6 owner asks and the drive 1 leg of A2 are
  deferred; R2 relies on `stop fast`.
- **17:** S0 and the software items (native tests) run; every hardware case is
  deferred.
- **18:** L1-L4 and U1 are deferred. G2 (reflash during motion) uses a finite
  move that outlasts the flash, with the known-good image and its esptool
  command prepared.
- **19:** B1-B4 are deferred; B5 and K1 are NOT RUN.
- **20:** the soak and closeout run; the set stays Partial until the deferred
  cases have run.

On return, the owner power-cycles all three drives and the lead runs
`baseline-compare`, then the deferred cases in the README table.

## Verify, finish and report

Run the prompt's focused checks and the affected existing gates: the native
suite (`python scripts/verify.py --mode quick` or the affected CTest targets),
`scripts/check_repository.py`, `scripts/check_promptset.py`, firmware builds
when firmware changes (Arduino `bench_s3_load_timer` and native ESP-IDF, see
the [ESP-IDF guide](../../esp_idf_probe.md)). Remote CI follows AGENTS: firmware,
API, console or harness behavior changes are major and need the four CI jobs
green on the exact final commit; documentation-only and report-only changes
need local checks.

A bench run uses an image built from a recorded commit. Flash only the
recorded board, build into a private PlatformIO build directory when another
agent may be using `.pio/build`, and record the image SHA-256 in the report and
in [hardware_bench.md](../../hardware_bench.md).

**Complete** means every owned deliverable and required check is closed: no
TODO, no success stub, no failed required check, no silently dropped acceptance
item. Missing owner action, vendor facts or instruments may block particular
cases: record **Partial** or **Blocked** with the exact unmet criteria and the
next evidence needed, keep the verified parts, and continue with the next ready
prompt.

Write `docs/reports/<YYYY-MM-DD>_wired_bench_<NN>_<slug>.md` with: objective and
scope; consumed prerequisites; source revisions; facts checked against the PDF
pages; implemented artifacts and decisions; commands and results per evidence
level with verdict tables; restoration inventory; deviations and their
disposition; documentation updated; blockers and next work; commits and CI
run IDs. Update the README status table, the operation inventory dispositions
the run changes, [hardware_bench.md](../../hardware_bench.md) (latest image and
ending state), the [backlog](../../backlog.md) and the [reports index](../../reports/README.md).
Historical reports stay unchanged; corrections are dated addenda.
