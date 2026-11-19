"""Wired bench 17: communication changes and persistence, harness side.

During the owner's absence (D15) only S0 reaches the bench: a read-only live
snapshot of each drive with its field table. Every case that saves,
factory-restores, changes 0x0013-0x0015, needs a power cycle, a DIP switch or
the owner is refused here before any traffic with "deferred: owner away".

The pure helpers serve the owner-return session: the field table and its
token, the live/stored difference, the gate before a save, the address policy
for this bus, the host-side restart witness and the guard against a second
factory command in one run.
"""
from __future__ import annotations

import hashlib
import json
import re

import bench_probe as probe
import bench_records as records

LEDGER = records.ROOT / "docs" / "reference" / "ess_rs_registers.json"
IMAGE_LEVEL = "17"
DEFERRED = "deferred: owner away (D15); see the owner-return run plan in the prompt 17 report"
CASES = {
    "S0": {"owner": False, "levels": ("read back",),
           "assertion": "live snapshot of each scenario drive: every baseline field read fresh, the field table "
                        "(register, storage class, live value, restore route, readback) written with its SHA-256; "
                        "readable ledger words without a read route named"},
    "P0": {"owner": True, "levels": ("drive-reported", "controller-timestamped"),
           "assertion": "owner power-cycles drive 2; the controller stays up, drives 1/4 answer, drive 2 returns"},
    "S1": {"owner": True, "levels": ("read back",),
           "assertion": "stored snapshot after an owner power cycle with no write; live minus stored listed"},
    "S2": {"owner": True, "levels": ("read back",),
           "assertion": "two budgeted proof saves on drive 4; full stored image equals S1"},
    "P2": {"owner": True, "levels": ("read back",), "assertion": "unsaved writes revert after a power cycle"},
    "P3": {"owner": True, "levels": ("read back",), "assertion": "saved writes survive; VERIFIED_FIELDS"},
    "P4a": {"owner": True, "levels": ("read back", "drive-reported", "controller-timestamped"),
            "assertion": "saved stop function right after power-up"},
    "P4b": {"owner": True, "levels": ("read back", "sent"), "assertion": "mosfet2 held across power-up"},
    "P4c": {"owner": True, "levels": ("read back", "sent"), "assertion": "relay2 held across power-up"},
    "P5": {"owner": True, "levels": ("read back",), "assertion": "P2 fields restored, saved; stored image equals S1"},
    "F1": {"owner": True, "levels": ("read back",), "assertion": "factory restore; measured defaults table"},
    "F2": {"owner": True, "levels": ("read back",), "assertion": "restricted recommissioning back to S1"},
    "C1": {"owner": True, "levels": ("drive-reported",), "assertion": "baud 38400 without save, power cycle"},
    "C2": {"owner": True, "levels": ("drive-reported",), "assertion": "the eight-tuple chain and back"},
    "D1": {"owner": True, "levels": ("drive-reported",), "assertion": "drive 2 at custom address 20"},
    "D2": {"owner": True, "levels": ("read back",), "assertion": "drive 2 back at DIP 2, custom node as in S1"},
    "S3": {"owner": True, "levels": ("drive-reported",), "assertion": "discovery: 1, 2, 4 at 115200 8N1 only"},
}
# Bus check (12 November) and prompt 17: a drive that loses a frame's first byte
# reads the function byte as an address, so the function codes this application
# sends (3, 6, 16) and their exception codes (131, 134, 144, inferred) are never
# used as a drive address on this bus, nor 0 (broadcast).
FORBIDDEN_ADDRESSES = (0, 3, 6, 16, 131, 134, 144)
CUSTOM_ADDRESS = 20
C2_CHAIN = (("38400", "8N1"), ("38400", "8E1"), ("19200", "8E1"), ("19200", "8O1"), ("9600", "8O1"),
            ("9600", "8N2"), ("115200", "8N2"), ("115200", "8N1"))
MAX_SAVES_PER_DRIVE = 12
WITNESS_KEY = "jog.speed"
FACTORY_COMMAND = re.compile(r"\bpersistence begin factory-restore\b")
# 0x0013-0x0015 have no plain setter: prompt 17 routes them through sessions.
SESSION_ROUTES = {
    "config.custom-node": "communication session for a change; route back: `persistence node restore WORD SOURCE` "
                          "(item 8, D7) while the DIP switches set the address, then a save",
    "config.baud": "communication session (`communication begin baud VALUE`, host, confirm), save if C1 needs it, "
                   "owner power cycle",
    "config.format": "communication session (`communication begin format VALUE`, host, confirm), save if C1 needs "
                     "it, owner power cycle",
}


def ledger_words(path=LEDGER) -> dict:
    """``{register: (source access, name)}`` of every ledger word, indexed groups
    included; gaps are absent and never read."""
    data = json.loads(path.read_text(encoding="utf-8"))
    words = {}
    for entry in data["registers"]:
        base = int(entry["address"], 16)
        for word in range(entry["words"]):
            words[base + word] = (entry["source_access"], entry["name"])
    for group in data["indexed_groups"]:
        for instance in group["instances"]:
            base = int(instance["base"], 16)
            for template in group["templates"]:
                for word in range(template["words"]):
                    words[base + template["offset"] + word] = (template["source_access"],
                                                               "%s_%d_%s" % (group["name"], instance["index"], template["name"]))
    return words


def route_of(field: dict, image_level: str = IMAGE_LEVEL) -> dict:
    """Restore route of one baseline field at this image level, as the planner chooses it."""
    level = int(image_level)
    if field["key"] in SESSION_ROUTES:
        return {"kind": "session", "command": SESSION_ROUTES[field["key"]], "readback": field["read"]}
    route = field.get("restore") or (records.PAIR_ROUTES.get(field["key"]) if level >= 10 else None) or \
        (records.PT_TARGET_ROUTE if field["key"] == "records.position.target" and level >= 13 else None) or \
        (records.candidate_route(field["key"]) if level >= int(records.CANDIDATE_IMAGE_LEVEL) else None)
    if route is not None:
        return {"kind": "typed setter", "command": route["command"], "readback": route["readback"]}
    snapshot = records.snapshot_route(field["key"])
    if snapshot is not None:
        r = records.SNAPSHOT_ROUTES[snapshot]
        return {"kind": "D7 snapshot", "command": r["command"].replace("-", " ") + " SNAPSHOT_ID",
                "readback": r["read_command"].replace("-read", "")}
    return {"kind": "none", "command": None, "readback": field["read"], "reason": field.get("none_reason")}


def field_table(address: int, live_raws: dict, stored_raws: dict | None = None, *, ledger=None) -> list:
    """One row per baseline field: register, storage class (ledger), live and
    stored values, restore route and readback. ``stored`` stays None until S1
    reads the drive after an owner power cycle with no write since."""
    ledger = ledger_words() if ledger is None else ledger
    live = records.baseline_fields(address, live_raws)
    stored = records.baseline_fields(address, stored_raws) if stored_raws is not None else None
    rows = []
    for i, field in enumerate(live):
        register = int(field["register"], 16)
        route = route_of(field)
        rows.append({"drive": address, "key": field["key"], "index": field["index"], "register": field["register"],
                     "storage": ledger.get(register, ("gap", None))[0], "live": field["value"],
                     "stored": stored[i]["value"] if stored is not None else None,
                     "stored_known": stored is not None and stored[i]["value"] is not None,
                     "route": route["kind"], "command": route["command"], "readback": route["readback"],
                     "reason": route.get("reason")})
    return rows


def table_sha256(rows: list) -> str:
    text = json.dumps(rows, sort_keys=True, separators=(",", ":"), default=str)
    return hashlib.sha256(text.encode("utf-8")).hexdigest().upper()


def backup_token(sha256: str) -> str:
    """`backup TOKEN` of `persistence qualification install`: the table hash's first 32 bits."""
    if not re.fullmatch(r"[0-9A-Fa-f]{64}", sha256) or not int(sha256[:8], 16):
        raise ValueError("a field table SHA-256 with a nonzero first word is required")
    return sha256[:8].lower()


def unread_words(rows: list, ledger=None) -> list:
    """Readable ledger words that no field of the table covers (identity
    0x0000/4 and state 0x0006/7 are read too). Write-only words are not readable."""
    ledger = ledger_words() if ledger is None else ledger
    covered = set(range(0x0000, 0x0004)) | set(range(0x0006, 0x000D))
    for row in rows:
        base = int(row["register"], 16)
        covered.add(base)
        if isinstance(row["live"], list) and len(row["live"]) == 2:
            covered.add(base + 1)
    return ["0x%04X %s %s" % (reg, *ledger[reg]) for reg in sorted(ledger) if ledger[reg][0] != "WO" and reg not in covered]


def differences(rows: list) -> list:
    """Live minus stored, per field; unknown when either side was not read."""
    out = []
    for row in rows:
        if not row["stored_known"] or row["live"] is None:
            out.append({**_ident(row), "status": "unknown", "live": row["live"], "stored": row["stored"]})
        elif row["live"] != row["stored"]:
            out.append({**_ident(row), "status": "ram_only", "live": row["live"], "stored": row["stored"],
                        "route": row["route"], "command": row["command"]})
    return out


def _ident(row):
    return {"drive": row["drive"], "key": row["key"], "index": row["index"], "register": row["register"]}


def save_gate(rows: list, named: tuple = ()) -> list:
    """Reasons that stop a save: the saveable RAM image must equal the stored
    image plus only the case's named changes ((key, index) pairs)."""
    blockers = []
    for diff in differences(rows):
        if diff["status"] == "unknown":
            blockers.append("%s%s: stored value unknown" % (diff["key"], _index(diff)))
        elif (diff["key"], diff["index"]) not in named:
            blockers.append("%s%s: RAM %s differs from stored %s" % (diff["key"], _index(diff), diff["live"], diff["stored"]))
    return blockers


def factory_gate(rows: list) -> list:
    """Fields without a restore route: each blocks a factory restore of the drive."""
    return ["%s%s: %s" % (r["key"], _index(r), r["reason"] or "no route") for r in rows if r["route"] == "none"]


def _index(row):
    return "[%d]" % row["index"] if row.get("index") else ""


def address_problem(address: int, in_use=(1, 2, 4)) -> str | None:
    """Harness policy for an address change on this bus (the core keeps 1..247)."""
    if type(address) is not int or not 1 <= address <= 247:
        return "outside 1..247"
    if address in FORBIDDEN_ADDRESSES:
        return "function or exception code of this application (lost-first-byte hazard)"
    if address in in_use:
        return "address in use on this bus"
    return None


def witness_plan(stored_word: int, maximum: int = probe.VELOCITY_LEGACY_MAX_RPM) -> dict:
    """Host-side restart witness (wired bench 17 after the audit): the
    application admits no restart fact, so a restart is proven from ordinary
    reads only. After `persistence finish`, prompt 14's JOG setter writes a
    speed distinct from the drive's stored JOG word (its S1 value), unsaved;
    after the owner power cycle `velocity read` must return the stored word.
    Without the restart, `velocity restore` puts the stored words back. The
    evidence is labelled host-only."""
    speed = stored_word if stored_word < 0x8000 else stored_word - 0x10000
    marker = 7 if speed != 7 else 8
    if not 1 <= marker <= maximum:
        raise ValueError("no witness speed inside the input velocity limit")
    return {"key": WITNESS_KEY, "register": "0x001D", "stored": stored_word, "marker": marker,
            "steps": ["persistence finish (the save session releases its lease)",
                      "velocity read (snapshot for the restore)", "velocity jog set %d ACCEL DECEL" % marker,
                      "velocity read: expect %d" % marker, "owner power cycle of this drive",
                      "velocity read: expect the stored word %d" % stored_word],
            "no_restart": ["velocity restore SNAPSHOT_ID", "velocity read"]}


def witness_verdict(before_cycle: int, after_cycle: int, marker: int, stored: int) -> str:
    """witnessed (host-only), no_restart, or contradicted from the two JOG reads."""
    if before_cycle != marker:
        return "contradicted"
    if after_cycle == stored and stored != marker:
        return "witnessed"
    return "no_restart" if after_cycle == marker else "contradicted"


def factory_guard(lines) -> str | None:
    """A run log that already holds a factory restore command refuses another."""
    for line in lines:
        if FACTORY_COMMAND.search(line):
            return "this run already sent a factory restore command; a second one is refused"
    return None


def plan(args, baseline, facts_):
    """Scenario-record fields: S0 is read-only; owner cases are refused before TX."""
    drives = list(args.drives or records.FIXTURE["input_drives"])
    cases = [{"id": c, "required": True, "assertion": CASES[c]["assertion"], "required_levels": list(CASES[c]["levels"])}
             for c in args.cases]
    commands = ["S0 per drive: useaddr A; every baseline read (identity, config, state, settings groups, 48 records, "
                "home, velocity, motion-profile read/forget); field table and SHA-256 (no write)"]
    commands += ["%s: refused before TX, %s" % (c, DEFERRED) for c in args.cases if CASES[c]["owner"]]
    bounds = {"drives": drives, "writes": 0, "saves": 0, "factory_restores": 0, "communication_writes": 0,
              "max_saves_per_drive": MAX_SAVES_PER_DRIVE, "custom_address": CUSTOM_ADDRESS,
              "forbidden_addresses": list(FORBIDDEN_ADDRESSES)}
    expected = {"read back": "s0_drive_A.json per drive: field table, SHA-256, backup token, unread words, "
                             "differences against the committed baseline"}
    restoration = {"admissible": True, "steps": [], "refused": []}
    return [], restoration, cases, commands, bounds, expected, []


def run_cases(console, run, baseline, args, timeout) -> dict:
    """S0 on every scenario drive; owner cases recorded NOT RUN without traffic."""
    detail = {"cases": {}}
    for cid in args.cases:
        if CASES[cid]["owner"]:
            for level in CASES[cid]["levels"]:
                run.verdict(cid, level, "NOT RUN", DEFERRED)
            detail["cases"][cid] = {"verdict": "NOT RUN", "reason": DEFERRED, "traffic": False}
    if "S0" not in args.cases:
        return detail
    ledger = ledger_words()
    tables, problems = {}, []
    for drive in run.scenario["drives"]:
        address = drive["address"]
        reads = records.read_drive(console, address, records.ALL_READS, timeout_s=timeout)
        raws = records.raws_from(reads)
        rows = field_table(address, raws, ledger=ledger)
        sha = table_sha256(rows)
        missing = [r["key"] + _index(r) for r in rows if r["live"] is None]
        diff = records.diff_fields(baseline, {address: raws})
        record = {"drive": address, "rows": rows, "sha256": sha, "backup_token": backup_token(sha),
                  "unread_words": unread_words(rows, ledger), "missing": missing,
                  "factory_gate": factory_gate(rows),
                  "baseline_diff": [d for d in diff if d["status"] != "equal"],
                  "reads": {k: {x: e.get(x) for x in ("command", "operation_id", "error")} for k, e in reads.items()}}
        run.folder.write_json("s0_drive%d.json" % address, record)
        tables[address] = {k: record[k] for k in ("sha256", "backup_token", "unread_words", "missing")}
        tables[address].update(rows=len(rows), no_route=len(record["factory_gate"]),
                               baseline_differs=len(record["baseline_diff"]))
        if missing:
            problems.append("drive %d: %d fields unread (%s)" % (address, len(missing), ", ".join(missing[:5])))
    detail["cases"]["S0"] = tables
    evidence = ["s0_drive%d.json" % d["address"] for d in run.scenario["drives"]]
    unread = sorted({w for t in tables.values() for w in t["unread_words"]})
    note = "readable ledger words without a read route: " + ", ".join(unread) if unread else None
    run.verdict("S0", "read back", "FAIL" if problems else "PASS",
                "; ".join(problems + ([note] if note else [])) or None, evidence)
    return detail
