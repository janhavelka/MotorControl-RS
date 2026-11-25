# Wired bench 19: discovery past a silent address and remaining multi-drive work, 20-23 November 2026

[Prompt 19](../prompts/ess_wired_bench/19_multi_drive_remaining.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md) and the owner-absence rules (D15).
Gate G3. State: **Partial**. Discovery now continues past a silent address in
one defined context (`past-silent`), with native, console and harness tests;
on the bench it found drives 1, 2 and 4 and silent 3 and 5-8, and the default
scan still stops at 3. Host-coordinate absolute group plans reached every
member's own target in 20 staged and sequential plans. After 3 of 4 group
stops during an absolute plan, the next group read reported one member's read
as not current, and in both such cases that reached another plan it was
refused `unavailable` before any frame: open, cause unresolved. B1-B4 are deferred to the owner; B5, K1 and E1 are NOT RUN.
The session was stopped once by a usage limit (20 to 23 November) and finished
on 23 November. The lead owns Git and CI.

## Objective and scope

Prompt items 1-4 (discovery continuation, multi-drive 06 disposition, group
absolute targets, HW-15) and the D15 safeguards for 19: B1-B4 deferred
(drives powered one at a time), B5 and K1 NOT RUN (no fail-safe bias fitted).
Owner direction: important issues only; minor findings at the end.
Implementation items 5 (broadcast trial route) and 6 (link check) were not
built: their only cases (B1-B5, K1) cannot run during the absence (handed on).

## Prerequisites, sources and audit entries

- Consumed: 09's group start evidence and routes, 13's PT promotion (no PT use
  here), 17's address policy (no address write; probes of 3 and 5-8 are reads),
  16's 1 s wait after `alarm-clear` (no alarm here), 18's final state (573/573
  equal) and `before_flash_check`, 12's evidence headroom check, 01's baseline.
- Source: `08a88eb` plus this session's tree at the flash; HEAD later
  `7b580b8` (documentation commits by other sessions only). The lead's
  uncommitted note in the prompt 18 report is not part of this work.
- **A2-F17** (late-reply rejection stronger than the settle evidence): applied
  as written in the prompt. No response-lifetime bound is claimed; the context
  makes a late reply foreign (one tuple, each address once, any late byte or
  foreign frame ends the scan) and the operator recovery is the reuse boundary.
  Native tests cover a late frame during the settle, at the next address
  before its TX and after it.

## Facts checked

| Claim | Source | Status |
| --- | --- | --- |
| ESS RTU frames carry no transaction ID; an idle gap cannot exclude a late reply | timing audit, runner contract | documented (unchanged) |
| No qualified response-lifetime bound exists for these drives | bus check, timing audit | unknown; not claimed |
| A drive at address 3 answered other drives' replies | 2026-11-12 bus check | not retested: no drive at 3 (K1 NOT RUN) |
| Absolute group targets use the drive's command coordinate (start word 5) after host conversion | `Position.cpp`, console guide | **measured**: endpoints 0 or -1 increment from the computed native target |

Model `0x4EEA`, firmware `0x0029`, 20-23 November 2026.

## Implemented artifacts and decisions

- **`past-silent`** (`DiscoveryApp.h`, `DiscoveryScan.h`, `ProbeConsole.cpp`):
  opt-in; one tuple (multi-tuple refused before TX); a probe with full TX and
  zero RX is recorded in a 247-address bitmap and costs no finding slot; one
  settle per silence (owner recovery drain and idle gap, one request, inside
  the query and overall budgets; none after the last address); any drained
  byte, any byte discarded before a later TX, partial bytes, a foreign frame or
  a frame fault ends the scan; a scan with silence ends in INTERLOCK and needs
  an explicit `recover` admitted after its end, then `discover restore` and
  `discover finish`. Default scans are unchanged. Main loop: a settle drains
  through the runner without the adapter clear, so every late byte is counted.
- **Runner and owner evidence** (`examples/common`): `Result::waitDiscarded`,
  `DrainResult::bytes`, `RecoveryResult::discarded`.
- **Harness**: `bench_probe.py` grammar (`past-silent`, one tuple), reply
  validation (silent list, settles, boundary, request accounting, immutable
  progress), firmware release of a delivered recovery at a `past-silent`
  begin, `discovery-check --cross-boundary`. New `scripts/bench_multi.py`
  (D1-D3, G1-G4, R1) with `test/bench_multi_test.py` (registered; runs with
  `sys.modules['serial'] = None`).
- **Help**: `discover` help said 20 tokens; the console takes 22 with `@ID`.
- **HW-15 (item 4)**: not changed. The input-start arm and trigger routes keep
  the 1 s wait like serial starts, so E1 needs the boot-time `ApplicationOptions`
  wait and a second image; not built (E1 NOT RUN).

## Images

| Image | SHA-256 | Use |
| --- | --- | --- |
| Arduino X (prompt 18) | `5E71644794122AA6C21E1FF9C31FDFE08919859318C470DB97D6278A1287654E` | on the board at the start |
| Arduino A (`08a88eb` + this tree, firmware source diff `e3d2f497`) | `92ACE8BF8334F54659A6AC9B4C145448D867E5AB824039B0AAB7DBD0B08EF4AE` | flashed 20 November; every case; on the board |

Flash: `compare_before` 573/573 equal, `before_flash_check` PASS, esptool
write of the four files (`2026-11-20_wb19_flash_a/`, same command as prompt
18), hashes verified, post-flash check PASS. Two planned controller resets
(RTS), each after `before_flash_check` PASS (`reset_a`, `reset_b`; the first
`reset_b` attempt did not restart the board, uptime 274074 s, the second did,
18 s).

## Results

| Case | Run | Level | Verdict | Measured |
| --- | --- | --- | --- | --- |
| D1 | `d1` | drive-reported | PASS | `debug raw`; probe and identity at 1, 2, 4: 6 transactions, 6 frames, 60 bytes received (7 + 13 per drive), 0 discarded, 0 failed; each reply from the addressed drive |
| D2 | `d2` | drive-reported | PASS | `discover addresses 1 8 identity requests 32 past-silent`: 1, 2, 4 responders with identity (active node = address), silent `[3,5,6,7,8]`, 4 settles `recovered`, 0 late bytes, 15 requests, outcome complete, INTERLOCK with boundary required; `probe 4` refused `busy` with 0 frames and `discover restore` refused `recovery_required`; `recover`, restore (tuple and selection as before), finish; `probe 1` success |
| D3 | `d3` | drive-reported | PASS | same scan without `past-silent`: 1, 2 responders, 3 no response, transport fault, INTERLOCK; restore refused until `recover`; then restore and finish |
| G1 | `motion` | drive-reported | PASS | 10 staged and 10 sequential host absolute plans alternating 1: +2000, 2: -1500, 4: +500 with each origin: all completed; endpoint error drive 1 0/-1, drive 2 0, drive 4 -1 (20 of 20 each). Start TX spread staged 17.2/18.0/18.2 ms, sequential 41.2/42.9/43.9 ms (min/median/max); first RUNNING bound spread staged 15.9/18.0/18.2 ms, sequential 36.0/37.0/37.2 ms (controller clock, not physical skew) |
| G2 | `motion` | sent | PASS | member 1's target equal to its read position: plan refused `invalid` before any frame (zero displacement), as documented |
| G3 | `motion`, `motion_c`, `motion_d` | drive-reported | **FAIL** | Group stops 1.5 s into ±6000 absolute plans stopped every member ~4550 short (4 stops). After the stop the next `group read state` reported one member's successful read as not current (stale) in 3 of 4 stops (member 1, 2, 1). In both such cases that reached another plan, it was refused `unavailable` before any frame (in `motion` the old harness stopped at the stale read): `motion_c` fast leg twice (a later mixed plan with that member relative was admitted), `motion_d` every plan for 5 s of diagnostic attempts and both legs. `motion_c` normal leg (no stale read): resumed to the targets and returned, errors 0/-1 |
| G4 | `motion_c` | drive-reported | PASS | sequential 1: absolute +1000, 2: relative -800, 4: absolute +300, then the return: endpoint errors 0/-1 |
| E1 | — | — | NOT RUN | arm and trigger keep the 1 s post-enable wait; measuring 0.2-1.0 s needs the boot-time wait option and a second image (not built) |
| B1-B4 | — | — | NOT RUN | deferred: owner away (drives powered one at a time; B4 is the same-day control row) |
| B5, K1 | — | — | NOT RUN | no fail-safe bias fitted (owner) |
| R1 | `motion_c` | drive-reported | PASS | all drives powered; per drive: probe, identity, config, state; +1000 (999-1000); +3000 stopped by `stop fast` 1 s in (952-954), standstill; one-member group +500 (499-500) |

`motion_b` (G3, G4, R1) is a harness-order run, not a drive result: the
restoration at the end of `motion` staged the profile words, which revokes
the start-speed relation of the members until a controller restart, so every
group plan was refused `unavailable` (selected-axis moves passed). Fixed by the
reset before `motion_c`; deviation 2.

Acceptance: 1 met (D2, D3 PASS, tests and guide). 2 met (disposition table
below). 3 partly: G1, G2, G4 PASS, G3 FAIL as measured; `move_absolute`
reason and evidence updated. 4 E1 NOT RUN with reason. 5 B1-B4 deferred,
multi-drive 01 open points unchanged. 6 K1 NOT RUN (no bias).

### Multi-drive 06 disposition

| 06 case | Disposition |
| --- | --- |
| 1 Addressing | D1-D3 here (image A, raw traffic): PASS |
| 2 Unicast group start | HIL regression (25 staged, 25 sequential, 10 group stops), 09 B1-B2; here G1 20 per-member plans with start TX and first RUNNING spreads as distributions; completion per member |
| 3 Broadcast | Not applicable (D5); investigation B1-B4 deferred |
| 4 Trigger start | 09 B1 (50 shared starts) and B3 |
| 5 Post-enable wait | Retention: HIL regression; drive timing E1 NOT RUN (above) |
| 6 Wired I/O | 05 mapping, 06 outputs and custom 9/10, 07 stop inputs, 11 limits, 12 switches, 13 shared segments; restart persistence: 17 (deferred to the owner) |
| 7 Short regression | R1 here with all drives powered: PASS; single powered drive deferred with B1-B3 |

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1, 2, 4 | `0x0021/5` | group and selected moves | `motion-profile stage 100 100 60 0 5000 0 A` at the end of each motion run, read back | baseline |
| 1, 2, 4 | host member scale, reference, origin | host only | `group clear` | none on the drive |
| 1, 2, 4 | position, HOMED | live | recorded | drive 1 35849, drive 2 26065, drive 4 111224; none homed |

`final_compare` (23 November, image A): 573 fields, 0 differ, 0 unknown. RAM
only, nothing saved, no communication setting written. The last profile stage
revoked the start-speed relation of drives 1, 2 and 4 until the next
controller restart (host state). Bench outputs never switched: **0 relay
operations** added. Outputs off; COM13 closed.

## Deviations and dispositions

1. **G3 stale read and refusal (open).** Reproduced deliberately (`motion_d`)
   with the member status captured at each refusal: the member was current,
   fresh, stopped, alarm-free, not reserved and not settling, yet the plan was
   refused `unavailable` with no reason in the reply. The native fake did not
   reproduce it (its drives cannot hold a long move). Likely the interrupted
   member move finalizes after its stop and leaves state the absolute route
   rejects; not established. Missing measurement: the member's state-block
   invalidation times and the readiness term that refuses (the group refusal
   names no reason). Safe: refused before any frame. Next: name the group
   refusal reason, then a native regression of the late finalization.
2. **Profile restoration between motion runs** (`motion_b`): see Results.
3. **Port open error.** The first `reset_b` check failed to open COM13
   (Windows error 995 while configuring); retried after 3 s, PASS. The failed
   attempt's log files were removed before the retry (not retained).
4. **Usage-limit stop.** The session stopped between `motion_c` (20 November)
   and `motion_d` (23 November) with the board idle; `before_flash_check` PASS
   on 23 November before any traffic.

No uncertain write was replayed; no motion continued after a case; only COM13
was opened.

## Deferred to the owner (D15)

B1-B3: power one drive at a time (pull the other two power connectors) and run
20 broadcast starts and 20 stops per drive; B4 the same-day control row. B5/K1
(optional): fit fail-safe bias, set one drive to DIP address 3, link check with
1- and 2-word reads, then back to 4. The broadcast trial route and the link
check (items 5 and 6) must be built first.

## Checks

| Check | Result |
| --- | --- |
| `discovery_app_test` (10 new past-silent cases), `discovery_console_test`, `discovery_harness_test` (3 new), `runner_test`, `bus_owner_test` | PASS |
| `test/bench_multi_test.py` (new, registered, `serial` = None) | PASS |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb19_quick` | PASS (122 registered CTest tests, package and installed consumers) |
| `generate_ess_registers.py --check`, `check_ess_operations.py`, `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench` | PASS |
| Arduino `bench_s3_load_timer` (image A) and native ESP-IDF 5.5.5 (`C:/esp/build/wb19`) | built; image A is the final firmware source (diff hash unchanged); IDF image `C78D81B0F6242DD9BC38E595E668475C217933FA1B3C6E0752E715FFCD574F4E`, not flashed |
| `bench_records.py check` per folder | integrity PASS everywhere; session PASS `d1`, `d2`, `d3`, `compare_before`, `final_compare`; FAIL as recorded `motion` (G3/G4/R1 not reached), `motion_b`, `motion_c`, `motion_d` (G3) |
| Remote CI (four jobs) | Not run (lead); firmware, console and harness changed, so required on the recording commit |

## Evidence

`build/bench/2026-11-20_wb19_*` and `2026-11-23_wb19_*` (not committed):
`compare_before`, `flash_a`, `explore` (local group commands), `d1`, `d2`,
`d3`, `motion`, `motion_b`, `reset_a`, `motion_c`, `reset_b`, `motion_d`,
`final_compare`. Each run folder holds `scenario.json`, `session.jsonl`,
`console.log`, `baseline_before.json`, `ending_reads.json`, `multi.json` and
`result.json`.

## Blockers and next work

- G3 open item above; then rerun G3 and a short group regression.
- B1-B5 and K1 with the owner, after the broadcast trial route and link check.
- E1: a boot-time post-enable wait option, a second image and per-drive
  sweeps on drives 2 and 4.

## Minor open items

- `bench_multi.py` checks endpoints against a ±2 increment tolerance (drive
  feedback offset); measured errors were 0 or -1.
- The status `deadline_us` of the selected axis shows 0 during a discovery
  settle (cosmetic).
- `discover` reply `findings` keep no per-address record of silent probes
  beyond the bitmap (by design; the wire timing of a silent probe is not kept).

## After the audit

Independent audit `aud-20261124-095050-1c0ff2` of `44e7f4f` (CI run
45146685010 green). Fixed, with native or host tests, no hardware rerun:

| Finding | Change | Test |
| --- | --- | --- |
| F1 pre-TX late byte | `Request::quietBeforeTx` (discovery `past-silent` only): a byte while waiting for bus idle ends the request before TX (`UNEXPECTED_RX`, fault, interlock); the old test that expected the probe to go out is reversed | `pastSilentByteBeforeTxSendsNothing`: no new frame, no responder, before the first silence and after a settle |
| F4 final silence | the last silence settles too; clean settle completes, no request left reports `request_limit`, late bytes the fault; D2 now expects five settles (the bench run measured four with image A) | `pastSilentAtLastAddressAndSilenceOnly` |
| F5 refused restore | `discover restore` during a pending recovery or transfer is refused and the scan stays INTERLOCK; a restore in progress replies OK | `pastSilentRestoreRefusedDuringRecovery`: early restore, then no automatic restore; failed recovery |
| F7 echo | silence also requires zero echo bytes | not representable: discovery requests use `Echo::NONE` |
| F2 G3 stop check | the group stop result and fresh stopped reads of every member gate any later plan; otherwise the prepared stops, standstill from fresh reads, and the case ends; each repeat needs the latest attempt's evidence | `test_stop_evidence_gates_later_plans` (failed stop, unmoved member, still running) |
| G3 reason | a group refusal names the member check (`member_binding`, `member_identity`, `member_configuration`, `member_profile`, `member_reference_stale`, or the first failing move readiness term); the harness accepts these | `testGroupRefusalNamesMemberCheck` (revoked start-speed relation, lost configuration) |

The G3 cause is still not established; the next G3 run will name the refusing
check. F3 (R1 profile restore per drive) needs R1 reordered and is in the
backlog with F6, F8 and F9.

Checks after the fixes: `discovery_app_test`, `discovery_console_test`,
`runner_test`, `bus_owner_test`, `group_app_test`, `discovery_harness_test`,
`bench_multi_test` (`serial` = None) PASS; `check_repository.py`,
`check_ess_operations.py`, `generate_ess_registers.py --check`,
`check_promptset.py` PASS; `verify.py --mode quick` in
`build/verify/wb19_audit_quick`: PASS (122 registered CTest tests, package and installed consumers). Builds (not flashed): Arduino
`937D69BE0CB1AFCB2387AA2C80C72BD84C2587E55C8733FC2BEC8315DC822100`, native
IDF 5.5.5 `5E45A4BEFBA0058498FE1E840C3533B17360508EFDD815D9A30F5861F28A0A1C`.

The firmware changed (discovery `past-silent` settle at the last address, the
pre-TX byte rule, restore refusal and the group refusal reason): stage (d)
needs a short bench regression, D2 on the new image and one group plan.

## Regression on the audited image, 24 November 2026

Image B (Arduino `bench_s3_load_timer` from `9b34e00`, firmware tree equal to
that commit): `937D69BE0CB1AFCB2387AA2C80C72BD84C2587E55C8733FC2BEC8315DC822100`.
Before the flash: `before_flash_check` PASS (all three drives stopped,
alarm-free, I/O `[0,1,2,3,0,0,0,0,0]`) and `compare_before_b` 573/573 equal.
esptool wrote bootloader, partitions, boot_app0 and the image, each "Hash of
data verified"; post-flash check PASS (`2026-11-24_wb19_flash_b/`). The bench
clock restarted at the flash (`bench` `now_us` 132 s at the end); the status
`uptime_ms` field kept counting across the esptool and RTS resets, so it is
not a restart witness. D2's five settles identify image B.

| Case | Run | Verdict | Measured |
| --- | --- | --- | --- |
| D2 | `d2_b` | PASS | responders 1, 2, 4 with identity, silent `[3,5,6,7,8]`, **5 settles** `recovered`, 0 late bytes, 16 requests, INTERLOCK; `probe 4` refused `busy` with 0 frames, `discover restore` refused `recovery_required`; `recover`, restore (boundary crossed), finish; `probe 1` success |
| G1 (one round) | `motion_e` | PASS | `--g1-runs 2`: staged and sequential target and return, endpoint errors 0/-1; start TX spread 17.1/17.5 ms staged, 42.9/42.9 ms sequential |
| G3 | `motion_e` | PASS | stop normal and stop fast 1.5 s in: every member ~4550/4600 short; the stop-evidence gate passed both times (stop results observed, fresh per-member reads stopped, every member moved); each resumed plan was admitted and reached the targets (errors 0/-1), then returned |

No stale group read and no refusal occurred in these two stops, so no named
refusal reason was captured. The earlier G3 failure (3 of 4 stops on image A)
did not recur; the stop gate's per-member reads now sit between the stop and
the next group read, which changes the sequence. The cause stays unresolved;
the named reason will identify it if it recurs.

Ending state: drives 1, 2 and 4 enabled, stopped, alarm-free, I/O
`[0,1,2,3,0,0,0,0,0]`, positions drive 1 35849, drive 2 26065, drive 4 111224
(every plan returned to its origin), none homed; `final_compare_b` 573/573
equal; profile words restored through the stage route (the start-speed
relation of 1, 2 and 4 is revoked again until the next controller restart);
outputs off, 0 relay operations; COM13 closed. Harness addition:
`bench_multi.py --g1-runs N` (even, 2..10).

## Commits and CI

The lead owns Git and CI. `44e7f4f` (prompt 19 work) passed CI run
45146685010; `9b34e00` (audit fixes) passed CI run 45371955989. The closeout
commit `0a94399` passed CI run 45379425041 on all four jobs (native gcc, native
clang, Arduino, ESP-IDF).
