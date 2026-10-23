# 02 - Read the three native velocity parameters

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Requirement: N01
`velocity_parameter_reads`. Prerequisite: 01's reviewed read contract.

## Inspect and assign

Read `Reads.h/.cpp`, `Velocity.h/.cpp`, `ProbeApp.h/.cpp`,
`ProbeConsole.cpp`, velocity app/console fixtures, the JOG ledger entries and
original ESS source. Lead owns API and application/CLI integration. After freezing
the result/sequence contract, delegate named read tests to one agent. Spawn a
read-only reviewer for raw interpretation, frame bounds and source-derived
vectors. One writer owns `reads_test.cpp`; no concurrent homing-read edits.

## Work

1. Add a typed producing read of JOG_SPEED 0x001D, JOG_ACCELERATION_TIME 0x001E
   and JOG_DECELERATION_TIME 0x001F. Use the reviewed contiguous read when valid;
   keep the plan bounded and explicit. Reuse current checked read sequencing,
   exact step expectations and result/provenance conventions.
2. Preserve all three raw words. Expose only reviewed interpretations, with
   explicit validity/qualification. A raw speed is not automatically signed
   velocity or physical rpm; annotated defaults do not settle native scaling.
   Do not change existing velocity request units or readiness from this read.
3. Provide an ordinary callable application read/result path and a shared
   console command under the existing velocity family. Final names follow 01's
   conventions. Keep command completion/result retention coherent with other
   reads and support programmatic use without the console.
4. Keep observation separate from desired/staged velocity values. Reads do not
   start, stop, stage, repair, save, or automatically qualify motion. Cached
   display must not hide a new transfer or turn stale words into readiness.
5. Update public documentation, CLI help/grammar and exact producing routes in
   the operation ledger/checker. Close only these three read obligations when
   the real API and required route are implemented and tested.

## Acceptance

Use independent request/reply vectors for all three words and a source-reviewed
maximum/unknown raw value case. Test exact window/count and zero writes,
wrong address/function/length/count/CRC, valid exception, stale target/step/
generation, deadline boundary/delayed delivery, cancellation and output
preservation. Returned evidence must survive mutation of the original RX buffer.

Exercise real app admission and console parsing, including invalid/extra tokens,
busy and stale result retrieval. Preserve existing finite velocity and stop
behavior and both frontend semantics. Run focused read/velocity/application/
parity and inventory tests, then required integrated checks/CI for this API
change. Record implementation and physical evidence separately in progress.

