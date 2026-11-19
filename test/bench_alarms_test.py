"""Wired bench 16 alarms-and-failures harness: the Implement 3 silence
classification, the silence bracket, the step 5 eligibility and branch, stop
frame counting, the position-limits grammar and reply check, and the plan
(no port)."""
import copy
import random
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import bench_alarms as alarms  # noqa: E402
import bench_probe as probe  # noqa: E402
import bench_records as records  # noqa: E402
import bench_velocity as velocity  # noqa: E402

ACK_US = 10_000_000


def silent_move():
    return {"type": "move", "ok": False, "state": "failed", "outcome": "transport_error", "execution": "acknowledged",
            "completion": "not_observed", "polls": 0,
            "trigger_evidence": {"response_confirmed": True, "earliest_us": ACK_US - 900, "latest_us": ACK_US},
            "failure_evidence": {"step": 2, "received_length": 0, "tx_accepted": 8, "delivered_us": ACK_US + 230_000}}


def state(start_us, ok=True, motion=24, alarm=5, position=(0, 100)):
    steps = [{"first": first, "attempted_us": start_us + 9000 * i, "latest_us": start_us + 9000 * i + 8000}
             for i, first in enumerate((6, 8, 10))]
    if not ok:
        return {"ok": False, "outcome": "transport_error", "steps": [{"first": 6, "attempted_us": start_us,
                                                                       "delivered_us": start_us + 200_000}]}
    return {"ok": True, "address": 4, "steps": steps, "state_blocks": [
        {"block": 0, "raw_motion": motion, "raw_alarm": alarm},
        {"block": 1, "raw_outputs": 0, "raw_inputs": 0},
        {"block": 2, "raw_speed": 0, "position_words": list(position)}]}


class ClassificationTest(unittest.TestCase):
    def test_alarm_found_after_silence(self):
        terminal, reply = silent_move(), state(ACK_US + 2_050_000)
        before = (copy.deepcopy(terminal), copy.deepcopy(reply))
        out = alarms.classify_silence(terminal, True, reply)
        self.assertEqual(out["class"], "alarm_after_silence")
        self.assertTrue(out["silent_poll"]["first_poll"])
        self.assertEqual((terminal, reply), before)  # The original result is kept.

    def test_no_reply_is_drive_silent(self):
        terminal = silent_move()
        before = copy.deepcopy(terminal)
        out = alarms.classify_silence(terminal, True, state(ACK_US + 2_100_000, ok=False))
        self.assertEqual(out["class"], "drive_silent")
        self.assertEqual(terminal, before)

    def test_reply_without_alarm(self):
        out = alarms.classify_silence(silent_move(), True, state(ACK_US + 2_000_000, motion=1, alarm=0))
        self.assertEqual(out["class"], "no_alarm")

    def test_alarm_flag_alone_counts(self):
        out = alarms.classify_silence(silent_move(), True, state(ACK_US + 2_000_000, motion=8, alarm=0))
        self.assertEqual(out["class"], "alarm_after_silence")

    def test_malformed_terminal_and_reply(self):
        for broken in ({}, dict(silent_move(), outcome="observation_limit"), dict(silent_move(), execution="unknown"),
                       dict(silent_move(), failure_evidence={"received_length": 3, "tx_accepted": 8})):
            before = copy.deepcopy(broken)
            self.assertEqual(alarms.classify_silence(broken, True, state(ACK_US + 2_500_000))["class"], "not_silence")
            self.assertEqual(broken, before)
        self.assertEqual(alarms.classify_silence(silent_move(), True, {})["class"], "malformed")
        blocks = dict(state(ACK_US + 2_500_000), state_blocks=[])
        self.assertEqual(alarms.classify_silence(silent_move(), True, blocks)["class"], "malformed")

    def test_a_read_before_two_seconds_does_not_decide(self):
        out = alarms.classify_silence(silent_move(), True, state(ACK_US + 1_700_000))
        self.assertEqual(out["class"], "too_early")


class BracketTest(unittest.TestCase):
    def test_bracket_from_reads(self):
        poll = alarms.silent_poll(silent_move())
        reads = [alarms.read_window(state(ACK_US + 1_400_000, ok=False)), alarms.read_window(state(ACK_US + 1_700_000)),
                 alarms.read_window(state(ACK_US + 2_000_000))]
        b = alarms.silence_bracket(poll, reads)
        self.assertEqual(b["last_silent_us"], ACK_US + 1_400_000)
        self.assertEqual(b["first_answer_us"], ACK_US + 1_700_000 + 26_000)
        self.assertEqual(b["end_after_ack_ms"], [1400.0, 1726.0])
        self.assertEqual(b["duration_ms"], [1170.0, 1726.0])
        edges = [{"at_us": ACK_US - 5, "active": False}, {"at_us": ACK_US + 1_080_000, "active": False}]
        self.assertEqual(alarms.edge_in(edges, False, b["ack_us"], b["first_answer_us"])["at_us"], ACK_US + 1_080_000)
        self.assertIsNone(alarms.edge_in(edges, True, b["ack_us"], b["first_answer_us"]))
        self.assertIsNone(alarms.silence_bracket(None, reads))

    def test_alarm_edges_by_polarity(self):
        trial = {"silence": {"bracket": {"ack_us": 0, "first_answer_us": 2_000_000}},
                 "clear": {"clear_tx_us": 3_000_000}, "t1_us": 4_000_000,
                 "edges": [{"at_us": 1_000_000, "active": False}, {"at_us": 3_010_000, "active": True}]}
        out = alarms.alarm_edges(trial, 0)
        self.assertTrue(out["inside"])
        self.assertEqual((out["alarm_after_ack_ms"], out["clear_edge_after_tx_ms"]), (1000.0, 10.0))
        self.assertFalse(alarms.alarm_edges(trial, 1)["inside"])  # Inverted output: the alarm edge reads active.


class FailedStopTest(unittest.TestCase):
    def stop(self, **changes):
        t = {"action_kind": "stop", "address": 4, "ok": False, "state": "failed", "execution": "acknowledged",
             "completion": "not_observed", "write_evidence": {"tx_accepted": 8}}
        t.update(changes)
        return t

    def test_eligibility(self):
        self.assertTrue(alarms.stop_eligible(self.stop(), 4))
        self.assertTrue(alarms.stop_eligible(self.stop(execution="unknown"), 4))
        for bad in (dict(execution="not_transmitted"), dict(completion="observed"), dict(ok=True),
                    dict(write_evidence={"tx_accepted": 0}), dict(action_kind="enable"), dict(address=2),
                    dict(group=True)):
            self.assertFalse(alarms.stop_eligible(self.stop(**bad), 4), bad)
        self.assertFalse(alarms.stop_eligible(None, 4))

    def test_branch(self):
        s = lambda motion, position: {"motion": motion, "speed": 0, "position_words": [0, position]}
        self.assertEqual(alarms.fresh_branch([s(6, 100), s(6, 112)]), "moving")
        self.assertEqual(alarms.fresh_branch([s(6, 100), s(6, 100)]), "stopped")
        self.assertEqual(alarms.fresh_branch([s(2, 100), s(2, 112)]), "stopped")  # Position alone is not RUNNING.
        self.assertEqual(alarms.fresh_branch([None, s(6, 100)]), "unavailable")

    def test_stop_frames(self):
        line = lambda at, raw: '< {"type":"traffic","kind":"TX","at_us":%d,"start_us":0,"raw_hex":"%s"}' % (at, raw)
        lines = [line(10, "0406002701003F51"), line(20, "0406002701003F51"), line(30, "0406002702003FA1"),
                 line(5, "0406002701003F51"), line(40, "0403000600022 45F"), "not traffic"]
        self.assertEqual(alarms.stop_frames(lines, 10, 40), {"4:normal": 2, "4:fast": 1})

    def test_delay_window(self):
        rng = random.Random(1)
        for _ in range(50):
            d = alarms.hw14_delay(rng, (0.362, 0.392), -0.02)
            self.assertTrue(0.382 <= d <= 0.412)


class PositionLimitsGrammarTest(unittest.TestCase):
    def test_arguments(self):
        self.assertEqual(probe.position_limits_arguments(()), {"request": "show"})
        self.assertEqual(probe.position_limits_arguments(("default",)), {"request": "default"})
        self.assertEqual(probe.position_limits_arguments(("set", "2000", "133334", "100", "case", "hw14")),
                         {"request": "set", "limits": {"max_rpm": 2000, "max_rate": 133334, "min_ramp": 100}, "case": "hw14"})
        for bad in (("set", "0", "1", "1", "case", "a"), ("set", "3001", "1", "1", "case", "a"),
                    ("set", "1", "0", "1", "case", "a"), ("set", "1", "2560001", "1", "case", "a"),
                    ("set", "1", "1", "2001", "case", "a"), ("set", "1", "1", "1", "case", "Bad"),
                    ("set", "1", "1", "1", "name", "a"), ("set", "1", "1", "1", "case", "abcdefghijklmnop"),
                    ("default", "x"), ("other",)):
            with self.assertRaises(ValueError):
                probe.position_limits_arguments(bad)

    def reply(self, **changes):
        item = {"ok": True, "result": "done", "request": "set", "reason": "none", "bus_traffic": False,
                "source": "installed", "case": "hw14", "address": 4, "max_rpm": 2000, "max_rate": 133334,
                "min_ramp": 100, "subdivision": 4000, "maximum_position_rpm": 2000, "default_maximum_position_rpm": 750,
                "install_binding": 3, "install_configuration": 9,
                "defaults": {"max_rpm": 2000, "max_rate": 50000, "min_ramp": 100}}
        item.update(changes)
        return item

    def test_reply_check(self):
        handle = SimpleNamespace(host_args=("set", "2000", "133334", "100", "case", "hw14"))
        probe.Console._check_position_limits(handle, self.reply())
        for bad in (dict(maximum_position_rpm=750), dict(case=""), dict(source="default"), dict(bus_traffic=True),
                    dict(defaults={"max_rpm": 2000, "max_rate": 200000, "min_ramp": 100}), dict(max_rate=133333),
                    dict(install_binding=0)):
            with self.assertRaises(probe.BenchError, msg=str(bad)):
                probe.Console._check_position_limits(handle, self.reply(**bad))
        show = SimpleNamespace(host_args=())
        probe.Console._check_position_limits(show, self.reply(
            request="show", source="default", case="", max_rate=50000, maximum_position_rpm=750,
            install_binding=0, install_configuration=0))


class PlanTest(unittest.TestCase):
    def test_plan_routes_every_field(self):
        baseline = records.load_baseline(records.DEFAULT_BASELINE)
        args = SimpleNamespace(address=4, cases=["G1", "A1", "A3", "A4", "A5", "F1", "F2", "R2", "F4", "N1"],
                               image_level="16")
        write_list, restoration, cases, *_ = alarms.plan(args, baseline, velocity.facts())
        keys = {e["key"] for e in write_list}
        self.assertTrue({"filters.deviation-threshold", "io.y0", "io.output-polarity", "drive.subdivision",
                         "profile.acceleration", "jog.speed"} <= keys)
        self.assertTrue(restoration["admissible"], restoration["refused"])
        self.assertEqual([c["id"] for c in cases], args.cases)

    def test_not_run_verdicts(self):
        class Run:
            def __init__(self):
                self.out = {}

            def verdict(self, cid, level, verdict, reason=None, evidence=()):
                self.out[(cid, level)] = (verdict, reason)
        run = Run()
        for cid in ("F4", "R1", "R3", "N1", "F3"):
            alarms.case_verdicts(run, cid, {"cases": {}}, None)
            mine = [v for k, v in run.out.items() if k[0] == cid]
            self.assertTrue(all(v[0] == "NOT RUN" for v in mine) and any(v[1] for v in mine), cid)
        alarms.case_verdicts(run, "F1", {"cases": {"F1": {"trials": [{"n": 0, "stop_ok": True}]}}}, None)
        self.assertEqual(run.out[("F1", "drive-reported")][0], "indeterminate")
        alarms.case_verdicts(run, "F2", {"cases": {"F1": {"trials": [{"n": 0, "stop_ok": True}]}}}, None)
        self.assertEqual(run.out[("F2", "drive-reported")][0], "NOT RUN")


if __name__ == "__main__":
    unittest.main()
