"""Offline checks of the coverage matrix rules (wired bench 20 item 5)."""
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("coverage_matrix", ROOT / "scripts/coverage_matrix.py")
matrix = importlib.util.module_from_spec(spec)
spec.loader.exec_module(matrix)


def inventory(*operations):
    return {"operations": [dict(dict(id="op", implementation="IMPLEMENTED", records=["WORD"], cli_commands=["cmd"],
                                     hardware=dict(state="PASS", evidence=["docs/reports/2026-11-18_a.md",
                                                                           "docs/reports/2026-11-20_b.md"]),
                                     reason="why"), **o) for o in operations]}


class CoverageMatrixTests(unittest.TestCase):
    def test_rows_cover_fields_indexes_and_routes_with_the_latest_report(self):
        rows = matrix.rows(inventory(dict(choices=["Bit.A"])))
        self.assertEqual([r["key"] for r in rows], ["field WORD", "index Bit.A", "route cmd"])
        self.assertEqual(rows[0]["historical"], "docs/reports/2026-11-20_b.md")
        self.assertEqual(matrix.problems(rows), [])

    def test_g3_row_cannot_get_a_final_image_pass(self):
        rows = matrix.rows(inventory({}), {"op": dict(case="M1-1", gate="G3", verdict="PASS")})
        self.assertTrue(all("G3 row" in p for p in matrix.problems(rows)))
        self.assertEqual(len(matrix.problems(rows)), len(rows))

    def test_unsupported_row_gets_no_hardware_effect_credit(self):
        rows = matrix.rows(inventory(dict(implementation="UNSUPPORTED")), {"op": dict(case="M2-1", verdict="PASS")})
        self.assertTrue(all("refusal check only" in p for p in matrix.problems(rows)))
        self.assertTrue(all(r["level"] == "sent" for r in rows))
        refused = matrix.rows(inventory(dict(implementation="UNSUPPORTED")), {"op": dict(case="M2-1", verdict="REFUSED")})
        self.assertEqual(matrix.problems(refused), [])

    def test_a_fail_is_never_rolled_up_away_and_a_pass_never_widens(self):
        self.assertEqual(matrix.rollup("FAIL", ["PASS", "PASS"]), "FAIL")
        self.assertEqual(matrix.rollup("PASS", ["PASS", "FAIL"]), "FAIL")
        self.assertEqual(matrix.rollup("PASS", ["PASS", "NOT_RUN"]), "PARTIAL")
        self.assertEqual(matrix.rollup("PASS", ["PASS", "PASS"]), "PASS")
        rows = matrix.rows(inventory(dict(hardware=dict(state="FAIL", evidence=[]))), {"op": dict(case="M1", verdict="PASS")})
        self.assertEqual(matrix.operations(rows), {"op": "FAIL"})

    def test_strict_mode_needs_a_case_or_exclusion_per_field(self):
        rows = matrix.rows(inventory({}))
        self.assertEqual(matrix.problems(rows, strict=True), ["op field WORD: testable field without a case or an exclusion"])
        rows = matrix.rows(inventory({}), {"op:WORD": dict(exclusion="stored record not executed (13)")})
        self.assertEqual(matrix.problems(rows, strict=True), [])
        self.assertRegex(matrix.problems(matrix.rows(inventory({}), {"op": dict(verdict="PASS")}))[0], "without a final-image case")

    def test_the_repository_inventory_builds_a_matrix(self):
        rows = matrix.rows(json.loads(matrix.INVENTORY.read_text(encoding="utf-8")))
        self.assertTrue(rows)
        self.assertEqual(matrix.problems(rows), [])
        self.assertIn("| id | key |", matrix.markdown(rows[:2]))


if __name__ == "__main__":
    unittest.main()
