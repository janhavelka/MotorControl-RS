"""Wired bench 15 control-and-tuning harness: Y0 intervals from aux1 edges, the
in-position sample, the threshold bisection, the spread comparison, the plan
and the verdicts (no port)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import bench_records as records  # noqa: E402
import bench_tuning as tun  # noqa: E402


def sample(end_us, motion):
    return {"motion": motion, "windows": {"motion": [end_us - 2000, end_us]}}


class IntervalTest(unittest.TestCase):
    def test_running_and_in_position_edges(self):
        # aux1 is inverted: Y0 = 2 on (running) reads inactive, Y0 = 4 off (left position) reads active.
        running = {"t0_us": 0, "t1_us": 3000000, "edges": [{"at_us": 50000, "active": False}, {"at_us": 1150000, "active": True}]}
        self.assertEqual(tun.y0_interval(running, 2), 1100.0)
        arrival = {"t0_us": 0, "t1_us": 3000000, "edges": [{"at_us": 52000, "active": True}, {"at_us": 1153000, "active": False}]}
        self.assertEqual(tun.y0_interval(arrival, 4), 1101.0)
        self.assertIsNone(tun.y0_interval(arrival, 2))  # The wrong first level is not a running edge.
        self.assertIsNone(tun.y0_interval({"t0_us": 0, "t1_us": 1, "edges": []}, 4))
        self.assertIsNone(tun.y0_interval(running, 5))

    def test_edges_outside_the_window_are_ignored(self):
        trial = {"t0_us": 100, "t1_us": 1000, "edges": [{"at_us": 50, "active": False}, {"at_us": 200, "active": False},
                                                        {"at_us": 1200, "active": True}]}
        self.assertIsNone(tun.y0_interval(trial, 2))


class InPositionTest(unittest.TestCase):
    def test_needs_the_move_to_leave_its_position(self):
        samples = [sample(10, 1), sample(20, 1), sample(30, 4), sample(40, 4), sample(50, 1)]
        self.assertEqual(tun.first_in_position(samples, 0), 50)
        self.assertIsNone(tun.first_in_position([sample(10, 1), sample(20, 1)], 0))
        self.assertEqual(tun.first_in_position([sample(10, 0), sample(20, 1)], 0), 20)


class AnalysisTest(unittest.TestCase):
    def test_spread_and_trend(self):
        base = tun.spread([1100.0, 1101.0, 1102.0])
        self.assertEqual(base, {"n": 3, "min": 1100.0, "max": 1102.0, "mean": 1101.0})
        self.assertEqual(tun.beyond([1090.0, 1095.0], base), "earlier")
        self.assertEqual(tun.beyond([1110.0], base), "later")
        self.assertEqual(tun.beyond([1101.5, 1099.0], base), "within")
        self.assertEqual(tun.beyond([None], base), "unknown")
        self.assertIsNone(tun.spread([None]))

    def test_percent_rounds_half_up_like_the_application(self):
        # Python's round() rounds 76.5 to 76; candidatePercent gives 77 (wired bench 15 T5).
        self.assertEqual(tun.percent(102, 75), 77)
        self.assertEqual((tun.percent(409, 75), tun.percent(409, 125), tun.percent(6553, 125)), (307, 511, 8191))

    def test_bisection(self):
        self.assertEqual(tun.bisect_next(4000, 0), 2000)
        self.assertEqual(tun.bisect_next(5, 4), None)
        passing, alarming, steps = 4000, 0, 0
        while (candidate := tun.bisect_next(passing, alarming)) is not None:
            steps += 1
            if candidate < 137:
                alarming = candidate
            else:
                passing = candidate
        self.assertEqual((passing, alarming), (137, 136))
        self.assertLessEqual(steps, tun.THRESHOLD_STEPS)

    def test_brake_edges(self):
        edges = [{"at_us": 900, "active": True}, {"at_us": 1500, "active": False}, {"at_us": 9000, "active": True}]
        self.assertEqual(tun.brake_edges(edges, 1000, 5000), [{"after_ms": 0.5, "active": False}])


class PlanAndVerdictTest(unittest.TestCase):
    def test_every_case_has_levels_and_kind(self):
        for cid, case in tun.CASES.items():
            self.assertTrue(case["levels"], cid)
            self.assertIn(case["kind"], ("baseline", "candidates", "threshold", "gains", "closed-loop", "not-run",
                                         "algorithm", "collision"), cid)
        self.assertEqual(tun.CASES["T12"]["kind"], "not-run")
        self.assertIn("owner away", tun.CASES["T12"]["reason"])

    def test_plan_has_an_admissible_restoration(self):
        class Args:
            address, image_level, cases = 4, "15", ["B", "T1", "T5", "T6", "T7", "T8", "T9", "T13"]
        baseline = records.load_baseline()
        write_list, plan, cases, *_ = tun.plan(Args, baseline, tun.velocity.facts())
        keys = {e["key"] for e in write_list}
        self.assertTrue({"io.y0", "filters.arrival-window", "current-loop.kp", "la.kp1", "control.lock-delay",
                         "control.lock-current", "control.closed-maximum-current", "collision.threshold"} <= keys)
        self.assertTrue(plan["admissible"], plan["refused"])
        self.assertEqual([c["id"] for c in cases], Args.cases)

    def test_algorithm_verdict_needs_zero_traffic(self):
        class Run:
            def __init__(self):
                self.out = {}

            def verdict(self, cid, level, verdict, reason=None, evidence=()):
                self.out[(cid, level)] = verdict
        refused = {"accepted": False, "result": "invalid", "latest_before": 5, "latest_after": 5}
        check = {"admitted": False, "reason": "algorithm_refused"}
        run = Run()
        tun.case_verdicts(run, "T11", {"cases": {"T11": {"writes": {1: refused, 2: refused}, "checks": {1: check, 2: check}}}}, None)
        self.assertEqual(run.out[("T11", "sent")], "PASS")
        moved = dict(refused, latest_after=6)
        tun.case_verdicts(run, "T11", {"cases": {"T11": {"writes": {1: moved, 2: refused}, "checks": {1: check, 2: check}}}}, None)
        self.assertEqual(run.out[("T11", "sent")], "FAIL")
        tun.case_verdicts(run, "T12", {"cases": {}}, None)
        self.assertEqual(run.out[("T12", "drive-reported")], "NOT RUN")


if __name__ == "__main__":
    unittest.main()
