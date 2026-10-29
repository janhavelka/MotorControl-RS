# 07 - Integrate interruption with owner, application and console

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Requirement: initial bounded N05 integration,
`position_interrupt`. Prerequisite: 06's implemented core contract.

Stage 01 limits this first integration to the
[native stored-profile contract](../../ess_position.md#planned-start-speed-setting-and-interruption-not-implemented)
and `position interrupt <original-id>`. Preserve N05 as PARTIAL even after that
usable subset lands; arbitrary new-target/profile replacement and attributable
observed completion still need exact missing evidence. Test rejection during
pre-trigger staging, and distinguish a host-known completion race from the
unavoidable drive finish-before-TX race. An ACK-only terminal does not clear the
possible-motion interlock or permit competing ordinary motion automatically.

## Inspect and assign

Read `RtuBusOwner`, `RtuRunner`, move/stop admission, `ProbeApp`,
`SimpleMotionApp.h`, console routes and existing move/owner/scheduling tests.
Compare FieldCore's retained motor/stop results read-only; do not copy product
types. Lead owns shared owner/application state and result reservations.
Delegate one disjoint test file after state transitions freeze. Spawn a read-only
reviewer for transaction lifetime, pressure and outcome conservation.

## Work

1. Add a normal programmatic application operation and explicit console intent
   that call 06's core API. Keep ordinary noninterrupting commands unchanged.
   Reject invalid/duplicate/stale replacement requests without modifying the
   active operation or consuming its result.
2. Define the supported replacement boundary: same exact target, compatible
   operation/configuration, at most the explicitly bounded pending replacement,
   and reserved result slots before admission. Distinguish a move not yet
   triggered from one that may be running. Reject unsupported modes clearly.
   Apply 06's setup-policy and coordinate/reference restrictions before any
   profile write; a pending replacement must not mutate the running move's
   parameters through an unqualified staging path.
3. Settle any in-flight transaction through the existing owner/runner; never
   truncate TX, reuse borrowed buffers early or accept the old reply as the new
   trigger. Preserve the interrupted operation's independent terminal evidence,
   including setup/trigger uncertainty.
4. Keep replacement and original results independently retrievable by exact
   identity. Do not overwrite the old result when the replacement fails.
   Urgent stop retains its reserved capacity/priority; define what a pending
   stop does to a replacement that has not yet transmitted or may have started.
5. Keep all work bounded and deadlines immutable. Do not add automatic replay,
   a second scheduler, unbounded replacement queue, hidden stop/start fallback
   or result expiry that discards uncertainty.
6. Update public/application docs, protocol-3 grammar/help and affected host
   parsers. Extend the ledger's real producing API/CLI route and negative route
   checks. Record the delivered subset and keep the broader N05 gap PARTIAL;
   never close it merely because a bit-3 route and ACK-only outcome now exist.

## Acceptance

Run real owner/application traces for replacement during staging, trigger,
active WAIT and final observation; old completion racing replacement admission;
old/malformed/late ACKs; partial TX; replacement trigger lost ACK; full ordinary
result slots; duplicate intent; wrong target; changed generation; cancellation;
pending stop and stop during each replacement phase.

Assert independent original/replacement/stop results, reserved stop capacity,
no reply misattribution and no uncertain write replay. Check exact API/CLI parity,
invalid/extra tokens and unchanged legacy behavior on both frontend builds.
Run focused owner/move/console tests and integrated verification/required final
commit CI. Physical interruption remains NOT RUN until separately exercised.

