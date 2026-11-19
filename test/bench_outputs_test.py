"""Wired bench 06 outputs analysis: rows, edges, brackets, events and classes
from recorded reply shapes; scenario arguments and plans (no port)."""
import copy
import io
from contextlib import redirect_stderr
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import bench_outputs as outputs  # noqa: E402
import bench_probe as probe  # noqa: E402
import bench_records as records  # noqa: E402
import bench_scenarios as scenarios  # noqa: E402


def state_reply(operation, start, motion=0, outputs_bits=0, alarm=0, speed=0):
    """A `read state` terminal in the console's shape (three read steps)."""
    steps = [{"first": first, "attempted_us": start + 25000 * i, "latest_us": start + 25000 * i + 9000}
             for i, first in enumerate((6, 8, 10))]
    return {"ok": True, "operation_id": operation, "address": 4, "steps": steps, "state_blocks": [
        {"block": 0, "raw_motion": motion, "raw_alarm": alarm},
        {"block": 1, "raw_outputs": outputs_bits, "raw_inputs": 0},
        {"block": 2, "raw_speed": speed, "position_words": [0, 0]}]}


def tx(seq, transaction, at, frame):
    raw = bytes(frame) + probe.wire_crc(bytes(frame)).to_bytes(2, "little")
    return {"sequence": seq, "transaction": transaction, "kind": "TX", "at_us": at, "start_us": at, "end_us": at + 700,
            "uncertainty_us": 0, "raw_hex": raw.hex()}


def rx(seq, transaction, end):
    return {"sequence": seq, "transaction": transaction, "kind": "RX", "at_us": end, "start_us": end - 600, "end_us": end,
            "uncertainty_us": 40, "raw_hex": "00"}


CUSTOM_ON = [4, 6, 0, 0x4F, 0, 1]
CUSTOM_OFF = [4, 6, 0, 0x4F, 0, 0]


class SampleTest(unittest.TestCase):
    def test_state_sample_windows_and_values(self):
        s = outputs.state_sample(state_reply(7, 1000, motion=5, outputs_bits=1, alarm=0, speed=60))
        self.assertEqual((s["motion"], s["outputs"], s["speed"]), (5, 1, 60))
        self.assertEqual(s["windows"]["motion"], [1000, 10000])
        self.assertEqual(s["windows"]["outputs"], [26000, 35000])
        self.assertIsNone(outputs.state_sample({"ok": False}))
        self.assertIsNone(outputs.state_sample({"ok": True, "steps": []}))
        self.assertEqual(outputs.sample_value(s, ("motion", outputs.RUNNING)), 1)
        self.assertEqual(outputs.sample_value(s, ("alarm",)), 0)
        self.assertEqual(outputs.sample_value(outputs.state_sample(state_reply(8, 0, motion=24)), ("alarm",)), 1)

    def test_transitions_and_overlapping_brackets(self):
        samples = [outputs.state_sample(state_reply(1, 0, outputs_bits=0)),
                   outputs.state_sample(state_reply(2, 100000, outputs_bits=1)),
                   outputs.state_sample(state_reply(3, 200000, outputs_bits=1))]
        found = outputs.transitions(samples, ("outputs", 1))
        self.assertEqual(len(found), 1)
        self.assertEqual((found[0]["earliest_us"], found[0]["latest_us"]), (25000, 134000))
        inside = outputs.bracket(60000, found, 1)
        self.assertEqual((inside["status"], inside["relation"]), ("measured", "contains the edge"))
        self.assertEqual(inside["relative_us"], [-35000, 74000])
        # The drive transition bracket ends before the edge: reported, not a fault.
        self.assertEqual(outputs.bracket(150000, found, 1)["relation"], "before the edge")
        self.assertEqual(outputs.bracket(10000, found, 1)["relation"], "after the edge")
        self.assertEqual(outputs.bracket(60000, found, 0)["status"], "not observed")
        self.assertEqual(outputs.bracket(9000000, found, 1)["status"], "not observed")


class AnalyseTest(unittest.TestCase):
    def setUp(self):
        self.traffic = [tx(1, 1, 100000, CUSTOM_ON), rx(2, 1, 103000), tx(3, 2, 300000, CUSTOM_OFF), rx(4, 2, 303000)]
        self.writes = outputs.wire_writes(self.traffic)
        self.samples = [outputs.state_sample(state_reply(1, 50000, outputs_bits=0)),
                        outputs.state_sample(state_reply(2, 150000, outputs_bits=1)),
                        outputs.state_sample(state_reply(3, 350000, outputs_bits=0))]

    def rows(self):
        return outputs.split_rows([
            {"label": "custom 1", "start_us": 90000, "expect": [
                {"active": False, "write": {"register": 0x4F, "value": 1}, "key": ("outputs", 1), "new": 1,
                 "closure": {"earliest_us": 103500, "latest_us": 104800}, "role": "custom 1"}]},
            {"label": "custom 0", "start_us": 290000, "expect": [
                {"active": True, "write": {"register": 0x4F, "value": 0}, "key": ("outputs", 1), "new": 0,
                 "role": "custom 0"}]}], 500000)

    def test_wire_writes_pair_tx_and_reply(self):
        self.assertEqual([(w["register"], w["value"], w["tx_start_us"], w["ack_end_us"]) for w in self.writes],
                         [(0x4F, 1, 100000, 103000), (0x4F, 0, 300000, 303000)])
        self.assertIsNone(outputs.find_write(self.writes, 0x4F, 1, 200000))
        self.assertEqual(outputs.find_write(self.writes, 0x4F, None, 200000)["value"], 0)

    def test_inverted_edges_pass_with_intervals(self):
        edges = [{"at_us": 105000, "active": False}, {"at_us": 306000, "active": True}]
        result = outputs.analyse(self.rows(), edges, self.samples, self.writes, [])
        self.assertEqual([r["status"] for r in result["rows"]], ["PASS", "PASS"])
        first = result["rows"][0]["intervals"][0]
        self.assertEqual(first["after_tx_start_us"], 5000)
        self.assertEqual(first["after_ack_end_us"], [1960, 2040])
        self.assertEqual(first["after_closure_us"], [200, 1500])
        self.assertEqual(first["drive"]["relation"], "contains the edge")
        self.assertEqual(result["summary"]["by_role"]["custom 1"]["n"], 1)
        self.assertEqual(result["unattributed"], [])

    def test_inverted_reading_missing_and_extra_edges_fail(self):
        wrong = [{"at_us": 105000, "active": True}, {"at_us": 306000, "active": False}]
        result = outputs.analyse(self.rows(), wrong, self.samples, self.writes, [])
        self.assertEqual(result["failed"], ["custom 1", "custom 0"])
        self.assertIn("direction", result["rows"][0]["reason"])
        missing = outputs.analyse(self.rows(), [{"at_us": 105000, "active": False}], self.samples, self.writes, [])
        self.assertIn("missing edge", missing["rows"][1]["reason"])
        extra = outputs.analyse(self.rows(), [{"at_us": 105000, "active": False}, {"at_us": 106000, "active": True},
                                              {"at_us": 306000, "active": True}], self.samples, self.writes, [])
        self.assertIn("extra edge", extra["rows"][0]["reason"])

    def test_lost_edges_and_missing_writes_are_indeterminate(self):
        edges = [{"at_us": 105000, "active": False}, {"at_us": 306000, "active": True}]
        lost = [{"from_us": 295000, "to_us": 320000, "edges_added": 3, "edges_seen": 1}]
        result = outputs.analyse(self.rows(), edges, self.samples, self.writes, lost)
        self.assertEqual(result["indeterminate"], ["custom 0"])
        result = outputs.analyse(self.rows(), edges, self.samples, [], [])
        self.assertIn("write_status", result["rows"][0]["intervals"][0])
        self.assertEqual(result["summary"]["write_missing"], 2)

    def test_unattributed_edge_is_reported(self):
        edges = [{"at_us": 10000, "active": False}, {"at_us": 105000, "active": False}, {"at_us": 306000, "active": True}]
        result = outputs.analyse(self.rows(), edges, self.samples, self.writes, [])
        self.assertEqual(result["unattributed"], [{"at_us": 10000, "active": False}])

    def test_recorded_rows_and_event_pairing(self):
        rows = outputs.split_rows([{"label": "move", "start_us": 0, "expect": None}], 900000)
        samples = [outputs.state_sample(state_reply(1, 0, motion=1)), outputs.state_sample(state_reply(2, 200000, motion=4)),
                   outputs.state_sample(state_reply(3, 600000, motion=1))]
        edges = [{"at_us": 120000, "active": True}, {"at_us": 650000, "active": False}, {"at_us": 800000, "active": True}]
        result = outputs.analyse(rows, edges, samples, [], [])
        self.assertEqual(result["rows"][0]["status"], "recorded")
        events = outputs.pair_events(edges, outputs.transitions(samples, ("motion", outputs.IN_POSITION)), 1)
        self.assertEqual([p.get("event") for p in events["pairs"]], ["clear", "set", None])
        self.assertEqual(len(events["unpaired_edges"]), 1)
        self.assertEqual(events["unpaired_transitions"], [])

    def test_closure_from_operation_results(self):
        move = {"trigger_evidence": {"qualified": True, "earliest_us": 10, "latest_us": 20}}
        action = {"write_evidence": {"qualified": True, "earliest_us": 30, "latest_us": 40}}
        driver = {"evidence_columns": ["register", "write", "qualified", "earliest_us", "latest_us"],
                  "evidence": [[0x4F, False, True, 1, 2], [0x4F, True, True, 50, 60]]}
        self.assertEqual(outputs.closure(move), {"earliest_us": 10, "latest_us": 20})
        self.assertEqual(outputs.closure(action), {"earliest_us": 30, "latest_us": 40})
        self.assertEqual(outputs.closure(driver, 0x4F), {"earliest_us": 50, "latest_us": 60})
        self.assertIsNone(outputs.closure(driver, 0x4C))
        self.assertIsNone(outputs.closure({"write_evidence": {"qualified": False}}))


class ClassTest(unittest.TestCase):
    def test_mapping_class(self):
        self.assertEqual(outputs.mapping_class(1, 1, 0), "after")
        self.assertEqual(outputs.mapping_class(1, 1, 1), "before")
        self.assertEqual(outputs.mapping_class(1, 0, 1), "not_separated")

    def test_experiment_classes(self):
        def obs(pairs):
            return [{"custom": c, "y0_on": on} for c, on in pairs]
        self.assertEqual(outputs.experiment_class(obs([(1, True), (2, False), (3, True), (0, False)])), "follows bit 0")
        self.assertEqual(outputs.experiment_class(obs([(1, False), (2, True), (3, True), (0, False)])), "follows bit 1")
        self.assertEqual(outputs.experiment_class(obs([(1, False), (2, False), (3, False), (4, True), (0, False)])),
                         "follows experimental bit 2")
        self.assertEqual(outputs.experiment_class(obs([(1, False), (2, False), (3, False), (4, False), (0, False)])),
                         "no response to the tested bits")
        self.assertEqual(outputs.experiment_class(obs([(1, False), (2, False), (3, False), (4, None), (0, False)])),
                         "bit 2 not tested")
        self.assertEqual(outputs.experiment_class(obs([(1, True), (2, True), (3, False), (0, False)])), "indeterminate")
        self.assertEqual(outputs.experiment_class(obs([(1, None)])), "not measured")


FIRMWARE = ROOT / "test" / "bench_outputs_test.py"  # Any existing file: the record hashes it, no flash.


class ScenarioArgumentsTest(unittest.TestCase):
    def args(self, *extra):
        return scenarios.arguments(["--scenario", "outputs", "--plan-only", "--firmware", str(FIRMWARE), *extra])

    def refused(self, *extra):
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.args(*extra)

    def test_case_address_image_and_removed_experiment(self):
        args = self.args("--case", "O1", "--address", "4", "--image-level", "06")
        self.assertEqual(args.repetitions, 10)
        self.refused("--case", "O1", "--address", "1", "--image-level", "06")
        self.refused("--case", "O1", "--address", "4", "--image-level", "05")
        self.refused("--case", "O12", "--address", "4", "--image-level", "06")
        self.refused("--case", "O10", "--address", "4", "--image-level", "06")  # D9 removed (Implement 3).
        self.refused("--case", "O11", "--address", "4", "--image-level", "06")
        self.refused("--case", "O1", "--address", "4", "--image-level", "06", "--repetitions", "21")
        self.refused("--scenario", "y0-map", "--repetitions", "3")

    def test_every_case_plans_an_admissible_restoration_before_the_port(self):
        baseline = records.load_baseline()
        for case in ("O1", "O2", "O3", "O4", "O5", "O6", "O7", "O8", "O9"):
            plan = scenarios.folder_plan(self.args("--case", case, "--address", "4", "--image-level", "06"), baseline)
            self.assertTrue(plan["restoration_plan"]["admissible"], (case, plan["restoration_plan"]["refused"]))
            self.assertEqual(plan["cases"][0]["id"], case)
            self.assertFalse([w for w in plan["case_writes"] if w["refusal"]], case)
        o7 = scenarios.folder_plan(self.args("--case", "O7", "--address", "4", "--image-level", "06"), baseline)
        snapshot = [s for s in o7["restoration_plan"]["steps"] if s["route"] == "snapshot"]
        self.assertEqual([(s["snapshot"], s["words"]) for s in snapshot], [("home", [24, 120, 60, 100, 0, 0])])


if __name__ == "__main__":
    unittest.main()
