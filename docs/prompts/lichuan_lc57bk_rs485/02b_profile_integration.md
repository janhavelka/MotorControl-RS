# 02b — Register LC57BK and its console family

Execute under [the execution contract](execution_contract.md), after 02.
Status: NOT STARTED. Apply the [shared integration contract](../lichuan_shared/02_profile_integration_contract.md)
once; append to the existing seam if TL57 landed first.

## Deliver

- `profiles/lichuan_lc57bk_rs485/Discovery.h` and source: `PreparedProbe`,
  `ProbeObservation`, `probeCapabilities`, `prepareProbe`, `checkProbe`.
- `examples/common/Lc57RtuValidator.{h,cpp}` and
  `examples/probe_cli/Lc57Console.{h,cpp}`; bounded profile application storage.
- `test/lc57_discovery_test.cpp`, `test/lc57_owner_test.cpp` and
  `test/lc57_app_test.cpp`; current ESS discovery/validator/console as structural
  examples. Update all affected build source lists and installed consumers.

## Work

1. Add `Manufacturer::LICHUAN` once and `DriveProfile::LICHUAN_LC57BK` to the
   real table-driven inventory. Guard each concrete common probe overload
   against the wrong profile, including the existing ESS overload. Test both
   directions and every compiled inventory entry; ESS frames remain identical.
2. Implement FC03 `0x0010/1` as a non-changing configuration probe after its
   reviewed access check. It proves responsiveness only. Expose real `lc57 probe`
   in the existing console and bench client; no global modal profile selection.
3. The validator retains/reconstructs immutable expected FC10 payload words
   and uses the checked LC parser. Reserve at least 23 TX bytes for homing and
   the reviewed reply bound. Only a checked exception returns `Err::EXCEPTION`.
4. Use common operation headers/reservations with per-profile static context
   storage. Admissions and stop/result handling must see conflicts across
   profiles at the same physical endpoint. Do not append all LC contexts to
   every ESS record. Measure capacity and RAM changes.
5. Allow only the active bus tuple. Initial LC route rates are 9600, 19200 and
   38400, with caller-declared 8N1 as an unqualified assumption. A different tuple
   rejects before TX. 4800 needs a separate host-timing change; 115200 is not
   documented here. Never inherit the ESS 115200 first-reply exception.
6. Host `result`, `cancel`, `release`, `recover`, `status` and `debug` route LC
   records explicitly. One `lc57` entry owns its grammar and `@ID` replies.
   Update Python response/profile validation and API/CLI parity now; later
   stages extend this family. Scanning remains unsupported until stage 11.

Run focused inventory, owner and application tests, quick verification, both
framework builds and exact-final-commit CI. No hardware is needed. Record
software and physical evidence separately and commit/push the completed chunk.
