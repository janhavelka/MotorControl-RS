"""Wired bench 14 velocity harness: the plan checker, slopes, the ramp fit, the
creep detector, the dead-man lapse hook, the PV selection row and verdicts (no port)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import bench_records as records  # noqa: E402
import bench_velocity as vel  # noqa: E402


def sample(t_s, position, motion=1, speed=0, alarm=0):
    """A read-state sample with its feedback read window around t_s."""
    words = [(position >> 16) & 0xFFFF, position & 0xFFFF]
    us = int(t_s * 1e6)
    return {"motion": motion, "alarm": alarm, "speed": speed & 0xFFFF, "position_words": words, "inputs": 0,
            "windows": {"feedback": [us - 2000, us + 2000], "motion": [us - 6000, us - 4000], "outputs": [us - 4000, us - 2000]}}


class PlanCheckerTest(unittest.TestCase):
    def test_one_terminal_two_purposes_refuses(self):
        self.assertIsNone(vel.plan_problem([("x0", "jog+"), ("x3", "deadman")]))
        self.assertIn("two purposes", vel.plan_problem([("x3", "jog-"), ("x3", "deadman")]))
        self.assertIn("unknown terminal", vel.plan_problem([("x4", "jog+")]))

    def test_every_case_plan_is_consistent(self):
        for cid, case in vel.CASES.items():
            self.assertIsNone(vel.plan_problem(case["uses"] + vel.layout_uses(case["layout"])), cid)

    def test_j2_and_j4_have_no_deadman(self):
        for cid in ("J2", "J4"):
            self.assertNotIn(("x3", "deadman"), vel.CASES[cid]["uses"])
        self.assertIsNotNone(vel.plan_problem(vel.CASES["J2"]["uses"] + [("x3", "deadman")]))

    def test_layout_functions(self):
        self.assertEqual(vel.LAYOUTS["pv2-deadman"]["x3"], ("deadman", 5, 1))
        self.assertEqual([vel.LAYOUTS["pin3"][t][1] for t in ("x0", "x1", "x2", "x3")], [13, 14, 15, 16])


class SlopeTest(unittest.TestCase):
    def test_rate_and_slope(self):
        self.assertEqual(vel.rate(60), 1000)
        self.assertEqual(vel.rate(-500, 6000), 50000)
        samples = [sample(1 + 0.05 * i, 1000 + 50 * i) for i in range(20)]
        self.assertAlmostEqual(vel.steady_slope(samples, 0, 10), 1000, places=3)
        self.assertIsNone(vel.steady_slope(samples[:1], 0, 10))

    def test_slope_verdict(self):
        self.assertEqual(vel.slope_verdict(1040, 60, 1000, 0.05)[0], "PASS")
        self.assertEqual(vel.slope_verdict(1100, 60, 1000, 0.05)[0], "FAIL")
        self.assertEqual(vel.slope_verdict(-1000, -60, 1000, 0.05)[0], "PASS")
        self.assertEqual(vel.slope_verdict(1000, -60, 1000, 0.05)[0], "FAIL")  # Wrong direction.
        self.assertEqual(vel.slope_verdict(None, 60, 1000, 0.05)[0], "indeterminate")

    def test_negative_positions_decode(self):
        samples = [sample(0.05 * i, -100 - 50 * i) for i in range(10)]
        self.assertAlmostEqual(vel.steady_slope(samples, -1, 10), -1000, places=3)


class RampFitTest(unittest.TestCase):
    def test_recovers_a_ramp(self):
        v, ramp, t0 = 1000.0, 0.3, 0.1
        pts = []
        for i in range(60):
            t = i * 0.02
            dt = t - t0
            p = 0 if dt <= 0 else v * dt * dt / (2 * ramp) if dt < ramp else v * ramp / 2 + v * (dt - ramp)
            pts.append((t, round(p)))
        fit = vel.ramp_fit(pts, v, 0.0, 0.0, 0.2)
        self.assertAlmostEqual(fit["ramp_s"], 0.3, delta=0.02)
        self.assertAlmostEqual(fit["t0"], 0.1, delta=0.01)
        self.assertIsNone(vel.ramp_fit(pts[:3], v))


class CreepTest(unittest.TestCase):
    def test_creep_and_standstill(self):
        still = [sample(0.05 * i, 500, motion=1, speed=3) for i in range(6)]
        self.assertFalse(vel.creeping(still))
        moving = [sample(0.05 * i, 500 + 3 * i) for i in range(6)]
        self.assertTrue(vel.creeping(moving))
        running = [sample(0.05 * i, 500, motion=1 | 4) for i in range(3)]
        self.assertTrue(vel.creeping(running))
        self.assertTrue(vel.creeping([sample(0, 500, speed=-20)]))  # Signed speed word.


class DeadmanTest(unittest.TestCase):
    def test_lapse_hook_renews_then_shortens_once(self):
        calls = []

        class Live:
            deadman_on_us = 123

        live = Live()
        original = vel.deadman_renew
        vel.deadman_renew = lambda live_, hold_ms=vel.DEADMAN_HOLD_MS: calls.append(hold_ms)
        try:
            hook, trial = vel.deadman_lapse_at(1.0), {}
            hook(live, 0.1, trial)
            hook(live, 0.6, trial)
            self.assertEqual(calls, [vel.DEADMAN_HOLD_MS, 400])
            self.assertIsNone(live.deadman_on_us)
            self.assertEqual(trial["lapse"]["lapse_at_s"], 1.0)
            hook(live, 0.9, trial)
            self.assertEqual(len(calls), 2)  # Never renewed after the lapse was set.
        finally:
            vel.deadman_renew = original


class VerdictTest(unittest.TestCase):
    def test_jog_verdict(self):
        good = {"result": {"outcome": "attributed", "position_change": 900, "completion": "velocity_stopped"}, "settle": {}}
        self.assertEqual(vel.jog_verdict(good, 1)[0], "PASS")
        self.assertEqual(vel.jog_verdict(good, -1)[0], "FAIL")
        self.assertEqual(vel.jog_verdict({"refused": "arm refused"}, 1)[0], "FAIL")
        moving = dict(good, settle={"moving": True})
        self.assertEqual(vel.jog_verdict(moving, 1)[0], "FAIL")

    def test_deadman_before_width(self):
        on = 10_000_000
        trial = {"on_us": on, "width_ms": 3000, "source": {"off_us": on + 3_000_000},
                 "result": {"completion": "velocity_stopped"},
                 "timeline": [{"change": 100, "reply_us": on + 400_000}, {"change": 700, "reply_us": on + 1_600_000},
                              {"change": 700, "reply_us": on + 2_000_000}]}
        self.assertTrue(vel.deadman_before_width(trial))
        late = dict(trial, timeline=[{"change": 2900, "reply_us": on + 3_100_000}])
        self.assertFalse(vel.deadman_before_width(late))

    def test_cell_tokens_and_highest(self):
        self.assertEqual(vel.cell_tokens(-120, 100, 100, 1000, 2000), ("qualify", "-120", "100", "100", "1000", "2000", "normal"))
        c1 = {"rows": [{"ramp": 100, "sign": 1, "cells": [{"rpm": 120, "verdict": "PASS"}, {"rpm": 300, "verdict": "FAIL"}]},
                       {"ramp": 500, "sign": 1, "cells": [{"rpm": 600, "verdict": "PASS"}]}]}
        self.assertEqual(vel.highest_passing(c1), 120)

    def test_abort_rules(self):
        self.assertIn("alarm", vel.abort_problem({"alarm": 5}))
        self.assertIn("slope", vel.abort_problem({"verdict": ("FAIL", "slope 10 /s")}))
        self.assertIn("moving", vel.abort_problem({"after": {"moving": True}}))
        self.assertIsNone(vel.abort_problem({"verdict": ("PASS", ""), "terminal": {"ok": True}, "after": {"moving": False}}))


class SessionRebuildTest(unittest.TestCase):
    def test_trials_from_session(self):
        def state(op, t_s, position):
            us = int(t_s * 1e6)
            return {"event": "reply", "response": {
                "type": "read", "command": "read-state", "ok": True, "operation_id": op, "address": 2,
                "state_blocks": [{"block": 0, "raw_motion": 4, "raw_alarm": 0},
                                 {"block": 1, "raw_outputs": 0, "raw_inputs": 0},
                                 {"block": 2, "raw_speed": 120, "position_words": [0, position]}],
                "steps": [{"first": 6, "attempted_us": us, "latest_us": us + 1000},
                          {"first": 8, "attempted_us": us + 1000, "latest_us": us + 2000},
                          {"first": 10, "attempted_us": us + 2000, "latest_us": us + 3000}]}}
        entries = [state(1, 0.0, 0),
                   {"event": "reply", "response": {"type": "reply", "command": "velocity", "ok": True, "operation_id": 7}},
                   state(8, 0.5, 500), state(9, 1.0, 1500),
                   {"event": "reply", "response": {"type": "velocity", "operation_id": 7, "native_rpm": 120, "ok": True,
                                                   "staging_words": [120, 100, 100], "requested": {"duration_us": 2000000},
                                                   "trigger_evidence": {"latest_us": 100000}, "stop_due_us": 2100000,
                                                   "outcome": "observed"}}]
        trials = vel.trials_from_session(entries)
        self.assertEqual(len(trials), 1)
        t = trials[0]
        self.assertEqual((t["rpm"], t["ramp"], t["duration_ms"], t["outcome"]), (120, 100, 2000, "observed"))
        self.assertEqual([vel.stops.position(s) for s in t["samples"]], [500, 1500])


class PvSelectionRowTest(unittest.TestCase):
    """The interlock admits a PIN change of a named selection member whose trigger holds 13 (PV) with its output off."""
    def facts(self, trigger_function):
        return {"address": 1, "unknown": [], "ages_ms": {n: 10 for n in records.INTERLOCK_READS},
                "functions": [trigger_function, 14, 15, 16], "polarity": 0, "running": False, "raw_alarm": 0,
                "alarm_flag": False}

    def test_pv_trigger_admits_selection(self):
        named = {"address": 1, "output": "relay2", "terminal": "x1", "function": 14, "polarity": 0,
                 "effect": records.SELECT_EFFECT, "trigger_terminal": "x0", "trigger_off": True}
        self.assertEqual(records.decide_participant(self.facts(13), "relay2", named=named)["row"], "P")
        self.assertEqual(records.decide_participant(self.facts(12), "relay2", named=named)["row"], "P")
        self.assertEqual(records.decide_participant(self.facts(9), "relay2", named=named)["row"], "L4")
        off = dict(named, trigger_off=False)
        self.assertEqual(records.decide_participant(self.facts(13), "relay2", named=off)["row"], "L4")


if __name__ == "__main__":
    unittest.main()
