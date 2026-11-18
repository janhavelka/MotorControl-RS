# 12 Switch homing with host-driven switches

Apply [COMMON.md](COMMON.md). Prerequisites: [06 Output functions on drive 4 Y0](06_outputs.md), [08 Input-start arm, trigger, observe and disarm](08_input_start_software.md), [10 Paired-value encodings and typed setters](10_paired_value_encodings.md), [11 Limit inputs and soft limits](11_limits_and_soft_limits.md). Gate: **G2**.

## Objective and boundaries

Run every implemented switch method, 1-14, 17 and 19-30, on the bench with
switches emulated by the host, and qualify each documented starting branch at
the drive-reported and controller-timestamped levels. Deliver:

- a **virtual switch** in the harness: origin on X0 (`mosfet1`), positive
  limit on X1 (`relay2`), negative limit on X2 (`relay1`), driven from polled
  position (a window) or from time, with a bracketed, live-enforced bound per
  emulated edge;
- per method and branch: expected against observed sequence, completion flags,
  drive 4's Y0 homed output (function 3) and the final reference;
- the **home start input** (function 11) on X3 (`mosfet2`) through prompt 08's
  route;
- **auxiliary choices** 0-11 with switch methods, extending prompt 10's
  method-35 admission, and **nonzero offsets** with prompt 10's encoding;
- a bounded **method 18** characterization, and the disposition of the
  collision methods −1..−4.

Non-goals: mechanical switch qualification, several drives homing from one
emulated switch (a window follows one drive's position), independent index or
shaft measurement, electrical timing. Emulated-switch results say so.

Absorbs all of [standalone 05](../standalone/05_homing.md) except the limit
reaction (prompt 11): N06 offset, N08 method 18 and collision, Q07 branches,
search/return/index behavior, bounded travel, stop and origin evidence, the
HW-11 rule (ARRIVED not required), the H-02 rechecks (RUNNING during a moving
home, HOMED falling after the trigger, position exactly 0, INPUT_STATUS
polarity) and the caption conflicts of methods 10, 13, 14, 26, 29, 30 and
method 28's exchanged drawings. Also [bench I/O](../../bench_io.md#test-plan-once-wired)
steps 5 and 6. Audit entries A2-F1, A2-F5, A2-F9, A2-F10 and A2-F15 apply.

## Read and audit

- Code: `src/profiles/ess_rs/Homing.cpp` (descriptors, `inputReadiness`,
  `checkHomeQualification`, `prepareHome`, `advanceHome`), `Homing.h`,
  `examples/probe_cli/ProbeApp.cpp` (`homeReadiness`, `homeRatesWithinPolicy`,
  `startHome`), `QualificationApp.h`, `HomeConsoleParser.cpp`,
  `HomeConsoleFormat.cpp`, prompt 08's route (`InputStartApp.h`), prompt 10's
  offset and auxiliary changes, prompt 11's soft-limit state.
- Tests: `test/homing_test.cpp`, `home_app_test.cpp`,
  `homing_auxiliary_app_test.cpp`, `home_console_parity_test.py`,
  `qualification_routes_app_test.cpp`.
- Guides: [homing](../../ess_homing.md), [methods](../../ess_homing_methods.md),
  [pair writes](../../ess_pair_writes.md), [bench I/O](../../bench_io.md).
- Function manual physical pages: p19-20 (methods 1-14, 33, 34 need a
  closed-loop motor with Z; external trigger is function 11), p27 (function 11
  edge-valid), p32-67 (diagrams per method, page list in the methods table),
  p51 (method 18: heading, prose and trajectory say positive limit, the three
  captions say "Negative limit signal"; visually checked), p72-73 (`0x0030`,
  `0x0031` with −1..−4, `0x0035/2`, `0x003B/0x003C`).
- Reports: prompts 05 (output and input delays), 06, 07 (stop input), 08, 10,
  11; [unattended tests](../../reports/2026-11-16_unattended_bench_tests.md#homing-auxiliary-all-twelve-choices-image-a);
  [standalone 14 regression](../../reports/2026-11-02_standalone_14_bench_regression.md).

### Primary evidence to recheck

1. Auxiliary: HEAD admits only 7 (`Homing.cpp:149`, `:193`,
   `QualificationApp.h:56-57`); prompt 10 adds 3 and 6 for method 35. The
   backlog's "homes ran with 0 and 7" is not supported by this route.
2. Offset: nonzero refused at `Homing.cpp:148`, `:190-192` and
   `ProbeApp.cpp:1569` until prompt 10's change.
3. Completion: HOMED low, RUNNING, stopped HOMED high, then a zero position
   (`Homing.cpp:319-321`). ARRIVED is not required.
4. Polls: 20 ms until RUNNING, then 500 ms (`ProbeApp.cpp:1680-1681`,
   `:43`), at most 64, within `moveTimeoutMs` ≤ 30 s. Expiry ends observation;
   it does not stop the motor.
5. `inputReadiness`: an active limit the method does not require fails a poll
   (`:88`); functions 7-13 on a connected terminal refuse (`:96`), so X3 = 11
   blocks serial `home`; an active stop input (4-6) fails too.
6. Bits 5/6 in `FAULTS` (`:13-16`, `:333-334`): use prompt 11's result first.
7. Rate policy: word × subdivision / 60 ≤ 50,000 (`ProbeApp.cpp:46`,
   `:1639-1645`).
8. `read state` runs while a home runs (`startTypedRead`, `ProbeApp.cpp:973`).
   Measure its cycle and its worst gap under the home's own polls.
9. Homes 33/34 completed on the free shaft: evidence for the `index` fact on
   this model and firmware only.

## Files and interfaces

- Harness: new `scripts/bench_switch.py` with `test/bench_switch_test.py`,
  registered in `CMakeLists.txt` beside `bench_scenarios` (line 195); runs use
  prompt 04's `scripts/bench_records.py`. Switch definitions: terminal, output,
  mode, bounds relative to the start, hold, speed bound. The loop polls
  `read state` through `Console`, decides with a pure function, sends
  `bench on OUTPUT HOLD_MS`/`bench off OUTPUT`, renews holds and logs each
  sample interval, decision and GPIO time.
- Core: `Homing.cpp`/`Homing.h` for auxiliary choices with switch methods, the
  expected final coordinate and offsets per prompt 10's encoding. The method 18
  descriptor stays UNRESOLVED until promotion (item 4).
- Application and console: `homeReadiness`, `startHome`, `QualificationApp.h`
  facts, the `home` grammar from prompt 10, reply fields for auxiliary and
  expected coordinate; the staging for prompt 08's `arm home` kind (refused as
  `unsupported` until this prompt; core effect `HOME_COMPLETION`).

## Implement

1. **Virtual switch, in the harness.** The firmware `bench` command stays
   GPIO-only; position exists only in ordinary reads. Window mode: origin
   active while position lies in `[a, b]`, a limit active beyond `c`. Time
   mode: edges at fixed delays after the trigger acknowledgement, for
   initially active branches and as a cross-check.
   **Bound.** A crossing of boundary `b` happened between the previous sample
   that showed the old side (from its request start `t₀`) and the current one
   (to its reply end `t₁`). The drive's input changes by
   `t_gpio + d_out + d_in`, where `d_out`/`d_in` are prompt 05's output and
   drive-input delay maxima. The emulated edge therefore lies within
   `Δx = v_max · (t_gpio + d_out + d_in − t₀)` of `b` on the travel side, with
   `v_max` the larger of the commanded rate (word × subdivision / 60) and the
   fastest observed sample speed, plus a stated margin. Preflight uses the
   budget `G` (worst sample gap, measured with the home's polls running) and
   `D` (worst decision-to-GPIO time): `v_max · (G + D + d_out + d_in)` must be
   at most a quarter of the narrowest window or gap, and every window wider
   than `2 · v_max · G`, else the geometry refuses before the trigger.
   **Live.** Each sample gap, decision delay and speed is checked against the
   budget. A gap or delay over budget, a speed over `v_max`, or a window
   crossed between two samples invalidates that edge's verdict and takes the
   stop path (`stop fast`, then fresh stopped state). Timing evidence stays
   separate from any independent switch position, which this bench lacks.
   Holds are 3 × the cycle, renewed each cycle; a lapsed hold only deasserts
   that switch, it never stops a search. Travel guard: position outside
   `[start − B, start + B]` sends `stop fast`.
   Unit tests: decisions, bound, refused geometry, budget overrun, skipped
   window, renewal and guard.
2. **What ends motion if a component fails.** Where X3 is free (all cases
   except H10), it carries a **dead-man stop**: function 5 with inverted
   polarity, held inactive by `mosfet2` on with a renewed hold. If the harness
   or the USB link stops, the hold lapses and the drive's own stop input ends
   the search; a controller reset also turns the output off. It needs prompt
   05's status mapping and prompt 07's stop evidence; case D0 verifies it
   first. Remaining exposure: a controller that hangs with the GPIO on, and
   H10, where X3 is the home input; there the search runs at the search speed
   until the controller, the link or the owner recovers (D11). Every such run
   ends with fresh stopped state on every drive. Without D0 the exposure
   applies to all cases and the scenario record says so.
3. **Auxiliary and offset.** Extend prompt 10's admission to switch methods:
   stored and qualified choice 0-11; expected coordinate 0, offset or −offset
   for 0-2 and 6-8; for 3-5 and 9-11 the observed coordinate is published and
   the harness checks it against the coordinate it tracked; 0-5 run the offset.
   Use 9 for geometry: it keeps coordinates across repeated runs.
4. **Method 18 characterization, then promotion.** The descriptor, `caps` and
   ordinary `home` stay UNRESOLVED. One experiment per hypothesis, each an
   exact-target record (drive 1, generation, native words 60/30/100, travel
   guard 6000, dead-man stop, restoration of homing words and I/O): H+ with
   X1 = 2 and X2 = 0, H− with X2 = 3 and X1 = 0, the window emulated on the
   tested input ahead in the positive direction. The record sets that run's
   required input, so the window is a required input, not an `INPUT_REQUIRED`
   failure. A reversal at the window supports that hypothesis; no reversal ends
   at the guard. Promotion to implemented needs one hypothesis with both
   branches passing twice, the ledger note and native tests, in a separate
   change; otherwise method 18 stays unresolved with the evidence.
5. **Collision methods −1..−4 stay unresolved**: only the p72 range row and
   p73 parameter rows exist, with no trajectory or direction, no wire form for
   negative values in a register printed `0~65535`, `0x003B/0x003C` without
   access marker, `0x0122/0x0123` reading 0/0 outside their ranges, and no hard
   stop on a free shaft. A run needs a mechanical stop (G3) and a vendor
   encoding.
6. **Home start input** through prompt 08's route: X3 = 11 on the tested drive
   only, `arm trigger ID mosfet2 WIDTH_MS`; shared-output check per COMMON.
7. **Homed output**: drive 4 Y0 = 3 through prompt 06's route.
8. Native tests for 3, 4 (refusal without the record) and 6.

## Bench procedure

1. Scenario record per COMMON. Drive 1 for every branch; one representative per
   family on drives 2 and 4. Stopped drives are read; prompt 11's L10 gives the
   effect of shared inputs on them. Write list: homing words, auxiliary, offset
   words, X3 function and polarity, drive 4 Y0. Bounds: subdivision 1000,
   H = 60 and L = 30 native, ramp 100, B = 12,000 increments.
2. Geometry relative to the start `s`: origin window 1000 increments wide,
   3000 ahead in the search direction; the family's limit 4000 beyond it; the
   other limit never asserted. Branches come from the start: before the window
   (approaching), inside (active), between window and limit (beyond).
   Reconstruct each path from samples: direction, H or L slope, reversals,
   edges, INPUT_STATUS bits.
3. Cases:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| D0 | X3 = 5 inverted, `mosfet2` held | +2000 move, 30 rpm; let the hold lapse mid-move | Drive stops | drive-reported, controller-timestamped | PASS enables item 2 |
| H1 | 17, 1 (negative limit); 2 (positive) | Inactive and active start | Limit-reference rows | drive-reported, controller-timestamped | Phase order matches; final edge within Δx |
| H2 | 19-22 | Inactive and active | Origin rows | as H1 | as H1 |
| H3 | 3-6, with Z | Inactive and active | Final Z within one revolution past the edge | as H1 | as H1; repeated runs (aux 9) end on one Z within Δx |
| H4 | 23-26, then 7-10 | Approaching, beyond, active | Positive-limit family rows | as H1 | as H3; captions 10, 26 checked |
| H5 | 27-30, then 11-14 | Approaching, beyond, active | Negative-limit family rows | as H1 | as H4; captions 13, 14, 29, 30, method 28 |
| H6 | Method 20, switch never asserted | Search runs | Guard stops it; no reference | drive-reported | No reference published |
| H7 | Method 24 mid-search | `stop normal`, then `stop fast` | Outputs off, home fails | drive-reported | As H6 |
| H8 | Method 20, offset 0 | Auxiliary 0-11 | Coordinate per choice | drive-reported | As documented or recorded |
| H9 | Method 20, offset ±2000 | Auxiliary 0-11 | Offset motion for 0-5; coordinate | drive-reported | As H8 |
| H10 | X3 = 11 (no dead-man), methods 35 and 20 | `arm trigger ID mosfet2 50`; width 2000; after `disarm`, `bench on mosfet2 50` during a home | One home per edge, or recorded | controller-timestamped | Last edge unattributed by design |
| H11 | Item 4, H+ then H− | Inactive and active start | Reversal on the sampled input, or guard | drive-reported | Decides method 18 per item 4 |
| H12 | Drive 4, Y0 = 3 | Method 24 | `aux1` edge between last low and first high HOMED poll | controller-timestamped | Inside the bracket, else indeterminate |
| H13 | Method 24, then prompt 11's enable route | Enable, move inside | Soft limits act after a switch home | drive-reported | As prompt 11 S2 |

4. Run every branch once and family representatives twice; auxiliary 7 for
   the zero contract, 9 for geometry. Any edge whose live budget failed is
   indeterminate for that branch, not PASS.
5. Restoration: generated per field from the baseline diff, including
   `0x0018`, both soft-limit pairs and the soft-limit state after H13, homing
   words, auxiliary, offset words, X3 function and polarity, Y0. Routes
   (prompt 10's homing and pair routes, `driver set`, `home auxiliary`,
   `io set`) are shown before the first write; then bench outputs off, drives
   stopped, enabled, alarm-free; COM13 closed.

## Error cases and negative tests

- Geometry over budget refuses before the trigger; a live overrun invalidates
  the edge and stops the drive.
- Stored auxiliary or offset differing from the qualified one, an unmeasured
  offset encoding, method 18 without its experiment record, and collision
  methods refuse before TX.
- With X3 = 11 connected, serial `home` refuses (`Homing.cpp:96`).
- A lost trigger echo leaves execution UNKNOWN and no reference; no replay.
- A final coordinate that differs from the choice's expectation fails without
  a reference.
- Harness or link loss: the dead-man stop ends the search where present;
  otherwise the stated exposure applies and standstill is read afterwards.
- Native: every auxiliary choice with zero and nonzero offset, wrong stored
  words, lost ACK at staging and trigger, stop and cancel in each phase,
  unchanged outputs on refusal.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick` (includes the new Python
test); `python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino and
ESP-IDF builds; four CI jobs on the exact final commit.

**Acceptance.**
1. The virtual switch with bracketed bound, preflight, live enforcement and
   guard is in the harness with tests; every emulated edge has its bound or an
   indeterminate verdict.
2. Every branch of methods 1-14, 17 and 19-30 has a verdict per evidence level;
   phase order, flags and final reference are compared with the method row.
3. Auxiliary 0-11 and nonzero offsets run through the ordinary route with the
   measured coordinate per choice in the guide.
4. Method 18 is promoted with evidence or stays unresolved with it; collision
   methods stay unresolved for item 5's reasons.
5. D0, H10 and H12 have verdicts; each case names what ends motion on failure.
6. Restoration matches the baseline on all three drives.

**Blockers.** Prompt 10's offset encoding missing blocks H9 (Partial). Prompt
08's route missing blocks H10; prompt 06's route blocks H12; prompt 11's enable
route not promoted blocks H13. A poll gap too large for any valid geometry
blocks that family; record the measured budget.

## Documentation

Update the [homing guide](../../ess_homing.md), [method table](../../ess_homing_methods.md)
(per-method bench disposition, method 18), [pair writes](../../ess_pair_writes.md)
(offset), [console](../../console.md) and [CLI contract](../../cli_contract.md),
[bench I/O](../../bench_io.md#test-plan-once-wired) steps 5-6, and inventory
rows `home_start`, `home_parameters`, `home_zero_offset`,
`homing_auxiliary_write`, gaps `homing_methods_remaining` and
`homing_nonzero_offset`. Report: `docs/reports/<date>_wired_bench_12_switch_homing.md`;
evidence `build/bench/<date>_wb12_switch_homing/`. Backlog: method 18 and
collision, offset, auxiliary and switch-homing rows. The report states how each
audit entry was applied.
