# Development and bench tools

The [verification entry point](../docs/verification.md) composes the repository's
checks and clean package consumers. Run from the project root:

```sh
python -m pip install -r scripts/requirements-verification.txt
python scripts/verify.py --mode quick
```

`--mode full` also builds the pinned Arduino/IDF firmware configurations.
Missing checks fail. Verification opens no serial port and changes no drive.

| Tool | Purpose |
| --- | --- |
| `verify.py` | Composed native/Python/package checks; optional firmware builds |
| `check_repository.py` | Offline snapshot hashes, maintained documentation links and metadata |
| `check_ess_operations.py` | Register/API/CLI coverage accounting; `--details` prints the full inventory |
| `generate_ess_registers.py` | Generate register tables and documentation from the reviewed ledger; `--check` verifies |
| `generate_version.py` | `sync` generates `Version.h` from `library.json`; `check` verifies |
| `prepare_lichuan_references.py` | Offline `--check` of Lichuan research snapshots/ledgers; `--extract` regenerates search aids; no serial I/O |
| `prepare_lichuan_tl57_references.py` | Separate TL57 research bundle checks using the same private source/frame validation helpers |
| `pio.cmd` | Windows wrapper for the installed PlatformIO toolchain |

## Position-profile archives

The host exports the current checked six-word profile and owns the file.
Firmware receives five candidate words; it never reads a host file or writes
the advisory start word at index 0. Example on the recorded port:

```sh
python scripts/bench_probe.py --port COM13 --address 1 --log export.jsonl motion-profile export profile.json
python scripts/bench_probe.py --port COM13 --address 1 --log stage.jsonl motion-profile stage profile.json --wiring x0 unconnected --wiring x1 unconnected --wiring x2 unconnected
```

Supply wiring declarations only when they describe the installation. The tool
does not change drive input assignments or infer fixture facts from the file.
Export refuses an existing destination. Stage validates the file before opening
the port, checks fresh configuration/identity/profile/state, then submits once.
It leaves the result retained: inspect and explicitly `release ID` when done.
There is no start, save, retry or automatic restore.

The bounded JSON schema is:

```json
{"schema":"MotorControlRS.position-profile.v1","address":1,"order":0,"words":[30,50,50,60,0,100],"model":20202,"firmware":41}
```

`words` are start speed, acceleration, deceleration, RPM and two target words.
`order` is 0 or 1. Model and firmware are optional as a pair. Generations, extra
keys, duplicate keys, non-integer values and files over 4 KiB reject. Export
preserves a readable out-of-range target; stage refuses it as `target_range`.
Archive values above motion policy can stage within the typed wire range;
later moves still enforce their own limits. Host restart requires fresh reads,
not imported provenance. See the [candidate contract](../docs/ess_position.md#candidate-profile-staging).

## Finite bench tools

These tools use the ordinary firmware console and the same strict command/result
parser. Live serial connections require `pyserial`; the fake-serial tests use
the Python standard library. Only one process may own the board's port.

`_bench_evidence.py` is the private pure read/motion-evidence boundary. Shared errors,
wire/age/count helpers and existing `Console` static check names remain available
through `bench_probe`; framing, correlation, deadlines and serial ownership stay
there. See the [dependency and delivery handoff](../docs/archive/reports/2026-10-12_host_evidence.md).

| Tool | Purpose |
| --- | --- |
| `bench_probe.py` | Read-only probes, state/health/load checks and explicitly selected typed feature operations |
| `bench_scenarios.py` | Named finite campaigns; default `quick` is motor read-only; `--list` and `--plan-only` open no port |
| `bench_motion.py` | Explicit finite position/action/stop phases with retained evidence and cleanup |
| `bench_repeat.py` | Bounded comparison of full, verified and stored position setup policies |
| `bench_session.py` | Shared session/evidence implementation imported by the campaign tools |
| `bench_records.py` | Wired-bench records: fixture, shared-output interlock, run folders, `result.json`, run checker, baseline comparison/restoration (snapshot routes for homing and JOG words), edge correlation; `check FOLDER` is offline |
| `bench_outputs.py` | Wired bench 06 `outputs` scenario: drive 4 Y0 cases O1-O11, `aux1` edges against write TX/ACK/closure and state-sample brackets, set/clear events against `0x0007` |
| `bench_stop_inputs.py` | Wired bench 07 `stop-inputs` scenario: X3 functions 4-6 during moves, velocity, homing, held stops, serial stops and group moves; interlocked `mosfet2` pulses with the named running member; Implement 3 trial classification, L1/LR/L2 tables |

`bench_stress.py` is a separate, bench-specific campaign tool. It has its own
console parser and is not covered by the strict parser claim above. It checks
the recorded USB identity, sets auxiliary 7 and can change RAM subdivision.
The extended mix includes MCU resets. `--addresses`, `--cross-reads` and
`--declare-unconnected` run it across several drives. Use `python scripts/bench_stress.py --help`
for options; that command opens no port. See the
[stress tool limits](../docs/bench_scenarios.md#stress-tool) before a live run.
Its `--declare-unconnected` refuses, before declaring, when the board wiring
table declares a terminal of the selected drive connected or is invalid.

Wiring replies: the strict parser requires the `wiring` reply's `sources`,
`board` and `fixture` keys, and `fixture_error` exactly when `fixture` is
`invalid`. It checks the source names and board shape and their consistency
with the declared values and table state: a `board` source must equal the
board value, `invalid_table` must be unknown and `default` is never connected.
Group member views must carry `input_sources`, `output_sources` and `board`
under the same checks. An image older than the board table lacks these keys
and is rejected with a named error. `bench_probe.board_declares_connected(reply)`
is true when the board declaration of the replying drive marks any terminal
connected. `bench_motion.select_drive(..., declare_unconnected=True)`, used by
`--declare-unconnected` in `bench_motion.py` and `bench_scenarios.py`, refuses
before any declaration or drive read when that board declaration marks a
terminal of the selected drive connected (whatever its current source), when
the table is invalid or when the reply lacks the board keys. It is admitted
with an absent table or an unlisted address. See the
[CLI contract](../docs/cli_contract.md#terminal-wiring-declarations).

`bench_probe.install_input_facts(console, address, purpose, mapping="unknown",
facts={})` states the four selected-axis input records a settings write needs
with connected terminals (`configure` or `segment`): it reads `read config` and
`read state`, copies each function and polarity and adds only the per-terminal
facts the caller names (`triggers-inhibited`); it never adds a fact on its own.
`bench_scenarios.py --scenario settings --setting input-filter --input-facts
configure` calls it before the change and again before the restoration, and
`bench_probe.py motion-profile stage FILE --input-facts configure` before the
stage. Refusals the records decide carry `reason`, `purpose`, `read_first` and
`terminal` ([CLI contract](../docs/cli_contract.md#settings-input-facts)).

### Wired-bench run folders

`bench_scenarios.py` has eight wired-bench scenarios: `input-map`, `y0-map`,
`baseline-compare`, `baseline-capture` and `restore` (prompt 04),
`status-mapping` and `input-timing` (prompt 05,
[guide](../docs/bench_scenarios.md#status-mapping-and-input-timing-wired-bench-05)),
and `outputs` (prompt 06, one `--case O1`..`O9` per run,
[guide](../docs/bench_scenarios.md#outputs-wired-bench-06)).
Each needs
`--run-dir build/bench/YYYY-MM-DD_wbNN_slug` (created exclusively; an existing
folder fails before the port opens) and `--firmware IMAGE`; scenarios that
write also need `--image-level NN`, the prompt whose routes the flashed image
has. The scenario record (`scenario.json`) is written before COM13 opens; the
folder then gets `session.jsonl`, `summary.json`, `console.log` (sent `>` and
received `<` lines), `baseline_before.json`, `ending_reads.json`,
`result.json` and scenario files. Each run ends with
`bench_records.check_run` on its own folder and exits 0 only when both results
pass (1 integrity failure, 2 session failure).

```powershell
python scripts/bench_scenarios.py --scenario input-map --case H01 --firmware build/pio_wb03/bench_s3_load_timer/firmware.bin --run-dir build/bench/2026-11-19_wb04_input_map
python scripts/bench_scenarios.py --scenario restore --address 1 --change 1 filters.input-filter 3 --input-facts configure --image-level 03 --firmware IMAGE --run-dir build/bench/2026-11-19_wb04_restore_filter
python scripts/bench_records.py check build/bench/2026-11-19_wb04_input_map
```

- `bench_records.bench_on(console, output, hold_ms, record)` is the only
  harness function that sends `bench on`. It first runs
  `bench_records.interlock`: fresh `io read` then `read state` of every drive
  the board declares connected on the switched terminal, one `bench` view,
  then the line-isolation truth table (rows L1-L7, cases in
  `test/line_isolation_cases.json`). `check_input_agreement` (`io read`
  against `read config`) runs once per session, outside the age window. A
  refusal sends no ON. `bench_probe.py bench on OUTPUT HOLD` goes through it
  too. Ordering is the primary guard (D14): the reads must be the last bus
  traffic before the ON (consecutive operation IDs, `drv` reports the last of
  them), and any other pending operation refuses. The read age is a 1000 ms
  backstop (`--max-age-ms` records another value); on 19 November the oldest
  read was 447-470 ms old at the ON
  ([wired bench 04](../docs/reports/2026-11-19_wired_bench_04_harness.md)).
- `--change ADDRESS KEY[INDEX] VALUE` is one case write through the field's
  ordinary setter; its restoration plan (from the committed baseline) is in the
  scenario record and runs at the end. Facts are stated, never inferred:
  `--input-facts configure|segment` and `--assign-facts TERMINAL FACT...`
  (`x3 triggers-inhibited`, `y0 indicator either`). A field without an
  admissible route makes the case NOT RUN before any write.
- `bench_records.before_flash_check(console)` is COMMON's check before a flash
  or controller reset.
- `baseline-capture --reason TEXT --prompt NN` writes a fresh capture and the
  re-baselined candidate into the run folder only; the committed baseline
  changes only when the prompt that intended it commits that file.

Use a new evidence prefix for each run. A selected settings/motion campaign can
write to or move the drive; a passing probe is not preparation for every feature.
See [scenario bounds and cleanup](../docs/bench_scenarios.md),
[motion checks](../docs/functional_bench.md) and
[the repeat-motion comparison](../docs/archive/reports/2026-10-05_repeat_motion_timing.md).
Uncertain writes are never automatically replayed.
`bench_probe.py typed-read --kind velocity_parameters` performs one checked raw
JOG-parameter read plus retained inspection/release; it does not stage or start
velocity. See [typed reads](../docs/ess_reads.md#native-velocity-parameter-read).

`bench_probe.py typed-read --kind homing_parameters` reads six native homing
records as seven raw words with retained inspection/release. It does not stage
homing, assemble an offset, establish an origin or renew qualification.

`bench_probe.Console.set_position_start_speed(native)` performs one explicit
selected-axis native0..3000 setting attempt through `motion-profile start-speed`.
It returns checked prior/echo/readback evidence and explicitly releases the result.
`begin`/`wait` can retain the device result for inspection; neither path retries
an uncertain write or qualifies physical speed/activation.

`bench_probe.Console.set_homing_auxiliary(choice)` selects one stored choice0..11
through the ordinary `home auxiliary` operation and returns checked prior/echo/
readback evidence before explicit release. `begin`/`wait` support retained results.
No home, active-policy qualification, save or uncertain-write replay follows.

Ordinary stop replies identify the replaced result with `replaced_operation_id`
and the reserved pool with `stop_escalation`. The host validates both fields
before removing that result from its live retention map. The old handle and run
log keep the terminal evidence, including uncertainty. Two primary stops and two
reserved fast escalations are separate from group-stop storage. Pending stops
cannot be replaced. Protocol-3 firmware without both fields keeps the legacy
two-stop limit: explicitly release a delivered result before another stop.

`bench_probe.Console.release_after_stop(stop_id, mechanics_permit_release=True,
triggers_inhibited=True)` requests release for the exact retained stop target.
Both caller facts are required; neither has a default. The command-line form is
`motor-release --after-stop ID --mechanics-permit-release --triggers-inhibited`.
The stop target supplies the address. Firmware still checks the retained stop
evidence and admission guards. This is an explicit action, never automatic
cleanup after a failed stop. Ordinary `motor-release` is unchanged.

Write validators check retained bytes independently of execution classification.
Late closure or supplied uncertainty stays UNKNOWN; malformed frames cannot
claim codec success. The same rules apply to live and retained results.
`Console.command("position-clear-qualification")` passively inspects the supplied
fact's exact target/configuration binding. No installation or motor traffic follows.
Home may report observed status/zero with an UNKNOWN trigger; the host preserves
that failed result and never promotes it to a reference or successful home.

## Reference preparation
`prepare_references.py` downloads the nine references listed in the source
inventory, validates their formats, extracts searchable PDF text, and writes
`docs/reference/sources.json` with SHA-256 checksums.

Requirements: Python 3.10+, `requests`, and `PyMuPDF`, supplied by
`requirements-verification.txt`. The download command is:

```powershell
python scripts/prepare_references.py
```

The downloader uses official regional storefronts when the main vendor host
rejects file downloads. Existing files with changed content are not replaced;
review such changes explicitly. This helper is documentation tooling only.

`prepare_serial_contrasts.py` preserves the five additional manufacturer
manuals. Use `python scripts/prepare_serial_contrasts.py --check` to verify
their recorded hashes offline without downloading or replacing anything.

The ESS JSON ledger is the transcription source for public descriptors, private
codec access tables and the readable catalogue. Regenerate after reviewed ledger
changes; never edit the generated tables directly. The two generators and their
native tests use only the Python standard library. Original vendor files and
historical bench evidence are not disposable build output.

`prepare_lichuan_tl57_references.py` checks the separate TL57-R source bundle,
203-word transcription, 53 published frames and six model candidates offline:

```sh
python scripts/prepare_lichuan_tl57_references.py --check
```

Optional `--extract` regenerates page-indexed search aids. `--download-missing`
uses stable original URLs, checks pinned bytes before exclusive creation and
never overwrites snapshots. Both Lichuan checkers use
`_lichuan_reference_tools.py` for snapshot integrity, ordered extract-page/text
checks and published-frame shape/CRC/provenance checks. They use the pinned
PyMuPDF version in `requirements-verification.txt`; line-ending and page-boundary
whitespace differences are ignored. Neither checker performs motor I/O. The
tooling regressions are registered with CTest; the two bundle checks remain
standalone until their profile stages register them. See the
[TL57 inventory](../docs/vendor/lichuan_tl57_rs485/README.md) and
[promptset audit](../docs/reports/2026-10-23_lichuan_promptset_audit.md).

Run the focused offline tooling regressions with:

```sh
python test/lichuan_reference_test.py
```
