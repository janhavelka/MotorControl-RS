# Finite Python bench scenarios

The [cross-platform qualification matrix](archive/reports/ess_release_29_2026-10-05.md) records
bounded combined load, owner-delay, result-pressure and finite feature campaigns
using these helpers. Its evidence archive includes exact plans and orchestration.
After a long phase or explicit recovery, refresh the prerequisites of the next
operation: settings require fresh identity for the current target generation.
A rejected admission is not permission to relax that check or replay a write.

`scripts/bench_scenarios.py` selects one finite experiment on the ordinary
firmware. Its default `quick` performs ten one-attempt probes, typed
identity/configuration/state reads and passive diagnostics. It sends no motor
write. Existing `bench_probe.py` feature checks remain usable; `bench_motion.py`
uses the same session/evidence owner. All use the existing strict `Console`
parser and public firmware commands. No second serial protocol engine exists.

```powershell
python scripts/bench_scenarios.py --list
python scripts/bench_scenarios.py --out build/bench/quick-new
python scripts/bench_scenarios.py --scenario position --phase forward --debug decoded --out build/bench/move-new --plan-only
```

`--list` and `--plan-only` open no port. Remove `--plan-only` to execute the
selected scenario. Every run reserves new `.json` and `.jsonl` files before
opening COM13; existing files are never overwritten. The wired-bench scenarios
use a run folder instead ([below](#wired-bench-scenarios-and-run-folders)). Use a different prefix for
a deliberate rerun and retain failures. Optional `--firmware FILE` records the
supplied image's hash; console version does not attest the flashed image.

| Scenario | Exact selection / limits | Cleanup and qualification |
| --- | --- | --- |
| `quick` | Default; `--count 1..100`, `--interval 0..1` seconds | Motor reads only; requires unloaded host |
| `observations` | Same bounded count/interval | Explicit state/health refresh and passive age checks |
| `load` | Explicit `--load WORK_US OWNER_DELAY_US CONSOLE_BYTES`; existing fixture bounds, count/interval | Require measured work/lines/service delay; retain measured window before restoring original host load |
| `configuration` | One typed configuration and I/O read | No settings write |
| `native-read` | `--native driver\|io\|control\|segment\|tuning`; `--native-args` use existing typed grammar | Reject setter arguments; inspect and release correlated result |
| `settings` | `--setting input-filter\|lock-delay --value N`; exactly one native-unit difference | Save raw group, refresh existing prerequisites, change once, read back, rebuild invalidated evidence, restore once and compare the complete raw group |
| `position` | `--phase forward\|absolute\|return`; selected `--address`, 100 native increments at 60 rpm, configured ramps | Same-session stop, fresh standstill and original profile restoration; absolute/return require the existing raw fixture window |
| `actions` | One explicit initially-enabled stationary release/enable cycle | Ends enabled; initial released state refuses before an action write |
| `stop` | `--stop-policy normal\|direct`; one finite 250-increment move | Require new RUNNING report, interrupted move result, stop and standstill, then restore original profile |
| `angle` | Explicit `--arguments` matching public wrapped-angle grammar; optional stop policy | Existing coordinate/firmware gates; one stop and profile restoration only after qualified standstill |
| `velocity` | Explicit public `--arguments`, finite duration and stop policy; timeout must include duration | Existing sign/ramp/stop gates; hardware execution remains unqualified. Typed raw parameter reads are available separately; these do not qualify velocity execution |
| `homing-prerequisites` | One caps/wiring/I/O check | No homing execution; method-specific admission still required. Existing explicit finite `bench_probe.py home` campaign remains available behind its gates |
| `discovery` | `--discovery` with existing bounded candidate/budget grammar; default selected ESS/current tuple | Require COMPLETE, retained findings and actual original host tuple/selection after restoration; incomplete scan fails without implicit recovery |
| `persistence-plan` | Explicit `--persistence-kind save\|factory-restore` | Local plan; no nonvolatile write or durability claim |
| `persistence` | Same explicit kind; existing backup/recommissioning gates | One deliberate action and verification; no replay, automatic restart or inferred durability |

Single-attempt scenarios report count 1 and reject `--count`/`--interval`.
Scenario-specific arguments cannot silently enter another scenario. No stress
mode sweeps setters, persistence or motion. `--address` is 1..247. The
`settings`, `position`, `actions` and `stop` scenarios first select that drive
with `useaddr`, as writes and moves apply only to the selected drive. They refuse
to change drives while another drive's profile snapshot still needs restoring.
`useaddr` resets the host's wiring declarations to the board table for a
listed drive, otherwise to unknown; declare the terminals of an unlisted drive
again before moving it. The tuning and control setters require a fresh
checked identity read of the selected drive reporting model `0x4EEA` and
firmware `0x0029`, the owner-confirmed ESS23-RS20 identity of drives 1, 2 and
4 (`ProbeApp.cpp` `exactModelQualified`, [13 November](reports/2026-11-13_multi_drive_settings_and_soak.md)).
Another identity is refused before TX. Corrected 18 November 2026: this
paragraph used to say the setters were qualified for node 1 only. The tuning
input-filter experiment also needs an input policy: fresh passive, unconnected
inputs, or four `configure` records. On the wired bench (X0-X3 connected since
18 November) pass `--input-facts configure`: before each write (the change and
the restoration, because the change clears the records) it calls
`bench_probe.install_input_facts`, which reads `read config` and `read state`,
installs one `configure` record per input with that read's function and
polarity and adds no fact. Without it, `--setting input-filter` refuses to start
on a drive whose terminals the board declares connected (or with an invalid
table), before any write. `lock-delay` has no input condition and takes no
`--input-facts`; the option cannot be combined with `--declare-unconnected`
([wired bench 03](reports/2026-11-18_wired_bench_03_connected_setters.md)).
Never pass `--declare-unconnected` on that bench. That option declares all six
terminals of the selected drive unconnected, after `useaddr` when the drive
changes. It is refused,
before any declaration or drive read (for another drive, after the local
`useaddr` and a `wiring` query), when the board table marks a terminal of that
drive connected, whatever its current source, so an earlier host override
cannot unlock it. It is also refused when the table is invalid or the `wiring`
reply lacks the board keys (an image older than
[wired bench 02](prompts/ess_wired_bench/02_fixture_wiring_declarations.md)).
It is admitted with an absent table or an unlisted address. On the wired bench
images drives 1, 2 and 4 are listed with connected inputs, so it is refused
there. `angle` still runs on node 1 only. `--timeout` is a
finite command bound up to 60 seconds; existing operation/request deadlines are
unchanged. The reusable motion phases use their fixed five-second command bounds.

Examples of explicit qualified inputs used in the [scenario evidence](archive/reports/ess_release_24_2026-10-05.md):

```powershell
python scripts/bench_scenarios.py --scenario observations --count 2 --out build/bench/state-new
python scripts/bench_scenarios.py --scenario native-read --native segment --native-args position 15 read --out build/bench/record-new
python scripts/bench_scenarios.py --scenario load --load 1000 0 64 --count 10 --out build/bench/load-new
python scripts/bench_scenarios.py --scenario settings --setting input-filter --value 3 --out build/bench/filter-new
python scripts/bench_scenarios.py --scenario position --phase forward --debug decoded --out build/bench/position-new
python scripts/bench_scenarios.py --scenario stop --stop-policy normal --debug decoded --out build/bench/stop-new
python scripts/bench_scenarios.py --scenario discovery --discovery addresses 1 1 tuple 115200 8N1 requests 2 results 2 --out build/bench/scan-new
```

These are separate experiments, not a command chain to execute blindly.
The filter example applies only when the checked original value is 2 and the
existing passive/unconnected-terminal policy permits 2/3. Unknown wiring is not
overridden. Settings refreshes must observe the changed native group before
rebuilding dependent I/O/configuration evidence; otherwise cache invalidation
correctly refuses restoration. Lock-delay experiments obey the existing exact
model and stopped-state policy. A failed or uncertain update keeps its backup
and exact progress; the tool does not automatically restore an unknown update.

One session owns connection, debug selection and artifacts until cleanup ends.
Before/after snapshots contain fixed local diagnostics plus three explicit typed
read kinds (discovery uses passive snapshots). Local console configuration and
drive configuration occupy separate keys. CPU availability/percentages, resource
watermarks, capture/owner gaps, protocol/parser evidence, operation IDs and
latencies retain their original meanings. Missing workload evidence fails.
Successful probes also require the recorded request/reply bytes to agree with
the CRC, address, model window, decoded model and reported parser disposition.
Ordinary read/configuration/motion scenarios compare before/after configuration;
a deliberate factory restore instead preserves its changed readback.

The JSONL stream flushes each record and defaults to 20,000 records/16 MiB, with
a 1 MiB per-record bound. CLI maxima are 200,000 records/64 MiB. Capacity exhaustion,
short file writes and disk/flush errors interlock subsequent evidence/traffic.
The independent summary retains fixed snapshots, cumulative failure counts,
the last 16 campaign records and last 32 motion-command records; no unbounded raw
history is held in memory. Raw lines and traffic events stream incrementally.
Asynchronous debug display drops remain separate from protocol failures.

On failure, no new experiment work or success-only motor reads are issued.
Only passive diagnostics are collected while framing remains intact. An accepted
finite move may receive one preplanned stop; a stop already attempted is never
replayed. Standstill requires fresh non-running, alarm-clear and zero raw-speed
reports, using at most ten reads under one observation deadline. Stop/restoration
failure remains failed even if another cleanup step succeeds. Broken framing,
disconnect/restart, stale IDs or evidence failure leave cleanup unknown and
prevent reuse of that connection. Host recovery is a separate explicit operation.
An interrupted startup read also blocks further traffic. Failure while closing
the owned port makes the session fail; simultaneous work/startup and closure
errors retain the primary `error` and separate `owner_error`.

The [scenario audit](archive/reports/ess_release_24_audit_2026-10-05.md) records
these failure regressions and the repeated COM13 finite functional checks.

Uncertain movement is never retried. A volatile profile backup is preserved by
same-session restoration, but cannot survive a board restart. Historical archive
imports, missing native setters, switch-dependent methods, unsupported
pair writes and restart/persistence fixtures remain the named owning gaps in
[the coverage matrix](ess_api_cli_coverage.md). Functional reports do not become
independent physical shaft or electrical measurements.

The explicit read-only velocity-parameter campaign uses the same normal owner:

```sh
python scripts/bench_probe.py --port COM13 --log build/bench/velocity-read-new.jsonl typed-read --kind velocity_parameters
```

Confirm the actual bench device first. This reads the three JOG words, inspects
and releases the result, with no staging, start, save or automatic qualification.

For the six homing parameter records, use the same explicit read-only campaign:

```sh
python scripts/bench_probe.py --port COM13 --log build/bench/home-read-new.jsonl typed-read --kind homing_parameters
```

This reads seven words and inspects/releases their checked result. It does not
qualify or start homing; both offset words remain uninterpreted raw evidence.

## Wired-bench scenarios and run folders

Prompt 04 of the [wired bench set](prompts/ess_wired_bench/04_bench_harness.md)
added five scenarios that each own one run folder. They replace the
18 November scratch scripts `io_map.py` and `y0_toggle.py`.

| Scenario | Work | Verdicts |
| --- | --- | --- |
| `input-map` | Outputs off and declaration check, then per output (`--outputs`, default all four): interlocked `bench on NAME --hold-ms` (default 2000), `read state` of drives 1, 2, 4, `bench off NAME`, `read state` again. No drive write unless `--change`. Aborts on an alarm or RUNNING flag | `declaration_agreement` (board table against `FIXTURE`, local) and `measured_map` (status bits against `FIXTURE`, drive-reported and controller-timestamped) are separate cases. `--expect-refusal OUTPUT` asserts an interlock refusal instead |
| `y0-map` | Drive 4: `io read`, `read state`, Y0 = 9 with the stated ASSIGN facts (`indicator either`), `--cycles` (default 5) custom on/off with `--phase-s` (default 3 s), Y0 back to 0 by the restoration plan; `debug raw` on | One `aux1` edge per custom write in the inverted sense, none for the Y0 writes (controller-timestamped); output status follows (drive-reported); edge timing reported, not judged |
| `baseline-compare` | Every baseline read of each `--drives` drive (61 per drive, drive selected first) | Each field equal, differs or unknown; differences listed per field |
| `baseline-capture` | Refuses with an output on, a drive running, alarmed or unreadable, or recovery required; then a fresh capture in the baseline schema | Values and dispositions against the committed file; `rebaselined.json` stays in the folder |
| `restore` | Optional case write `--change`; every baseline read; the restoration plan writes each differing planned field through its ordinary setter with stated facts, each read back; every read again | Acknowledged and read back |

**Interlock.** `bench_records.bench_on` is the only harness function that
sends `bench on` (also behind `bench_probe.py bench on`). Before the ON it
reads `io read` then `read state` of every drive the board table declares
connected on the switched terminal (the state last, so the motion and alarm
sample is the newest) and takes one `bench` view (the controller time `now_us`
and the outputs). The decision uses the nine I/O words (polarity in word 0,
X0-X3 functions in words 1-4) and the state. Declaration agreement and the
session's input agreement (`check_input_agreement`: `io read` against
`read config` on each drive, once per session, outside the age window) must
pass, and no output may be on. Each drive is decided by the first matching row:

| Row | Participant | Decision |
| --- | --- | --- |
| L1 | Missing, silent, unreadable, a read older than `max_age_ms`, unknown input/output/motion bits, or the interlock reads not the last bus traffic before the ON | Refuse |
| L2 | Alarm code or alarm flag | Refuse |
| L3 | Named running member whose fresh reads and pending operation match its address, operation ID, line, terminal function and polarity and intended effect | Admit |
| L4 | Running in any other case; on the whole line, any accepted operation still pending on the console other than the named running member's | Refuse |
| L5 | Switched terminal holds 7-13 | Refuse |
| L6 | Switched terminal holds 14-17 and the drive holds 12 or 13 | Refuse |
| L7 | Switched terminal holds 14-17 otherwise, or 0-6 | Admit |

Polarity relaxes no row. Freshness follows
[decision D14](prompts/ess_wired_bench/README.md#decisions): ordering is the
primary guard. Between the first interlock read and the ON the console may send
only the interlock's reads (with their releases), one local `drv` and one
`bench` view; the reads must have consecutive controller operation IDs, `drv`
must report the last of them as the latest admitted operation, and with no
named member the owner has no request pending. Otherwise row L1 refuses. An
output already on, or any other pending operation (row L4), refuses too. The
read age is a 1000 ms backstop: it runs from each read's request start
(`started_us`) to the bench view plus the host time after it, and after the ON
the reply's `on_us` gives the actual age; an age above the bound switches all
outputs off and fails. Prompt 08's firmware trigger applies the same rule and
bound to its own read times. Measured on 19 November: each `io read` takes
26-27 ms and each `read state` 24 ms on the controller, the console adds
38-52 ms between reads and about 51 ms to the view, so the oldest read is
about 420-470 ms old at the ON on three drives; the earlier 300 ms default was
not reachable. The cases LI01-LI15 in `test/line_isolation_cases.json` are the
shared truth table data for prompt 08.

**Run folder.** `--run-dir build/bench/YYYY-MM-DD_wbNN_slug` is created
exclusively before the port opens. It holds `scenario.json` (schema
`motorcontrol-rs/wired-bench-scenario/1`: image path and SHA-256, commit,
drives, terminals, commands, cases with `required`, `assertion` and
`required_levels`, named running members, write list, bounds, prepared stop,
restoration plan, planned left-changed fields, expected evidence, abort
conditions, cleanup, `max_age_ms`, stated facts), `session.jsonl` and
`summary.json` (from `run_recorded`), `console.log`, `baseline_before.json`,
`ending_reads.json`, `result.json` and scenario files (`input_map.json`,
`y0_map.json`, `traffic.json`, `restore.json`, `baseline_diff.json`,
`baseline_capture.json`, `rebaselined.json`). `--firmware IMAGE` is required;
the console still does not attest the flashed image.

**Result.** `result.json` (schema `motorcontrol-rs/wired-bench-result/1`)
names the prompt, slug, date, commit and image, each case with its verdict per
evidence level (PASS, FAIL, NOT RUN, BLOCKED, indeterminate; the last three
need a reason) and evidence paths, the restoration (`outputs_off`, written
fields with baseline, ending, `restored`, `uncertain`, storage and reason,
drives stopped/alarm/enabled, `port_closed`) and deviations.

**Checker.** `python scripts/bench_records.py check FOLDER` returns two
results and never edits the folder. Integrity: the folder is complete, both
schemas are valid, the image SHA-256 and commit agree in `scenario.json`,
`result.json` and the image file (recomputed when it exists), at least one
case, known verdicts with reasons, evidence paths exist. Session success:
every required case is PASS at each required level (an expected refusal passes
its no-ON assertion), every scenario drive is in the ending reads, stopped,
alarm-free and enabled as required, no uncertain write, every written field is
restored and read back or was planned as left changed before the first write,
outputs off, port closed. Exit 0 both pass, 1 integrity failure, 2 session
failure. Each scenario runs the checker on its own folder and passes only when
both results pass.

**Restoration.** `bench_records.restoration_plan` turns a write list into
ordered steps from the committed baseline: the field's ordinary route (its
`requires` not later than `--image-level`) with facts the scenario states,
I/O ordered so every FC06 intermediate passes the core custom-output
dependency, trigger functions (7-13) first, a start-speed word before its
relation. A null route is admitted only by an experimental record bound to the
image SHA-256, prompt, case, drive, FC10 window and a checked source read of
the run (prompt 10). `restore` writes only fields that differ from the captured
value and whose current value is known, reads each back, and stops at an
uncertain write without replay.

**Re-baselining.** The committed
[baseline](reports/wired_bench_baseline.json) changes only through
`baseline-capture --reason TEXT --prompt NN`. It reads every field fresh,
keeps each field's disposition (a null route is replaced only when the image
now admits one; a route's command takes the new value) and moves the old
header with every per-field change into `history`. Admissible reasons are an
intended persistent change (a save or factory restore in 17), a newly
delivered route, or an owner change to the fixture or drives; an unexplained
difference is a deviation, never a re-baseline. The new file is committed with
the report of the prompt that intended it. `compare` and `restore` always use
the committed file and refuse one whose latest history entry has no reason.

**Correlation.** `bench_records.correlate` reports COMMON Timing intervals on
the controller clock. For an output edge at GPIO time g, a drive's input
transition lies after the request start of the last `read state` sample with
the old bit and before the reply closure of the first with the new bit:
`[earliest - g, latest - g]`. Each `aux1` edge is reported against the request
start (TX record) and reply end (RX `end_us` with its uncertainty) of the
nearest FC06 to `0x004B`-`0x004F` on drive 4. A debug-record sequence gap or
an `aux1` overwrite inside a pairing window makes it indeterminate; an edge
before the reply end is a measured relationship, not a fault.

### Status mapping and input timing (wired bench 05)

[Prompt 05](prompts/ess_wired_bench/05_status_mapping_and_input_timing.md)
added two folder scenarios. Both need `--image-level 05`.

| Scenario | Work | Verdicts |
| --- | --- | --- |
| `status-mapping` | Per terminal (`--terminals`, default X0-X3) on drives 1, 2, 4: function 0 where the power-up function is not 0; status off, interlocked `bench on` 2000 ms, on, off again at polarity 0; polarity bit 1 at function 0 (mapping `unknown`), the same reads, the inverse write; PIN function 14+t, polarity 1 with the measured class stated, the reads, the inverse write; the power-up function. Before every write: `useaddr`, `io read`, `read config`, `read state`, a `bench` view and the prompt's preconditions (outputs off, no function 7-13 on any drive, other inputs 0, terminal t at its predicted bit). An uncertain or ambiguous write stops all stimulus and writes, reads once more and reports the field uncertain. `--mapping-cases record` runs M05 and N01 against the built-in record instead | M01 status follows the output at polarity 0; M02/M03 one class (`after`, `before`) on every terminal and drive at function 0 and PIN; M04 I/O words equal the baseline. `status_mapping.json` keeps every raw status word |
| `input-timing` | One `--filter-value` (prompt 05 candidates) on `--address` (default 1): the filter written through the CONFIGURE path and read back; `--delay-trials` (default 20) interlocked `mosfet2` 200 ms pulses; `--width-trials` (default 10) per `--widths` (default 1-50 ms); `--compare-relay` adds `relay2` (X1) against `mosfet1` (X0) at filter 2. Each trial reads the observed drive last in the interlock, then polls `read state` back to back (`--poll-depth`, default 2; 05 used 1) until 300 ms after the expected OFF (at most 1300 ms when the pulse is not back at 0), with one random pause of 0-80 ms after the first sample (`--phase-seed`). `--traffic off` turns `debug raw` off. The filter is restored from the baseline | F01 filter read back; F02 on/off intervals of every trial (indeterminate when one is missing); F03 per width observed, missed (instant gaps below the pulse width over the pulse and the measured off delay) or indeterminate; F04 relay against MOSFET. `input_timing.json` keeps every sample |

The input-status sample of a `read state` is its `0x0008` step: request start
(`attempted_us`) and reply closure (`latest_us`) from the operation reply, on
the controller clock. These are the same times the traffic records carry, and
every poll's reply is complete, so a dropped `debug raw` display record does
not remove a sample. `bench_records.timing_trial` gives the intervals, the
poll spacing and the width class.

## Stress tool

`scripts/bench_stress.py` runs randomized finite cases on the recorded ESS bench.
It is separate from the named scenarios above and has its own console parser.
It requires Python 3.10+ and `pyserial`. `--help` opens no port.

The tool checks the USB identity, sets homing auxiliary 7 and can change RAM
subdivision. `--mix extended` also resets the MCU during a move. The default
duration is one hour; a new long campaign needs its own test scope.

`--addresses 1,2,4` runs each case on one drive chosen at random. Before a case
on another drive, the tool releases the profile snapshot (stress runs never
restore it), selects the drive with `useaddr` and probes it. It reselects after
an MCU reset. `--declare-unconnected` states that no terminal of any drive is
wired and declares this again after each `useaddr`; use it only when that is
the fixture. As with the scenario option, it first reads `wiring` and refuses
(`SELECT` anomaly, nothing declared) when the board table declares a terminal
of that drive connected, the table is invalid or the image does not report the
board declaration. `--cross-reads` reads another drive's state at each poll while one
moves. The run fails with `OTHER_DRIVE` if that drive runs, alarms or changes
position. After each case the reference position is taken only once the drive
has held still for 2 s: after a stop, firmware `0x0029` can still change its
reported position by one increment for about 1.5 s
([three-drive tests](reports/2026-11-12_multi_drive_write_motion_stress.md)).
`--subdivision` is refused with more than one drive.

Use a new `--out` directory. It holds case results, health samples, a summary
and compressed incident traffic. Subdivision is restored to `--restore`
(default 1000) only after a clean run. Other RAM settings are left as recorded.

On a motion anomaly, the default path records read-only diagnostics and exits.
`--recover` permits the separate bounded recovery and verification moves.
A USB stall has its own path: wait on the same handle, reopen, then try one
normal stop. Motion resumes only if its matching terminal confirms standstill.
A refused, missing, failed or uncertain stop ends the run even with `--recover`.
Stop evidence and retained operation IDs are kept. No uncertain write is replayed.

Offline tests cover this USB-stop decision. They do not qualify the full
campaign, physical stop behavior or USB driver timing. See the
[November investigation](reports/2026-11-02_standalone_promptset_audit.md) and
[rate-ceiling report](reports/2026-11-04_stop_normal_rate_ceiling.md) for actual runs.
