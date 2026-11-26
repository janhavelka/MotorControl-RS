# Changelog

## Unreleased

### Added

- Wired bench 20 closeout: the coverage matrix of the operation inventory with its
  final-image case map (`docs/reports/2026-11-26_wired_bench_20_case_map.json`, 625 rows,
  `--strict` 0 problems); an operation whose rows are all refusals rolls up as REFUSED,
  never PASS. The 26 operations exercised on frozen image H cite the wired bench 20
  report; no inventory state changed. Wired bench 19 G3 is recorded as resolved.
- Wired bench 20 G3: after a group stop, `bench_multi.py` reads the members until each
  reports stopped twice in a row (RUNNING clear, speed 0), at most 2 s
  (`stopped_states`); a position change between reads ends the wait at once. A single
  read 78 ms after the stop terminal saw drive 1's speed word still 33 with its position
  unchanged and reported "not stopped".
- Wired bench 20 firmware fixes. The Arduino loop task and the native ESP-IDF
  motor owner task have 16 KiB of stack instead of 12 KiB: an armed input start left
  2,548 bytes free at 12 KiB (deepest frames `Console::dispatch` 2,464 B, the arm
  admission 1,216 B, position preparation 1,648 and 1,584 B). The simple route's own
  host convention step (command scale, boot origin) no longer invalidates a
  start-speed relation stated for the same target in the generation just before it
  (`carryStartSpeedRelation`); the raw word, profile closure and age are still
  checked, and every other generation change still revokes it. Before, the first
  simple move after a controller restart refused a relation stated right before it.
- Wired bench 20 b6: the wired soak's drive-reported endpoint check for armed starts
  (A1, A2, B1) allows the documented ±2 increment feedback offset (wired bench 09 E1;
  the same rule as `bench_multi.py` and `bench_pt.py`). Three deliberate B1 reruns
  showed drive 1 at a steady +1 with no change within 2 s. `bench_input_starts.py`
  keeps zero tolerance by default.
- Wired bench 20 b4: the safety stops of `bench_stress.py` and the prepared stops of
  `bench_multi.py` (used by `bench_soak_wired.py`) bind a drive whose stop is refused
  `unavailable` with one `read state` and send one new stop fast. The firmware admits
  an action, a stop included, only to the selected drive or to one with a checked reply
  since the last selection change or controller restart; the refused stop sent nothing.
- Wired bench 20 soak tools. `scripts/bench_stress.py`: a plan check
  (`plan_check`) that validates each bench edge's role (attributed start
  through `arm trigger`, selector, switch, stop, negative) against the planned
  input functions of drives 1, 2 and 4, and rejects a raw edge used to start
  an attributed operation, a role that does not match the assignments, a reset
  without the interlock and a relay budget overrun (10,000 operations per
  relay, at most one per 2 s); relay counters from `bench` `ons` across
  controller restarts (`--relay-used`); `--cases`; `--wiring declared`
  (prompt 02's board table, refused together with `--declare-unconnected`);
  `stats` and `bench` in health samples; cross-reads that accept motion only
  on drives moving by design; the before-reset interlock before every
  controller reset (resets at standstill only); a group stop and one stop per
  moving drive on a USB stall; `stop fast` to every drive at once on an
  anomaly that can mean continued motion; incident files indexed in
  `incidents.jsonl` as they are written. pyserial is imported only where a
  port opens. New `scripts/bench_soak_wired.py`: the wired soak share on the
  existing runners (prompt 09 input starts A1/A2/B1/B2/A7/I1 with the
  `aux1` bracket, prompt 19 group plans relative/absolute, staged/sequential,
  group stop normal/fast, prompt 14 velocity cells on drives 2 and 4 with
  `velocity restore` on the same drive at once, prompt 13 G1 PT trigger with
  its PIN0 selector on `relay2` under the relay rate and budget, layout,
  records 1-16 and position mode restored on the same drive inside the case),
  the plan check before it starts (other drives on a switched line may hold
  only functions that command no motion: none, origin, limits, stops), group
  clear, the baseline I/O state and a per-field I/O diff after
  every case, and the prepared stops before the incident on any failure. The
  soak reads the board declarations once before any arm: the interlock read
  them on its first ON, inside an armed session, where selecting other drives
  made `motion-profile forget` refuse `busy` and A7's X3 stop pulse was never
  sent (wired bench 20 b2). It writes one `cases.jsonl` line per case and has
  its own session budget (`--evidence-mib`, default 512 MiB, about 2 h). New
  `scripts/coverage_matrix.py`: one row per inventory ID and field, index or
  route; a G3 row cannot get a final-image PASS, an UNSUPPORTED row gets a
  refusal check only, a roll-up keeps a known FAIL. Tests
  `bench_stress_test.py`, `bench_soak_wired_test.py`, `coverage_matrix_test.py`
  and an inventory test for evidence moved without the JSON.
- Discovery `past-silent` (wired bench 19): one tuple, silent addresses
  recorded beside the findings, one bounded settle per silence counted as a
  request, any late byte or foreign frame ends the scan, and a scan with
  silence ends needing `recover`, `discover restore`, `discover finish`. The
  runner reports bytes discarded before TX (`Result::waitDiscarded`), a drain
  its discarded bytes (`DrainResult::bytes`) and a recovery its drained bytes
  (`RecoveryResult::discarded`). Harness validation, `discovery-check
  --cross-boundary`, and native, console and harness tests.
- After the wired bench 19 audit: a `past-silent` request that hears any byte
  while waiting for bus idle ends before its TX (`Request::quietBeforeTx`,
  ordinary requests unchanged); the last silence settles like the others;
  `discover restore` refused during a pending recovery leaves the scan in
  INTERLOCK; silence also requires zero echo bytes; a group move refusal names
  the member check that refused (`reason`, `read_first: group read state`);
  `bench_multi.py` G3 checks the stop result and fresh stopped reads before any
  later plan and ends the case with the prepared stops otherwise.
- Wired bench 19 harness `scripts/bench_multi.py` (D1-D3 discovery cases,
  G1-G4 host-coordinate group absolute plans, R1 per-drive regression) with
  `test/bench_multi_test.py`; pyserial is imported only where a port opens.

- Wired bench 18 harness `scripts/bench_link.py`: link-loss cases on drive 4,
  one per run folder, with the fault between console sessions (RTS reset on a
  new handle, the prepared esptool command of the running image, a killed
  child process), `before_flash_check` before every reset or reflash, the
  watchdog reading of a silent window, the move outcome across a fault and the
  pulse-credit rule; owner cases refused before traffic under D15.
  `bench_stress.py` gains `--usb slow-reader`, `--usb saturate --count N`
  (sent, replied, `input_dropped` and the uncounted loss), `--skip CASE...`
  and the host OS, pyserial and USB driver versions in its records; it
  states the start-speed relation from its fresh profile read before every
  move (a profile stage had locked its moves) and stops every drive after a
  failed exit; `run_recorded(after_snapshot=False)` for a session handed to a
  reset.
- Native test `sameContextDelayedReply`: a reply delayed past its request's
  closure that matches a reused request context byte for byte is refused
  before reuse by the owner interlock and is indistinguishable after an
  explicit recovery boundary.
- Wired bench 17: stated facts for drive communication changes and
  persistence in the standalone application. `communication qualification
  install FIELD VALUE stationary effects route-back [dip-off]` and `persistence
  qualification install KIND stationary effects route-back backup TOKEN
  [complete-backup]`, each with `inspect` and `revoke` (API
  `installCommunicationFacts`, `installPersistenceFacts` and their pairs;
  `CommissioningFacts.h`): one exact request bound to the selected target,
  binding, configuration and host serial generations, 60 s, consumed by one
  begin. A parsed begin takes the statement before any refusal; the
  persistence statement names the snapshot's endpoint, which the save writes
  to. Factory restore is refused before TX (`recommissioning_unavailable`) at
  the statement and at the begin. The custom node route back `persistence node
  record` and `persistence node restore WORD SOURCE dip-observed` (API
  `persistenceNode`) writes only the word recorded from a checked snapshot of
  the same drive, with the stated DIP observation and a fresh identity read
  showing the DIP address. The restart marker and restart fact are withdrawn
  after the audit (`marker_setter_unavailable`, `restart_witness_unavailable`):
  the marker must use prompt 14's JOG setter inside the lease. Operation
  inventory host routes for the four statements.
- Harness scenario `comm-persistence` (`scripts/bench_persistence.py`): S0 live
  snapshot with the field table, its SHA-256 and backup token; owner cases
  refused before TX under D15; owner-return helpers (live/stored difference,
  save gate, factory gate, address policy, host-side restart witness, factory
  command guard);
  grammar and reply checks for the new routes in `bench_probe`.

- Wired bench 16: named position limits (D6/D12) in the standalone
  application. `position limits [set RPM RATE RAMP case NAME | default]` (API
  `installPositionLimits`, `inspectPositionLimits`, `revokePositionLimits`;
  `PositionLimitsRoute.h`, `PositionLimitsApp.h`) installs host limits for the
  selected axis within the wire ranges (rpm 1..3000, rate 1..2,560,000,
  minimum ramp 0..2000), RAM only, bound to target, binding, configuration and
  host serial generations. Selected-axis moves and staging use it, interrupts
  keep their original's limits, group, input-started and home moves keep the
  defaults. Move results add `position_limits`; `settings` reports the active
  ceiling, the default ceiling and the source. Operation inventory host route
  `position-limits`; native test `position_limits_app_test`.
- Harness scenario `alarms-failures` (`scripts/bench_alarms.py`, cases G1,
  A1-A5, F1-F4, R1-R3, N1): alarm 5 with the silence diagnostics and the
  reporting rule (`classify_silence`: `alarm_after_silence`, `drive_silent`,
  `no_alarm`), alarm-clear and starts after it, the HW-14 and Q05 reproduction
  attempts through the named limits, the step 5 failed-stop decision and the
  release after stop; `position-limits` grammar and reply checks in
  `bench_probe`. Vendor bundle `docs/reports/2026-11-19_wired_bench_16_vendor_bundle.zip`.

- Wired bench 15: control and tuning candidates. The core admits one listed
  current candidate (`DriverPrerequisites::listedCurrentQualified` and its
  field, value, algorithm word and source read) and the D7 exact restore
  (`exactRestore`, `restoreSource`: one field written back to the word of an
  earlier complete checked read of the same group and drive, whatever its
  range, a current word only at the source's algorithm word;
  `DriverError::RESTORE_SOURCE_REQUIRED`). The listed rows are 0x0103 and
  0x0106 at algorithm 3 only. The standalone application
  replaces the lock-delay-only rule and the two tuning experiments with one
  candidate row per field (`SettingCandidates.h`), one field per request,
  pinned first reads per drive and host session as restore sources, the
  deviation-threshold D6 call and `tuning candidates [check ... | threshold
  VALUE | reset]` (API `tuningCandidates`); update terminals add
  `exact_restore_source` and `listed_current`. The harness adds the
  `control-tuning` scenario (`scripts/bench_tuning.py`) and the velocity
  regression case G2.

- Velocity limits record and D6 calls (wired bench 14, `VelocityLimits.h`,
  `VelocityLimitsApp.h`). Serial velocity keeps the legacy envelope (|rpm| <=
  60, 1..1000 ms) without a call. `velocity limits qualify RPM ACCEL DECEL
  SUBDIVISION DURATION_MS normal|fast` (API `velocityLimits`) admits exactly one
  matching run, which consumes it; `velocity limits raise` admits the promoted
  table (`VelocityPromoted.h`, empty) and its interpolation rule
  (`velocityPromotedAdmission`: only between two same-ramp, same-direction,
  same-subdivision, same-stop entries at most a factor of two apart, never
  between standalone HW-14 rate cells); `velocity limits reset`, `useaddr`,
  `recover`, a host tuple change and a restart return to the legacy envelope.
  The poll interval grows so 64 polls cover the duration (up to 12000 ms); the
  stop budget follows the stop policy. Refusals name `outside_velocity_limits`.
  Caps report `velocity_limits` and the console parses 1..12000 ms.
- JOG setter `velocity jog set SPEED ACCEL DECEL` (API `setJogParameters`):
  one FC10 `0x001D/3` with no start and a readback, after a checked
  `velocity read`, the speed within the input velocity limit (60 rpm, also
  while raised: raised limits are serial-only), no active input 8-10 and
  CONFIGURE facts for them. `qualify` and `raise` are exclusive
  (`raise_active`, `cell_armed`, `cell_run_active`); a standalone promoted
  entry admits only its exact duration.
- Speed input, JOG and PV started by input: `arm velocity xN` (functions 8,
  9, 10) and `arm pv xN RECORD pin3|pv2-deadman|pv-pt-mix` (function 13), the
  selected axis only, no write at arm; the core `prepareInputStart` admits 8-10
  as `VELOCITY_RUN`. Member views add `layout`, `selector_count`, `stored_rpm`
  and `expected_direction`; the trigger admits held selector and dead-man
  outputs under the PT rules. An edge-mode PV arm (`0x0051` = 1) is admitted
  only in `pv2-deadman` (`pv_edge_needs_deadman`); `pv-pt-mix` refuses mode 0
  (`pv_pt_mix_needs_edge_mode`).
- Harness `velocity-jog-pv` scenario (`scripts/bench_velocity.py`): the plan
  checker (one purpose per terminal), dead-man setup and renewal, read-state
  slope sampling, the ramp fit, the creep detector, phase C cells with abort
  rules and the prepared `stop fast` on any harness failure; pt-segments case
  G1 (image F regression). The interlock admits a named selection member with a
  PV trigger (13); `bench_records` passes a stated input mapping to settings
  facts.

- PT records started by input (wired bench 13). `arm segment xN relative
  RECORD [group] [poll-ms N] [window-ms N]` arms a stored PT record 1-8 with
  X0..X3 holding the trigger 12 and PIN0-PIN2 (14-16); it sends no frame and
  refuses with one reason per check (`function_not_12`, `selector_layout`,
  `selector_polarity`, `position_mode_unknown|mismatch`,
  `triggers_inhibited_required`, `input_active`, `pin_levels`, `record_stale`,
  `word_order_unqualified`, `record_sign_unqualified`, `record_speed_zero`,
  `record_rate_ceiling`, `endpoint_outside_limits`, `soft_limit_state_unknown`,
  `soft_limit_endpoint`, ...). Records 9-16 refuse `record_unreachable`. The
  execution rate ceiling is the position speed limit min(2000, 3,000,000 /
  subdivision) rpm; storage still admits 0..3000. `absolute` refuses
  `pt_absolute_unresolved`: with `0x0050` = 1 records ran as displacements
  on `0x4EEA`/`0x0029` (P2). `group` arms one record for every declared member,
  each with its own per-member record read. The trigger admits held selector
  outputs (never the trigger's, no start function 7-13 on their terminals) and
  checks the armed PIN levels and record read in row M. Member views add
  `record`, `record_operation_id`, `record_target`, `record_words` and
  `selector_terminals`; caps add `segment_arm`
  `pt_records_1_8_three_selectors`, and `input_start_kinds`/`input_start_functions`
  list `segment`/12. Measured on drives 1, 2 and 4: records 1-8
  each moved their own target, singly and as a three-drive group.
- `InputStartResult::lastRequestUs`/`lastReplyEndUs`: the bracket of the
  latest consumed sample (console `last_request_us`, `last_reply_us`), for a
  host path guard.
- Harness `pt-segments` scenario (`scripts/bench_pt.py`: W0a, P1, P2, P2r, P3,
  P5, P8, P9, P10, P12) with the PIN helper `select_record`, the
  selection-consequence check over all sixteen records, the W0 path guard,
  evidence headroom and the prepared stop when a trial raises. The interlock
  adds the named selection member (row P) and held PIN outputs
  (`held_selection_problem`); the restoration planner uses the PT target setter
  from image level 13.
- Switch homing with host-driven switches (wired bench 12). Moving home methods
  admit every stored auxiliary choice 0-11 with a zero or a stated offset
  (`offset` fact); `homeEndRule` gives the end coordinate (offset, zero,
  -offset, or the observed coordinate for 3-5 and 9-11, published as such) and
  the home result adds `expected_known`. Method 35 keeps 7 and 6. The
  `soft-limits` fact also applies to a moving method with auxiliary 7, so the
  enable route accepts a switch home's zero reference. Harness: the virtual
  switch `scripts/bench_switch.py` (window mode, bracketed bound, preflight,
  live budget, travel guard, hold renewal) and the `switch` scenario; the
  interlock gains the `origin` effect, a home member's pre-start io bound up to
  the home deadline (`HOME_NAMED_IO_MAX_AGE_MS`) and `bench_renew`. A switch trial
  starts only with evidence headroom and stops a drive still moving after the
  home terminal (wired bench 12 deviations 1 and 2).
- Limit inputs and soft limits (wired bench 11). Move and velocity observers
  report `LIMIT_INPUT` (26, 28) for a stop with an assigned limit input reported
  active (connected terminals, 0x0008 read) and `SOFT_LIMIT` (27, 29) for
  `0x0007` bits 5/6; alarm and release stay `DRIVE_FAULT`. The standalone refuses
  moves, velocity, group members and arms while a limit input reads active
  (`limit_input_active`), keeps a soft-limit state (known disabled, bound,
  unknown) and admits `driver set soft-limit 1` from its own home 35 with the
  new `soft-limits` home fact, a checked pair read and the measured identity; the
  host axis soft limits are installed after the readback. Harness scenario
  `limits` (`scripts/bench_limits.py`).
- Paired-value encodings (wired bench 10). The register ledger gains an
  `experiment_windows` list (decision D4; now the sixteen PT target pairs),
  which the generator checks like reviewed windows (plus decision, prompt,
  date, family, one writable pair, never also reviewed) and emits as
  `Detail::canWriteExperiment` and a catalogue table. The codec adds the
  separate `validateExperimentWriteRequest`, `buildExperimentWrite`,
  `parseExperimentWrite` and `isExperimentWindow`, which production validation
  refuses; `Rtu::essExperimentValidator()` serves an experimental image. The
  soft-limit pairs are promoted (D10): reviewed FC10 `0x0037/2` and `0x0039/2`
  at `0x0019` = 0, two's complement, native position increments, and `driver
  set positive-limit|negative-limit` writes one FC10 and one two-word readback
  per limit, alone, at `0x0019` and `0x0018` known 0 (order 1 `unsupported`).
  `DriverProgress` adds the pair words, `PreparedDriver` the function and a
  13-byte request, `DriverObservation` decoded limits at order 0. Homing admits
  a nonzero offset within ±`0x0FFFFFFF` for method 35 with stored auxiliary 6
  (`HomePrerequisites::offsetQualified`/`offsetOrder`); completion needs the
  end coordinate equal to the offset; auxiliary 3 is refused. Console: `home
  ... zero|OFFSET`, the home `offset` fact (method 35), driver `pair_words`,
  decoded DRIVE limits, caps `limit_pair_writes` true, `limit_pair_word_order`
  0 and `home_offset`. Experimental images only (`MOTORCONTROLRS_WB10_EXPERIMENT`):
  `driver experiment` for the PT windows, kept for prompt 13. Harness:
  `scripts/bench_pairs.py` (`paired-values`: P1-P4, P3D, E1, E2, E3P, E3N,
  R10, restorations RB/E3X/E2R) and the soft-limit restoration route from
  image level 10. Measured on drives 1, 2, 4 (`0x4EEA`/`0x0029`): limit and
  offset words high word first at order 0; limits +300/-300 stop at +332/-332
  with `0x0007` bits 5/6; PT pairs store and read back; order 1 changed no
  interpretation within a power cycle; auxiliary 3 keeps the running
  coordinate.

- Wired bench 09 harness: `scripts/bench_input_starts.py` and the folder
  scenarios `input-start-single` (A1-A11), `input-start-shared` (B1-B3, short
  regression R) and `position-interrupt` (I1-I4, serial endpoint diagnostic E1)
  with named states entered by ordinary writes and read back, the shared line
  check against the state, armed starts through the firmware trigger and
  deliberate unarmed pulses. The interlock (`bench_records`) gains a named
  *start member* (row M, stationary with function 7, for D13 starts) and
  *session members* (an armed drive's I/O words from the firmware trigger's own
  `0x0040/5` read, because its `io read` is refused while reserved); truth
  table cases LI16-LI18.

- Input-started motion route (wired bench 08, software only): the core
  `InputStart.h` attributes a start that a drive input causes from fresh
  `0x0006/7` samples and caller-noted source edges (outcomes `ATTRIBUTED`,
  `INDETERMINATE` with the missing witness, `NO_START`, `UNATTRIBUTED_MOTION`,
  `SECOND_START`, `STOPPED`, ...; execution and completion separate; no write,
  retrigger or retry). The standalone application, console (`arm position`,
  `arm trigger`, `arm inspect`, `disarm`) and API (`armInputStart`,
  `triggerInputStart`, `copyInputStart`, `disarmInputStart`) stage function 7
  starts with no start word on the selected axis or group members in both
  `0x0050` modes, own the bench output mapped to the terminal, pulse it once
  after the firmware's line-isolation reads (shared truth table
  `test/line_isolation_cases.json`, D14 ordering, 1000 ms backstop) and disarm
  without stopping or changing an assignment. Home, PT and PV kinds refuse
  before any traffic until prompts 12-14. The board declares the bench output
  -> terminal map (`kFixtureBenchOutputs`, menuconfig), reported by `wiring`
  and compared by the harness. Harness commands `arm-position`, `arm-trigger`,
  `arm-inspect`, `disarm` and the `input-start` folder scenario. Guide:
  `docs/ess_input_start.md`. Not run on hardware (prompt 09).
- A DRIVE settings read of a group member (`driver read ADDRESS`) records that
  member's `0x0050`/`0x0051` for the input-start arm.

- Standalone `bench` command for the bench board's relays, MOSFETs and isolated
  input, on Arduino and native ESP-IDF. Every on has a 1..60000 ms hold with a
  timer and a service-loop backstop; every stop request turns all outputs off.
  The input interrupt records edge times. Wiring guide in `docs/bench_io.md`;
  nothing is wired or hardware-tested yet.
- Board-declared terminal wiring for the standalone application
  (`examples/common/FixtureWiring.h`, `ApplicationOptions::fixtureWiring`). A
  validated, immutable table applies at boot, after `useaddr` and to new group
  members; `wiring`/`group wiring` override it for the session (source `host`).
  An absent table keeps the old defaults; an invalid one makes every
  declaration unknown. `wiring` adds `sources`, `board` and `fixture`; group
  member views add `input_sources`, `output_sources` and `board`. Arduino bench
  images take the table from `BoardPins.h`, native ESP-IDF from `menuconfig`.
  `declareWiring`/`copyWiring` expose the selected-axis route. The harness
  checks the new keys and refuses `--declare-unconnected` over board-connected
  terminals or an invalid table.
- Settings routes with connected terminals (wired bench 03). Start speed,
  homing auxiliary, the profile stage and the two tuning experiments take the
  passive unconnected path or four current `configure` input records; DRIVE
  fields use the same records and PT/PV/start records `segment` records, with
  no assigned limit/stop/control input active. A refusal the records decide is
  `qualification_required` with `reason` (`input_facts_required`,
  `input_facts_stale`, `input_active`), `purpose`, `read_first` and `terminal`,
  before any frame; the single settings' revalidation after their prior read
  reports `reason` with `update_not_admitted`. API: `copySettingsRefusal`, the
  reason as the submit status message. Harness:
  `bench_probe.install_input_facts`, `bench_scenarios.py --input-facts
  configure` and `motion-profile stage --input-facts configure`; the settings
  scenario refuses `input-filter` on a board-connected drive without it.
- Wired-bench harness (wired bench 04, example tooling): `scripts/bench_records.py`
  with the fixture description, declaration agreement, the shared-output
  interlock (`bench_on`, truth table L1-L7, cases in
  `test/line_isolation_cases.json`), `before_flash_check`, exclusive run
  folders, scenario records, `result.json`, the offline checker
  (`bench_records.py check FOLDER`: integrity and session success, exit 0/1/2),
  per-field baseline comparison, restoration planning and restoration through
  ordinary setters, capture and re-baselining, and interval correlation.
  `bench_scenarios.py` scenarios `input-map`, `y0-map`, `baseline-compare`,
  `baseline-capture` and `restore` with `--run-dir`; `run_recorded` takes the
  folder layout and writes a console transcript.
- Recorded input status mapping (wired bench 05): `RECORDED_INPUT_MAPPINGS` in
  `examples/probe_cli/IoStatusRecord.h` (model `0x4EEA`, firmware `0x0029`,
  after inversion on X0-X3, measured on drives 1, 2, 4), the console mapping
  token `recorded` for input installs, refusals `mapping_not_recorded` (with
  `read_first` `read identity` without a fresh identity) and
  `mapping_contradicts_record`, and `copyRecordedInputMapping`;
  `installIoQualification` refuses a stated mapping against an applicable
  record. Harness: `status-mapping` (M01-M04, `--mapping-cases record` for
  M05/N01) and `input-timing` scenarios, `bench_records.status_bit`,
  `classify_mapping`, `timing_trial`, the interlock option `read_last`,
  `apply_write(check=...)`, input-polarity restoration with per-terminal
  mappings, install mapping `recorded`.
- Snapshot restore routes (wired bench 06): `home restore SNAPSHOT_ID|inspect`
  and `velocity restore SNAPSHOT_ID|inspect`, API `restoreHomingParameters`,
  `restoreVelocityParameters` and `copyParameterWrite`. Each writes the exact
  words of an earlier checked `home read` (`0x0031/6`, raw offset included) or
  `velocity read` (`0x001D/3`) of the same drive and binding with no start,
  then reads them back. They need fresh stopped, alarm-free state and no
  uninhibited home-enable (11) or speed/JOG (8-10) input. The axis is reserved
  until the readback; a mismatch reports `readback_mismatch`; a lost
  acknowledgement is never resent and refuses the next restore until a read.
  Each binding keeps 8 snapshots, with its first checked read pinned.
- Recorded output status mapping (wired bench 06): `RECORDED_OUTPUT_MAPPINGS`
  (`0x4EEA`/`0x0029`, Y0 after inversion, Y1 not recorded), `recorded` for
  output installs, `copyRecordedOutputMapping`. Caps `output_function_11` and
  `cross_mapped_custom` (`not_admitted`) and `output_status_mapping`
  (`recorded_y0`). Harness: the `outputs` scenario (`scripts/bench_outputs.py`,
  cases O1-O9), and snapshot routes in the restoration planner.
- Stop and release inputs (wired bench 07). One admission rule for moves,
  stages, group moves, velocity and home: refused before TX while a fresh
  input status, read through the terminal's move record (default polarity
  needs none), shows an assigned release (4), stop (5) or emergency stop (6)
  input active, with `unavailable` and `reason` `stop_input_active`,
  `release_input_active`, `input_state_stale` or `input_mapping_unknown`
  (`purpose` `motion`, `read_first`, `terminal`; API `copySettingsRefusal`).
  Velocity now needs a fresh checked input status like a move. `enable` is
  refused while an assigned release input reads active (measured: the input
  outranks a serial enable); caps `external_enable_precedence` is
  `release_input`. Stops are never refused by an input.
- Core move and velocity observers (wired bench 07): `MovePrerequisites` and
  `VelocityPrerequisites::stopInputMask`/`stopInputActiveLow` add `0x0008` to
  each observation read. A move ends `MoveError::STOP_INPUT` (24) after a report
  with that input active and a stopped drive, or `STOPPED_SHORT` (25) after two
  consecutive stopped reports without arrival once running was seen, instead
  of polling to its deadline; a velocity run fails `VelocityError::STOP_INPUT`
  (26) or `STOPPED_EARLY` (27) and keeps its timed stop. Neither is ever a
  completion. Results add `stop_inputs` and the first report as evidence.
- Sequential group moves send no later start once a started member reported
  its stop/release input active.
- Harness: the `stop-inputs` scenario (`scripts/bench_stop_inputs.py`, cases
  S1-S9, R1, R2a, R2b, R3, G1, G2) with the Implement 3 trial classification;
  `test/stop_input_parity_test.py` checks the actual application terminals
  against the validators.

### Changed

- `communication begin` and `persistence begin` need the stated facts above;
  the application no longer reads route-back, backup and DIP facts from
  `commissioningPrerequisites`/`persistencePrerequisites` fields that only
  native tests could set. The DIP fact (`addressDipOffQualified`) now states
  that the operator will set SW2-SW5 OFF before the activation restart, since
  `0x0013` is written while the switches still set the old address. A begin
  without a valid statement is refused `unavailable` before TX (measured on
  image T). Register ledger notes for `0x0003` and `0x0013`-`0x0015`.
- Accepted alarm-clear bytes start the 1 s enable wait (HW-15), as an enable
  does. Wired bench 16 A3 (drive 4, firmware `0x0029`): a start 0.59 s after
  the clear's acknowledgement was ignored 3 of 3 times; starts from 0.91 s were
  acted on. Advanced moves, staging, group moves, velocity, home and the
  input-start arm answer `busy` during the window; `moveby`/`moveto` wait. The
  input-start trigger now checks each member's wait as well (refusal
  `enable_settling` before the port call). Native tests
  `testAlarmClearStartsEnableWait` and the alarm-clear wait cases of
  `move_app_test`, `velocity_app_test`, `group_app_test` and
  `input_start_app_test`.
- Control and tuning setters (wired bench 15) admit one field per request and
  only the field's candidate row or the exact restore of the pinned read. The
  deviation threshold admits 1..4000 from any stored value (lowering a stored
  value above 4000 included); above 4000 only after the D6 call. The input
  filter admits a measured value from any stored value. Pulse mean starts at 1
  (a written 0 read back 1) and pulse low-pass at 1. The console passes a
  tuning word outside its documented range to the application as given (only
  the exact restore admits it). The planner routes every control and tuning
  field through its setter from image level 15.
- Serial velocity with a speed, JOG or PV input (8-10, 13) on a connected
  terminal needs a FINITE_MOVE record with triggers inhibited and the input
  inactive (`triggers_inhibited_required`, `input_active`); FINITE_MOVE records
  now accept 8-10 and 13 (serial moves still refuse them). `caps` no longer
  lists `external_jog` as unsupported.
- Stored PT target pairs promoted (wired bench 13 W0a, D10): the sixteen
  windows `0x0060 + 6(n-1)/2` are reviewed FC10 windows, and
  `SEGMENT_PULSE_TARGET` (`segment position N set target VALUE`) writes the
  target alone, one FC10 and a two-word readback, 0..`0x0FFFFFFF` at `0x0019`
  known 0 (`DriverPrerequisites::wordOrderKnown`/`wordOrder`); negative targets
  stay refused (sign unmeasured). Caps report `segment_pulse_writes` true and
  `segment_pulse_claim` `order_0_non_negative`. Removed with the last
  experiment windows: the ledger `experiment_windows` key (the generator now
  refuses it), `Detail::canWriteExperiment`, the codec
  `validateExperimentWriteRequest`/`buildExperimentWrite`/`parseExperimentWrite`/`isExperimentWindow`,
  `Rtu::essExperimentValidator()`, the `MOTORCONTROLRS_WB10_EXPERIMENT` build
  flag, `PairExperimentApp.h` and the `PAIR_EXPERIMENT` parameter-write kind.
  `driver experiment ...` is refused `unsupported` before TX on every image.

- Soft-limit candidates no longer reject with `PAIR_WRITE_UNSUPPORTED` at word
  order 0 (wired bench 10); order 1 still does. The home qualification refusal
  `stored_offset_nonzero` is now `stated_offset_required`: a stored nonzero
  offset needs the `offset` fact, and each home then states the offset it
  stages. `home methods` reports `offset` `zero_or_method_35_auxiliary_6` and
  `auxiliary` `[7,6]`.

- Input polarity with UNKNOWN status mapping (wired bench 05): the core and the
  application admit a change on any terminal, connected included, whose
  previous and final function are both 0; the core now takes the final function
  from the request, so one candidate that inverts a terminal and assigns it a
  function is refused without a mapping on unconnected terminals too (the
  application already refused it).
- Output polarity with UNKNOWN status mapping (wired bench 06): admitted for a
  load stated with either level (`allowedLevels` 3); inactive-only and
  active-only loads are still refused.
- The `release` action is observed as long as a configured stop (2 s plus
  margin, 50 ms polls). With output function 5 (brake) assigned, firmware
  `0x0029` reports RELEASED about 1.34 s after the release, past the old
  500 ms window, which ended in `observation_limit`. The write is never repeated.
- `scripts/verify.py` checks the velocity parity test's app fixtures, and the
  move and velocity fixtures of `stop_input_parity_test.py`.
- Shared-output interlock row L3 (wired bench 07): a running member's own `io
  read` is refused during its operation (`axis_conflict`), so a named member
  may carry its `io read` from just before the operation, with every operation
  in between recorded as a read or the case's own motion and a 10 s bound
  (`bench_records.named_io_problem`, `NAMED_IO_MAX_AGE_MS`); several named
  members per output (group moves); a named member that is stopping under its
  own operation takes the ordinary rows. The move and velocity validators know
  the three-word observation and the new failure details.
- The input-filter tuning experiment admits `MEASURED_INPUT_FILTERS`
  {0, 1, 2, 3, 5, 10, 20, 50} from and to any value in the set (was 2/3);
  100 left the candidate set because it suppressed 200 ms pulses.
- `bench_probe.py bench on` runs the shared-output interlock before the ON
  (`io read` then `read state` per drive as the last bus traffic, no other
  pending operation, a 1000 ms read-age backstop per decision D14;
  `--max-age-ms` records another bound).

- Refusals of the start-speed, homing-auxiliary, DRIVE, tuning-experiment,
  segment and profile-stage routes that were `unavailable` or `invalid` because
  of connected or unknown terminal wiring are now `qualification_required` with
  a named reason. Revoking `configure` records also cancels unsent single-setting
  and tuning writes that copied them.

### Fixed

- A serial velocity run's stop budget after its timed stop is 4.5 s (direct)
  and 5.5 s (normal), each including the ordinary stops' recovery allowance
  `STOP_RECOVERY_US`. With 2 s and 3 s, a direct stop after a late failed
  poll and a full recovery expired unsent and a normal stop lost the
  observation of a 2000 ms deceleration (`lateFailedPollStopAfterFullRecovery`).
- Explicit `recover` replies `busy`, before any traffic, while a serial
  velocity run may be moving (start on the wire, observing or stopping); it
  used to cancel the run, which then ended with no stop and the drive running.
  The run's own held stop performs the one recovery it needs
  (`recoverRefusedWhileRunOwesStop`).
- `alarm-clear` is admitted against an address's historical conflict only in
  the state wired bench 15 saw (selected axis; fresh reads after the conflict
  began with the alarm, RELEASED, speed 0 and the same position in two
  consecutive feedback samples). Alarm 5 latched during a move left RUNNING
  set with speed 0, so no stop observed completion, the conflict never
  reconciled and the alarm could only be cleared after a controller restart.
  Otherwise, and while work is active on the address, it still refuses; the
  conflict and a failed-stop result stay until a checked stop.
- Input-start sessions no longer turn INDETERMINATE once the never-drained
  32-edge `aux1` ring has wrapped: only `aux1` edges recorded since arming that
  the ring no longer retains (or a `bench clear`) count as lost witnesses.
  Found on the bench (wired bench 09 B1 on 08's image: every session from the
  17th two-edge trial after `bench clear` was INDETERMINATE); native regression
  `testAux1RingWrapIsNotLostEvidence`.
- The input-start core treats a drive as at standstill only when it reports
  RUNNING clear and speed word 0. After a serial, group or input stop the drive
  clears RUNNING while its speed word still decays and its position settles by
  up to about 30 increments; the old rule reported that tail as `second_start`
  (wired bench 09 B2/B3, A6d back-to-back reads). Native regression
  `testStopTailIsNotASecondStart` from the recorded words. A further start is
  now RUNNING reported again after the completion; a later position or speed
  change without RUNNING (still seen after some stops on drives 1, 2 and 4,
  with no second move) sets `further` to `not_excluded` and is never reported
  as `second_start`.
- The `input-start` harness scenario kept the profile session open after its
  fresh `motion-profile read`: the shared baseline read helper forgot it, so
  every arm on the bench was refused `move_readiness` (wired bench 09 pilot).
- Group stop-on-failure moves require result capacity and reserve stop/recovery
  IDs before staging. Retained results and unrelated stops cannot consume that
  operation's fallback resources. An explicit stop keeps the reservation until
  it settles; recovery invalidates probe caches at admission.
- Same-value polarity writes follow the connected-terminal reply rule.
  Assignment inspection uses the same exact-read checks as write admission.
- I/O settings check each intermediate FC06 state. Connected terminals use
  confirmed replies; explicit protection removal and output load limits apply.
- Input qualification keeps its original lifetime across refreshed reads.
  Revocation cancels dependent unsent work even after future facts were consumed;
  selected-axis staging keeps the prepared write deadline.
- The portable S2 consumer checks console lines without a redundant full-size
  buffer; the production console keeps its full group output capacity.
- Candidate profile staging preserves start-speed relation revocation across
  group changes, invalidates contradicted group profiles, and reports pending
  stop cancellation consistently to the host.
- Per-address enable waits survive selection, group changes, recovery and rebinding.
  Selected staging and group admission use the same original deadline and write
  evidence. Drive-address changes cannot bypass an active wait.
- Velocity retains a transmitted poll's evidence when the overall deadline expires.
- Stop results wait for attached recovery before delivery or slot reuse. Action
  and velocity JSON retain recovery and zero-byte attempt evidence across later recovery.
- Release qualification expires at the exact age limit, including while queued.
- Host communication and persistence checks derive write outcomes from raw evidence;
  contradictory communication readback cannot suppress uncertainty.
- A stop during position-stage readback now keeps the settled write/read
  evidence but cannot mark the cancelled stage ready or publish stored words.
  Both frontend application tests cover this race.
- The stress tool no longer resumes after a USB stall when its cleanup stop is
  refused, missing, failed or uncertain. It requires the matching observed-stop
  result, retains evidence and ends the run on failure even with `--recover`.
  Offline regressions now run in CI.
- Verification now rejects empty or incomplete CTest evidence, including
  listing-only detailed logs, while preserving skipped/disabled-test rejection.
  IDF fixture descriptions distinguish exported source from installed packages.
- Completion-operation coverage now requires the actual producing API and
  definitions; unrelated getters, raw codecs and declaration-only bodies cannot
  claim implementation. Cross-operation regressions cover delayed reads,
  setting/qualification invalidation, recovery, rebind and urgent stop with full
  ordinary result storage on both frontends.
- Reject homing host records that claim success after interruption by stop.
  Actual emitted-result regressions cover all 30 supported methods on both
  application frontends; core motion and stop behavior are unchanged.
- Host homing-result validation requires an integer method ID, rejecting JSON
  booleans and floating-point values that compare equal to supported methods.
- Homing rejects unknown MOTION bits as unqualified readiness while retaining
  raw evidence. Host validation also rejects contradictory logical inputs in
  low/activity witnesses and preserves core failure precedence. Active-phase
  qualification replacement, in-flight stop and the 64-poll bound have explicit
  regressions; duplicate qualification checks and zero-evidence copying removed.
- Setting sessions cancel unsent FC06 work when newer checked state/input/feedback
  contradicts admission; contradictory priors revoke stale readiness even without
  a write. Changed start speed requires an explicit, generation-bound application
  relation witness; fresh reads cannot fabricate or renew qualification.
- Start-speed coverage requires its exact producing route; setter-only profile
  help shows only callable examples, and capabilities no longer list delivered
  setters as unimplemented. Strict host restore validation now checks exactly
  the five written words while preserving both start observations.
- Velocity/home help and error usage now expose parameter reads and motion only
  when their respective callbacks exist. Human read results name unresolved
  interpretation states and distinguish numeric policy checks from drive limits;
  raw zero offset remains separate from a calibrated origin.
- Human parameter-read errors point to the callable `help home` or `help velocity`
  family instead of the internal JSON command names.
- Live host terminals now reject the conflicting kind flags already rejected by
  retained inspection; malformed move kinds raise the ordinary protocol error.
- Host settings groups reject non-string selectors with the normal protocol
  error; pending velocity results reject terminal-only stop-policy claims.
- Verification now requires the shared native-IDF simple-motion fixture to
  remain registered with its actual executable; missing or misdirected fixtures
  fail before a passing test run can hide the omission.
- Reference-owner admission accepts a typed step deadline within its immutable
  operation budget, including earlier readiness expiry. It still rejects work
  outside that budget and never replays an uncertain write.
- Shared application completion handling preserves captured timing, source
  confirmation and uncertainty while keeping each operation's response policy.

### Added

- Per-terminal I/O qualification through public application, console and host
  routes. Assignment, drive settings, segment settings and finite moves use
  separate purposes. Selected axes and group members share function-specific
  move rules; PV/JOG assignments remain excluded from finite position moves.
- Independent group-member relative/absolute requests, explicit member coordinate
  facts and inert console/host plans. All members are checked before writes;
  staging, starts, stops and results retain their own evidence.
- Validated position-profile candidates and host archive export/import through
  the ordinary application and console. One retained result, a fresh prior read
  and one deadline cover five-word staging/readback; no start, save or replay.
  Start speed remains separate. Shared group staging now keeps parent deadlines
  and copied failure evidence; target-range refusals are named before writes.
- Write acknowledgement now uses the same inclusive closure deadline in all
  seven ESS engines. Late replies and supplied execution uncertainty stay UNKNOWN.
  Checked rejected writes preserve evidence without inventing changed settings or
  a new move conflict when no setup was sent.
- Position-clear qualification binds the exact target and configuration, with
  passive API/console inspection and cancellation of revoked unsent clears.
  Active origin alone permits ordinary moves. Reference readiness and home-rate
  refusals are named; standalone home uses a nominal 50,000 increments/s ceiling.
- Ambiguous valid home-trigger echoes permit bounded status/zero observation.
  Observed completion cannot change UNKNOWN execution or publish a reference.
  Console and host validation retain the same evidence and failure precedence.
- Separate velocity poll dispatch/settlement deadlines and a guarded
  `rebindVelocityStop` continuation. Explicit and velocity-owned stops with
  zero accepted bytes can use one bounded recovery; original failures and
  recovery evidence remain retained. Partial or lost stop writes never replay.
- The real stop-path fixture now runs on both Arduino and native ESP-IDF adapters.
- Public standalone `submitStop` and copied action results, with two protected
  fast-stop escalation slots beside two primary stop results. Console results
  identify exact slot replacement. Qualified release after a failed stop is
  explicit and preserves motion uncertainty; enable/alarm clear stay blocked.
- Cancelling a transmitted ordinary probe lets it settle without faulting the
  bus. Discovery restoration failures and post-ACK stop bus faults end promptly
  with their actual evidence.
- Host stop accounting follows exact firmware replacement IDs and preserves
  old logged evidence. Ordinary, recovery, primary-stop, escalation and group
  result quotas stay separate. Older images keep explicit stop-result release.
- Typed console routes state commissioning facts on the ordinary image, with no bus
  traffic: `home qualification install|revoke`, `velocity qualification install|revoke`,
  `position-clear qualification install|revoke` and `motion-profile start-speed relation`.
  A missing or unknown fact is refused by name, and a missing read is named. `caps`
  reports them under `qualification_routes`. `installVelocityQualification`,
  `installPositionClearQualification` and their revokes join the application API.
- `ActionOptions::runningPollIntervalUs` spaces move and home polls after RUNNING.
- `bench_probe.py home` accepts methods 33 and 34, plus switch methods with explicit
  `--fact inputs` and `--wiring`. It takes the reads and installs only `--fact` values.
- `MoveContext::positionMismatch` and JSON `position_mismatch`: RUNNING-path completion
  whose final feedback differs from the known endpoint. It stays OBSERVED.
- `ActionContext::rawPositionWords` and JSON `raw_position_words`: position-clear
  words in wire order, which carry no assumed word significance.
- Portable source/installed consumer checks for both native scalar setters and
  representative admitted homing families, preserving strict C++11/C++17,
  independent headers and codec-only isolation. Storage review records actual
  host/Xtensa ABI and stack deltas separately from runtime memory qualification.
- Origin plus negative-limit homing methods 11–14 and 27–30 through the existing
  qualified API/console path, completing the 27 planned switch-method partition.
  Both inputs are required for every spatial branch; only 11–14 need internal Z.
  Preserve method 28's exchanged drawings and separate caption conflicts,
  bounded observation/no replay and physical NOT RUN status. Method 18,
  collision methods and nonzero offsets remain unresolved.
- Origin plus positive-limit homing methods 7–10 and 23–26 through the existing
  bounded core, application and console. Both inputs are required for every
  starting location; only 7–10 require internal Z. Independent vectors cover
  all 24 spatial branches and conservative sampled completion. Physical
  reversal/index qualification remains NOT RUN.
- Origin-reference homing methods 3–6 and 19–22 through the shared bounded
  core, application qualification and console paths. All require checked origin
  inputs; only 3–6 require internal Z. Independent method/branch fixtures retain
  sampled-evidence limits, status/zero completion and no replay. Physical
  qualification remains NOT RUN. Method 18 and negative methods remain unresolved.
- Limit-reference homing methods 1, 2 and 17 through the existing core,
  qualification-install API and ordinary console route. Methods 1/2 require
  the matching negative/positive limit and qualified internal Z; 17 uses the
  negative limit without index. Checked logical-input observation is mandatory
  for switch methods. Sampled edges remain diagnostic; completion requires
  fresh status transitions and checked zero. Method 18 and collision guards
  remain; physical limit homing is NOT RUN.
- Public application homing qualification install/inspect/revoke and retained
  submit/result APIs, with checked configuration, logical inputs and parameter
  readback. Independent fixture age, bounded combined status/input observations
  and copied revocation preserve final-zero evidence while preventing a revoked
  reference. `home qualification` inspects the retained facts; explicit
  `install` and `revoke` forms update them without bus traffic. Bench evidence
  covers 33/34/35; switch methods still need physical qualification.
- Typed serial position interruption in the core: exact independently qualified
  native stored-profile retrigger, copied binding/age evidence and one checked
  bit-3 trigger. Default starts and stationary observed moves are unchanged.
  The ordinary `submitPositionInterrupt` API and `position interrupt ORIGINAL_ID`
  console/host route reserve independent original/replacement results, settle
  old owner traffic and preserve urgent-stop priority. Results remain ACK-only;
  no active setup writes or completion attribution is claimed. N05 remains PARTIAL.
  The explicit intent is `PositionInterruption::INTERRUPT_CURRENT`, avoiding the
  Xtensa SDK's `INTERRUPT` macro without changing SDK state or wire values.
- Explicit homing auxiliary setting for all twelve native choices, with retained
  prior read, checked FC06 echo and independent readback. Ordinary app API,
  `home auxiliary` and strict host results share the bounded settings session.
  Possible effects expire qualification; readback 7 does not qualify active home
  policy, zero offset or additional methods. No motion, I/O change or save.
- Explicit native position start-speed setter (0..3000), retained prior read,
  checked FC06 echo, independent readback and possible-write invalidation. Ordinary
  app API and shared `motion-profile start-speed` use one bounded session; fixed
  origins/desired values survive, while fresh motion evidence is required. No
  motion, persistence, replay, rollback or profile-window expansion.
- Typed seven-word homing-parameter read, ordinary application submit/copy/release
  and shared `home read` with strict host inspection. Preserves unknown codes and
  both raw offset words; known method membership is separate from existing
  execution support. Offset order/sign/scale and physical rates remain unresolved;
  no homing method or write window is enabled.
- Typed non-changing JOG speed/ramp read with raw words, explicit unresolved
  interpretation, admitted configuration generation and copied evidence. Shared
  `velocity read` and ordinary application submit/copy/release use the same owner;
  reads do not stage motion or renew qualification. Native tests cover both
  frontends; the Python host validates live and retained results.

### Changed

- Revalidated the standalone ESS audit against current code and original
  manuals. Corrected stale staging, homing, sign and restart claims. Added
  bounded prompts for remaining stop/recovery, evidence and cleanup work.
- Removed the unused `admitAxisWrite` wrapper. Tests now call the production
  write-policy check; real writers still invalidate evidence at TX acceptance.
  Homing refusal text now describes host policy without claiming undocumented
  drive reactions.
- Unified author, copyright and contact metadata: THYMOS Solution s.r.o., Jan Havelka, info@thymos.cz.
- The default position ceiling is 50,000 command increments/s, down from 200,000
  (`PositionLimits::maximumCommandRate`). Above about 60,000/s, firmware `0x0029` can
  turn a `stop normal` sent late in the acceleration ramp into endless creep. 2,000 rpm
  stays available up to subdivision 1,500; at 51,200 the ceiling is 58 rpm, was 234.
  An explicit caller `PositionLimits` keeps its own value. See the
  [stop normal report](docs/reports/2026-11-04_stop_normal_rate_ceiling.md).
- The standalone moves backwards: `moveby -N`, `moveto` below zero and negative
  advanced targets. It states `negativeTwosComplementVerified` for firmware `0x0029`,
  where the encoding was checked on the bench. The RAM boot zero reads feedback as
  signed, so it survives crossing zero. Negative velocity keeps its own qualification.
- For 1 s after an `enable` is transmitted, `moveby`/`moveto` wait and advanced move,
  velocity and home starts answer `busy` without traffic. Firmware `0x0029` echoed and
  ignored starts sent up to 0.6 s after an enable.
- A home completes on HOMED seen after a witnessed low, with RUNNING clear and position
  exactly 0. IN_POSITION is no longer required: firmware `0x0029` leaves it clear when a
  home follows a stopped move, so a finished home used to end `deadline`.
- `home`, `velocity` and `position-clear` reply `qualification_required` when their
  qualification is absent, in place of `unavailable` or `invalid`.
- Home supervision uses `moveTimeoutMs` (30 s by default), not 3 s, and polls every
  500 ms once a poll follows RUNNING, so a long search stays observed. There is still
  no automatic stop; a home that ends uncertain now says the drive may still be moving.
- Move poll timing is unchanged but set through the new option, not by changing the
  admitted options mid-operation.
- Home qualification no longer requires stored method, rates and ramp to match the
  candidate, because staging overwrites them. Auxiliary 7 and zero offset are still
  required, so a drive at factory defaults can home after `home auxiliary 7`.
- Home admission replies `accepted`, as other operations do. It replied `done`, which
  `bench_probe.py` rejects.
- Human mode recycles a delivered, settled interrupt pair, so repeated `position
  interrupt` no longer fills every result slot. Failed or uncertain results stay.
- `platformio.ini` now defaults to the documented console image `bench_s3_load_timer`,
  so a bare upload no longer flashes the console-less preview.
- An explicit, never-transmitted stop can use one bounded recovery through the
  existing path and be sent once under its admission deadline. Failed recovery
  ends it `not_transmitted`. A transmitted stop whose reply is lost stays `unknown`
  and is never resent automatically. The later
  [stop/evidence audit](docs/reports/2026-11-15_standalone_15_16_audit.md)
  records result-lifetime and evidence corrections.
- `cancel` and `monitor off` leave an in-flight frame to settle, so they do not interlock
  the bus. Probe and capture transactions keep their existing cancellation.
- `stop fast` can escalate a running `stop normal`, including past an undelivered report.
  The firmware keeps two primary stop records and two protected fast-stop escalation
  records. Group stops have their own pool; protocol 3 still advertises
  `outstanding_capacity: 10`.
- Stop admission no longer depends on ordinary result capacity. Delivered failed stop
  evidence is overwritten, oldest first, and counted in `drv.stop_evidence_overwritten`.
- The first stop for the selected axis does not need a prior probe.
- A new `recover` releases a delivered terminal recovery result. An automatic recovery
  has no report and is visible through `drv.automatic_recovery_operation_id` and `result`.
- A second stop arriving while a stop reply is pending is attempted and answered.
- `bench_probe.py` cleanup answers a refused stop with one `recover` and one fresh stop,
  retaining every result in `cleanup_attempts`. Stop commands wait for the firmware's
  longer stop budget.
- Homing input checks are per method, at admission and on every poll. An assigned
  origin is never a fault for 1, 2, 17, 33 and 34; a limit outside the method's
  path still fails; method 35 forbids no sensor. Unused terminals may hold release
  or stop functions, which fail while active, and undriven motion triggers.
- A write reply is classified in one place. Exception 05 is now UNKNOWN rather than
  REJECTED for position, velocity, homing, action and driver writes, so a velocity
  start answered with 05 stops. Supplied execution uncertainty is never ACKNOWLEDGED;
  a position, velocity or homing setup step carrying it does not lead to a start.
- Typed position targets reject a magnitude above `0x0FFFFFFF` before traffic,
  narrowing the signed 32-bit range. The bound interprets a malformed manual row.
- Relative moves reject a supplied established reference that is not idle, stationary
  ACTUAL evidence, as absolute moves did.
- Finite velocity preparation rejects a duration that `maxPolls * pollIntervalUs`
  cannot cover. An observation closing after the duration now ends the budget without
  a failure, unless its alarm or fault words report one.
- Typed read and probe frames require the full request to have been transmitted.
- After a confirmation, a later readback mismatch makes a communication write uncertain.
- `rawPositionWords` in a home result only reports a qualified, confirmed, on-time frame.
- Method 18's reason describes its negative-limit captions as a likely caption error.
  Its support remains unresolved.

### Maintenance

- Archived completed multi-drive instructions. Active prompts retain input-start,
  wired/restart qualification and the blocked broadcast investigation. Updated
  current guides and indexes; dated release records and raw evidence stay intact.
- Reviewed the ESS documents for broadcast and multi-drive start. None is
  documented, and address 0 is only "reserved for the system". Added the
  [multi-drive promptset](docs/prompts/ess_multi_drive/README.md) and
  its backlog items: a broadcast bench test, drive group, free input programming
  and input-triggered start.
- Reconciled prompt status with the November bench results, corrected API comments
  and added a local Doxygen configuration. CI does not run Doxygen.
- Removed both finished ESS software prompt folders and the completed read-stage
  redirect. Useful contracts live in the maintained guides; dated reports retain
  checks and CI evidence. Standalone 03 owns the remaining N05 evidence work.
  Removed duplicate delivery history from the backlog and roadmap.
- Independently audited the delivered ESS software scope and corrected stale
  homing/gap counts, I/O qualification wording and prompt navigation. Broader
  N05 replacement/completion and
  separate source/physical gaps remain active and PARTIAL. No runtime behavior
  or coverage count changes follow from this documentation closeout.
- Completed and independently audited all seven maintainability stages at their
  specified seams. Archived the finished promptset, complexity review and
  implementation handoffs; corrected stale pending-work claims and consolidated
  navigation around current guides, the source map and open qualification work.
  Preserved vendor references and all raw bench evidence.
- Shared one private concrete transport-evidence copier across action, position,
  velocity and homing. Full received length remains separate from the bounded
  capture; envelope checks, parsing, publication, deadlines and outcomes remain
  local. Commissioning policy is unchanged.
- Extracted HOME grammar and bounded formatting into separate private units.
  Console retains command inventory, admission, correlation and output ownership;
  full-width evidence and formatting/backpressure behavior remain covered.
- Extracted application debug orchestration into its own private translation
  unit with concrete console/output-pressure inputs, unchanged storage and
  noncopyable capture ownership. Added linked session tests and S2 consumption;
  other sessions retain their documented cache, child-result and lease boundaries.
- Shared live/retained host payload classification and validation, preserving
  original request expectations, immutable terminals and explicit lifecycle
  ownership. Added family/conflict transcripts and actual console routing parity.
- Extracted canonical host error/wire/check helpers and probe/capture/typed-read/
  state/cache validators into a private pure module, retaining existing imports
  and Console aliases. Corrected file-spec test loaders and added error-identity,
  no-I/O import and wire-precedence coverage; session ownership is unchanged.
- Extracted host action/move/velocity/home checks and their canonical parser/schema
  dependencies into the same pure module, preserving exception messages,
  validation order, exact arithmetic and public script arguments. Settings and
  session validators retain their existing boundaries.
- Extracted five private ESS settings-admission validators, preserving first-error
  precedence, full-candidate checks, in-place publication and write/readback order.
  Added baseline-tested precedence, unchanged-output, alias and I/O-order cases;
  public contexts, group metadata and sequencing remain unchanged.
- Replaced the shared application's six record-context flags with one private
  operation kind and explicit dispatch, preserving concrete storage, lifecycle
  facts, result retention and physical uncertainty. Added failed-preparation
  and cross-kind slot-reuse coverage, including the shared Arduino/IDF fixture.
- Archived the completed ESS delivery sequence, superseded maintenance draft
  and 83 historical reports with their evidence. Current reports retain
  operating-limit evidence and unresolved investigations. Removed the redundant
  development-candidate page and completed backlog entries.
- Updated documentation and operation-ledger links for the archive. Report and
  archive indexes are now checked alongside active and archived prompt links;
  raw JSON/JSONL/ZIP artifacts retain their bytes and hashes.
- Corrected the units preview's obsolete ramp-register message: engineering
  acceleration conversion remains unresolved, while native ramp-time settings
  are implemented.
- Made settings read-count/field policy explicit and shared the exact echo-readback
  eligibility rule between event handling and uncertainty tracking.
- Shared the console's ordinary/reserved-stop slot selection across admission
  and tracking, preserving callback and delivery order.
- Grouped identical native-fixture source declarations; compiler invocations and
  test registrations remain unchanged.
- Audited complexity and delivered seven staged maintainability refactors.
  The older maintenance draft remains archived as superseded without execution.
- Reused private RTU frame/word/CRC helpers and removed a redundant register
  decode buffer while preserving checked-parser output and validation contracts.
- Simplified cached-health help and callback checks, removed an unused example
  include and duplicated console-test setup, and corrected nearby API comments.
- Removed unused SDK-test stubs and write-only counters; consolidated the
  stack-watermark fake without removing active fault injection or assertions.
- Reused host-tool motion-phase and field-map definitions and corrected the
  repeat plan's obsolete deadline label without changing command timing.
- Removed unreachable console-rendering code, corrected the obsolete `?` help
  hint, and resolved C++11/volatile build warnings without changing motor semantics.
- Corrected current API/console bounds, settings and motion evidence, repository
  identity and FieldCore integration guidance. Historical reports and vendor
  references remain unchanged; larger refactors have a separate promptset.
- Separated the maintained bench fixture and latest recorded settings from its
  historical running ledger. Removed obsolete owner/units implementation plans
  and distinguished fixed conversion defaults from later motor readbacks.

## 1.0.0 - First release

All earlier 0.x numbers were development versions, not published releases.
This entry combines that work and the subsequent fixes into one release history.

### Added

- Framework-independent C++11 core with caller-owned buffers and bounded
  operation state. No UART, heap allocation, clocks or automatic retries.
- Exact integer/rational targets, coordinate frames, origins, limits, wrapped
  angles and explicit rounding. Independent position, velocity and acceleration
  units; conversions require the relevant scale and reference information.
- Generated ESS-RS register catalogue and access policy from one reviewed
  ledger. Checked FC03/FC06/FC10 codecs, exceptions, paired values and probes.
- Typed identity, configuration, state, alarm and I/O observations with separate
  validity, configuration context and age.
- Finite position, bounded velocity, action/stop and supported homing sequences.
  Acknowledgement, completion, interruption and unknown execution are distinct.
- Typed driver, optional I/O, stored-record, control and tuning settings.
  Checked readback retains partial updates and invalidates affected references.
- Explicit host serial selection, communication commissioning, persistence
  preparations and bounded non-changing discovery.
- Shared Arduino and native ESP-IDF ESP32-S3 application: one bus owner,
  priority stop, bounded queues/results, passive traffic diagnostics and console.
- Simple `moveby`, `moveto`, `speed`, `accel`, `decel`, `subdivision` and
  `settings` commands alongside the detailed API-oriented console.
- Finite Python feature/load/motion scenarios with strict reply correlation,
  incremental evidence, explicit cleanup and no uncertain motion replay.
- Quick/full verifier, GCC/Clang and firmware CI, isolated public-header builds,
  C++11/C++17 consumers and clean source/install packages.

### Changed

- Package identity is `MotorControl-RS`; C++ namespace, include directory and
  CMake target use `MotorControlRS`.
- Platform startup, USB, tasks, allocation and pins stay outside the core.
  Arduino and native IDF reuse the same application and USB console driver.
- Default position policy combines 2,000 rpm, 200,000 command increments/s and
  100..2,000 ms native ramps. Active subdivision is required; no silent clamp.
- Simple moves use nearest-step rounding and a RAM-only session origin.
  Detailed APIs retain explicit rounding, reference and generation contracts.
- Human console commands recycle delivered successful results. Retained
  failure details do not keep motor ownership after a confirmed stop.

### Fixed

- Exact conversion overflow/cancellation cases and unnecessary gearing or lead
  requirements for conversions that do not cross those coordinate boundaries.
- Delayed observation delivery, stale generations, recovery settlement and
  old queued work incorrectly surviving recovery.
- Partial settings reconciliation, lost origin context and stale prepared work.
- Stop/resume ownership, rejected-move cleanup and successful-result pressure
  that could leave ordinary commands unusable.
- Short moves missed between polls: two fresh exact stopped endpoint readings
  can establish completion under the documented shared mover conditions.
- Stop observation budgeting and strict handling of interleaved serial replies.
- USB output lost-event failure: both firmware consumers use the existing IDF
  USB driver; bounded backpressure remains separate from motor traffic.

### Removed

- Superseded direct admission paths and duplicated application workflows.
- Product-specific board names from reusable interfaces.
- The implication that catalogue coverage, a write echo or a successful build
  establishes full motor functionality or physical qualification.

### Known limits

ESS23-RS20 raw model `0x4EEA`, firmware word `0x0029` is the tested fixture.
Other models, external-input mechanics and all native families are not fully
qualified. Nine named native-family gaps and 18 guarded paired setters remain.
High-speed drive reset-like and persistent-RUNNING behavior remains unresolved
outside the conservative tested envelope. No motor firmware update was applied.
The four-hour run and boundary tests are finite evidence, not a guarantee of
absolute reliability. See the full checkout's `docs/releases/1.0.0.md` and
`docs/ess_api_cli_coverage.md` for scope and evidence.
