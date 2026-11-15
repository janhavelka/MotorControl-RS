# 02b — Register TL57 and its ordinary firmware route

Follow the [execution contract](execution_contract.md), stage 02 and the
[shared integration contract](../lichuan_shared/02_profile_integration_contract.md).
Implement the shared seam once; extend it if LC57BK already supplied it. This
stage is implementation work, not an enum-only placeholder or profile selector.

1. Add the TL57 profile and LICHUAN manufacturer with a real validator and
   `prepareProbe`/`checkProbe` for `0x0001/1`. A valid reply proves responsiveness,
   not manufacturer, version-label interpretation or exact model.
2. Make the discovery inventory table-driven. Guard the concrete ESS overload
   with `profile == ESS_RS` before consulting generic capabilities; otherwise
   adding TL57 capabilities makes ESS storage emit the wrong probe. Update
   pinned inventory counts, installed consumers and Python profile expectations,
   preserving ESS request bytes and outcomes.
3. Register one `tl57` family in the command inventory, with parsing/formatting
   in `Tl57Console.h/.cpp` and an owner-backed `tl57 probe` route. Top-level ESS
   commands retain their meaning. Share framing, IDs, queues, help, result,
   cancellation, release and recovery. Do not create another UART owner.
4. Use the shared per-profile fixed storage and common record header for
   admission/delivery, target generations and retained outcomes. Check the
   intersection of profile and host serial tuples before TX. An owner sequence
   ID is not an axis reservation. Decode and label copied debug traffic using
   the admitted profile before the first TL57 transaction.
5. Register sources/tests in CMake, PlatformIO and native IDF source lists.
   Keep `discover` rejecting unsupported profile routing until stage 11 adds
   the bounded scan path. Subsequent stages deliver their own real API/CLI
   routes and parity tests; stage 11 audits integration instead of inventing it.

Test probe output isolation, wrong concrete profile, ambiguous compatible RTU
replies, retained results, tuple refusal and unchanged ESS behavior. Verify the
ordinary Arduino and IDF images and installed/source consumers, then terminal
CI on the exact final commit. No motor traffic or hardware qualification is
implied. Record actual FieldCore differences read-only and update the ledger.
