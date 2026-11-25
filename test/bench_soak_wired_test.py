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
        self.assertEqual({s["role"] for s in steps if soak.CASES[s["case"]][0] != "starts"}, {"serial"})

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
        args = types.SimpleNamespace(commit="c", port="COM13", case_id="K1", duration=60.0, cases=None, image_level="19")
        record = json.loads(json.dumps(soak.scenario_record(args, {"path": "f.bin", "sha256": "0" * 64}, baseline)))
        data = dict(schema=soak.records.SCENARIO_SCHEMA, prompt="20", slug="soak", date="2026-11-25",
                    run="2026-11-25_wb20_soak", **copy.deepcopy(record))
        self.assertEqual(soak.records.check_scenario(data), [])
        self.assertTrue(record["restoration_plan"]["admissible"])
        keys = {(e["drive"], e["key"]) for e in record["write_list"]}
        self.assertTrue({(2, "jog.speed"), (4, "io.y0"), (1, "io.x0"), (1, "profile.target")} <= keys)


if __name__ == "__main__":
    unittest.main()
