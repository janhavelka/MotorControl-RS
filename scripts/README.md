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
203-word transcription, 52 published frames and six model candidates offline:

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
