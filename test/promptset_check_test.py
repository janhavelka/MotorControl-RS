"""The promptset validator rejects structural faults and accepts a minimal valid set."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_promptset", ROOT / "scripts/check_promptset.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)

BODY = """
## Objective and boundaries
Deliver the thing. {pad}
## Read and audit
Read the code.
### Primary evidence to recheck
The fact.
## Files and interfaces
One file.
## Implement
1. Work.
## Bench procedure
{bench}
## Error cases and negative tests
Refusals.
## Verify and exit
**Checks.** Run tests.
**Acceptance.** It works.
**Blockers.** None.
## Documentation
The guide.
"""


def prompt(number, title, preamble, bench="1. Run it."):
    return f"# {number:02d} {title}\n{preamble}\n" + BODY.format(pad="x" * 2600, bench=bench)


class PromptsetCheck(unittest.TestCase):
    def build(self, folder, *, first=None, second=None, gate2="G0", readme_gate2="G0"):
        sequence = {"prompts": [
            {"id": 1, "file": "01_first.md", "title": "First", "depends_on": [], "gate": "G1"},
            {"id": 2, "file": "02_second.md", "title": "Second", "depends_on": [1], "gate": gate2},
        ]}
        (folder / "SEQUENCE.json").write_text(json.dumps(sequence), encoding="utf-8")
        (folder / "01_first.md").write_text(first or prompt(
            1, "First", "Apply [COMMON.md](COMMON.md). Prerequisites: none. Gate: **G1**."), encoding="utf-8")
        (folder / "02_second.md").write_text(second or prompt(
            2, "Second", f"Apply [COMMON.md](COMMON.md). Prerequisites: [01](01_first.md). Gate: **{gate2}**.",
            bench="None: gate G0." if gate2 == "G0" else "1. Run it."), encoding="utf-8")
        (folder / "README.md").write_text(
            "| [01](01_first.md) | a | none | G1 |\n"
            f"| [02](02_second.md) | b | 01 | {readme_gate2} |\n"
            "| 01 | Pending | — | — |\n| 02 | Complete | — | — |\n", encoding="utf-8")

    def test_valid_set_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.build(Path(tmp))
            self.assertEqual(checker.check(Path(tmp)), [])

    def test_repository_set_passes(self):
        self.assertEqual(checker.check(ROOT / "docs/prompts/ess_wired_bench"), [])

    def test_faults_are_reported(self):
        cases = {
            "missing heading": dict(second=prompt(2, "Second", "Apply [COMMON.md](COMMON.md). Prerequisites: [01](01_first.md). Gate: **G0**.",
                                                  bench="None: gate G0.").replace("## Implement\n", "")),
            "gate mismatch": dict(second=prompt(2, "Second", "Apply [COMMON.md](COMMON.md). Prerequisites: [01](01_first.md). Gate: **G2**.",
                                                bench="None: gate G0.")),
            "missing prerequisite link": dict(second=prompt(2, "Second", "Apply [COMMON.md](COMMON.md). Prerequisites: none. Gate: **G0**.",
                                                            bench="None: gate G0.")),
            "G0 bench statement": dict(second=prompt(2, "Second", "Apply [COMMON.md](COMMON.md). Prerequisites: [01](01_first.md). Gate: **G0**.")),
            "placeholder": dict(first=prompt(1, "First", "Apply [COMMON.md](COMMON.md). Prerequisites: none. Gate: **G1**.") + "\nTODO later\n"),
            "readme gate": dict(readme_gate2="G2"),
            "wrong title": dict(first=prompt(1, "Other", "Apply [COMMON.md](COMMON.md). Prerequisites: none. Gate: **G1**.")),
        }
        for label, overrides in cases.items():
            with self.subTest(label), tempfile.TemporaryDirectory() as tmp:
                self.build(Path(tmp), **overrides)
                self.assertTrue(checker.check(Path(tmp)), label)


if __name__ == "__main__":
    unittest.main()
