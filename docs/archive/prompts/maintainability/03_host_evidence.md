# 03: separate pure host evidence validation

Read [execution and delivery rules](README.md). Inspect
[bench_probe.py](../../../../scripts/bench_probe.py), its four campaign/session
callers, native-console parity fixtures and all import paths in Python tests.
Inspect FieldCore result/evidence contracts read-only; retain independent host
checking of motor firmware evidence rather than adopting measurement results.

Several large `Console._check_*` methods are already static and use no session
state. Their placement beside serial ownership hides this useful boundary.

## First boundary

1. Inventory pure helpers and their dependencies. Start with probe/capture/read
   evidence, then typed reads, state and cache checks. Map `BenchError`, integer/
   age/count checks, `wire_crc` and `_reply_status`. The latter relies on callers
   having checked retained raw storage and lengths; preserve that ordering.
2. Put the canonical shared error/wire/check helpers and first validator family
   in an explicitly imported private module. It must not import `Console` or
   `bench_probe`, open a port, read a clock, retain session state or send commands.
   Keep existing externally used helper/error imports available through the
   current module, with one `BenchError` identity and one implementation.
3. Address imports deliberately. `bench_probe_test.py` currently loads the
   script with `spec_from_file_location` without adding `scripts` to the search
   path, whereas other tests do. Exercise direct script execution and supported
   imports; update the test loader for the real layout. Do not hide circular or
   missing imports with exception-driven fallback modules or duplicate classes.
4. Extract action, move, velocity and home validators in later checked chunks.
   Settings validation may follow with explicit group layout/validation stages;
   preserve distinct field masks, unknown prior values and settlement policies.

## Keep state where it belongs

Keep framing, byte/traffic budgets, command/operation correlation, reset and
duplicate detection, release, deadlines and serial I/O in `Console`. Initially
leave host, communication, persistence, discovery, debug/traffic and motion-
profile checks there: they update session caches/confirmation state. Moving
them requires a separate state-ownership analysis, not changing `self` to an
untyped dictionary. No automatic retries or new cleanup policy.

## Acceptance

Run `bench_probe_test.py`, motion/scenario tests and every actual C++-console/
Python parity suite. Exercise valid and malformed raw evidence, `bool` supplied
as integer, partial TX, CRC/length/echo precedence, stale generation, late
delivery, interleaved diagnostics and error-class identity across import paths.

Pure checks must accept/reject the same evidence with the same relied-on
messages. Preserve output/exception behavior and public script arguments.
Keep a dependency list and remaining stateful validators in the handoff.
Complete quick verification and all exact-commit CI jobs.
