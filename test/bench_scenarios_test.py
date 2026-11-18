"""Finite named scenarios, exclusive evidence and cleanup failure boundaries."""
from contextlib import contextmanager, redirect_stdout, redirect_stderr
import copy
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import bench_probe as probe
import bench_scenarios as scenarios
import bench_session as session

SPEC = importlib.util.spec_from_file_location('scenario_serial_fakes', ROOT / 'test/bench_probe_test.py')
wire = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(wire)


class ScenarioConsole:
    instances = []

    def __init__(self, connection=None, on_event=None):
        self.connection = connection
        self.on_event = on_event or (lambda *a, **k: None)
        self.synchronized = True
        self.calls = []
        self.setting = 10
        self.selected = 1
        self.fail_setting = False
        self.fail_restore = False
        self.now = 0
        self.instances.append(self)

    def emit(self, event, **fields):
        self.on_event(event, **fields)

    def clock(self):
        return self.now

    def sleep(self, duration):
        self.now += duration

    def drain_startup(self, duration):
        pass

    def identify(self, **kwargs):
        return self.command('version', **kwargs)

    def command(self, name, **kwargs):
        self.calls.append((name, kwargs))
        result = dict(ok=True, command=name)
        if name == 'wiring':
            result.update(result='done', target=self.selected, address=self.selected)
        if name == 'motion-profile':
            result.update(pending=False, restore_unsettled=False, saved=False, restored=False)
        if name == 'useaddr':
            self.selected = kwargs['address']
            result.update(result='done', address=self.selected, operation_id=0)
        if name in ('tuning', 'control') and 'set' in kwargs.get('driver_args', ()) and \
                kwargs.get('address', 1) != self.selected:
            result.update(ok=False, result='unavailable', operation_id=0)
            self.emit('fake_command', name=name, arguments=kwargs, result=result)
            return result
        if name == 'read-config':
            result['config'] = dict(raw=[0, 1000, 0])
        if name == 'read-state':
            result['state_blocks'] = [dict(block=0, running=False, raw_alarm=0, alarm_flag=False),
                                      dict(block=2, raw_speed=0)]
        if name in ('tuning', 'control'):
            tokens = kwargs['driver_args']
            if 'set' in tokens:
                value = int(tokens[-1])
                if self.fail_setting or self.fail_restore and value == 10:
                    result.update(ok=False, outcome='uncertain', uncertain=True)
                else:
                    self.setting = value
            fields = probe.TUNING_FIELDS['filters'] if name == 'tuning' else probe.CONTROL_FIELDS
            values = [0] * len(fields)
            values[fields.index('input-filter' if name == 'tuning' else 'lock-delay')] = self.setting
            result['observation'] = dict(raw=values)
        self.emit('fake_command', name=name, arguments=kwargs, result=result)
        return result


class ScenarioArgumentsTest(unittest.TestCase):
    def parse(self, args):
        with redirect_stderr(io.StringIO()):
            return scenarios.arguments(['--plan-only'] + args)

    def test_default_quick_is_read_only_named_plan(self):
        args = self.parse([])
        self.assertEqual(args.scenario, 'quick')
        self.assertEqual(args.count, 10)
        console = ScenarioConsole()
        record = dict(before={'config': {'config': {'raw': [0, 1000, 0]}},
                              'load': dict(zip(probe.LOAD_FIELDS, (0, 0, 0)))})
        with patch.object(probe, 'campaign') as campaign:
            scenarios.run_scenario(console, args, record)
        self.assertEqual(campaign.call_args.args[1], 'stress')
        self.assertIsNone(campaign.call_args.kwargs['load'])
        self.assertEqual(console.calls, [('read-config', dict(address=1, timeout_s=5))])

    def test_crossed_or_unbounded_options_fail_before_open(self):
        examples = [
            ['--load', '1000', '0', '64'], ['--phase', 'forward'],
            ['--native', 'driver'], ['--setting', 'lock-delay', '--value', '10'],
            ['--persistence-kind', 'save'], ['--count', '101'], ['--interval', 'nan'],
            ['--stop-policy', 'fast'],
            ['--timeout', 'inf'], ['--address', '0'], ['--evidence-records', '1'],
            ['--scenario', 'native-read', '--native', 'io', '--native-args', 'set', 'x0-function', 'none'],
            ['--scenario', 'angle', '--address', '2', '--arguments', '90', 'deg', 'motor', 'positive', 'reject', '60', 'configured'],
            ['--scenario', 'position', '--address', '248'],
            ['--scenario', 'position', '--count', '1'],
            ['--scenario', 'homing-prerequisites', '--interval', '0'],
            ['--scenario', 'velocity'], ['--scenario', 'persistence'],
        ]
        for argv in examples:
            with self.subTest(argv=argv), patch.object(session, 'open_port') as port:
                with self.assertRaises((SystemExit, ValueError)):
                    self.parse(argv)
                port.assert_not_called()

    def test_each_explicit_native_read_reuses_checked_campaign(self):
        for native, tokens in [('driver', ['read']), ('io', ['read']), ('control', ['read']),
                               ('tuning', ['filters', 'read']), ('segment', ['position', '15', 'read'])]:
            args = self.parse(['--scenario', 'native-read', '--native', native, '--native-args'] + tokens)
            console = ScenarioConsole()
            record = dict(before={'config': {'config': {'raw': [0, 1000, 0]}}})
            with patch.object(probe, 'driver_read_campaign') as campaign:
                scenarios.run_scenario(console, args, record)
            self.assertEqual(campaign.call_args.kwargs['command'], native)
            self.assertEqual(campaign.call_args.kwargs['driver_args'], tuple(tokens))

    def test_plan_only_does_not_open_port_or_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / 'plan'
            with patch.object(scenarios, 'run_recorded') as run, redirect_stdout(io.StringIO()):
                self.assertEqual(scenarios.main(['--out', str(prefix), '--plan-only']), 0)
            run.assert_not_called()
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_changed_configuration_is_failed_without_implicit_repair(self):
        console = ScenarioConsole()
        record = dict(before={'config': {'config': {'raw': [1, 1000, 0]}}})
        args = self.parse(['--scenario', 'native-read', '--native', 'driver'])
        with patch.object(probe, 'driver_read_campaign'):
            with self.assertRaisesRegex(probe.BenchError, 'configuration changed'):
                scenarios.run_scenario(console, args, record)
        self.assertNotIn('workload_verified', record)
        self.assertEqual(console.calls, [('read-config', dict(address=1, timeout_s=5))])

    def test_nonrepeating_scenario_records_one_attempt(self):
        args = self.parse(['--scenario', 'native-read', '--native', 'driver'])
        self.assertEqual(args.count, 1)

    def test_unknown_direct_scenario_never_runs_or_claims_workload(self):
        console = ScenarioConsole()
        args = self.parse([])
        args.scenario = 'typo'
        with self.assertRaises(ValueError):
            scenarios.run_scenario(console, args, {})
        self.assertEqual(console.calls, [])

    def test_angle_primary_failure_and_restore_failure_are_separate(self):
        console = ScenarioConsole()
        args = self.parse(['--scenario', 'angle', '--arguments', '36', 'deg', 'motor', 'positive', 'reject', '60', 'configured'])
        record = {'campaigns': [{'mode': 'move-angle', 'cleanup': 'drive_reported_standstill', 'ok': False}]}
        with patch.object(scenarios.motion, 'profile_snapshot', return_value={'original': [0]*6}), \
                patch.object(probe, 'move_campaign', side_effect=probe.BenchError('original move uncertain')), \
                patch.object(scenarios.motion, 'restore_profile', side_effect=probe.BenchError('restore unknown')):
            with self.assertRaisesRegex(probe.BenchError, 'original move uncertain'):
                scenarios._angle(console, args, record)
        self.assertIn('restore unknown', record['profile_restoration']['error'])

    def test_angle_framing_failure_never_restores(self):
        console = ScenarioConsole()
        console.synchronized = False
        args = self.parse(['--scenario', 'angle', '--arguments', '36', 'deg', 'motor', 'positive', 'reject', '60', 'configured'])
        record = {'campaigns': [{'mode': 'move-angle', 'cleanup': 'unknown', 'ok': False}]}
        with patch.object(scenarios.motion, 'profile_snapshot', return_value={'original': [0]*6}), \
                patch.object(probe, 'move_campaign', side_effect=probe.BenchError('framing failed')), \
                patch.object(scenarios.motion, 'restore_profile') as restore:
            with self.assertRaisesRegex(probe.BenchError, 'framing failed'):
                scenarios._angle(console, args, record)
        restore.assert_not_called()
        self.assertFalse(record['profile_restoration']['known'])

    def test_load_capture_precedes_host_workload_restoration(self):
        args = self.parse(['--scenario', 'load', '--load', '1000', '0', '64'])
        console = ScenarioConsole()
        record = dict(before={'config': {'config': {'raw': [0, 1000, 0]}},
                              'load': dict(zip(probe.LOAD_FIELDS, (0, 0, 0)))})
        with patch.object(probe, 'campaign'):
            scenarios.run_scenario(console, args, record)
        self.assertEqual(console.calls[:2], [('load', dict(timeout_s=5)),
                                            ('load', dict(load=(0, 0, 0), timeout_s=5))])
        self.assertTrue(record['load_cleanup']['restored'])
        self.assertIn('load_window', record)

    def test_interrupted_load_preserves_error_and_restores_only_host_fixture(self):
        args = self.parse(['--scenario', 'load', '--load', '1000', '0', '64'])
        console = ScenarioConsole()
        record = dict(before={'load': dict(zip(probe.LOAD_FIELDS, (0, 0, 0)))})
        with patch.object(probe, 'campaign', side_effect=KeyboardInterrupt()):
            with self.assertRaises(KeyboardInterrupt):
                scenarios.run_scenario(console, args, record)
        self.assertEqual([name for name, _ in console.calls], ['load', 'load'])
        self.assertTrue(record['load_cleanup']['restored'])
        self.assertNotIn('workload_verified', record)

    def test_homing_prerequisite_inventory_does_not_execute_home(self):
        args = self.parse(['--scenario', 'homing-prerequisites'])
        console = ScenarioConsole()
        record = dict(before={'config': {'config': {'raw': [0, 1000, 0]}}})
        scenarios.run_scenario(console, args, record)
        self.assertEqual([name for name, _ in console.calls], ['caps', 'wiring', 'io', 'read-config'])
        self.assertFalse(record['homing']['execution_attempted'])
        self.assertIn('method-specific', record['homing']['readiness'])

    def test_persistence_preview_never_executes_or_claims_durability(self):
        args = self.parse(['--scenario', 'persistence-plan', '--persistence-kind', 'save'])
        console = ScenarioConsole()
        record = dict(before={'config': {'config': {'raw': [0, 1000, 0]}}})
        with patch.object(probe, 'persistence_campaign') as campaign:
            scenarios.run_scenario(console, args, record)
        for key in ('execute', 'verify', 'finish'):
            self.assertIs(campaign.call_args.kwargs[key], False)
        self.assertFalse(record['durability_verified'])


class SettingsScenarioTest(unittest.TestCase):
    def args(self, value=11):
        return scenarios.arguments(['--plan-only', '--scenario', 'settings', '--setting', 'input-filter', '--value', str(value)])

    def test_checked_change_and_exact_original_restore_once(self):
        console = ScenarioConsole()
        record = {}
        scenarios._setting(console, self.args(), record)
        writes = [args['driver_args'] for name, args in console.calls
                  if name == 'tuning' and 'set' in args['driver_args']]
        self.assertEqual(writes, [('filters', 'set', 'input-filter', '11'), ('filters', 'set', 'input-filter', '10')])
        self.assertTrue(record['setting']['restored'])
        self.assertEqual(console.setting, 10)

    def test_filter_effects_and_changed_readback_require_rebuilt_io_evidence_before_restore(self):
        class InvalidatingConsole(ScenarioConsole):
            def __init__(self):
                super().__init__()
                self.io_fresh = False
                self.stationary_fresh = True
                self.observed_filter = 10
                self.invalidations = []
                self.write_preconditions = []

            def invalidate(self, reason):
                self.io_fresh = False
                self.stationary_fresh = False
                self.invalidations.append(reason)

            def command(self, name, **kwargs):
                tokens = kwargs.get('driver_args', ())
                if name == 'tuning' and 'set' in tokens:
                    flags = (self.io_fresh, self.stationary_fresh)
                    self.write_preconditions.append(flags)
                    if not all(flags):
                        self.calls.append((name, kwargs))
                        return dict(ok=False, result='invalid', tx_bytes=0)
                    result = super().command(name, **kwargs)
                    self.invalidate('accepted_filter_tx')
                    return result
                result = super().command(name, **kwargs)
                if name == 'tuning' and 'read' in tokens and self.observed_filter != self.setting:
                    self.observed_filter = self.setting
                    self.invalidate('changed_filter_observation')
                elif name == 'io': self.io_fresh = True
                elif name == 'read-state': self.stationary_fresh = True
                return result

        console = InvalidatingConsole()
        # The full session supplied identity/configuration; I/O still needs an
        # explicit refresh. A changed group read invalidates dependent evidence.
        record = {}
        scenarios._setting(console, self.args(), record)
        self.assertTrue(record['setting']['restored'])
        self.assertEqual(console.setting, 10)
        self.assertEqual(console.write_preconditions, [(True, True)] * 2)
        self.assertEqual(console.invalidations.count('accepted_filter_tx'), 2)
        self.assertEqual(console.invalidations.count('changed_filter_observation'), 2)
        commands = [name for name, _ in console.calls]
        write_indexes = [i for i, (name, args) in enumerate(console.calls)
                         if name == 'tuning' and 'set' in args['driver_args']]
        self.assertEqual(commands[write_indexes[0] + 1:write_indexes[1]],
                         ['tuning', 'read-identity', 'read-config', 'io', 'read-state'])

    def test_another_drive_is_selected_before_any_write(self):
        console = ScenarioConsole()
        args = scenarios.arguments(['--plan-only', '--scenario', 'settings', '--setting', 'input-filter',
                                    '--value', '11', '--address', '2'])
        record = {}
        scenarios._setting(console, args, record)
        names = [name for name, _ in console.calls]
        self.assertLess(names.index('useaddr'), names.index('tuning'))
        self.assertEqual(console.selected, 2)
        writes = [kw for name, kw in console.calls if name == 'tuning' and 'set' in kw['driver_args']]
        self.assertEqual([kw['address'] for kw in writes], [2, 2])
        self.assertTrue(record['setting']['restored'])
        self.assertTrue(record['selection']['changed'])

    def test_noop_or_large_setting_change_never_yields_write(self):
        for value in (10, 12, 0, 65535):
            console = ScenarioConsole()
            with self.assertRaisesRegex(probe.BenchError, 'one native-unit change'):
                scenarios._setting(console, self.args(value), {})
            self.assertFalse(any('set' in args.get('driver_args', ()) for _, args in console.calls))

    def test_uncertain_application_preserves_backup_and_never_restores_or_replays(self):
        console = ScenarioConsole()
        console.fail_setting = True
        record = {}
        with self.assertRaisesRegex(probe.BenchError, 'uncertain'):
            scenarios._setting(console, self.args(), record)
        self.assertFalse(record['setting']['restored'])
        self.assertEqual(len([args for _, args in console.calls if 'set' in args.get('driver_args', ())]), 1)
        self.assertEqual(record['setting']['before']['observation']['raw'][0], 10)

    def test_unknown_restore_remains_failure_and_never_retries(self):
        console = ScenarioConsole()
        console.fail_restore = True
        record = {}
        with self.assertRaisesRegex(probe.BenchError, 'uncertain'):
            scenarios._setting(console, self.args(), record)
        self.assertFalse(record['setting']['restored'])
        self.assertEqual(len([args for _, args in console.calls if 'set' in args.get('driver_args', ())]), 2)


class ConnectedConsole(ScenarioConsole):
    """Board-declared connected X0-X3 (wired bench 03); installs answer like the firmware."""
    def command(self, name, **kwargs):
        if name == 'io-qualification-install':
            self.calls.append((name, kwargs))
            return dict(ok=True, command=name, result='done')
        result = super().command(name, **kwargs)
        if name == 'wiring':
            result.update(board=dict(inputs=[2, 2, 2, 2], outputs=[1, 1]), fixture='applied')
        if name == 'read-config':
            result['config'] = dict(raw=dict(input_polarity=0), inputs=[
                dict(function=f, known=True, wiring=2) for f in (1, 2, 3, 0)])
        return result


class ConnectedSettingsScenarioTest(unittest.TestCase):
    def args(self, *extra):
        return scenarios.arguments(['--plan-only', '--scenario', 'settings', '--setting', 'input-filter', '--value', '11', *extra])

    def test_connected_board_needs_input_facts_before_any_write(self):
        console = ConnectedConsole()
        with self.assertRaisesRegex(probe.BenchError, 'needs --input-facts configure'):
            scenarios._setting(console, self.args(), {})
        self.assertFalse(any('set' in kw.get('driver_args', ()) for _, kw in console.calls))
        self.assertFalse(any(name == 'io-qualification-install' for name, _ in console.calls))

    def test_records_precede_each_write_and_carry_no_added_fact(self):
        console = ConnectedConsole()
        record = {}
        scenarios._setting(console, self.args('--input-facts', 'configure'), record)
        self.assertTrue(record['setting']['restored'])
        names = [name for name, _ in console.calls]
        writes = [i for i, (name, kw) in enumerate(console.calls) if name == 'tuning' and 'set' in kw['driver_args']]
        self.assertEqual(len(writes), 2)
        for start, end in ((0, writes[0]), (writes[0], writes[1])):
            window = names[start:end]
            self.assertEqual(window.count('io-qualification-install'), 4, window)
            self.assertLess(window.index('read-config'), window.index('io-qualification-install'))
        installs = [kw['host_args'] for name, kw in console.calls if name == 'io-qualification-install']
        self.assertEqual(installs[:4], [('1', 'x0', '1', '0', 'configure', 'unknown'), ('1', 'x1', '2', '0', 'configure', 'unknown'),
                                        ('1', 'x2', '3', '0', 'configure', 'unknown'), ('1', 'x3', '0', '0', 'configure', 'unknown')])
        self.assertEqual(installs[:4], installs[4:])

    def test_option_scope(self):
        for argv in (['--scenario', 'settings', '--setting', 'lock-delay', '--value', '11', '--input-facts', 'configure'],
                     ['--scenario', 'settings', '--setting', 'input-filter', '--value', '3', '--input-facts', 'configure',
                      '--declare-unconnected'],
                     ['--scenario', 'quick', '--input-facts', 'configure'],
                     ['--scenario', 'settings', '--setting', 'input-filter', '--value', '3', '--input-facts', 'segment']):
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                scenarios.arguments(['--plan-only'] + argv)


class RecordedSessionTest(unittest.TestCase):
    def setUp(self):
        ScenarioConsole.instances.clear()
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.prefix = Path(self.tmp.name) / 'evidence'
        self.opens = 0
        self.closes = 0

    @contextmanager
    def opener(self, *args):
        self.opens += 1
        try:
            yield object()
        finally:
            self.closes += 1

    def recorded(self, run, **kwargs):
        def execute(console, record):
            run(console, record)
            record['workload_verified'] = True
        with patch.object(session, 'Console', ScenarioConsole):
            return session.run_recorded(port='fake', out=self.prefix, scenario='fake', inputs={},
                                        run=execute, port_opener=self.opener, **kwargs)

    def summary(self):
        return json.loads(self.prefix.with_suffix('.json').read_text())

    def test_one_owner_fixed_snapshots_and_streamed_bounded_campaign_tail(self):
        def run(console, record):
            for index in range(35):
                console.emit('summary', mode='probe', number=index, ok=True)
        result = self.recorded(run)
        self.assertTrue(result['ok'])
        self.assertEqual((self.opens, self.closes), (1, 1))
        self.assertEqual(result['campaigns_seen'], 35)
        self.assertEqual(len(result['campaigns']), 16)
        self.assertEqual(result['campaigns'][0]['number'], 19)
        lines = [json.loads(line) for line in self.prefix.with_suffix('.jsonl').read_text().splitlines()]
        self.assertEqual(len([line for line in lines if line['event'] == 'summary']), 35)

    def test_jsonl_file_bytes_match_evidence_capacity_on_windows(self):
        result = self.recorded(lambda console, record:
                               console.emit('summary', mode='probe', ok=True, text='motor \u00b0'))
        raw = self.prefix.with_suffix('.jsonl').read_bytes()
        self.assertNotIn(b'\r\n', raw)
        self.assertEqual(len(raw), result['evidence']['bytes'])
        self.assertEqual(len(raw.splitlines()), result['evidence']['records'])
        self.assertEqual(self.summary()['evidence'], result['evidence'])

    def test_existing_artifacts_are_not_replaced_and_port_never_opens(self):
        for suffix in ('.json', '.jsonl'):
            with self.subTest(suffix=suffix):
                prefix = Path(self.tmp.name) / suffix[1:]
                existing = prefix.with_suffix(suffix)
                existing.write_text('previous evidence')
                with patch.object(session, 'Console', ScenarioConsole):
                    with self.assertRaises(FileExistsError):
                        session.run_recorded(port='fake', out=prefix, scenario='fake', inputs={},
                            run=lambda *_: None, port_opener=self.opener)
                self.assertEqual(existing.read_text(), 'previous evidence')
        self.assertEqual(self.opens, 0)

    def test_failure_only_collects_cached_diagnostics_and_retains_original_error(self):
        def run(console, record):
            raise probe.BenchError('work failed')
        with self.assertRaisesRegex(probe.BenchError, 'work failed'):
            self.recorded(run)
        result = self.summary()
        self.assertFalse(result['ok'])
        self.assertEqual(result['error'], 'work failed')
        self.assertEqual(set(result['failure_diagnostics']), set(session.CACHED))
        calls = ScenarioConsole.instances[-1].calls
        self.assertEqual([name for name, _ in calls[-len(session.CACHED):]], list(session.CACHED))
        self.assertNotIn('after', result)

    def test_disconnect_skips_all_failure_diagnostics(self):
        def run(console, record):
            console.synchronized = False
            raise probe.BenchError('device disconnected')
        with self.assertRaisesRegex(probe.BenchError, 'device disconnected'):
            self.recorded(run)
        self.assertEqual(self.summary()['failure_diagnostics'], {})
        self.assertIn('framing unavailable', self.summary()['failure_diagnostics_error'])

    def test_port_close_failure_cannot_leave_a_passing_summary(self):
        @contextmanager
        def failing_close(*args):
            yield object()
            raise OSError('disconnect while closing')
        def run(console, record):
            record['workload_verified'] = True
        with patch.object(session, 'Console', ScenarioConsole):
            with self.assertRaisesRegex(OSError, 'disconnect while closing'):
                session.run_recorded(port='fake', out=self.prefix, scenario='quick', inputs={},
                                     run=run, port_opener=failing_close)
        result = self.summary()
        self.assertFalse(result['ok'])
        self.assertEqual(result['error'], 'disconnect while closing')
        self.assertIn('after', result)  # Successful work stays inspectable.

    def test_work_and_port_close_failures_are_both_retained(self):
        @contextmanager
        def failing_close(*args):
            try:
                yield object()
            finally:
                raise OSError('port close failed')
        def run(console, record):
            raise probe.BenchError('original uncertain movement')
        with patch.object(session, 'Console', ScenarioConsole):
            with self.assertRaisesRegex(probe.BenchError, 'original uncertain movement') as caught:
                session.run_recorded(port='fake', out=self.prefix, scenario='position', inputs={},
                                     run=run, port_opener=failing_close)
        result = self.summary()
        self.assertFalse(result['ok'])
        self.assertEqual(result['error'], 'original uncertain movement')
        self.assertEqual(result['owner_error'], 'port close failed')
        self.assertIsInstance(caught.exception.__cause__, OSError)
        self.assertNotIn('after', result)

    def test_startup_failure_survives_port_close_failure(self):
        @contextmanager
        def failing_close(*args):
            try:
                yield object()
            finally:
                raise OSError('port close failed')
        for stage in ('__init__', 'drain_startup', 'identify'):
            with self.subTest(stage=stage):
                prefix = Path(self.tmp.name) / stage
                with patch.object(session, 'Console', ScenarioConsole), \
                        patch.object(ScenarioConsole, stage, side_effect=probe.BenchError('startup failed')):
                    with self.assertRaisesRegex(probe.BenchError, 'startup failed'):
                        session.run_recorded(port='fake', out=prefix, scenario='quick', inputs={},
                                             run=lambda *_: self.fail('work must not run'), port_opener=failing_close)
                result = json.loads(prefix.with_suffix('.json').read_text())
                self.assertFalse(result['ok'])
                self.assertEqual(result['error'], 'startup failed')
                self.assertEqual(result['owner_error'], 'port close failed')
                self.assertNotIn('before', result)
                self.assertNotIn('after', result)

    def test_interrupted_startup_read_prevents_diagnostic_writes(self):
        port = wire.Serial()
        @contextmanager
        def opener(*args):
            yield port
        with patch.object(port, 'read', side_effect=[b'{"type":', KeyboardInterrupt()]):
            with self.assertRaises(KeyboardInterrupt):
                session.run_recorded(port='fake', out=self.prefix, scenario='quick', inputs={},
                                     run=lambda *_: self.fail('work must not run'), port_opener=opener)
        result = self.summary()
        self.assertFalse(result['ok'])
        self.assertEqual(result['error'], 'interrupted')
        self.assertEqual(result['failure_diagnostics'], {})
        self.assertIn('framing unavailable', result['failure_diagnostics_error'])
        self.assertEqual(port.writes, [])

    def test_failed_campaign_summary_forbids_new_motor_after_reads(self):
        def run(console, record):
            console.emit('summary', mode='probe', ok=False, error='workload did not run')
        with self.assertRaises(probe.BenchError):
            self.recorded(run)
        calls = ScenarioConsole.instances[-1].calls
        self.assertEqual(sum(name.startswith('read-') for name, _ in calls), 3)

    def test_early_failed_campaign_cannot_be_erased_by_summary_tail(self):
        def run(console, record):
            console.emit('summary', mode='probe', ok=False, error='first workload failed')
            for _ in range(16):
                console.emit('summary', mode='diagnostic', ok=True)
        with self.assertRaises(probe.BenchError):
            self.recorded(run)
        self.assertFalse(self.summary()['ok'])

    def test_evidence_exhaustion_is_failure_with_exclusive_summary(self):
        def run(console, record):
            console.emit('summary', ok=True)
        with self.assertRaises(probe.BenchError):
            self.recorded(run, max_records=2)
        result = self.summary()
        self.assertFalse(result['ok'])
        self.assertTrue(result['evidence']['exhausted'])
        self.assertEqual((self.opens, self.closes), (1, 1))

    def test_empty_callback_cannot_claim_requested_workload(self):
        with patch.object(session, 'Console', ScenarioConsole):
            with self.assertRaisesRegex(probe.BenchError, 'workload did not complete'):
                session.run_recorded(port='fake', out=self.prefix, scenario='quick', inputs={},
                    run=lambda *_: None, port_opener=self.opener)
        self.assertFalse(self.summary()['ok'])
        self.assertNotIn('after', self.summary())

    def test_snapshot_preserves_local_transport_configuration_separately(self):
        console = ScenarioConsole()
        result = session.snapshot(console, refresh=True)
        self.assertEqual(result['console_config']['command'], 'config')
        self.assertEqual(result['config']['command'], 'read-config')
        self.assertIn('drv', result)
        self.assertIn('memory', result)
        self.assertIn('load', result)

    def test_discovery_restoration_is_checked_against_actual_cached_host_and_selection(self):
        original_tuple = dict(baud=9600, format='8E1')
        before = dict(host=dict(ok=True, active=original_tuple, active_known=True, blocked=False, configuring=False),
                      wiring=dict(ok=True, target=7, address=7, generation=9))
        for index, mutation in enumerate((None, 'tuple', 'blocked', 'unknown', 'configuring', 'target', 'generation')):
            with self.subTest(mutation=mutation):
                self.prefix = Path(self.tmp.name) / ('restore_' + str(index))
                after = copy.deepcopy(before)
                if mutation == 'tuple': after['host']['active'] = dict(baud=115200, format='8N1')
                if mutation == 'blocked': after['host']['blocked'] = True
                if mutation == 'unknown': after['host']['active_known'] = False
                if mutation == 'configuring': after['host']['configuring'] = True
                if mutation == 'target': after['wiring'].update(target=1, address=1)
                if mutation == 'generation': after['wiring']['generation'] = 10
                def run(console, record):
                    record['scan'] = dict(restored=True, original_tuple=original_tuple,
                                          original_target=[7, 7, 9])
                with patch.object(session, 'snapshot', side_effect=[before, after]):
                    if mutation is None:
                        self.assertTrue(self.recorded(run, refresh=False)['ok'])
                    else:
                        with self.assertRaises(probe.BenchError):
                            self.recorded(run, refresh=False)
                        self.assertFalse(self.summary()['ok'])

    def test_fragmented_serial_records_run_through_real_console(self):
        connection = wire.Serial(fragment=3)
        clock = wire.Clock()
        @contextmanager
        def opener(*args):
            self.opens += 1
            try:
                yield connection
            finally:
                self.closes += 1
        def console(port, on_event):
            return probe.Console(port, on_event=on_event, clock=clock, sleeper=clock.sleep)
        def run(console, record):
            result = console.command('probe', address=1, timeout_s=1)
            self.assertTrue(result['ok'])
            record['workload_verified'] = True
            console.emit('summary', mode='probe', probes_attempted=1, probes_passed=1, ok=True)
        with patch.object(session, 'Console', console), patch.object(session, 'snapshot', return_value={}):
            result = session.run_recorded(port='fake', out=self.prefix, scenario='probe', inputs={},
                run=run, port_opener=opener)
        self.assertTrue(result['ok'])
        self.assertEqual((self.opens, self.closes), (1, 1))
        self.assertEqual([item.decode().split()[1] for item in connection.writes], ['version', 'probe', 'release'])
        lines = [json.loads(line) for line in self.prefix.with_suffix('.jsonl').read_text().splitlines()]
        self.assertTrue(any(item['event'] == 'summary' for item in lines))

    def test_stale_or_partial_serial_response_cannot_pass_or_send_recovery(self):
        for index, payload in enumerate((wire.encoded(wire.reply(999, 'probe', result='accepted', address=1)),
                                         b'{"type":"reply"', b'')):
            with self.subTest(payload=payload):
                prefix = Path(self.tmp.name) / ('bad_' + str(index))
                clock = wire.Clock()
                def handler(request_id, command, args):
                    return payload if command == 'probe' else wire.Serial.normal(request_id, command, args)
                connection = wire.Serial(handler=handler, fragment=7)
                @contextmanager
                def opener(*args):
                    yield connection
                def console(port, on_event):
                    return probe.Console(port, on_event=on_event, clock=clock, sleeper=clock.sleep)
                def run(console, record):
                    console.command('probe', address=1, timeout_s=.2)
                    record['workload_verified'] = True
                with patch.object(session, 'Console', console), patch.object(session, 'snapshot', return_value={}):
                    with self.assertRaises(probe.BenchError):
                        session.run_recorded(port='fake', out=prefix, scenario='probe', inputs={},
                            run=run, port_opener=opener)
                result = json.loads(prefix.with_suffix('.json').read_text())
                self.assertFalse(result['ok'])
                self.assertNotIn('after', result)
                self.assertEqual(result['failure_diagnostics'], {})
                self.assertEqual([item.decode().split()[1] for item in connection.writes], ['version', 'probe'])

    def test_firmware_hash_is_labelled_provenance_and_does_not_attest_board(self):
        image = Path(self.tmp.name) / 'image.bin'
        image.write_bytes(b'firmware')
        result = self.recorded(lambda *_: None, firmware=image)
        self.assertEqual(result['firmware_artifact']['bytes'], 8)
        self.assertIn('does not attest', result['firmware_artifact']['provenance'])
        self.assertEqual(len(result['firmware_artifact']['sha256']), 64)


RECORDS_SPEC = importlib.util.spec_from_file_location('records_fakes', ROOT / 'test/bench_records_test.py')
fakes = importlib.util.module_from_spec(RECORDS_SPEC)
RECORDS_SPEC.loader.exec_module(fakes)
records = scenarios.records


class FakePort:
    def __init__(self):
        self.closed = False

    def close(self):
        self.closed = True


class FolderScenarioTest(unittest.TestCase):
    """The five wired-bench scenarios end to end against the bench fake."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.image = Path(self.tmp.name) / 'firmware.bin'
        self.image.write_bytes(b'image')
        self.benches = []
        self.opened = 0
        self.number = 0

    def run_scenario(self, argv, bench_factory=None):
        self.number += 1
        folder = Path(self.tmp.name) / ('2026-11-19_wb04_run%d' % self.number)
        args = scenarios.arguments(['--run-dir', str(folder), '--firmware', str(self.image), '--commit', 'abc'] + argv)
        factory = bench_factory or fakes.FakeBench

        def console(connection, on_event):
            bench = factory(on_event)
            self.benches.append(bench)
            return bench

        def opener(*_):
            self.opened += 1
            return FakePort()
        with patch.object(session, 'Console', console), patch.object(scenarios.time, 'sleep'), \
                redirect_stdout(io.StringIO()):
            code = scenarios.run_folder(args, port_opener=opener, check_board=lambda port: None)
        result = json.loads((folder / 'result.json').read_text(encoding='utf-8'))
        return code, folder, result, self.benches[-1]

    @staticmethod
    def verdicts(result):
        return {case['id']: case['verdicts'] for case in result['cases']}

    @staticmethod
    def sets(bench):
        return [(name, kw) for name, kw in bench.calls if kw.get('driver_args') and 'set' in kw['driver_args']]

    def test_input_map_passes_both_checks(self):
        code, folder, result, bench = self.run_scenario(['--scenario', 'input-map', '--case', 'H01'])
        self.assertEqual(code, 0, result)
        self.assertEqual(self.verdicts(result), {'H01.declarations': {'local': 'PASS'},
                                                 'H01.map': {'drive-reported': 'PASS', 'controller-timestamped': 'PASS'}})
        detail = json.loads((folder / 'input_map.json').read_text())
        self.assertEqual([row['output'] for row in detail['outputs']], ['mosfet1', 'relay2', 'relay1', 'mosfet2'])
        self.assertEqual(self.sets(bench), [])
        self.assertTrue((folder / 'console.log').read_text())
        for name in records.RUN_FILES:
            self.assertTrue((folder / name).is_file(), name)

    def test_swapped_relays_fail_the_map_but_not_the_declarations(self):
        swapped = dict(fakes.MAPPING, relay1=1, relay2=2)
        code, folder, result, _ = self.run_scenario(['--scenario', 'input-map', '--case', 'H01'],
                                                    lambda on_event: fakes.FakeBench(on_event, mapping=swapped))
        self.assertEqual(code, 2)
        verdicts = self.verdicts(result)
        self.assertEqual(verdicts['H01.declarations'], {'local': 'PASS'})
        self.assertEqual(verdicts['H01.map']['drive-reported'], 'FAIL')

    def test_alarm_during_the_map_aborts_without_writes(self):
        class AlarmBench(fakes.FakeBench):
            def _bench(self, tokens):
                if tokens[:1] == ('on',):
                    self.state[2]['raw_alarm'] = 9
                return super()._bench(tokens)
        code, folder, result, bench = self.run_scenario(['--scenario', 'input-map'], AlarmBench)
        self.assertNotEqual(code, 0)
        self.assertEqual(self.sets(bench), [])
        ons = [kw['host_args'] for name, kw in bench.calls if name == 'bench' and (kw['host_args'] or ())[:1] == ('on',)]
        self.assertEqual(len(ons), 1)
        self.assertFalse(any(o['active'] for o in bench.outputs.values()))
        self.assertTrue(any('alarm or RUNNING' in d for d in result['deviations']))

    def test_expected_refusal_after_a_trigger_assignment_sends_no_on_and_restores(self):
        code, folder, result, bench = self.run_scenario(
            ['--scenario', 'input-map', '--case', 'H08', '--outputs', 'mosfet2', '--expect-refusal', 'mosfet2',
             '--change', '2', 'io.x3', '7', '--assign-facts', 'x3', 'triggers-inhibited', '--image-level', '03'])
        self.assertEqual(code, 0, result)
        self.assertEqual(self.verdicts(result), {'H08.declarations': {'local': 'PASS'}, 'H08.refusal': {'local': 'PASS'},
                                                 'H08.change': {'acknowledged': 'PASS', 'read back': 'PASS'}})
        self.assertFalse(any(name == 'bench' and (kw['host_args'] or ())[:1] == ('on',) for name, kw in bench.calls))
        self.assertEqual([kw['driver_args'] for _, kw in self.sets(bench)], [('set', 'x3', '7'), ('set', 'x3', '0')])
        installs = [kw['host_args'] for name, kw in bench.calls if name == 'io-qualification-install']
        self.assertEqual(installs, [('2', 'x3', '7', '0', 'assign', 'unknown', 'triggers-inhibited'),
                                    ('2', 'x3', '0', '0', 'assign', 'unknown', 'triggers-inhibited')])
        detail = json.loads((folder / 'input_map.json').read_text())
        self.assertEqual(detail['outputs'][0]['row'], 'L5')
        self.assertEqual(bench.io(2), fakes.POWER_UP)

    def test_change_without_stated_facts_is_not_run_before_any_write(self):
        code, folder, result, bench = self.run_scenario(
            ['--scenario', 'restore', '--address', '1', '--case', 'H04', '--change', '1', 'io.input-polarity', '1',
             '--assign-facts', 'x0', 'triggers-inhibited', '--image-level', '03'])
        self.assertEqual(self.verdicts(result)['H04'], {'acknowledged': 'NOT RUN', 'read back': 'NOT RUN'})
        self.assertEqual(self.sets(bench), [])
        self.assertEqual(code, 2)

    def test_restore_after_a_case_write(self):
        code, folder, result, bench = self.run_scenario(
            ['--scenario', 'restore', '--address', '1', '--case', 'H04', '--change', '1', 'filters.input-filter', '3',
             '--input-facts', 'configure', '--image-level', '03'])
        self.assertEqual(code, 0, result)
        detail = json.loads((folder / 'restore.json').read_text())
        self.assertEqual([(d['key'], d['current']) for d in detail['differing']], [('filters.input-filter', 3)])
        self.assertEqual([kw['driver_args'] for _, kw in self.sets(bench)],
                         [('filters', 'set', 'input-filter', '3'), ('filters', 'set', 'input-filter', '2')])
        self.assertEqual(result['restoration']['fields'][0]['ending'], 2)

    def test_y0_map_writes_its_plan_first_and_counts_inverted_edges(self):
        seen = {}

        class Bench(fakes.FakeBench):
            def _group_write(self, address, read, index, value, register):
                if 'plan' not in seen:
                    folder = Path(self_test.tmp.name) / ('2026-11-19_wb04_run%d' % self_test.number)
                    seen['plan'] = json.loads((folder / 'scenario.json').read_text())['restoration_plan']
                return super()._group_write(address, read, index, value, register)
        self_test = self
        code, folder, result, bench = self.run_scenario(['--scenario', 'y0-map', '--address', '4', '--case', 'H02',
                                                         '--cycles', '2', '--phase-s', '0.5', '--image-level', '03'], Bench)
        self.assertEqual(code, 0, result)
        self.assertEqual([s['key'] for s in seen['plan']['steps']], ['io.custom', 'io.y0'])
        detail = json.loads((folder / 'y0_map.json').read_text())
        self.assertEqual([r['write'] for r in detail['rows']], ['io.y0 9', 'io.custom 1', 'io.custom 0', 'io.custom 1', 'io.custom 0'])
        edges = detail['correlation']['aux1']['edges']
        self.assertEqual([e['active'] for e in edges], [False, True, False, True])
        self.assertEqual({e['status'] for e in edges}, {'measured'})
        self.assertEqual(bench.io(4), fakes.POWER_UP)
        self.assertEqual(bench.debug_mode, 'off')

    def test_baseline_compare_and_capture(self):
        code, folder, result, _ = self.run_scenario(['--scenario', 'baseline-compare', '--case', 'H06'])
        self.assertEqual(code, 0, result)
        diff = json.loads((folder / 'baseline_diff.json').read_text())
        self.assertEqual((diff['compared'], diff['differs'], diff['unknown']), (573, [], []))
        code, folder, result, _ = self.run_scenario(['--scenario', 'baseline-capture', '--case', 'H09',
                                                     '--reason', 'check', '--prompt', '04'])
        self.assertEqual(code, 0, result)
        rebased = json.loads((folder / 'rebaselined.json').read_text())
        self.assertEqual(rebased['history'][-1]['reason'], 'check')
        self.assertEqual(records.compare_baselines(fakes.BASELINE, json.loads((folder / 'baseline_capture.json').read_text())), [])

    def test_capture_refuses_a_running_drive(self):
        class Running(fakes.FakeBench):
            def __init__(self, on_event):
                super().__init__(on_event)
                self.state[2]['running'] = True
        code, folder, result, _ = self.run_scenario(['--scenario', 'baseline-capture', '--reason', 'r', '--prompt', '04'],
                                                    Running)
        self.assertEqual(code, 2)
        self.assertTrue(any('baseline-capture refused' in d for d in result['deviations']))
        self.assertFalse((folder / 'baseline_capture.json').exists())

    def test_existing_run_folder_fails_before_the_port_opens(self):
        folder = Path(self.tmp.name) / '2026-11-19_wb04_taken'
        folder.mkdir()
        args = scenarios.arguments(['--scenario', 'input-map', '--run-dir', str(folder), '--firmware', str(self.image),
                                    '--commit', 'abc'])
        with redirect_stdout(io.StringIO()), self.assertRaises(FileExistsError):
            scenarios.run_folder(args, port_opener=lambda *_: self.fail('port opened'), check_board=lambda port: None)
        self.assertEqual(list(folder.iterdir()), [])

    def test_folder_scenario_arguments(self):
        bad = [['--scenario', 'input-map', '--out', 'x'],
               ['--scenario', 'input-map', '--run-dir', 'build/bench/2026-11-19_wb04_a'],
               ['--scenario', 'quick', '--run-dir', 'build/bench/2026-11-19_wb04_a', '--out', 'x'],
               ['--scenario', 'input-map', '--run-dir', 'build/bench/bad_name', '--firmware', 'f'],
               ['--scenario', 'restore', '--run-dir', 'build/bench/2026-11-19_wb04_a', '--firmware', 'f'],
               ['--scenario', 'y0-map', '--address', '1', '--run-dir', 'build/bench/2026-11-19_wb04_a', '--firmware', 'f',
                '--image-level', '03'],
               ['--scenario', 'input-map', '--expect-refusal', 'relay1', '--outputs', 'mosfet2',
                '--run-dir', 'build/bench/2026-11-19_wb04_a', '--firmware', 'f'],
               ['--scenario', 'baseline-capture', '--run-dir', 'build/bench/2026-11-19_wb04_a', '--firmware', 'f'],
               ['--scenario', 'restore', '--input-facts', 'configure', '--image-level', '03',
                '--run-dir', 'build/bench/2026-11-19_wb04_a', '--firmware', 'f']]
        for argv in bad:
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                scenarios.arguments(argv)


if __name__ == '__main__':
    unittest.main()
