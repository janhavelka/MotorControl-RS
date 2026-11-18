#!/usr/bin/env python3
"""Structural checks for a sequenced promptset (docs/prompts/<set>/).

Checks SEQUENCE.json against the prompt files and the README index/status
tables: file names and titles, the preamble (prerequisite links and gate),
the required headings in order, the Verify-and-exit paragraphs, the G0 bench
statement, size bounds and placeholder words. Exit status 1 lists every
problem; nothing is modified.
"""
import argparse
import json
from pathlib import Path
import re
import sys

HEADINGS = ("## Objective and boundaries", "## Read and audit", "### Primary evidence to recheck",
            "## Files and interfaces", "## Implement", "## Bench procedure",
            "## Error cases and negative tests", "## Verify and exit", "## Documentation")
GATES = ("G0", "G1", "G2", "G3")
PLACEHOLDERS = re.compile(r"\b(TODO|TBD|FIXME|XXX)\b")
MIN_BYTES, MAX_BYTES = 2500, 24000


def check(folder: Path) -> list[str]:
    errors = []
    try:
        sequence = json.loads((folder / "SEQUENCE.json").read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        return [f"SEQUENCE.json: {exc}"]
    prompts = sequence.get("prompts", [])
    if not prompts:
        return ["SEQUENCE.json: no prompts"]
    by_id = {}
    for entry in prompts:
        pid = entry.get("id")
        if type(pid) is not int or pid in by_id:
            errors.append(f"SEQUENCE.json: invalid or duplicate id {pid!r}")
            continue
        by_id[pid] = entry
    ids = sorted(by_id)
    if ids != list(range(1, len(ids) + 1)):
        errors.append("SEQUENCE.json: ids must be 1..N without gaps")
    listed = set()
    for pid in ids:
        entry = by_id[pid]
        name = entry.get("file", "")
        listed.add(name)
        label = f"{pid:02d}"
        if not re.fullmatch(rf"{label}_[a-z0-9_]+\.md", name):
            errors.append(f"{name}: file name must be {label}_slug.md")
        if entry.get("gate") not in GATES:
            errors.append(f"{name}: gate must be one of {GATES}")
        deps = entry.get("depends_on", [])
        if any(type(d) is not int or d not in by_id or d >= pid for d in deps):
            errors.append(f"{name}: depends_on must name earlier prompts")
        path = folder / name
        if not path.is_file():
            errors.append(f"{name}: missing prompt file")
            continue
        raw = path.read_bytes()
        if not MIN_BYTES <= len(raw) <= MAX_BYTES:
            errors.append(f"{name}: size {len(raw)} bytes outside {MIN_BYTES}..{MAX_BYTES}")
        text = raw.decode("utf-8").replace("\r\n", "\n")
        lines = [line for line in text.split("\n") if line.strip()]
        if not lines or lines[0] != f"# {label} {entry.get('title', '')}":
            errors.append(f"{name}: first line must be '# {label} {entry.get('title', '')}'")
        preamble = lines[1] if len(lines) > 1 else ""
        if not preamble.startswith("Apply [COMMON.md](COMMON.md)."):
            errors.append(f"{name}: second line must start with 'Apply [COMMON.md](COMMON.md).'")
        gate = re.search(r"Gate: \*\*(G[0-3])\*\*\.", preamble)
        if not gate or gate.group(1) != entry.get("gate"):
            errors.append(f"{name}: preamble gate must be **{entry.get('gate')}**")
        linked = sorted({int(m) for m in re.findall(r"\]\((\d\d)_[a-z0-9_]+\.md\)", preamble.split("Gate:")[0])})
        if linked != sorted(deps):
            errors.append(f"{name}: preamble prerequisite links {linked} differ from depends_on {sorted(deps)}")
        if not deps and "Prerequisites: none." not in preamble:
            errors.append(f"{name}: preamble must say 'Prerequisites: none.'")
        positions = []
        for heading in HEADINGS:
            match = re.search(rf"^{re.escape(heading)}\s*$", text, re.M)
            if not match:
                errors.append(f"{name}: missing heading '{heading}'")
            else:
                positions.append(match.start())
        if positions != sorted(positions):
            errors.append(f"{name}: headings out of order")
        verify = text.split("## Verify and exit", 1)[-1].split("\n## ", 1)[0]
        for paragraph in ("**Checks.**", "**Acceptance.**", "**Blockers.**"):
            if paragraph not in verify:
                errors.append(f"{name}: Verify and exit lacks {paragraph}")
        bench = text.split("## Bench procedure", 1)[-1].split("\n## ", 1)[0]
        if entry.get("gate") == "G0" and "None: gate G0." not in bench:
            errors.append(f"{name}: a G0 prompt states 'None: gate G0.' under Bench procedure")
        if PLACEHOLDERS.search(text):
            errors.append(f"{name}: contains a placeholder word")
    for path in folder.glob("[0-9][0-9]_*.md"):
        if path.name not in listed:
            errors.append(f"{path.name}: prompt file not in SEQUENCE.json")
    readme = (folder / "README.md").read_text(encoding="utf-8") if (folder / "README.md").is_file() else ""
    for pid in ids:
        entry = by_id[pid]
        label = f"{pid:02d}"
        index_row = re.search(rf"^\| \[{label}\]\({re.escape(entry['file'])}\) \|(.*)$", readme, re.M)
        if not index_row:
            errors.append(f"README.md: index row for {label} missing or linking another file")
        else:
            cells = [c.strip() for c in index_row.group(1).split("|")]
            gate_cell = cells[-2] if len(cells) >= 2 else ""
            if gate_cell != entry["gate"]:
                errors.append(f"README.md: index gate for {label} is {gate_cell!r}, expected {entry['gate']}")
        if not re.search(rf"^\| {label} \| (Pending|Partial|Blocked|Complete) \|", readme, re.M):
            errors.append(f"README.md: status row for {label} missing or with an unknown state")
    return errors


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path, nargs="?", default=Path(__file__).resolve().parents[1] / "docs/prompts/ess_wired_bench")
    args = parser.parse_args(argv)
    errors = check(args.folder)
    for error in errors:
        print(error)
    print(f"{args.folder.name}: {'FAIL' if errors else 'PASS'} ({len(errors)} problems)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
