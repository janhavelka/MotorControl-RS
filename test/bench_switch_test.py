"""Wired bench 12 virtual switch: decisions, bound, preflight, live budget,
skipped windows, hold renewal, travel guard and the branch analysis (no port)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import bench_records as records  # noqa: E402
import bench_switch as sw  # noqa: E402

BUDGET = {"gap_ms": 150, "decision_ms": 700}


def reply(position, t_us, address=1, motion=4, operation_id=1):
    words = [(position >> 16) & 0xFFFF, position & 0xFFFF]
    steps = [{"first": first, "attempted_us": t_us + k * 25000, "latest_us": t_us + k * 25000 + 20000}
             for k, first in enumerate((6, 8, 10))]
    return {"ok": True, "operation_id": operation_id, "address": address,
            "state_blocks": [{"block": 0, "raw_motion": motion, "raw_alarm": 0},
                             {"block": 1, "raw_outputs": 0, "raw_inputs": 0},
                             {"block": 2, "raw_speed": 10, "position_words": words}],
            "steps": steps}


class FakeConsole:
    """read state from a scripted (position, time) list; bench off and renewals recorded."""

    def __init__(self, track):
        self.track = list(track)
        self.sent = []
        self.now = 0

    def command(self, name, **kwargs):
        self.sent.append((name, kwargs))
        if name == "read-state":
            position, self.now = self.track.pop(0)
            return reply(position, self.now)
        if name == "bench":
            args = kwargs.get("host_args", ())
            output = args[1] if len(args) > 1 else "mosfet1"
            active = args[:1] == ("on",)
            return {"ok": True, "now_us": self.now + 60000,
                    "outputs": [{"name": output, "active": active, "on_us": 1000 if active else None,
                                 "off_us": None if active else self.now + 60000, "hold_ms": 0, "ons": 1}]}
        if name == "stop":
            return {"ok": True, "outcome": "observed"}
        raise AssertionError(name)


class FakeLive:
    def __init__(self, track):
        self.console = FakeConsole(track)
        self.samples, self.views, self.events = [], [], []
        self.record = {}
        self.timeout = 1.0
        self.address = 1

    def call(self, name, **kwargs):
        return self.console.command(name, **kwargs)

    def checked(self, name, **kwargs):
        return self.console.command(name, **kwargs)


def switcher(track, method=24, branch="approaching", start=0, v_high=250.0, budget=BUDGET, gpio_delay_us=500000):
    live = FakeLive(track)
    g = sw.geometry(method, branch, start)
    s = sw.Switcher(live, g, v_high, v_high / 2, budget, 77, {"io_read": {}, "between": []})
    gpio = []

    def on(output, running=True):
        t = live.console.now + 20000 + gpio_delay_us
        s.level[output], s.on_us[output] = True, t
        s.renewed[output] = sw.time.monotonic()
        gpio.append((output, t))
        return t
    s.assert_output = on
    stops = []
    s.stop = lambda reason: (stops.append(reason), setattr(s, "stop_path", reason))
    return s, live, gpio, stops


class Geometry(unittest.TestCase):
    def test_layouts_and_levels(self):
        g = sw.geometry(24, "approaching", 100)
        self.assertEqual(g["origin"], [3100, 4100])
        self.assertEqual(g["positive"], 8100)
        self.assertIsNone(g["negative"])
        self.assertEqual(sw.levels(g, 3100), {"mosfet1": True, "relay2": False, "relay1": False})
        self.assertEqual(sw.levels(g, 8100)["relay2"], True)
        a = sw.geometry(24, "active", 0)
        self.assertEqual(a["origin"], [-500, 500])
        self.assertTrue(sw.levels(a, 0)["mosfet1"])
        b = sw.geometry(27, "beyond", 0)  # Negative family: limit 500 behind (search -), origin ahead.
        self.assertEqual(b["negative"], -500)
        self.assertEqual(b["origin"], [3500, 4500])
        self.assertEqual(sw.levels(b, 0), {"mosfet1": False, "relay2": False, "relay1": False})
        limit = sw.geometry(17, "active", 0)
        self.assertEqual(limit["negative"], 500)  # Start inside the negative limit.
        self.assertTrue(sw.levels(limit, 0)["relay1"])
        self.assertEqual(sw.geometry(17, "approaching", 0)["negative"], -3000)
        with self.assertRaises(ValueError):
            sw.geometry(20, "beyond", 0)
        self.assertIsNone(sw.geometry(20, "approaching", 0, origin=False)["origin"])

    def test_method_rows(self):
        expected = set(range(1, 15)) | {17} | set(range(19, 31))
        self.assertEqual(set(sw.METHODS), expected)
        self.assertEqual(sw.METHODS[25]["paths"], {"approaching": "+-", "beyond": "+-+-", "active": "+-"})
        self.assertEqual(sw.METHODS[13]["paths"]["beyond"], "-+-+")
        self.assertTrue(sw.METHODS[9]["index"] and not sw.METHODS[25]["index"])
        self.assertEqual(sw.METHODS[22]["paths"], {"approaching": "-", "active": "+-"})

    def test_rate(self):
        self.assertEqual(sw.rate(60), 1000.0)
        self.assertEqual(sw.rate(30), 500.0)


class Bound(unittest.TestCase):
    def test_edge_bound(self):
        # 250/s over (t_gpio + 35 ms - t0) = 0.6 + 0.035 s, plus the margin.
        self.assertAlmostEqual(sw.edge_bound(0, 600000, 250.0), 250 * 0.635 + sw.EDGE_MARGIN)

    def test_preflight(self):
        g = sw.geometry(24, "approaching", 0)
        self.assertIn("quarter", sw.preflight(g, sw.rate(60), BUDGET))
        self.assertIsNone(sw.preflight(g, sw.rate(15), BUDGET))
        narrow = dict(g, origin=[0, 60])
        self.assertIsNotNone(sw.preflight(narrow, 100.0, {"gap_ms": 400, "decision_ms": 10}))
        self.assertIsNone(sw.preflight(sw.geometry(17, "approaching", 0), sw.rate(60), BUDGET))  # A limit alone.

    def test_skipped(self):
        g = sw.geometry(20, "approaching", 0)
        self.assertEqual(sw.skipped(g, 2900, 4200), ["mosfet1"])
        self.assertEqual(sw.skipped(g, 2900, 3500), [])


class LiveChecks(unittest.TestCase):
    def test_on_then_off_valid(self):
        # 1000/s against a 1000/s search (ceiling 1250/s).
        track = [(2800, 0), (2900, 100000), (3010, 210000), (3800, 1000000), (3900, 1100000), (4005, 1205000),
                 (4110, 1310000)]
        s, live, gpio, stops = switcher(track, v_high=1000.0, gpio_delay_us=400000)
        for _ in range(len(track)):
            self.assertTrue(s.step())
        self.assertEqual(stops, [])
        self.assertEqual([(e["output"], e["level"]) for e in s.edges], [("mosfet1", True), ("mosfet1", False)])
        on = s.edges[0]
        self.assertTrue(on["valid"] and on["boundary"] == 3000 and on["dx"] > 0)
        self.assertEqual(s.edges[1]["boundary"], 4000)

    def test_gap_over_budget_stops(self):
        s, live, gpio, stops = switcher([(2900, 0), (3010, 400000)])
        s.step()
        self.assertFalse(s.step())
        self.assertTrue(stops and "gap" in stops[0])
        self.assertFalse(s.edges[0]["valid"])

    def test_decision_over_budget_stops(self):
        s, live, gpio, stops = switcher([(2980, 0), (3010, 100000)], gpio_delay_us=900000)
        s.step()
        self.assertFalse(s.step())
        self.assertIn("decision", stops[0])

    def test_skipped_window_stops(self):
        s, live, gpio, stops = switcher([(2900, 0), (4200, 100000)], v_high=20000.0)
        s.step()
        self.assertFalse(s.step())
        self.assertIn("window crossed", stops[0])

    def test_speed_ceiling(self):
        # 500/s against a 250/s search: over the 1.25 x ceiling, below a coordinate change.
        track = [(2800, 0), (2850, 100000), (2900, 200000), (2950, 300000), (3005, 410000)]
        s, live, gpio, stops = switcher(track)
        results = [s.step() for _ in track]
        self.assertFalse(all(results))
        self.assertTrue(any("speed" in r for r in stops))

    def test_guard(self):
        s, live, gpio, stops = switcher([(0, 0), (12001, 100000)], v_high=200000.0)
        s.step()
        self.assertFalse(s.step())
        self.assertIn("travel guard", stops[0])

    def test_coordinate_change_freezes(self):
        track = [(2980, 0), (3005, 100000), (3030, 200000), (0, 300000), (25, 400000)]
        s, live, gpio, stops = switcher(track)
        for _ in track:
            self.assertTrue(s.step())
        self.assertTrue(s.frozen)
        self.assertEqual(s.samples[-1]["track"], 3055)
        self.assertEqual([e["event"] for e in s.events], ["coordinate change"])

    def test_renewal_and_late_renewal(self):
        s, live, gpio, stops = switcher([(3010, 0), (3020, 100000)])
        renewed = []
        orig = records.bench_renew
        records.bench_renew = lambda console, output, hold, on_us, timeout_s=0: renewed.append((output, hold, on_us)) or {"ok": True}
        try:
            s.level["mosfet1"], s.on_us["mosfet1"] = True, 5
            s.renewed["mosfet1"] = sw.time.monotonic() - 1.0  # One cycle budget (0.85 s) elapsed.
            s.renew()
            self.assertEqual(renewed, [("mosfet1", s.hold_ms, 5)])
            self.assertEqual(s.hold_ms, 3 * (150 + 700))
            s.renewed["mosfet1"] = sw.time.monotonic() - 2.4  # Within 0.5 s of the hold: switched off instead.
            s.renew()
            self.assertEqual(len(renewed), 1)
            self.assertFalse(s.level["mosfet1"])
            self.assertEqual(live.console.sent[-1], ("bench", {"host_args": ("off", "mosfet1")}))
        finally:
            records.bench_renew = orig


class Renewal(unittest.TestCase):
    def test_lapse_is_detected(self):
        class C:
            def __init__(self, on_us):
                self.on_us, self.sent = on_us, []

            def command(self, name, **kwargs):
                self.sent.append(kwargs.get("host_args"))
                return {"ok": True, "outputs": [{"name": "mosfet1", "active": True, "on_us": self.on_us}]}
        good = C(100)
        records.bench_renew(good, "mosfet1", 2000, 100)
        self.assertEqual(good.sent, [("on", "mosfet1", "2000")])
        lapsed = C(900)
        with self.assertRaises(records.BenchError):
            records.bench_renew(lapsed, "mosfet1", 2000, 100)
        self.assertEqual(lapsed.sent[-1], ("off",))

    def test_home_member_io_bound(self):
        self.assertEqual(records.named_io_bound(None), records.NAMED_IO_MAX_AGE_MS)
        self.assertEqual(records.named_io_bound({"io_age_bound_ms": 31000}), 31000)
        self.assertEqual(records.named_io_bound({"io_age_bound_ms": 60000}), records.NAMED_IO_MAX_AGE_MS)
        member = {"output": "mosfet1", "terminal": "x0", "effect": "origin", "function": 1, "polarity": 0,
                  "operation_id": 9, "address": 1, "io_age_bound_ms": 31000}
        home = [{"operation_id": 9, "address": 1, "command": "home"}]
        move = [{"operation_id": 9, "address": 1, "command": "move-relative"}]
        self.assertIsNone(records._named_mismatch(member, "mosfet1", "x0", 1, 0, home))
        self.assertIn("needs a pending home", records._named_mismatch(member, "mosfet1", "x0", 1, 0, move))
        self.assertIn("origin", records.EFFECTS)


class Headroom(unittest.TestCase):
    def test_evidence_headroom(self):
        class Budget:
            def __init__(self, used_bytes, used_records, exhausted=False):
                self.max_bytes, self.max_records = 64 * 1024 * 1024, 200000
                self.bytes, self.records, self.exhausted = used_bytes, used_records, exhausted

        class C:
            pass
        c = C()
        self.assertIsNone(sw.headroom_problem(c))
        c.evidence_budget = Budget(10 * 1024 * 1024, 1000)
        self.assertIsNone(sw.headroom_problem(c))
        c.evidence_budget = Budget(61 * 1024 * 1024, 1000)
        self.assertIn("headroom", sw.headroom_problem(c))
        c.evidence_budget = Budget(0, 195000)
        self.assertIn("headroom", sw.headroom_problem(c))
        c.evidence_budget = Budget(0, 0, exhausted=True)
        self.assertIsNotNone(sw.headroom_problem(c))


class Moving(unittest.TestCase):
    def test_still_moving(self):
        self.assertTrue(sw.still_moving({"motion": 4, "speed": 0}))
        self.assertTrue(sw.still_moving({"motion": 0, "speed": 12}))
        self.assertFalse(sw.still_moving({"motion": 0, "speed": 5}))
        self.assertFalse(sw.still_moving({"motion": 0, "speed": 65532}))
        self.assertTrue(sw.still_moving({"motion": 0, "speed": 65524}))


class Analysis(unittest.TestCase):
    @staticmethod
    def samples(points):
        return [{"track": x, "frame": 0, "windows": {"feedback": [t, t + 20000]}} for x, t in points]

    def test_segments_and_path(self):
        pts = [(0, 0), (500, 100000), (3100, 600000), (3150, 700000), (3120, 800000), (3000, 900000), (2990, 1000000),
               (3010, 1100000), (3200, 1300000)]
        segs = sw.segments(self.samples(pts))
        self.assertEqual(sw.path_string(segs), "+-+")
        self.assertEqual(sw.path_string(sw.segments(self.samples([(0, 0), (40, 100000), (10, 200000)]))), "")

    def test_final_and_verdict(self):
        g = sw.geometry(24, "approaching", 0)
        edges = [{"output": "mosfet1", "level": True, "boundary": 3000, "dx": 150.0, "valid": True, "switched": True,
                  "problems": []}]
        segs = [{"sign": 1}]
        self.assertTrue(sw.final_check(g, edges, 3080, segs)["ok"])
        self.assertFalse(sw.final_check(g, edges, 3300, segs)["ok"])
        trial = {"method": 24, "branch": "approaching", "path": "+-+", "edges": edges, "stop_path": None,
                 "terminal": {"ok": True}, "final": sw.final_check(g, edges, 3080, segs)}
        v = sw.branch_verdict(trial)
        self.assertEqual((v["drive-reported"], v["controller-timestamped"]), ("PASS", "PASS"))
        self.assertEqual(sw.branch_verdict(dict(trial, path="+"))["drive-reported"], "FAIL")
        bad = [dict(edges[0], valid=False, problems=["gap"])]
        self.assertEqual(sw.branch_verdict(dict(trial, edges=bad))["drive-reported"], "indeterminate")
        self.assertEqual(sw.branch_verdict({"method": 24, "branch": "approaching", "refused": "x"})["drive-reported"],
                         "NOT RUN")
        # Internal Z: one revolution more is allowed.
        self.assertTrue(sw.final_check(sw.geometry(8, "approaching", 0), edges, 3900, segs)["ok"])


if __name__ == "__main__":
    unittest.main()
