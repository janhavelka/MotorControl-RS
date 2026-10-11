# 03: separate host evidence checks from serial ownership

Read [the execution rules](README.md). Inspect `scripts/bench_probe.py`, its
callers in `bench_session.py`, `bench_motion.py`, `bench_repeat.py` and
`bench_scenarios.py`, plus `test/bench_probe_test.py` and actual console-parity
fixtures. Inspect FieldCore motor result/evidence requirements read-only.

Problem: the roughly 4,900-line host module mixes argument validation, a serial
session, strict motor evidence validation and campaign orchestration. `Console`
alone contains large `_check_move`, `_check_driver`, `_check_home` and
`_check_communication` methods alongside `_dispatch`, `begin` and `wait`.
The checks are valuable; their ownership and pure dependencies are hard to see.

1. Identify pure validators versus checks that update session state. A pure
   validator must not own a port, read time, send commands or retain mutable
   session state. Keep JSON/framing bounds and command/operation correlation in
   `Console`, including unsolicited/duplicate messages and reset detection.
2. Extract one related pure validator family to an explicitly imported private
   module. Pass exact evidence/expectations; preserve error classes/messages
   when clients/tests use them. Avoid circular imports by locating shared small
   constants/error types deliberately. Do not reproduce the same command list
   or wire policy in an unchecked new registry.
3. Keep stateful communication/persistence/discovery confirmations, profile
   history and release ownership in the session until their dependencies are
   separately understood. Do not serialize whole sessions or use global state.
4. Preserve one-attempt behavior and all fail-closed checks: exact integer types
   (Python booleans are not integers here), CRC/length/echo, closure timing,
   partial TX, output count, raw field consistency and independent outcomes.
   Shared transport fields do not imply shared operation-success policies.
5. Run existing fake serial and actual C++-console/Python parity suites, including
   interleaved diagnostic input budgets and uncertainty after interruption.
   Exercise the supported script entry points/import paths without opening a
   serial port. Add only cases exposing a changed boundary or uncovered risk.

Accept unchanged public CLI/import behavior and evidence decisions, with clear
module ownership and no weakened checks. Complete quick verification and all
exact-commit CI. Record remaining large validators as deliberate follow-up;
do not combine this with new motor behavior or an automatic cleanup/retry policy.
