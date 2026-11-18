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
import re
import time

import bench_motion as motion
import bench_probe as probe
import bench_records as records
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
}

SCENARIOS = {
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
    parser.add_argument('--max-age-ms', type=int, default=records.MAX_AGE_MS,
                        help='input-map: interlock read age bound, recorded in the scenario record')
    parser.add_argument('--cycles', type=int, default=5, help='y0-map: custom on/off cycles (1..20)')
    parser.add_argument('--phase-s', type=float, default=3.0, help='y0-map: phase length in seconds (0.5..10)')
    parser.add_argument('--change', nargs=3, metavar=('ADDRESS', 'KEY', 'VALUE'),
                        help='input-map/restore: one explicit case write through the ordinary setter, restored at the end')
    parser.add_argument('--assign-facts', nargs='+', action='append', metavar='TERMINAL FACT',
                        help='input-map/restore: ASSIGN facts for one terminal of the change drive: '
                             'xN [triggers-inhibited] [protection-removal] or yN LOAD LEVELS')
    parser.add_argument('--drives', nargs='+', type=int, help='baseline-compare/capture: drives (default 1 2 4)')
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
                          (args.change, ('input-map', 'restore')), (args.assign_facts, ('input-map', 'restore')),
                          (args.drives, ('baseline-compare', 'baseline-capture')),
                          (args.reason, ('baseline-capture',)), (args.prompt, ('baseline-capture',))):
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
    if args.scenario in ('restore', 'y0-map') or (args.scenario == 'input-map' and args.change):
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
    if args.scenario == 'y0-map':
        return {str(records.FIXTURE['y0_owner']): {'assign': {'y0': ['indicator', 'either']}}}
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
    drives = sorted(set(records.FIXTURE['input_drives'] if name == 'input-map' else
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
    if write_list:
        plan = records.restoration_plan(baseline, write_list, args.image_level, facts)
        for entry in write_list:
            steps, refusal = records.plan_entry(baseline, entry, args.image_level, facts, restore=False)
            forward.append({'entry': entry, 'steps': steps, 'refusal': refusal})
    cases, commands, bounds, expected = [], [], {}, {}
    if name == 'input-map':
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
    return {'drives': [{'address': a, 'required_enabled': True} for a in drives],
            'terminals': (['x0', 'x1', 'x2', 'x3'] if name == 'input-map' else ['y0'] if name == 'y0-map' else []),
            'commands': commands, 'cases': cases,
            'running_members': [], 'write_list': write_list, 'bounds': bounds, 'prepared_stop': PREPARED_STOP,
            'restoration_plan': plan, 'case_writes': forward, 'left_changed': left,
            'expected_evidence': expected, 'abort_conditions': ABORTS,
            'cleanup': ['restoration plan for every written field (each read back, no replay)', 'bench off',
                        'read state and io read of every scenario drive (ending_reads.json)',
                        'select the originally selected drive', 'close COM13'],
            'max_age_ms': args.max_age_ms if name == 'input-map' else records.MAX_AGE_MS, 'facts': facts,
            'image_level': args.image_level}


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


FOLDER_RUNNERS = {'input-map': _input_map, 'y0-map': _y0_map, 'baseline-compare': _baseline_compare,
                  'baseline-capture': _baseline_capture, 'restore': _restore}


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
                     debug='raw' if args.scenario == 'y0-map' else None, firmware=args.firmware,
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
