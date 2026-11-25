"""Offline checks of the wired soak driver (wired bench 20): plan, verdicts,
per-case restoration, stops and bookkeeping. No port is opened."""
import copy
import json
from pathlib import Path
import random
import sys
import tempfile
import types
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import bench_soak_wired as soak  # noqa: E402
import bench_stress as stress  # noqa: E402

USED = {"relay1": 0, "relay2": 0}


class PlanTests(unittest.TestCase):
    def test_every_soak_case_passes_the_plan_check(self):
        steps = soak.plan_steps(list(soak.CASES))
        self.assertEqual(stress.plan_check(steps, USED), [])
        stop = [s for s in steps if s["case"] == "stop_input"]
        self.assertEqual([s["role"] for s in stop], ["attributed_start", "stop"])
        self.assertEqual(stop[1]["members"], [4])
        group = next(s for s in steps if s["case"] == "start_group")
        self.assertEqual(group["members"], [1, 2, 4])
        self.assertEqual({s["role"] for s in steps if soak.CASES[s["case"]][0] not in ("starts", "pt")}, {"serial"})

    def test_a_start_function_outside_the_case_is_rejected(self):
        steps = soak.plan_steps(["start_relative"])
        steps[0]["assignments"][2]["x0"] = 7
        self.assertRegex(stress.plan_check(steps, USED)[0], "drive 2 x0 = 7 does not match role attributed_start")

    def test_final_restoration_leaves_velocity_words_to_their_cases(self):
        plan = {"admissible": True, "steps": [{"key": "io.x0"}, {"key": "jog.speed", "snapshot": "velocity"}]}
        self.assertEqual(soak.final_plan(plan)["steps"], [{"key": "io.x0"}])

    def test_options(self):
        base = ["--run-dir", "x", "--firmware", "f.bin", "--commit", "c"]
        with mock.patch("sys.stderr"), self.assertRaises(SystemExit):
            soak.arguments(base)  # --relay-used is required.
        with mock.patch("sys.stderr"), self.assertRaises(SystemExit):
            soak.arguments(base + ["--relay-used", "relay1=1,relay2=2", "--port", "COM15"])
        args = soak.arguments(base + ["--relay-used", "relay1=1,relay2=2", "--cases", "5"])
        self.assertEqual((args.repetitions, args.cases, args.relay_used), (1, 5, {"relay1": 1, "relay2": 2}))


class VerdictTests(unittest.TestCase):
    def test_start_problems_list_every_non_pass_level(self):
        verdicts = {"drive-reported": ("PASS", None), "controller-timestamped": ("indeterminate", "edge lost")}
        with mock.patch.object(soak.starts, "case_verdicts", return_value=verdicts):
            self.assertEqual(soak.start_problems("start_relative", "A2", {}),
                             ["start_relative controller-timestamped indeterminate: edge lost"])

    def test_group_problems(self):
        ok = dict(label="g 1", ok=True, errors={1: 0, 2: -2, 4: 1})
        self.assertEqual(soak.group_problems("g", [ok, dict(label="g 2", ok=False, stopped_by_case=True)]), [])
        for trial, text in ((dict(label="g 1", ok=False, state="refused", error="stale"), "plan refused"),
                            (dict(ok, errors={2: 3}), "endpoint errors"),
                            (dict(label="g 1", ok=False, stopped_by_case=True, stop_problems=["member 2 moving"]), "stop")):
            with self.subTest(text):
                self.assertRegex(soak.group_problems("g", [trial])[0], text)

    def test_velocity_problems(self):
        good = {"run": {"terminal": {"ok": True}, "settle": {"moving": False}},
                "restored": [{"key": "velocity.window", "restored": True, "uncertain": False}]}
        self.assertEqual(soak.velocity_problems("velocity", good), [])
        for change, text in ((dict(run={"refused": {"result": "busy"}}), "refused"),
                             (dict(run={"terminal": {"ok": False}, "settle": {"moving": False}}), "terminal"),
                             (dict(run={"terminal": {"ok": True}, "settle": {"moving": True}}), "still moving"),
                             (dict(restored=[{"key": "velocity.window", "restored": False}]), "not restored")):
            with self.subTest(text):
                self.assertRegex(" ".join(soak.velocity_problems("velocity", dict(good, **change))), text)

    def test_restoration_problems_name_each_field_and_drive(self):
        still = {"alarm": 0, "running": False, "raw_speed": 0, "position": 0}
        diff = [{"drive": 1, "group": "io", "key": "x0", "status": "equal"},
                {"drive": 4, "group": "io", "key": "x3", "status": "different"}]
        with mock.patch.object(soak.stops, "moving", side_effect=lambda s: s.get("running")):
            problems = soak.restoration_problems(diff, {1: still, 2: dict(still, running=True), 4: dict(still, alarm=5)})
        self.assertEqual(problems, ["drive 4 io x3: different", "drive 2 not read stopped and alarm-free",
                                    "drive 4 not read stopped and alarm-free"])


class FakeLive:
    def __init__(self):
        self.events, self.samples, self.views, self.snapshots = [], [], [], {}
        self.console, self.timeout, self.run = object(), 5.0, object()
        self.current, self.group, self.address = "BASE", False, 2

    def call(self, name, **kwargs):
        self.events.append(name)
        return {"ok": True, "outputs": [{"name": "relay1", "ons": 4}]} if name == "bench" else {"ok": True}

    checked = call


class LoopTests(unittest.TestCase):
    def fixture(self, folder, cases=None, **runner):
        args = types.SimpleNamespace(seed=1, only=["velocity"], relay_used=dict(USED), duration=1e6, cases=cases,
                                     health_every=2)
        s = soak.Soak(FakeLive(), FakeLive(), args, Path(folder))
        s.runners["velocity"] = runner.get("run", lambda name, argument: ({"ok": True}, []))
        s.restore_case = runner.get("restore", lambda: [])
        return s

    def test_case_limit_health_and_relay_counts(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(soak.bench_switch, "headroom_problem",
                                                                        return_value=None):
            s = self.fixture(folder, cases=4)
            self.assertEqual(s.loop(), 0)
            self.assertEqual((s.completed, s.ended, s.counts), (4, "case limit", {"velocity": 4}))
            lines = (Path(folder) / "health.jsonl").read_text(encoding="utf-8").splitlines()
            self.assertEqual(len(lines), 3)  # Every second case and at the end.
            self.assertEqual(json.loads(lines[0])["relay_ops"]["relay1"]["total"], 0)

    def test_evidence_headroom_ends_the_soak_without_an_anomaly(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(soak.bench_switch, "headroom_problem",
                                                                        return_value="headroom low"):
            s = self.fixture(folder)
            self.assertEqual(s.loop(), 0)
            self.assertEqual((s.completed, s.ended), (0, "evidence headroom: headroom low"))

    def test_failure_sends_the_prepared_stops_before_the_incident_and_ends(self):
        order = []

        def failing(name, argument):
            raise soak.probe.BenchError("console lost")
        for runner in ({"run": failing}, {"restore": lambda: ["drive 4 io x3: different"]}):
            with self.subTest(runner=sorted(runner)), tempfile.TemporaryDirectory() as folder, \
                    mock.patch.object(soak.bench_switch, "headroom_problem", return_value=None), \
                    mock.patch.object(soak.multi, "prepared_stops", side_effect=lambda live: order.append("stops")):
                s = self.fixture(folder, **runner)
                incident = s.incident
                s.incident = lambda *a: order.append("incident") or incident(*a)
                self.assertEqual(s.loop(), 3)
                self.assertEqual(order[-2:], ["stops", "incident"])
                index = [json.loads(line) for line in (Path(folder) / "incidents.jsonl").read_text(encoding="utf-8").splitlines()]
                self.assertEqual(index[0]["case"], "velocity")
                self.assertTrue((Path(folder) / index[0]["file"]).exists())
                self.assertEqual((s.completed, s.ended), (0, "anomaly"))

    def test_velocity_words_are_restored_on_the_same_drive_even_when_the_run_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            s = self.fixture(folder)
            s.random = random.Random(2)
            s.velocity_plans = {2: {"steps": ["plan 2"]}, 4: {"steps": ["plan 4"]}}
            restore = mock.Mock(return_value=[{"key": "velocity.window", "restored": True, "uncertain": False}])
            with mock.patch.object(soak.records, "use_drive"), mock.patch.object(soak.velocity, "qualify_velocity"), \
                    mock.patch.object(soak.velocity, "velocity_run", side_effect=soak.probe.BenchError("lost")), \
                    mock.patch.object(soak, "_stop_restore", restore):
                with self.assertRaises(soak.probe.BenchError):
                    s.run_velocity("velocity", None)
            address = s.vlive.address
            self.assertIn(address, soak.VELOCITY_DRIVES)
            self.assertEqual(restore.call_args.args[3], s.velocity_plans[address])
            self.assertEqual(restore.call_args.kwargs["owner"], address)
            self.assertNotIn("velocity", s.vlive.snapshots)

    def test_trim_bounds_host_memory(self):
        with tempfile.TemporaryDirectory() as folder:
            s = self.fixture(folder)
            s.live.events, s.live.views = list(range(5000)), [1, 2]
            s.trim(keep=10)
            self.assertEqual((s.live.events, s.live.views), (list(range(4990, 5000)), []))


class ScenarioTests(unittest.TestCase):
    def test_scenario_record_is_valid_and_restorable(self):
        baseline = soak.records.load_baseline()
        args = types.SimpleNamespace(commit="c", port="COM13", case_id="K1", duration=60.0, cases=None, image_level="20")
        record = json.loads(json.dumps(soak.scenario_record(args, {"path": "f.bin", "sha256": "0" * 64}, baseline)))
        data = dict(schema=soak.records.SCENARIO_SCHEMA, prompt="20", slug="soak", date="2026-11-25",
                    run="2026-11-25_wb20_soak", **copy.deepcopy(record))
        self.assertEqual(soak.records.check_scenario(data), [])
        self.assertTrue(record["restoration_plan"]["admissible"])
        keys = {(e["drive"], e["key"]) for e in record["write_list"]}
        self.assertTrue({(2, "jog.speed"), (4, "io.y0"), (1, "io.x0"), (1, "profile.target"), (2, "io.x1"),
                         (4, "records.position.target")} <= keys)
        self.assertEqual(len([e for e in record["write_list"] if e["drive"] == 1 and e["key"].startswith("records.")]), 32)
        for address in soak.PT_DRIVES:
            plan = soak.pt_plan(baseline, address, "20")
            self.assertTrue(plan["admissible"])
            self.assertFalse([s for s in plan["steps"] if s["key"].startswith("io.")])  # restore_layout owns them.

    def test_merge_facts_is_a_union(self):
        merged = soak.merge_facts({"4": {"assign": {"x0": ["a"]}}, "n": 1}, {"4": {"assign": {"x0": ["a", "b"], "x1": ["c"]}}})
        self.assertEqual(merged, {"4": {"assign": {"x0": ["a", "b"], "x1": ["c"]}}, "n": 1})


def pt_trial(**changes):
    trial = {"run": {"verdict": ("PASS", None)}, "trigger": {"admitted": False, "reason": soak.SHORT_HOLD},
             "ons_before": 7, "ons_after": 7, "continued_motion": []}
    trial.update(changes)
    return trial


class PtTests(unittest.TestCase):
    """The PT trigger case: plan roles, verdict, relay accounting and in-case restoration."""

    def test_plan_admits_the_pt_layout_on_each_drive_over_baseline_others(self):
        steps = soak.plan_steps(["pt_trigger"])
        self.assertEqual(len(steps), 2 * len(soak.PT_DRIVES))
        self.assertEqual({(s["role"], s["output"], s["function"]) for s in steps},
                         {("selector", "relay2", 14), ("attributed_start", "mosfet1", 12)})
        self.assertEqual(stress.plan_check(steps, USED, {"relay2": 100}), [])
        self.assertRegex(stress.plan_check(steps, None)[0], "not stated")
        self.assertRegex(stress.plan_check(steps, {"relay1": 0, "relay2": 9950}, {"relay2": 100})[0], "budget")
        for terminal, function in (("x1", 14), ("x0", 7), ("x1", 4)):
            with self.subTest(terminal=terminal, function=function):
                bad = copy.deepcopy(steps[:2])  # Drive 1's case; drive 2 outside it.
                for step in bad:
                    step["assignments"][2][terminal] = function
                self.assertTrue(stress.plan_check(bad, USED))

    def test_pt_problems(self):
        self.assertEqual(soak.pt_problems("pt", pt_trial()), [])
        self.assertRegex(soak.pt_problems("pt", None)[0], "no trial")
        for change, text in ((dict(run={"verdict": ("FAIL", "change 790, expected 800")}), "expected 800"),
                             (dict(trigger={"admitted": True}), "short PIN hold"), (dict(ons_after=8), "short PIN hold"),
                             (dict(continued_motion=[4]), "continued motion")):
            with self.subTest(text):
                self.assertRegex(" ".join(soak.pt_problems("pt", pt_trial(**change))), text)

    def test_relay_problems(self):
        self.assertEqual(soak.relay_problems("pt", 2, 10.0), [])
        self.assertRegex(soak.relay_problems("pt", 4, 60.0)[0], "planned at most 3")
        self.assertRegex(soak.relay_problems("pt", 3, 3.0)[0], "3 relay2 operations in 3.0 s")

    def fixture(self, folder, ons_after=12):
        args = types.SimpleNamespace(seed=1, only=["pt_trigger"], relay_used=dict(USED), duration=1e6, cases=None,
                                     health_every=100)
        ptlive = FakeLive()
        ptlive.call = lambda name, **kwargs: {"ok": True, "outputs": [{"name": "relay2", "ons": ons_after}]}
        s = soak.Soak(FakeLive(), FakeLive(), args, Path(folder), ptlive)
        s.pt_plans = {a: {"steps": ["plan %d" % a]} for a in soak.PT_DRIVES}
        s.relays.observe([{"name": "relay2", "ons": 10}])
        return s

    def test_layout_records_and_mode_are_restored_on_the_same_drive_even_when_the_trial_fails(self):
        order = []
        restore = mock.Mock(side_effect=lambda *a, **k: order.append("records") or
                            [{"key": "records.position.target", "index": 1, "restored": True, "uncertain": False}])
        with tempfile.TemporaryDirectory() as folder:
            s = self.fixture(folder)
            s.relays.last_on["relay2"] = soak.time.monotonic()
            with mock.patch.object(soak.pt, "setup_table", side_effect=lambda live, a, mode: order.append("setup")),                     mock.patch.object(soak.pt, "run_regression", side_effect=soak.probe.BenchError("lost")),                     mock.patch.object(soak.pt, "outputs_off", side_effect=lambda live: order.append("off")),                     mock.patch.object(soak.pt, "restore_layout", side_effect=lambda live, a: order.append("layout")),                     mock.patch.object(soak, "_stop_restore", restore),                     mock.patch.object(soak.time, "sleep", side_effect=lambda s_: order.append("wait %.0f" % s_)):
                with self.assertRaises(soak.probe.BenchError):
                    s.run_pt("pt_trigger", None)
            self.assertEqual(order, ["wait 2", "setup", "off", "layout", "records"])
            address = restore.call_args.kwargs["owner"]
            self.assertEqual(restore.call_args.args[3], s.pt_plans[address])
            self.assertEqual(s.relays.total("relay2"), 2)  # 10 -> 12 counted from the bench view.

    def test_a_passing_case_counts_its_relay_operations(self):
        restore = mock.Mock(return_value=[{"key": "records.position.target", "index": 1, "restored": True}])
        with tempfile.TemporaryDirectory() as folder:
            s = self.fixture(folder, ons_after=16)
            with mock.patch.object(soak.pt, "setup_table"), mock.patch.object(soak.pt, "outputs_off"),                     mock.patch.object(soak.pt, "restore_layout"), mock.patch.object(soak, "_stop_restore", restore),                     mock.patch.object(soak.pt, "run_regression", return_value=pt_trial()):
                detail, problems = s.run_pt("pt_trigger", None)
            self.assertEqual(detail["relay_operations"], 6)
            self.assertRegex(problems[0], "6 relay2 operations, planned at most 3")

    def test_pt_refuses_to_start_outside_the_baseline_state(self):
        with tempfile.TemporaryDirectory() as folder:
            s = self.fixture(folder)
            s.live.current = "NB"
            with mock.patch.object(soak.pt, "setup_table") as setup, self.assertRaises(soak.probe.BenchError):
                s.run_pt("pt_trigger", None)
            setup.assert_not_called()

    def test_relay_budget_ends_the_soak_before_a_pt_case(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(soak.bench_switch, "headroom_problem",
                                                                        return_value=None):
            s = self.fixture(folder)
            s.relays.used["relay2"] = stress.RELAY_BUDGET - soak.PT_RELAY_OPS_MAX + 1  # One more case would overrun.
            s.runners["pt"] = mock.Mock()
            self.assertEqual(s.loop(), 0)
            self.assertEqual(s.ended, "relay budget")
            s.runners["pt"].assert_not_called()

    def test_case_limited_plan_states_its_worst_case_relay_operations(self):
        with tempfile.TemporaryDirectory() as folder:
            s = self.fixture(folder)
            s.names, s.args.cases = list(soak.CASES), 1000
            share = soak.CASES["pt_trigger"][1] / sum(c[1] for c in soak.CASES.values())
            self.assertEqual(s.relay_planned(), {"relay2": soak.math.ceil(2 * 1000 * share * soak.PT_RELAY_OPS_MAX)})
            self.assertEqual(s.plan_check(), [])
            s.args.relay_used = {"relay1": 0, "relay2": stress.RELAY_BUDGET}
            self.assertRegex(s.plan_check()[0], "exceed the budget")


class DeclarationTests(unittest.TestCase):
    """Wired bench 20 A7: no selection change or profile forget inside an armed session."""

    def soak(self, folder):
        args = types.SimpleNamespace(seed=1, only=None, relay_used=dict(USED), duration=1, cases=None, health_every=1)
        return soak.Soak(FakeLive(), FakeLive(), args, Path(folder), FakeLive())

    def test_declarations_are_shared_before_any_arm(self):
        for live in (FakeLive(),):
            live.record = {}
        with tempfile.TemporaryDirectory() as folder:
            s = self.soak(folder)
            for live in (s.live, s.vlive, s.ptlive):
                live.record = {}
            declarations = {"verdict": "PASS", "rows": [], "problems": []}
            s.share_declarations(declarations)
            self.assertTrue(all(live.record["declarations"] is declarations for live in (s.live, s.vlive, s.ptlive)))
            with self.assertRaises(soak.probe.BenchError):
                s.share_declarations({"verdict": "FAIL", "problems": ["drive 4 board differs"]})

    def test_interlock_with_shared_declarations_never_reads_them_again(self):
        """The regression: with the session's declarations the interlock selects no drive."""
        records = soak.records
        console = types.SimpleNamespace(command=mock.Mock(side_effect=AssertionError("no console traffic expected")))
        scenario = {"declarations": {"verdict": "PASS", "rows": [], "problems": []}, "max_age_ms": 300,
                    "running_members": []}
        with mock.patch.object(records, "check_declarations", side_effect=AssertionError("selection change")) as check, \
                mock.patch.object(records, "use_drive", side_effect=AssertionError("selection change")), \
                mock.patch.object(records, "check_input_agreement",
                                  return_value={"verdict": "FAIL", "problems": ["stop here"]}):
            entry = records.interlock(console, "mosfet2", scenario)
        check.assert_not_called()
        console.command.assert_not_called()
        self.assertEqual(entry["row"], "L1")

    def test_case_records_carry_duration_and_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            s = self.soak(folder)
            s.live.console = types.SimpleNamespace(evidence_budget=types.SimpleNamespace(bytes=2500000, records=4000))
            s.case_record("stop_input", {"verdicts": {"drive-reported": ["PASS", None]}}, [], None, soak.time.monotonic())
            line = json.loads((Path(folder) / "cases.jsonl").read_text(encoding="utf-8"))
            self.assertEqual((line["case"], line["evidence_bytes"], line["verdicts"]["drive-reported"][0]),
                             ("stop_input", 2500000, "PASS"))

    def test_evidence_budget_option(self):
        base = ["--run-dir", "x", "--firmware", "f.bin", "--commit", "c", "--relay-used", "relay1=1,relay2=2"]
        self.assertEqual(soak.arguments(base).evidence_mib, 512)
        with mock.patch("sys.stderr"), self.assertRaises(SystemExit):
            soak.arguments(base + ["--evidence-mib", "8"])


class PreparedStopTests(unittest.TestCase):
    def test_an_unavailable_drive_is_bound_by_one_read_then_stopped(self):
        """Wired bench 20 b4: a stop to a drive with no checked reply since the last selection is refused."""
        live = FakeLive()
        live.detail = {}
        sent = []

        def call(name, **kwargs):
            sent.append((name, kwargs.get("address")))
            if name == "stop" and kwargs["address"] == 4 and sent.count(("stop", 4)) == 1:
                return {"ok": False, "result": "unavailable"}
            return {"ok": True, "result": "accepted"}
        live.call = call
        soak.multi.prepared_stops(live)
        self.assertEqual(sent, [("group-stop", None), ("stop", 1), ("stop", 2), ("stop", 4), ("read-state", 4), ("stop", 4)])
        self.assertEqual(live.detail, {})


if __name__ == "__main__":
    unittest.main()
