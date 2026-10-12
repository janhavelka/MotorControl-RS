# Pure host evidence validation, 12 October 2026

[Maintainability prompt 03](../prompts/maintainability/03_host_evidence.md) starts
from `24a3ace` on the existing `main` branch. This is a host-code boundary
refactor; firmware, installed core APIs, console protocol 3, command arguments,
retry/cleanup policy and hardware qualification are unchanged.

## Inventory and first delivered boundary

The canonical private module [scripts/_bench_evidence.py](../../scripts/_bench_evidence.py)
contains shared error/wire/check helpers and probe/capture/typed-read/state/cache
validators. It imports only the standard library and consumes supplied evidence
and timestamps. It does not import `Console` or `bench_probe`, own a port, read a
clock, retain session state or send commands.

| Definitions | Dependencies and purpose |
| --- | --- |
| `BenchError` | One canonical exception class; every existing host/campaign import receives this identity |
| `check_counts`, `operation_id` | Actual integers with their original unsigned64 and nonzero unsigned32 bounds; `bool` is rejected |
| `unique_object`, `invalid_constant` | Pure strict JSON decoding hooks; decoder/framing ownership stays in `Console` |
| `evidence_age_valid`, `evidence_age_deadline` | Supplied timestamps and optional expiry; no clock reads |
| `wire_crc`, `_reply_status` | Independent retained-wire checks; no firmware-parser trust |
| `HOST_BAUDS`, `TYPED_WINDOWS` | Single reviewed constant tables, re-exported through the original module |
| `check_probe`, `check_capture_read`, `check_read_evidence` | Shared checks, `re`, CRC/status; failed-result and successful-raw validation order retained |
| `check_typed_read` | Shared checks, constants, wire status, `check_state_values` |
| `check_state_values`, `check_state_block` | Concrete decoded-block checks; explicit configuration/provenance inputs |
| `check_cached_state` | Counts and state-block checks; supplied cache snapshot, selected binding, invalidation watermark and age |

The corresponding `Console._operation_id` and `Console._check_*` names are direct
`staticmethod` aliases. There is one implementation per check. Existing imports
of errors, wire helpers, age/count checks and JSON hooks through `bench_probe`
remain available. Local integer helpers retain their distinct bounds and
signedness; they were not homogenized.

`_reply_status` still assumes callers checked retained raw storage/hex shape and
captured prefix lengths before calling it. Its order remains length range,
address, function, exact normal/exception length, FC03 byte count, CRC, exception,
then write echo. Its expanded docstring makes that precondition explicit without
adding checks or changing first errors. Motion age uses strict expiry; cached
state uses its existing inclusive expiry. Observation-current checks still require
evidence strictly after an action invalidation watermark.

## Imports and session ownership

The four actual callers (`bench_motion`, `bench_repeat`, `bench_session`,
`bench_scenarios`) retain their existing `bench_probe` imports. Supported normal
imports put `scripts` on the search path; direct script execution does that through
Python's ordinary script-path behavior. The four file-spec loaders in
`bench_probe_test`, communication, persistence and discovery harness tests now
explicitly add that directory, matching the native-console parity tests.
There are no exception-driven import fallbacks, production search-path mutations,
circular imports or duplicate error classes.

`Console` still owns framing, line/byte/traffic budgets, command/operation
correlation, reset/duplicate detection, release, deadlines and serial I/O. Host,
communication, persistence, discovery, debug/traffic and motion-profile validators
remain there because they inspect or update session caches/confirmation state.
Recovery/axis/use-address/wiring/admission helpers and load checks also stay there;
they are outside the extracted evidence families. Pure settings validation stays
in `Console` for a separate optional group-policy extraction.

The next checked chunk extracts action/move/velocity/home validators, their
canonical argument parsers and required schema constants into the same private
module. Prompt 03 is not complete at this first boundary.

## FieldCore boundary

Current FieldCore result/evidence/binding/owner and SHZK module source was inspected
read-only at `c591ecddcd28ad3965c89a60b37279aa7f9e9940`. Its device results contain
float measurements and valid/stale/error masks; owner observations borrow RX bytes
until delivery and record millisecond completion. Its transport has eight-byte TX
storage, 8N1 validation and TX-prefix stripping; the SHZK module can retry within
deadlines. The host checker keeps motor-specific retained frames, microsecond
closure/delivery intervals, target/generation provenance, write-source uncertainty
and independent decoded-field checks. Measurement masks, sensor retry rules and
health projections are not substitutes for motor evidence. No FieldCore file changed.

## First-boundary verification

- `bench_probe_test.py`: 246 tests PASS, including four added import/purity/wire
  precedence cases. Existing malformed raw frames, boolean counters, partial TX,
  stale generations, late delivery, interleaved diagnostics and no-replay cases remain.
- Fresh quick verification in `build/host-evidence-reads`: 77/77 registered suites
  and all 15 source/package/install checks PASS. Discovery ran with its actual native
  fixture, and all seven C++-console/Python parity suites ran; no test was skipped.
- Independent AST comparison against the baseline: 16 moved definitions match
  after renaming/rebinding and the wire-helper docstring update; all 35 remaining
  `Console` method bodies are unchanged. Direct CLI `--help` output matches baseline.
- New tests prove one error/helper identity across normal imports, file-spec aliases
  and all four callers; isolated pure import/typed validation forbids session/serial
  imports and file/clock calls. Direct CLI help also runs from an unrelated directory.

Every pushed chunk requires GCC, Clang, Arduino and IDF CI on its exact commit;
the delivery handoff identifies those runs. Python-only extraction changes no
firmware storage layout, so the earlier application storage measurements remain
unchanged. Hardware **NOT RUN**; no image was flashed or serial port opened.
