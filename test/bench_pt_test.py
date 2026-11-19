"""Wired bench 13 PT harness: PIN map, layout, selection consequences, the
fallback gate, the W0 path guard and verdicts, and the interlock's named
selection members and held PIN outputs (no port)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import bench_pt as pt  # noqa: E402
import bench_records as records  # noqa: E402


def targets(first=400, step=400, sentinel=100):
    out = {n: step * n if n > 1 else first for n in range(1, 9)}
    out.update({n: sentinel for n in range(9, 17)})
    return out


class PinMapTest(unittest.TestCase):
    def test_all_eight_records(self):
        for record in range(1, 9):
            levels = pt.pin_levels(record)
            inputs = sum(1 << (1 + b) for b, o in enumerate(pt.PIN_OUTPUTS) if levels[o])
            self.assertEqual(pt.selected_record(inputs), record)
        self.assertEqual(pt.pin_levels(1), {"relay2": False, "relay1": False, "mosfet2": False})
        self.assertEqual(pt.pin_levels(2), {"relay2": True, "relay1": False, "mosfet2": False})  # PIN0 on relay2 (D2)
        self.assertEqual(pt.pin_levels(3), {"relay2": False, "relay1": True, "mosfet2": False})

    def test_records_beyond_eight_refuse(self):
        for record in (0, 9, 16):
            with self.assertRaises(ValueError):
                pt.pin_levels(record)

    def test_trigger_bit_does_not_select(self):
        self.assertEqual(pt.selected_record(0b0001), 1)  # X0 (trigger) active only

    def test_layout(self):
        self.assertIsNone(pt.layout_problem([0, 12, 14, 15, 16, 0, 0, 0, 0]))
        self.assertIn("not the PT layout", pt.layout_problem([0, 1, 2, 3, 0, 0, 0, 0, 0]))
        self.assertIn("not the PT layout", pt.layout_problem([0, 12, 14, 15, 17, 0, 0, 0, 0]))  # PIN3 instead of PIN2
        self.assertIn("polarity", pt.layout_problem([2, 12, 14, 15, 16, 0, 0, 0, 0]))


class SelectionConsequenceTest(unittest.TestCase):
    def test_table_inside(self):
        self.assertIsNone(pt.selection_problem(targets(), 1000, 0, pt.table_bounds(0)))

    def test_reachable_record_outside_the_bound(self):
        t = targets()
        t[8] = 4001  # relative +4001 from the start
        problem = pt.selection_problem(t, 1000, 0, pt.table_bounds(0))
        self.assertIn("record 8", problem)

    def test_unreachable_record_still_checked(self):
        t = targets()
        t[12] = -5000  # records 9-16 are bounded as well: PIN3's level is not established
        self.assertIn("record 12", pt.selection_problem(t, 0, 0, pt.table_bounds(0)))

    def test_absolute_window(self):
        t = targets()
        self.assertIsNone(pt.selection_problem(t, 50000, 1, pt.table_bounds(1)))
        t[3] = 8001
        self.assertIn("record 3", pt.selection_problem(t, 0, 1, pt.table_bounds(1)))

    def test_missing_record(self):
        t = targets()
        del t[16]
        self.assertIn("not every record", pt.selection_problem(t, 0, 0, pt.table_bounds(0)))

    def test_w0_bounds(self):
        t = {n: 100 for n in range(2, 17)}
        t[1] = 65536
        self.assertIsNone(pt.selection_problem(t, 19167, 0, pt.w0_bounds()))
        t[7] = 101
        self.assertIn("record 7", pt.selection_problem(t, 19167, 0, pt.w0_bounds()))
        t[7], t[1] = 100, 6553600  # swapped words in record 1 would run 6.5 M
        self.assertIn("record 1", pt.selection_problem(t, 19167, 0, pt.w0_bounds()))


class FallbackGateTest(unittest.TestCase):
    def test_ledger_default_is_beyond_the_bound(self):
        base = {n: 5000 for n in range(1, 17)}  # 01 baseline [0,5000]: beyond the 4000 bound
        self.assertIn("outside", pt.fallback_gate(base, 0, 0))

    def test_short_records(self):
        t = targets()
        t[4] = 150
        self.assertIn("[4]", pt.fallback_gate(t, 0, 0))

    def test_gate_holds(self):
        self.assertIsNone(pt.fallback_gate(targets(), 0, 0))


class PathGuardTest(unittest.TestCase):
    def test_band(self):
        low, high = pt.path_band(1.0, 1.05, 65536, 300)
        self.assertEqual((low, high), (5000 * (1.0 - pt.PATH_LAG_S) - 400, 5000 * 1.05 + 400))
        self.assertEqual(pt.path_band(20, 20.1, 65536, 300), (65136, 65936))

    def test_on_path(self):
        self.assertIsNone(pt.guard_problem(4500, 1.0, 1.05, 65536, 300))

    def test_mis_selection_at_fifteen_rpm(self):
        self.assertIsNotNone(pt.guard_problem(int(250 * 0.7), 0.7, 0.75, 65536, 300))

    def test_overspeed_and_overrun(self):
        self.assertIsNotNone(pt.guard_problem(9000, 1.0, 1.05, 65536, 300))
        self.assertIsNotNone(pt.guard_problem(66000, 20, 20.1, 65536, 300))

    def test_swapped_order_stops_early(self):
        self.assertIsNotNone(pt.guard_problem(1, 0.7, 0.75, 65536, 300))

    def test_w0a_verdicts(self):
        self.assertEqual(pt.w0a_verdict(65536), ("PASS", 0))
        self.assertEqual(pt.w0a_verdict(65538), ("PASS", 0))
        self.assertEqual(pt.w0a_verdict(1), ("PASS", 1))
        self.assertEqual(pt.w0a_verdict(65000), ("FAIL", None))
        self.assertEqual(pt.w0a_verdict(None), ("indeterminate", None))


class RunVerdictTest(unittest.TestCase):
    def test_match_and_tolerance(self):
        ok = {"expected_change": 1200, "result": {"outcome": "attributed", "position_change": 1201}}
        self.assertEqual(pt.run_verdict(ok)[0], "PASS")
        off = {"expected_change": 1200, "result": {"outcome": "attributed", "position_change": 1100}}
        self.assertEqual(pt.run_verdict(off)[0], "FAIL")
        sentinel = {"expected_change": 1200, "result": {"outcome": "attributed", "position_change": 100}}
        self.assertEqual(pt.run_verdict(sentinel)[0], "FAIL")

    def test_repeat_moves_nothing(self):
        still = {"repeat": True, "expected_change": 0, "result": {"outcome": "indeterminate", "position_change": 0}}
        self.assertEqual(pt.run_verdict(still)[0], "PASS")
        moved = {"repeat": True, "expected_change": 0, "result": {"outcome": "attributed", "position_change": 800,
                                                                    "running_observed": True}}
        self.assertEqual(pt.run_verdict(moved)[0], "FAIL")

    def test_stop_verdict(self):
        trial = {"stop_sent": {"policy": "fast", "outputs_after": {"relay2": {"active": False}}},
                 "results": {1: {"outcome": "stopped", "further": "none_seen", "position_change": 1200}},
                 "expected": {1: 3200}}
        self.assertEqual(pt.stop_verdict(trial, [1])[0], "PASS")
        trial["stop_sent"]["outputs_after"]["relay2"]["active"] = True
        self.assertEqual(pt.stop_verdict(trial, [1])[0], "FAIL")


def facts(address, functions, running=False):
    return {"address": address, "unknown": [], "ages_ms": {"io": 10.0, "read-state": 5.0}, "functions": list(functions),
            "polarity": 0, "running": running, "raw_alarm": 0, "alarm_flag": False}


def member(address, output="relay2", terminal="x1", function=14, trigger_off=True):
    return {"address": address, "output": output, "terminal": terminal, "function": function, "polarity": 0,
            "effect": records.SELECT_EFFECT, "trigger_terminal": "x0", "trigger_off": trigger_off}


class SelectionInterlockTest(unittest.TestCase):
    def test_named_selection_member_row_p(self):
        row = records.decide_participant(facts(1, [12, 14, 15, 16]), "relay2", named=member(1))
        self.assertEqual((row["row"], row["decision"]), ("P", "admit"))

    def test_unnamed_selector_with_trigger_refuses_l6(self):
        row = records.decide_participant(facts(1, [12, 14, 15, 16]), "relay2")
        self.assertEqual(row["row"], "L6")

    def test_selection_member_refusals(self):
        self.assertEqual(records.decide_participant(facts(1, [12, 14, 15, 16]), "relay2",
                                                    named=member(1, trigger_off=False))["row"], "L4")
        self.assertEqual(records.decide_participant(facts(1, [7, 14, 15, 16]), "relay2", named=member(1))["row"], "L4")
        self.assertEqual(records.decide_participant(facts(1, [12, 14, 15, 16], running=True), "relay2",
                                                    named=member(1))["row"], "L4")
        self.assertEqual(records.decide_participant(facts(1, [12, 14, 15, 16]), "relay1", named=member(1))["row"], "L4")

    def test_non_member_limits_admitted(self):
        row = records.decide_participant(facts(2, [1, 2, 3, 0]), "relay2", named=[member(1)])
        self.assertEqual(row["row"], "L7")

    def test_held_outputs(self):
        sel = dict(pt.SELECTION)
        f = {1: facts(1, [12, 14, 15, 16]), 2: facts(2, [1, 2, 3, 0]), 4: facts(4, [1, 2, 3, 0])}
        self.assertIsNone(records.held_selection_problem(["relay2"], "relay1", sel, f))
        self.assertIsNone(records.held_selection_problem(["relay2", "relay1"], "mosfet1", sel, f))
        self.assertIn("trigger output is on", records.held_selection_problem(["mosfet1"], "relay2", sel, f))
        self.assertIn("no PIN selection", records.held_selection_problem(["relay2"], "relay1", None, f))
        f[2] = facts(2, [1, 7, 3, 0])  # a start function on the held relay2 terminal X1
        self.assertIn("start function", records.held_selection_problem(["relay2"], "relay1", sel, f))


if __name__ == "__main__":
    unittest.main()
