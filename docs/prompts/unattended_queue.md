# Unattended queue

This queue drives the hourly Claude Code loop while the owner is away (see the
[unattended plan](README.md#unattended-plan-20-november-to-about-6-december-2026)).
Each row is one queue prompt. The loop works on the number named first under
"Next work" in its handoff, one stage (a-d) per run, and marks a row COMPLETE
only after stage (d): its unattended scope is done, checked, reported and
committed. Steps that need the owner, an instrument or hardware that is not on
the bench stay NOT RUN in the prompt's own report and are listed in the
[wired bench deferred table](ess_wired_bench/README.md#deferred-to-owner-return);
they do not block COMPLETE here. The promptset's own status may stay Partial.

Bench work follows [AGENTS.md](../../AGENTS.md) and the owner-absence rules (D15)
in the [wired bench contract](ess_wired_bench/COMMON.md#owner-absence-20-november-to-about-6-december-2026).
Lichuan work is software only: no Lichuan drive is on the bench, so LC57BK 12a
and 12 and TL57 12 are not in this queue.

A complexity review (rows 3, 20, 34 and 47) has the same four stages: (a)
independent read-only review of the period's code for simplification,
readability and maintainability with unchanged behavior, and a short list of
agreed changes; (b) the changes in small tested blocks; (c) an independent
review of the diff; (d) native tests, both firmware builds, a short bench
regression where firmware changed, and the report.

States: Pending, In progress, COMPLETE, Blocked.

| Queue prompt | Period | Notes | State |
| --- | --- | --- | --- |
| [1 Wired bench 19: remaining multi-drive and discovery work](ess_wired_bench/19_multi_drive_remaining.md) | A | Done 2026-11-24 (44e7f4f, audit fixes 9b34e00, regression on image B); G3 cause open, broadcast cases deferred | COMPLETE |
| [2 Wired bench 20: soak, regression and closeout](ess_wired_bench/20_soak_regression_closeout.md) | A | Done 2026-11-26: K1 8.53 h on image H, review fixes in final image I; owner-deferred cases open | COMPLETE |
| [3 Period A complexity review](README.md#unattended-plan-20-november-to-about-6-december-2026) | A | Code changed by wired bench 01-20 | Pending |
| [4 Standalone 03: position replacement evidence](standalone/03_position_interrupt.md) | B | Includes the N05 remainder of wired bench 09 | Pending |
| [5 Standalone 04: paired-value encodings](standalone/04_paired_values.md) | B | Mostly superseded by wired bench 10 and 13; disposition per item | Pending |
| [6 Standalone 05: homing semantics](standalone/05_homing.md) | B | Hardware moved to wired bench 11 and 12 | Pending |
| [7 Standalone 06: stored records, I/O and tuning](standalone/06_io_and_tuning.md) | B | Hardware moved to wired bench 05-07, 13 and 15 | Pending |
| [8 Standalone 07: reset-like and stop failures](standalone/07_drive_failures.md) | B | Hardware moved to wired bench 16 | Pending |
| [9 Standalone 08: shaft and mechanism measurement](standalone/08_motion_measurement.md) | B | Needs an independent instrument: software and disposition only | Pending |
| [10 Standalone 09: electrical timing and frame shapes](standalone/09_transport_timing.md) | B | Needs an analyzer: software and disposition only | Pending |
| [11 Standalone 10: wire faults and restart durability](standalone/10_faults_and_persistence.md) | B | Restart cases deferred (wired bench 17 run plan) | Pending |
| [12 Standalone 11: USB saturation and overnight work](standalone/11_usb_and_overnight.md) | B | Wired bench 18 covered Q1/S1; overnight runs split into segments | Pending |
| [13 Standalone 12: platforms and bootloader reads](standalone/12_platforms.md) | B | No ESP32-S2 hardware: S2 claims NOT RUN | Pending |
| [14 Standalone 14: delivered-software bench regression](standalone/14_bench_regression.md) | B | | Pending |
| [15 Standalone 17: application cleanup](standalone/17_application_cleanup.md) | B | | Pending |
| [16 Multi-drive 05: input-triggered start](ess_multi_drive/05_input_triggered_start.md) | B | Mostly delivered by wired bench 08 and 09 | Pending |
| [17 Multi-drive 06: shared-bus qualification and closeout](ess_multi_drive/06_two_drive_qualification.md) | B | | Pending |
| [18 Multi-drive 07: ignored broadcast starts and stops](ess_multi_drive/07_broadcast_misses.md) | B | Needs drives powered one at a time: deferred | Pending |
| [19 Standalone 13: coverage and release-scope closeout](standalone/13_closeout.md) | B | After every other standalone row | Pending |
| [20 Period B complexity review](README.md#unattended-plan-20-november-to-about-6-december-2026) | B | | Pending |
| [21 Lichuan shared 01: existing code and tooling](lichuan_shared/01_existing_code_and_tooling.md) | C | | Pending |
| [22 LC57BK 01: source policy and native coverage](lichuan_lc57bk_rs485/01_evidence_and_coverage.md) | C | | Pending |
| [23 LC57BK 02: checked codecs](lichuan_lc57bk_rs485/02_codec.md) | C | | Pending |
| [24 Lichuan shared 02 with LC57BK 02b: profile integration](lichuan_shared/02_profile_integration_contract.md) | C | Also [LC57BK 02b](lichuan_lc57bk_rs485/02b_profile_integration.md) | Pending |
| [25 LC57BK 03: typed reads and discovery](lichuan_lc57bk_rs485/03_reads_and_discovery.md) | C | | Pending |
| [26 LC57BK 04: actions and stop](lichuan_lc57bk_rs485/04_actions_and_stop.md) | C | | Pending |
| [27 LC57BK 05: finite positioning](lichuan_lc57bk_rs485/05_finite_position.md) | C | | Pending |
| [28 LC57BK 06: jog, velocity and pause](lichuan_lc57bk_rs485/06_velocity_and_pause.md) | C | | Pending |
| [29 LC57BK 07: segments](lichuan_lc57bk_rs485/07_segments.md) | C | | Pending |
| [30 LC57BK 08: homing](lichuan_lc57bk_rs485/08_homing.md) | C | | Pending |
| [31 LC57BK 09: I/O and settings](lichuan_lc57bk_rs485/09_io_and_settings.md) | C | | Pending |
| [32 LC57BK 10: communication and persistence](lichuan_lc57bk_rs485/10_commissioning_and_persistence.md) | C | | Pending |
| [33 LC57BK 11: console and integration](lichuan_lc57bk_rs485/11_console_and_integration.md) | C | | Pending |
| [34 Period C complexity review](README.md#unattended-plan-20-november-to-about-6-december-2026) | C | | Pending |
| [35 TL57 01: evidence and implementation policy](lichuan_tl57_rs485/01_evidence_and_coverage.md) | D | | Pending |
| [36 TL57 02: checked codecs](lichuan_tl57_rs485/02_codec.md) | D | | Pending |
| [37 TL57 02b: profile integration](lichuan_tl57_rs485/02b_profile_integration.md) | D | | Pending |
| [38 TL57 03: observations and probes](lichuan_tl57_rs485/03_reads_and_discovery.md) | D | | Pending |
| [39 TL57 04: actions and stop](lichuan_tl57_rs485/04_actions_and_stop.md) | D | | Pending |
| [40 TL57 05: finite position moves](lichuan_tl57_rs485/05_finite_position.md) | D | | Pending |
| [41 TL57 06: JOG and velocity](lichuan_tl57_rs485/06_jog_and_velocity.md) | D | | Pending |
| [42 TL57 07: stored paths](lichuan_tl57_rs485/07_segments.md) | D | | Pending |
| [43 TL57 08: homing and soft limits](lichuan_tl57_rs485/08_homing_and_limits.md) | D | | Pending |
| [44 TL57 09: I/O and settings](lichuan_tl57_rs485/09_io_and_settings.md) | D | | Pending |
| [45 TL57 10: commissioning and persistence](lichuan_tl57_rs485/10_commissioning_and_persistence.md) | D | | Pending |
| [46 TL57 11: console and integration](lichuan_tl57_rs485/11_console_and_integration.md) | D | | Pending |
| [47 Period D complexity review](README.md#unattended-plan-20-november-to-about-6-december-2026) | D | | Pending |
