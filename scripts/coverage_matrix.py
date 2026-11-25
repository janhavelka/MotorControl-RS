#!/usr/bin/env python3
"""Coverage matrix of the ESS operation inventory (wired bench 20 item 5).

One row per inventory ID and field (record), index (choice) or console route,
with the historical evidence, the final-image case, gate, drive, fixture,
evidence level and remaining reason. Final-image cases come from a JSON case
map (``--cases``) keyed by ``ID`` or ``ID:KEY``; each entry may name ``case``,
``gate``, ``drives``, ``fixture``, ``level``, ``verdict`` and ``reason``, or
``exclusion`` instead of a case.

Rules: a G3 row cites its wired bench 16-19 evidence and never gets a
final-image PASS; an UNSUPPORTED row gets a refusal check only (verdict
REFUSED, level ``sent``) and no hardware-effect credit; a roll-up never erases
a known FAIL or widens an earlier PASS. Reads files only; no bench traffic.
"""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "docs/reference/ess_rs_operations.json"
VERDICTS = ("PASS", "FAIL", "NOT_RUN", "BLOCKED", "REFUSED")
COLUMNS = ("id", "key", "historical", "case", "gate", "drives", "fixture", "level", "verdict", "reason")


def historical(operation):
    """Latest dated report among the operation's hardware evidence, or None."""
    reports = sorted(p for p in operation.get("hardware", {}).get("evidence", []) if "/reports/" in p)
    return reports[-1] if reports else None


def keys(operation):
    found = [("field", r) for r in operation.get("records", [])]
    found += [("index", c) for c in operation.get("choices", [])]
    found += [("route", c) for c in operation.get("cli_commands", [])]
    return found or [("operation", operation["id"])]


def rows(inventory, cases=None):
    """Matrix rows from the inventory and the case map."""
    cases = cases or {}
    out = []
    for operation in inventory["operations"]:
        for kind, key in keys(operation):
            entry = dict(cases.get(operation["id"], {}), **cases.get(f"{operation['id']}:{key}", {}))
            row = dict(id=operation["id"], key=f"{kind} {key}", implementation=operation["implementation"],
                       inventory=operation["hardware"]["state"], historical=historical(operation),
                       case=entry.get("case"), gate=entry.get("gate"), drives=entry.get("drives"),
                       fixture=entry.get("fixture"), level=entry.get("level"),
                       verdict=entry.get("verdict", "NOT_RUN"), exclusion=entry.get("exclusion"),
                       reason=entry.get("reason") or entry.get("exclusion") or operation.get("reason"))
            if row["implementation"] == "UNSUPPORTED":
                row["level"] = "sent"  # Refused before TX: no hardware-effect credit.
            out.append(row)
    return out


def problems(matrix, strict=False):
    """Rule violations; ``strict`` also requires a case or an exclusion on every field row."""
    found = []
    for row in matrix:
        name = f"{row['id']} {row['key']}"
        if row["verdict"] not in VERDICTS:
            found.append(f"{name}: unknown verdict {row['verdict']}")
        if row["gate"] == "G3" and row["verdict"] == "PASS":
            found.append(f"{name}: a G3 row cannot get a final-image PASS; cite 16-19")
        if row["implementation"] == "UNSUPPORTED" and row["verdict"] not in ("REFUSED", "NOT_RUN", "FAIL"):
            found.append(f"{name}: UNSUPPORTED gets a refusal check only, no hardware-effect credit")
        if row["implementation"] != "UNSUPPORTED" and row["verdict"] == "REFUSED":
            found.append(f"{name}: REFUSED is the UNSUPPORTED refusal check only")
        if row["verdict"] != "NOT_RUN" and not row["case"]:
            found.append(f"{name}: verdict without a final-image case")
        if strict and row["key"].startswith("field ") and not (row["case"] or row["exclusion"]):
            found.append(f"{name}: testable field without a case or an exclusion")
    return found


def rollup(inventory_state, verdicts):
    """One operation's result: a known FAIL stays FAIL; PASS needs every row PASS
    (REFUSED counts for UNSUPPORTED rows) and never widens a non-PASS inventory state."""
    verdicts = list(verdicts)
    if inventory_state == "FAIL" or "FAIL" in verdicts:
        return "FAIL"
    if verdicts and all(v in ("PASS", "REFUSED") for v in verdicts):
        return "PASS" if inventory_state in ("PASS", "NOT_RUN") else inventory_state
    if "BLOCKED" in verdicts:
        return "BLOCKED"
    return "NOT_RUN" if inventory_state != "PASS" or not verdicts else "PARTIAL"


def operations(matrix):
    grouped = {}
    for row in matrix:
        grouped.setdefault(row["id"], (row["inventory"], []))[1].append(row["verdict"])
    return {key: rollup(state, verdicts) for key, (state, verdicts) in grouped.items()}


def markdown(matrix):
    cell = lambda v: "" if v is None else (", ".join(map(str, v)) if isinstance(v, list) else str(v)).replace("|", "\\|")
    lines = ["| " + " | ".join(COLUMNS) + " |", "|" + " --- |" * len(COLUMNS)]
    lines += ["| " + " | ".join(cell(row[c]) for c in COLUMNS) + " |" for row in matrix]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--inventory", type=Path, default=INVENTORY)
    parser.add_argument("--cases", type=Path, help="JSON case map keyed by ID or ID:KEY")
    parser.add_argument("--markdown", type=Path, help="write the matrix table here")
    parser.add_argument("--json", type=Path, help="write rows and roll-ups here")
    parser.add_argument("--strict", action="store_true", help="require a case or exclusion on every field row")
    args = parser.parse_args(argv)
    matrix = rows(json.loads(args.inventory.read_text(encoding="utf-8")),
                  json.loads(args.cases.read_text(encoding="utf-8")) if args.cases else None)
    found = problems(matrix, args.strict)
    if args.markdown:
        args.markdown.write_text(markdown(matrix), encoding="utf-8")
    if args.json:
        args.json.write_text(json.dumps(dict(rows=matrix, operations=operations(matrix), problems=found), indent=1),
                             encoding="utf-8")
    print(f"{len(matrix)} rows, {len(operations(matrix))} operations, {len(found)} problems")
    for problem in found:
        print(problem)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
