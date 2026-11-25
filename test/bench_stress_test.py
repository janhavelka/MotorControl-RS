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
        run.relays, run.completed, run.by_design, run.group_active = bench.RelayCounter(), 0, set(), False
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
        self.selected, self.boards, self.fixture, self.legacy = 1, {}, "absent", False

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
        if command == "wiring" and self.legacy:
            return dict(ok=True, result="done", _id=ident)
        if command == "wiring":
            return dict(ok=True, result="done", _id=ident, board=self.boards.get(self.selected), fixture=self.fixture,
                        **({"fixture_error": "duplicate_address"} if self.fixture == "invalid" else {}))
        if command.startswith("useaddr "):
            self.selected = int(command.split()[1])
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
        self.assertEqual(console.sent[useaddr + 1], "wiring")  # Board declaration checked first.
        self.assertEqual(console.sent[useaddr + 2:useaddr + 8],
                         [f"wiring {t} unconnected" for t in ("x0", "x1", "x2", "x3", "y0", "y1")])
        console.sent.clear()
        run.args.declare_unconnected = False
        run.select(4)
        self.assertFalse([c for c in console.sent if c.startswith("wiring")])

    def test_declared_wiring_is_refused_over_the_board_table(self):
        bench_boards = {1: dict(inputs=[2] * 4, outputs=[1, 1]), 4: dict(inputs=[2] * 4, outputs=[2, 1])}
        for boards, fixture, message in ((bench_boards, "applied", "board declares connected terminals on drive 4"),
                                         ({}, "invalid", "invalid"), (None, None, "lacks the board declaration")):
            console = ScriptedConsole()
            if boards is None:
                console.legacy = True
            else:
                console.boards, console.fixture = boards, fixture
            run = self.run_fixture(console)
            run.args.declare_unconnected = True
            with self.subTest(fixture=fixture), self.assertRaisesRegex(bench.Anomaly, message):
                run.select(4)
            self.assertFalse([c for c in console.sent if c.startswith("wiring ")])
        console = ScriptedConsole()
        console.boards, console.fixture = bench_boards, "applied"
        run = self.run_fixture(console)
        run.args.declare_unconnected = True
        run.select(2)  # Unlisted in this table: the stated fact is admitted.
        self.assertEqual(len([c for c in console.sent if c.startswith("wiring ")]), 6)

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


class UsbModeTests(unittest.TestCase):
    """Wired bench 18: the slow-reader pause, the saturation figure and the skipped cases."""

    def test_saturation_loss_counts_only_unexplained_lines(self):
        self.assertEqual(bench.saturation_loss(10000, 9990, 5, 12)["uncounted"], 3)
        self.assertEqual(bench.saturation_loss(10000, 10000, 0, 0)["uncounted"], 0)
        self.assertIsNone(bench.saturation_loss(10, 9, None, 4)["uncounted"])

    def test_slow_reader_pause_extends_the_wait_it_interrupts(self):
        console = bench.Console.__new__(bench.Console)
        console.ring, console.pauses, console.terminals = collections.deque(maxlen=10), [], {}
        console.pause_chance, console.rng = 1.0, random.Random(1)
        clock = Clock()
        arrivals = {"at": 4.0}

        def pump(seconds):
            clock.sleep(max(seconds, 0.01))
            if clock.now >= arrivals["at"]:
                console.terminals[5] = {"id": 5}
        console.pump = pump
        with mock.patch.object(bench.time, "time", clock.time), mock.patch.object(bench.time, "sleep", clock.sleep):
            item = console.terminal({"_id": 5}, 1.0)
        self.assertEqual(item, {"id": 5})
        self.assertEqual(len(console.pauses), 1)
        self.assertTrue(0.5 <= console.pauses[0][1] <= 5.0)

    def test_safety_stop_reaches_every_drive_and_survives_a_dead_console(self):
        run = bench.Run.__new__(bench.Run)
        run.addresses = [1, 2, 4]
        console = ScriptedConsole()
        console.terminal = lambda reply, wait: {"outcome": "observed"}
        released = []
        console.release_all = lambda: released.append(True)
        run.console = console
        run.safety_stop()
        self.assertEqual([c for c in console.sent], ["stop fast 1", "stop fast 2", "stop fast 4"])
        self.assertEqual(run.safety_stops[0][2], "observed")
        self.assertEqual(released, [True])

        def dead(command, wait=None):
            raise bench.Unresponsive(command)
        console.send = dead
        run.safety_stop()
        self.assertEqual([s[1] for s in run.safety_stops], ["failed"] * 3)

    def test_skipped_home_keeps_the_auxiliary_word(self):
        run = bench.Run.__new__(bench.Run)
        run.args = types.SimpleNamespace(skip=("home",))
        run.console = ScriptedConsole()
        run.prepare_auxiliary()
        self.assertEqual(run.console.sent, [])


def assignments(**terminals):
    """Planned functions of drives 1, 2 and 4; ``x0={4: 7}`` sets one terminal of one drive."""
    plan = {d: {t: 0 for t in bench.TERMINALS} for d in bench.DRIVES}
    for terminal, functions in terminals.items():
        for drive, function in functions.items():
            plan[drive][terminal] = function
    return plan


def start_step(**changes):
    step = dict(case="arm_start", role="attributed_start", route="arm_trigger", output="mosfet1",
                members=[4], function=7, assignments=assignments(x0={4: 7}))
    step.update(changes)
    return step


class PlanCheckTests(unittest.TestCase):
    """Wired bench 20 item 3: every rejection of the plan check."""

    def test_attributed_starts_through_the_trigger_route_are_admitted(self):
        group = start_step(case="arm_group", members=[1, 2, 4], assignments=assignments(x0={1: 7, 2: 7, 4: 7}))
        stop = dict(case="stop_input", role="stop", route="bench_on", output="mosfet2", members=[4],
                    assignments=assignments(x3={4: 5}))
        negative = dict(case="negative_pulse", role="negative", route="bench_on", output="mosfet2", members=[],
                        assignments=assignments())
        self.assertEqual(bench.plan_check([start_step(), group, stop, negative] + bench.mix_plan(["plain", "mcu_reset"])), [])

    def test_raw_edge_as_attributed_start_is_rejected(self):
        problems = bench.plan_check([start_step(route="bench_on")])
        self.assertEqual(len(problems), 1)
        self.assertIn("raw edge used as the start", problems[0])

    def test_edge_role_must_match_the_planned_assignments(self):
        cases = {
            "non-member start function": start_step(assignments=assignments(x0={2: 7, 4: 7})),
            "member function differs": start_step(case="arm_group", members=[1, 2, 4],
                                                  assignments=assignments(x0={1: 7, 2: 8, 4: 7})),
            "stop line with a start": dict(case="stop_input", role="stop", route="bench_on", output="mosfet2",
                                           members=[4], assignments=assignments(x3={1: 7, 4: 5})),
            "switch member not a switch": dict(case="limit", role="switch", route="bench_on", output="relay2",
                                               members=[1], assignments=assignments(x1={1: 5})),
            "negative onto a start": dict(case="negative_pulse", role="negative", route="bench_on", output="mosfet1",
                                          members=[], assignments=assignments(x0={2: 7})),
            "selector through the trigger": dict(case="pin", role="selector", route="arm_trigger", output="relay1",
                                                 members=[4], function=13, assignments=assignments(x2={4: 13})),
        }
        for label, step in cases.items():
            with self.subTest(label):
                problems = bench.plan_check([step], relay_used={"relay1": 0, "relay2": 0})
                self.assertEqual(len(problems), 1, problems)
                self.assertRegex(problems[0], "does not match role")

    def test_incomplete_assignments_and_unknown_roles_are_rejected(self):
        partial = assignments(x0={4: 7})
        del partial[2]
        self.assertRegex(bench.plan_check([start_step(assignments=partial)])[0], "incomplete")
        self.assertRegex(bench.plan_check([dict(case="x", role="pulse")])[0], "unknown edge role")

    def test_reset_without_interlock_is_rejected(self):
        self.assertRegex(bench.plan_check([dict(case="mcu_reset", role="reset")])[0], "without the before-reset interlock")
        self.assertEqual(bench.plan_check(bench.mix_plan(["mcu_reset", "simple_boot"])), [])

    def test_relay_budget_overrun_and_unstated_totals_are_rejected(self):
        limit = dict(case="limit", role="switch", route="bench_on", output="relay2", members=[1, 2, 4],
                     assignments=assignments(x1={1: 2, 2: 2, 4: 2}))
        self.assertEqual(bench.plan_check([limit], {"relay1": 0, "relay2": 9000}, {"relay2": 1000}), [])
        self.assertRegex(bench.plan_check([limit], {"relay1": 0, "relay2": 9001}, {"relay2": 1000})[0],
                         "relay2: 10001 operations exceed the budget")
        self.assertRegex(bench.plan_check([limit])[0], "not stated")


class RelayCounterTests(unittest.TestCase):
    def test_counts_survive_a_controller_restart(self):
        counter = bench.RelayCounter({"relay1": 100, "relay2": 7})
        counter.observe([dict(name="relay1", ons=5), dict(name="relay2", ons=0), dict(name="mosfet1", ons=50)])
        counter.observe([dict(name="relay1", ons=8)])
        counter.observe([dict(name="relay1", ons=2)])  # Restart: ons counts from boot again.
        self.assertEqual(counter.totals()["relay1"], dict(before=100, run=5, total=105))
        self.assertEqual(counter.total("relay2"), 7)

    def test_rate_and_budget_limits(self):
        counter = bench.RelayCounter({"relay1": 0, "relay2": bench.RELAY_BUDGET})
        counter.last_on["relay1"] = 10.0
        self.assertAlmostEqual(counter.admit("relay1", 10.5), 1.5)
        self.assertEqual(counter.admit("relay1", 13.0), 0.0)
        with self.assertRaises(bench.Anomaly) as caught:
            counter.admit("relay2", 0.0)
        self.assertEqual(caught.exception.kind, "RELAY_BUDGET")


class WiringAndOptionTests(unittest.TestCase):
    def wiring(self, address, **changes):
        board = bench.records._expected_board(address)
        reply = dict(ok=True, fixture="applied", board=board, sources=["board"] * 6,
                     inputs=board["inputs"], outputs=board["outputs"], bench_output_map="applied",
                     bench_outputs=bench.records.expected_bench_outputs())
        reply.update(changes)
        return reply

    def test_declared_wiring_matches_prompt_02(self):
        for address in (1, 2, 4):
            self.assertIsNone(bench.declared_wiring_problem(self.wiring(address), address))
        self.assertRegex(bench.declared_wiring_problem(self.wiring(4), 1), "differs from the fixture")
        self.assertRegex(bench.declared_wiring_problem(self.wiring(4, outputs=[1, 1]), 4), "host override|differ")
        self.assertRegex(bench.declared_wiring_problem(self.wiring(1, fixture="invalid"), 1), "invalid")
        self.assertRegex(bench.declared_wiring_problem(dict(ok=True), 1), "lacks")

    def test_declared_wiring_is_checked_after_each_selection(self):
        console = ScriptedConsole()
        console.send = mock.Mock(side_effect=lambda c, wait=None: self.wiring(2) if c == "wiring" else dict(ok=True))
        run = MultiDriveTests.run_fixture(None, console)
        run.args.wiring = "declared"
        run.select(2)
        sent = [call.args[0] for call in console.send.call_args_list]
        self.assertIn("wiring", sent)
        self.assertFalse([c for c in sent if c.startswith("wiring ")])  # Checked, never declared.
        console.send = mock.Mock(side_effect=lambda c, wait=None: self.wiring(4) if c == "wiring" else dict(ok=True))
        with self.assertRaisesRegex(bench.Anomaly, "drive 1"):
            run.select(1)

    def test_unconnected_declaration_and_declared_wiring_are_refused_together(self):
        args = types.SimpleNamespace(declare_unconnected=True, wiring="declared", cases=None)
        self.assertRegex(bench.option_problems(args)[0], "contradicts")
        args.declare_unconnected, args.cases = False, 0
        self.assertRegex(bench.option_problems(args)[0], "positive")
        self.assertEqual(bench.relay_totals("relay1=3,relay2=4"), {"relay1": 3, "relay2": 4})
        with self.assertRaises(bench.argparse.ArgumentTypeError):
            bench.relay_totals("relay1=3")


class InterlockConsole(ScriptedConsole):
    """Answers read config/state of every drive for the before-reset interlock."""
    def __init__(self, functions, states):
        super().__init__(states=states)
        self.functions = functions

    def send(self, command, wait=None):
        self.sent.append(command)
        words = command.split()
        return dict(ok=True, result="accepted", _id=len(self.sent), kind=words[1], address=int(words[2]))

    def terminal(self, reply, wait):
        if reply["kind"] == "config":
            return dict(ok=True, config=dict(inputs=[dict(function=f, known=True) for f in self.functions[reply["address"]]]))
        motion, alarm, position = self.states[reply["address"]]
        return dict(ok=True, state_blocks=[dict(raw_motion=motion, raw_alarm=alarm, raw_speed=0,
                                                position_words=[position >> 16, position & 0xFFFF])])


class SoakSafetyTests(unittest.TestCase):
    def run_fixture(self, console):
        run = MultiDriveTests.run_fixture(None, console)
        run.by_design, run.group_active = set(), False
        return run

    def test_cross_reads_tolerate_motion_only_on_drives_moving_by_design(self):
        console = ScriptedConsole(states={2: (1 | bench.RUNNING, 0, 900)})
        run = self.run_fixture(console)
        run.addresses, run.positions = [1, 2], {2: 500}
        run.by_design = {2}
        run.cross_read()  # Moving and moved by design: admitted.
        run.by_design = set()
        with self.assertRaises(bench.Anomaly):
            run.cross_read()
        console.states[2] = (1 | bench.RUNNING, 1, 900)
        run.by_design = {2}
        with self.assertRaises(bench.Anomaly):
            run.cross_read()  # An alarm is never by design.

    def test_reset_interlock_reads_every_drive_and_refuses_start_functions(self):
        states = {a: (1, 0, 0) for a in (1, 2, 4)}
        clean = {a: [1, 2, 3, 0] for a in (1, 2, 4)}
        run = self.run_fixture(InterlockConsole(clean, states))
        rows = run.reset_interlock()
        self.assertEqual(sorted(rows), [1, 2, 4])
        self.assertEqual([c for c in run.console.sent if c.startswith("read config")],
                         ["read config 1", "read config 2", "read config 4"])
        for functions, state in (({**clean, 2: [1, 7, 3, 0]}, states),
                                 (clean, {**states, 4: (1 | bench.RUNNING, 0, 0)}),
                                 (clean, {**states, 1: (1, 5, 0)})):
            with self.subTest(functions=functions, state=state), self.assertRaises(bench.Anomaly) as caught:
                self.run_fixture(InterlockConsole(functions, state)).reset_interlock()
            self.assertEqual(caught.exception.kind, "RESET_INTERLOCK")

    def test_mcu_reset_never_resets_without_the_interlock(self):
        run = self.run_fixture(ScriptedConsole())
        run.prepare = mock.Mock(return_value={"position": 0})
        run.reset_interlock = mock.Mock(side_effect=bench.Anomaly("RESET_INTERLOCK", {}))
        run.console.reset_mcu = mock.Mock()
        with self.assertRaises(bench.Anomaly):
            run.case_mcu_reset()
        run.console.reset_mcu.assert_not_called()

    def test_incident_file_and_index_are_written_at_once(self):
        import tempfile, gzip, json
        with tempfile.TemporaryDirectory() as folder:
            run = self.run_fixture(ScriptedConsole())
            run.out, run.moved, run.console.ring = Path(folder), 0, [(1.0, "line")]
            path = run.incident("POSITION", {"delta": 3})
            with gzip.open(path, "rt", encoding="utf-8") as f:
                self.assertEqual(json.loads(f.readline())["incident"], "POSITION")
            index = (Path(folder) / "incidents.jsonl").read_text(encoding="utf-8").splitlines()
            self.assertEqual(json.loads(index[0])["file"], path.name)

    def loop_fixture(self, case, cases=None, recover=False):
        run = bench.Run.__new__(bench.Run)
        run.console = ScriptedConsole()
        run.args = types.SimpleNamespace(mix="standard", only="plain", debug="off", subdivision=None, duration=1e9,
                                         recover=recover, cases=cases)
        run.started, run.random, run.cases = bench.time.time(), random.Random(1), io.StringIO()
        run.addresses, run.address, run.positions, run.per_drive = [1], 1, {}, collections.Counter()
        run.relays, run.completed, run.by_design, run.group_active = bench.RelayCounter(), 0, set(), False
        run.counts, run.anomalies, run.usb_stalls, run.moved = collections.Counter(), [], [], 0
        run.select, run.sample_health, run.summary = mock.Mock(), mock.Mock(), mock.Mock()
        run.state = mock.Mock(return_value={"position": 0})
        run.case_plain = case
        return run

    def test_case_count_limit_ends_the_run(self):
        run = self.loop_fixture(mock.Mock(return_value="ok"), cases=3)
        self.assertEqual(run.loop(), 0)
        self.assertEqual((run.completed, run.case_plain.call_count), (3, 3))

    def test_continued_motion_gets_the_stop_fast_before_the_incident(self):
        order = []
        run = self.loop_fixture(mock.Mock(side_effect=bench.Anomaly("RUNNING_STUCK", "samples")))
        run.safety_stop = mock.Mock(side_effect=lambda: order.append("stop fast"))
        run.incident = mock.Mock(side_effect=lambda name, detail: order.append(name) or Path(name))
        run.postmortem = mock.Mock(side_effect=lambda: order.append("postmortem"))
        self.assertEqual(run.loop(), 3)
        self.assertEqual(order[:3], ["stop fast", "RUNNING_STUCK", "postmortem"])


class GroupStallTests(UsbStopTests):
    """The USB-stall stop covers every moving drive, with a group stop for group cases."""

    def test_group_case_sends_group_stop_and_one_stop_per_moving_drive(self):
        run = self.run_fixture(stopped())
        run.group_active, run.by_design = True, {1, 2}
        sent = run.console.sent
        console_send = run.console.send

        def send(command, wait=None):
            if command == "group stop normal":
                sent.append(command)
                return dict(ok=True, result="accepted", _id=99, group=True)
            if command == "stop normal 2":  # Same strict confirmation as drive 1.
                reply = console_send("stop normal 1", wait)
                sent[-1] = command
                return reply
            return console_send(command, wait)
        run.console.send = send
        terminal = run.console.terminal
        run.console.terminal = lambda reply, wait: dict(ok=True) if reply.get("group") else terminal(reply, wait)
        self.assertFalse(run.usb_stall("console timeout"))  # Drive 2's terminal names target 1: unproven.
        record = run.usb_stalls[-1]
        self.assertEqual([c for c in sent if "stop" in c], ["group stop normal", "stop normal 1", "stop normal 2"])
        self.assertTrue(record["group_stop"]["confirmed"])
        self.assertEqual([s["confirmed"] for s in record["stops"]], [True, False])

    def test_recover_still_ends_the_run_when_the_stall_stop_is_refused_missing_or_uncertain(self):
        for terminal, options in ((stopped(), {"accepted": False}), (None, {}), (stopped(), {"timeout": "terminal"}),
                                  (stopped(completion="not_observed"), {})):
            with self.subTest(terminal=terminal, options=options):
                run = self.run_fixture(terminal, **options)
                run.args = types.SimpleNamespace(mix="standard", only="plain", debug="off",
                                                 subdivision=None, duration=60, recover=True)
                run.started, run.random, run.cases = self.clock.time(), random.Random(1), io.StringIO()
                run.addresses, run.positions, run.per_drive = [1], {}, collections.Counter()
                run.relays, run.completed, run.by_design, run.group_active = bench.RelayCounter(), 0, set(), False
                run.select, run.state = mock.Mock(), mock.Mock(return_value={"position": 0})
                run.case_plain = mock.Mock(side_effect=bench.Unresponsive("move relative"))
                run.recover = mock.Mock(side_effect=AssertionError("must not start recovery motion"))
                self.assertEqual(run.loop(), 4)
                run.recover.assert_not_called()


if __name__ == "__main__":
    unittest.main()
