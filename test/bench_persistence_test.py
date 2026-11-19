"""Wired bench 17 harness: field table, gates, address policy, marker and the
grammar and reply checks of the stated-facts and lease routes."""
import sys
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import bench_persistence as bp  # noqa: E402
import bench_probe as probe  # noqa: E402
import bench_records as records  # noqa: E402


def baseline_raws(address):
    drive = records.load_baseline()["drives"][str(address)]
    raws = {k: v.get("raw") for k, v in drive["reads"].items()}
    raws.update({k: v.get("raw") for k, v in drive["live"].items()})
    return raws


class FieldTable(unittest.TestCase):
    def setUp(self):
        self.rows = bp.field_table(4, baseline_raws(4))

    def test_every_baseline_field_has_storage_route_and_value(self):
        self.assertEqual(len(self.rows), 191)
        self.assertTrue(all(r["live"] is not None and r["storage"] in ("RW", "RW/S") for r in self.rows))
        self.assertTrue(all(r["stored"] is None and not r["stored_known"] for r in self.rows))
        routes = {r["key"]: r["route"] for r in self.rows}
        self.assertEqual(routes["config.baud"], "session")
        self.assertEqual(routes["jog.speed"], "D7 snapshot")
        self.assertEqual(routes["drive.positive-limit"], "typed setter")
        # Start record 1 discards writes: its row blocks a factory restore.
        self.assertEqual([g.split(":")[0] for g in bp.factory_gate(self.rows)], ["records.start[1]"])

    def test_unread_words_are_named(self):
        unread = bp.unread_words(self.rows)
        self.assertEqual(unread[:2], ["0x003B UNSPECIFIED COLLISION_THRESHOLD_003B",
                                      "0x003C UNSPECIFIED COLLISION_CURRENT_003C"])
        self.assertEqual(len(unread), 18)  # Plus the 16 reserved PT record words.
        self.assertTrue(all("RESERVED" in w for w in unread[2:]))

    def test_table_token(self):
        sha = bp.table_sha256(self.rows)
        self.assertEqual(sha, bp.table_sha256(bp.field_table(4, baseline_raws(4))))
        self.assertEqual(bp.backup_token(sha), sha[:8].lower())
        with self.assertRaises(ValueError):
            bp.backup_token("0" * 64)

    def test_save_gate_needs_stored_image_and_named_changes(self):
        raws = baseline_raws(4)
        self.assertTrue(all("stored value unknown" in b for b in bp.save_gate(self.rows)))
        rows = bp.field_table(4, raws, raws)
        self.assertEqual(bp.differences(rows), [])
        self.assertEqual(bp.save_gate(rows), [])
        changed = dict(raws)
        changed["control"] = list(raws["control"])
        changed["control"][7] = raws["control"][7] + 1  # lock-delay 0x0107 in RAM.
        rows = bp.field_table(4, changed, raws)
        diffs = bp.differences(rows)
        self.assertEqual([(d["key"], d["status"]) for d in diffs], [("control.lock-delay", "ram_only")])
        self.assertEqual(len(bp.save_gate(rows)), 1)
        self.assertEqual(bp.save_gate(rows, named=(("control.lock-delay", None),)), [])


class Policies(unittest.TestCase):
    def test_address_policy(self):
        for address in bp.FORBIDDEN_ADDRESSES[1:]:
            self.assertIn("function or exception code", bp.address_problem(address))
        self.assertEqual(bp.address_problem(0), "outside 1..247")
        self.assertEqual(bp.address_problem(248), "outside 1..247")
        self.assertEqual(bp.address_problem(2), "address in use on this bus")
        self.assertIsNone(bp.address_problem(bp.CUSTOM_ADDRESS))

    def test_witness_plan_and_verdict(self):
        self.assertEqual(bp.witness_plan(120)["marker"], 7)
        self.assertEqual(bp.witness_plan(7)["marker"], 8)
        self.assertEqual(bp.witness_verdict(7, 120, 7, 120), "witnessed")
        self.assertEqual(bp.witness_verdict(7, 7, 7, 120), "no_restart")
        self.assertEqual(bp.witness_verdict(120, 120, 7, 120), "contradicted")
        self.assertEqual(bp.witness_verdict(7, 99, 7, 120), "contradicted")

    def test_factory_guard(self):
        self.assertIsNone(bp.factory_guard(["@4 persistence plan factory-restore", "@5 persistence begin save"]))
        self.assertIn("second", bp.factory_guard(["@9 persistence begin factory-restore"]))

    def test_owner_cases_refused_without_traffic(self):
        verdicts = []
        run = types.SimpleNamespace(verdict=lambda *a, **k: verdicts.append(a), scenario={"drives": []})
        args = types.SimpleNamespace(cases=["P0", "C1", "F1"], drives=None)
        detail = bp.run_cases(None, run, {}, args, 1.0)  # No console: nothing may reach it.
        self.assertEqual(set(detail["cases"]), {"P0", "C1", "F1"})
        self.assertTrue(all(v[2] == "NOT RUN" and v[3].startswith("deferred: owner away") for v in verdicts))
        plan = bp.plan(types.SimpleNamespace(cases=["S0", "D1"], drives=None), {}, {})
        self.assertEqual(plan[0], [])
        self.assertEqual(plan[4]["writes"], 0)


class Grammar(unittest.TestCase):
    def test_arguments(self):
        good = {
            "communication-qualification-install": ("address", "20", "stationary", "effects", "route-back", "dip-off"),
            "persistence-qualification-install": ("save", "stationary", "effects", "route-back", "backup", "945373b7"),
            "persistence-node": ("restore", "0", "55", "dip-observed"),
        }
        for command, tokens in good.items():
            probe.commissioning_arguments(command, tokens)
        self.assertTrue(probe.commissioning_arguments("persistence-node", ("restore", "0", "55", "dip-observed"))["dip_observed"])
        self.assertEqual(probe.commissioning_arguments("persistence-node", ("record",))["action"], "record")
        bad = [("communication-qualification-install", ("baud", "57600", "stationary")),
               ("communication-qualification-install", ("address", "20", "stationary", "stationary")),
               ("persistence-qualification-install", ("save", "stationary", "backup")),
               ("persistence-qualification-install", ("save", "backup", "0")),
               ("persistence-qualification-install", ("save", "backup", "1", "backup", "2")),
               ("persistence-node", ("restore", "256", "1")), ("persistence-node", ("restore", "0", "0")),
               ("persistence-node", ("restore", "0", "5", "dip")), ("communication-qualification-inspect", ("x",))]
        for command, tokens in bad:
            with self.assertRaises(ValueError, msg=str(tokens)):
                probe.commissioning_arguments(command, tokens)


def handle(command, args):
    return types.SimpleNamespace(command=command, host_args=args)


def node_reply(**restore):
    base = dict(address=2, word=0, previous=20, source=55, snapshot_operation_id=9, attempted=True,
                execution="acknowledged", readback_known=True, readback=0, ok=True)
    base.update(restore)
    return dict(type="reply", profile="ess_rs", id=1, command="persistence-node", ok=True, result="done",
                request="restore", pending=False, operation_id=12, error="none",
                record=dict(present=True, address=2, target=2, word=0, source=55, recorded_us=5),
                restore=base, write_evidence={}, read_evidence={})


class Replies(unittest.TestCase):
    check = staticmethod(probe.Console._check_commissioning)

    def test_local_dispositions(self):
        h = handle("persistence-qualification-install", ("save", "stationary", "effects", "route-back", "backup", "1"))
        self.check(h, dict(ok=True, result="done"))
        self.check(h, dict(ok=False, result="missing_fact", fact="backup"))
        self.check(h, dict(ok=False, result="recommissioning_unavailable"))
        with self.assertRaises(probe.BenchError):
            self.check(h, dict(ok=False, result="missing_fact"))
        with self.assertRaises(probe.BenchError):
            self.check(h, dict(ok=True, result="done", operation_id=4))

    def test_inspection(self):
        item = dict(ok=True, result="done", present=True, validity="valid", field="baud", register=20, value=1, address=2,
                    target=2, generation=3, binding_generation=3, configuration_generation=4, serial_generation=1,
                    installed_us=1, now_us=2, maximum_age_us=60000000, stationary=True, effects=True, route_back=True,
                    dip_off=False, bus_traffic=False)
        h = handle("communication-qualification-inspect", ())
        self.check(h, item)
        with self.assertRaises(probe.BenchError):
            self.check(h, {**item, "present": False})  # Absent but reported valid with a field.

    def test_node_route(self):
        h = handle("persistence-node", ("restore", "0", "55", "dip-observed"))
        self.check(h, node_reply())
        for broken in (dict(readback=5), dict(execution="unknown"), dict(word=3, readback=3)):
            with self.assertRaises(probe.BenchError, msg=str(broken)):
                self.check(h, node_reply(**broken))
        refused = node_reply(attempted=False, ok=False, readback_known=False, execution="not_transmitted")
        self.check(h, {**refused, "ok": False, "result": "dip_evidence_stale"})
        with self.assertRaises(probe.BenchError):
            self.check(h, {**refused, "ok": False, "result": "restart_marker_required"})

if __name__ == "__main__":
    unittest.main()
