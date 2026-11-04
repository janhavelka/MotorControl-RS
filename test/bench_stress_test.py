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
        run.case_plain = mock.Mock(side_effect=bench.Unresponsive("move relative"))
        run.recover = mock.Mock(side_effect=AssertionError("must not start recovery motion"))
        self.assertEqual(run.loop(), 4)
        self.check_record(run, False)
        run.recover.assert_not_called()
        self.assertEqual(run.console.retained, [17, 91])
        self.assertEqual(run.anomalies[-1]["kind"], "CONSOLE_UNRESPONSIVE")


if __name__ == "__main__":
    unittest.main()
