"""Wired bench 19 harness: pure checks, case selection and the scenario record.
Runs without pyserial: the module must import it only where a port opens."""
import sys
import types
import unittest
from unittest import mock
from pathlib import Path

sys.modules["serial"] = None  # CI runners have no pyserial.
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import bench_multi as multi  # noqa: E402
import bench_records as records  # noqa: E402


def finding(address, outcome=1, known=True):
    return {"address": address, "outcome": outcome, "raw_model": 20202, "identity_known": known,
            "identity": [20202, 41, address, address] if known else [0, 0, 0, 0]}


def ended(**changes):
    value = {"phase": 4, "outcome": 1, "requests": 16, "findings": [finding(a) for a in multi.DRIVES],
             "silent": list(multi.EXPECTED_SILENT), "settles": 5, "settle": "recovered", "late_bytes": 0,
             "boundary_required": True, "boundary_crossed": False, "restored": False}
    value.update(changes)
    return value


RESTORED = dict(ended(), phase=5, boundary_crossed=True, restored=True)
REFUSED = {"probe": "busy", "frames": 0, "restore": "recovery_required"}


class MultiTests(unittest.TestCase):
    def test_d1_counts_every_reply_byte(self):
        replies = [{"command": "probe", "address": a, "ok": True, "answered": a} for a in multi.DRIVES]
        good = {"started": 6, "frames": 6, "failed": 0, "timeouts": 0, "rx_bytes": 60, "discarded": 0}
        self.assertEqual(multi.d1_verdict(good, replies)[0], "PASS")
        for key, value in (("rx_bytes", 61), ("discarded", 1), ("failed", 1), ("started", 7)):
            with self.subTest(key=key):
                self.assertEqual(multi.d1_verdict(dict(good, **{key: value}), replies)[0], "FAIL")
        foreign = replies[:2] + [dict(replies[2], answered=3)]
        self.assertEqual(multi.d1_verdict(good, foreign)[0], "FAIL")

    def test_d2_requires_silence_boundary_and_refusals(self):
        self.assertEqual(multi.d2_problems(ended(), REFUSED, RESTORED), [])
        bad = [(ended(silent=[3, 5, 6, 7]), REFUSED, RESTORED),
               (ended(late_bytes=7, settle="late_bytes"), REFUSED, RESTORED),
               (ended(boundary_required=False), REFUSED, RESTORED),
               (ended(findings=[finding(1), finding(2), finding(4, known=False)]), REFUSED, RESTORED),
               (ended(), dict(REFUSED, probe="accepted"), RESTORED),
               (ended(), dict(REFUSED, frames=1), RESTORED),
               (ended(), REFUSED, dict(RESTORED, boundary_crossed=False))]
        for args in bad:
            with self.subTest(args=args):
                self.assertTrue(multi.d2_problems(*args))

    def test_d3_keeps_todays_interlock(self):
        end = ended(outcome=6, requests=5, silent=[], settles=0, boundary_required=False,
                    findings=[finding(1), finding(2), finding(3, outcome=5, known=False)])
        self.assertEqual(multi.d3_problems(end, {"restore": "recovery_required"}, dict(end, phase=5, restored=True)), [])
        self.assertTrue(multi.d3_problems(dict(end, silent=[3]), {"restore": "recovery_required"}, dict(end, phase=5, restored=True)))
        self.assertTrue(multi.d3_problems(end, {"restore": "accepted"}, dict(end, phase=5, restored=True)))

    def test_targets_errors_and_group_verdict(self):
        origins, start = {1: 100, 2: 200, 4: 300}, {1: 150, 2: 250, 4: 350}
        intents = {1: ("absolute", 2000), 2: ("relative", -800), 4: ("absolute", 0)}
        self.assertEqual(multi.member_targets(origins, intents, start), {1: 2100, 2: -550, 4: 300})
        ok = {"label": "a", "ok": True, "errors": multi.endpoint_errors({1: 10, 2: 20}, {1: 9, 2: 22})}
        self.assertEqual(multi.g_verdict([ok], 2)[0], "PASS")
        self.assertEqual(multi.g_verdict([ok], 1)[0], "FAIL")
        self.assertEqual(multi.g_verdict([ok, {"label": "b", "ok": False, "state": "refused", "error": "unavailable"}], 2)[0], "FAIL")

    def test_stale_member_reads(self):
        reply = {"members": [{"address": 1, "read_state": "succeeded", "status": "OK", "state_known": False},
                             {"address": 2, "read_state": "succeeded", "status": "OK", "state_known": True},
                             {"address": 4, "read_state": "failed", "status": "TIMEOUT", "state_known": False}]}
        self.assertEqual(multi.stale_members(reply), [1])

    def test_stop_evidence_gates_later_plans(self):
        stop = {"ok": True, "state": "succeeded", "members": [{"address": a, "completion": "observed"} for a in multi.DRIVES]}
        still = {a: {"motion": 0, "speed": 0} for a in multi.DRIVES}
        start, end = {1: 0, 2: 0, 4: 0}, {1: 1500, 2: -1500, 4: 1500}
        self.assertEqual(multi.stop_problems(stop, still, start, end), [])
        failed = dict(stop, ok=False, state="failed", members=[dict(m, completion="not_observed") for m in stop["members"]])
        self.assertTrue(multi.stop_problems(failed, still, start, end))                       # failed stop
        self.assertTrue(multi.stop_problems(stop, still, start, {1: 1500, 2: 0, 4: 1500}))  # unmoved member
        running = {**still, 4: {"motion": multi.RUNNING_BIT, "speed": 60}}
        self.assertTrue(multi.stop_problems(stop, running, start, end))                       # still running
        self.assertFalse(multi.all_stopped(running))
        self.assertTrue(multi.all_stopped(still))
        self.assertTrue(multi.stop_problems(None, still, start, end))

    def test_r1_rows(self):
        row = {"address": 1, "probe": "success", "identity": True, "config": True, "state": True,
               "move": {"ok": True, "outcome": "observed", "travel": 1000},
               "stop_fast": {"travel": 950, "settled": True}, "group": {"ok": True, "state": "succeeded", "travel": 500}}
        self.assertEqual(multi.r1_problems([row]), [])
        self.assertTrue(multi.r1_problems([dict(row, stop_fast={"travel": 3000, "settled": True})]))
        self.assertTrue(multi.r1_problems([dict(row, group={"ok": True, "state": "succeeded", "travel": 0})]))

    def test_case_selection_and_record(self):
        args = multi.arguments(["--group", "MOTION", "--cases", "G3", "R1", "--run-dir", "x", "--firmware", "x",
                                "--commit", "c"])
        self.assertEqual(multi.selected_cases(args), ["G3", "R1"])
        with self.assertRaises(SystemExit):
            multi.arguments(["--group", "D2", "--cases", "G3", "--run-dir", "x", "--firmware", "x", "--commit", "c"])
        baseline = records.load_baseline()
        record = multi.scenario_record(args, {"path": "x", "sha256": "0" * 64}, "c", baseline)
        self.assertEqual([c["id"] for c in record["cases"]], ["G3", "R1"])
        self.assertEqual({(e["drive"], e["key"]) for e in record["write_list"]},
                         {(a, k) for a in multi.DRIVES for k in ("profile.acceleration", "profile.deceleration",
                                                                  "profile.speed", "profile.target")})
        reads = multi.scenario_record(multi.arguments(["--group", "D2", "--run-dir", "x", "--firmware", "x", "--commit", "c"]),
                                      {"path": "x", "sha256": "0" * 64}, "c", baseline)
        self.assertEqual(reads["write_list"], [])
        self.assertIn("past-silent", reads["commands"][0])


class StoppedStatesTests(unittest.TestCase):
    """Wired bench 20 G3: a late speed word after a group stop is not continued motion."""

    def live(self, sequences):
        reads = {a: iter(seq) for a, seq in sequences.items()}
        sample = lambda motion, speed, position: {"motion": motion, "speed": speed, "position_words": [0, position]}
        live = types.SimpleNamespace(read_state=lambda a: sample(*next(reads[a])))
        return live

    def test_late_speed_word_settles_to_stopped(self):
        still = [(0, 0, 500)] * 4
        live = self.live({1: [(0, 33, 900), (0, 0, 900), (0, 0, 900)], 2: still, 4: still})
        states = multi.stopped_states(live, limit_s=5.0)
        self.assertEqual(multi.stop_problems({"ok": True, "members": [{"address": a, "completion": "observed"} for a in multi.DRIVES]},
                                             states, {1: 0, 2: 0, 4: 0}, {1: 900, 2: 500, 4: 500}), [])

    def test_continued_motion_returns_at_once(self):
        live = self.live({1: [(0, 33, 900), (4, 60, 940)], 2: [(0, 0, 500)] * 2, 4: [(0, 0, 500)] * 2})
        states = multi.stopped_states(live, limit_s=60.0)
        self.assertTrue(states[1]["motion"] & multi.RUNNING_BIT)

    def test_a_member_that_never_settles_stays_a_problem(self):
        clock = [0.0]
        def tick():
            clock[0] += 0.5
            return clock[0]
        live = self.live({1: [(0, 33, 900)] * 50, 2: [(0, 0, 500)] * 50, 4: [(0, 0, 500)] * 50})
        with mock.patch.object(multi.time, "monotonic", tick):
            states = multi.stopped_states(live, limit_s=2.0)
        self.assertEqual(states[1]["speed"], 33)


if __name__ == "__main__":
    unittest.main()
