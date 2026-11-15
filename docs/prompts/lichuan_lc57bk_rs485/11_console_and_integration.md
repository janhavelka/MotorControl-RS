# 11 — Shared console and mixed-profile ownership

Execute this stage under [the execution contract](execution_contract.md), after
the independently implemented subsets of 02-10. Status: NOT STARTED. Reinspect
landed standalone ESS and current TL57 changes before extending the 02b dispatch seam.

## Work

1. Audit the 02b `lc57` family and each route delivered in 03..10. Preserve ESS
   command meanings and the existing console protocol. There is no modal
   profile selector; family-prefixed commands use the same installed APIs.
2. Both Arduino and native ESP-IDF must call the same installed Lichuan APIs and
   shared application code. No register encoding, unit conversion or special
   motion workaround belongs only in a console handler or HIL path.
3. Use the existing owner/runner and 02b validator. Keep one physical bus owner
   with priority stop and retained results. Claim fairness between families
   only if each has a producer slot and tests exercise it. Preserve application-
   owned same-axis admission (currently in `ProbeApp.cpp`); the bus owner's
   `SequenceId` correlates work and does not reserve an axis. A completion routes
   by the admitted profile/target generation,
   never whichever profile the user selected afterward.
4. Validate immutable serial tuples for all admitted work. Mixed addresses or
   manufacturers do not imply compatible baud/parity, protocol timing or probe
   behavior. Explicit profile selection performs no I/O and never changes a
   drive's settings. Never fall back to ESS writes for an unknown profile.
5. Size request/event/result/cache storage against actual largest operations,
   including Lichuan's 23-byte homing frame. Keep fixed caller-owned storage and
   measure RAM/stack/firmware differences. Exclude descriptions from basic codecs.
6. Extend help, capability output, status decoding, runtime debug observation
   and bounded Python tooling. Passive traffic observation must copy owner data
   without consuming protocol bytes or blocking under console pressure.
   Route decoding by admitted profile; do not label LC traffic as failed ESS
   decoding. Add LC bounded scans, profile-typed findings, `discover` parser
   and Python support now. Unknown/unrouted profiles still reject before TX.
7. Inspect FieldCore read-only: its ESS contexts, current 21-byte TX/192-byte RX
   transaction storage and supported serial tuples need separate integration
   review. Its current 8N1/selected 9600..115200 policy does not admit LC57BK's
   documented 4800 baud. Document this without importing its types or changing
   its sensor protocols. Record current facts, not old handoffs.

## Deliver

Extend `Lc57Console`, LC application/cache records, shared discovery scan and
Python LC module. Add `test/lc57_scan_test.cpp` and mixed-profile owner/console
cases in `test/lc57_app_test.cpp`. Review ESS `DiscoveryApp.h`, `DiscoveryScan.h`,
`StateCache.h` and their tests against the landed seam, not copied assumptions.

## Verification and acceptance

Test two profiles and multiple targets through the same owner, profile changes
with work/results pending, wrong-target frames, fair scheduling, urgent stop,
late replies, console saturation and exact API/CLI parity. Build all affected
Arduino environments and native-IDF consumers using pinned toolchains. Run quick
verification and exact-final-commit CI for all required jobs.

Flashing is not necessary merely to prove compilation. If an image reaches an
authorized board, run the short existing-profile regression and focused admitted
checks; never send Lichuan frames to the ESS bench. Record unperformed HIL and
resource measurements explicitly. Commit/push and provide the handoff.
