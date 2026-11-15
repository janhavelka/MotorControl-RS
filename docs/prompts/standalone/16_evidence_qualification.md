# 16 - Align evidence and qualification boundaries

Read the [execution contract](execution_contract.md) and the
[current audit](../../reports/2026-11-15_standalone_audit_revalidation.md).
This stage owns K-02's closure remainder, R-01, R-07, R-09..R-12, R-16 and ESS C9.
Deliver the independent blocks below separately. Do not turn a conservative
rejection into success without the required source and observation evidence.

## 16a - Write evidence

Inspect all seven `classifyWrite` consumers and their public evidence contracts.
Define one explicit closure rule: distinguish acknowledgement evidence from
an operation completed inside its deadline. Position, velocity, homing and
actions currently accept the classifier's default `onTime=true`; driver,
communication and persistence supply a deadline check. Either keep a justified
distinction with precise contracts, or align the callers. Do not change an
independent golden vector to fit the implementation.

Test exact and late closure, delayed host delivery of on-time closure,
unqualified timing, exception 05, other documented exceptions, malformed
frames and supplied `executionUnknown`. Outputs and copied evidence must stay
consistent across core, console and host validators.

Fix R-10 and R-12 only after separating transmitted effects from execution
uncertainty. A checked rejected FC06 with readback equal to its known previous
value is not a new contradiction. A rejected stored-position trigger without
setup is different from a failed operation that already staged parameters.
Retain the write evidence in both cases. Test genuine contradictory readback,
lost ACK, partial staging and no-staging rejection before narrowing an interlock.

## 16b - Qualification lifetime and home envelope

Before changing qualification, fix R-01's narrow active-origin rejection in
shared ordinary move readiness. An origin level alone is not a serial start,
stop or limit. Test a completed home with origin still active followed by an
ordinary move, including the shared group-member facts. Preserve active-limit
and existing unsupported trigger/velocity assignment guards. The broader
per-function expansion remains multi-drive 04, outside this stage.

1. Bind position-clear qualification to its exact target, binding and
   configuration generation (R-09), using the smallest existing fact structure.
   Check install, inspect, revoke, rebind, recovery and configuration changes
   through the public application and console routes. A later read cannot
   restore a revoked qualification.
2. Set a documented application home rate policy (R-16). Native source ranges
   are not proof that the tested drive works at every subdivision/rate.
   The current position ceiling is 50,000 increments/s; it is a tested
   application policy, not a vendor limit for homing. Decide and document
   whether the default home envelope shares that ceiling. Keep explicit
   supplied qualification and native source bounds separate. Check search and
   return rates against actual subdivision before any write, without clamping.
3. Preserve the R-11 wording correction: opposite-limit/release/stop guards
   are conservative host admission policy. Keep the guards until the owning
   fixture evidence permits a change. Do not claim direction-aware trajectory
   checking that is absent.
4. Name the monitor/reference readiness refusal at the existing public app and
   console boundary (R-07). Preserve guard precedence and zero traffic. A
   generic INVALID must not hide the known need to settle monitoring first.

Tests: qualification installed then recovered/rebound/configured, old read
delivery, missing subdivision, boundary rates, unchanged output and zero
traffic on rejection. Keep methods 33/34/35 regressions and the existing
zero-offset, auxiliary and native ramp restrictions.

## 16c - Unconfirmed home trigger

C9 is a portability limitation: `advanceHome` ends after an unconfirmed
trigger echo while the drive may have started. Design any continued observation
contract separately from the write result. UNKNOWN must remain UNKNOWN; a new
HOMED/RUNNING sample must not silently confirm the trigger or reuse an old home.
Compare position/velocity behavior, but do not copy it blindly.

Specify whether fresh qualified status transitions plus final zero can support
a useful observed result after uncertain trigger execution, and which reference
claim is justified. If the protocol cannot attribute the event, keep the
reference unavailable and record the exact limit. No trigger replay, assumed
response source, automatic stop or reduced zero/transition guard is permitted.

Tests must use real core/app events: ambiguous echo, lost trigger ACK, stale
HOMED high, missing RUNNING, method 35, final zero, revocation during the last
reply, stop and cancellation. Preserve all evidence and output precedence.

## Finish

Run focused core, qualification, home, move, communication and console/host
tests, fresh quick verification and required exact-commit CI for behavior/API
changes. Document actual delivered contracts and remaining source/fixture
limits. A documented choice alone does not count as implemented behavior.
