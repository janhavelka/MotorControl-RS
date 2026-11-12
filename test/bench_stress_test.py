"""Offline checks for the stress tool's USB stop boundary."""
import collections
import importlib.util
import io
from pathlib import Path
import random
import sys
import types
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
serial = types.ModuleType("serial")
serial.tools = types.ModuleType("serial.tools")
serial.tools.list_ports = types.ModuleType("serial.tools.list_ports")
spec = importlib.util.spec_from_file_location("bench_stress", ROOT / "scripts/bench_stress.py")
bench = importlib.util.module_from_spec(spec)
with mock.patch.dict(sys.modules, {"serial": serial, "serial.tools": serial.tools,
                                  "serial.tools.list_ports": serial.tools.list_ports}):
    spec.loader.exec_module(bench)


class Clock:
    def __init__(self):
        self.now = 0.0

    def time(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class Console:
    def __init__(self, clock, terminal, accepted=True, timeout=None):
        self.clock, self.item = clock, terminal
        self.accepted, self.timeout = accepted, timeout
        self.ring, self.sent, self.retained = [], [], [17]
        self.next_id, self.port = 1, self
        self.reply = None

    def pump(self, seconds):
        self.clock.sleep(seconds)

    def close(self):
        pass

    def open(self):
        pass

    def write(self, data):
        self.ring.append(data)

    def send(self, command, wait=None):
        self.sent.append(command)
        ident = self.next_id
        self.next_id += 1
        if command != "stop normal 1":
            return {}
        if self.timeout == "send":
            raise bench.Unresponsive(command)
        self.reply = dict(ok=self.accepted, result="accepted" if self.accepted else "recovery_required",
                          _id=ident, operation_id=91 if self.accepted else None)
        if self.accepted:
            self.retained.append(91)
        return self.reply

    def terminal(self, reply, wait):
        if self.timeout == "terminal":
            raise bench.Unresponsive("stop terminal")
        if self.item is None:
            return None
        item = dict(id=reply["_id"], command_id=reply["_id"], operation_id=91)
        item.update(self.item)
        return item


def stopped(**changes):
    item = dict(type="action", ok=True, action_kind="stop", stop_policy="normal",
                target=1, address=1, state="succeeded", outcome="observed",
                execution="acknowledged", completion="observed", observation_known=True,
                raw_alarm=0, raw_motion=2)
    item.update(changes)
    return item


class UsbStopTests(unittest.TestCase):
    def setUp(self):
        self.clock = Clock()
        self.time_patch = mock.patch.object(bench.time, "time", self.clock.time)
        self.sleep_patch = mock.patch.object(bench.time, "sleep", self.clock.sleep)
        self.time_patch.start()
        self.sleep_patch.start()
        self.addCleanup(self.time_patch.stop)
        self.addCleanup(self.sleep_patch.stop)

    def run_fixture(self, terminal, **console_options):
        run = bench.Run.__new__(bench.Run)
        run.console = Console(self.clock, terminal, **console_options)
        run.usb_stalls, run.anomalies = [], []
        run.counts = collections.Counter()
        run.incident = mock.Mock(return_value=Path("incident.jsonl.gz"))
        run.summary = mock.Mock()
        return run

    def check_record(self, run, confirmed):
        self.assertEqual(run.console.sent.count("stop normal 1"), 1)
        self.assertEqual(run.usb_stalls[-1]["stop_confirmed"], confirmed)
        self.assertIn(17, run.console.retained)
        incident = next(call.args[1] for call in run.incident.call_args_list
                        if call.args[0] == "USB_STALL")
        self.assertEqual(incident["stop_confirmed"], confirmed)
        run.summary.assert_called()

    def test_matching_observed_stop_continues_and_keeps_results(self):
        run = self.run_fixture(stopped())
        self.assertTrue(run.usb_stall("console timeout"))
        self.check_record(run, True)
        self.assertEqual(run.console.retained, [17, 91])
        record = run.usb_stalls[-1]
        self.assertEqual(record["stop_reply"], run.console.reply)
        self.assertEqual(record["stop_terminal"]["operation_id"], 91)

    def test_unproven_stop_never_continues(self):
        cases = [None, stopped(ok=False, outcome="transport_error"),
                 stopped(completion="not_observed", outcome="acknowledged"),
                 stopped(id=999), stopped(command_id=999), stopped(operation_id=999),
                 stopped(action_kind="enable"), stopped(target=2), stopped(address=2),
                 stopped(execution="unknown"), stopped(observation_known=False),
                 stopped(raw_motion=4), stopped(raw_motion=None), stopped(raw_motion=True),
                 stopped(raw_alarm=5), stopped(raw_motion=8)]
        for terminal in cases:
            with self.subTest(terminal=terminal):
                run = self.run_fixture(terminal)
                self.assertFalse(run.usb_stall("console timeout"))
                self.check_record(run, False)
                self.assertEqual(run.console.retained, [17, 91])
                self.assertEqual(run.usb_stalls[-1]["stop_terminal"] is None, terminal is None)

    def test_refusal_and_timeout_are_saved_without_retry(self):
        for options in ({"accepted": False}, {"timeout": "send"}, {"timeout": "terminal"}):
            with self.subTest(options=options):
                run = self.run_fixture(stopped(), **options)
                self.assertFalse(run.usb_stall("console timeout"))
                self.check_record(run, False)
                record = run.usb_stalls[-1]
                if "timeout" in options:
                    self.assertIn("stop_error", record)
                else:
                    self.assertEqual(record["stop_reply"]["result"], "recovery_required")

    def test_failed_usb_stop_exits_even_with_recover(self):
        run = self.run_fixture(stopped(ok=False, outcome="transport_error"))
        run.args = types.SimpleNamespace(mix="standard", only="plain", debug="off",
                                         subdivision=None, duration=60, recover=True)
        run.started = self.clock.time()
        run.random = random.Random(1)
        run.cases = io.StringIO()
        run.prepare_auxiliary = mock.Mock()
        run.addresses, run.positions, run.per_drive = [1], {}, collections.Counter()
        run.select = mock.Mock()
        run.state = mock.Mock(return_value={"position": 0})
        run.case_plain = mock.Mock(side_effect=bench.Unresponsive("move relative"))
        run.recover = mock.Mock(side_effect=AssertionError("must not start recovery motion"))
        self.assertEqual(run.loop(), 4)
        self.check_record(run, False)
        run.recover.assert_not_called()
        self.assertEqual(run.console.retained, [17, 91])
        self.assertEqual(run.anomalies[-1]["kind"], "CONSOLE_UNRESPONSIVE")


class ScriptedConsole:
    """Answers the selection and read commands of the multi-drive stress mode."""
    def __init__(self, states=None, saved=True):
        self.sent, self.retained, self.ring = [], [], []
        self.states = states or {}
        self.saved = saved
        self.next_id = 1

    def pump(self, seconds):
        pass

    def release_all(self):
        pass

    def send(self, command, wait=None):
        self.sent.append(command)
        ident = self.next_id
        self.next_id += 1
        if command == "motion-profile inspect":
            return dict(ok=True, pending=False, saved=self.saved, _id=ident)
        if command == "motion-profile forget":
            self.saved = False
            return dict(ok=True, _id=ident)
        if command.startswith("useaddr ") or command.startswith("wiring ") or command == "probe":
            return dict(ok=True, result="done", _id=ident)
        if command.startswith("read state "):
            return dict(ok=True, result="accepted", _id=ident, address=int(command.split()[-1]))
        return dict(ok=True, _id=ident)

    def terminal(self, reply, wait):
        address = reply["address"]
        motion, alarm, position = self.states[address]
        return dict(ok=True, state_blocks=[dict(raw_motion=motion, raw_alarm=alarm,
                                                position_words=[position >> 16, position & 0xFFFF])])


class MultiDriveTests(unittest.TestCase):
    def run_fixture(self, console, addresses=(1, 2, 4), cross_reads=True):
        run = bench.Run.__new__(bench.Run)
        run.console = console
        run.args = types.SimpleNamespace(cross_reads=cross_reads)
        run.addresses, run.address, run.selected = list(addresses), addresses[0], addresses[0]
        run.auxiliary_ready, run.positions = set(addresses), {}
        run.counts = collections.Counter()
        run.random = random.Random(3)
        return run

    def test_selection_releases_the_snapshot_and_selects_the_drive(self):
        console = ScriptedConsole()
        run = self.run_fixture(console)
        run.select(4)
        self.assertEqual(console.sent[:3], ["motion-profile inspect", "motion-profile forget", "useaddr 4"])
        self.assertEqual((run.address, run.selected), (4, 4))
        console.sent.clear()
        run.select(4)
        self.assertEqual(console.sent, [])

    def test_declared_wiring_follows_each_selection(self):
        console = ScriptedConsole()
        run = self.run_fixture(console)
        run.args.declare_unconnected = True
        run.select(2)
        useaddr = console.sent.index("useaddr 2")
        self.assertEqual(console.sent[useaddr + 1:useaddr + 7],
                         [f"wiring {t} unconnected" for t in ("x0", "x1", "x2", "x3", "y0", "y1")])
        console.sent.clear()
        run.args.declare_unconnected = False
        run.select(4)
        self.assertFalse([c for c in console.sent if c.startswith("wiring")])

    def test_stop_targets_the_selected_drive(self):
        console = ScriptedConsole()
        run = self.run_fixture(console)
        run.select(2)
        console.terminal = lambda reply, wait: dict(ok=True)
        run.stop("fast")
        self.assertEqual(console.sent[-1], "stop fast 2")

    def test_cross_read_accepts_a_still_drive_and_records_its_position(self):
        console = ScriptedConsole(states={2: (1, 0, 500), 4: (1, 0, 500)})
        run = self.run_fixture(console)
        run.positions = {2: 500, 4: 500}
        for _ in range(4):
            run.cross_read()
        self.assertEqual(run.counts["cross_read"], 4)
        self.assertTrue(all(c.split()[-1] in ("2", "4") for c in console.sent))

    def test_cross_read_fails_on_motion_alarm_or_moved_position(self):
        for state in ((1 | bench.RUNNING, 0, 500), (1, 1, 500), (1, 0, 501)):
            with self.subTest(state=state):
                console = ScriptedConsole(states={2: state})
                run = self.run_fixture(console, addresses=(1, 2))
                run.positions = {2: 500}
                with self.assertRaises(bench.Anomaly) as caught:
                    run.cross_read()
                self.assertEqual(caught.exception.kind, "OTHER_DRIVE")

    def test_reference_position_waits_for_post_stop_settling(self):
        clock = Clock()
        positions = iter([100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99])

        class Settling(ScriptedConsole):
            def pump(self, seconds):
                clock.sleep(seconds)

        console = Settling()
        run = self.run_fixture(console, addresses=(2,))
        run.state = lambda check=True: {"position": next(positions), "raw_speed": 0}
        with mock.patch.object(bench.time, "time", clock.time):
            self.assertEqual(run.settled_position(hold=2.0), 99)
        self.assertEqual(run.counts["settle_change"], 1)

    def test_unsettled_position_is_an_anomaly(self):
        clock = Clock()
        counter = iter(range(1000))

        class Settling(ScriptedConsole):
            def pump(self, seconds):
                clock.sleep(seconds)

        run = self.run_fixture(Settling(), addresses=(2,))
        run.state = lambda check=True: {"position": next(counter), "raw_speed": 0}
        with mock.patch.object(bench.time, "time", clock.time):
            with self.assertRaises(bench.Anomaly) as caught:
                run.settled_position(hold=2.0, limit=8.0)
        self.assertEqual(caught.exception.kind, "NOT_SETTLED")

    def test_single_drive_runs_send_no_cross_reads(self):
        console = ScriptedConsole()
        run = self.run_fixture(console, addresses=(1,))
        run.cross_read()
        self.assertEqual(console.sent, [])


if __name__ == "__main__":
    unittest.main()
