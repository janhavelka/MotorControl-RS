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


class FolderBase(unittest.TestCase):
    """Run one wired-bench folder scenario against a bench fake."""

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


class FolderScenarioTest(FolderBase):
    """The five wired-bench scenarios end to end against the bench fake."""

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


class MappingBench(fakes.FakeBench):
    """Status bits after (default) or before the 0x0040 inversion, per drive."""

    classes = {}

    def __init__(self, on_event, **kwargs):
        super().__init__(on_event, **kwargs)
        self.installs = []

    def _inputs(self, address):
        physical = super()._inputs(address)
        mapping = self.classes.get(address, 'after')
        return physical ^ (self.io(address)[0] & 0xF) if mapping == 'after' else physical

    def _command(self, name, address, host, driver, kwargs):
        if name == 'io-qualification-install':
            self.installs.append(host)
        return super()._command(name, address, host, driver, kwargs)


def mapping_bench(classes=None, **attributes):
    def factory(on_event):
        bench = MappingBench(on_event)
        bench.classes = dict(classes or {})
        for key, value in attributes.items():
            setattr(bench, key, value)
        return bench
    return factory


class StatusMappingScenarioTest(FolderBase):
    """Prompt 05 status-mapping scenario against the bench fake."""

    ARGS = ['--scenario', 'status-mapping', '--image-level', '05']

    def polarity_writes(self, bench, address):
        return [kw['driver_args'][2] for name, kw in bench.calls
                if name == 'io' and kw.get('address') == address and kw['driver_args'][:2] == ('set', 'input-polarity')]

    def test_after_class_on_every_terminal_and_drive(self):
        code, folder, result, bench = self.run_scenario(self.ARGS, mapping_bench())
        self.assertEqual(code, 0, result)
        self.assertEqual(self.verdicts(result), {'M01': {'drive-reported': 'PASS', 'controller-timestamped': 'PASS'},
                                                 'M02': {'drive-reported': 'PASS'}, 'M03': {'drive-reported': 'PASS'},
                                                 'M04': {'read back': 'PASS'}})
        detail = json.loads((folder / 'status_mapping.json').read_text())
        self.assertEqual(set(detail['classes'].values()), {'after'})
        self.assertEqual(len(detail['classes']), 12)
        for a in (1, 2, 4):
            self.assertEqual(bench.io(a), fakes.POWER_UP)
            self.assertEqual(self.polarity_writes(bench, a), ['1', '0', '1', '0', '2', '0', '2', '0', '4', '0', '4', '0',
                                                              '8', '0', '8', '0'])
        # Function 0 inversions state mapping unknown and no fact; PIN inversions the measured class.
        x1 = [h for h in bench.installs if h[1] == 'x1' and h[0] == '2']
        self.assertEqual(x1, [('2', 'x1', '0', '0', 'assign', 'unknown', 'protection-removal'),
                              ('2', 'x1', '0', '1', 'assign', 'unknown'), ('2', 'x1', '0', '0', 'assign', 'unknown'),
                              ('2', 'x1', '15', '0', 'assign', 'unknown', 'triggers-inhibited'),
                              ('2', 'x1', '15', '1', 'assign', 'after', 'triggers-inhibited'),
                              ('2', 'x1', '15', '0', 'assign', 'after', 'triggers-inhibited'),
                              ('2', 'x1', '2', '0', 'assign', 'unknown', 'triggers-inhibited')])
        ons = [kw['host_args'] for name, kw in bench.calls if name == 'bench' and (kw['host_args'] or ())[:1] == ('on',)]
        self.assertEqual(len(ons), 12)
        self.assertTrue(all(h[2] == '2000' for h in ons))

    def test_before_class_is_recorded_as_before(self):
        code, folder, result, _ = self.run_scenario(self.ARGS + ['--terminals', 'x3'],
                                                    mapping_bench({1: 'before', 2: 'before', 4: 'before'}))
        self.assertEqual(code, 0, result)
        detail = json.loads((folder / 'status_mapping.json').read_text())
        self.assertEqual(detail['classes'], {'x3/1': 'before', 'x3/2': 'before', 'x3/4': 'before'})

    def test_contradiction_keeps_no_record_and_restores(self):
        code, folder, result, bench = self.run_scenario(self.ARGS + ['--terminals', 'x3', 'x0'],
                                                        mapping_bench({2: 'before'}))
        self.assertEqual(code, 2)
        verdicts = self.verdicts(result)
        self.assertEqual(verdicts['M02']['drive-reported'], 'FAIL')
        self.assertEqual(verdicts['M04']['read back'], 'PASS')
        for a in (1, 2, 4):
            self.assertEqual(bench.io(a), fakes.POWER_UP)

    def test_uncertain_polarity_write_ends_without_further_writes(self):
        code, folder, result, bench = self.run_scenario(self.ARGS + ['--terminals', 'x3'],
                                                        mapping_bench(fail_write=('io', 0, 8)))
        self.assertEqual(code, 2)
        self.assertEqual([kw['driver_args'] for _, kw in self.sets(bench)], [('set', 'input-polarity', '8')])
        self.assertTrue(any('ambiguous' in d for d in result['deviations']), result['deviations'])
        field = next(f for f in result['restoration']['fields'] if f['key'] == 'io.input-polarity' and f['drive'] == 1)
        self.assertTrue(field['uncertain'])
        self.assertFalse(any(o['active'] for o in bench.outputs.values()))

    def test_an_active_other_input_aborts_before_the_write(self):
        class Stuck(MappingBench):
            def _inputs(self, address):
                return super()._inputs(address) | (2 if address == 4 else 0)
        code, folder, result, bench = self.run_scenario(self.ARGS + ['--terminals', 'x3'], Stuck)
        self.assertEqual(code, 2)
        self.assertEqual(self.sets(bench), [])

    def test_status_mapping_arguments(self):
        for argv in (['--scenario', 'status-mapping', '--run-dir', 'build/bench/2026-11-19_wb05_a', '--firmware', 'f'],
                     ['--scenario', 'status-mapping', '--image-level', '04', '--run-dir', 'build/bench/2026-11-19_wb05_a',
                      '--firmware', 'f'],
                     ['--scenario', 'input-map', '--terminals', 'x3', '--run-dir', 'build/bench/2026-11-19_wb05_a',
                      '--firmware', 'f']):
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                scenarios.arguments(argv)


class MappingHelpersTest(unittest.TestCase):
    def test_status_bit_and_class(self):
        self.assertEqual([records.status_bit(m, p, on) for m in ('after', 'before') for p in (0, 1) for on in (False, True)],
                         [0, 1, 1, 0, 0, 1, 0, 1])
        self.assertEqual([records.classify_mapping(o, n) for o, n in ((1, 0), (0, 1), (0, 0), (1, 1), (None, 1))],
                         ['after', 'before', 'contradiction', 'contradiction', 'contradiction'])
        with self.assertRaises(ValueError):
            records.status_bit('unknown', 1, True)

class TimingBench(MappingBench):
    """Outputs end by their hold timer; the drive reports each input ``delay_ms`` late."""

    delay_ms = 3

    def __init__(self, on_event, **kwargs):
        super().__init__(on_event, **kwargs)
        self.history = []  # (output, on_us, off_us)

    def _tick(self):
        super()._tick()
        for name, o in self.outputs.items():
            if o['active'] and o['due_us'] and self.now_us >= o['due_us']:
                o.update(active=False, off_us=o['due_us'], due_us=0, off_reason='hold_elapsed')

    def _inputs(self, address):
        bits, at = 0, self.now_us - 7000 - self.delay_ms * 1000  # The fake samples 0x0008 7 ms before the reply.
        for name, o in self.outputs.items():
            started = o['on_us'] if o['ons'] else None
            ended = o['off_us'] if not o['active'] else None
            if started is not None and started <= at and (ended is None or ended > at or ended < started):
                bits |= 1 << self.mapping[name]
        return bits ^ (self.io(address)[0] & 0xF)

    def begin(self, name, **kwargs):
        return {'reply': self.command(name, **kwargs)}

    def wait(self, handle, release=False):
        return handle['reply']


class InputTimingScenarioTest(FolderBase):
    ARGS = ['--scenario', 'input-timing', '--image-level', '05', '--address', '1']

    def test_filter_value_run_writes_measures_and_restores(self):
        code, folder, result, bench = self.run_scenario(
            self.ARGS + ['--filter-value', '3', '--delay-trials', '3', '--width-trials', '2', '--widths', '5', '50'],
            TimingBench)
        self.assertEqual(code, 0, result)
        self.assertEqual(self.verdicts(result), {'F01': {'read back': 'PASS'},
                                                 'F02': {'controller-timestamped': 'PASS', 'drive-reported': 'PASS'},
                                                 'F03': {'controller-timestamped': 'PASS', 'drive-reported': 'PASS'}})
        self.assertEqual([kw['driver_args'] for _, kw in self.sets(bench)],
                         [('filters', 'set', 'input-filter', '3'), ('filters', 'set', 'input-filter', '2')])
        self.assertEqual(bench.settings[1]['tuning.filters'][0], 2)
        detail = json.loads((folder / 'input_timing.json').read_text())
        self.assertEqual(detail['delay_summary']['on']['measured'], 3)
        self.assertEqual(set(detail['widths']), {'5', '50'})
        self.assertEqual(detail['widths']['50']['counts']['observed'], 2)
        # The observed drive is read last by every interlock.
        for entry in detail['interlock_rows']:
            self.assertEqual(entry['decision'], 'admit')
        self.assertEqual(bench.debug_mode, 'off')
        ons = [kw['host_args'] for name, kw in bench.calls if name == 'bench' and (kw['host_args'] or ())[:1] == ('on',)]
        self.assertEqual([h[2] for h in ons], ['200'] * 3 + ['5', '5', '50', '50'])

    def test_relay_compare_at_filter_2_writes_nothing(self):
        code, folder, result, bench = self.run_scenario(
            self.ARGS + ['--filter-value', '2', '--delay-trials', '2', '--width-trials', '0', '--compare-relay'],
            TimingBench)
        self.assertEqual(code, 0, result)
        self.assertEqual(self.sets(bench), [])
        detail = json.loads((folder / 'input_timing.json').read_text())
        self.assertEqual(set(detail['relay']), {'relay2', 'mosfet1'})
        self.assertEqual(detail['relay']['relay2']['summary']['observed'], 2)
        self.assertEqual(self.verdicts(result)['F04'], {'controller-timestamped': 'PASS'})

    def test_timing_arguments(self):
        base = ['--scenario', 'input-timing', '--run-dir', 'build/bench/2026-11-19_wb05_a', '--firmware', 'f',
                '--image-level', '05']
        for extra in (['--filter-value', '4'], ['--filter-value', '1000'], ['--filter-value', '3', '--compare-relay'],
                      ['--filter-value', '2', '--poll-depth', '4'], ['--filter-value', '2', '--widths', '0'],
                      ['--filter-value', '2', '--address', '3']):
            with self.subTest(extra=extra), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                scenarios.arguments(base + extra)


class TimingAnalysisTest(unittest.TestCase):
    @staticmethod
    def sample(start, raw):
        return {'address': 1, 'operation_id': start, 'request_start_us': start, 'reply_end_us': start + 8000,
                'uncertainty_us': 0, 'raw_inputs': raw}

    def test_observed_missed_and_indeterminate(self):
        pre = self.sample(0, 0)
        polls = [self.sample(t, 0) for t in range(100500, 200000, 6000)]
        missed = records.timing_trial([pre] + polls, on_us=100000, off_us=130000, bit=8, window_end_us=150000)
        self.assertEqual(missed['class'], 'missed')
        self.assertLess(missed['max_instant_gap_us'], 30000)
        short = records.timing_trial([pre] + polls, on_us=100000, off_us=110000, bit=8, window_end_us=150000)
        self.assertEqual(short['class'], 'indeterminate')  # 14 ms instant gaps are not below a 10 ms pulse.
        polls[1] = self.sample(polls[1]["request_start_us"], 8)
        seen = records.timing_trial([pre] + polls, on_us=100000, off_us=110000, bit=8, window_end_us=150000)
        self.assertEqual(seen['class'], 'observed')
        self.assertEqual(seen['on']['status'], 'measured')
        self.assertEqual(seen['off']['status'], 'measured')
        unknown = records.timing_trial([pre, self.sample(100500, 0), self.sample(106500, 0)], on_us=100000, off_us=130000, bit=8, window_end_us=150000)
        self.assertEqual(unknown['class'], 'indeterminate')

class RecordBench(MappingBench):
    """Record image fake: installs follow the record rules, refusals send no frame."""

    BUS = ('read-state', 'read-identity', 'read-config', 'io', 'tuning', 'segment', 'driver', 'control')

    def __init__(self, on_event, **kwargs):
        super().__init__(on_event, **kwargs)
        self.frames, self.identity_fresh, self.valid = 0, False, {}
        self.debug_mode = 'off'

    def _command(self, name, address, host, driver, kwargs):
        if name == 'useaddr' and address != self.selected:
            self.identity_fresh = False
        if name == 'debug':
            reply = super()._command(name, address, host, driver, kwargs)
            reply['observed'] = self.frames
            return reply
        if name == 'read-identity':
            self.identity_fresh = True
        if name == 'io-qualification-install':
            self.installs.append(host)
            terminal, function, polarity, mapping = host[1], int(host[2]), int(host[3]), host[5]
            index = records.TERMINALS.index(terminal) if terminal in records.TERMINALS else None
            current = (self.io(self.selected)[0] >> index) & 1 if index is not None else 0
            if mapping == 'recorded' and not self.identity_fresh:
                return dict(ok=False, result='mapping_not_recorded', read_first='read identity')
            if mapping in ('before',) and self.identity_fresh:
                return dict(ok=False, result='mapping_contradicts_record')
            if mapping == 'unknown' and function and polarity != current:
                return dict(ok=False, result='qualification_required')
            self.valid[terminal] = (function, polarity)
            return dict(ok=True, result='done')
        if name == 'io' and driver[:2] == ('set', 'input-polarity'):
            value = int(driver[2])
            changed = [t for i, t in enumerate(records.TERMINALS) if ((self.io(address)[0] ^ value) >> i) & 1]
            if any(self.valid.get(t) != (self.io(address)[1 + records.TERMINALS.index(t)],
                                          (value >> records.TERMINALS.index(t)) & 1) for t in changed):
                return dict(ok=False, result='qualification_required', operation_id=0)
        if name == 'tuning' and driver[:3] == ('filters', 'set', 'input-filter') and int(driver[3]) not in \
                (0, 1, 2, 3, 5, 10, 20, 50):  # The committed MEASURED_INPUT_FILTERS.
            return dict(ok=False, result='invalid', operation_id=0)
        if name in self.BUS:
            self.frames += 1
        reply = super()._command(name, address, host, driver, kwargs)
        if name == 'io' and driver[:1] == ('set',):
            self.valid = {}
        return reply


class RecordCasesScenarioTest(FolderBase):
    ARGS = ['--scenario', 'status-mapping', '--image-level', '05', '--mapping-cases', 'record']

    def test_m05_and_n01_against_the_record(self):
        code, folder, result, bench = self.run_scenario(self.ARGS, RecordBench)
        self.assertEqual(code, 0, result)
        self.assertEqual(self.verdicts(result), {'M05': {'sent': 'PASS', 'read back': 'PASS'}, 'N01': {'sent': 'PASS'}})
        detail = json.loads((folder / 'status_record.json').read_text())
        self.assertEqual([r['result'] for r in detail['n01']], ['invalid', 'invalid', 'mapping_not_recorded',
                                                                 'mapping_contradicts_record'])
        self.assertEqual([kw['driver_args'] for _, kw in self.sets(bench)],
                         [('set', 'x3', '17'), ('set', 'input-polarity', '8'), ('set', 'input-polarity', '8'),
                          ('set', 'input-polarity', '0'), ('set', 'x3', '0'), ('filters', 'set', 'input-filter', '4'),
                          ('filters', 'set', 'input-filter', '100')])
        self.assertEqual(bench.io(1), fakes.POWER_UP)
        self.assertIn(('1', 'x3', '17', '1', 'assign', 'recorded', 'triggers-inhibited'), bench.installs)
        self.assertEqual(bench.debug_mode, 'off')


import bench_stop_inputs as stop_inputs  # noqa: E402


def _sample(t_us, motion, inputs=0, speed=0, position=0, address=4):
    """One read-state sample in bench_outputs.state_sample form; request start t_us, reply 25 ms later."""
    words = [(position >> 16) & 0xFFFF, position & 0xFFFF]
    return {'operation_id': t_us, 'address': address, 'motion': motion, 'alarm': 0, 'outputs': 0, 'inputs': inputs,
            'speed': speed, 'position_words': words,
            'windows': {'motion': [t_us, t_us + 25000], 'outputs': [t_us, t_us + 25000], 'feedback': [t_us, t_us + 25000]}}


def _trial(samples, **extra):
    trial = {'on_us': 1_000_000, 'off_us': 1_200_000, 'samples': samples, 'start_us': 0, 'steps': 3000,
             'start_position': 0, 'terminal': {'ok': False}, 'edges': [{'at_us': 1_110_000, 'active': True}],
             'y0': 2, 'function': 5}
    trial.update(extra)
    return trial


class StopInputClassificationTest(unittest.TestCase):
    """Implement 3: every witness, or indeterminate with the missing one named."""

    def recorded(self):
        # Shape of wb07_pilot_s1_f5c: running, X3 seen, stopped short, no further start.
        return [_sample(200_000, 6, speed=60, position=100), _sample(600_000, 6, speed=60, position=500),
                _sample(1_018_000, 6, 8, speed=59, position=900), _sample(1_100_000, 2, 8, speed=49, position=930),
                _sample(1_190_000, 2, 8, speed=4, position=933), _sample(1_300_000, 2, 0, position=933),
                _sample(3_100_000, 2, 0, position=933)]

    def test_recorded_stop_is_input_stopped(self):
        result = stop_inputs.classify(_trial(self.recorded()))
        self.assertEqual(result['class'], 'input-stopped', result['missing'])
        w = result['witnesses']
        self.assertEqual(w['y0_edge_us'], 110_000)
        self.assertEqual(w['stop_bracket_us'], [18_000, 125_000])
        self.assertTrue(w['stopped_short'] and not w['further_start'] and w['input_seen'])

    def test_completion_or_restart_is_never_input_stopped(self):
        done = stop_inputs.classify(_trial(self.recorded(), terminal={'ok': True}))
        self.assertEqual(done['class'], 'indeterminate')
        self.assertIn('the operation reported completion', done['missing'])
        samples = self.recorded() + [_sample(3_200_000, 6, speed=60, position=1000)]
        again = stop_inputs.classify(_trial(samples))
        self.assertTrue(again['witnesses']['further_start'])
        self.assertEqual(stop_inputs.move_verdict([dict(_trial(samples), analysis=again)])[0], 'FAIL')

    def test_velocity_stop_after_its_own_timed_stop_is_indeterminate(self):
        # The input is seen while running; the first stopped sample follows the
        # route's own timed stop (competing end at 1000 ms after the start).
        samples = [_sample(200_000, 4, speed=30), _sample(1_018_000, 4, 8, speed=30), _sample(1_150_000, 4, 8, speed=30),
                   _sample(1_400_000, 0, 0)]
        result = stop_inputs.classify(_trial(samples, steps=None, start_position=None, competing_end_us=1_300_000,
                                             edges=[]))
        self.assertEqual(result['class'], 'indeterminate')
        self.assertIn('the stop is not separated from the competing end condition', result['missing'])
        early = stop_inputs.classify(_trial(samples[:2] + [_sample(1_100_000, 0, 8)], steps=None, start_position=None,
                                            competing_end_us=1_300_000, edges=[]))
        self.assertEqual(early['class'], 'input-stopped', early['missing'])

    def test_stop_first_seen_after_the_safety_stop_is_indeterminate(self):
        result = stop_inputs.classify(_trial(self.recorded(), safety_stop_us=1_050_000))
        self.assertEqual(result['class'], 'indeterminate')
        self.assertIn('first stopped sample after the prepared safety stop', result['missing'])

    def test_missing_witnesses_are_named(self):
        no_motion = stop_inputs.classify(_trial([_sample(1_100_000, 2, 8, position=0), _sample(1_300_000, 2)]))
        self.assertIn('no motion witnessed after the start and before the stimulus', no_motion['missing'])
        unseen = [dict(s, inputs=0) for s in self.recorded()]
        blind = stop_inputs.classify(_trial(unseen, edges=[]))
        self.assertIn('neither the input bit nor a Y0 edge witnessed the stimulus', blind['missing'])
        self.assertEqual(stop_inputs.classify({'samples': []})['missing'], ['source not commanded'])

    def test_short_pulse_that_did_not_stop_the_move(self):
        samples = [_sample(200_000, 6, speed=60, position=100), _sample(1_050_000, 6, speed=60, position=1000),
                   _sample(2_900_000, 6, speed=60, position=2900), _sample(3_200_000, 3, position=3000)]
        result = stop_inputs.classify(_trial(samples, terminal={'ok': True}))
        self.assertEqual(result['class'], 'not stopped')

    def test_group_verdict(self):
        member = lambda **w: {'class': 'input-stopped', 'witnesses': dict({'stopped_short': True, 'further_start': False,
                                                                           'reported_complete': False, 'travel': 1300}, **w),
                              'missing': []}
        trial = {'index': 1, 'on_us': 1, 'starts_after_on': 0, 'members': [{'address': a, 'state': 'failed'} for a in (1, 2, 4)],
                 'per_member': {a: member() for a in (1, 2, 4)}}
        self.assertEqual(stop_inputs.group_verdict([trial]), ('PASS', None))
        self.assertEqual(stop_inputs.group_verdict([dict(trial, starts_after_on=1)])[0], 'FAIL')
        restart = dict(trial, per_member={1: member(further_start=True), 2: member(), 4: member()})
        self.assertEqual(stop_inputs.group_verdict([restart])[0], 'FAIL')
        refused = {'index': 1, 'interlock_refused': 'read operation IDs are not consecutive'}
        self.assertEqual(stop_inputs.group_verdict([refused])[0], 'indeterminate')


class StopInputsPlanTest(unittest.TestCase):
    def args(self, *extra):
        return scenarios.arguments(['--scenario', 'stop-inputs', '--firmware', str(ROOT / 'README.md'), '--plan-only',
                                    '--address', '4', '--image-level', '07', *extra])

    def test_plan_restores_x3_y0_and_profile_per_drive(self):
        baseline = scenarios.records.load_baseline(scenarios.records.DEFAULT_BASELINE)
        args = self.args('--case', 'S1', '--function', '5')
        plan = scenarios.folder_plan(args, baseline)
        steps = [(s['drive'], s['key'], s['write']['text']) for s in plan['restoration_plan']['steps']]
        self.assertEqual(steps, [(4, 'io.x3', 'io set x3 0 4'), (4, 'io.y0', 'io set y0 0 4'),
                                 (4, 'profile.acceleration', 'motion-profile stage 100 100 60 0 5000 0 4')])
        self.assertEqual(plan['restoration_plan']['steps'][0]['facts'], {'x3': ['triggers-inhibited', 'protection-removal']})
        group = scenarios.folder_plan(self.args('--case', 'G1', '--function', '6', '--leave-profile'), baseline)
        self.assertEqual(sorted({s['drive'] for s in group['restoration_plan']['steps'] if s['key'] == 'io.x3'}), [1, 2, 4])
        self.assertFalse(any(s['key'].startswith('profile.') for s in group['restoration_plan']['steps']))
        self.assertEqual(len(group['left_changed']), 12)
        self.assertTrue(all(e['storage'] == 'RAM' and e['reason'] for e in group['left_changed']))
        velocity = scenarios.folder_plan(self.args('--case', 'S3', '--function', '6'), baseline)
        self.assertIn('snapshot', [s['route'] for s in velocity['restoration_plan']['steps']])

    def test_snapshot_window_maps_per_field(self):
        # A restored window is split into its fields, so a scalar baseline
        # (jog.speed 120) is compared with its own word, not the whole window.
        self.assertEqual(scenarios.window_values('velocity', [120, 100, 100]),
                         {'jog.speed': 120, 'jog.acceleration': 100, 'jog.deceleration': 100})
        self.assertEqual(scenarios.window_values('home', [35, 60, 30, 100, 0, 0]),
                         {'homing.method': 35, 'homing.search-speed': 60, 'homing.return-speed': 30,
                          'homing.ramp': 100, 'homing.offset': [0, 0]})
        self.assertEqual(scenarios.window_values('home', None), {})

    def test_arguments_are_bounded_per_case(self):
        for bad in (['--case', 'S1', '--function', '4'], ['--case', 'S8', '--function', '5'],
                    ['--case', 'S1', '--function', '5', '--leave-profile'], ['--case', 'S7', '--function', '5'],
                    ['--case', 'S1', '--function', '5', '--sub', 'a'], ['--case', 'X9', '--function', '5']):
            with self.subTest(bad=bad), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.args(*bad)
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            scenarios.arguments(['--scenario', 'stop-inputs', '--firmware', 'x', '--plan-only', '--case', 'S1',
                                 '--function', '5', '--image-level', '06'])
        self.assertEqual(self.args('--case', 'S8', '--function', '5', '--deceleration', '1000',
                                   '--stimulus', 'stop fast').repetitions, 5)


class InputStartPlanTest(unittest.TestCase):
    """Wired bench 08: the input-start scenario's plan and bounds (no port)."""

    def args(self, *extra):
        return scenarios.arguments(['--scenario', 'input-start', '--firmware', str(ROOT / 'README.md'), '--plan-only',
                                    '--address', '4', *extra])

    def test_plan_names_the_route_write_and_restoration(self):
        args = self.args('--change', '4', 'io.x3', '7', '--assign-facts', 'x3', 'triggers-inhibited', '--image-level', '08')
        plan = scenarios.folder_plan(args, scenarios.records.load_baseline(args.baseline))
        self.assertEqual([c['id'] for c in plan['cases']], ['input-start'])
        self.assertEqual(plan['cases'][0]['required_levels'], ['local', 'controller-timestamped', 'drive-reported'])
        self.assertEqual(plan['write_list'], [{'drive': 4, 'key': 'io.x3', 'index': None, 'candidate': 7}])
        self.assertTrue(plan['restoration_plan']['admissible'])
        self.assertEqual([s['key'] for s in plan['restoration_plan']['steps']], ['io.x3'])
        self.assertEqual(plan['terminals'], ['x3'])
        self.assertIn('arm trigger ID mosfet2 200', plan['commands'])
        self.assertEqual(plan['bounds']['window_ms'], 10000)

    def test_bounds_and_mismatches_refuse_before_the_port(self):
        for bad in (('--width-ms', '19'), ('--width-ms', '5001'), ('--poll-ms', '501'), ('--window-ms', '99'),
                    ('--trigger-output', 'mosfet1'), ('--rpm', '0'),
                    ('--change', '2', 'io.x3', '7', '--assign-facts', 'x3', 'triggers-inhibited', '--image-level', '08'),
                    ('--change', '4', 'io.x2', '7', '--assign-facts', 'x2', 'triggers-inhibited', '--image-level', '08'),
                    ('--change', '4', 'io.x3', '7', '--assign-facts', 'x3', 'triggers-inhibited')):
            with self.subTest(bad=bad), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.args(*bad)
        self.assertEqual(self.args('--start-terminal', 'x0', '--trigger-output', 'mosfet1').start_terminal, 'x0')


# --------------------------------------------------------------------------
# Wired bench 09: input-started and serially interrupted moves

import bench_input_starts as starts  # noqa: E402


class ProfileConsole:
    """Records commands; a motion-profile read answers settled, as the firmware does."""

    def __init__(self):
        self.calls = []

    def clock(self):
        return 0.0

    def sleep(self, _):
        pass

    def command(self, name, **kwargs):
        self.calls.append((name, kwargs.get('host_args') or kwargs.get('driver_args')))
        reply = {'ok': True, 'operation_id': len(self.calls)}
        if name == 'wiring':
            reply.update(address=4, target=4, result='done')
        if name == 'driver':
            reply['observation'] = {'raw': [0, 1000, 0, 0, 0, 0, 1]}
        if name == 'motion-profile':
            reply.update(pending=False, session_ok=True, original=[30, 100, 100, 60, 0, 5000],
                         current=[30, 100, 100, 60, 0, 5000], error='none')
        return reply


class ReferenceReadsTest(unittest.TestCase):
    """Wired bench 09 deviation 1: the fresh sequence keeps the profile session open."""

    def test_documented_order_without_a_forget_after_the_profile_read(self):
        console = ProfileConsole()
        out = starts.reference_reads(console, 4, 5.0)
        names = [(n, a[0] if a else None) for n, a in console.calls if n != 'wiring']
        self.assertEqual(names, [('read-config', None), ('driver', 'read'), ('motion-profile', 'forget'),
                                 ('motion-profile', 'read'), ('motion-profile-start-speed-relation', '30'),
                                 ('read-state', None)])
        read = names.index(('motion-profile', 'read'))
        self.assertNotIn(('motion-profile', 'forget'), names[read:])
        self.assertEqual(out['motion-profile'], [30, 100, 100, 60, 0, 5000])
        self.assertEqual(out['driver']['raw'][6], 1)

    def test_the_baseline_read_helper_forgets_its_session(self):
        # The cause: records.read_group reads and then forgets the profile session.
        console = ProfileConsole()
        scenarios.records.read_group(console, 4, 'motion-profile')
        self.assertEqual([a[0] for n, a in console.calls if n == 'motion-profile'], ['read', 'forget'])


def _result(change, expected, outcome='attributed', further='not_excluded', start=-10000, end=50000, on=1_000_000):
    return {'known': True, 'outcome': outcome, 'missing': [] if outcome == 'attributed' else ['spacing'],
            'further': further, 'completion': 'finite_stopped', 'running_observed': True, 'change_observed': True,
            'change_start_us': on + start, 'change_end_us': on + end, 'position_change': change,
            'expected_known': True, 'expected_change': expected, 'position_matches': change == expected,
            'samples': 40, 'lost_samples': 0, 'late_samples': 0,
            'spacing_us': {'min': 50000, 'median': 51000, 'max': 52000, 'count': 38}, 'stop_id': 0,
            'stopped_after_stop': False}


def _session(results, on=1_000_000, off=1_050_000, rows=None):
    return {'operation_id': 300, 'phase': 'done', 'error': 'none',
            'source': {'state': 'hold_elapsed', 'on_us': on, 'off_us': off},
            'isolation': {'admitted': True, 'rows': rows or [{'address': a, 'row': 'M'} for a in results]},
            'capacity': {'debug_overwrites': 0, 'aux1_overwrites': 0, 'stop_service_delay_max_us': 0},
            'members': [{'address': a, 'result': r} for a, r in results.items()]}


class InputStartAnalysisTest(unittest.TestCase):
    def test_start_delay_brackets_and_endpoint_error(self):
        edges = [{'at_us': 1_013_381, 'active': False}, {'at_us': 3_100_000, 'active': True}]
        a = starts.start_trial(_session({4: _result(1999, 2000), 1: _result(2000, 2000)}), edges)
        self.assertEqual(a['d_us'], 13381)
        self.assertEqual(a['members'][4]['bracket_us'], [-10000, 50000])
        self.assertEqual(a['members'][4]['endpoint_error'], -1)
        self.assertEqual(a['members'][1]['endpoint_error'], 0)
        self.assertEqual((a['y0_on_during_hold'], a['y0_on_after_release']), (1, 0))

    def test_no_edge_within_a_second_leaves_d_unknown(self):
        a = starts.start_trial(_session({4: _result(500, 500)}), [{'at_us': 2_100_000, 'active': False}])
        self.assertIsNone(a['d_us'])

    def test_verdicts_split_attribution_and_exact_endpoint(self):
        edges = [{'at_us': 1_014_000, 'active': False}]
        exact = [{'index': 1, 'analysis': starts.start_trial(_session({4: _result(500, 500)}), edges)}]
        self.assertEqual(starts.start_verdicts(exact, [4]),
                         {'controller-timestamped': ('PASS', None), 'drive-reported': ('PASS', None)})
        off = [{'index': 1, 'analysis': starts.start_trial(_session({4: _result(499, 500)}), edges)}]
        v = starts.start_verdicts(off, [4])
        self.assertEqual(v['controller-timestamped'][0], 'PASS')
        self.assertEqual(v['drive-reported'][0], 'FAIL')
        self.assertIn('no tolerance', v['drive-reported'][1])

    def test_second_start_fails_and_a_missing_witness_is_indeterminate(self):
        edges = [{'at_us': 1_014_000, 'active': False}]
        second = [{'index': 1, 'analysis': starts.start_trial(_session({4: _result(1000, 500, 'second_start', 'seen')}),
                                                              edges)}]
        self.assertEqual(starts.start_verdicts(second, [4])['drive-reported'][0], 'FAIL')
        open_ = [{'index': 1, 'analysis': starts.start_trial(_session({4: _result(500, 500, 'indeterminate')}), edges)}]
        self.assertEqual(starts.start_verdicts(open_, [4])['controller-timestamped'][0], 'indeterminate')
        self.assertEqual(starts.start_verdicts([], [4])['drive-reported'][0], 'NOT RUN')

    def test_shared_spread_bounds_from_member_brackets(self):
        a = starts.start_trial(_session({1: _result(2000, 2000, start=1442, end=63104),
                                         2: _result(2000, 2000, start=-30711, end=30243),
                                         4: _result(2000, 2000, start=-10728, end=48517)}), [])
        self.assertEqual(starts.spread(a), {'lower_us': 0, 'upper_us': 63104 + 30711})
        summary = starts.summarize_starts([{'analysis': a}])
        self.assertEqual(summary['members']['1']['endpoint_errors'], {'0': 1})
        self.assertAlmostEqual(summary['spread_upper_ms']['max'], 93.815)

    def test_width_table_and_floor(self):
        trials = []
        for width, starts_seen in ((5, (0, 0, 0)), (10, (1, 0, 1)), (12, (1, 1, 1)), (15, (1, 1, 1)), (20, (1, 1, 1))):
            for n in starts_seen:
                trials.append({'width_ms': width, 'count': {'starts': n, 'position_change': 200 * n}})
        table = starts.width_table(trials)
        self.assertEqual(table['reliable_floor_ms'], 12)
        self.assertEqual({k: v for k, v in table['widths']['10'].items() if k != 'changes'},
                         {'trials': 3, 'single': 2, 'none': 1, 'multiple': 0, 'unclear': 0})
        trials.append({'width_ms': 30, 'count': {'starts': 2}})
        self.assertIsNone(starts.width_table(trials)['reliable_floor_ms'])

    def test_count_starts_from_running_episodes_and_y0_edges(self):
        samples = [_sample(0, 0, position=100), _sample(50_000, 4, position=150), _sample(100_000, 4, position=250),
                   _sample(150_000, 1, position=301)]
        # Recorded A3 shape: one episode, one Y0 edge, feedback +201 for a +200 start.
        self.assertEqual(starts.count_starts(samples, 100, 301, 200, 1),
                         {'running_episodes': 1, 'y0_edges': 1, 'position_change': 201, 'starts': 1})
        self.assertIsNone(starts.count_starts(samples, 100, 301, 200, 2)['starts'])  # Edge and samples disagree.
        quiet = [_sample(0, 1, position=100), _sample(50_000, 1, position=300)]
        self.assertIsNone(starts.count_starts(quiet, 100, 300, 200, 0)['starts'])  # Moved with no witness.
        self.assertEqual(starts.count_starts(quiet[:1], 100, 100, 200, 0)['starts'], 0)

    def test_stop_classification_needs_every_witness(self):
        base = {'analysis': starts.start_trial(_session({4: _result(1450, 3000)}), [{'at_us': 1_014_000, 'active': False}]),
                'moving_witness': {4: 'running_sample'}, 'edges': [{'at_us': 1_014_000, 'active': False}]}
        self.assertEqual(starts.stop_classification(base, 3000, 2_000_000)[4]['class'], 'stopped')
        unwitnessed = dict(base, moving_witness={})
        self.assertEqual(starts.stop_classification(unwitnessed, 3000, 2_000_000)[4]['class'], 'indeterminate')
        restarted = dict(base, edges=base['edges'] + [{'at_us': 2_500_000, 'active': False}])
        self.assertEqual(starts.stop_classification(restarted, 3000, 2_000_000)[4]['class'], 'fail')
        full = dict(base, analysis=starts.start_trial(_session({4: _result(3000, 3000)}), []))
        self.assertEqual(starts.stop_classification(full, 3000, 2_000_000)[4]['class'], 'fail')
        late = dict(full, moving_witness={})
        self.assertEqual(starts.stop_classification(late, 3000, 2_000_000)[4]['class'], 'indeterminate')

    def test_interrupt_analysis_compares_both_meanings(self):
        samples = [_sample(0, 4, position=0), _sample(1_000_000, 4, position=900), _sample(2_000_000, 4, position=2500),
                   _sample(3_000_000, 1, position=3900), _sample(4_000_000, 1, position=3900)]
        a = starts.interrupt_analysis({'form': 'relative', 'steps': 3000, 'start_position': 0, 'interrupt_position': 900,
                                       'samples': samples, 'interrupt_tx_us': 1_050_000})
        self.assertEqual((a['minus_start'], a['minus_interrupt']), (3900, 3000))
        self.assertTrue(a['ends_at_interrupt_plus_steps'])
        self.assertFalse(a['ends_at_start_plus_steps'])
        self.assertEqual(a['arrived_transitions'][0]['to'], 1)
        b = starts.interrupt_analysis({'form': 'absolute', 'steps': 3000, 'target': 3900, 'samples': samples})
        self.assertTrue(b['ends_at_target'])


class InputStartsPlanTest(unittest.TestCase):
    """The 09 families: cases, named states, write list and restoration before the port."""

    def args(self, scenario, *extra):
        return scenarios.arguments(['--scenario', scenario, '--firmware', str(ROOT / 'README.md'), '--plan-only',
                                    '--image-level', '08', *extra])

    def test_shared_plan_writes_x0_mode_and_y0_and_restores_them(self):
        args = self.args('input-start-shared', '--cases', 'B1', 'B3', '--leave-profile')
        plan = scenarios.folder_plan(args, scenarios.records.load_baseline(args.baseline))
        keys = {(e['drive'], e['key']): e['candidate'] for e in plan['write_list']}
        for address in (1, 2, 4):
            self.assertEqual(keys[(address, 'io.x0')], 7)
            self.assertEqual(keys[(address, 'drive.position-mode')], 1)
            self.assertEqual(keys[(address, 'io.x3')], 5)
        self.assertEqual(keys[(4, 'io.y0')], 2)
        self.assertTrue(plan['restoration_plan']['admissible'])
        steps = [(s['drive'], s['key']) for s in plan['restoration_plan']['steps']]
        self.assertLess(steps.index((4, 'io.x0')), steps.index((4, 'drive.position-mode')))
        self.assertTrue(all(e['key'].startswith('profile.') for e in plan['left_changed']))
        self.assertEqual([c['id'] for c in plan['cases']], ['B1', 'B3'])
        self.assertEqual(plan['terminals'], ['x0', 'x3', 'y0'])

    def test_single_plan_with_a11_restores_the_velocity_words_by_snapshot(self):
        args = self.args('input-start-single', '--cases', 'A2', 'A11')
        plan = scenarios.folder_plan(args, scenarios.records.load_baseline(args.baseline))
        routes = {s['key']: s['route'] for s in plan['restoration_plan']['steps']}
        self.assertEqual(routes['jog.speed'], 'snapshot')
        self.assertEqual(routes['io.x0'], 'ordinary')
        self.assertEqual(routes['profile.acceleration'], 'ordinary')  # One 0x0021/5 stage step.

    def test_snapshot_restores_run_before_other_drives_are_selected(self):
        # Wired bench 09 A11: drive 1 and 2 steps first selected those drives and
        # made drive 4's velocity snapshot stale (`velocity restore` refused).
        args = self.args('input-start-single', '--cases', 'A11')
        plan = scenarios.folder_plan(args, scenarios.records.load_baseline(args.baseline))['restoration_plan']
        ordered = scenarios.snapshot_steps_first(plan)['steps']
        self.assertEqual(ordered[0]['route'], 'snapshot')
        self.assertEqual(ordered[0]['drive'], 4)
        self.assertEqual(sorted(map(json.dumps, ordered)), sorted(map(json.dumps, plan['steps'])))
        rest = [s for s in plan['steps'] if s['route'] != 'snapshot']
        self.assertEqual(ordered[1:], rest)

    def test_cases_belong_to_their_family_and_bounds_refuse(self):
        for bad in (('input-start-single', '--cases', 'B1'), ('input-start-shared',),
                    ('position-interrupt', '--cases', 'I1', 'I1'), ('input-start-single', '--cases', 'A1', '--case', 'A1'),
                    ('input-start-single', '--cases', 'A1', '--repetitions', '51')):
            with self.subTest(bad=bad), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.args(*bad)
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            scenarios.arguments(['--scenario', 'position-interrupt', '--firmware', str(ROOT / 'README.md'), '--plan-only',
                                 '--image-level', '07', '--cases', 'I1'])

    def test_named_states_hold_function_7_only_on_members(self):
        for name, state in starts.STATES.items():
            for address in starts.DRIVES:
                words = starts.expected_io(state, address)
                if address not in state['members']:
                    self.assertFalse(7 <= words[1] <= 13 or 7 <= words[4] <= 13, (name, address))
        self.assertEqual(starts.expected_io(starts.STATES['NC-Y4'], 4)[6], 4)


if __name__ == '__main__':
    unittest.main()
