# Bounded ESS discovery

The installed common [Discovery.h](../include/MotorControlRS/Discovery.h) provides
`discoveryProfileCount`, `getDiscoveryProfile` and `getDiscoveryCapabilities`.
Include [profiles/ess_rs/Discovery.h](../include/MotorControlRS/profiles/ess_rs/Discovery.h)
for `ESS_RS::PreparedProbe`, `ESS_RS::ProbeObservation` and the native and common
`prepareProbe`/`checkProbe` overloads. Concrete wire evidence belongs to the
profile; the common header has no manufacturer-header dependency. Only
STEPPERONLINE / ESS-RS is implemented.
Inventory and capability calls perform no I/O. A probe emits exactly FC03 model
word `0x0000/1`; optional refinement uses the existing typed identity read
`0x0000/4`. Original function-manual physical p68 marks these fields read-only;
no consuming effect or ESS communication-watchdog interaction is documented.

`ESS_RS::PreparedProbe` copies bytes and immutable target, operation ID, absolute
deadline and supplied active tuple. `checkProbe` consumes a correlated supplied
event, copies at most 37 raw bytes and retains full received length, accepted TX,
uncertainty and qualified closure bounds. Rejected envelopes leave output
unchanged. An on-time qualified closure may be delivered later. Unknown model
values, including `0x4EEA`, remain responder evidence with unresolved mapping.
Checked exceptions, malformed/mismatched replies, no response, deadline and
transport/timing failures remain separate. A valid reply never excludes collision
or proves manufacturer, exact model, readiness, scale or motion completion.

The standalone [ProbeApp.cpp](../examples/probe_cli/ProbeApp.cpp) owns one
cooperative scan using the existing UART and bus owner. It has no second queue,
task, synchronized ingress or generic schema engine. Storage lives in the
application's PSRAM allocation; driver/capture state and stack remain internal.
The public core has no example, platform, clock or I/O dependency.

```
profile list
probe [ADDRESS]                    # ping is the same minimal query
read identity [ADDRESS]
discover [profile ess_rs | manufacturer stepperonline]
         [addresses FIRST LAST] [tuple BAUD FORMAT]...
         [query-ms N] [overall-ms N] [requests N] [results N] [identity]
         [past-silent]
discover inspect|cancel|restore|finish
```

Default scope is the bound ESS axis at the actual current host tuple, a 500 ms
query budget, 5000 ms overall budget, 16 admitted requests and 8 findings.
Direct application configuration accepts addresses 1..247, up to 4 distinct
adapter-supported tuples, query budgets 1..5000 ms, overall budgets 1..60000 ms,
1..256 requests and 1..8 findings. The console retains its 128-byte line limit
including the terminator and 22-token limit including an optional `@ID`;
omit default options to fit a command. Tuple formats are
8N1/8N2/8E1/8O1 at 9600/19200/38400/115200 baud. Support means host setup
capability; alternate motor tuples remain unqualified. Only the recorded
115200 8N1 bench tuple has the 304 us first-response exception.

Each admitted probe consumes one finding and one request. Identity refinement
consumes another request and attaches evidence to that finding. All attempts,
including exceptions and failures, count against bounds. Queries use the smaller
of their absolute deadline and the remaining overall deadline; setup time counts.
Progress and raw evidence survive cancellation, deadlines, capacity exhaustion,
recovery and result inspection. Findings never update the bound axis or its cache.
Refinement disagreement between model or active node and the probe/target is
explicit identity ambiguity. Multiple reads are not an atomic identity snapshot.

BEGIN refuses active motion, axis uncertainty/reservations, monitoring, unsettled
transport, host configuration and commissioning. During the scan ordinary work
and host/configuration mutations are excluded. Existing eligible urgent owner work
takes precedence. An admitted stop for the original known axis preempts further
queries and waits for in-flight transport plus original-tuple restoration; it
retains its original deadline and dedicated frontend/result capacity. If that
deadline expires or transport cannot be repaired, no stop transmission or physical
stop is claimed. Urgent work cannot bypass recovery or use a mismatched tuple.

Cancellation cancels unsent requests and lets physical TX/reception settle.
`cancel SCAN_OPERATION_ID`, or bare `cancel` while a scan owns the session, uses
the same cancellation policy. Faults stop with partial results; there is no
automatic recovery or retry, and a default scan never continues past a fault.
`past-silent` (below) is the one defined continuation. Healthy endings restore the exact
starting tuple, rather than startup defaults, before ordinary work resumes.
Failed restoration keeps an explicit interlock. Use `recover` for a transaction
fault, then `discover restore`; a failed host setup uses `discover restore` for
explicit repair. These actions never resume scanning.

### Continuing past a silent address (`past-silent`)

Without a response-lifetime bound (none is qualified for these drives) an idle
gap cannot exclude a late reply, and ESS frames carry no transaction ID. So a
scan continues past silence only where a late reply cannot match a later
request context:

- One tuple only; a multi-tuple `past-silent` scan is refused before any TX
  (`past_silent_one_tuple`, API `INVALID`). Addresses ascend, so no address is
  probed twice.
- Silence is a probe that sent every byte and received none (`NO_RESPONSE`,
  no echo or adapter fault). It is recorded in the `silent` list beside the
  findings and consumes no finding slot; it is an observation, not proof of
  absence.
- Before the next, different address the scan runs one bounded settle: the
  owner's recovery drain and idle gap, counted as one request and limited by
  the query and overall budgets. It never resends the probe. Silence at the
  last address needs no settle.
- Any byte the settle drains, or that the runner discards before a later
  probe's TX, ends the scan (`settle: late_bytes`, `late_bytes`). Partial
  bytes, a frame from another address (a late reply is never a responder at
  the next address), CRC, length, gap, echo or correlation faults end it with
  today's interlock.
- A scan that recorded silence ends in INTERLOCK with `boundary_required`,
  whatever its outcome. `discover restore` and ordinary traffic stay refused
  until an explicit `recover` admitted after the scan ended has recovered the
  bus (`boundary_crossed`); then `discover restore` and `discover finish` as
  usual. That operator recovery is the boundary before a silent address can be
  probed again. Cancellation during a settle lets the settle finish first.
- Beginning a `past-silent` scan releases a delivered earlier `recover`
  result (the settles need the owner's one recovery slot); an undelivered one
  refuses `busy`.

The reply adds `settings.past_silent`, `silent`, `silent_count`, `settles`,
`settle` (`none`, `recovered`, `late_bytes`, `expired`, `read_error`,
`transport_error`), `late_bytes`, `boundary_required` and `boundary_crossed`.
`bench_probe.py discovery-check --cross-boundary ...` runs the operator
sequence after such a scan; without the flag the campaign stops at the
interlock. Wired bench 19 (20 November 2026, image A,
`0x4EEA`/`0x0029`): `discover addresses 1 8 past-silent identity requests 32`
found 1, 2 and 4 with identity, silent 3 and 5-8, four settles and no late
byte; a probe and `discover restore` were refused before the boundary, then
`recover`, restore and finish succeeded. The same scan without `past-silent`
stopped at 3 as before. One clean bench scan does not prove correlation in
general.
Recovery also cancels a healthy scan's unadmitted continuations, even when its
completion is serviced late. An admitted query still settles and retains evidence;
restoration waits for recovery settlement before ordinary work resumes.
Logical target/configuration generations are separate from transient host
selection; historical evidence keeps its original context. Explicit recovery
still invalidates logical confidence.

`inspect` is non-consuming. One terminal scan remains until `finish` releases its
retention, then the next BEGIN may replace it. Generic transaction `result/release`
remain for ordinary transactions; scan evidence uses `discover inspect/finish`.
Console command IDs correlate each control separately from scan/query operation
IDs. Output remains bounded to 8192 bytes with existing nonblocking backpressure.
Python validates the same grammar, correlation, immutable evidence and deadlines;
`discovery-check` performs one finite scan and reports an interlock without
automatically repairing it. Serial provenance is re-queried after scans.

[Discovery evidence](archive/reports/ess_release_22_2026-10-05.md) records native failure
coverage, actual COM13 scans/restoration and the remaining qualification limits.
The [fresh audit](archive/reports/ess_release_22_audit_2026-10-05.md) adds recovery-before-
delivery, refinement failure/cancellation and strict retained-evidence checks.
