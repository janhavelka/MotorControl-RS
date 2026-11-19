"""Repeatable named finite scenarios using the ordinary console and shared checks.

Quick is motor read-only. Motion, settings and persistence require explicit
selection. No all-feature sweep, implicit recovery, retry or motor rebinding.
The wired-bench scenarios (input-map, y0-map, baseline-compare,
baseline-capture, restore) write one run folder each (--run-dir), switch bench
outputs only through the shared-output interlock, record their plan before the
port opens and end with both run checks (exit 0 pass, 1 integrity, 2 session).
"""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import random
import re
import time

import bench_motion as motion
import bench_outputs as bench_out
import bench_probe as probe
import bench_records as records
import bench_stop_inputs as bench_stop
import bench_limits as bench_lim
import bench_switch as bench_sw
import bench_pt
import bench_velocity
import bench_input_starts as bench_starts
from bench_session import checked, run_recorded

# Wired-bench scenarios (prompt 04): one run folder each, scenario record first,
# result.json and both run checks at the end.
FOLDER_SCENARIOS = {
    'input-map': 'interlocked 2 s ON of each bench output, read state of every input drive on and off; '
                 'measured output-to-terminal map from status bits and declaration agreement, separate verdicts',
    'y0-map': 'Y0 owner: Y0 = 9 with stated ASSIGN facts, N custom on/off cycles, Y0 restored; '
              'one inverted aux1 edge per custom write; debug raw traffic correlated',
    'baseline-compare': 'fresh read of every committed baseline field on the scenario drives; per-field differences',
    'baseline-capture': 'fresh baseline (01 schema) and the re-baselined candidate, written to the run folder only',
    'restore': 'optional explicit case write (--change), then every differing planned field restored from the '
               'committed baseline through its ordinary setter with stated facts, each read back',
    'status-mapping': 'wired bench 05: per terminal on drives 1, 2, 4 status with polarity 0, the inversion at '
                      'function 0 (mapping unknown) and at PIN 14+t (mapping stated), inverse writes, power-up '
                      'functions; --mapping-cases record: M05/N01 against the built-in record',
    'input-timing': 'wired bench 05: one input-filter value on one drive (CONFIGURE path, read back), interlocked '
                    'mosfet2 pulses with back-to-back read state: delay intervals, width table, optional relay '
                    'against MOSFET; the filter restored from the baseline',
    'outputs': 'wired bench 06: one Y0 case (--case O1..O11) on drive 4: Y0 set through the ordinary I/O route '
               '(load indicator, levels either), the case stimulus with read-state polls, aux1 edges and debug '
               'decoded traffic tabulated per COMMON Timing, restoration from the baseline',
    'input-start': 'wired bench 08: one function 7 start on the selected drive: optional --change X3 = 7 with '
                   'ASSIGN facts, fresh config, driver, profile and state reads, a FINITE_MOVE record with triggers '
                   'inhibited, arm, one owned MOSFET pulse after the firmware isolation reads, observation to its '
                   'conclusion, disarm, restoration of the case write',
    'input-start-single': 'wired bench 09: cases A1..A11 (--cases) on drive 4: function 7 on X0 started by mosfet1 '
                          'through the arm route (or a deliberate unarmed pulse, D13), named states NA-abs, NA-rel, '
                          'NA-X3 entered by ordinary writes and the documented fresh reads; restoration from the baseline',
    'input-start-shared': 'wired bench 09: cases B1..B3 (--cases): X0 = 7 and 0x0050 = 1 on drives 1, 2, 4 as one '
                          'group armed together (absolute-by), one mosfet1 edge, group stop and X3 stop; restoration',
    'position-interrupt': 'wired bench 09: cases I1..I4 (--cases): serial moves of drive 4 interrupted by '
                          '`position interrupt` (0x0027 bit 3), drive-reported endpoints, Y0 2 or 4 edges; restoration',
    'stop-inputs': 'wired bench 07: one case (--case S1..S9, R1, R2a, R2b, R3, G1, G2) and one X3 function '
                   '(--function 4, 5 or 6): X3 assigned on drive 4 (phase A) or 1, 2, 4 (phase B) with ASSIGN facts, '
                   'Y0 2 or 5, interlocked mosfet2 pulses (named running member, pre-start io read), read-state '
                   'samples through the hold, the release and 2 s after; Implement 3 classification; restoration',
    'limits': 'wired bench 11: limit cases (--cases L1..L10) on drive 4 (group/stationary on 1, 2, 4): X1 = 2 '
              '(relay2) and X2 = 3 (relay1) as found, 0x0017 per case through driver set over-limit, Y0 = 2 unless '
              '--polls-only, interlocked relay ONs (named running member, effect limit), read-state samples through '
              'the hold, the release and 2 s after, bits 4-6, alarm and recovery per trial; restoration',
    'switch': 'wired bench 12: switch homing cases (--cases H0..H9) on one drive (--address) with host-driven '
              'switches: origin X0 (mosfet1), positive limit X1 (relay2), negative limit X2 (relay1) emulated from '
              'read-state positions (window mode, bound, preflight, live budget, travel guard), stored auxiliary per '
              'case through home auxiliary, the homing words staged by each home; restoration from the baseline',
    'pt-segments': 'wired bench 13: PT records (--cases P1, P2r, P3, P5, P8, P9, P10, P12; the recorded W0a and P2 '
                   'need images A and C) on one drive (--address) or 1, 2, 4: X0 = 12 (mosfet1), X1-X3 = PIN0-PIN2 '
                   '(relay2, relay1, mosfet2) with read-back, record targets through the promoted setter, PIN selection '
                   'through the interlock with named selection members, arm segment (relative), the firmware trigger, '
                   'path guard and prepared stop; restoration from the baseline',
    'velocity-jog-pv': 'wired bench 14: velocity limits, JOG setter, speed/JOG inputs, PV records and the phase C cells '
                       '(--cases R1..R6, V1, J1..J4, PV1..PV6, C1..C4) on one drive (--address; phase C on drive 2): '
                       'qualification cells per run, dead-man X3 = 5 inverted on a renewed mosfet2 hold, read-state '
                       'sampling, the prepared stop fast on any harness failure; restoration from the baseline',
}

SCENARIOS = {
    'paired-values': 'wired bench 10 (bench_pairs.py): --case P1..P4, P3D, E1, E2, E3P, E3N (experimental images), '
                     'R10 (production regression of the promoted soft-limit setter and home offset), restorations; '
                     'own run folder, scenario record and both run checks',
    'quick': 'ten probes by default; typed baseline and passive health; no motor writes',
    'observations': 'finite explicit health refreshes with per-field freshness checks',
    'load': 'finite read-only probes with explicitly selected host workload and measured execution',
    'configuration': 'typed configuration and optional-I/O reads only',
    'native-read': 'one selected typed native group/record; no writes or segment trigger',
    'settings': 'one explicit input-filter or lock-delay increment/decrement; readback and restore once; '
                '--input-facts configure states CONFIGURE records for connected terminals',
    'position': 'one qualified finite 100-step position phase; same-session stop and profile restoration',
    'actions': 'explicit stopped-state enable/release cycle from enabled state; ends enabled',
    'angle': 'one explicit wrapped-angle request; existing preparation/gates; stop and profile restoration',
    'velocity': 'one explicitly bounded velocity request and stop; native parameter restoration not implemented',
    'stop': 'normal/fast stop during one finite 250-step move; same-session restoration',
    'homing-prerequisites': 'capabilities, wiring, configuration and I/O evidence; no home execution',
    'discovery': 'bounded explicit scan; COMPLETE and host restoration required, no recovery',
    'persistence-plan': 'local plan only; no durability claim or nonvolatile write',
    'persistence': 'one explicit save/factory-restore through existing backup/recommissioning gates',
    **FOLDER_SCENARIOS,
}


def _change(tokens):
    """--change ADDRESS KEY[INDEX] VALUE."""
    address, key, value = tokens
    match = re.fullmatch(r'([a-z0-9.-]+)(?:\[([0-9]{1,2})\])?', key)
    if not address.isdigit() or not 1 <= int(address) <= 247 or match is None or not re.fullmatch(r'[0-9]{1,5}', value):
        raise ValueError('--change needs ADDRESS KEY[INDEX] VALUE with a native integer')
    return {'drive': int(address), 'key': match[1], 'index': int(match[2]) if match[2] else None,
            'candidate': int(value)}


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM13')
    parser.add_argument('--out', type=Path, help='exclusive new summary/JSONL prefix')
    parser.add_argument('--run-dir', type=Path,
                        help='wired-bench scenarios: new run folder build/bench/YYYY-MM-DD_wbNN_slug (created exclusively)')
    parser.add_argument('--case', help='wired-bench scenarios: case ID in the scenario record (default the scenario name)')
    parser.add_argument('--baseline', type=Path, default=records.DEFAULT_BASELINE,
                        help='committed restoration baseline (wired-bench scenarios)')
    parser.add_argument('--image-level', help='wired-bench scenarios: two-digit prompt whose routes the flashed image has')
    parser.add_argument('--commit', help='source revision to record (default: git HEAD plus a working-tree hash)')
    parser.add_argument('--outputs', nargs='+', choices=tuple(records.FIXTURE['outputs']),
                        help='input-map: outputs to switch, in order (default all four)')
    parser.add_argument('--expect-refusal', nargs='+', choices=tuple(records.FIXTURE['outputs']), default=[],
                        help='input-map: outputs whose interlock must refuse (no bench on)')
    parser.add_argument('--hold-ms', type=int, default=2000, help='input-map: bench hold per ON (1..60000)')
    parser.add_argument('--start-terminal', choices=records.TERMINALS, default='x3', help='input-start: armed terminal')
    parser.add_argument('--start-mapping', choices=('unknown', 'before-inversion', 'after-inversion', 'recorded'),
                        default='unknown', help='input-start: status mapping stated in the FINITE_MOVE record')
    parser.add_argument('--arm-form', choices=('relative', 'absolute-by'), default='relative', help='input-start: arm form')
    parser.add_argument('--steps', type=int, default=1000, help='input-start: native steps (displacement or absolute-by)')
    parser.add_argument('--rpm', type=int, default=60, help='input-start: staged speed')
    parser.add_argument('--trigger-output', choices=('mosfet1', 'mosfet2'), default='mosfet2', help='input-start: pulse output')
    parser.add_argument('--width-ms', type=int, default=200, help='input-start: pulse width 20..5000 ms')
    parser.add_argument('--poll-ms', type=int, default=50, help='input-start: sample interval 20..500 ms')
    parser.add_argument('--window-ms', type=int, default=10000, help='input-start: armed window 100..30000 ms')
    parser.add_argument('--max-age-ms', type=int, default=records.MAX_AGE_MS,
                        help='input-map: interlock read age bound, recorded in the scenario record')
    parser.add_argument('--cycles', type=int, default=5, help='y0-map: custom on/off cycles (1..20)')
    parser.add_argument('--phase-s', type=float, default=3.0, help='y0-map: phase length in seconds (0.5..10)')
    parser.add_argument('--repetitions', type=int, help='outputs/stop-inputs: repetitions of the case (default per case)')
    parser.add_argument('--function', type=int, choices=(4, 5, 6), help='stop-inputs: X3 input function of the run')
    parser.add_argument('--deceleration', type=int, choices=(100, 1000), help='stop-inputs S8: configured deceleration (ms)')
    parser.add_argument('--stimulus', choices=('input', 'stop normal', 'stop fast'),
                        help='stop-inputs S8: what stops the move (default the input)')
    parser.add_argument('--sub', choices=('a', 'b', 'c', 'abc'), help='stop-inputs S7: sub-cases (default abc)')
    parser.add_argument('--cases', nargs='+', help='wired bench 09/11/12: case IDs of the scenario family, run in order')
    parser.add_argument('--methods', nargs='+', type=int, help='switch: only these methods of the cases')
    parser.add_argument('--branches', nargs='+', choices=bench_sw.BRANCHES, help='switch: only these starting branches')
    parser.add_argument('--search', type=int, help='switch: native search word H (5..60)')
    parser.add_argument('--return-speed', type=int, help='switch: native return word L (5..30)')
    parser.add_argument('--auxiliary', type=int, help='switch: stored 0x0030 choice for the branch cases (0..11, default 9)')
    parser.add_argument('--choices', nargs='+', type=int, help='switch H8/H9: auxiliary choices (default 0..11)')
    parser.add_argument('--budget-gap-ms', type=float, help='switch: G, worst sample gap of the budget (ms)')
    parser.add_argument('--budget-decision-ms', type=float, help='switch: D, worst decision-to-GPIO time (ms)')
    parser.add_argument('--over-limit', type=int, choices=(0, 1), help='limits: 0x0017 for every case of the run '
                        '(default per case: L4 1, others 0)')
    parser.add_argument('--polls-only', action='store_true',
                        help='limits: no Y0 write (drive-reported polls only; the V-01 run before any I/O write)')
    parser.add_argument('--leave-profile', action='store_true',
                        help='stop-inputs G1/G2: leave the 0x0021-0x0025 words of the members changed (RAM) for a later group '
                             'run of this controller session; a profile stage would revoke the start-speed relation of the members '
                             'for the session')
    parser.add_argument('--change', nargs=3, metavar=('ADDRESS', 'KEY', 'VALUE'),
                        help='input-map/restore: one explicit case write through the ordinary setter, restored at the end')
    parser.add_argument('--assign-facts', nargs='+', action='append', metavar='TERMINAL FACT',
                        help='input-map/restore: ASSIGN facts for one terminal of the change drive: '
                             'xN [triggers-inhibited] [protection-removal] or yN LOAD LEVELS')
    parser.add_argument('--drives', nargs='+', type=int, help='baseline-compare/capture: drives (default 1 2 4)')
    parser.add_argument('--c4-rpm', type=int, help='velocity-jog-pv C4: the highest passing C1 speed when C1 ran in an earlier run')
    parser.add_argument('--c1-rows', nargs='+', help='velocity-jog-pv C1: RAMP:SIGN rows to run (a resumed run), default all four')
    parser.add_argument('--terminals', nargs='+', choices=records.TERMINALS,
                        help='status-mapping: terminals in order (default x0 x1 x2 x3)')
    parser.add_argument('--filter-value', type=int, help='input-timing: input-filter coefficient for the run')
    parser.add_argument('--delay-trials', type=int, default=20, help='input-timing: 200 ms delay trials (1..40)')
    parser.add_argument('--width-trials', type=int, default=10, help='input-timing: trials per width (0..20; 0 skips)')
    parser.add_argument('--widths', nargs='+', type=int, help='input-timing: pulse widths in ms '
                        '(default 1 2 3 5 7 10 12 15 20 30 50)')
    parser.add_argument('--compare-relay', action='store_true',
                        help='input-timing: also relay2 (X1) and mosfet1 (X0) 200 ms trials (filter 2 only)')
    parser.add_argument('--poll-depth', type=int, default=2, help='input-timing: outstanding read state requests (1..3)')
    parser.add_argument('--phase-seed', type=int, default=5,
                        help='input-timing: seed of the per-trial random pause after the first sample')
    parser.add_argument('--traffic', choices=('raw', 'off'), default='raw',
                        help='input-timing: debug mode during the run (raw: traffic records; off: console bandwidth '
                             'for the polls, the step times of every read stay in its reply)')
    parser.add_argument('--mapping-cases', choices=('map', 'record'),
                        help='status-mapping: map (M01-M04, default) or record (M05, N01 on the record image)')
    parser.add_argument('--reason', help='baseline-capture: reason for the re-baselined candidate')
    parser.add_argument('--prompt', help='baseline-capture: two-digit prompt that intends the re-baselining')
    parser.add_argument('--scenario', choices=SCENARIOS, default='quick')
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--plan-only', action='store_true')
    parser.add_argument('--address', type=int, default=1)
    parser.add_argument('--declare-unconnected', action='store_true',
                        help='settings/position/actions/stop: state that no terminal of the selected drive is wired; '
                             'refused when the board table declares a terminal of that drive connected or is invalid')
    parser.add_argument('--count', type=int, help='quick/load/observations only; default ten')
    parser.add_argument('--interval', type=float, help='quick/load/observations only; default 0.05s')
    parser.add_argument('--timeout', type=float, default=5)
    parser.add_argument('--debug', choices=('off', 'raw', 'decoded'))
    parser.add_argument('--firmware', type=Path, help='image provenance; no upload or flash attestation')
    parser.add_argument('--load', nargs=3, type=int, metavar=('WORK_US', 'OWNER_DELAY_US', 'CONSOLE_BYTES'))
    parser.add_argument('--phase', choices=('forward', 'absolute', 'return'))
    parser.add_argument('--stop-policy', choices=('normal', 'fast'))
    parser.add_argument('--arguments', nargs='+', help='exact public angle/velocity grammar tokens')
    parser.add_argument('--native', choices=('driver', 'io', 'control', 'segment', 'tuning'))
    parser.add_argument('--native-args', nargs='+')
    parser.add_argument('--setting', choices=('input-filter', 'lock-delay'))
    parser.add_argument('--input-facts', choices=('configure', 'segment'),
                        help='settings: before each write, state four CONFIGURE input records from fresh reads '
                             '(function and polarity from read config, no added facts); required for input-filter '
                             'when the board declares a terminal of the drive connected. input-map/restore: the '
                             'CONFIGURE or SEGMENT records the change and its restoration state')
    parser.add_argument('--value', type=int)
    parser.add_argument('--discovery', nargs='*', default=None)
    parser.add_argument('--persistence-kind', choices=('save', 'factory-restore'))
    parser.add_argument('--evidence-records', type=int, default=20000)
    parser.add_argument('--evidence-mib', type=int, default=16)
    args = parser.parse_args(argv)
    if args.list:
        return args
    repeatable = args.scenario in ('quick', 'load', 'observations')
    if not repeatable and (args.count is not None or args.interval is not None):
        parser.error('--count/--interval apply only to quick, load or observations')
    if args.count is None:
        args.count = 10 if repeatable else 1
    if args.interval is None:
        args.interval = .05 if repeatable else 0
    if not 1 <= args.address <= 247 or not 1 <= args.count <= 100:
        parser.error('address1..247 and finite count1..100 required')
    if not 0 <= args.interval <= 1 or not 0 < args.timeout <= 60:
        parser.error('finite interval0..1s and timeout0..60s required')
    if not 2 <= args.evidence_records <= 200000 or not 1 <= args.evidence_mib <= 64:
        parser.error('finite evidence record/byte limits required')
    if args.load is not None:
        if args.scenario != 'load': parser.error('--load requires the load scenario')
        probe.check_load(tuple(args.load))
    elif args.scenario == 'load': parser.error('load requires all three explicit workload settings')
    for option, scopes in ((args.phase, ('position',)), (args.arguments, ('angle', 'velocity')),
                          (args.native, ('native-read',)), (args.native_args, ('native-read',)),
                          (args.setting, ('settings',)), (args.value, ('settings',)),
                          (args.input_facts, ('settings', 'restore', 'input-map')),
                          (args.discovery, ('discovery',)),
                          (args.persistence_kind, ('persistence', 'persistence-plan')),
                          (args.run_dir, tuple(FOLDER_SCENARIOS)), (args.case, tuple(FOLDER_SCENARIOS)),
                          (args.image_level, tuple(FOLDER_SCENARIOS)),
                          (args.outputs, ('input-map',)), (args.expect_refusal or None, ('input-map',)),
                          (args.change, ('input-map', 'restore', 'input-start')), (args.assign_facts, ('input-map', 'restore', 'input-start')),
                          (args.drives, ('baseline-compare', 'baseline-capture', 'pt-segments')),
                          (args.c4_rpm, ('velocity-jog-pv',)),
                          (args.c1_rows, ('velocity-jog-pv',)),
                          (args.terminals, ('status-mapping',)), (args.mapping_cases, ('status-mapping',)),
                          (args.filter_value, ('input-timing',)), (args.widths, ('input-timing', 'stop-inputs')),
                          (args.function, ('stop-inputs',)), (args.deceleration, ('stop-inputs',)),
                          (args.stimulus, ('stop-inputs',)), (args.sub, ('stop-inputs',)),
                          (args.leave_profile or None, ('stop-inputs', 'limits', *bench_starts.FAMILIES)),
                          (args.cases, (*bench_starts.FAMILIES, 'limits', 'switch', 'pt-segments', 'velocity-jog-pv')),
                          (args.methods, ('switch',)), (args.branches, ('switch',)), (args.search, ('switch',)),
                          (args.return_speed, ('switch',)), (args.auxiliary, ('switch',)), (args.choices, ('switch',)),
                          (args.budget_gap_ms, ('switch',)), (args.budget_decision_ms, ('switch',)),
                          (args.over_limit, ('limits',)), (args.polls_only or None, ('limits',)),
                          (args.compare_relay or None, ('input-timing',)),
                          (None if args.traffic == 'raw' else args.traffic, ('input-timing',)),
                          (args.reason, ('baseline-capture',)), (args.prompt, ('baseline-capture',)),
                          (args.repetitions, ('outputs', 'stop-inputs', 'limits', 'switch', *bench_starts.FAMILIES))):
        if option is not None and args.scenario not in scopes:
            parser.error('scenario-specific arguments cannot enter another scenario')
    if args.scenario == 'settings' and args.input_facts == 'segment':
        parser.error('the settings scenario states configure records only')
    if args.scenario in FOLDER_SCENARIOS:
        _folder_arguments(parser, args)
        return args
    if args.stop_policy is not None and args.scenario not in ('angle', 'velocity', 'stop'):
        parser.error('--stop-policy requires angle, velocity or stop')
    if args.scenario == 'angle' and args.address != 1:
        parser.error('the angle scenario does not select a drive; it runs on node 1 only')
    if args.scenario in ('angle', 'velocity'):
        if not args.arguments: parser.error('explicit public motion arguments required')
        tokens = tuple(args.arguments)
        parsed = probe.move_arguments('angle', tokens) if args.scenario == 'angle' else probe.velocity_arguments(tokens)
        if args.scenario == 'velocity' and args.timeout * 1000000 <= parsed['duration_us']:
            parser.error('velocity timeout must include finite duration and explicit stop time')
    if args.scenario == 'native-read':
        if args.native is None: parser.error('one explicit native group required')
        tokens = tuple(args.native_args or ('read',))
        candidate = probe.tuning_arguments(tokens)[1] if args.native == 'tuning' else \
            probe.segment_arguments(tokens)[2] if args.native == 'segment' else \
            probe.driver_arguments(tokens, 'control_settings' if args.native == 'control' else 'io' if args.native == 'io' else 'drive')
        if candidate: parser.error('native-read never writes; select a bounded settings experiment explicitly')
    if args.scenario == 'settings' and (args.setting is None or args.value is None or not 0 <= args.value <= 65535):
        parser.error('settings requires a documented selected field and native uint16 candidate')
    if args.input_facts and args.declare_unconnected:
        parser.error('--input-facts states facts for connected terminals; --declare-unconnected says none are wired')
    if args.input_facts and args.setting == 'lock-delay':
        parser.error('lock delay has no input condition; --input-facts applies to input-filter')
    if args.scenario == 'discovery':
        request = probe.discovery_arguments(tuple(args.discovery or ()))
        if request['action'] != 'begin': parser.error('discovery scenario requires a finite scan, not a local control')
    if args.scenario in ('persistence', 'persistence-plan') and args.persistence_kind is None:
        parser.error('explicit save/factory-restore kind required')
    if not args.plan_only and args.out is None:
        parser.error('--out is required for exclusive evidence before port access')
    return args


def _same_configuration(before, after):
    if before['config']['raw'] != after['config']['raw']:
        raise probe.BenchError('observed drive configuration changed; no implicit repair')


def _input_facts(console, args, record, key):
    """State the CONFIGURE records the input-filter experiment needs on connected terminals."""
    if args.input_facts:
        record['setting'][key] = probe.install_input_facts(console, args.address, args.input_facts, timeout_s=args.timeout)


def _setting(console, args, record):
    group = 'tuning' if args.setting == 'input-filter' else 'control'
    # Settings writes apply to the selected drive only.
    record['selection'] = motion.select_drive(console, args.address, declare_unconnected=args.declare_unconnected)
    if group == 'tuning' and not args.input_facts and not args.declare_unconnected:
        # Refuse before any write: the board's connected terminals need stated facts.
        wiring = console.command('wiring', timeout_s=args.timeout)
        if not wiring['ok']:
            raise probe.BenchError('wiring query refused before the settings scenario')
        record['selection']['board_wiring'] = wiring
        if probe.board_declares_connected(wiring) or wiring.get('fixture') == 'invalid':
            raise probe.BenchError('the board declares connected terminals on this drive; input-filter needs --input-facts configure')
    prefix = ('filters',) if group == 'tuning' else ()
    fields = probe.TUNING_FIELDS['filters'] if group == 'tuning' else probe.CONTROL_FIELDS
    before = checked(console, group, driver_args=prefix + ('read',), address=args.address, timeout_s=args.timeout)
    if group == 'tuning':
        # Observe changed settings before rebuilding dependent I/O evidence.
        record['setting_io'] = checked(console, 'io', driver_args=('read',),
                                       address=args.address, timeout_s=args.timeout)
    original = before['observation']['raw'][fields.index(args.setting)]
    if abs(args.value - original) != 1:
        raise probe.BenchError('this finite settings experiment permits one native-unit change only; no-op does not qualify work')
    requested = prefix + ('set', args.setting, str(args.value))
    restore = prefix + ('set', args.setting, str(original))
    record['setting'] = dict(before=before, requested=requested, restoration=restore, restored=False,
                             input_facts=args.input_facts)
    console.emit('settings_plan', **record['setting'])
    probe.observe_stopped(console, address=args.address, timeout_s=args.timeout)
    if group == 'tuning':
        _input_facts(console, args, record, 'facts')
    applied = False
    try:
        result = checked(console, group, driver_args=requested, address=args.address, timeout_s=args.timeout)
        applied = True  # Checked terminal includes exact stored readback, not just echo.
        record['setting']['applied'] = result
    finally:
        # Only a known applied update can enter its preplanned distinct restore.
        # Failed/uncertain update retains backup and interlock; no write replay.
        if applied and console.synchronized:
            # Effects invalidate preparation caches. Re-establish the same
            # checked prerequisites before the distinct restoration request.
            record['setting']['restore_baseline'] = checked(console, group, driver_args=prefix + ('read',), address=args.address, timeout_s=args.timeout)
            record['setting']['restore_identity'] = checked(console, 'read-identity', address=args.address, timeout_s=args.timeout)
            record['setting']['restore_configuration'] = checked(console, 'read-config', address=args.address, timeout_s=args.timeout)
            if group == 'tuning':
                record['setting']['restore_io'] = checked(console, 'io', driver_args=('read',), address=args.address, timeout_s=args.timeout)
            probe.observe_stopped(console, address=args.address, timeout_s=args.timeout)
            if group == 'tuning':
                _input_facts(console, args, record, 'restore_facts')  # The write cleared the earlier records.
            record['setting']['restore_result'] = checked(console, group, driver_args=restore, address=args.address, timeout_s=args.timeout)
            after = checked(console, group, driver_args=prefix + ('read',), address=args.address, timeout_s=args.timeout)
            record['setting']['after'] = after
            if before['observation']['raw'] != after['observation']['raw']:
                raise probe.BenchError('exact native settings restoration disagrees')
            record['setting']['restored'] = True


def _angle(console, args, record):
    record['profile_before'] = motion.profile_snapshot(console, timeout_s=args.timeout)
    failure = None
    try:
        probe.move_campaign(console, command='move-angle', move_args=tuple(args.arguments),
                            cleanup_stop=args.stop_policy or 'normal', timeout_s=args.timeout, address=args.address)
    except BaseException as error:
        failure = error
        raise
    finally:
        summaries = record.get('campaigns', [])
        settled = next((s for s in reversed(summaries) if s.get('mode') == 'move-angle'), {})
        if console.synchronized and settled.get('cleanup') == 'drive_reported_standstill':
            try:
                record['profile_restoration'] = motion.restore_profile(console, timeout_s=args.timeout)
            except BaseException as error:
                record['profile_restoration'] = {'known': False, 'error': str(error) or 'interrupted'}
                if failure is None:
                    raise
        else:
            record['profile_restoration'] = {'known': False, 'reason': 'no qualified standstill or no admitted move; no restore write'}


def run_scenario(console, args, record):
    name = args.scenario
    if name not in SCENARIOS:
        raise ValueError('unknown finite scenario: ' + str(name))
    if name == 'quick':
        if any(record['before']['load'][key] for key in probe.LOAD_FIELDS):
            raise probe.BenchError('quick regression expects unloaded host; select load explicitly')
        probe.campaign(console, 'stress', count=args.count,
                       interval_s=args.interval, timeout_s=args.timeout, address=args.address,
                       load=None)
    elif name == 'load':
        original = tuple(record['before']['load'][key] for key in probe.LOAD_FIELDS)
        failure = None
        try:
            probe.campaign(console, 'load', count=args.count, interval_s=args.interval,
                           timeout_s=args.timeout, address=args.address, load=tuple(args.load))
        except BaseException as error:
            failure = error
            raise
        finally:
            record['load_cleanup'] = {'original': original, 'restored': False}
            if console.synchronized:
                try:
                    # Read the measured window before resetting it on restoration.
                    record['load_window'] = checked(console, 'load', timeout_s=args.timeout)
                    record['load_cleanup']['result'] = checked(console, 'load', load=original, timeout_s=args.timeout)
                    record['load_cleanup']['restored'] = True
                except BaseException as error:
                    record['load_cleanup']['error'] = str(error) or 'interrupted'
                    if failure is None: raise
    elif name == 'observations':
        probe.state_health_campaign(console, count=args.count, interval_s=args.interval,
                                    timeout_s=args.timeout, address=args.address)
    elif name == 'configuration':
        probe.typed_read_campaign(console, kind='config', timeout_s=args.timeout, address=args.address)
        probe.driver_read_campaign(console, command='io', driver_args=('read',), timeout_s=args.timeout, address=args.address)
    elif name == 'native-read':
        probe.driver_read_campaign(console, command=args.native, driver_args=tuple(args.native_args or ('read',)),
                                   timeout_s=args.timeout, address=args.address)
    elif name == 'settings': _setting(console, args, record)
    elif name in ('position', 'actions', 'stop'):
        record['motion'] = {}
        phase = (args.phase or 'forward') if name == 'position' else 'actions' if name == 'actions' else 'stop-' + (args.stop_policy or 'normal')
        motion.run_phase(console, phase, record['motion'], args.address, args.declare_unconnected)
    elif name == 'angle': _angle(console, args, record)
    elif name == 'velocity':
        record['native_parameter_restoration'] = {'known': False, 'owning_gap': '11', 'reason': 'standalone native velocity parameter reads unavailable'}
        probe.velocity_campaign(console, velocity_args=tuple(args.arguments), cleanup_stop=args.stop_policy or 'normal',
                                timeout_s=args.timeout, address=args.address)
    elif name == 'homing-prerequisites':
        record['homing'] = dict(capabilities=checked(console, 'caps', timeout_s=args.timeout),
                               wiring=checked(console, 'wiring', timeout_s=args.timeout),
                               inputs=checked(console, 'io', driver_args=('read',), address=args.address, timeout_s=args.timeout),
                               execution_attempted=False, readiness='method-specific admission still required',
                               native_parameter_restoration='standalone reads remain owning gap14')
    elif name == 'discovery':
        record['scan'] = probe.discovery_campaign(console, tokens=tuple(args.discovery or ()), timeout_s=args.timeout)
    elif name in ('persistence', 'persistence-plan'):
        record['persistence'] = probe.persistence_campaign(console, kind=args.persistence_kind, timeout_s=args.timeout,
                                execute=name == 'persistence', verify=name == 'persistence', finish=name == 'persistence')
        record['durability_verified'] = False  # No physical restart supplied by this tool.
    # Settings baseline is refreshed only after successful scenario cleanup.
    if name != 'discovery':
        after = checked(console, 'read-config', address=args.address, timeout_s=args.timeout)
        if name == 'persistence' and args.persistence_kind == 'factory-restore':
            record['configuration_after_action'] = after
        else:
            _same_configuration(record['before']['config'], after)
    record['workload_verified'] = True


def _folder_arguments(parser, args):
    """Wired-bench scenarios: a run folder, an image and explicit, bounded inputs."""
    if args.out is not None:
        parser.error('wired-bench scenarios write a run folder: use --run-dir, not --out')
    if not args.plan_only and args.run_dir is None:
        parser.error('--run-dir is required for exclusive evidence before port access')
    if args.run_dir is not None:
        try:
            records.RunFolder.from_path(args.run_dir)
        except ValueError as error:
            parser.error(str(error))
    if args.firmware is None:
        parser.error('--firmware IMAGE is required: the scenario record names the image and its SHA-256')
    if args.declare_unconnected or args.debug is not None:
        parser.error('wired-bench scenarios use the board declarations and choose their own debug mode')
    if args.image_level is not None and not re.fullmatch(r'[0-9]{2}', args.image_level):
        parser.error('--image-level needs a two-digit prompt')
    if args.scenario in ('restore', 'y0-map', 'status-mapping', 'input-timing', 'outputs', 'stop-inputs') or \
            (args.scenario == 'input-map' and args.change):
        if args.image_level is None:
            parser.error('a scenario that writes needs --image-level (the prompt whose routes the image has)')
    if args.case is not None and not re.fullmatch(r'[A-Za-z0-9_-]{1,24}', args.case):
        parser.error('--case is a short ID such as H01')
    if args.change is not None:
        try:
            args.change = _change(args.change)
        except ValueError as error:
            parser.error(str(error))
        if args.scenario == 'restore' and args.change['drive'] != args.address:
            parser.error('restore --change must name the --address drive')
    args.assign_facts = _assign_facts(parser, args.assign_facts or [])
    if (args.input_facts or args.assign_facts) and args.change is None:
        parser.error('facts are stated for an explicit --change only')
    if args.scenario == 'input-map':
        if not 1 <= args.hold_ms <= probe.BENCH_MAX_HOLD_MS or not 1 <= args.max_age_ms <= 5000:
            parser.error('input-map needs a hold of 1..60000 ms and a read age bound of 1..5000 ms')
        outputs = args.outputs or list(records.FIXTURE['outputs'])
        if len(set(outputs)) != len(outputs) or any(o not in outputs for o in args.expect_refusal):
            parser.error('input-map outputs are distinct and include every expected refusal')
        args.outputs = outputs
    if args.scenario == 'input-start':
        if (not probe.INPUT_START_WIDTH_MS[0] <= args.width_ms <= probe.INPUT_START_WIDTH_MS[1] or
                not probe.INPUT_START_POLL_MS[0] <= args.poll_ms <= probe.INPUT_START_POLL_MS[1] or
                not probe.INPUT_START_WINDOW_MS[0] <= args.window_ms <= probe.INPUT_START_WINDOW_MS[1] or
                not 1 <= args.rpm <= 3000 or not 1 <= abs(args.steps) <= 0x0FFFFFFF):
            parser.error('input-start bounds: width 20..5000 ms, poll 20..500 ms, window 100..30000 ms, rpm 1..3000')
        if records.FIXTURE['outputs'][args.trigger_output] != args.start_terminal:
            parser.error('the trigger output does not drive the armed terminal (FIXTURE outputs)')
        if args.change is not None and (args.change['drive'] != args.address or args.change['key'] != 'io.' + args.start_terminal):
            parser.error('input-start --change assigns the armed terminal of the --address drive')
        if args.change is not None and args.image_level is None:
            parser.error('a scenario that writes needs --image-level (the prompt whose routes the image has)')
    if args.scenario == 'y0-map':
        if args.address != records.FIXTURE['y0_owner']:
            parser.error('y0-map runs on the Y0 owner, drive %d' % records.FIXTURE['y0_owner'])
        if not 1 <= args.cycles <= 20 or not .5 <= args.phase_s <= 10:
            parser.error('y0-map needs 1..20 cycles and a 0.5..10 s phase')
    if args.drives is not None and (len(set(args.drives)) != len(args.drives) or
                                    any(a not in records.FIXTURE['input_drives'] for a in args.drives)):
        parser.error('--drives names distinct fixture drives 1, 2, 4')
    if args.scenario == 'baseline-capture':
        if not args.reason or not args.reason.strip() or not args.prompt or not re.fullmatch(r'[0-9]{2}', args.prompt):
            parser.error('baseline-capture needs --reason TEXT and a two-digit --prompt')
    if args.scenario == 'status-mapping':
        if args.image_level is None or int(args.image_level) < 5:
            parser.error('status-mapping needs --image-level 05 or later (the polarity route of prompt 05)')
        if args.terminals is not None and len(set(args.terminals)) != len(args.terminals):
            parser.error('--terminals names distinct terminals')
        args.mapping_cases = args.mapping_cases or 'map'
    if args.scenario == 'outputs':
        if args.case not in bench_out.CASES:
            parser.error('outputs needs --case O1..O11 (prompt 06 case table)')
        if bench_out.CASES[args.case].get('removed'):
            parser.error('%s is a D9 experiment case: it ran on the experiment image of wired bench 06; the '
                         'experiment admission was removed with the ledger record (Implement 3)' % args.case)
        if args.address != records.FIXTURE['y0_owner']:
            parser.error('outputs runs on the Y0 owner, drive %d (--address)' % records.FIXTURE['y0_owner'])
        if args.image_level is None or int(args.image_level) < 6:
            parser.error('outputs needs --image-level 06 or later (restore routes, either-level polarity, D9)')
        if args.repetitions is None:
            args.repetitions = bench_out.CASES[args.case]['repetitions']
        if not 0 <= args.repetitions <= 20:
            parser.error('outputs needs 0..20 repetitions')
    if args.scenario == 'stop-inputs':
        case = bench_stop.CASES.get(args.case)
        if case is None:
            parser.error('stop-inputs needs --case %s (prompt 07 case table)' % ', '.join(bench_stop.CASES))
        if args.function not in case['functions']:
            parser.error('%s runs X3 function %s (--function)' % (args.case, ' or '.join(map(str, case['functions']))))
        if args.image_level is None or int(args.image_level) < 7:
            parser.error('stop-inputs needs --image-level 07 or later (the stop/release rule and observer witnesses)')
        if args.repetitions is None:
            args.repetitions = case['runs']
        if not 1 <= args.repetitions <= 20:
            parser.error('stop-inputs needs 1..20 repetitions')
        if args.case == 'S8' and (args.deceleration is None or args.stimulus is None):
            parser.error('S8 needs --deceleration 100|1000 and --stimulus input|stop normal|stop fast')
        if args.case != 'S8' and (args.deceleration is not None or args.stimulus is not None):
            parser.error('--deceleration and --stimulus belong to S8')
        if args.sub is not None and args.case != 'S7':
            parser.error('--sub belongs to S7')
        if args.leave_profile and case['kind'] != 'group':
            parser.error('--leave-profile belongs to the group cases')
        if args.widths is not None and (args.case != 'S9' or any(not 1 <= w <= 200 for w in args.widths)):
            parser.error('--widths belongs to S9 (1..200 ms)')
    if args.scenario == 'limits':
        if not args.cases or any(c not in bench_lim.CASES for c in args.cases) or len(set(args.cases)) != len(args.cases):
            parser.error('limits needs --cases from %s' % ', '.join(bench_lim.CASES))
        if args.image_level is None or int(args.image_level) < 10:
            parser.error('limits needs --image-level 10 or later (prompt 07 rule, prompt 10 limit pairs)')
        if args.case is not None:
            parser.error('limits names its cases with --cases')
        if args.address != records.FIXTURE['y0_owner']:
            parser.error('limits runs from drive %d (--address)' % records.FIXTURE['y0_owner'])
        if args.repetitions is not None and not 1 <= args.repetitions <= 10:
            parser.error('limits --repetitions overrides the case runs within 1..10')
    if args.scenario == 'switch':
        if not args.cases or any(c not in bench_sw.CASES for c in args.cases) or len(set(args.cases)) != len(args.cases):
            parser.error('switch needs --cases from %s' % ', '.join(bench_sw.CASES))
        if args.image_level is None or int(args.image_level) < 12:
            parser.error('switch needs --image-level 12 (auxiliary choices with switch methods)')
        if args.case is not None:
            parser.error('switch names its cases with --cases')
        if args.address not in records.FIXTURE['input_drives']:
            parser.error('switch runs on drive 1, 2 or 4 (--address)')
        if args.search is None or args.return_speed is None or not 5 <= args.search <= 60 or not 5 <= args.return_speed <= 30:
            parser.error('switch needs --search 5..60 and --return-speed 5..30 (prompt 12 bounds H 60, L 30)')
        if args.budget_gap_ms is None or args.budget_decision_ms is None or args.budget_gap_ms <= 0 or args.budget_decision_ms <= 0:
            parser.error('switch needs the budget --budget-gap-ms G and --budget-decision-ms D')
        if args.auxiliary is None:
            args.auxiliary = 9
        if not 0 <= args.auxiliary <= 11 or any(not 0 <= c <= 11 for c in (args.choices or ())):
            parser.error('switch auxiliary choices are 0..11')
        if args.repetitions is not None and not 1 <= args.repetitions <= 3:
            parser.error('switch --repetitions within 1..3')
        if {'H12', 'H13'} & set(args.cases) and args.address != records.FIXTURE['y0_owner']:
            parser.error('switch H12 and H13 run on drive %d' % records.FIXTURE['y0_owner'])
    if args.scenario == 'pt-segments':
        if not args.cases or any(c not in bench_pt.CASES for c in args.cases) or len(set(args.cases)) != len(args.cases):
            parser.error('pt-segments needs --cases from %s' % ', '.join(bench_pt.CASES))
        if args.image_level is None or int(args.image_level) < 12:
            parser.error('pt-segments needs --image-level 12 (W0 experimental image) or 13 (promoted targets)')
        if args.case is not None:
            parser.error('pt-segments names its cases with --cases')
        if args.address not in records.FIXTURE['input_drives'] or any(a not in records.FIXTURE['input_drives'] for a in args.drives or ()):
            parser.error('pt-segments runs on drives 1, 2 and 4')
        if set(args.cases) - {'W0a', 'W0b'} and int(args.image_level) < 13:
            parser.error('the D10-path cases need the promoted image (--image-level 13)')
        if 'W0a' in args.cases and int(args.image_level) >= 13:
            parser.error('W0a runs on the experimental image (--image-level 12)')
    if args.scenario == 'velocity-jog-pv':
        if not args.cases or any(c not in bench_velocity.CASES for c in args.cases) or len(set(args.cases)) != len(args.cases):
            parser.error('velocity-jog-pv needs --cases from %s' % ', '.join(bench_velocity.CASES))
        if args.image_level is None or int(args.image_level) < 14:
            parser.error('velocity-jog-pv needs --image-level 14 (the velocity limits route)')
        if args.case is not None:
            parser.error('velocity-jog-pv names its cases with --cases')
        if args.address not in records.FIXTURE['input_drives']:
            parser.error('velocity-jog-pv runs on drive 1, 2 or 4')
        if {c for c in args.cases if c.startswith('C')} and args.address != 2:
            parser.error('phase C runs on drive 2 (D15 safeguard for prompt 14)')
    if args.scenario in bench_starts.FAMILIES:
        family = bench_starts.FAMILIES[args.scenario]
        allowed = [c for c, case in bench_starts.CASES.items() if case['family'] == family]
        if not args.cases or any(c not in allowed for c in args.cases) or len(set(args.cases)) != len(args.cases):
            parser.error('%s needs --cases from %s' % (args.scenario, ', '.join(allowed)))
        if args.image_level is None or int(args.image_level) < 8:
            parser.error('%s needs --image-level 08 or later (the input-start route)' % args.scenario)
        if args.case is not None:
            parser.error('%s names its cases with --cases' % args.scenario)
        if args.repetitions is not None and not 1 <= args.repetitions <= 50:
            parser.error('%s --repetitions overrides the case runs within 1..50' % args.scenario)
    if args.scenario == 'input-timing':
        args.widths = list(args.widths or TIMING_WIDTHS_MS)
        if args.image_level is None or int(args.image_level) < 5:
            parser.error('input-timing needs --image-level 05 or later (the widened filter experiment)')
        if args.filter_value not in TIMING_CANDIDATES:
            parser.error('--filter-value is one of the prompt 05 candidate set %s' % (TIMING_CANDIDATES,))
        if args.address not in records.FIXTURE['input_drives']:
            parser.error('input-timing runs on drive 1, 2 or 4')
        if not 1 <= args.delay_trials <= 40 or not 0 <= args.width_trials <= 20 or not 1 <= args.poll_depth <= 3:
            parser.error('input-timing needs 1..40 delay trials, 0..20 width trials and a poll depth of 1..3')
        if len(set(args.widths)) != len(args.widths) or any(not 1 <= w <= 2000 for w in args.widths):
            parser.error('--widths are distinct, 1..2000 ms')
        if args.compare_relay and args.filter_value != 2:
            parser.error('--compare-relay runs at filter 2 only')


def _assign_facts(parser, groups):
    out = {}
    for group in groups:
        terminal, facts = group[0], group[1:]
        if terminal in records.TERMINALS:
            if len(set(facts)) != len(facts) or any(f not in ('triggers-inhibited', 'protection-removal') for f in facts):
                parser.error('input ASSIGN facts are triggers-inhibited and protection-removal')
        elif terminal in ('y0', 'y1'):
            if len(facts) != 2 or facts[0] not in ('none', 'indicator', 'control') or \
                    facts[1] not in ('inactive', 'active', 'either'):
                parser.error('output ASSIGN facts are LOAD (none, indicator, control) and LEVELS (inactive, active, either)')
        else:
            parser.error('--assign-facts names x0..x3 or y0..y1')
        if terminal in out:
            parser.error('one --assign-facts per terminal')
        out[terminal] = list(facts)
    return out


def _facts(args):
    """The facts the scenario record states, per drive and purpose."""
    if args.scenario == 'status-mapping':
        if args.mapping_cases == 'record':
            return {'1': {'assign': {'x3': ['triggers-inhibited']}, 'configure': {}}}
        return {str(a): {'assign': {t: list(f) for t, f in MAPPING_FACTS.items()}}
                for a in records.FIXTURE['input_drives']}
    if args.scenario == 'input-timing':
        return {str(args.address): {'configure': {}}}
    if args.scenario == 'y0-map':
        return {str(records.FIXTURE['y0_owner']): {'assign': {'y0': ['indicator', 'either']}}}
    if args.scenario == 'outputs':
        return {str(records.FIXTURE['y0_owner']): {'assign': {'y0': ['indicator', 'either']}, 'configure': {}}}
    if args.scenario in bench_starts.FAMILIES:
        return bench_starts.facts()
    if args.scenario == 'pt-segments':
        return bench_pt.pt_facts()
    if args.scenario == 'velocity-jog-pv':
        return bench_velocity.facts()
    if args.scenario == 'switch':
        out = {str(args.address): {'configure': {}}}
        if args.address == records.FIXTURE['y0_owner']:
            out[str(args.address)]['assign'] = {'y0': list(bench_lim.Y0_FACTS)}
        return out
    if args.scenario == 'limits':
        out = {str(a): {'configure': {}} for a in records.FIXTURE['input_drives']}
        out[str(records.FIXTURE['y0_owner'])]['assign'] = {'y0': list(bench_lim.Y0_FACTS)}
        return out
    if args.scenario == 'stop-inputs':
        out = {}
        for address in bench_stop.phase_drives(bench_stop.CASES[args.case]):
            out[str(address)] = {'assign': {'x3': list(bench_stop.X3_FACTS)}, 'configure': {}}
        out.setdefault(str(records.FIXTURE['y0_owner']), {'assign': {}, 'configure': {}})
        out[str(records.FIXTURE['y0_owner'])]['assign']['y0'] = list(bench_stop.Y0_FACTS)
        return out
    if args.change is None:
        return {}
    stated = {}
    if args.input_facts:
        stated[args.input_facts] = {}
    if args.assign_facts:
        stated['assign'] = dict(args.assign_facts)
    return {str(args.change['drive']): stated}


PREPARED_STOP = ['stop fast A on each scenario drive (a stop turns every bench output off first); '
                 'then read state A until stopped']
ABORTS = ['an alarm or RUNNING flag in any read', 'an output on after its hold or off_reason timer_fault',
          'a refused, failed or uncertain write (no replay)', 'console framing or evidence failure']


def _case(cid, assertion, levels, required=True):
    return {'id': cid, 'required': required, 'assertion': assertion, 'required_levels': list(levels)}


def folder_plan(args, baseline):
    """Scenario-record fields of one wired-bench scenario, built before any port access."""
    name, case = args.scenario, args.case or args.scenario
    facts = _facts(args)
    write_list, left = [], []
    drives = sorted(set(records.FIXTURE['input_drives'] if name in ('input-map', 'status-mapping', 'input-timing') else
                        args.drives or records.FIXTURE['input_drives'] if name.startswith('baseline') else
                        [args.address]) | ({args.change['drive']} if args.change else set()))
    plan = {'admissible': True, 'steps': [], 'refused': []}
    forward = []
    if args.change:
        write_list.append(dict(args.change))
    if name == 'y0-map':
        owner = records.FIXTURE['y0_owner']
        write_list = [{'drive': owner, 'key': 'io.y0', 'index': None, 'candidate': 9},
                      {'drive': owner, 'key': 'io.custom', 'index': None, 'candidate': 1}]
    if name == 'status-mapping':
        write_list = _mapping_write_list(args)
    if name == 'input-timing' and args.filter_value != 2:
        write_list = [{'drive': args.address, 'key': 'filters.input-filter', 'index': None, 'candidate': args.filter_value}]
    experiment = None
    if name == 'stop-inputs':
        write_list, plan, cases, commands, bounds, expected = _stop_plan(args, baseline, facts)
        if args.leave_profile:
            left = [{'drive': e['drive'], 'key': e['key'], 'index': None, 'storage': 'RAM',
                     'reason': 'group run of a series: a profile stage would revoke the start-speed relation of the members '
                               'for this controller session; the last group run restores these words'}
                    for e in write_list if e['key'].startswith('profile.')]
        for entry in write_list:
            steps, refusal = records.plan_entry(baseline, entry, args.image_level, facts, restore=False)
            if refusal and (entry['key'].startswith('profile.') or records.snapshot_route(entry['key'])):
                refusal = None  # Written by the case stimulus (move, velocity or home staging), not by a setter.
            forward.append({'entry': entry, 'steps': steps, 'refusal': refusal})
        write_list_done = True
    elif name == 'limits':
        write_list, plan, cases, commands, bounds, expected = _limits_plan(args, baseline, facts)
        if args.leave_profile:
            left = [{'drive': e['drive'], 'key': e['key'], 'index': None, 'storage': 'RAM',
                     'reason': 'limits series: a profile stage would revoke the start-speed relation of group members '
                               'for this controller session; the last run of the session restores these words'}
                    for e in write_list if e['key'].startswith('profile.')]
        for entry in write_list:
            steps, refusal = records.plan_entry(baseline, entry, args.image_level, facts, restore=False)
            if refusal and (entry['key'].startswith('profile.') or records.snapshot_route(entry['key'])):
                refusal = None  # Written by the case's moves or velocity run, not by a setter.
            forward.append({'entry': entry, 'steps': steps, 'refusal': refusal})
        write_list_done = True
    elif name in ('pt-segments', 'velocity-jog-pv'):
        module = bench_pt if name == 'pt-segments' else bench_velocity
        write_list, plan, cases, commands, bounds, expected, left = module.plan(args, baseline, facts)
        for entry in write_list:
            steps, refusal = records.plan_entry(baseline, entry, args.image_level, facts, restore=False)
            if refusal and entry['key'].startswith('profile.'):
                refusal = None  # Written by the return moves, restored through the stage route.
            if refusal and name == 'velocity-jog-pv' and records.snapshot_route(entry['key']) == 'velocity':
                refusal = None  # Written by the JOG setter and velocity runs; restored by velocity restore.
            forward.append({'entry': entry, 'steps': steps, 'refusal': refusal})
        write_list_done = True
    elif name == 'switch':
        write_list, plan, cases, commands, bounds, expected = _switch_plan(args, baseline, facts)
        for entry in write_list:
            steps, refusal = records.plan_entry(baseline, entry, args.image_level, facts, restore=False)
            if refusal and (records.snapshot_route(entry['key']) or entry['key'].startswith('profile.')):
                refusal = None  # Staged by each home (or H13's moves), restored from the snapshot or the stage route.
            forward.append({'entry': entry, 'steps': steps, 'refusal': refusal})
        write_list_done = True
    elif name in bench_starts.FAMILIES:
        write_list, plan, cases, commands, bounds, expected = _starts_plan(args, baseline, facts)
        if args.leave_profile:
            left = [{'drive': e['drive'], 'key': e['key'], 'index': None, 'storage': 'RAM',
                     'reason': 'input-start series: a profile stage would revoke the start-speed relation of group '
                               'members for this controller session; the last run of the session restores these words'}
                    for e in write_list if e['key'].startswith('profile.')]
        for entry in write_list:
            steps, refusal = records.plan_entry(baseline, entry, args.image_level, facts, restore=False)
            if refusal and (entry['key'].startswith('profile.') or records.snapshot_route(entry['key'])):
                refusal = None  # Written by the case's arm staging or velocity run, not by a setter.
            forward.append({'entry': entry, 'steps': steps, 'refusal': refusal})
        write_list_done = True
    elif name == 'outputs':
        write_list, plan, cases, commands, bounds, expected, experiment = _outputs_plan(args, baseline, facts)
        for entry in write_list:
            steps, refusal = records.plan_entry(baseline, entry, args.image_level, facts, restore=False)
            if refusal and (entry['key'].startswith('profile.') or records.snapshot_route(entry['key'])):
                refusal = None  # Written by the case stimulus (move or home staging), not by a setter.
            forward.append({'entry': entry, 'steps': steps, 'refusal': refusal})
        write_list_done = True
    else:
        write_list_done = False
    if write_list and not write_list_done:
        plan = records.restoration_plan(baseline, write_list, args.image_level, facts)
        for entry in write_list:
            steps, refusal = records.plan_entry(baseline, entry, args.image_level, facts, restore=False)
            forward.append({'entry': entry, 'steps': steps, 'refusal': refusal})
    if not write_list_done:
        cases, commands, bounds, expected = [], [], {}, {}
    if name in ('outputs', 'stop-inputs', 'limits', 'switch', 'pt-segments', 'velocity-jog-pv') or name in bench_starts.FAMILIES:
        pass
    elif name == 'input-start':
        cases.append(_case(case, 'one owned MOSFET pulse on the armed terminal after the firmware isolation reads; the '
                           'staged function 7 start is ATTRIBUTED from fresh samples (onset bracket, no further start '
                           'through the post-release window); no serial start word', ['local', 'controller-timestamped',
                                                                                        'drive-reported']))
        commands = ['useaddr A', 'case write (--change) with its stated facts' if args.change else 'no case write',
                    'read config A; driver read A; motion-profile read; read state A',
                    'io qualification install A %s 7 0 finite-move %s triggers-inhibited' % (args.start_terminal, args.start_mapping),
                    'arm position %s %s %d %d poll-ms %d window-ms %d' % (args.start_terminal, args.arm_form, args.steps,
                                                                          args.rpm, args.poll_ms, args.window_ms),
                    'arm inspect ID until armed', 'arm trigger ID %s %d' % (args.trigger_output, args.width_ms),
                    'arm inspect ID until done', 'disarm ID if not done; release ID', 'restoration plan steps']
        bounds = {'steps': args.steps, 'rpm': args.rpm, 'width_ms': args.width_ms, 'poll_ms': args.poll_ms,
                  'window_ms': args.window_ms, 'output': args.trigger_output, 'terminal': args.start_terminal}
        expected = {'local': 'arm accepted, staged words read back, session armed',
                    'controller-timestamped': 'isolation rows (L7 for non-members, M for the member), ON GPIO-write time',
                    'drive-reported': 'member result outcome, first-change bracket, position change, spacing'}
    elif name == 'input-map':
        mapped = [o for o in args.outputs if o not in args.expect_refusal]
        cases.append(_case(case + '.declarations', 'declaration_agreement: the board table of drives 1, 2, 4 '
                           'equals FIXTURE (X0-X3 connected on each, Y0 on drive 4 only)', ['local']))
        if mapped:
            cases.append(_case(case + '.map', 'measured_map: each output sets exactly its FIXTURE terminal bit on '
                               'drives 1, 2, 4 and the bit clears after off; no alarm or RUNNING',
                               ['drive-reported', 'controller-timestamped']))
        if args.expect_refusal:
            cases.append(_case(case + '.refusal', 'the interlock refuses %s; no bench on is sent and its ons count '
                               'is unchanged' % ', '.join(args.expect_refusal), ['local']))
        if args.change:
            cases.append(_case(case + '.change', 'the case write is acknowledged and read back, then restored to '
                               'the captured value and read back', ['acknowledged', 'read back']))
        commands.append('session: wiring of 1, 2, 4 (declaration agreement); io read and read config of 1, 2, 4 '
                        '(input agreement)')
        for output in args.outputs:
            commands += ['interlock %s: io read then read state of 1, 2, 4; bench view' % output,
                         'bench on %s %d' % (output, args.hold_ms if output in mapped else 50),
                         'read state 1, 2, 4', 'bench off %s' % output, 'read state 1, 2, 4']
        bounds = {'hold_ms': args.hold_ms, 'expected_refusal_hold_ms': 50, 'outputs': args.outputs,
                  'max_age_ms': args.max_age_ms}
        expected = {'local': 'wiring replies (board, sources, fixture); interlock decisions with reads and ages',
                    'drive-reported': 'raw input status of each drive with the output on and off',
                    'controller-timestamped': 'GPIO on/off times and the input-transition interval per drive'}
    elif name == 'y0-map':
        cases.append(_case(case, 'one aux1 edge per custom write, inverted (custom 1 -> aux1 inactive, custom 0 -> '
                           'active), none for the Y0 function writes; drive output status follows',
                           ['controller-timestamped', 'drive-reported']))
        commands = ['useaddr 4', 'debug raw', 'io read 4', 'read state 4', 'bench clear',
                    'io qualification install 4 y0 9 0 0 assign unknown indicator either', 'io set y0 9 4']
        commands += ['per cycle and C in (1, 0): io read 4, read config 4, read state 4; io qualification install '
                     '4 y0 9 0 C assign unknown indicator either; io set custom C 4; read state 4; bench',
                     'restoration plan steps', 'debug off (previous mode)']
        bounds = {'cycles': args.cycles, 'phase_s': args.phase_s, 'writes': 2 + 2 * args.cycles}
        expected = {'controller-timestamped': 'aux1 edge times against the FC06 request start and reply end',
                    'drive-reported': 'output status bit 0 after each custom write'}
    elif name == 'status-mapping':
        cases, commands, bounds, expected = _mapping_plan(args)
    elif name == 'input-timing':
        cases, commands, bounds, expected = _timing_plan(args)
    elif name == 'baseline-compare':
        cases.append(_case(case, 'every committed baseline field of the scenario drives read fresh; each '
                           'difference listed per field; no unknown field', ['read back']))
        commands = ['useaddr A; 61 reads of drive A (identity, config, state, settings groups, 48 records, home, '
                    'velocity, motion-profile read/forget)']
        expected = {'read back': 'baseline_diff.json: status per field (equal, differs, unknown)'}
    elif name == 'baseline-capture':
        cases.append(_case(case, 'a fresh capture has the committed values and dispositions; the re-baselined '
                           'candidate stays in the run folder', ['read back']))
        commands = ['bench; drv; read state 1, 2, 4 (refuse with an output on, a drive running, alarmed or '
                    'unreadable, or recovery required)', 'the baseline-compare reads of every drive']
        expected = {'read back': 'baseline_capture.json, rebaselined.json, baseline_diff.json'}
        bounds = {'reason': args.reason, 'prompt': args.prompt}
    elif name == 'restore':
        cases.append(_case(case, 'the differing field(s) are written back to the captured value through the '
                           'ordinary setter with the stated facts and read back; no other field differs',
                           ['acknowledged', 'read back']))
        commands = ['useaddr A; every baseline read of drive A', 'case write (--change) with its stated facts',
                    'every baseline read of drive A', 'restoration plan steps', 'every baseline read of drive A']
        expected = {'acknowledged': 'write terminals of the case write and the restoration',
                    'read back': 'group reads before, after the case write and after the restoration'}
    if name in ('stop-inputs', 'limits', 'switch', 'pt-segments', 'velocity-jog-pv') or name in bench_starts.FAMILIES:
        drives = list(records.FIXTURE['input_drives'])
    record = {'drives': [{'address': a, 'required_enabled': True} for a in drives],
            'terminals': (['x0', 'x1', 'x2', 'x3'] if name == 'input-map' else ['y0'] if name in ('y0-map', 'outputs') else
                          ['x3', 'y0'] if name == 'stop-inputs' else
                          ['x1', 'x2', 'y0'] if name == 'limits' else
                          ['x0', 'x1', 'x2'] if name == 'switch' else
                          list(records.TERMINALS) if name in ('pt-segments', 'velocity-jog-pv') else
                          ['x0', 'x3', 'y0'] if name in bench_starts.FAMILIES else
                          [args.start_terminal] if name == 'input-start' else
                          (args.terminals or list(records.TERMINALS)) if name == 'status-mapping' else
                          ['x3'] + (['x0', 'x1'] if args.compare_relay else []) if name == 'input-timing' else []),
            'commands': commands, 'cases': cases,
            'running_members': [], 'write_list': write_list, 'bounds': bounds, 'prepared_stop': PREPARED_STOP,
            'restoration_plan': plan, 'case_writes': forward, 'left_changed': left,
            'expected_evidence': expected, 'abort_conditions': ABORTS,
            'cleanup': ['restoration plan for every written field (each read back, no replay)', 'bench off',
                        'read state and io read of every scenario drive (ending_reads.json)',
                        'select the originally selected drive', 'close COM13'],
            'max_age_ms': args.max_age_ms if name == 'input-map' else records.MAX_AGE_MS, 'facts': facts,
            'image_level': args.image_level}
    if experiment is not None:
        record['experiment'] = experiment
    return record


class _Run:
    """Mutable run state shared by one folder scenario and its result writer."""

    def __init__(self, scenario, folder):
        self.scenario, self.folder = scenario, folder
        self.cases = {c['id']: {'verdicts': {}, 'reason': None, 'evidence': ['session.jsonl', 'console.log']}
                      for c in scenario['cases']}
        self.fields = {}
        self.drives = []
        self.outputs_off = False
        self.deviations = []
        self.original = None

    def verdict(self, cid, level, verdict, reason=None, evidence=()):
        case = self.cases[cid]
        case['verdicts'][level] = verdict
        if reason:
            case['reason'] = reason if not case['reason'] else case['reason'] + '; ' + reason
        for path in evidence:
            if path not in case['evidence']:
                case['evidence'].append(path)

    def field(self, entry, **values):
        ident = (entry['drive'], entry['key'], entry.get('index'))
        item = self.fields.setdefault(ident, {'drive': entry['drive'], 'key': entry['key'],
                                              'index': entry.get('index'), 'baseline': entry.get('baseline'),
                                              'ending': None, 'restored': False, 'uncertain': False,
                                              'storage': 'RAM', 'reason': None})
        item.update(values)
        return item


def _state_summary(reply):
    blocks = records._blocks(reply)
    motion_block, status, feedback = blocks.get(0, {}), blocks.get(1, {}), blocks.get(2, {})
    return {'ok': reply.get('ok') is True, 'operation_id': reply.get('operation_id'),
            'running': motion_block.get('running'), 'enabled': motion_block.get('enabled'),
            'released': motion_block.get('released'), 'raw_alarm': motion_block.get('raw_alarm'),
            'alarm_flag': motion_block.get('alarm_flag'), 'raw_speed': feedback.get('raw_speed'),
            'position_words': feedback.get('position_words'), 'raw_inputs': status.get('raw_inputs'),
            'raw_outputs': status.get('raw_outputs')}


def _preflight(console, run, timeout):
    view = checked(console, 'bench', host_args=(), timeout_s=timeout)
    run.original = checked(console, 'wiring', timeout_s=timeout)['address']
    active = [o['name'] for o in view['outputs'] if o['active']]
    if active:
        raise probe.BenchError('a bench output is already on: ' + ', '.join(active))
    return view


def _finish(console, run, timeout):
    """Outputs off, ending reads of every scenario drive, original selection."""
    if not console.synchronized:
        run.deviations.append('console framing lost; no ending reads')
        return
    off = records.outputs_off(console, timeout_s=timeout)
    run.outputs_off = off['outputs_off']
    ending = {'bench': off['view'], 'drives': {}}
    for drive in run.scenario['drives']:
        address = drive['address']
        state = console.command('read-state', address=address, timeout_s=timeout)
        io = console.command('io', driver_args=('read',), address=address, timeout_s=timeout)
        ending['drives'][str(address)] = {'state': _state_summary(state),
                                          'io': (io.get('observation') or {}).get('raw') if io.get('ok') else None}
        s = ending['drives'][str(address)]['state']
        run.drives.append({'address': address, 'stopped': s['ok'] and s['running'] is False and s['raw_speed'] == 0,
                           'alarm': bool(s['raw_alarm']) or s['alarm_flag'] is not False, 'enabled': s['enabled'],
                           'required_enabled': drive['required_enabled']})
    if run.original is not None:
        records.use_drive(console, run.original, timeout_s=timeout)
    run.folder.write_json('ending_reads.json', ending)


def _case_write(console, run, entry, steps, timeout):
    """The case's own write through its ordinary route; read back once."""
    step = steps[0]
    records.use_drive(console, step['drive'], timeout_s=timeout)
    current, _ = records.readback(console, step, timeout_s=timeout)
    field = run.field(entry, baseline=step['baseline'])
    if current != step['baseline']:
        raise probe.BenchError('drive %d %s reads %s before the case write, baseline %s; no write'
                               % (step['drive'], step['key'], current, step['baseline']))
    outcome = records.apply_write(console, step, timeout_s=timeout)
    ending, _ = records.readback(console, step, timeout_s=timeout)
    field.update(ending=ending, case_write={k: v for k, v in outcome.items() if k != 'reply'})
    if outcome['uncertain']:
        field.update(uncertain=True, reason='case write uncertain; not replayed')
        raise probe.BenchError('case write uncertain; no replay')
    if not outcome['written']:
        field.update(ending=current, restored=True, reason='case write refused before transmission')
        raise probe.BenchError('case write refused: %s' % outcome['result'].get('result'))
    return outcome, ending


def _restore_written(console, run, plan, timeout):
    """Run the recorded restoration plan for every written field; merge into run.fields."""
    if not plan['steps']:
        return []
    results = records.restore(console, plan, timeout_s=timeout)
    for result in results:
        if result['key'] == 'profile.start-speed-relation':
            continue
        run.field(result, ending=result['ending'], restored=result['restored'], uncertain=result['uncertain'],
                  reason=result['reason'], restoration={k: result.get(k) for k in ('before', 'written', 'write')})
    return results


def _input_map(console, args, record, run, baseline):
    timeout = args.timeout
    scenario = run.scenario
    cid = args.case or args.scenario
    view = _preflight(console, run, timeout)
    ctx = {'max_age_ms': scenario['max_age_ms'], 'running_members': scenario['running_members']}
    declarations = records.check_declarations(console, timeout_s=timeout)
    ctx['declarations'] = declarations
    ctx['input_agreement'] = records.check_input_agreement(console, timeout_s=timeout)
    run.verdict(cid + '.declarations', 'local', declarations['verdict'],
                None if declarations['verdict'] == 'PASS' else '; '.join(declarations['problems']), ['input_map.json'])
    before = {}
    for address in records.FIXTURE['input_drives']:
        before[address] = records.read_drive(console, address, ('io', 'config', 'state'), select=False, timeout_s=timeout)
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, ('io', 'config'))
    run.folder.write_json('baseline_before.json', {'bench': records._bench_view(view), 'drives': {
        str(a): {k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')} for k, e in r.items()}
        for a, r in before.items()}, 'diff': [d for d in diff if d['status'] != 'equal'],
        'compared': len(diff)})
    detail = {'declarations': declarations, 'input_agreement': ctx['input_agreement'], 'outputs': [],
              'interlock': ctx.setdefault('interlock', [])}
    changed_ok = True
    try:
        if declarations['verdict'] != 'PASS':
            raise probe.BenchError('declaration agreement failed; no write and no bench output')
        if ctx['input_agreement']['verdict'] != 'PASS':
            raise probe.BenchError('io read and read config disagree; no write and no bench output: ' +
                                   '; '.join(ctx['input_agreement']['problems']))
        if args.change:
            write = scenario['case_writes'][0]
            if write['refusal'] or not scenario['restoration_plan']['admissible']:
                reason = (write['refusal'] or {}).get('reason') or '; '.join(
                    r['reason'] for r in scenario['restoration_plan']['refused'])
                for level in ('acknowledged', 'read back'):
                    run.verdict(cid + '.change', level, 'NOT RUN', 'no admissible route before any write: ' + reason)
                for cid_other in run.cases:
                    if cid_other != cid + '.declarations' and not run.cases[cid_other]['verdicts']:
                        for level in next(c for c in scenario['cases'] if c['id'] == cid_other)['required_levels']:
                            run.verdict(cid_other, level, 'NOT RUN', 'the case write has no admissible route')
                return
            outcome, ending = _case_write(console, run, write['entry'], write['steps'], timeout)
            changed_ok = ending == write['entry']['candidate']
            run.verdict(cid + '.change', 'acknowledged', 'PASS' if outcome['written'] else 'FAIL')
            detail['change'] = {'entry': write['entry'], 'readback': ending,
                                'result': outcome['result']}
            if not changed_ok:
                raise probe.BenchError('case write read back %s, requested %s' % (ending, write['entry']['candidate']))
        for output in args.outputs:
            detail['outputs'].append(_map_output(console, args, run, ctx, output, timeout))
    finally:
        if console.synchronized:
            off = records.outputs_off(console, timeout_s=timeout)
            detail['outputs_off_after_map'] = off
        if args.change and console.synchronized:
            plan = scenario['restoration_plan']
            if plan['admissible']:
                _restore_written(console, run, plan, timeout)
                field = run.fields.get((args.change['drive'], args.change['key'], args.change['index']))
                if field:
                    run.verdict(cid + '.change', 'read back',
                                'PASS' if changed_ok and field['restored'] and not field['uncertain'] else 'FAIL',
                                None if field['restored'] else field['reason'])
        run.folder.write_json('input_map.json', detail)
    mapped = [row for row in detail['outputs'] if not row.get('expected_refusal')]
    if mapped and cid + '.map' in run.cases:
        drive_ok = all(row.get('measured_ok') for row in mapped)
        reasons = [row['problem'] for row in mapped if row.get('problem')]
        run.verdict(cid + '.map', 'drive-reported', 'PASS' if drive_ok else 'FAIL', '; '.join(reasons) or None,
                    ['input_map.json'])
        timing = [row.get('timing_ok') for row in mapped]
        run.verdict(cid + '.map', 'controller-timestamped',
                    'PASS' if all(timing) else 'indeterminate' if drive_ok else 'FAIL',
                    None if all(timing) else 'an input-transition interval could not be bracketed', ['input_map.json'])
    refusals = [row for row in detail['outputs'] if row.get('expected_refusal')]
    if refusals:
        ok = all(row.get('refused') and row.get('ons_unchanged') and not row.get('bench_on_sent') for row in refusals)
        run.verdict(cid + '.refusal', 'local', 'PASS' if ok else 'FAIL',
                    None if ok else 'an expected refusal admitted an output', ['input_map.json'])


def _map_output(console, args, run, ctx, output, timeout):
    """One output: interlocked ON, read state on, OFF, read state off."""
    terminal = records.FIXTURE['outputs'][output]
    bit = 1 << records.TERMINALS.index(terminal)
    expected_refusal = output in args.expect_refusal
    drives = records.FIXTURE['input_drives']
    view = checked(console, 'bench', host_args=(), timeout_s=timeout)
    ons_before = next(o['ons'] for o in view['outputs'] if o['name'] == output)
    row = {'output': output, 'terminal': terminal, 'bit': bit, 'expected_refusal': expected_refusal,
           'ons_before': ons_before}
    hold = 50 if expected_refusal else args.hold_ms
    try:
        on = records.bench_on(console, output, hold, ctx, timeout_s=timeout)
    except records.InterlockRefused as refusal:
        after = checked(console, 'bench', host_args=(), timeout_s=timeout)
        ons_after = next(o['ons'] for o in after['outputs'] if o['name'] == output)
        row.update(refused=True, bench_on_sent=False, decision=refusal.entry['decision'], row=refusal.entry['row'],
                   reason=refusal.entry['reason'], address=refusal.entry.get('address'), ons_after=ons_after,
                   ons_unchanged=ons_after == ons_before, active_after=next(
                       o['active'] for o in after['outputs'] if o['name'] == output))
        if not expected_refusal:
            row['problem'] = 'interlock refused %s: %s' % (output, refusal.entry['reason'])
        return row
    entry = ctx['interlock'][-1]
    row.update(refused=False, bench_on_sent=True, on_us=entry['on']['outputs'][output]['on_us'],
               age_at_on_ms=entry['age_at_on_ms'])
    if expected_refusal:
        # Deviation: send the prepared independent stops at once, then confirm standstill.
        row['stops'] = [console.command('stop', stop_policy='fast', address=a, timeout_s=timeout).get('ok')
                        for a in drives]
        row['outputs_off'] = records.outputs_off(console, timeout_s=timeout)
        row['after'] = {str(a): _state_summary(console.command('read-state', address=a, timeout_s=timeout))
                        for a in drives}
        row['problem'] = 'expected refusal admitted; prepared fast stops sent'
        run.deviations.append('%s interlock admitted an expected refusal; prepared stops sent' % output)
        raise probe.BenchError(row['problem'])
    on_states = {a: console.command('read-state', address=a, timeout_s=timeout) for a in drives}
    off = checked(console, 'bench', host_args=('off', output), timeout_s=timeout)
    off_view = next(o for o in off['outputs'] if o['name'] == output)
    off_states = {a: console.command('read-state', address=a, timeout_s=timeout) for a in drives}
    row.update(off_us=off_view['off_us'], off_reason=off_view['off_reason'],
               on_reply_hold_ms=entry['on']['outputs'][output]['hold_ms'])
    problems = []
    if off_view['active'] or off_view['off_reason'] not in ('request',):
        problems.append('off reply shows %s active=%s reason %s' % (output, off_view['active'], off_view['off_reason']))
    row['on'], row['off'] = {}, {}
    for a in drives:
        for label, states in (('on', on_states), ('off', off_states)):
            s = _state_summary(states[a])
            row[label][str(a)] = s
            if not s['ok'] or s['running'] is not False or s['raw_alarm'] or s['alarm_flag']:
                problems.append('drive %d %s read: ok=%s running=%s alarm=%s' % (a, label, s['ok'], s['running'], s['raw_alarm']))
        observed = row['on'][str(a)]['raw_inputs']
        measured = [t for i, t in enumerate(records.TERMINALS) if observed is not None and (observed >> i) & 1]
        row['on'][str(a)]['measured_terminals'] = measured
        if measured != [terminal]:
            problems.append('drive %d: %s set %s, FIXTURE %s' % (a, output, measured or 'no bit', terminal))
        if row['off'][str(a)]['raw_inputs'] != 0:
            problems.append('drive %d: input bits %s after %s off' % (a, row['off'][str(a)]['raw_inputs'], output))
    alarm_or_running = any('read:' in p for p in problems)
    old = {a: entry['samples'].get(a) for a in drives}
    edges = [{'output': output, 'edge': 'on', 'gpio_us': row['on_us'], 'bit': bit, 'new_level': True,
              'samples': {a: [old[a], records.io_sample(a, on_states[a])] for a in drives}},
             {'output': output, 'edge': 'off', 'gpio_us': row['off_us'], 'bit': bit, 'new_level': False,
              'samples': {a: [records.io_sample(a, on_states[a]), records.io_sample(a, off_states[a])] for a in drives}}]
    row['intervals'] = records.correlate([], [], outputs=edges)['outputs']
    row['timing_ok'] = all(d['status'] == 'measured' for r in row['intervals'] for d in r['drives'].values())
    row['measured_ok'] = not problems
    if problems:
        row['problem'] = '; '.join(problems)
    if alarm_or_running:
        records.outputs_off(console, timeout_s=timeout)
        raise probe.BenchError('alarm or RUNNING during the map; map ends here: ' + row['problem'])
    return row


def _y0_map(console, args, record, run, baseline):
    timeout = args.timeout
    scenario = run.scenario
    cid = args.case or args.scenario
    owner = records.FIXTURE['y0_owner']
    _preflight(console, run, timeout)
    declarations = records.check_declarations(console, timeout_s=timeout)
    records.use_drive(console, owner, timeout_s=timeout)
    before = records.read_drive(console, owner, ('io', 'state'), select=False, timeout_s=timeout)
    diff = records.diff_fields(baseline, {owner: records.raws_from(before)}, ('io',))
    run.folder.write_json('baseline_before.json', {'declarations': declarations, 'drives': {str(owner): {
        k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')} for k, e in before.items()}},
        'diff': diff})
    plan = scenario['restoration_plan']
    writes = {w['entry']['key']: w for w in scenario['case_writes']}
    if not plan['admissible'] or any(w['refusal'] for w in writes.values()):
        for level in scenario['cases'][0]['required_levels']:
            run.verdict(cid, level, 'NOT RUN', 'no admissible route before any write: ' + '; '.join(
                [r['reason'] for r in plan['refused']] + [w['refusal']['reason'] for w in writes.values() if w['refusal']]))
        return
    if declarations['verdict'] != 'PASS' or any(d['status'] != 'equal' for d in diff):
        raise probe.BenchError('Y0 owner not at the baseline I/O words or declarations disagree; no write')
    traffic = []
    emit = console.emit

    def tap(event, **fields):
        if event == 'traffic' and len(traffic) < 8192:
            traffic.append(fields['response'])
        emit(event, **fields)
    console.emit = tap
    views, rows = [], []
    detail = {'declarations': declarations, 'rows': rows}
    try:
        checked(console, 'bench', host_args=('clear',), timeout_s=timeout)
        views.append(checked(console, 'bench', host_args=(), timeout_s=timeout))
        detail['edges_after_clear'] = views[0]['input']['edges']
        debug_before = checked(console, 'debug', host_args=(), timeout_s=timeout)

        def write(key, value):
            step = dict(writes[key]['steps'][0], target=value, candidate=value)
            command, kwargs, text = records.write_call({'key': key, 'index': None}, value, owner)
            step['write'] = {'command': command, 'kwargs': kwargs, 'text': text}
            time.sleep(.05)
            outcome = records.apply_write(console, step, timeout_s=timeout)
            time.sleep(.05)
            run.field(writes[key]['entry'], baseline=step['baseline'])
            if outcome['uncertain'] or not outcome['written']:
                run.field(writes[key]['entry'], uncertain=outcome['uncertain'],
                          reason='y0-map write %s %s failed or uncertain; not replayed' % (key, value))
                raise probe.BenchError('y0-map write %s %s failed or uncertain' % (key, value))
            state = console.command('read-state', address=owner, timeout_s=timeout)
            view = checked(console, 'bench', host_args=(), timeout_s=timeout)
            views.append(view)
            reply = outcome['reply']
            evidence = [dict(zip(reply.get('evidence_columns', []), r)) for r in reply.get('evidence', [])]
            fc06 = next((e for e in evidence if e.get('write')), None)
            rows.append({'write': '%s %d' % (key, value), 'operation_id': reply.get('operation_id'),
                         'readback': (reply.get('observation') or {}).get('raw'),
                         'fc06': {k: fc06.get(k) for k in ('register', 'attempted_us', 'earliest_us', 'latest_us',
                                                           'delivered_us')} if fc06 else None,
                         'raw_outputs': _state_summary(state)['raw_outputs'],
                         'edges': view['input']['edges'], 'overwritten': view['input']['overwritten'],
                         'aux1_active': view['input']['active']})
            return rows[-1]
        write('io.y0', 9)
        started = time.monotonic()
        for cycle in range(args.cycles):
            for bit in (1, 0):
                row = write('io.custom', bit)
                row['cycle'] = cycle + 1
                due = started + (2 * cycle + (1 if bit else 2)) * args.phase_s
                time.sleep(max(0.0, due - time.monotonic()))
        detail['debug_before'] = {k: debug_before.get(k) for k in ('observed', 'emitted', 'dropped', 'missed', 'overwritten')}
        detail['debug_after'] = {k: v for k, v in checked(console, 'debug', host_args=(), timeout_s=timeout).items()
                                 if k in ('observed', 'emitted', 'dropped', 'missed', 'overwritten')}
    finally:
        console.emit = emit
        if console.synchronized:
            _restore_written(console, run, plan, timeout)
            views.append(checked(console, 'bench', host_args=(), timeout_s=timeout))
            detail['edges_after_restore'] = views[-1]['input']['edges']
        detail['correlation'] = records.correlate(traffic, [records._bench_view(v) for v in views], owner=owner)
        detail['traffic_records'] = len(traffic)
        run.folder.write_json('y0_map.json', detail)
        run.folder.write_json('traffic.json', traffic)
    custom = [r for r in rows if r['write'].startswith('io.custom')]
    problems, previous = [], detail.get('edges_after_clear', 0)
    for r in rows:
        added = r['edges'] - previous
        previous = r['edges']
        expected = 1 if r['write'].startswith('io.custom') else 0
        if added != expected:
            problems.append('%s added %d edges' % (r['write'], added))
        elif expected and r['aux1_active'] != r['write'].endswith(' 0'):
            problems.append('%s left aux1 active=%s (inverted sense expects %s)' % (r['write'], r['aux1_active'],
                                                                                   r['write'].endswith(' 0')))
    if rows and detail.get('edges_after_restore') != rows[-1]['edges']:
        problems.append('the Y0 restoration write changed the edge count')
    measured = [e for e in detail['correlation']['aux1']['edges'] if e['status'] == 'measured']
    if len(measured) != len(custom):
        # Timing is reported, not judged; a missing pairing is recorded, never read as no edge.
        run.deviations.append('y0-map timing: %d of %d edges paired with their FC06 (see y0_map.json)'
                              % (len(measured), len(detail['correlation']['aux1']['edges'])))
    if len(custom) != 2 * args.cycles:
        problems.append('%d custom writes, planned %d' % (len(custom), 2 * args.cycles))
    run.verdict(cid, 'controller-timestamped', 'PASS' if not problems else 'FAIL', '; '.join(problems) or None,
                ['y0_map.json', 'traffic.json'])
    status = [r['raw_outputs'] == (1 if r['write'] == 'io.custom 1' else 0) for r in custom]
    run.verdict(cid, 'drive-reported', 'PASS' if custom and all(status) else 'FAIL',
                None if custom and all(status) else 'output status did not follow every custom write', ['y0_map.json'])


def _read_drives(console, drives, timeout):
    out = {}
    for address in drives:
        out[address] = records.read_drive(console, address, records.ALL_READS, timeout_s=timeout)
    return out


def _compact(reads):
    return {str(a): {k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')} for k, e in r.items()}
            for a, r in reads.items()}


def _baseline_compare(console, args, record, run, baseline):
    timeout = args.timeout
    cid = args.case or args.scenario
    view = _preflight(console, run, timeout)
    drives = [d['address'] for d in run.scenario['drives']]
    reads = _read_drives(console, drives, timeout)
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in reads.items()})
    run.folder.write_json('baseline_before.json', {'bench': records._bench_view(view), 'drives': _compact(reads)})
    differs = [d for d in diff if d['status'] == 'differs']
    unknown = [d for d in diff if d['status'] == 'unknown']
    run.folder.write_json('baseline_diff.json', {'compared': len(diff), 'differs': differs, 'unknown': unknown})
    for d in differs:
        run.deviations.append('drive %d %s%s differs: baseline %s, read %s' % (
            d['drive'], d['key'], '[%d]' % d['index'] if d['index'] else '', d['baseline'], d['current']))
    run.verdict(cid, 'read back', 'PASS' if not unknown and diff else 'indeterminate',
                None if not unknown else '%d fields unknown (failed reads)' % len(unknown), ['baseline_diff.json'])


def _baseline_capture(console, args, record, run, baseline):
    timeout = args.timeout
    cid = args.case or args.scenario
    view = _preflight(console, run, timeout)
    drv = checked(console, 'drv', timeout_s=timeout)
    drives = [d['address'] for d in run.scenario['drives']]
    refusals = []
    if drv.get('recovery_required') or drv.get('pending'):
        refusals.append('owner reports recovery required or a pending operation')
    for address in drives:
        state = console.command('read-state', address=address, timeout_s=timeout)
        if not probe.drive_reported_stopped(state):
            refusals.append('drive %d not readable, stopped and alarm-free' % address)
    if refusals:
        raise probe.BenchError('baseline-capture refused: ' + '; '.join(refusals))
    image = run.scenario['image']
    capture = records.capture_baseline(console, drives, date=run.folder.date, commit=run.scenario['commit'],
                                       image=image, run='build/bench/' + run.folder.name, timeout_s=timeout)
    run.folder.write_json('baseline_before.json', {'bench': records._bench_view(view), 'drv': {
        k: drv.get(k) for k in ('recovery_required', 'pending', 'busy')}, 'capture': 'baseline_capture.json'})
    run.folder.write_json('baseline_capture.json', capture)
    rebased = records.rebaseline(baseline, capture, args.reason, args.prompt)
    run.folder.write_json('rebaselined.json', rebased)
    changes = records.compare_baselines(baseline, capture)
    run.folder.write_json('baseline_diff.json', {'changes': changes})
    for change in changes:
        run.deviations.append('drive %d %s %s change: %s -> %s' % (change['drive'], change['key'], change['change'],
                                                                   change['old'], change['new']))
    run.verdict(cid, 'read back', 'PASS' if not changes else 'FAIL',
                None if not changes else '%d value or disposition differences' % len(changes),
                ['baseline_capture.json', 'rebaselined.json', 'baseline_diff.json'])


def _restore(console, args, record, run, baseline):
    timeout = args.timeout
    cid = args.case or args.scenario
    scenario = run.scenario
    view = _preflight(console, run, timeout)
    address = args.address
    before = _read_drives(console, [address], timeout)
    diff = records.diff_fields(baseline, {address: records.raws_from(before[address])})
    run.folder.write_json('baseline_before.json', {'bench': records._bench_view(view), 'drives': _compact(before),
                                                   'diff': [d for d in diff if d['status'] != 'equal']})
    detail = {'before': [d for d in diff if d['status'] != 'equal']}
    plan = scenario['restoration_plan']
    write = (scenario['case_writes'] or [None])[0]
    try:
        if write is not None and (write['refusal'] or not plan['admissible']):
            reason = '; '.join([r['reason'] for r in plan['refused']] + ([write['refusal']['reason']] if write['refusal'] else []))
            for level in scenario['cases'][0]['required_levels']:
                run.verdict(cid, level, 'NOT RUN', 'no admissible route before any write: ' + reason)
            return
        if any(d['status'] != 'equal' for d in diff):
            raise probe.BenchError('drive %d differs from the baseline before the case write; no write' % address)
        if write is not None:
            outcome, ending = _case_write(console, run, write['entry'], write['steps'], timeout)
            detail['case_write'] = {'entry': write['entry'], 'readback': ending, 'result': outcome['result']}
            if ending != write['entry']['candidate']:
                raise probe.BenchError('case write read back %s, requested %s' % (ending, write['entry']['candidate']))
        middle = _read_drives(console, [address], timeout)
        mid = records.diff_fields(baseline, {address: records.raws_from(middle[address])})
        detail['differing'] = [d for d in mid if d['status'] != 'equal']
        planned = {(s['drive'], s['key'], s.get('index')) for s in plan['steps']}
        unplanned = [d for d in detail['differing'] if (d['drive'], d['key'], d['index']) not in planned]
        detail['results'] = _restore_written(console, run, plan, timeout)
        after = _read_drives(console, [address], timeout)
        final = records.diff_fields(baseline, {address: records.raws_from(after[address])})
        detail['after'] = [d for d in final if d['status'] != 'equal']
        written = [r for r in detail['results'] if r.get('written')]
        acknowledged = bool(written) and all(r['write']['result'].get('ok') for r in written if 'result' in r.get('write', {}))
        run.verdict(cid, 'acknowledged', 'PASS' if acknowledged else 'FAIL',
                    None if acknowledged else 'no acknowledged restoration write')
        clean = not detail['after'] and not unplanned and all(r['restored'] for r in detail['results'])
        run.verdict(cid, 'read back', 'PASS' if clean else 'FAIL',
                    None if clean else 'fields differ after the restoration: %s' % (
                        [(d['key'], d['current']) for d in detail['after'] + unplanned]), ['restore.json'])
    finally:
        run.folder.write_json('restore.json', detail)


# --------------------------------------------------------------------------
# Wired bench 05: input status mapping (0x0008 against 0x0040)

TERMINAL_OUTPUT = {terminal: output for output, terminal in records.FIXTURE['outputs'].items()}
MAPPING_FACTS = {'x0': ['triggers-inhibited'], 'x1': ['protection-removal', 'triggers-inhibited'],
                 'x2': ['protection-removal', 'triggers-inhibited'], 'x3': ['triggers-inhibited']}
MAPPING_HOLD_MS = 2000


def _mapping_write_list(args):
    """Every I/O field the status-mapping run writes, per drive (furthest candidate)."""
    if args.mapping_cases == 'record':
        return [{'drive': 1, 'key': 'io.input-polarity', 'index': None, 'candidate': 8},
                {'drive': 1, 'key': 'io.x3', 'index': None, 'candidate': 17}]
    out = []
    for a in records.FIXTURE['input_drives']:
        out.append({'drive': a, 'key': 'io.input-polarity', 'index': None, 'candidate': 15})
        out += [{'drive': a, 'key': 'io.' + t, 'index': None, 'candidate': 14 + i}
                for i, t in enumerate(records.TERMINALS) if t in (args.terminals or records.TERMINALS)]
    return out


def _mapping_plan(args):
    """Cases, commands, bounds and expected evidence of the status-mapping scenario."""
    if args.mapping_cases == 'record':
        cases = [_case('M05', 'drive 1 X3 = 17: polarity 8 refused before TX with mapping unknown, admitted and read '
                       'back with mapping recorded; polarity 0 and X3 0 restored', ['sent', 'read back']),
                 _case('N01', 'refused before TX: input filter outside the kept set, recorded without a fresh '
                       'identity, an explicit mapping against the record', ['sent'])]
        commands = ['useaddr 1; io read 1; read config 1; read state 1; io qualification install 1 x3 17 0 assign '
                    'unknown triggers-inhibited; io set x3 17 1', 'io qualification install 1 x3 17 1 assign unknown '
                    'triggers-inhibited (refused); io set input-polarity 8 1 (refused, 0 frames)',
                    'read identity 1; io qualification install 1 x3 17 1 assign recorded triggers-inhibited; '
                    'io set input-polarity 8 1; io read 1', 'restore polarity 0 (recorded) and X3 0',
                    'N01: tuning filters set input-filter 4 1 with CONFIGURE records; useaddr 2/1, io read 1, read '
                    'state 1, install recorded (no fresh identity); read identity 1, install with the opposite mapping']
        bounds = {'drive': 1, 'terminal': 'x3', 'no bench output': True}
        expected = {'sent': 'debug raw observed frame count unchanged across each refusal; operation ID 0',
                    'read back': 'io read words after each admitted write'}
        return cases, commands, bounds, expected
    cases = [_case('M01', 'status bit t follows its output at polarity 0 on drives 1, 2, 4; other bits 0',
                   ['drive-reported', 'controller-timestamped']),
             _case('M02', 'inversion at function 0: one class (after or before) on every terminal and drive',
                   ['drive-reported']),
             _case('M03', 'inversion at PIN function 14+t: the same class as M02', ['drive-reported']),
             _case('M04', 'I/O words of drives 1, 2, 4 equal the baseline at the end', ['read back'])]
    commands = ['session: wiring of 1, 2, 4; io read and read config of 1, 2, 4',
                'per terminal t: step 1 io set xt 0 A (power-up function not 0) with ASSIGN facts',
                'step 2: read state 1, 2, 4; interlock; bench on OUTPUT %d; read state 1, 2, 4; bench off OUTPUT; '
                'read state 1, 2, 4' % MAPPING_HOLD_MS,
                'step 3: io set input-polarity (bit t = 1) A with ASSIGN xt 0 1 unknown; step 2 reads; '
                'io set input-polarity (bit t = 0) A with ASSIGN xt 0 0 unknown; read state 1, 2, 4',
                'step 4: io set xt 14+t A (triggers-inhibited); polarity bit t = 1 and back with mapping stated '
                'as measured; io set xt POWER-UP A',
                'every write: useaddr A; io read A; read config A; read state A; bench view; preconditions; '
                'install; write; read config A; io read A']
    bounds = {'hold_ms': MAPPING_HOLD_MS, 'terminals': args.terminals or list(records.TERMINALS),
              'writes_per_terminal_and_drive': 'at most 7', 'no motion': True}
    expected = {'drive-reported': 'raw input status (0x0008) of every drive off, on and off again per phase',
                'controller-timestamped': 'GPIO on/off times of each ON',
                'read back': 'io read words after every write and at the end'}
    return cases, commands, bounds, expected


class _Ambiguous(probe.BenchError):
    """An uncertain I/O write or a readback that is neither the old nor the requested value."""


def _io_step(baseline, address, key, target, facts, mappings=None):
    """One scenario-owned ordinary I/O write (``io set``), planned like 04's steps."""
    field = records.baseline_field(baseline, address, key)
    command, kwargs, text = records.write_call(field, target, address)
    return {'drive': address, 'key': key, 'index': None, 'register': field['register'], 'read': 'io',
            'baseline': field['value'], 'candidate': target, 'target': target, 'route': 'ordinary',
            'purpose': 'assign', 'facts': facts, 'mappings': dict(mappings or {}),
            'write': {'command': command, 'kwargs': kwargs, 'text': text}}


class _MappingState:
    """Latest checked I/O words per drive and the measured class per drive and terminal."""

    def __init__(self, drives):
        self.words = {a: None for a in drives}
        self.classes = {}
        self.rows = []
        self.writes = []

    def predicted(self, address, terminal, polarity_bit, source_on):
        """Status bit the current hypothesis predicts; None while it is not measured."""
        if not polarity_bit:
            return int(source_on)
        mapping = self.classes.get((address, terminal))
        return None if mapping is None else records.status_bit(mapping, polarity_bit, source_on)


def _check_quiet(state, address, words, status, terminal, view, polarity_bit):
    """Every-write preconditions of prompt 05 (Bench procedure): a list of problems."""
    problems = []
    on = [o['name'] for o in view.get('outputs', []) if o.get('active')]
    if on:
        problems.append('bench outputs on: ' + ', '.join(on))
    for drive, known in state.words.items():
        current = words if drive == address else known
        if current is None or any(7 <= f <= 13 for f in current[1:5]):
            problems.append('drive %d holds function 7-13 or its I/O words are unknown: %s' % (drive, current))
    raw = status.get('raw_inputs')
    index = records.TERMINALS.index(terminal)
    if type(raw) is not int:
        problems.append('input status unknown')
    else:
        if raw & ~(1 << index):
            problems.append('an input other than %s reads active with its output off: raw %d' % (terminal, raw))
        expected = state.predicted(address, terminal, polarity_bit, False)
        if expected is not None and ((raw >> index) & 1) != expected:
            problems.append('%s reads %d, the hypothesis predicts %d' % (terminal, (raw >> index) & 1, expected))
    return problems


def _mapping_write(console, run, state, baseline, address, key, target, facts, terminal, mappings, timeout):
    """One write after the prompt's preconditions, then its readback; never replayed."""
    records.use_drive(console, address, timeout_s=timeout)
    step = _io_step(baseline, address, key, target, facts, mappings)
    seen = {}

    def check(out):
        words = list(out['io']['observation']['raw'])
        status = _state_summary(out['state'])
        view = checked(console, 'bench', host_args=(), timeout_s=timeout)
        index = records.TERMINALS.index(terminal)
        seen.update(words=words, raw_inputs=status['raw_inputs'])
        problems = _check_quiet(state, address, words, status, terminal, view, (words[0] >> index) & 1)
        if problems:
            raise probe.BenchError('precondition before %s: %s' % (step['write']['text'], '; '.join(problems)))
    before = (state.words.get(address) or [None] * 9)[probe.IO_FIELDS.index(key[3:])]
    outcome = records.apply_write(console, step, timeout_s=timeout, check=check)
    ending, entry = records.readback(console, step, timeout_s=timeout)
    if entry.get('raw') is not None:
        state.words[address] = list(entry['raw'])
    row = {'drive': address, 'write': step['write']['text'], 'facts': facts, 'mappings': step['mappings'],
           'before_words': seen.get('words'), 'before_inputs': seen.get('raw_inputs'), 'readback': ending,
           'operation_id': outcome['result'].get('operation_id'), 'written': outcome['written'],
           'uncertain': outcome['uncertain'], 'result': outcome['result'].get('result')}
    state.writes.append(row)
    ident = {'drive': address, 'key': key, 'index': None}
    run.field(ident, baseline=step['baseline'], ending=ending,
              restored=ending == step['baseline'], reason=None)
    if outcome['uncertain'] or ending not in (before, target):
        again, _ = records.readback(console, step, timeout_s=timeout)
        row['read_again'] = again
        run.field(ident, ending=again, uncertain=True, restored=False,
                  reason='ambiguous write or readback (%s -> %s, read %s then %s); not replayed'
                         % (before, target, ending, again))
        raise _Ambiguous('%s: read %s then %s' % (step['write']['text'], ending, again))
    if not outcome['written'] or ending != target:
        raise probe.BenchError('%s refused or not read back: %s, read %s'
                               % (step['write']['text'], outcome['result'].get('result'), ending))
    return row


def _status_reads(console, drives, timeout):
    return {a: console.command('read-state', address=a, timeout_s=timeout) for a in drives}


def _status_check(states, terminal, expected):
    """Status rows of every drive and the problems. ``expected[a]`` is a bit or None."""
    index = records.TERMINALS.index(terminal)
    rows, problems = {}, []
    for a, reply in states.items():
        s = _state_summary(reply)
        raw = s['raw_inputs']
        rows[str(a)] = {'raw_inputs': raw, 'operation_id': s['operation_id'], 'running': s['running'],
                        'raw_alarm': s['raw_alarm']}
        if not s['ok'] or s['running'] is not False or s['raw_alarm'] or s['alarm_flag'] is not False:
            problems.append('drive %d: ok=%s running=%s alarm=%s' % (a, s['ok'], s['running'], s['raw_alarm']))
            continue
        if type(raw) is not int:
            problems.append('drive %d: input status unknown' % a)
            continue
        if raw & ~(1 << index):
            problems.append('drive %d: input other than %s active (raw %d)' % (a, terminal, raw))
        bit = (raw >> index) & 1
        rows[str(a)]['bit'] = bit
        if expected.get(a) is not None and bit != expected[a]:
            problems.append('drive %d: %s reads %d, expected %d' % (a, terminal, bit, expected[a]))
    return rows, problems


def _switched(console, ctx, output, drives, timeout):
    """Interlocked ON for MAPPING_HOLD_MS, status of every drive, OFF, status again."""
    records.bench_on(console, output, MAPPING_HOLD_MS, ctx, timeout_s=timeout)
    entry = ctx['interlock'][-1]
    on = _status_reads(console, drives, timeout)
    off = checked(console, 'bench', host_args=('off', output), timeout_s=timeout)
    view = next(o for o in off['outputs'] if o['name'] == output)
    after = _status_reads(console, drives, timeout)
    return {'on': on, 'after': after, 'on_us': entry['on']['outputs'][output]['on_us'], 'off_us': view['off_us'],
            'off_reason': view['off_reason'], 'interlock': {k: entry.get(k) for k in ('decision', 'row', 'reason')}}


def _phase(console, ctx, state, output, terminal, drives, phase, timeout):
    """Status with the source off, on and off again; the class where polarity is 1."""
    index = records.TERMINALS.index(terminal)
    polarity = {a: (state.words[a][0] >> index) & 1 for a in drives}
    row = {'terminal': terminal, 'output': output, 'phase': phase, 'polarity': polarity,
           'functions': {a: state.words[a][1 + index] for a in drives}}
    state.rows.append(row)
    off = _status_reads(console, drives, timeout)
    row['off'], problems = _status_check(off, terminal,
                                         {a: state.predicted(a, terminal, polarity[a], False) for a in drives})
    if problems:
        row['problems'] = problems
        raise probe.BenchError('%s %s, source off: %s' % (terminal, phase, '; '.join(problems)))
    switched = _switched(console, ctx, output, drives, timeout)
    row['on'], on_problems = _status_check(switched['on'], terminal,
                                           {a: state.predicted(a, terminal, polarity[a], True) for a in drives})
    row['after'], after_problems = _status_check(switched['after'], terminal,
                                                 {a: state.predicted(a, terminal, polarity[a], False) for a in drives})
    row.update({k: switched[k] for k in ('on_us', 'off_us', 'off_reason', 'interlock')})
    problems = on_problems + after_problems
    for a in drives:
        if polarity[a]:
            off_bit, after_bit = row['off'][str(a)].get('bit'), row['after'][str(a)].get('bit')
            row.setdefault('classes', {})[str(a)] = records.classify_mapping(
                off_bit, row['on'][str(a)].get('bit')) if off_bit == after_bit else 'contradiction'
            if off_bit != after_bit:
                problems.append('drive %d: %s reads %s before and %s after the ON' % (a, terminal, off_bit, after_bit))
    if switched['off_reason'] != 'request':
        problems.append('off reason %s' % switched['off_reason'])
    if problems:
        row['problems'] = problems
        raise probe.BenchError('%s %s: %s' % (terminal, phase, '; '.join(problems)))
    return row


def _mapping_terminal(console, run, ctx, state, baseline, terminal, drives, timeout):
    """Steps 1-4 of prompt 05 for one terminal on every drive."""
    index = records.TERMINALS.index(terminal)
    output = TERMINAL_OUTPUT[terminal]
    bit = 1 << index
    power_up = {a: records.baseline_field(baseline, a, 'io.' + terminal)['value'] for a in drives}
    for a in drives:  # Step 1: function 0 where the power-up function is not 0.
        old = state.words[a][1 + index]
        if old:
            _mapping_write(console, run, state, baseline, a, 'io.' + terminal, 0,
                           {terminal: records._input_assign_facts(old, 0)}, terminal, None, timeout)
    _phase(console, ctx, state, output, terminal, drives, 'polarity0', timeout)  # Step 2.
    for a in drives:  # Step 3: inversion at function 0, mapping unknown.
        _mapping_write(console, run, state, baseline, a, 'io.input-polarity', state.words[a][0] | bit,
                       {terminal: []}, terminal, {terminal: 'unknown'}, timeout)
    row = _phase(console, ctx, state, output, terminal, drives, 'function0', timeout)
    classes = row.get('classes', {})
    for a in drives:
        if classes.get(str(a)) in ('after', 'before'):
            state.classes[(a, terminal)] = classes[str(a)]
    for a in drives:  # Inverse write, admitted for either outcome.
        _mapping_write(console, run, state, baseline, a, 'io.input-polarity', state.words[a][0] & ~bit,
                       {terminal: []}, terminal, {terminal: 'unknown'}, timeout)
    contradictions = [a for a in drives if (a, terminal) not in state.classes]
    if contradictions:
        raise probe.BenchError('%s at function 0: drives %s give neither class' % (terminal, contradictions))
    _, problems = _status_check(_status_reads(console, drives, timeout), terminal, {a: 0 for a in drives})
    if problems:
        raise probe.BenchError('%s after the inverse write: %s' % (terminal, '; '.join(problems)))
    pin = 14 + index  # Step 4: the PIN selector, the same inversion with the class stated.
    for a in drives:
        _mapping_write(console, run, state, baseline, a, 'io.' + terminal, pin,
                       {terminal: records._input_assign_facts(0, pin)}, terminal, None, timeout)
    for a in drives:
        _mapping_write(console, run, state, baseline, a, 'io.input-polarity', state.words[a][0] | bit,
                       {terminal: ['triggers-inhibited']}, terminal, {terminal: state.classes[(a, terminal)]}, timeout)
    row = _phase(console, ctx, state, output, terminal, drives, 'pin', timeout)
    for a in drives:
        _mapping_write(console, run, state, baseline, a, 'io.input-polarity', state.words[a][0] & ~bit,
                       {terminal: ['triggers-inhibited']}, terminal, {terminal: state.classes[(a, terminal)]}, timeout)
    for a in drives:
        _mapping_write(console, run, state, baseline, a, 'io.' + terminal, power_up[a],
                       {terminal: records._input_assign_facts(pin, power_up[a])}, terminal, None, timeout)
    differing = [a for a in drives if row.get('classes', {}).get(str(a)) != state.classes[(a, terminal)]]
    if differing:
        raise probe.BenchError('%s at PIN %d: drives %s differ from the function-0 class' % (terminal, pin, differing))


def _frames(console, timeout):
    """Owner frame count (debug ``observed``) and the latest admitted operation ID."""
    debug = checked(console, 'debug', host_args=(), timeout_s=timeout)
    drv = checked(console, 'drv', timeout_s=timeout)
    return {'observed': debug.get('observed'), 'operation_id': drv.get('operation_id'), 'mode': debug.get('mode')}


def _refused_before_tx(console, run, label, call, expect, timeout):
    """One command expected to be refused locally: its reply and the frame counts around it."""
    before = _frames(console, timeout)
    reply = call()
    after = _frames(console, timeout)
    row = {'case': label, 'result': reply.get('result'), 'ok': reply.get('ok'), 'read_first': reply.get('read_first'),
           'operation_id': reply.get('operation_id'), 'frames_before': before, 'frames_after': after}
    row['frames_sent'] = (after['observed'] - before['observed']) if type(before['observed']) is int and \
        type(after['observed']) is int else None
    row['pass'] = (reply.get('ok') is False and row['frames_sent'] == 0 and
                   before['operation_id'] == after['operation_id'] and
                   (expect is None or reply.get('result') == expect[0]) and
                   (expect is None or expect[1] is None or reply.get('read_first') == expect[1]))
    return row


def _install(console, tokens, timeout):
    probe.io_qualification_arguments('io-qualification-install', tokens)
    return console.command('io-qualification-install', host_args=tokens, timeout_s=timeout)


def _record_cases(console, args, record, run, baseline):
    """M05 and N01 on drive 1 against the built-in record (image with IoStatusRecord.h entry)."""
    timeout = args.timeout
    scenario = run.scenario
    address = 1
    view = _preflight(console, run, timeout)
    before = {address: records.read_drive(console, address, ('io', 'config', 'state', 'tuning.filters'), timeout_s=timeout)}
    diff = records.diff_fields(baseline, {address: records.raws_from(before[address])}, ('io', 'config', 'tuning.filters'))
    run.folder.write_json('baseline_before.json', {'bench': records._bench_view(view), 'drives': _compact(before),
                                                   'diff': [d for d in diff if d['status'] != 'equal'],
                                                   'compared': len(diff)})
    state = _MappingState(list(records.FIXTURE['input_drives']))
    for a in state.words:
        reply = records.read_group(console, a, 'io', timeout_s=timeout)
        state.words[a] = list(reply['raw']) if reply.get('raw') else None
    state.classes[(address, 'x3')] = 'after'  # The record under test; checked by every precondition read.
    plan = scenario['restoration_plan']
    detail = {'m05': [], 'n01': [], 'writes': state.writes}
    try:
        if any(d['status'] != 'equal' for d in diff) or not plan['admissible']:
            raise probe.BenchError('drive 1 differs from the baseline or the restoration plan is not admissible')
        frames = _frames(console, timeout)
        if frames['mode'] != 'raw' or type(frames['observed']) is not int:
            raise probe.BenchError('debug raw frame counter unavailable; no case')
        _mapping_write(console, run, state, baseline, address, 'io.x3', 17, {'x3': ['triggers-inhibited']}, 'x3',
                       None, timeout)
        records.use_drive(console, address, timeout_s=timeout)
        checked(console, 'io', driver_args=('read',), address=address, timeout_s=timeout)
        checked(console, 'read-state', address=address, timeout_s=timeout)
        unknown = _install(console, ('1', 'x3', '17', '1', 'assign', 'unknown', 'triggers-inhibited'), timeout)
        detail['m05'].append({'case': 'install unknown', 'result': unknown.get('result'), 'ok': unknown.get('ok')})
        detail['m05'].append(_refused_before_tx(console, run, 'io set input-polarity 8 1 after unknown', lambda: console.command(
            'io', driver_args=('set', 'input-polarity', '8'), address=address, timeout_s=timeout), None, timeout))
        checked(console, 'read-identity', address=address, timeout_s=timeout)
        recorded = _mapping_write(console, run, state, baseline, address, 'io.input-polarity', 8,
                                  {'x3': ['triggers-inhibited']}, 'x3', {'x3': 'recorded'}, timeout)
        detail['m05'].append({'case': 'io set input-polarity 8 1 with recorded', 'readback': recorded['readback'],
                              'written': recorded['written']})
        inverse = _mapping_write(console, run, state, baseline, address, 'io.input-polarity', 0,
                                 {'x3': ['triggers-inhibited']}, 'x3', {'x3': 'recorded'}, timeout)
        detail['m05'].append({'case': 'io set input-polarity 0 1 with recorded', 'readback': inverse['readback'],
                              'written': inverse['written']})
        _mapping_write(console, run, state, baseline, address, 'io.x3', 0, {'x3': ['triggers-inhibited']}, 'x3',
                       None, timeout)
        m05_sent = unknown.get('ok') is False and detail['m05'][1]['pass']
        m05_read = recorded['readback'] == 8 and inverse['readback'] == 0
        run.verdict('M05', 'sent', 'PASS' if m05_sent else 'FAIL',
                    None if m05_sent else 'the unknown-mapping path was not refused before TX', ['status_record.json'])
        run.verdict('M05', 'read back', 'PASS' if m05_read else 'FAIL',
                    None if m05_read else 'recorded-path readback %s/%s' % (recorded['readback'], inverse['readback']),
                    ['status_record.json'])
        # N01: input filters outside the kept set (never a candidate; a candidate the measurement dropped).
        for value in (4, 100):
            step = _filter_step(baseline, address, value)
            records.use_drive(console, address, timeout_s=timeout)
            checked(console, 'read-identity', address=address, timeout_s=timeout)
            checked(console, 'read-config', address=address, timeout_s=timeout)
            checked(console, 'tuning', driver_args=('filters', 'read'), address=address, timeout_s=timeout)
            checked(console, 'io', driver_args=('read',), address=address, timeout_s=timeout)
            probe.install_input_facts(console, address, 'configure', timeout_s=timeout)
            detail['n01'].append(_refused_before_tx(console, run, 'tuning filters set input-filter %d 1' % value,
                                                    lambda: call_write(console, step, timeout), None, timeout))
        # N01: recorded without a fresh identity (a binding change since the identity read).
        records.use_drive(console, 2, timeout_s=timeout)
        records.use_drive(console, address, timeout_s=timeout)
        checked(console, 'io', driver_args=('read',), address=address, timeout_s=timeout)
        checked(console, 'read-state', address=address, timeout_s=timeout)
        detail['n01'].append(_refused_before_tx(console, run, 'install recorded without a fresh identity', lambda: _install(
            console, ('1', 'x3', '0', '1', 'assign', 'recorded'), timeout), ('mapping_not_recorded', 'read identity'),
            timeout))
        # N01: an explicit mapping against the record.
        checked(console, 'read-identity', address=address, timeout_s=timeout)
        checked(console, 'io', driver_args=('read',), address=address, timeout_s=timeout)
        checked(console, 'read-state', address=address, timeout_s=timeout)
        detail['n01'].append(_refused_before_tx(console, run, 'install before against the record', lambda: _install(
            console, ('1', 'x3', '0', '1', 'assign', 'before'), timeout), ('mapping_contradicts_record', None), timeout))
        ok = all(row['pass'] for row in detail['n01'])
        run.verdict('N01', 'sent', 'PASS' if ok else 'FAIL',
                    None if ok else 'a refusal sent frames or named another reason', ['status_record.json'])
    finally:
        if console.synchronized:
            detail['outputs_off'] = records.outputs_off(console, timeout_s=timeout)
            for step in plan['steps']:
                if step['key'] == 'io.input-polarity':
                    step['mappings'] = {'x3': 'recorded'}
            records.use_drive(console, address, timeout_s=timeout)  # The identity below must be of this binding.
            checked(console, 'read-identity', address=address, timeout_s=timeout)
            detail['restoration'] = _restore_written(console, run, plan, timeout)
            final = records.read_drive(console, address, ('io', 'tuning.filters'), timeout_s=timeout)
            detail['final'] = {k: v.get('raw') for k, v in final.items()}
        run.folder.write_json('status_record.json', detail)


def call_write(console, step, timeout):
    return records.call(console, step['write'], timeout_s=timeout)


def _status_mapping(console, args, record, run, baseline):
    if args.mapping_cases == 'record':
        return _record_cases(console, args, record, run, baseline)
    timeout = args.timeout
    scenario = run.scenario
    cid = args.case or 'M'
    drives = list(records.FIXTURE['input_drives'])
    view = _preflight(console, run, timeout)
    ctx = {'max_age_ms': scenario['max_age_ms'], 'running_members': [], 'interlock': []}
    ctx['declarations'] = records.check_declarations(console, timeout_s=timeout)
    ctx['input_agreement'] = records.check_input_agreement(console, timeout_s=timeout)
    before = {a: records.read_drive(console, a, ('io', 'config', 'state'), select=False, timeout_s=timeout)
              for a in drives}
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, ('io', 'config'))
    run.folder.write_json('baseline_before.json', {'bench': records._bench_view(view), 'drives': _compact(before),
                                                   'diff': [d for d in diff if d['status'] != 'equal'],
                                                   'compared': len(diff)})
    state = _MappingState(drives)
    for a in drives:
        state.words[a] = list(before[a]['io']['raw']) if before[a]['io'].get('raw') else None
    detail = {'declarations': ctx['declarations'], 'input_agreement': ctx['input_agreement'], 'rows': state.rows,
              'writes': state.writes, 'interlock': ctx['interlock']}
    plan = scenario['restoration_plan']
    ambiguous = None
    terminals = args.terminals or list(records.TERMINALS)
    try:
        if ctx['declarations']['verdict'] != 'PASS' or ctx['input_agreement']['verdict'] != 'PASS':
            raise probe.BenchError('declaration or input agreement failed; no write and no bench output')
        if any(d['status'] != 'equal' for d in diff) or not plan['admissible']:
            raise probe.BenchError('drives differ from the baseline or the restoration plan is not admissible; no write')
        for terminal in terminals:
            _mapping_terminal(console, run, ctx, state, baseline, terminal, drives, timeout)
    except _Ambiguous as error:
        ambiguous = str(error)
        run.deviations.append('ambiguous write: every source off, no further stimulus or write; ' + ambiguous)
        raise
    finally:
        if console.synchronized:
            detail['outputs_off'] = records.outputs_off(console, timeout_s=timeout)
            if ambiguous is None and plan['admissible']:
                # Polarity first; at a nonzero function it needs the class measured in step 3.
                for step in plan['steps']:
                    if step['key'] == 'io.input-polarity':
                        step['mappings'] = {t: state.classes[(step['drive'], t)] for t in records.TERMINALS
                                            if (step['drive'], t) in state.classes}
                detail['restoration'] = _restore_written(console, run, plan, timeout)
            final = {a: records.read_group(console, a, 'io', timeout_s=timeout) for a in drives}
            detail['final_io'] = {str(a): r.get('raw') for a, r in final.items()}
            detail['classes'] = {'%s/%d' % (t, a): c for (a, t), c in sorted(state.classes.items())}
        run.folder.write_json('status_mapping.json', detail)
    _mapping_verdicts(run, cid, state, terminals, detail, baseline, drives)


def _mapping_verdicts(run, cid, state, terminals, detail, baseline, drives):
    rows = {(r['terminal'], r['phase']): r for r in state.rows}
    m01 = [rows.get((t, 'polarity0')) for t in terminals]
    ok = all(r and not r.get('problems') and r.get('on') for r in m01)
    reason = None if ok else 'status did not follow the output at polarity 0 on every drive'
    run.verdict(cid + '01', 'drive-reported', 'PASS' if ok else 'FAIL', reason, ['status_mapping.json'])
    run.verdict(cid + '01', 'controller-timestamped', 'PASS' if ok else 'FAIL', reason, ['status_mapping.json'])
    for case, phase in (('02', 'function0'), ('03', 'pin')):
        found = [(t, a, (rows.get((t, phase)) or {}).get('classes', {}).get(str(a))) for t in terminals for a in drives]
        classes = {c for _, _, c in found}
        complete = all(c in ('after', 'before') for _, _, c in found)
        verdict = 'PASS' if complete and len(classes) == 1 else 'FAIL'
        run.verdict(cid + case, 'drive-reported', verdict,
                    None if verdict == 'PASS' else 'classes %s: no record' % found, ['status_mapping.json'])
    expected = {a: [records.baseline_field(baseline, a, 'io.' + n)['value'] for n in probe.IO_FIELDS] for a in drives}
    final = detail.get('final_io') or {}
    ok = all(final.get(str(a)) == expected[a] for a in drives)
    run.verdict(cid + '04', 'read back', 'PASS' if ok else 'FAIL',
                None if ok else 'I/O words %s, baseline %s' % (final, expected), ['status_mapping.json'])


# --------------------------------------------------------------------------
# Wired bench 05: input filter (0x0108) delay and pulse width

TIMING_WIDTHS_MS = (1, 2, 3, 5, 7, 10, 12, 15, 20, 30, 50)
TIMING_CANDIDATES = (0, 1, 2, 3, 5, 10, 20, 50, 100)  # Prompt 05 candidate set (IoStatusRecord.h)
TIMING_DELAY_HOLD_MS = 200
TIMING_POST_MS = 300   # Polling continues at least this long after the expected OFF.
TIMING_CAP_MS = 1300   # ... and at most this long when the pulse is not seen back at 0.
# After the first sample of a trial the polls pause a random 0..TIMING_PHASE_MS
# (seeded per run, recorded per trial). Without it every trial samples the
# same instants after the ON and the OFF, and the brackets cannot narrow.
TIMING_PHASE_MS = 80


def _timing_plan(args):
    """Cases, commands, bounds and expected evidence of one input-timing run."""
    value, address = args.filter_value, args.address
    cases = []
    if value != 2:
        cases.append(_case('F01', 'filter %d written through the CONFIGURE path and read back equal' % value,
                           ['read back']))
    cases.append(_case('F02', '%d x mosfet2 %d ms on drive %d at filter %d: on and off intervals and poll spacing'
                       % (args.delay_trials, TIMING_DELAY_HOLD_MS, address, value),
                       ['controller-timestamped', 'drive-reported']))
    if args.width_trials:
        cases.append(_case('F03', 'mosfet2 W ms for W in %s, %d trials each: observed, missed or indeterminate'
                           % (list(args.widths), args.width_trials), ['controller-timestamped', 'drive-reported']))
    if args.compare_relay:
        cases.append(_case('F04', '%d x relay2 (X1) and %d x mosfet1 (X0), %d ms: delay difference'
                           % (args.delay_trials, args.delay_trials, TIMING_DELAY_HOLD_MS), ['controller-timestamped']))
    commands = ['session: wiring of 1, 2, 4; io read and read config of 1, 2, 4; debug raw',
                'useaddr %d; read identity, config, tuning filters, io, state of %d' % (address, address)]
    if value != 2:
        commands.append('tuning filters set input-filter %d %d with four CONFIGURE records (read config, read state, '
                        'io qualification install %d xN F P configure unknown); read config; tuning filters read %d'
                        % (value, address, address, address))
    commands += ['per trial: interlock of the output (drive %d read last); bench on OUTPUT HOLD; back-to-back '
                 'read state %d (%d outstanding) to at least %d ms after the expected OFF; bench'
                 % (address, address, args.poll_depth, TIMING_POST_MS),
                 'restoration plan steps; debug off']
    bounds = {'drive': address, 'filter': value, 'delay_trials': args.delay_trials, 'debug': args.traffic,
              'phase_pause_ms': [0, TIMING_PHASE_MS], 'phase_seed': args.phase_seed,
              'delay_hold_ms': TIMING_DELAY_HOLD_MS, 'widths_ms': list(args.widths) if args.width_trials else [],
              'width_trials': args.width_trials, 'compare_relay': args.compare_relay, 'poll_depth': args.poll_depth,
              'post_ms': TIMING_POST_MS, 'cap_ms': TIMING_CAP_MS, 'relay_min_hold_ms': 20, 'no motion': True}
    expected = {'read back': 'tuning filters read after the write and after the restoration',
                'controller-timestamped': 'GPIO on/off times; per sample the 0x0008 request start and reply end',
                'drive-reported': 'raw input status of every sample'}
    return cases, commands, bounds, expected


def _filter_step(baseline, address, value):
    field = records.baseline_field(baseline, address, 'filters.input-filter')
    command, kwargs, text = records.write_call(field, value, address)
    return {'drive': address, 'key': 'filters.input-filter', 'index': None, 'register': field['register'],
            'read': 'tuning.filters', 'baseline': field['value'], 'candidate': value, 'target': value,
            'route': 'ordinary', 'purpose': 'configure', 'facts': {},
            'write': {'command': command, 'kwargs': kwargs, 'text': text}}


def _poll(console, address, depth, timeout, done, replies, pause_s=0.0):
    """Back-to-back ``read state`` of one drive with ``depth`` requests outstanding,
    appended to ``replies``, until ``done(latest_serviced_us)``; one host pause
    of ``pause_s`` after the first reply. A failed poll ends the trial; it is
    reported, never retried."""
    handles, latest = [], 0
    while True:
        while len(handles) < depth and not done(latest):
            handles.append(console.begin('read-state', address=address, timeout_s=timeout))
        if not handles:
            return
        reply = console.wait(handles.pop(0), release=True)
        replies.append(reply)
        if len(replies) == 1 and pause_s > 0 and not handles:
            console.sleep(pause_s)
        if reply.get('ok') and type(reply.get('serviced_us')) is int:
            latest = max(latest, reply['serviced_us'])
        else:
            latest = 1 << 62


def _timing_trial(console, ctx, address, output, hold_ms, depth, timeout, rng=None):
    """One interlocked pulse and its samples; problems list alarms, RUNNING and foreign inputs."""
    terminal = records.FIXTURE['outputs'][output]
    bit = 1 << records.TERMINALS.index(terminal)
    records.bench_on(console, output, hold_ms, ctx, timeout_s=timeout)
    entry = ctx['interlock'][-1]
    on_us = entry['on']['outputs'][output]['on_us']
    expected_off = on_us + hold_ms * 1000
    replies, seen = [], {'checked': 0, 'on': False, 'back': False}

    def done(latest):
        for reply in replies[seen['checked']:]:
            sample = records.io_sample(address, reply)
            if sample is not None and sample['reply_end_us'] >= on_us:
                if sample['raw_inputs'] & bit:
                    seen['on'] = True
                elif seen['on']:
                    seen['back'] = True
        seen['checked'] = len(replies)
        if latest < expected_off + TIMING_POST_MS * 1000:
            return False
        return (seen['on'] and seen['back']) or latest >= expected_off + TIMING_CAP_MS * 1000
    pause_ms = round(rng.uniform(0, TIMING_PHASE_MS), 3) if rng is not None else 0.0
    _poll(console, address, depth, timeout, done, replies, pause_ms / 1000.0)
    view = checked(console, 'bench', host_args=(), timeout_s=timeout)
    o = next(x for x in view['outputs'] if x['name'] == output)
    row = {'output': output, 'hold_ms': hold_ms, 'on_us': on_us, 'off_us': o['off_us'], 'off_reason': o['off_reason'],
           'active_after': o['active'], 'interlock_row': entry['row'], 'pause_ms': pause_ms,
           'pre_sample': entry['samples'].get(address), 'samples': [], 'failed_polls': 0}
    problems = []
    for reply in replies:
        sample = records.io_sample(address, reply)
        s = _state_summary(reply)
        if sample is None:
            row['failed_polls'] += 1
            problems.append('poll %s failed: %s' % (reply.get('operation_id'), reply.get('result', reply.get('outcome'))))
            continue
        row['samples'].append(sample)
        if s['running'] is not False or s['raw_alarm'] or s['alarm_flag'] is not False:
            problems.append('alarm or RUNNING in operation %s' % s['operation_id'])
        if sample['raw_inputs'] & ~bit:
            problems.append('input other than %s active (raw %d)' % (terminal, sample['raw_inputs']))
    if o['active'] or o['off_reason'] not in ('hold_elapsed', 'hold_elapsed_late'):
        problems.append('%s active=%s off reason %s after the trial' % (output, o['active'], o['off_reason']))
    row['problems'] = problems
    return row, bit


def _analyse_trials(rows, bit, window_end_us=None):
    """Per trial ``records.timing_trial``; ``window_end_us`` is relative to the OFF (None: unknown)."""
    out = []
    for row in rows:
        samples = ([row['pre_sample']] if row.get('pre_sample') else []) + row['samples']
        end = None if window_end_us is None else row['off_us'] + window_end_us
        result = records.timing_trial(samples, on_us=row['on_us'], off_us=row['off_us'], bit=bit, window_end_us=end)
        result.update(output=row['output'], hold_ms=row['hold_ms'], off_reason=row['off_reason'],
                      problems=row['problems'], failed_polls=row['failed_polls'])
        out.append(result)
    return out


def _delay_summary(trials):
    """Interval bounds (ms, relative to the edge) and spacing over a set of trials."""
    def stats(values):
        values = sorted(values)
        return {'min': values[0], 'median': values[len(values) // 2], 'max': values[-1]} if values else None
    summary = {'trials': len(trials)}
    for edge in ('on', 'off'):
        measured = [t[edge] for t in trials if t[edge]['status'] == 'measured']
        summary[edge] = {'measured': len(measured),
                         'lower_ms': stats([m['relative_us'][0] / 1000.0 for m in measured]),
                         'upper_ms': stats([m['relative_us'][1] / 1000.0 for m in measured])}
        if measured:  # Valid only if the delay is the same in every trial: [largest lower, smallest upper].
            summary[edge]['bracket_if_constant_ms'] = [summary[edge]['lower_ms']['max'], summary[edge]['upper_ms']['min']]
    spacing = [t['spacing_us'] for t in trials if t.get('spacing_us')]
    summary['spacing_ms'] = {'median': stats([s['median'] / 1000.0 for s in spacing]),
                             'max': stats([s['max'] / 1000.0 for s in spacing])}
    summary['width_ms'] = stats([t['width_us'] / 1000.0 for t in trials])
    summary['observed'] = sum(1 for t in trials if t['observed'])
    return summary


def _input_timing(console, args, record, run, baseline):
    timeout = args.timeout
    scenario = run.scenario
    address, value, depth = args.address, args.filter_value, args.poll_depth
    view = _preflight(console, run, timeout)
    ctx = {'max_age_ms': scenario['max_age_ms'], 'running_members': [], 'interlock': [], 'read_last': address}
    ctx['declarations'] = records.check_declarations(console, timeout_s=timeout)
    ctx['input_agreement'] = records.check_input_agreement(console, timeout_s=timeout)
    records.use_drive(console, address, timeout_s=timeout)
    keys = ('io', 'config', 'tuning.filters')
    before = {a: records.read_drive(console, a, keys, select=False, timeout_s=timeout)
              for a in records.FIXTURE['input_drives']}
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, keys)
    run.folder.write_json('baseline_before.json', {'bench': records._bench_view(view), 'drives': _compact(before),
                                                   'diff': [d for d in diff if d['status'] != 'equal'],
                                                   'compared': len(diff)})
    traffic, emit = [], console.emit

    def tap(event, **fields):
        if event == 'traffic' and len(traffic) < 150000:
            traffic.append(fields['response'])
        emit(event, **fields)
    console.emit = tap
    detail = {'drive': address, 'filter': value, 'poll_depth': depth, 'declarations': ctx['declarations'],
              'input_agreement': ctx['input_agreement'], 'interlock_rows': [], 'delay': [], 'widths': {}, 'relay': {}}
    plan = scenario['restoration_plan']
    stage = 'start'
    rng = random.Random(args.phase_seed)
    detail['phase'] = {'seed': args.phase_seed, 'max_ms': TIMING_PHASE_MS}
    try:
        if ctx['declarations']['verdict'] != 'PASS' or ctx['input_agreement']['verdict'] != 'PASS':
            raise probe.BenchError('declaration or input agreement failed; no write and no bench output')
        if any(d['status'] != 'equal' for d in diff) or not plan['admissible']:
            raise probe.BenchError('drives differ from the baseline or the restoration plan is not admissible; no write')
        detail['debug_before'] = {k: v for k, v in checked(console, 'debug', host_args=(), timeout_s=timeout).items()
                                  if k in ('mode', 'observed', 'emitted', 'dropped', 'missed', 'overwritten')}
        if value != 2:
            stage = 'filter write'
            records.use_drive(console, address, timeout_s=timeout)
            step = _filter_step(baseline, address, value)

            def check(out):
                bench = checked(console, 'bench', host_args=(), timeout_s=timeout)
                on = [o['name'] for o in bench['outputs'] if o['active']]
                if on:
                    raise probe.BenchError('bench output on before the filter write: ' + ', '.join(on))
            outcome = records.apply_write(console, step, timeout_s=timeout, check=check)
            ending, _ = records.readback(console, step, timeout_s=timeout)
            run.field(step, baseline=step['baseline'], ending=ending, restored=ending == step['baseline'])
            detail['filter_write'] = {'result': outcome['result'], 'readback': ending, 'uncertain': outcome['uncertain']}
            if outcome['uncertain']:
                run.field(step, uncertain=True, reason='filter write uncertain; not replayed')
                raise probe.BenchError('filter write uncertain; no replay')
            run.verdict('F01', 'read back', 'PASS' if outcome['written'] and ending == value else 'FAIL',
                        None if ending == value else 'read back %s, requested %d' % (ending, value), ['input_timing.json'])
            if ending != value:
                raise probe.BenchError('filter write read back %s, requested %d' % (ending, value))
        stage = 'delay'
        rows = []
        for _ in range(args.delay_trials):
            row, bit = _timing_trial(console, ctx, address, 'mosfet2', TIMING_DELAY_HOLD_MS, depth, timeout, rng)
            rows.append(row)
            if row['problems']:
                raise probe.BenchError('delay trial: ' + '; '.join(row['problems']))
        detail['delay_rows'] = rows
        detail['delay'] = _analyse_trials(rows, 8)
        detail['delay_summary'] = _delay_summary(detail['delay'])
        on_up, off_up = detail['delay_summary']['on']['upper_ms'], detail['delay_summary']['off']['upper_ms']
        window = None
        if on_up and off_up and detail['delay_summary']['on']['measured'] == len(rows) and \
                detail['delay_summary']['off']['measured'] == len(rows):
            window = int(max(on_up['max'], off_up['max']) * 1000)
        detail['window_after_off_us'] = window
        if args.width_trials:
            stage = 'width'
            for width in args.widths:
                wrows = []
                for _ in range(args.width_trials):
                    row, bit = _timing_trial(console, ctx, address, 'mosfet2', width, depth, timeout, rng)
                    wrows.append(row)
                    if row['problems']:
                        raise probe.BenchError('width trial %d ms: %s' % (width, '; '.join(row['problems'])))
                trials = _analyse_trials(wrows, 8, window)
                detail['widths'][str(width)] = {'rows': wrows, 'trials': trials, 'counts': {
                    c: sum(1 for t in trials if t['class'] == c) for c in ('observed', 'missed', 'indeterminate')},
                    'actual_width_ms': _delay_summary(trials)['width_ms'],
                    'max_instant_gap_ms': max((t.get('max_instant_gap_us') or 0 for t in trials), default=0) / 1000.0}
        if args.compare_relay:
            stage = 'relay'
            for output in ('relay2', 'mosfet1'):
                crows = []
                for _ in range(args.delay_trials):
                    row, bit = _timing_trial(console, ctx, address, output, TIMING_DELAY_HOLD_MS, depth, timeout, rng)
                    crows.append(row)
                    if row['problems']:
                        raise probe.BenchError('%s trial: %s' % (output, '; '.join(row['problems'])))
                trials = _analyse_trials(crows, bit)
                detail['relay'][output] = {'rows': crows, 'trials': trials, 'summary': _delay_summary(trials)}
        stage = 'done'
        detail['debug_after'] = {k: v for k, v in checked(console, 'debug', host_args=(), timeout_s=timeout).items()
                                 if k in ('mode', 'observed', 'emitted', 'dropped', 'missed', 'overwritten')}
    finally:
        console.emit = emit
        detail['stage'] = stage
        if console.synchronized:
            detail['outputs_off'] = records.outputs_off(console, timeout_s=timeout)
            if plan['admissible'] and plan['steps']:
                detail['restoration'] = _restore_written(console, run, plan, timeout)
        detail['interlock_rows'] = [{k: e.get(k) for k in ('output', 'decision', 'row', 'reason', 'age_at_on_ms')}
                                    for e in ctx['interlock']]
        detail['traffic_records'] = len(traffic)
        run.folder.write_json('input_timing.json', detail)
        run.folder.write_json('traffic.json', traffic)
    _timing_verdicts(run, args, detail)


def _timing_verdicts(run, args, detail):
    summary = detail.get('delay_summary')
    if summary:
        complete = summary['on']['measured'] == summary['trials'] == summary['off']['measured']
        level = 'PASS' if complete else 'indeterminate'
        reason = None if complete else '%d/%d on and %d/%d off intervals measured' % (
            summary['on']['measured'], summary['trials'], summary['off']['measured'], summary['trials'])
        run.verdict('F02', 'controller-timestamped', level, reason, ['input_timing.json'])
        run.verdict('F02', 'drive-reported', 'PASS' if summary['observed'] == summary['trials'] else 'indeterminate',
                    None if summary['observed'] == summary['trials'] else '%d of %d pulses observed'
                    % (summary['observed'], summary['trials']), ['input_timing.json'])
    if args.width_trials and len(detail['widths']) == len(args.widths):
        for level in ('controller-timestamped', 'drive-reported'):
            run.verdict('F03', level, 'PASS', None, ['input_timing.json'])
    if args.compare_relay and len(detail['relay']) == 2:
        run.verdict('F04', 'controller-timestamped', 'PASS', None, ['input_timing.json'])




# --------------------------------------------------------------------------
# Outputs (wired bench 06): one case of the Y0 case table per run.

OUTPUT_WRITES = {
    'O1': (('io.y0', 9), ('io.custom', 1)),
    'O2': (('io.y0', 9), ('io.output-polarity', 1), ('io.custom', 1)),
    'O3': (('io.output-polarity', 1),),
    'O4': (('io.y0', 2),),
    'O5': (('io.y0', 2), ('io.output-polarity', 1)),
    'O6': (('io.y0', 4),),
    'O7': (('io.y0', 3), ('homing.auxiliary', 7), ('homing.method', 35)),
    'O8': (('io.y0', 5),),
    'O9': (('io.y0', 1), ('filters.deviation-threshold', 1)),
    'O10': (('io.y0', 10),),
    'O11': (('io.y0', 11),),
}
PROFILE_KEYS = ('profile.acceleration', 'profile.deceleration', 'profile.speed', 'profile.target')
MOTION_CASES = ('O4', 'O5', 'O6', 'O7', 'O9')


def _outputs_write_list(args):
    owner = records.FIXTURE['y0_owner']
    out = [{'drive': owner, 'key': key, 'index': None, 'candidate': value} for key, value in OUTPUT_WRITES[args.case]]
    if args.case in MOTION_CASES:
        out += [{'drive': owner, 'key': key, 'index': None, 'candidate': None} for key in PROFILE_KEYS]
    return out


def _outputs_plan(args, baseline, facts):
    owner = records.FIXTURE['y0_owner']
    case = bench_out.CASES[args.case]
    write_list = _outputs_write_list(args)
    context = {'snapshots': {'home': {}}} if args.case == 'O7' else None
    plan = records.restoration_plan(baseline, write_list, args.image_level, facts, context=context)
    cid = args.case
    assertions = {
        'O1': 'Y0 = 9, polarity 0: custom bit 0 on/off %d cycles; one inverted aux1 edge per write; 0x0009 bit 0 '
              'follows' % args.repetitions,
        'O2': 'Y0 = 9, polarity 1: custom bit 0 on/off %d cycles; level inverted against O1; the 0x0009 mapping '
              'consistent' % args.repetitions,
        'O3': 'Y0 = 0: aux1 and 0x0009 recorded at polarity 0 and 1',
        'O4': 'Y0 = 2: %d moves of 1000/3000 steps and one stop fast mid-move: one ON and one OFF edge per move' %
              args.repetitions,
        'O5': 'Y0 = 2, polarity 1: one 1000-step move, inverse of O4',
        'O6': 'Y0 = 4: %d complete moves, a stop fast short of target, position clear; set/clear events against '
              '0x0007 bit 0' % args.repetitions,
        'O7': 'Y0 = 3: home 35 from a not-homed state, then %d x (a 200-step move, home 33); set/clear events '
              'against 0x0007 bit 1' % args.repetitions,
        'O8': 'Y0 = 5: %d x (motor-release, enable, 1 s); edges follow the commands and 0x0007 bit 4' %
              args.repetitions,
        'O9': 'Y0 = 1, threshold 1: %d x (2000-step move at 60 rpm latching alarm 5, recovery, stop fast, '
              'alarm-clear); edge inside the silence, off edge at alarm-clear' % args.repetitions,
        'O10': 'D9 experiment: Y0 = 10, custom 1, 2, 3, 0, Y0 0; outcome class',
        'O11': 'D9 experiment: Y0 = 11, custom 1, 2, 3, 4, 0, Y0 0; outcome class',
    }
    cases = [_case(cid, assertions[args.case], case['levels'])]
    commands = ['useaddr 4', 'debug decoded', 'read config/io read/read state 4 (baseline compare of the I/O words)',
                'bench clear', 'bench']
    commands += {'custom': ['io set y0 9 4 (ASSIGN y0 indicator either)'] +
                 (['io set output-polarity 1 4'] if case['polarity'] else []) +
                 ['per cycle and C in (1, 0): read config, read state, io read, facts, io set custom C 4; read state; bench'],
                 'levels': ['read state, bench', 'io set output-polarity 1 4', 'read state, bench'],
                 'running': ['io set y0 2 4'] + (['io set output-polarity 1 4'] if case['polarity'] else []) +
                 ['motion-profile read', 'per move: read config, read state, move relative N steps native 60 configured, '
                  'read state until stopped (bench every 4th)'] +
                 (['move relative 3000 ...; stop fast after RUNNING and 0.8 s; move relative -3000'] if args.case == 'O4' else []),
                 'arrival': ['io set y0 4 4', 'motion-profile read', 'per move: as O4', 'move +3000 with stop fast',
                             'move -1000', 'position-clear qualification install clear-semantics', 'position-clear'],
                 'homing': ['CONFIGURE facts', 'home auxiliary 7', 'io set y0 3 4', 'motion-profile read',
                            'per cycle: read config, read state, home read (the first is the restore snapshot), '
                            'home qualification install 35 ...; home 35 60 30 100 zero; move +/-200',
                            'home qualification install 33 ... index reference; home 33 60 30 100 zero'],
                 'brake': ['io set y0 5 4', 'per cycle: motor-release, read state x3, enable, read state x3, 1 s'],
                 'alarm': ['CONFIGURE facts', 'tuning filters set deviation-threshold 1 4', 'io set y0 1 4',
                           'motion-profile read', 'per cycle: move relative 2000 (no reply expected), 2 s, read state '
                           '(recover only if required), stop fast, alarm-clear, read state'],
                 'experiment': ['per step: read config, io read, read state; io experiment d9 FIELD VALUE 4 indicator '
                                'either; io experiment inspect until done; read state; bench']}[case['kind']]
    commands += ['restoration: experiment custom 0 then Y0 0 (O10/O11), motion-profile restore, home restore of the '
                 'first home read (O7), the plan steps', 'debug (previous mode)']
    bounds = {'repetitions': args.repetitions, 'moves_max_steps': 3000, 'speed_rpm': 60,
              'experiment': case['kind'] == 'experiment'}
    expected = {level: {'controller-timestamped': 'aux1 edges against the write request start and reply end and '
                                                  'the drive transition brackets (bench_out.json)',
                        'read back': 'I/O readback after each write; 0x0009 per step',
                        'drive-reported': 'drive status samples around each edge'}[level] for level in case['levels']}
    experiment = None
    if case['kind'] == 'experiment':
        experiment = {'decision': 'D9', 'prompt': '06', 'date': '2026-11-19', 'drive': owner, 'terminal': 'y0',
                      'polarity': 0, 'load': Y0_FACTS_TEXT, 'function': case['function'],
                      'custom_candidates': case['custom'],
                      'restoration': ['io experiment d9 custom 0 4 indicator either (from any candidate)',
                                      'io experiment d9 y0 0 4 indicator either (then the plan finds Y0 equal)']}
    return write_list, plan, cases, commands, bounds, expected, experiment


Y0_FACTS_TEXT = 'indicator either'


def _outputs_restore(console, live, run, plan, timeout):
    """Restoration per the step 2 table: stop and alarm first, then the
    experiment words, the profile snapshot and the plan."""
    owner = records.FIXTURE['y0_owner']
    # The prepared stop first: harmless on a stopped drive, and it reconciles
    # the address conflict an uncertain action leaves (home or release).
    run.deviations_stop = console.command('stop', stop_policy='fast', address=owner, timeout_s=timeout).get('outcome')
    state = console.command('read-state', address=owner, timeout_s=timeout)
    summary = _state_summary(state)
    if not state.get('ok') or summary['running'] or summary['raw_speed']:
        run.deviations.append('drive 4 not reported stopped before restoration; prepared stop fast sent')
        console.command('stop', stop_policy='fast', address=owner, timeout_s=timeout)
        state = probe.observe_stopped(console, address=owner, timeout_s=timeout)
        summary = _state_summary(state)
    if summary['raw_alarm'] or summary['alarm_flag']:
        console.command('stop', stop_policy='fast', address=owner, timeout_s=timeout)
        console.command('alarm-clear', address=owner, timeout_s=timeout)
        summary = _state_summary(console.command('read-state', address=owner, timeout_s=timeout))
    if summary['released']:
        console.command('enable', address=owner, timeout_s=timeout)
        time.sleep(1.0)
    if 'profile' in live.snapshots and not live.snapshots.get('profile_replaced'):
        console.command('read-config', address=owner, timeout_s=timeout)
        console.command('read-state', address=owner, timeout_s=timeout)
        try:
            live.snapshots['profile_restored'] = bench_out._profile(live, 'restore')
        except probe.BenchError as error:
            run.deviations.append('motion-profile restore: %s' % error)
    if live.snapshots.get('profile_replaced'):
        # The replacement session's original is not the baseline; release it so
        # the planner reads the current words fresh and stages the baseline.
        console.command('motion-profile', host_args=('forget',), timeout_s=timeout)
    snapshots = {'home': live.snapshots['home']} if 'home' in live.snapshots else {}
    results = records.restore(console, plan, timeout_s=timeout, snapshots=snapshots) if plan['steps'] else []
    for result in results:
        if result['key'] == 'profile.start-speed-relation':
            continue
        if result['key'] == 'home.window':
            for key in records.SNAPSHOT_ROUTES['home']['keys']:
                run.field({'drive': owner, 'key': key, 'index': None},
                          baseline=(records.baseline_field(live.baseline, owner, key) or {}).get('value'),
                          ending=None if result['ending'] is None else result['ending'],
                          restored=result['restored'], uncertain=result['uncertain'], reason=result['reason'],
                          restoration={'window': 'home', 'before': result.get('before'), 'write': result.get('write')})
            continue
        run.field(result, ending=result['ending'], restored=result['restored'], uncertain=result['uncertain'],
                  reason=result['reason'], restoration={k: result.get(k) for k in ('before', 'written', 'write')})
    return results


def _outputs(console, args, record, run, baseline):
    timeout = args.timeout
    scenario = run.scenario
    cid = args.case
    owner = records.FIXTURE['y0_owner']
    _preflight(console, run, timeout)
    declarations = records.check_declarations(console, timeout_s=timeout)
    records.use_drive(console, owner, timeout_s=timeout)
    before = records.read_drive(console, owner, ('io', 'state', 'config'), select=False, timeout_s=timeout)
    diff = records.diff_fields(baseline, {owner: records.raws_from(before)}, ('io',))
    run.folder.write_json('baseline_before.json', {'declarations': declarations, 'drives': {str(owner): {
        k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')} for k, e in before.items()}},
        'diff': diff})
    plan = scenario['restoration_plan']
    if not plan['admissible'] or any(w['refusal'] for w in scenario['case_writes'] if
                                     not w['entry']['key'].startswith('profile.')):
        reasons = [r['reason'] for r in plan['refused']] + [w['refusal']['reason'] for w in scenario['case_writes']
                                                            if w['refusal'] and not w['entry']['key'].startswith('profile.')]
        for level in scenario['cases'][0]['required_levels']:
            run.verdict(cid, level, 'NOT RUN', 'no admissible route before any write: ' + '; '.join(reasons))
        return
    if declarations['verdict'] != 'PASS' or any(d['status'] != 'equal' for d in diff):
        raise probe.BenchError('Y0 owner not at the baseline I/O words or declarations disagree; no write')
    state = _state_summary(before['state'].get('reply') or {})
    live = bench_out.Live(console, run, baseline, args, scenario['facts'], timeout)
    emit = console.emit
    console.emit = live.tap(emit)
    detail = {'case': cid, 'declarations': declarations}
    error = None
    try:
        checked(console, 'bench', host_args=('clear',), timeout_s=timeout)
        live.view()
        detail['debug_before'] = checked(console, 'debug', host_args=(), timeout_s=timeout)
        if live.case['kind'] == 'homing':
            records.use_drive(console, owner, timeout_s=timeout)
            console.command('read-config', address=owner, timeout_s=timeout)
            snapshot = checked(console, 'home-read', address=owner, timeout_s=timeout)
            h = snapshot.get('homing_parameters') or {}
            live.snapshots['home'] = {'operation_id': snapshot['operation_id'], 'address': owner,
                                      'words': [h.get('raw_method'), h.get('raw_search_speed'), h.get('raw_return_speed'),
                                                h.get('raw_ramp_time'), *(h.get('raw_offset_words') or [None, None])],
                                      'baseline_words': records.snapshot_words(baseline, owner, 'home'), 'read': h}
            if live.snapshots['home']['words'] != live.snapshots['home']['baseline_words']:
                run.deviations.append('drive 4 homing words before the case %s differ from the baseline %s '
                                      '(left by an earlier run, RAM); the restore returns them to the pre-run words'
                                      % (live.snapshots['home']['words'], live.snapshots['home']['baseline_words']))
        bench_out.RUNNERS[live.case['kind']](live)
        detail['debug_after'] = checked(console, 'debug', host_args=(), timeout_s=timeout)
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append('case %s stopped: %s' % (cid, caught))
    finally:
        console.emit = emit
        if console.synchronized:
            try:
                # Restoration edges get their own recorded row, after every stimulus row.
                live.rows.append({'label': 'restoration', 'start_us': live.now_us(), 'expect': None})
                detail['restoration'] = _outputs_restore(console, live, run, plan, timeout)
            except (Exception, KeyboardInterrupt) as caught:
                run.deviations.append('restoration stopped: %s' % caught)
            live.views.append(checked(console, 'bench', host_args=(), timeout_s=timeout))
        writes = bench_out.wire_writes(live.traffic)
        edges, lost = records._merge_edges([records._bench_view(v) for v in live.views])
        rows = bench_out.split_rows(live.rows, live.views[-1]['now_us'] if live.views else 0)
        detail['analysis'] = bench_out.analyse(rows, edges, live.samples, writes, lost)
        detail.update(rows_input=rows, edges=edges, lost_edge_windows=lost, writes=writes, samples=live.samples,
                      observations=live.observations, events=live.events, snapshots=live.snapshots,
                      traffic_records=len(live.traffic), traffic_gaps=records._traffic_gaps(
                          [r for r in live.traffic if isinstance(r, dict) and type(r.get('sequence')) is int]))
        on_values = {'arrival': (('motion', bench_out.IN_POSITION), 1), 'homing': (('motion', bench_out.HOMED), 1),
                     'brake': (('motion', bench_out.RELEASED), 0), 'alarm': (('alarm',), 1),
                     'running': (('motion', bench_out.RUNNING), 1)}
        if live.case['kind'] in on_values:
            key, on = on_values[live.case['kind']]
            if live.case['polarity']:
                on = 1 - on
            # Edges of the Y0 function and polarity writes (setup, restoration)
            # are not drive transitions; they are listed, not paired.
            own = [r for r in detail['analysis']['rows'] if r['label'].startswith('setup io.') or r['label'] == 'restoration']
            function_edges = [e for r in own for e in r['edges']]
            stimulus_edges = [e for e in edges if e not in function_edges]
            detail['events'] = bench_out.pair_events(stimulus_edges, bench_out.transitions(live.samples, key), on)
            detail['events']['function_write_edges'] = function_edges
        run.folder.write_json('outputs.json', detail)
        run.folder.write_json('traffic.json', live.traffic)
    _outputs_verdicts(run, live, cid, detail, error)


def _outputs_verdicts(run, live, cid, detail, error):
    analysis = detail['analysis']
    kind = live.case['kind']
    levels = live.case['levels']
    evidence = ['outputs.json', 'traffic.json']
    if error is not None and not analysis['rows']:
        for level in levels:
            run.verdict(cid, level, 'NOT RUN', 'stopped before the stimulus: %s' % error)
        return
    failed, indeterminate = analysis['failed'], analysis['indeterminate']
    unattributed = analysis['unattributed']
    stopped = 'stopped early: %s' % error if error else None
    if kind in ('custom', 'running', 'brake', 'alarm'):
        problems = ['%s FAIL' % label for label in failed] + (['%d unattributed edges' % len(unattributed)] if unattributed else [])
        verdict = 'FAIL' if problems else 'indeterminate' if indeterminate or error else 'PASS'
        run.verdict(cid, 'controller-timestamped', verdict, '; '.join(problems + [stopped] if stopped else problems) or None, evidence)
    if kind == 'custom':
        obs = live.observations
        if live.case['polarity']:
            classes = {bench_out.mapping_class(o['custom'], 1, o['status_bit']) for o in obs}
            detail_text = 'mapping classes %s' % sorted(classes)
            run.verdict(cid, 'read back', 'PASS' if len(classes) == 1 and 'contradiction' not in classes else 'FAIL',
                        detail_text, evidence)
        else:
            ok = obs and all(o['status_bit'] == o['custom'] for o in obs)
            run.verdict(cid, 'read back', 'PASS' if ok else 'FAIL', None if ok else '0x0009 bit 0 did not follow every write', evidence)
    if kind == 'levels':
        run.verdict(cid, 'controller-timestamped', 'PASS' if len(live.observations) == 2 else 'NOT RUN',
                    'levels recorded: %s' % live.observations, evidence)
    if kind in ('running', 'arrival', 'homing', 'brake', 'alarm') and 'drive-reported' in levels:
        events = detail.get('events') or {}
        unpaired = len(events.get('unpaired_edges', [])) + len(events.get('unpaired_transitions', []))
        measured = analysis['summary']['drive_unmeasured'] == 0
        if kind in ('arrival', 'homing'):
            verdict = 'PASS' if events.get('pairs') and not unpaired and not error else 'indeterminate'
            run.verdict(cid, 'controller-timestamped', verdict,
                        '%d edges, %d paired, %d unpaired edges, %d unpaired transitions' % (
                            len(events.get('pairs', [])), len(events.get('pairs', [])) - len(events.get('unpaired_edges', [])),
                            len(events.get('unpaired_edges', [])), len(events.get('unpaired_transitions', []))), evidence)
            run.verdict(cid, 'drive-reported', verdict, None, evidence)
        else:
            run.verdict(cid, 'drive-reported', 'PASS' if measured and not unpaired else 'indeterminate',
                        None if measured and not unpaired else '%d unmeasured brackets, %d unpaired' % (
                            analysis['summary']['drive_unmeasured'], unpaired), evidence)


def _stop_write_list(args):
    case = bench_stop.CASES[args.case]
    owner = records.FIXTURE['y0_owner']
    out = [{'drive': a, 'key': 'io.x3', 'index': None, 'candidate': args.function}
           for a in bench_stop.phase_drives(case)]
    if case.get('y0') is not None:
        out.append({'drive': owner, 'key': 'io.y0', 'index': None, 'candidate': case['y0']})
    moving = [a for a in records.FIXTURE['input_drives']] if case['kind'] == 'group' else \
        [owner] if case['kind'] in ('move', 'absolute', 'width', 'race', 'serial') or args.case == 'S5' else []
    for address in moving:
        out += [{'drive': address, 'key': key, 'index': None, 'candidate': None} for key in bench_stop.PROFILE_KEYS]
    if case['kind'] == 'velocity':
        out += [{'drive': owner, 'key': key, 'index': None, 'candidate': None}
                for key in records.SNAPSHOT_ROUTES['velocity']['keys']]
    if case['kind'] == 'home':
        out += [{'drive': owner, 'key': key, 'index': None, 'candidate': None}
                for key in records.SNAPSHOT_ROUTES['home']['keys']]
    if args.case in ('S4', 'S5'):
        out.append({'drive': owner, 'key': 'homing.auxiliary', 'index': None, 'candidate': 7})
    return out


def _stop_plan(args, baseline, facts):
    case = bench_stop.CASES[args.case]
    write_list = _stop_write_list(args)
    snapshots = {}
    if case['kind'] == 'velocity':
        snapshots['velocity'] = {}
    if case['kind'] == 'home':
        snapshots['home'] = {}
    planned = [e for e in write_list if not (getattr(args, 'leave_profile', False) and e['key'].startswith('profile.'))]
    plan = records.restoration_plan(baseline, planned, args.image_level, facts,
                                    context={'snapshots': snapshots} if snapshots else None)
    cid = args.case
    effect = {4: 'release', 5: 'stop', 6: 'emergency stop'}[args.function]
    assertion = {
        'S1': 'X3 = %d (%s): %d relative 3000-step moves, mosfet2 200 ms at ~1 s; each stopped short, not reported '
              'complete, no start in the hold, the release and 2 s after (Implement 3 witnesses)',
        'S2': 'X3 = %d (%s): %d absolute moves of 3000 steps, mosfet2 200 ms at ~1 s; as S1',
        'S3': 'X3 = %d (%s): %d velocity runs 30 rpm 1000 ms, mosfet2 200 ms at ~0.4 s; classified per Implement 3',
        'S4': 'X3 = %d (%s): %d x home 33 and 34 with mosfet2 during the search; not reported homed',
        'S5': 'X3 = %d (%s), mosfet2 3000 ms: move, stage, velocity, home 35 and group moves refused before TX with the '
              'reason and zero traffic records (%d run)',
        'S6': 'X3 = %d (%s): read state with X3 inactive, mosfet2 3000 ms, a move on that read: start, ignore or '
              'start after release recorded (%d run)',
        'S7': 'X3 = %d (%s), deceleration 1000 ms: (a) stop normal then mosfet2, (b) mosfet2 then stop fast, '
              '(c) stop fast while held: output off_reason motor_stop (%d run each)',
        'S8': 'X3 = %d (%s): %d moves stopped at ~1 s by the stimulus at the stated deceleration; L1, L2, final position',
        'S9': 'X3 = %d (%s): 3000-step moves, mosfet2 widths %s ms, %d per width; stopped or not per width',
        'R1': 'X3 = %d (%s), Y0 = 5, stationary: mosfet2 2000 ms, %d runs; LR, released, re-enabled by itself or not',
        'R2a': 'X3 = %d (%s), Y0 = 5: %d 1000-step moves, mosfet2 200 ms at ~0.5 s; released sampled, move fails, LR',
        'R2b': 'X3 = %d (%s), Y0 = 2: %d 1000-step moves, mosfet2 200 ms at ~0.5 s; L1 for function 4, Implement 3',
        'R3': 'X3 = %d (%s): mosfet2 3000 ms, enable while held (%d run); decides Implement 4',
        'R2d': 'R2a diagnostic, X3 = %d (%s), Y0 = 5: %d 3000-step moves, mosfet2 1500 ms at ~0.5 s; whether the '
               'release acts after a delay and the move fails',
        'R1s': 'R2a diagnostic, X3 = %d (%s), Y0 = 5, stationary: mosfet2 200 ms, %d runs; whether the brake output '
               'returns without a release',
        'G1': 'phase B, X3 = %d (%s) on 1, 2, 4: %d staged group moves of 3000 steps, mosfet2 200 ms; every member '
              'stops short with its own outcome, no restart',
        'G2': 'phase B, X3 = %d (%s): %d sequential group move, mosfet2 during member 1; no later start sent',
    }[cid]
    if cid == 'S9':
        text = assertion % (args.function, effect, list(args.widths or case['widths']), args.repetitions)
    else:
        text = assertion % (args.function, effect, args.repetitions)
    if cid == 'S8':
        text += ' (deceleration %d ms, stimulus %s)' % (args.deceleration, args.stimulus)
    cases = [_case(cid, text, case['levels'])]
    commands = ['wiring of 1, 2, 4; io read, read config, read state of 1, 2, 4 (baseline words)', 'bench clear',
                'io set y0 %s 4 (ASSIGN y0 indicator either)' % case.get('y0'),
                'io set x3 %d A on %s (ASSIGN x3 triggers-inhibited protection-removal)' % (
                    args.function, ' '.join(map(str, bench_stop.phase_drives(case)))),
                'shared line check: io read 1, 2, 4 (X3 = F on phase drives, 0 elsewhere, polarity 0, no 7-13)',
                'per trial: read config, read state, motion-profile read, start-speed relation, io read, read config, '
                'read state, the operation, read state until the delay, interlock (named member, pre-start io read), '
                'bench on mosfet2 HOLD, read state through the hold + 2 s, bench views, terminal, stop fast (reconcile)',
                'restoration: stop fast, enable/alarm-clear if needed, plan steps (X3 0 with protection-removal, Y0 0, '
                'profile stage, velocity/home restore of the pre-run read)']
    bounds = {'repetitions': args.repetitions, 'moves_max_steps': 3000, 'speed_rpm': 60, 'velocity_rpm': 30,
              'velocity_ms': 1000, 'hold_ms_max': 5000, 'function': args.function}
    expected = {'controller-timestamped': 'mosfet2 on/off times, aux1 edges (Y0 2 running or 5 brake), the stop bracket '
                                          'from samples (stop_inputs.json)',
                'drive-reported': 'motion, input and position words of each sample; the operation terminal',
                'sent': 'refusal replies with reason and the debug traffic counter before and after'}
    expected = {level: expected[level] for level in case['levels']}
    return write_list, plan, cases, commands, bounds, expected


def window_values(name, words):
    """Per-field values of one restored snapshot window, in register order
    (a pair field such as the home offset takes two words)."""
    if not isinstance(words, list):
        return {}
    out, at = {}, 0
    for key in records.SNAPSHOT_ROUTES[name]['keys']:
        width = 2 if key == 'homing.offset' else 1
        if at + width > len(words):
            break
        out[key] = words[at] if width == 1 else list(words[at:at + width])
        at += width
    return out


def snapshot_steps_first(plan):
    """The plan with its home/velocity snapshot restores first. A snapshot is
    bound to the selection it was read in: restoring another drive's fields
    first selects that drive (`useaddr`) and makes the snapshot stale (wired
    bench 09 A11: `velocity restore` refused `unavailable` after the drive 1
    and 2 steps). Snapshot restores do not depend on the I/O steps."""
    steps = plan.get('steps') or []
    return dict(plan, steps=[s for s in steps if s.get('route') == 'snapshot'] +
                [s for s in steps if s.get('route') != 'snapshot'])


def _stop_restore(console, live, run, plan, timeout, owner=None):
    """Restoration per the prompt 07 step 2 table: prepared stops first, then
    enable/alarm-clear if needed, then the plan (X3, Y0, snapshots, profile).
    ``owner`` is the drive of the run's snapshots (default the Y0 owner)."""
    addresses = list(records.FIXTURE['input_drives'])
    for address in addresses:
        console.command('stop', stop_policy='fast', address=address, timeout_s=timeout)
    for address in addresses:
        state = console.command('read-state', address=address, timeout_s=timeout)
        summary = _state_summary(state)
        if not state.get('ok') or summary['running'] or summary['raw_speed']:
            run.deviations.append('drive %d not reported stopped before restoration' % address)
            probe.observe_stopped(console, address=address, timeout_s=timeout)
        if summary['raw_alarm'] or summary['alarm_flag']:
            console.command('alarm-clear', address=address, timeout_s=timeout)
            run.deviations.append('drive %d alarm %s cleared before restoration' % (address, summary['raw_alarm']))
        if summary['released']:
            console.command('enable', address=address, timeout_s=timeout)
            time.sleep(1.1)
    console.command('motion-profile', host_args=('forget',), timeout_s=timeout)
    snapshots = {k: v for k, v in live.snapshots.items() if k in ('home', 'velocity')}
    plan = snapshot_steps_first(plan)
    results = records.restore(console, plan, timeout_s=timeout, snapshots=snapshots) if plan['steps'] else []
    owner = records.FIXTURE['y0_owner'] if owner is None else owner
    for result in results:
        if result['key'] == 'profile.start-speed-relation':
            continue
        name = result['key'].split('.')[0] if result['key'].endswith('.window') else None
        if name in ('home', 'velocity'):
            endings = window_values(name, result['ending'])
            for key in records.SNAPSHOT_ROUTES[name]['keys']:
                field = records.baseline_field(live.baseline, owner, key) or {}
                ending = endings.get(key)
                run.field({'drive': owner, 'key': key, 'index': None}, baseline=field.get('value'),
                          ending=ending, restored=result['restored'] or (ending is not None and ending == field.get('value')),
                          uncertain=result['uncertain'], reason=result['reason'],
                          restoration={'window': name, 'before': result.get('before'), 'write': result.get('write')})
            continue
        run.field(result, ending=result['ending'], restored=result['restored'], uncertain=result['uncertain'],
                  reason=result['reason'], restoration={k: result.get(k) for k in ('before', 'written', 'write')})
    return results


def _stop_inputs(console, args, record, run, baseline):
    timeout = args.timeout
    scenario = run.scenario
    cid = args.case
    case = bench_stop.CASES[cid]
    _preflight(console, run, timeout)
    declarations = records.check_declarations(console, timeout_s=timeout)
    before = {a: records.read_drive(console, a, ('io', 'state', 'config'), select=False, timeout_s=timeout)
              for a in records.FIXTURE['input_drives']}
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, ('io',))
    filters = {a: (console.command('tuning', driver_args=('filters', 'read'), address=a, timeout_s=timeout).get('observation')
                   or {}).get('raw') for a in records.FIXTURE['input_drives']}
    debug = checked(console, 'debug', host_args=(), timeout_s=timeout)
    run.folder.write_json('baseline_before.json', {'declarations': declarations, 'filters': filters, 'debug': debug.get('mode'),
                                                   'drives': {str(a): {k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')}
                                                                       for k, e in r.items()} for a, r in before.items()},
                                                   'diff': diff})
    plan = scenario['restoration_plan']
    if not plan['admissible'] or any(w['refusal'] for w in scenario['case_writes']
                                     if not w['entry']['key'].startswith('profile.') and not records.snapshot_route(w['entry']['key'])):
        reasons = [r['reason'] for r in plan['refused']] + [w['refusal']['reason'] for w in scenario['case_writes'] if w['refusal']]
        for level in scenario['cases'][0]['required_levels']:
            run.verdict(cid, level, 'NOT RUN', 'no admissible route before any write: ' + '; '.join(reasons))
        return
    if declarations['verdict'] != 'PASS' or any(d['status'] != 'equal' for d in diff):
        raise probe.BenchError('I/O words differ from the baseline or declarations disagree; no write')
    live = bench_stop.Live(console, run, baseline, args, scenario['facts'], timeout)
    live.record['declarations'] = declarations
    detail = {'case': cid, 'function': args.function, 'declarations': declarations['verdict'], 'filters': filters,
              'debug_mode': debug.get('mode')}
    error = None
    try:
        checked(console, 'bench', host_args=('clear',), timeout_s=timeout)
        live.view()
        detail.update(bench_stop.run_case(live, args))
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append('case %s stopped: %s' % (cid, caught))
    finally:
        if console.synchronized:
            try:
                detail['restoration'] = _stop_restore(console, live, run, plan, timeout)
            except (Exception, KeyboardInterrupt) as caught:
                run.deviations.append('restoration stopped: %s' % caught)
        detail.update(events=live.events, snapshots=live.snapshots, views=len(live.views))
        found, lost = records._merge_edges([records._bench_view(v) for v in live.views if v.get('ok')])
        detail.update(edges=found, lost_edge_windows=lost)
        run.folder.write_json('stop_inputs.json', detail)
    for entry in scenario['left_changed']:
        run.field(entry, ending=None, restored=False, storage=entry['storage'], reason=entry['reason'])
    bench_stop.case_verdicts(run, cid, case, detail, error)


# --------------------------------------------------------------------------
# Wired bench 11: limit inputs (bench_limits.py)

def _limits_write_list(args):
    owner = records.FIXTURE['y0_owner']
    out = []
    kinds = {bench_lim.CASES[c]['kind'] for c in args.cases}
    if not args.polls_only:
        out.append({'drive': owner, 'key': 'io.y0', 'index': None, 'candidate': 2})
    overs = {args.over_limit if args.over_limit is not None else bench_lim.CASES[c]['over_limit'] for c in args.cases
             if bench_lim.CASES[c]['kind'] not in ('group', 'stationary')}
    if any(overs):
        out.append({'drive': owner, 'key': 'drive.over-limit', 'index': None, 'candidate': 1})
    moving = list(records.FIXTURE['input_drives']) if 'group' in kinds else \
        [owner] if kinds & {'move', 'recovery', 'held'} else []
    for address in moving:
        out += [{'drive': address, 'key': key, 'index': None, 'candidate': None} for key in bench_lim.PROFILE_KEYS]
    if 'velocity' in kinds or 'soft-velocity' in kinds:
        out += [{'drive': owner, 'key': key, 'index': None, 'candidate': None}
                for key in records.SNAPSHOT_ROUTES['velocity']['keys']]
    if kinds & {'soft', 'soft-velocity'}:
        out += [{'drive': owner, 'key': 'drive.positive-limit', 'index': None, 'candidate': [0, bench_lim.SOFT_LIMIT]},
                {'drive': owner, 'key': 'drive.negative-limit', 'index': None,
                 'candidate': [0xFFFF, (0x10000 - bench_lim.SOFT_LIMIT) & 0xFFFF]},
                {'drive': owner, 'key': 'drive.soft-limit', 'index': None, 'candidate': 1},
                {'drive': owner, 'key': 'homing.auxiliary', 'index': None, 'candidate': 7}]
        out += [{'drive': owner, 'key': key, 'index': None, 'candidate': None}
                for key in records.SNAPSHOT_ROUTES['home']['keys']]
        if 'soft' in kinds and not any(e['key'].startswith('profile.') for e in out):
            out += [{'drive': owner, 'key': key, 'index': None, 'candidate': None} for key in bench_lim.PROFILE_KEYS]
    return out


def _limits_plan(args, baseline, facts):
    write_list = _limits_write_list(args)
    kinds = {bench_lim.CASES[c]['kind'] for c in args.cases}
    planned = [e for e in write_list if not (args.leave_profile and e['key'].startswith('profile.'))]
    snapshots = {}
    if kinds & {'velocity', 'soft-velocity'}:
        snapshots['velocity'] = {}
    if kinds & {'soft', 'soft-velocity'}:
        snapshots['home'] = {}
    plan = records.restoration_plan(baseline, planned, args.image_level, facts,
                                    context={'snapshots': snapshots} if snapshots else None)
    cases = [_case(c, bench_lim.CASES[c]['assertion'] + (' (polls only)' if args.polls_only else ''),
                   [lvl for lvl in bench_lim.CASES[c]['levels'] if not (args.polls_only and lvl == 'controller-timestamped')])
             for c in args.cases]
    commands = ['wiring of 1, 2, 4; io read, read config, read state of 1, 2, 4 (baseline I/O words required)', 'bench clear',
                'no I/O write (polls only)' if args.polls_only else 'io set y0 2 4 (ASSIGN y0 indicator either)',
                'per case: driver set over-limit V 4 with CONFIGURE records when 0x0017 differs; read config',
                'per trial: read config, read state, motion-profile read, start-speed relation, io read, read config, '
                'read state, the operation, read state until the delay, interlock (named member, effect limit, pre-start '
                'io read), bench on relay2|relay1 HOLD, read state through the hold + 2 s, terminal, read state, stop fast '
                '(reconcile), recovery reads (alarm-clear, enable, release and enable only while needed)',
                'restoration: stop fast, enable/alarm-clear if needed, plan steps (Y0 0, 0x0017 0, profile stage, velocity '
                'restore of the pre-run read)']
    bounds = {'subdivision': 1000, 'move_steps_max': bench_lim.MOVE_STEPS, 'speed_rpm': 60, 'velocity_rpm': 60,
              'velocity_ms': 1000, 'relay_hold_ms_max': 5000, 'cases': list(args.cases), 'over_limit': args.over_limit}
    expected = {'drive-reported': 'motion, alarm, input and position words of each sample; terminals; recovery reads '
                                  '(limits.json)',
                'controller-timestamped': 'relay on/off times, aux1 edges of Y0 = 2, the stop bracket from samples'}
    return write_list, plan, cases, commands, bounds, expected


def _limits(console, args, record, run, baseline):
    timeout = args.timeout
    scenario = run.scenario
    _preflight(console, run, timeout)
    declarations = records.check_declarations(console, timeout_s=timeout)
    before = {a: records.read_drive(console, a, ('io', 'state', 'config'), select=False, timeout_s=timeout)
              for a in records.FIXTURE['input_drives']}
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, ('io',))
    run.folder.write_json('baseline_before.json', {'declarations': declarations,
                                                   'drives': {str(a): {k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')}
                                                                       for k, e in r.items()} for a, r in before.items()},
                                                   'diff': diff})
    plan = scenario['restoration_plan']
    if not plan['admissible'] or any(w['refusal'] for w in scenario['case_writes']):
        reasons = [r['reason'] for r in plan['refused']] + [w['refusal']['reason'] for w in scenario['case_writes'] if w['refusal']]
        for case in scenario['cases']:
            for level in case['required_levels']:
                run.verdict(case['id'], level, 'NOT RUN', 'no admissible route before any write: ' + '; '.join(reasons))
        return
    if declarations['verdict'] != 'PASS' or any(d['status'] != 'equal' for d in diff):
        raise probe.BenchError('I/O words differ from the baseline or declarations disagree; no write')
    live = bench_lim.Live(console, run, baseline, args, scenario['facts'], timeout, args.cases[0])
    live.record['declarations'] = declarations
    detail = {'cases_requested': list(args.cases), 'declarations': declarations['verdict']}
    error = None
    try:
        checked(console, 'bench', host_args=('clear',), timeout_s=timeout)
        live.view()
        detail.update(bench_lim.run_cases(live, args, args.cases))
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append('limits run stopped: %s' % caught)
        detail.setdefault('error', str(caught))
        detail.setdefault('cases', getattr(live, 'partial', {}))
    finally:
        if console.synchronized:
            try:
                detail['restoration'] = _stop_restore(console, live, run, plan, timeout)
            except (Exception, KeyboardInterrupt) as caught:
                run.deviations.append('restoration stopped: %s' % caught)
        detail.update(events=live.events, snapshots=live.snapshots, views=len(live.views),
                      interlock=live.record.get('interlock', []))
        found, lost = records._merge_edges([records._bench_view(v) for v in live.views if v.get('ok')])
        detail.update(edges=found, lost_edge_windows=lost)
        run.folder.write_json('limits.json', detail)
    for entry in scenario['left_changed']:
        run.field(entry, ending=None, restored=False, storage=entry['storage'], reason=entry['reason'])
    for cid in args.cases:
        bench_lim.case_verdicts(run, cid, detail, error)


# --------------------------------------------------------------------------
# Wired bench 12: switch homing with host-driven switches (bench_switch.py)

def _switch_write_list(args):
    address = args.address
    out = [{'drive': address, 'key': 'homing.auxiliary', 'index': None, 'candidate': args.auxiliary}]
    out += [{'drive': address, 'key': key, 'index': None, 'candidate': None}
            for key in records.SNAPSHOT_ROUTES['home']['keys']]
    if 'H12' in args.cases:
        out.append({'drive': address, 'key': 'io.y0', 'index': None, 'candidate': bench_sw.Y0_HOMED})
    if 'H13' in args.cases:
        out += [{'drive': address, 'key': 'drive.positive-limit', 'index': None, 'candidate': [0, bench_lim.SOFT_LIMIT]},
                {'drive': address, 'key': 'drive.negative-limit', 'index': None,
                 'candidate': [0xFFFF, (0x10000 - bench_lim.SOFT_LIMIT) & 0xFFFF]},
                {'drive': address, 'key': 'drive.soft-limit', 'index': None, 'candidate': 1}]
        out += [{'drive': address, 'key': key, 'index': None, 'candidate': None} for key in bench_lim.PROFILE_KEYS]
    return out


def _switch_plan(args, baseline, facts):
    write_list = _switch_write_list(args)
    plan = records.restoration_plan(baseline, write_list, args.image_level, facts, context={'snapshots': {'home': {}}})
    cases = [_case(c, bench_sw.CASES[c]['assertion'], bench_sw.CASES[c]['levels']) for c in args.cases]
    v_high, v_low = bench_sw.rate(args.search), bench_sw.rate(args.return_speed)
    commands = ['wiring of 1, 2, 4; io read, read config, read state of 1, 2, 4 (baseline I/O words required)', 'bench clear',
                'per case: home read A; home auxiliary N (CONFIGURE) when the stored choice differs; read back',
                'per trial: read config, read state (start s), geometry relative to s and its preflight; interlocked '
                'bench on of an initially active switch; read config, read state, home read, home qualification '
                'install M H L 100 facts; io read, read config, read state; home M H L 100 zero|OFFSET; read state '
                'loop: level change -> bench off OUT, or the interlock (named member, effect origin|limit, pre-start '
                'io read bounded by the home deadline) and bench on OUT HOLD; renewals; guard -> stop fast',
                'after the terminal: three read state, bench off, read state until stopped',
                'restoration: stop fast on 1, 2, 4, then home restore of the first home read and home auxiliary 0']
    bounds = {'subdivision': 1000, 'search_word': args.search, 'return_word': args.return_speed, 'ramp_word': 100,
              'rate_high': v_high, 'rate_low': v_low, 'guard_increments': bench_sw.GUARD,
              'window': bench_sw.WINDOW, 'approach': bench_sw.APPROACH, 'limit_gap': bench_sw.LIMIT_GAP,
              'budget_gap_ms': args.budget_gap_ms, 'budget_decision_ms': args.budget_decision_ms,
              'edge_ms': bench_sw.D_EDGE_MS, 'hold_ms': int(3 * (args.budget_gap_ms + args.budget_decision_ms)),
              'home_deadline_ms': 30000, 'auxiliary': args.auxiliary, 'cases': list(args.cases),
              'exposure': 'no drive-side stop: a harness or link failure leaves a search running until the controller, '
                          'the link or the owner recovers (D11); every trial ends with fresh stopped state'}
    expected = {'drive-reported': 'positions, motion and input words of each sample; the home terminal; the path, '
                                  'final position and end coordinate per trial (switch.json)',
                'controller-timestamped': 'GPIO on/off times, the bound of every emulated edge, gaps and delays'}
    return write_list, plan, cases, commands, bounds, expected


def _switch(console, args, record, run, baseline):
    timeout = args.timeout
    scenario = run.scenario
    _preflight(console, run, timeout)
    declarations = records.check_declarations(console, timeout_s=timeout)
    before = {a: records.read_drive(console, a, ('io', 'state', 'config'), select=False, timeout_s=timeout)
              for a in records.FIXTURE['input_drives']}
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, ('io',))
    run.folder.write_json('baseline_before.json', {'declarations': declarations,
                                                   'drives': {str(a): {k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')}
                                                                       for k, e in r.items()} for a, r in before.items()},
                                                   'diff': diff})
    plan = scenario['restoration_plan']
    if not plan['admissible'] or any(w['refusal'] for w in scenario['case_writes']):
        reasons = [r['reason'] for r in plan['refused']] + [w['refusal']['reason'] for w in scenario['case_writes'] if w['refusal']]
        for case in scenario['cases']:
            for level in case['required_levels']:
                run.verdict(case['id'], level, 'NOT RUN', 'no admissible route before any write: ' + '; '.join(reasons))
        return
    if declarations['verdict'] != 'PASS' or any(d['status'] != 'equal' for d in diff):
        raise probe.BenchError('I/O words differ from the baseline or declarations disagree; no write')
    live = bench_sw.Live(console, run, baseline, args, scenario['facts'], timeout, args.address)
    live.record['declarations'] = declarations
    detail = {'cases_requested': list(args.cases), 'declarations': declarations['verdict']}
    error = None
    try:
        checked(console, 'bench', host_args=('clear',), timeout_s=timeout)
        live.view()
        detail.update(bench_sw.run_cases(live, args, args.cases))
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append('switch run stopped: %s' % caught)
        detail.setdefault('error', str(caught))
        detail.setdefault('cases', getattr(live, 'partial', {}))
    finally:
        if console.synchronized:
            try:
                detail['restoration'] = _stop_restore(console, live, run, plan, timeout, owner=args.address)
            except (Exception, KeyboardInterrupt) as caught:
                run.deviations.append('restoration stopped: %s' % caught)
        detail.update(events=live.events, snapshots=live.snapshots, views=len(live.views),
                      interlock=live.record.get('interlock', []))
        found, lost = records._merge_edges([records._bench_view(v) for v in live.views if v.get('ok')])
        detail.update(edges=found, lost_edge_windows=lost)
        run.folder.write_json('switch.json', detail)
    for cid in args.cases:
        bench_sw.case_verdicts(run, cid, detail, error)


def _pt(console, args, record, run, baseline):
    """Wired bench 13: PT records through PIN selection (bench_pt.py)."""
    timeout = args.timeout
    scenario = run.scenario
    _preflight(console, run, timeout)
    declarations = records.check_declarations(console, timeout_s=timeout)
    before = {a: records.read_drive(console, a, ('io', 'state', 'config'), select=False, timeout_s=timeout)
              for a in records.FIXTURE['input_drives']}
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, ('io',))
    run.folder.write_json('baseline_before.json', {'declarations': declarations,
                                                   'drives': {str(a): {k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')}
                                                                       for k, e in r.items()} for a, r in before.items()},
                                                   'diff': diff})
    plan = scenario['restoration_plan']
    if not plan['admissible'] or any(w['refusal'] for w in scenario['case_writes']):
        reasons = [r['reason'] for r in plan['refused']] + [w['refusal']['reason'] for w in scenario['case_writes'] if w['refusal']]
        for case in scenario['cases']:
            for level in case['required_levels']:
                run.verdict(case['id'], level, 'NOT RUN', 'no admissible route before any write: ' + '; '.join(reasons))
        return
    if declarations['verdict'] != 'PASS' or any(d['status'] != 'equal' for d in diff):
        raise probe.BenchError('I/O words differ from the baseline or declarations disagree; no write')
    live = bench_pt.Live(console, run, baseline, args, timeout)
    live.record['declarations'] = declarations
    detail = {'cases_requested': list(args.cases), 'declarations': declarations['verdict']}
    error = None
    try:
        checked(console, 'bench', host_args=('clear',), timeout_s=timeout)
        live.view()
        detail.update(bench_pt.run_cases(live, args, args.cases))
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append('pt-segments run stopped: %s' % caught)
        detail.setdefault('error', str(caught))
        detail.setdefault('cases', getattr(live, 'partial', {}))
    finally:
        if console.synchronized:
            try:
                for address in records.FIXTURE['input_drives']:
                    console.command('stop', stop_policy='fast', address=address, timeout_s=timeout)
                console.command('group-clear', timeout_s=timeout)
                for address in sorted({e['drive'] for e in scenario['write_list']}):
                    bench_pt.restore_layout(live, address)
                detail['restoration'] = _stop_restore(console, live, run, plan, timeout, owner=args.address)
            except (Exception, KeyboardInterrupt) as caught:
                run.deviations.append('restoration stopped: %s' % caught)
        detail.update(events=live.events, snapshots=live.snapshots, views=len(live.views),
                      interlock=live.record.get('interlock', []), samples=live.samples)
        run.folder.write_json('pt.json', detail)
    for cid in args.cases:
        bench_pt.case_verdicts(run, cid, detail, error)


def _velocity(console, args, record, run, baseline):
    """Wired bench 14: velocity limits, JOG, speed inputs, PV records, phase C cells (bench_velocity.py)."""
    timeout = args.timeout
    scenario = run.scenario
    _preflight(console, run, timeout)
    declarations = records.check_declarations(console, timeout_s=timeout)
    before = {a: records.read_drive(console, a, ('io', 'state', 'config'), select=False, timeout_s=timeout)
              for a in records.FIXTURE['input_drives']}
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, ('io',))
    run.folder.write_json('baseline_before.json', {'declarations': declarations,
                                                   'drives': {str(a): {k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')}
                                                                       for k, e in r.items()} for a, r in before.items()},
                                                   'diff': diff})
    plan = scenario['restoration_plan']
    if not plan['admissible'] or any(w['refusal'] for w in scenario['case_writes']):
        reasons = [r['reason'] for r in plan['refused']] + [w['refusal']['reason'] for w in scenario['case_writes'] if w['refusal']]
        for case in scenario['cases']:
            for level in case['required_levels']:
                run.verdict(case['id'], level, 'NOT RUN', 'no admissible route before any write: ' + '; '.join(reasons))
        return
    if declarations['verdict'] != 'PASS' or any(d['status'] != 'equal' for d in diff):
        raise probe.BenchError('I/O words differ from the baseline or declarations disagree; no write')
    live = bench_velocity.Live(console, run, baseline, args, timeout)
    live.record['declarations'] = declarations
    detail = {'cases_requested': list(args.cases), 'declarations': declarations['verdict']}
    error = None
    try:
        checked(console, 'bench', host_args=('clear',), timeout_s=timeout)
        live.view()
        detail.update(bench_velocity.run_cases(live, args, args.cases))
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append('velocity-jog-pv run stopped: %s' % caught)
        detail.setdefault('error', str(caught))
        detail.setdefault('cases', getattr(live, 'partial', {}))
    finally:
        if console.synchronized:
            try:
                console.command('stop', stop_policy='fast', address=args.address, timeout_s=timeout)
                console.command('velocity-limits', host_args=('reset',), timeout_s=timeout)
                bench_velocity.restore_layout(live)
                detail['restoration'] = _stop_restore(console, live, run, plan, timeout, owner=args.address)
            except (Exception, KeyboardInterrupt) as caught:
                run.deviations.append('restoration stopped: %s' % caught)
        detail.update(events=live.events, snapshots=live.snapshots, views=len(live.views),
                      interlock=live.record.get('interlock', []), samples=live.samples,
                      relay_operations=live.relay_operations)
        run.folder.write_json('velocity.json', detail)
    for cid in args.cases:
        bench_velocity.case_verdicts(run, cid, detail, error)


# --------------------------------------------------------------------------
# Wired bench 08: input-start route (arm, one owned pulse, observe, disarm)

def _inspect_until(console, session, done, limit_s, timeout):
    """Poll ``arm-inspect`` until done(session) or the bound; the last reply."""
    deadline = console.clock() + limit_s
    while True:
        reply = checked(console, 'arm-inspect', host_args=(str(session),), timeout_s=timeout)
        if done(reply['session']) or console.clock() >= deadline:
            return reply
        console.sleep(0.05)


def _input_start(console, args, record, run, baseline):
    """One function 7 start on the selected drive: the documented fresh reads,
    the FINITE_MOVE record with triggers inhibited, arm, one owned pulse through
    the firmware's own isolation decision, observation to its conclusion,
    disarm, restoration of the case write."""
    timeout = args.timeout
    cid = args.case or args.scenario
    scenario = run.scenario
    view = _preflight(console, run, timeout)
    address, terminal = args.address, args.start_terminal
    before = _read_drives(console, [address], timeout)
    diff = records.diff_fields(baseline, {address: records.raws_from(before[address])})
    run.folder.write_json('baseline_before.json', {'bench': records._bench_view(view), 'drives': _compact(before),
                                                   'diff': [d for d in diff if d['status'] != 'equal']})
    plan = scenario['restoration_plan']
    write = (scenario['case_writes'] or [None])[0]
    detail = {'session': None}
    session = None
    try:
        if write is not None:
            if write['refusal'] or not plan['admissible']:
                reason = '; '.join([r['reason'] for r in plan['refused']] + ([write['refusal']['reason']] if write['refusal'] else []))
                for level in scenario['cases'][0]['required_levels']:
                    run.verdict(cid, level, 'NOT RUN', 'no admissible route before any write: ' + reason)
                return
            outcome, ending = _case_write(console, run, write['entry'], write['steps'], timeout)
            detail['case_write'] = {'entry': write['entry'], 'readback': ending, 'result': outcome['result']}
        records.use_drive(console, address, timeout_s=timeout)
        # The documented fresh sequence after the last write: config, DRIVE (0x0050), profile, state.
        # The profile read keeps its session open (wired bench 09: records.read_group's read-and-forget
        # left the move readiness unbound and the arm refused move_readiness on the bench).
        detail['fresh_reads'] = bench_starts.reference_reads(console, address, timeout)
        detail['record'] = _install(console, (str(address), terminal, '7', '0', 'finite-move', args.start_mapping,
                                              'triggers-inhibited'), timeout)
        arm_args = (terminal, args.arm_form, str(args.steps), str(args.rpm), 'poll-ms', str(args.poll_ms),
                    'window-ms', str(args.window_ms))
        arm = console.command('arm-position', host_args=arm_args, timeout_s=timeout)
        detail['arm'] = arm
        if not arm.get('ok'):
            run.verdict(cid, 'local', 'FAIL', 'arm refused: %s (%s)' % (arm.get('result'), arm.get('reason')))
            return
        session = arm['session']['operation_id']
        armed = _inspect_until(console, session, lambda s: s['phase'] != 'staging', 5.0, timeout)
        detail['armed'] = armed
        if armed['session']['phase'] != 'armed':
            run.verdict(cid, 'local', 'FAIL', 'session %s: %s' % (armed['session']['phase'], armed['session']['error']))
            return
        run.verdict(cid, 'local', 'PASS')
        trigger = console.command('arm-trigger', host_args=(str(session), args.trigger_output, str(args.width_ms)),
                                  timeout_s=timeout)
        detail['trigger'] = trigger
        decided = _inspect_until(console, session, lambda s: s['phase'] != 'triggering', 5.0, timeout)
        isolation = decided['session']['isolation']
        detail['decided'] = decided
        if not trigger.get('ok') or not isolation['admitted']:
            run.verdict(cid, 'controller-timestamped', 'FAIL', 'trigger refused: %s %s %s' % (
                trigger.get('reason'), isolation.get('row'), isolation.get('reason')))
            return
        source = decided['session']['source']
        run.verdict(cid, 'controller-timestamped', 'PASS' if source['triggered'] and source['on_us'] else 'FAIL',
                    'isolation rows %s; ON at %s us (GPIO write)' % ([r['row'] for r in isolation['rows']], source['on_us']))
        final = _inspect_until(console, session, lambda s: s['phase'] == 'done', args.window_ms / 1000.0 + 5.0, timeout)
        detail['final'] = final
        member = final['session']['members'][0]['result']
        outcome = member.get('outcome') if member.get('known') else 'none'
        verdict = 'PASS' if outcome == 'attributed' else 'indeterminate' if outcome == 'indeterminate' else 'FAIL'
        run.verdict(cid, 'drive-reported', verdict, 'outcome %s, missing %s, change %s of %s, spacing %s' % (
            outcome, member.get('missing'), member.get('position_change'), member.get('expected_change'),
            member.get('spacing_us')), ['input_start.json'])
    finally:
        if session is not None:
            last = console.command('arm-inspect', host_args=(str(session),), timeout_s=timeout)
            if last.get('ok') and last['session']['phase'] != 'done':
                detail['disarm'] = console.command('disarm', host_args=(str(session),), timeout_s=timeout)
            detail['release'] = console.command('release', operation_id=session, timeout_s=timeout)
        detail['results'] = _restore_written(console, run, plan, timeout)
        run.folder.write_json('input_start.json', detail)


# --------------------------------------------------------------------------
# Wired bench 09: input-started and serially interrupted moves

STARTS_ASSERTIONS = {
    'A1': 'NA-abs: 20 x (arm absolute, targets alternating P0+2000 and P0; arm trigger mosfet1 50 ms; disarm): every '
          'start attributed, endpoint reached, no further start; D = aux1 running-on - mosfet1 on_us',
    'A2': 'NA-rel: 10 x (arm relative +500; trigger 50 ms): each change +500 as reported, no tolerance',
    'A3': 'NA-rel: +200 staged (arm, disarm untriggered); unarmed mosfet1 pulses W in {5, 10, 12, 15, 20, 30, 50, 200} '
          'ms, 3 per W, named start member (D13): starts per W from the +200 steps; shortest W with 3/3 single starts',
    'A4': 'NA-rel: arm +500; trigger 2000 ms: starts during the hold and after the falling edge (report only)',
    'A5': 'NA-abs: arm P0+1000; trigger 2000 ms: as A4; a second start at a reached target is visible only as RUNNING '
          'or a Y0 edge (report only)',
    'A6': 'NA-abs then NA-rel: per policy (stop normal, stop fast) 3 fresh 3000-step input-started moves, moving '
          'witness, the serial stop at ~1 s: stopped short, no start in the window',
    'A6d': 'diagnostic, NA-abs: 2 input-started 3000-step moves per policy (stop fast, stop normal at ~1 s); back-to-back '
           'read state of drive 4 for 1.5 s from the stop: RUNNING, speed word and position after the stop',
    'A7': 'NA-X3 (X3 = 5 on drive 4): 5 input-started 3000-step moves, moving witness, mosfet2 200 ms at ~1 s (session '
          'member named): stopped short as 07 classifies, no start in the window',
    'A8': 'NA-rel: arm +3000; trigger 3000 ms; moving witness; stop fast at ~1 s: the stop quiesces the trigger '
          '(off_reason motor_stop) and no start follows',
    'A9': 'NA-abs: disarm before the trigger, then arm trigger: refused with no port call; disarm while moving: the '
          'move ends at its target',
    'A10': 'NA-abs: arm P0+1000, disarm untriggered, unarmed mosfet1 50 ms (deliberate, D13): reaches P0+1000; stage '
           'P0, io set x0 0, pulse again: the drive stays; cleanup X0 = 7',
    'A11': 'NA-rel: +300 staged, velocity 30 rpm 1000 ms, unarmed mosfet1 50 ms at ~300 ms (named running member, '
           'fixed-length), 3 runs: end position from the edge and the velocity result (p29 method 1)',
    'B1': 'NB: 50 x (arm group absolute-by +2000 / -2000; trigger 50 ms; disarm): every member attributed once; '
          'first-change spread as a distribution',
    'B2': 'NB: input-started group moves of 3000 steps; group stop normal (5), group stop fast (5) at ~1 s: every '
          'member stops short, no start in the window',
    'B2d': 'diagnostic, NB: 2 input-started group moves per policy (group stop fast, normal at ~1 s); back-to-back '
           'read state of drives 1 and 2 for 2 s from the group stop: RUNNING, speed word and position after the stop',
    'B3': 'NB-X3 (X3 = 5 on 1, 2, 4): input-started group move, mosfet2 200 ms at ~1 s (three session members named), '
          '3 runs: all stop',
    'I1': 'NC: relative 3000 at 60 rpm, position interrupt at ~1 s, 10 runs (alternating sign): final position against '
          'start + 3000 and against interrupt position + 3000, Y0 running edges',
    'I2': 'NC: absolute T = P0 +/- 3000, interrupt at ~1 s, 5 runs: final position T',
    'I3': 'NC: relative 1000, interrupt near the end (0.85, 0.95, 1.05 s), 3 runs: each outcome recorded, no replay',
    'I4': 'NC-Y4 (Y0 = 4 arrival): I1 and I2 forms, 5 runs each: whether ARRIVED separates replacement from original',
    'R': 'short regression, baseline I/O: identity, config and state reads of 1, 2, 4; per drive one serial relative '
         '+1000 move and stop fast at ~0.5 s (stopped, observed); one staged group move of 1, 2, 4',
    'Z': 'restoration only, baseline I/O: no motion; the run restores the 0x0021-0x0025 words of 1, 2, 4 '
         '(the last run of a controller session)',
    'E1': 'diagnostic, NC: 6 plain serial absolute moves of +/-2000 at 60 rpm per drive 1, 2, 4: final feedback after '
          'completion and 1 s at rest against the target (separates a drive endpoint offset from the input start)',
}


def _starts_plan(args, baseline, facts):
    cases = list(args.cases)
    write_list = bench_starts.write_list(cases)
    planned = [e for e in write_list if not (args.leave_profile and e['key'].startswith('profile.'))]
    snapshots = {'velocity': {}} if any(bench_starts.CASES[c]['kind'] == 'fixed-length' for c in cases) else None
    plan = records.restoration_plan(baseline, planned, args.image_level, facts,
                                    context={'snapshots': snapshots} if snapshots else None)
    cases_out = [_case(c, STARTS_ASSERTIONS[c], bench_starts.CASES[c]['levels']) for c in cases]
    commands = ['wiring and declarations; io read, read config, read state, driver read of 1, 2, 4 (baseline_before)',
                'per state: ordinary writes with stated facts in order (function 7 removed first, X3, Y0, 0x0050, '
                'function 7 last), each read back; shared line check (io read 1, 2, 4 against the state)',
                'per armed trial: read config, driver read, motion-profile read (session kept), start-speed relation, '
                'read state, io qualification install A x0 7 0 finite-move unknown triggers-inhibited (members: '
                'driver read, member installs, group read state); arm position; line check of non-members; '
                'arm trigger ID mosfet1 W; arm inspect and bench views until done; disarm/release; read state',
                'unarmed pulses: bench_records.bench_on with the named start, running or session member (D14 interlock)',
                'interrupt trials: move relative|absolute, read state, position interrupt ORIGINAL at the delay, read '
                'state to rest and 2 s after, stop fast (settles the possible-motion interlock)',
                'restoration: stop fast on 1, 2, 4, group clear, plan steps (X0 7 -> baseline first, X3, Y0, 0x0050, '
                'velocity restore, profile stage unless left)']
    bounds = {'moves_max_steps': bench_starts.MAX_STEPS, 'speed_rpm': bench_starts.FINITE_RPM, 'velocity_rpm': 30,
              'velocity_ms': 1000, 'hold_ms_max': bench_starts.MAX_HOLD_MS, 'poll_ms': 50, 'cases': cases,
              'repetitions_override': args.repetitions}
    expected = {'controller-timestamped': 'mosfet1/mosfet2 GPIO on/off times, aux1 edges of drive 4 Y0 (2 running, 4 '
                                          'arrival), the session isolation rows and read ages',
                'drive-reported': 'session results (outcome, first-change bracket, position change, spacing) and read '
                                  'state samples; move and interrupt terminals',
                'sent': 'refusal replies with bus_traffic and the mosfet1 ons count before and after',
                'read back': 'the restoration of every written field, each read back (result.json restoration)'}
    levels = sorted({lv for c in cases for lv in bench_starts.CASES[c]['levels']})
    return write_list, plan, cases_out, commands, bounds, {lv: expected[lv] for lv in levels}


def _input_starts(console, args, record, run, baseline):
    timeout = args.timeout
    scenario = run.scenario
    _preflight(console, run, timeout)
    declarations = records.check_declarations(console, timeout_s=timeout)
    before = {a: records.read_drive(console, a, ('io', 'state', 'config', 'driver'), select=False, timeout_s=timeout)
              for a in records.FIXTURE['input_drives']}
    diff = records.diff_fields(baseline, {a: records.raws_from(r) for a, r in before.items()}, ('io', 'driver'))
    filters = {a: (console.command('tuning', driver_args=('filters', 'read'), address=a, timeout_s=timeout).get('observation')
                   or {}).get('raw') for a in records.FIXTURE['input_drives']}
    debug = checked(console, 'debug', host_args=(), timeout_s=timeout)
    run.folder.write_json('baseline_before.json', {'declarations': declarations, 'filters': filters, 'debug': debug.get('mode'),
                                                   'drives': {str(a): {k: {x: e.get(x) for x in ('command', 'operation_id', 'raw', 'error')}
                                                                       for k, e in r.items()} for a, r in before.items()},
                                                   'diff': diff})
    plan = scenario['restoration_plan']
    blocked = [w for w in scenario['case_writes'] if w['refusal']]
    if not plan['admissible'] or blocked:
        reasons = [r['reason'] for r in plan['refused']] + [w['refusal']['reason'] for w in blocked]
        for case in scenario['cases']:
            for level in case['required_levels']:
                run.verdict(case['id'], level, 'NOT RUN', 'no admissible route before any write: ' + '; '.join(reasons))
        return
    if declarations['verdict'] != 'PASS' or any(d['status'] != 'equal' for d in diff):
        raise probe.BenchError('I/O or drive words differ from the baseline or declarations disagree; no write: %s'
                               % [d for d in diff if d['status'] != 'equal'])
    live = bench_starts.Live(console, run, baseline, args, timeout)
    live.record['declarations'] = declarations
    detail = {'cases': {}, 'declarations': declarations['verdict'], 'filters': filters, 'debug_mode': debug.get('mode')}
    errors = {}
    try:
        checked(console, 'bench', host_args=('clear',), timeout_s=timeout)
        live.view()
        for cid in args.cases:
            try:
                detail['cases'][cid] = bench_starts.run_case(live, cid)
            except (Exception, KeyboardInterrupt) as caught:
                errors[cid] = caught
                run.deviations.append('case %s stopped: %s' % (cid, caught))
                detail['cases'].setdefault(cid, {})['error'] = str(caught)
                break
    finally:
        if console.synchronized:
            try:
                records.outputs_off(console, timeout_s=timeout)
                if live.group:
                    console.command('group-clear', timeout_s=timeout)
                detail['restoration'] = _stop_restore(console, live, run, plan, timeout)
            except (Exception, KeyboardInterrupt) as caught:
                run.deviations.append('restoration stopped: %s' % caught)
        detail.update(events=live.events, snapshots=live.snapshots, views=len(live.views))
        found, lost = live.edges()
        detail.update(edges=found, lost_edge_windows=lost)
        run.folder.write_json('input_starts.json', detail)
    for entry in scenario['left_changed']:
        run.field(entry, ending=None, restored=False, storage=entry['storage'], reason=entry['reason'])
    for cid in args.cases:
        error = errors.get(cid)
        for level, (verdict, reason) in bench_starts.case_verdicts(cid, detail['cases'].get(cid, {}), error).items():
            if cid not in detail['cases']:
                verdict, reason = 'NOT RUN', 'not reached: an earlier case stopped the run'
            elif error and verdict == 'PASS':
                verdict, reason = 'indeterminate', 'stopped: %s' % error
            run.verdict(cid, level, verdict, reason, ['input_starts.json'])


FOLDER_RUNNERS = {'input-start': _input_start, 'input-start-single': _input_starts, 'input-start-shared': _input_starts,
                  'position-interrupt': _input_starts, 'input-map': _input_map, 'y0-map': _y0_map, 'baseline-compare': _baseline_compare,
                  'baseline-capture': _baseline_capture, 'restore': _restore, 'status-mapping': _status_mapping,
                  'input-timing': _input_timing, 'outputs': _outputs, 'stop-inputs': _stop_inputs, 'limits': _limits,
                  'switch': _switch, 'pt-segments': _pt, 'velocity-jog-pv': _velocity}


def run_folder(args, *, port_opener=None, check_board=None):
    """Plan, record, run and check one wired-bench scenario in its own run folder."""
    folder = records.RunFolder.from_path(args.run_dir) if args.run_dir else None
    baseline = records.load_baseline(args.baseline)
    image = records.image_record(args.firmware)
    commit = args.commit or records.source_commit()
    plan = folder_plan(args, baseline)
    fields = dict(image=image, commit=commit, port=args.port, **plan)
    print(json.dumps({'scenario': args.scenario, 'effect': SCENARIOS[args.scenario], 'record': fields},
                     indent=1, default=str), flush=True)
    if args.plan_only:
        return 0
    (check_board or records.check_board)(args.port)
    folder.create()
    scenario = records.write_scenario(folder, **json.loads(json.dumps(fields)))
    run = _Run(scenario, folder)
    closed = {'value': False}

    @contextmanager
    def opener(name, baud, timeout_s):
        connection = (port_opener or probe.open_port)(name, baud, timeout_s)
        try:
            yield connection
        finally:
            connection.close()
            closed['value'] = True

    def work(console, record):
        try:
            FOLDER_RUNNERS[args.scenario](console, args, record, run, baseline)
        finally:
            _finish(console, run, args.timeout)
        record['workload_verified'] = True
    error = None
    try:
        run_recorded(port=args.port, scenario=args.scenario, inputs={'run': folder.name, 'case': args.case},
                     run=work, address=args.address, timeout_s=args.timeout,
                     debug='decoded' if args.scenario == 'outputs' else
                     'raw' if args.scenario == 'y0-map' or (args.scenario == 'input-timing' and args.traffic == 'raw')
                     or (args.scenario == 'status-mapping' and args.mapping_cases == 'record') else None,
                     firmware=args.firmware,
                     port_opener=opener, max_records=args.evidence_records,
                     max_bytes=args.evidence_mib * 1024 * 1024,
                     paths=dict(summary=folder / 'summary.json', jsonl=folder / 'session.jsonl',
                                console=folder / 'console.log'))
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append('scenario stopped: ' + (str(caught) or type(caught).__name__))
    for name, label in (('baseline_before.json', 'before reads'), ('ending_reads.json', 'ending reads')):
        if not (folder / name).exists():
            folder.write_json(name, {'recorded': False, 'reason': 'scenario stopped before its %s: %s' % (
                label, (str(error) or type(error).__name__) if error else 'not reached')})
    cases = []
    for case in scenario['cases']:
        state = run.cases[case['id']]
        for level in case['required_levels']:
            if level not in state['verdicts']:
                run.verdict(case['id'], level, 'NOT RUN', 'not reached: ' + (str(error) if error else 'no verdict'))
        evidence = [p for p in state['evidence'] if (folder / p).exists()]
        cases.append({**case, 'verdicts': state['verdicts'], 'reason': state['reason'], 'evidence': evidence})
    for entry in scenario['write_list']:
        if (entry['drive'], entry['key'], entry.get('index')) not in run.fields:
            run.field(entry, baseline=(records.baseline_field(baseline, entry['drive'], entry['key'],
                                                              entry.get('index')) or {}).get('value'),
                      ending=(records.baseline_field(baseline, entry['drive'], entry['key'],
                                                     entry.get('index')) or {}).get('value'),
                      restored=True, reason='not written')
    records.write_result(folder, commit=commit, image=image, cases=cases,
                         restoration={'outputs_off': run.outputs_off, 'fields': list(run.fields.values()),
                                      'drives': run.drives, 'port_closed': closed['value']},
                         deviations=run.deviations)
    report = records.check_run(folder.path)
    print(json.dumps(report, indent=1))
    return records.check_exit(report)


def main(argv=None):
    # Wired bench 10: the paired-values scenario (bench_pairs.py) has its own
    # argument set and run folder handling; delegate before parsing.
    tokens = list(__import__('sys').argv[1:] if argv is None else argv)
    if '--scenario' in tokens and tokens[tokens.index('--scenario') + 1:tokens.index('--scenario') + 2] == ['paired-values']:
        import bench_pairs
        index = tokens.index('--scenario')
        return bench_pairs.main(tokens[:index] + tokens[index + 2:])
    try:
        args = arguments(argv)
        if args.list:
            print(json.dumps(SCENARIOS, indent=2)); return 0
        if args.scenario in FOLDER_SCENARIOS:
            return run_folder(args)
        inputs = {key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()}
        print(json.dumps({'scenario': args.scenario, 'effect': SCENARIOS[args.scenario], 'inputs': inputs}, indent=2), flush=True)
        if args.plan_only: return 0
        run_recorded(port=args.port, out=args.out, scenario=args.scenario, inputs=inputs,
                     run=lambda console, record: run_scenario(console, args, record),
                     address=args.address, timeout_s=args.timeout, debug=args.debug,
                     firmware=args.firmware, refresh=args.scenario != 'discovery',
                     max_records=args.evidence_records, max_bytes=args.evidence_mib * 1024 * 1024)
    except (Exception, KeyboardInterrupt) as error:
        print('Scenario FAILED: ' + (str(error) or 'interrupted')); return 1
    print('Scenario completed; exact evidence: ' + str(args.out)); return 0


if __name__ == '__main__':
    raise SystemExit(main())
