# 12 — Lichuan qualification and release reconciliation

Execute this stage under [the execution contract](execution_contract.md).
Status: NOT STARTED. Software closeout is actionable; physical work requires an
identified, available and authorized Lichuan fixture. The ESS bench is not that fixture.

## Software closeout

1. Reconcile all 127 source rows, prose-only functions and LC-D01..LC-D15 against
   actual callable APIs, CLI coverage and tests. Mark missing code separately
   from unresolved device behavior, unavailable fixtures and unsupported modes.
2. Exercise clean source/install/PlatformIO packages, standalone headers,
   C++11/C++17/no-RTTI consumption, codec-only linking and both profile regressions.
   Verify exact final commit through all required existing CI jobs.
3. Update README, profile/discovery/CLI contracts, native coverage, backlog,
   roadmap and release notes to the actual supported subset. Preserve the
   existing 1.0.0 tag; choose any new version under the real release policy.
   Packaging and a green build do not qualify every listed model or operation.

## Physical qualification when the fixture exists

1. Record nameplate/model, open/closed-loop hardware, firmware/raw identity if
   available, source revision, supply, wiring, shaft/load condition, serial tuple,
   adapter/image and authorization. Resolve listing/manual encoder/model
   discrepancies; do not infer firmware identity from a generic responder.
2. Begin with non-changing reads and retained raw frames. Establish configuration,
   state address, coherent feedback, parser/echo evidence and transport timing.
   No automatic writes, alarm clears, setup changes or probing by motion.
3. Establish explicit command/stop prerequisites and a restoration plan. Run
   small finite position moves, prepared stop paths, repeated starts/rearm,
   absolute/relative policies and negative motion only when its encoding is
   actually qualified. Retain before/after settings and all uncertain outcomes.
4. Qualify open-loop and closed-loop behavior independently. Measure encoder
   mapping and shaft displacement when claiming physical position accuracy.
   Software timestamps bound observations; electrical timing needs measurement.
5. Add method-specific homing, switch/output/brake tests only with their fixtures.
   Add communication/persistence restart cases only with actual restart control.
   Deliberate wire faults and lost-ACK scenarios must preserve no-replay and have
   an explicit bounded test plan. No unbounded jog, moving link-loss or extended
   unattended soak is implied by this stage.
6. If failures occur, deliberately reproduce within authorization and retain
   bounded diagnostics for root-cause analysis. Keep failed aggregates and raw
   evidence; after a proper fix rerun the original scenario and short regression.
   Leave an unresolved investigation when the distinguishing measurement is absent.

## Completion criteria

Deliver a matrix by exact model/firmware/image/operation: implemented,
software-tested, hardware-tested, unsupported and unresolved. Attach local/CI
results and any hardware report with restoration outcome. Do not label the entire
family qualified from one free-shaft motor. Archive completed implementation
prompts only after transferring remaining qualification obligations to the
maintained backlog. If hardware is absent, finish truthful software closeout and
leave physical qualification explicitly open. Commit/push the owned changes.
