# Live and retained host reply routing, 12 October 2026

[Maintainability prompt 04](../prompts/maintainability/04_host_dispatch.md)
starts from `81fc6c0`, after the completed and independently audited
[host-evidence extraction](2026-10-12_host_evidence.md). This changes the Python
host checker; console protocol 3, firmware, public core APIs and command syntax
remain unchanged.

## Reply inventory and baseline

The inventory below was checked against `Console.begin`, `_dispatch`, `_complete`
and `wait`, the actual `ProbeConsole.cpp` formatters and existing transcripts.
Every asynchronous command first admits an operation with an explicit `accepted`
reply, then delivers a terminal. An admission refusal is a local reply with no
terminal payload. `result ID` uses the same terminal evidence with `type=reply`,
`command=result` and the inspection request ID, retaining the original
`command_id` and `operation_id`.

| Original command | Live type | Payload discriminator and expectations |
| --- | --- | --- |
| `probe` | `probe` | No active kind flag; original address, checked model-register frame |
| `capture-read` | `capture_read` | `capture_read=true`; original address, capture/register/raw-wire evidence |
| `read-identity`, `read-config`, `read-state` | `read` | `read_kind=identity/config/state`; original address and matching typed windows |
| `enable`, `motor-release`, `alarm-clear`, `position-clear`, `stop` | `action` | `action_kind=enable/release/clear_alarm/clear_position/stop`; original address and stop policy |
| `move-relative`, `move-absolute`, `move-angle` | `move` | `move_kind=relative/absolute/angle`; original address and exact move arguments |
| `velocity` | `velocity` | `velocity=true`; original address and finite velocity/stop arguments |
| `home` | `home` | `home=true`; original address and homing arguments |
| `driver`, `io`, `segment`, `control`, `tuning` | Same as command | `driver=true`; original arguments and group `drive/io/segment/control_settings/tuning` |
| `recover` | `recovery` | `recovery=true`; recovery outcome and deadlines, no motor address requirement |

`home`, `driver`, `velocity`, `recovery` and `capture_read` must be booleans
when supplied; absent flags default to false. `read_kind`, `action_kind` and
`move_kind` default to null and otherwise select their closed inventories.
Every active discriminator conflicts with every other active discriminator.
The all-inactive combination selects a raw probe, which still undergoes the
full probe evidence checks. Missing the selected discriminator cannot silently
change the kind of a known original operation.

`stop_policy` is required for stop actions (`normal` or `fast`), permitted for
completed velocity under its own validator, and null/absent for pending velocity
and other kinds. Settings
terminals validate their group and read/update payload (the legacy DRIVE group
may be omitted and defaults to `drive`); a pending settings result
has only the common driver flag, so does not require terminal-only group/data
fields. Segment subgroups are `position_segment`, `speed_segment` and
`segment_start_speed`; tuning subgroups are `filters`, `current_loop`, `la` and
`collision`. The original group/index/arguments remain authoritative.
Successful and failed terminals retain each family's distinct required
evidence; a failed operation is not an admission refusal. Pending inspection
requires `ok=true` and an explicitly boolean `recovery`, checks kind/original
correlation, and does not require terminal wire/progress fields. A completed
original may never regress to pending.

An operation ID unknown to this Console can still be inspected. Its IDs must
be valid and match the inspection request, all flags must be consistent, and
the selected payload must validate. A terminal classified as a raw probe must
explicitly identify register 0/count 1 when no original request is available,
including on failure. There is no original address,
argument or serial expectation to invent, and no local operation is created.
Historical settings groups and motion details come from validated evidence.
For known operations, the original request supplies those expectations; a
response cannot substitute its own claimed request. Host-serial provenance is
optional only when neither reply nor original supplies it; a known original
tuple must match, including its historical generation.

Other commands remain local replies checked by `_complete`: host/configuration,
communication, persistence, discovery, debug, motion-profile, wiring/address,
axis preparation, monitor, status/health, load, memory and lifecycle controls.
Passive traffic records follow their separate bounded observation path. These
are not alternative kinds of retained asynchronous terminal.

## Ownership, validation order and deliberate correction

`_dispatch` owns request ID/command/profile/result correlation, duplicate and
deadline rejection, reply notification, admission/retention quotas, live event
type and original IDs, host-serial expectations, immutable comparison and final
completion. The baseline retained kind rejection order is home, settings,
velocity, move, action/stop policy, then recovery/capture/typed read. Payload
checks follow the complete discriminator checks, never the first truthy flag.

The baseline had two routing paths: live terminals selected a validator from
the original command, while inspection additionally rejected all conflicting
flags. Some live validators ignored foreign kind flags: a valid probe decorated
with `home=true` could pass live validation and fail subsequent inspection.
Both paths now use one private `_check_operation_payload` function. This
deliberately rejects those malformed live replies at the same kind boundary.
A non-string retained `move_kind` now raises `BenchError` instead of leaking
Python's string-concatenation `TypeError`. Malformed live selector errors now
come from the shared kind boundary before family payload checks; their messages
can therefore differ from the former family-specific errors. Valid transcripts
and existing retained-kind checks keep their rejection order/messages.

Review found a second ambiguity in the historical fallback: the legacy failed
probe checker accepts compact failure records once their address is checked.
Removing a failed settings/action/home discriminator could therefore bypass its
family validator when the original request was unknown. The shared fallback
now requires the formatter's exact integer probe window (0/1) for unknown
originals. This also rejects a capture failure relabelled as a probe. Known
compact probe failures remain compatible, and pending historical results still
need only their lifecycle/kind fields. This is a narrow identity check, not a
rewrite of raw failure-wire validation.

The helper sends nothing, updates no confirmations and publishes no state.
The original `_complete` session checks/confirmation updates remain explicit
before `handle.terminal` and the completion event. Admission still publishes
the accepted handle only after operation-ID and quota checks. Inspection never
replaces the original terminal; comparison excludes only the existing routing
keys (`type`, `id`, `command`, `recovery`). Release remains explicit and cannot
succeed locally before the original terminal. Input/traffic budgets, reset
detection, safe-idle interruption, deadline checks and no-replay behavior stay
with the owner.

## FieldCore boundary and remaining work

FieldCore was rechecked read-only at
`c591ecddcd28ad3965c89a60b37279aa7f9e9940`. `Rs485Task::collectModuleResult`
checks the module result against its active command and reading catalogue before
enqueueing it; invalid module results become a failed internal-fault result.
`ShzkDeviceModule::takeResult` consumes its pending result, and the owner's
`takeDeviceResult` consumes the matching device/request queue entry and frees
its reservation. This host deliberately retains immutable motor evidence until
explicit release. FieldCore's measurement result types, borrowed observations
and sensor retry policy are not imported or edited.

Large stateful host, communication, persistence, discovery, motion-profile and
debug/traffic validators remain in Console for separate work. Pure settings
group-policy extraction remains optional and separate. No second command
registry, generic async engine or protocol revision was introduced. Firmware
source, fixed contexts and storage layout are unchanged, so no firmware stack
or binary-size change is attributed to this Python refactor. Hardware **NOT RUN**;
no serial port was opened or firmware flashed.

## Verification and independent review

Three parallel agents implemented routing, extended fixtures and independently
reviewed correctness/ownership against the actual baseline and working diff.
The lead checked the resulting code and tests. AST comparison confirms that
only `_dispatch` changed among the 31 existing Console methods; the other 30,
including `_complete`, `begin`, `wait` and every session validator, are unchanged.
The only new production definition is `_check_operation_payload`.

- `bench_probe_test.py`: **251 tests PASS**, including five new routing tests.
  The tests cover each reply family live/retained/historical, every conflicting
  selector, missing and ill-typed selectors, 28 pending cross-kind combinations,
  failed-family fallback and strict historical raw-window identity. They assert
  retained object identity, absent publication on refusal and emitted commands.
  Six existing malformed-selector cases in three test methods now expect the
  earlier shared kind error; their rejection assertions were retained.
- All seven real console/Python parity suites PASS. The existing **162** actual
  core/formatter records (14 DRIVE, 16 I/O, 18 segment, 23 control, 66 tuning,
  15 home, 10 velocity) now additionally pass through live, known-retained and
  unknown-historical dispatch. Removing their family selector must fail,
  including for actual partial/failed terminals. No payload schema or firmware
  formatter was copied into a test replacement.
- Existing fake-serial cases retain original command/argument and host-tuple
  checks, immutable content/regression rejection, duplicate/interleaved
  accounting, reset and interruption, independent diagnostic budgets, retained
  quotas, explicit release/recovery and no command replay. Motion, scenario,
  communication, persistence and discovery callers run in common verification.
- Independent differential review compared **1,635** historical selector cases:
  only the documented 72 malformed-move exception changes and 24 earlier probe
  identity errors differed. A separate 15-case failed-family stripping check
  rejected 13 previously accepted non-probe payloads, preserved the valid failed
  probe and kept recovery refused. The **1,485** live selector comparisons found
  only stricter refusal or earlier kind errors; none became newly acceptable.
- Fresh quick verification in `build/host-dispatch-quick`: **77/77 suites and
  all 15 source/package/install checks PASS**, including rebuilt native parity
  fixtures, offline references, generated metadata/inventory, package contents,
  isolated C++11/C++17 headers and catalogue-independent consumers.
- Full local verification in `build/host-dispatch-idf`: **77/77 suites and all
  19 checks PASS** with pinned ESP-IDF 5.5.5, including the S3 application,
  exported-core S3/S2 and portable-owner S2 consumers.

Every push requires GCC, Clang, Arduino and native-IDF CI on its exact commit;
the final delivery handoff identifies the resulting run. Firmware source and
registration did not change, so additional local Arduino builds are not required
for this host-only change. CI still builds all four Arduino environments.

## Fresh independent audit

The completed change at `eb5cd42` was audited again against the full original
prompt, execution rules, actual `81fc6c0..eb5cd42` diff, current source, callers
and test assertions. Three fresh parallel reviewers examined correctness,
requirements/coverage and simplicity/ownership. The lead independently
reproduced the confirmed issues and checked the corrections.

Two malformed-selector gaps were found and corrected:

- A non-string `driver_group`, such as a JSON array or object, reached dictionary
  membership in `_check_driver` and leaked `TypeError`. The group now passes a
  string check before membership and raises the existing `BenchError` group
  message. The default omitted DRIVE group and all valid settings groups retain
  their behavior; no group registry was added.
- Pending velocity results bypass terminal payload validation and previously
  admitted arbitrary `stop_policy` values. The actual pending formatter emits
  null for velocity; only a pending stop action carries its policy. The shared
  classifier now requires pending velocity policy to be absent/null. Completed
  velocity retains its required normal/fast policy and original-argument checks.

Both fixes stay within reply-kind/group classification. Session mutation,
publication, pending lifecycle, immutable comparison, budgets and no-replay
handling remain unchanged. Review of the original extraction found no other
scope or simplicity correction worth adding. Existing move variants and exact
request checks already exercise their shared correlation behavior; duplicate
subtype fixtures were not added.

FieldCore was rechecked read-only at the same `c591ecdd` revision, including
owner result validation/enqueueing and consuming module/delivery callbacks.
The documented boundary remains accurate. Hardware **NOT RUN**.

The two regression tests exercise malformed settings groups on live, retained
and unknown-historical routes, plus valid absent/null and malformed non-null
pending velocity policy on known and historical operations. Assertions require
`BenchError`, no terminal/completion publication on refusal, unchanged original
retention and no additional emitted command. All **253 fake-serial tests**, all
seven real console/Python parity suites, **37 motion campaign** and **36 scenario**
tests pass. A final independent reviewer rechecked the two-line production fix
and its tests; no further confirmed gap remains.

Fresh pre-correction quick verification in
`build/host-dispatch-independent-audit` passed **77/77 suites and all 15 checks**.
After both corrections, fresh full verification in
`build/host-dispatch-independent-idf` reran the complete quick checks and passed
**77/77 suites and all 19 checks**, including all 253 fake-serial cases, rebuilt
parity fixtures, source/install consumers and pinned ESP-IDF 5.5.5 S3 application,
S3/S2 core and portable S2 consumers. The original implementation's
[exact-commit CI](https://github.com/janhavelka/MotorControl-RS/actions/runs/38247910480)
was independently queried again: all four jobs passed for `eb5cd42`.
The audit corrections require their own exact-commit CI before final handoff.
