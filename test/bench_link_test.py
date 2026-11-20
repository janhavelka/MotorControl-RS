"""Wired bench 18 link-loss harness: the watchdog reading of a silent window,
a finite move across a controller fault, the item 4 pulse credit rule, the R0
refusal check, the scenario record of every case and the deferred cases
(no port)."""
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import bench_link as link  # noqa: E402
import bench_records as records  # noqa: E402


def sample(motion=1, position=0, speed=0, alarm=0):
    return {"motion": motion, "alarm": alarm, "speed": speed, "inputs": 0, "outputs": 0,
            "position_words": [(position >> 16) & 0xFFFF, position & 0xFFFF]}


class WatchdogTests(unittest.TestCase):
    def test_running_window_reports_travel(self):
        a = link.watchdog_answer(10, sample(motion=link.RUNNING | 1, position=24000, speed=120), sample(position=0))
        self.assertEqual((a["answer"], a["travel"]), ("running", 24000))

    def test_soft_limit_stop_is_not_a_watchdog(self):
        a = link.watchdog_answer(60, sample(motion=link.SOFT_POSITIVE | link.HOMED | 1, position=150500), sample())
        self.assertEqual(a["answer"], "stopped_at_soft_limit")

    def test_plain_stop_and_unreadable(self):
        self.assertEqual(link.watchdog_answer(30, sample(motion=1, position=9000), sample())["answer"], "stopped")
        self.assertEqual(link.watchdog_answer(30, None, sample())["answer"], "unreadable")


class MoveOutcomeTests(unittest.TestCase):
    def test_completed_within_tolerance(self):
        o = link.move_outcome(100, 10000, [sample(motion=link.RUNNING, position=3000, speed=60), sample(position=10101)])
        self.assertEqual(o["class"], "completed")
        self.assertTrue(o["moving_after_fault"])

    def test_short_running_fault_and_unreadable(self):
        self.assertEqual(link.move_outcome(0, 12000, [sample(position=4000)])["class"], "stopped_short")
        self.assertEqual(link.move_outcome(0, 12000, [sample(motion=link.RUNNING, position=4000, speed=60)])["class"],
                         "still_running")
        self.assertEqual(link.move_outcome(0, 12000, [sample(position=4000, alarm=5)])["class"], "drive_fault")
        self.assertEqual(link.move_outcome(None, 12000, [sample()])["class"], "unreadable")
        self.assertEqual(link.move_outcome(0, 12000, [None])["class"], "unreadable")


class PulseCreditTests(unittest.TestCase):
    def test_completion_means_no_detected_pulse(self):
        self.assertEqual(link.pulse_credit({"class": "completed"}, True, True)["verdict"], "no_pulse_detected")

    def test_short_stop_is_credited_only_with_every_cause_excluded(self):
        short = {"class": "stopped_short"}
        self.assertEqual(link.pulse_credit(short, True, True)["verdict"], "pulse_credited")
        self.assertEqual(link.pulse_credit(short, False, True)["verdict"], "indeterminate")
        self.assertEqual(link.pulse_credit(short, True, False)["verdict"], "indeterminate")
        self.assertEqual(link.pulse_credit({"class": "drive_fault"}, True, True)["verdict"], "indeterminate")


class RefusalTests(unittest.TestCase):
    rows = [{"name": "marker read", "ok": False, "result": "marker_setter_unavailable", "reason": None,
             "frames": [7, 7]},
            {"name": "factory restore begin", "ok": False, "result": "unavailable",
             "reason": "recommissioning_unavailable", "frames": [7, 7]}]

    def test_named_refusals_with_no_frame_pass(self):
        self.assertTrue(link.refusals_pass(self.rows))

    def test_a_frame_or_another_reason_fails(self):
        framed = [dict(self.rows[0], frames=[7, 8]), self.rows[1]]
        other = [self.rows[0], dict(self.rows[1], reason="busy")]
        self.assertFalse(link.refusals_pass(framed))
        self.assertFalse(link.refusals_pass(other))
        self.assertFalse(link.refusals_pass(self.rows[:1]))


class InterlockTests(unittest.TestCase):
    """A reset or reflash case refuses to start when before_flash_check fails
    (a trigger function 7-17 assigned, a drive moving or unreadable)."""

    def test_failed_check_raises_before_any_fault(self):
        live = SimpleNamespace(console=None, timeout=1.0, detail={})
        failing = {"verdict": "FAIL", "rows": [{"address": 1, "problems": ["input function 7-17 assigned: [7, 2, 3, 0]"]}]}
        original = records.before_flash_check
        records.before_flash_check = lambda console, timeout_s: failing
        try:
            with self.assertRaises(link.probe.BenchError):
                link.interlock(live, "before")
            self.assertEqual(live.detail["interlock"]["before"], failing)
            records.before_flash_check = lambda console, timeout_s: {"verdict": "PASS", "rows": []}
            self.assertEqual(link.interlock(live, "w1")["verdict"], "PASS")
        finally:
            records.before_flash_check = original


class PlanTests(unittest.TestCase):
    def args(self, case, **extra):
        return SimpleNamespace(case=case, port="COM13", image_level="18", windows=[10.0, 30.0, 60.0],
                               esptool="esptool.exe", flash_dir=Path("flash"), **extra)

    def test_every_case_has_a_valid_record_and_an_admissible_plan(self):
        baseline = records.load_baseline()
        image = {"path": "image.bin", "sha256": "A" * 64}
        for case in link.CASES:
            record = link.scenario_record(self.args(case), image, "commit", baseline)
            data = {"schema": records.SCENARIO_SCHEMA, "prompt": "18", "slug": "x", "date": "2026-11-20",
                    "run": "2026-11-20_wb18_x", **record}
            self.assertEqual(records.check_scenario(data), [], case)
            self.assertTrue(record["restoration_plan"]["admissible"], case)
            if not link.CASES[case].get("deferred"):
                self.assertIn(case, link.EXPOSURE)

    def test_inputs_cases_write_stop_on_all_terminals_and_restore_them(self):
        record = link.scenario_record(self.args("G1"), {"path": "i", "sha256": "A" * 64}, "c", records.load_baseline())
        keys = {e["key"]: e["candidate"] for e in record["write_list"]}
        self.assertEqual({k: keys[k] for k in ("io.x0", "io.x1", "io.x2", "io.x3")}, dict.fromkeys(
            ("io.x0", "io.x1", "io.x2", "io.x3"), link.STOP_INPUT))
        restored = {s["key"] for s in record["restoration_plan"]["steps"]}
        self.assertTrue({"io.x0", "io.x1", "io.x2", "io.x3"} <= restored)

    def test_w1_bound_covers_the_longest_window(self):
        rate = link.JOG[0] * 1000 // 60  # increments/s at subdivision 1000
        self.assertGreater(link.SOFT_LIMIT, rate * (max(self.args("W1").windows) + 5))
        self.assertEqual(link.HOME24[1:], ("120", "60", "100"))  # Baseline search, return and ramp words.

    def test_g2_needs_its_flash_folder(self):
        with self.assertRaises(SystemExit):
            link.arguments(["--case", "G2", "--run-dir", "x", "--firmware", "f.bin", "--commit", "c"])


if __name__ == "__main__":
    unittest.main()
