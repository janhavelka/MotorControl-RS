#!/usr/bin/env python3
"""Wired-bench run records: fixture, shared-output interlock, run folders,
results, run checks, baseline comparison and restoration, edge correlation.

Example bench tooling (wired bench prompt 04), not library API. It drives the
ordinary console through ``bench_probe.Console`` and adds no motor semantics:
every write goes through an existing setter with the facts the scenario record
states, and nothing is retried or replayed. ``python scripts/bench_records.py
check FOLDER`` checks a run folder offline and never edits it.
"""
from __future__ import annotations

import argparse
import copy
import time
from datetime import date as _date
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

import bench_probe as probe
from _bench_evidence import BenchError, invalid_constant, unique_object

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RUN_ROOT = ROOT / "build" / "bench"
DEFAULT_BASELINE = ROOT / "docs" / "reports" / "wired_bench_baseline.json"

SCENARIO_SCHEMA = "motorcontrol-rs/wired-bench-scenario/1"
RESULT_SCHEMA = "motorcontrol-rs/wired-bench-result/1"
BASELINE_SCHEMA = "motorcontrol-rs/wired-bench-baseline/1"
VERDICTS = ("PASS", "FAIL", "NOT RUN", "BLOCKED", "indeterminate")
NEEDS_REASON = ("NOT RUN", "BLOCKED", "indeterminate")
LEVELS = ("local", "sent", "acknowledged", "read back", "drive-reported",
          "controller-timestamped", "independently observed")
STORAGE = ("RAM", "saved")

# The wired bench fixture (COMMON Bench rules, bench_io.md). Output -> drive
# terminal as wired on 18 November (relays swapped against the plan, D2).
FIXTURE = {
    "board": {"port": "COM13", "usb": "303A:1001", "serial": "3C:0F:02:CD:6B:98"},
    "outputs": {"mosfet1": "x0", "relay2": "x1", "relay1": "x2", "mosfet2": "x3"},
    "input_drives": [1, 2, 4],
    "y0_owner": 4,
    "aux1": {"sinking": True, "inverted": True},
    "edge_kind": {"mosfet1": "clean", "mosfet2": "clean", "relay1": "bounce", "relay2": "bounce"},
}
TERMINALS = ("x0", "x1", "x2", "x3")
CONNECTED = probe.WIRING_CONNECTED
UNCONNECTED = 1
# Interlock freshness (decision D14, 19 November). Ordering is the primary
# guard: the interlock's reads are the last bus traffic before the ON, an output
# already on refuses, and any accepted operation still pending other than the
# named running member's refuses. The read age is a backstop: each read's age
# runs from its request start on the controller clock to the bench view taken
# just before the ON, plus the host time after that view; the post-ON check
# repeats it from ``on_us``. 300 ms was measured unreachable through the
# console (worst 424 ms). Prompt 08's firmware trigger applies the same rule
# and bound to its own read times.
MAX_AGE_MS = 1000
# Intended effects a named running member may receive (truth table row L3).
EFFECTS = {"limit": (2, 3), "release": (4,), "stop": (5, 6), "fixed-length": (7,),
           "selection": (14, 15, 16, 17)}
# Wired bench 07: a running member's own operation reserves its axis, so the
# firmware refuses its `io read` (axis_conflict, measured). Row L3 then takes
# the member's I/O words from its last checked `io read` before the operation
# was admitted, with only read operations in between: from that read to the ON
# every settings write to the axis is refused by the reservation, and a drive
# restart cannot resume the operation (the fresh `read state` must still show
# it running). This bound replaces the 1000 ms backstop for those words only.
NAMED_IO_MAX_AGE_MS = 10000
# Operations the console may admit between that io read and the named
# operation: checked reads (an io entry must be marked a read) and the motion
# of the case itself (marked motion), none of which changes an I/O assignment.
BETWEEN_READS = ("read-config", "read-state", "velocity-read", "home-read", "read-identity", "group-read-state")
BETWEEN_MOTION = ("move-relative", "move-absolute", "velocity", "home", "group-move", "stop", "group-stop", "enable",
                  "motor-release", "alarm-clear")
# Per ON and participant: the nine I/O words (polarity word 0, X0-X3 functions
# words 1-4), then the state, so the motion/alarm sample is the newest. The
# agreement of the I/O words with ``read config`` is a session-level check
# (``check_input_agreement``), outside the age window.
INTERLOCK_READS = ("io", "read-state")


class InterlockRefused(BenchError):
    """The shared-output interlock refused; no ``bench on`` was sent."""

    def __init__(self, entry: dict):
        super().__init__("interlock refused %s: %s" % (entry.get("output"), entry.get("reason")))
        self.entry = entry


class RecordError(ValueError):
    """A scenario record, result or baseline does not meet its schema."""


def _tuples(kwargs: dict) -> dict:
    """Console arguments after a JSON round trip: token lists back to tuples."""
    return {k: tuple(v) if isinstance(v, list) else v for k, v in (kwargs or {}).items()}


def call(console, spec: dict, *, timeout_s=5.0) -> dict:
    """Send one planned console call ``{command, kwargs}``."""
    return console.command(spec["command"], timeout_s=timeout_s, **_tuples(spec.get("kwargs")))


def _ok(console, name, *, timeout_s=5.0, **kwargs):
    result = console.command(name, timeout_s=timeout_s, **kwargs)
    if not result.get("ok"):
        raise BenchError("%s refused/failed: %s" % (name, result.get("result", result.get("outcome"))))
    return result


def _blocks(state: dict) -> dict:
    return {block.get("block"): block for block in state.get("state_blocks", []) if isinstance(block, dict)}


def _load_json(path: Path):
    with Path(path).open("rb") as stream:
        return json.loads(stream.read().decode("utf-8"), object_pairs_hook=unique_object,
                          parse_constant=invalid_constant)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def image_record(path) -> dict:
    """Image path relative to the repository when possible, and its SHA-256."""
    path = Path(path)
    resolved = path.resolve()
    try:
        shown = resolved.relative_to(ROOT).as_posix()
    except ValueError:
        shown = str(resolved)
    return {"path": shown, "sha256": sha256_file(resolved)}


def source_commit(root: Path = ROOT) -> str:
    """HEAD, plus the working-tree diff hash when tracked files changed."""
    def git(*args):
        return subprocess.run(["git", *args], cwd=root, capture_output=True, check=True).stdout
    head = git("rev-parse", "HEAD").decode().strip()
    diff = git("diff", "--binary", "HEAD")
    untracked = git("ls-files", "--others", "--exclude-standard").decode().split()
    if not diff and not untracked:
        return head
    digest = hashlib.sha256(diff)
    for name in sorted(untracked):
        if name.startswith("build/"):
            continue
        digest.update(name.encode())
        try:
            digest.update((root / name).read_bytes())
        except OSError:
            pass
    return "%s + uncommitted working tree (sha256 %s)" % (head, digest.hexdigest().upper())


def check_board(port: str = FIXTURE["board"]["port"]) -> dict:
    """USB identity of the recorded bench board before the port is opened."""
    from serial.tools import list_ports
    board = FIXTURE["board"]
    info = {p.device: p for p in list_ports.comports()}.get(port)
    vid, pid = (int(v, 16) for v in board["usb"].split(":"))
    if (port != board["port"] or info is None or (info.vid, info.pid) != (vid, pid) or
            (info.serial_number or "").upper() != board["serial"]):
        raise BenchError("%s is not the recorded bench board (%s)" % (port, info and info.hwid))
    return {"port": port, "hwid": info.hwid, "serial": info.serial_number}


# --------------------------------------------------------------------------
# Selection and board declarations

def use_drive(console, address: int, *, timeout_s=5.0) -> tuple[dict, bool]:
    """Select ``address`` locally (``useaddr``); return the new ``wiring`` reply.

    Refuses while another drive's profile snapshot still needs restoring
    (the same rule as ``bench_motion.select_drive``). Sends no bus traffic.
    """
    wiring = _ok(console, "wiring", timeout_s=timeout_s)
    if wiring.get("address") == address:
        return wiring, False
    profile = _ok(console, "motion-profile", host_args=("inspect",), timeout_s=timeout_s)
    if profile.get("pending") or profile.get("restore_unsettled"):
        raise BenchError("profile snapshot is pending or unsettled; no target change")
    if profile.get("saved"):
        if not (profile.get("restored") or profile["original"][1:] == profile["current"][1:]):
            raise BenchError("another drive has an unrestored profile snapshot; restore it first")
        _ok(console, "motion-profile", host_args=("forget",), timeout_s=timeout_s)
    _ok(console, "useaddr", address=address, timeout_s=timeout_s)
    return _ok(console, "wiring", timeout_s=timeout_s), True


def expected_bench_outputs() -> list:
    """FIXTURE outputs as the firmware's map: terminal index per output, BenchIo order."""
    return [TERMINALS.index(FIXTURE["outputs"][name]) for name in probe.BENCH_OUTPUTS]


def _expected_board(address: int) -> dict:
    outputs = [CONNECTED if address == FIXTURE["y0_owner"] else UNCONNECTED, UNCONNECTED]
    return {"inputs": [CONNECTED] * 4, "outputs": outputs}


def check_declarations(console, *, timeout_s=5.0) -> dict:
    """Verdict ``declaration_agreement``: 02's board table against FIXTURE.

    Selects each input drive locally, reads ``wiring`` and compares its board
    masks and sources with X0-X3 connected on every input drive and Y0
    connected on the Y0 owner only, and the board's bench output -> terminal
    map (wired bench 08) with ``FIXTURE["outputs"]``. An absent or invalid
    table or map, or a reply without the board keys (an image older than
    wired bench 02) or the map (older than wired bench 08), fails. The
    original selection is restored. Status bits are not consulted: this says
    what the board declares, not what is physically wired.
    """
    rows, problems = [], []
    original = _ok(console, "wiring", timeout_s=timeout_s)
    selected = original.get("address")
    try:
        for address in FIXTURE["input_drives"]:
            reply, changed = use_drive(console, address, timeout_s=timeout_s)
            selected = reply.get("address", selected)
            expected = _expected_board(address)
            row = {"address": address, "selected_by_useaddr": changed, "expected": expected,
                   "fixture": reply.get("fixture"), "board": reply.get("board"),
                   "sources": reply.get("sources"), "inputs": reply.get("inputs"), "outputs": reply.get("outputs")}
            row["bench_outputs"] = reply.get("bench_outputs")
            if any(key not in reply for key in ("board", "sources", "fixture")):
                row["problem"] = "reply lacks board/sources/fixture (image older than the board fixture table)"
            elif "bench_outputs" not in reply:
                row["problem"] = "reply lacks bench_outputs (image older than the wired bench 08 output map)"
            elif reply.get("bench_output_map") != "applied" or reply["bench_outputs"] != expected_bench_outputs():
                row["problem"] = "bench output map %s %s differs from FIXTURE %s" % (
                    reply.get("bench_output_map"), reply["bench_outputs"], expected_bench_outputs())
            elif reply["fixture"] != "applied":
                row["problem"] = "fixture table %s%s" % (reply["fixture"], " (%s)" % reply["fixture_error"]
                                                        if reply.get("fixture_error") else "")
            elif reply["board"] != expected:
                row["problem"] = "board declaration differs from FIXTURE"
            else:
                declared = (reply.get("inputs") or []) + (reply.get("outputs") or [])
                board = expected["inputs"] + expected["outputs"]
                sources = reply.get("sources") or []
                if len(sources) != 6 or any(s in ("default", "invalid_table") for s in sources):
                    row["problem"] = "declaration sources are not board or host"
                elif declared != board:
                    row["problem"] = "session declarations differ from the board table (host override)"
            row["verdict"] = "FAIL" if "problem" in row else "PASS"
            if "problem" in row:
                problems.append("drive %d: %s" % (address, row["problem"]))
            rows.append(row)
    finally:
        if getattr(console, "synchronized", True) and selected != original.get("address"):
            try:
                use_drive(console, original["address"], timeout_s=timeout_s)
            except BenchError as error:
                problems.append("original selection not restored: %s" % error)
    return {"verdict": "FAIL" if problems else "PASS", "rows": rows, "problems": problems}


# --------------------------------------------------------------------------
# Line-isolation truth table (shared with prompt 08's firmware trigger)

def named_io_problem(named: dict) -> str | None:
    """Why a named running member's pre-start ``io read`` cannot stand in for a
    fresh one (wired bench 07, row L3), or None. ``named['io_read']`` is that
    read's reply summary; ``named['between']`` lists every operation the
    console admitted after it and before the named one."""
    read = named.get("io_read")
    if not isinstance(read, dict) or not read.get("ok"):
        return "no checked io read before the named operation"
    words = read.get("raw")
    if not (isinstance(words, list) and len(words) == 9 and all(type(w) is int for w in words)):
        return "pre-start io read lacks nine words"
    first, last = read.get("operation_id"), named.get("operation_id")
    if type(first) is not int or type(last) is not int or not first < last:
        return "pre-start io read is not before the named operation"
    between = named.get("between")
    if not isinstance(between, list):
        return "operations between the io read and the named operation are not recorded"
    ids = [b.get("operation_id") for b in between]
    if ids != list(range(first + 1, last)):
        return "operations %s between the io read %s and the named operation %s are not all recorded" % (
            ids, first, last)
    def allowed(b):
        command = b.get("command")
        return ((command in BETWEEN_READS and not b.get("motion")) or (command == "io" and b.get("read") is True) or
                (command in BETWEEN_MOTION and b.get("motion") is True))
    writes = [b for b in between if not allowed(b)]
    if writes:
        return "an operation that may change assignments came between the io read and the named operation: %s" % writes[0]
    return None


def participant_facts(address: int, reads: dict, now_us: int | None, extra_ms: float = 0.0,
                      named: dict | None = None) -> dict:
    """Decision inputs of one participant from its io read and read state.

    Ages run from each read's ``started_us`` (request start) to ``now_us``
    (the controller time of the bench view before the ON) plus ``extra_ms``.
    A named running member (row L3) uses its pre-start io read, bounded by
    ``NAMED_IO_MAX_AGE_MS`` (wired bench 07).
    """
    facts = {"address": address, "unknown": [], "ages_ms": {}, "operation_ids": {}}
    if named is not None and reads.get("io") is None:
        problem = named_io_problem(named)
        if problem:
            facts["unknown"].append(problem)
            return facts
        read = named["io_read"]
        reads = dict(reads, io={"ok": True, "operation_id": read["operation_id"], "started_us": read["started_us"],
                                "observation": {"raw": list(read["raw"]), "unknown_input_polarity_bits": 0}})
        facts["age_bounds_ms"] = {"io": NAMED_IO_MAX_AGE_MS}
        facts["io_source"] = "pre-start io read %s" % read["operation_id"]
    missing = [name for name in INTERLOCK_READS if reads.get(name) is None]
    if missing:
        facts["unknown"].append("missing " + ", ".join(missing))
        return facts
    for name in INTERLOCK_READS:
        result = reads[name]
        facts["operation_ids"][name] = result.get("operation_id")
        if not result.get("ok"):
            facts["unknown"].append("%s failed: %s" % (name, result.get("result", result.get("outcome"))))
            continue
        started = result.get("started_us")
        if type(started) is not int or type(now_us) is not int or started > now_us:
            facts["unknown"].append(name + " has no usable sample time")
        else:
            facts["ages_ms"][name] = round((now_us - started) / 1000.0 + extra_ms, 3)
    if facts["unknown"]:
        return facts
    observation = reads["io"].get("observation") or {}
    words = observation.get("raw")
    if not (isinstance(words, list) and len(words) == 9 and all(type(w) is int for w in words)):
        facts["unknown"].append("io read lacks nine words")
        return facts
    if observation.get("unknown_input_polarity_bits"):
        facts["unknown"].append("input polarity bits unknown")
    if any(f > 17 for f in words[1:5]):
        facts["unknown"].append("input function outside 0..17")
    blocks = _blocks(reads["read-state"])
    motion, status = blocks.get(0), blocks.get(1)
    if motion is None or status is None:
        facts["unknown"].append("read state lacks motion or I/O block")
    else:
        if motion.get("alarm_known") is not True or motion.get("unknown_motion_bits") != 0:
            facts["unknown"].append("unknown motion bits")
        if status.get("unknown_input_bits") != 0 or status.get("unknown_output_bits") != 0:
            facts["unknown"].append("unknown input/output bits")
        if (type(motion.get("running")) is not bool or type(motion.get("alarm_flag")) is not bool or
                type(motion.get("raw_alarm")) is not int):
            facts["unknown"].append("motion flags missing")
        else:
            facts.update(running=motion["running"], raw_alarm=motion["raw_alarm"],
                         alarm_flag=motion["alarm_flag"], raw_inputs=status.get("raw_inputs"))
    facts.update(functions=list(words[1:5]), polarity=words[0])
    return facts


def _named_mismatch(named: dict, output: str, terminal: str, function: int, polarity: int, pending) -> str | None:
    """First difference between a named running member and the fresh reads, or None."""
    if named.get("output") != output:
        return "named line %s, switched %s" % (named.get("output"), output)
    if named.get("terminal") != terminal:
        return "named terminal %s, switched %s" % (named.get("terminal"), terminal)
    effect = named.get("effect")
    if effect not in EFFECTS or named.get("function") not in EFFECTS[effect]:
        return "named function %s does not have effect %s" % (named.get("function"), effect)
    if function != named["function"]:
        return "terminal reads function %d, named %d" % (function, named["function"])
    if polarity != named.get("polarity"):
        return "terminal reads polarity %d, named %s" % (polarity, named.get("polarity"))
    console_id = named.get("console_operation_id", named.get("operation_id"))
    console_address = named.get("console_address", named.get("address"))
    if not any(p.get("operation_id") == console_id and p.get("address") == console_address for p in pending):
        return "named operation %s is not the pending operation" % console_id
    return None


def decide_participant(facts: dict, output: str, *, max_age_ms: float = MAX_AGE_MS,
                       named: dict | None = None, pending=()) -> dict:
    """First matching row L1-L7 for one participant (prompt 04 truth table)."""
    terminal = FIXTURE["outputs"][output]
    index = TERMINALS.index(terminal)

    def decision(row, admit, reason):
        return {"address": facts.get("address"), "row": row, "decision": "admit" if admit else "refuse",
                "reason": reason}
    if facts.get("silent"):
        return decision("L1", False, "silent")
    if facts.get("unknown"):
        return decision("L1", False, "; ".join(facts["unknown"]))
    bounds = facts.get("age_bounds_ms") or {}
    old = sorted(name for name, age in facts.get("ages_ms", {}).items() if age > bounds.get(name, max_age_ms))
    if old or len(facts.get("ages_ms", {})) != len(INTERLOCK_READS):
        return decision("L1", False, "read older than %s ms: %s" % (max_age_ms, ", ".join(old) or "age unknown"))
    if facts["raw_alarm"] or facts["alarm_flag"]:
        return decision("L2", False, "alarm %d flag %s" % (facts["raw_alarm"], facts["alarm_flag"]))
    function = facts["functions"][index]
    polarity = (facts["polarity"] >> index) & 1
    if isinstance(named, list):
        named = next((m for m in named if m.get("address") == facts.get("address")), None)
    if facts.get("io_source"):
        # Wired bench 07: the member's own pending operation reserves the axis.
        if named is None or named.get("address") != facts.get("address"):
            return decision("L4", False, "pre-start io words admitted only for a named member")
        mismatch = _named_mismatch(named, output, terminal, function, polarity, pending)
        if mismatch is not None:
            return decision("L4", False, "named member mismatch: " + mismatch)
        if facts["running"]:
            return decision("L3", True, "named running member, effect %s (pre-start io read)" % named["effect"])
        # A named member stopping under its own operation: the ordinary rows decide.
    elif named is not None and named.get("address") == facts.get("address") and facts["running"]:
        mismatch = _named_mismatch(named, output, terminal, function, polarity, pending)
        if mismatch is None:
            return decision("L3", True, "named running member, effect %s" % named["effect"])
        return decision("L4", False, "running named member mismatch: " + mismatch)
    if facts["running"]:
        return decision("L4", False, "running and not a named member")
    if 7 <= function <= 13:
        return decision("L5", False, "%s holds function %d" % (terminal, function))
    if 14 <= function <= 17 and any(f in (12, 13) for f in facts["functions"]):
        return decision("L6", False, "%s holds selector %d with a 12/13 trigger on the drive" % (terminal, function))
    return decision("L7", True, "%s holds function %d" % (terminal, function))


def _members(named) -> list:
    """Named running members as a list (one dict, a list of them, or None)."""
    if named is None:
        return []
    return list(named) if isinstance(named, list) else [named]


def _foreign_pending(pending, named) -> list:
    """Accepted operations still pending that are not a named running member's.
    A group operation names its console operation through ``console_operation_id``."""
    members = _members(named)

    def own(p):
        return any(p.get("operation_id") == m.get("console_operation_id", m.get("operation_id")) and
                   p.get("address") == m.get("console_address", m.get("address")) for m in members)
    return [p for p in pending if not own(p)]


def isolation_decision(output: str, participants: dict, *, max_age_ms: float = MAX_AGE_MS,
                       named: dict | None = None, pending=()) -> dict:
    """Admit only when every participant on the line is admitted.

    Row L4 also covers the line: any accepted operation still pending on the
    console other than the named running member's refuses every participant
    (D14), whatever drive it addresses.
    """
    if output not in FIXTURE["outputs"]:
        raise ValueError("unknown bench output")
    if not participants:
        return {"decision": "refuse", "row": "L1", "address": None, "reason": "no participant on the line",
                "participants": []}
    members = _members(named)
    rows = [decide_participant(participants[a], output, max_age_ms=max_age_ms,
                               named=next((m for m in members if m.get("address") == a), None), pending=pending)
            for a in sorted(participants)]
    foreign = _foreign_pending(pending, named)
    if foreign:
        first = foreign[0]
        return {"decision": "refuse", "row": "L4", "address": first.get("address"),
                "reason": "operation %s on drive %s is still pending and is not the named member's"
                          % (first.get("operation_id"), first.get("address")), "participants": rows}
    refused = next((r for r in rows if r["decision"] == "refuse"), None)
    first = refused or rows[0]
    return {"decision": "refuse" if refused else "admit", "row": first["row"], "address": first["address"],
            "reason": first["reason"] if refused else "every participant admitted", "participants": rows}


def case_participants(case: dict) -> dict:
    """Facts from one entry of ``test/line_isolation_cases.json``."""
    out = {}
    for key, drive in case["drives"].items():
        address = int(key)
        if drive.get("silent"):
            out[address] = {"address": address, "silent": True, "unknown": ["silent"], "ages_ms": {}}
            continue
        age = drive.get("age_ms", 0)
        out[address] = {"address": address, "unknown": [], "ages_ms": {name: age for name in INTERLOCK_READS},
                        "functions": list(drive["io"][1:5]), "polarity": drive["io"][0],
                        "running": drive.get("running", False), "raw_alarm": drive.get("raw_alarm", 0),
                        "alarm_flag": drive.get("alarm_flag", False)}
    return out


def pending_operations(console) -> list:
    """Accepted operations still running on this console (no terminal yet)."""
    out = []
    for handle in getattr(console, "pending", {}).values():
        if getattr(handle, "terminal", None) is None and getattr(handle, "accepted", False) and \
                getattr(handle, "operation_id", None):
            out.append({"operation_id": handle.operation_id, "address": handle.address, "command": handle.command})
    return out


def _bench_view(reply: dict) -> dict:
    return {"now_us": reply.get("now_us"),
            "outputs": {o.get("name"): {k: o.get(k) for k in ("active", "ons", "on_us", "off_us", "hold_ms", "off_reason")}
                        for o in reply.get("outputs", [])},
            "input": {k: (reply.get("input") or {}).get(k) for k in ("active", "edges", "overwritten", "recent")}}


def interlock(console, output: str, scenario: dict, *, timeout_s=5.0) -> dict:
    """Shared-output interlock: decide one ON of ``output`` (truth table L1-L7).

    Reads ``io read`` then ``read state`` of every drive the board table
    declares connected on the switched terminal, then one ``bench`` view
    (controller time, outputs). Declaration agreement and the session's
    input agreement (``check_input_agreement``, run once and kept in
    ``scenario``) must pass; an output already on refuses. ``scenario``
    supplies ``max_age_ms`` (default 300) and ``running_members``; the
    decision entry is returned, never cached. ``read_last`` (an address)
    reads that participant last, so its state sample is the newest before the
    ON (wired bench 05 timing). Sends no ``bench on``.
    """
    if output not in FIXTURE["outputs"]:
        raise ValueError("unknown bench output")
    terminal = FIXTURE["outputs"][output]
    index = TERMINALS.index(terminal)
    max_age_ms = scenario.get("max_age_ms", MAX_AGE_MS)
    members = [m for m in scenario.get("running_members", []) if m.get("output") == output]
    named = members[0] if len(members) == 1 else (members or None)
    entry = {"output": output, "terminal": terminal, "max_age_ms": max_age_ms, "named": named,
             "decision": "refuse", "row": None, "reason": None, "reads": {}}
    declarations = scenario.get("declarations")
    if declarations is None:
        declarations = scenario["declarations"] = check_declarations(console, timeout_s=timeout_s)
    entry["declaration_agreement"] = declarations["verdict"]
    if declarations["verdict"] != "PASS":
        entry.update(row="L1", reason="declaration agreement failed: " + "; ".join(declarations["problems"]))
        return entry
    participants = [row["address"] for row in declarations["rows"] if (row.get("board") or {}).get("inputs", [0] * 4)[index] == CONNECTED]
    last = scenario.get("read_last")
    if last in participants:  # The observed drive's sample is then the newest before the ON.
        participants = [a for a in participants if a != last] + [last]
    agreement = scenario.get("input_agreement")
    if agreement is None:
        agreement = scenario["input_agreement"] = check_input_agreement(console, timeout_s=timeout_s)
    entry["input_agreement"] = agreement["verdict"]
    if agreement["verdict"] != "PASS":
        entry.update(row="L1", reason="io read and read config disagree: " + "; ".join(agreement["problems"]))
        return entry
    reads = {}
    sent = []
    command = console.command
    own = "command" in vars(console)

    def tracked(name, **kwargs):
        sent.append(name)
        return command(name, **kwargs)
    console.command = tracked  # Counts every send from here to the view, releases included.
    try:
        for address in participants:
            reads[address] = {}
            member = next((m for m in members if m.get("address") == address and "io_read" in m), None)
            for name in INTERLOCK_READS:
                if name == "io" and member is not None:
                    continue  # Its own operation reserves the axis: the pre-start io read stands (row L3).
                kwargs = {"driver_args": ("read",)} if name == "io" else {}
                reads[address][name] = console.command(name, address=address, timeout_s=timeout_s, **kwargs)
            entry["reads"][address] = {name: {k: r.get(k) for k in ("ok", "operation_id", "started_us", "serviced_us")}
                                       for name, r in reads[address].items()}
        drv = console.command("drv", timeout_s=timeout_s)
        view = console.command("bench", host_args=(), timeout_s=timeout_s)
    finally:
        if own:
            console.command = command
        else:
            del console.command
    marked = console.clock()
    entry["samples"] = {a: io_sample(a, reads[a]["read-state"]) for a in participants}
    entry["ordering"] = ordering = interlock_ordering(participants, reads, sent, drv, named)
    entry["named_io"] = {m["address"]: {"operation_id": m["io_read"].get("operation_id"),
                                        "started_us": m["io_read"].get("started_us"), "raw": m["io_read"].get("raw"),
                                        "problem": named_io_problem(m)}
                         for m in members if "io_read" in m}
    entry["bench_before"] = _bench_view(view)
    if not view.get("ok"):
        entry.update(row="L1", reason="bench view refused: %s" % view.get("result"))
        return entry
    active = [name for name, o in entry["bench_before"]["outputs"].items() if o.get("active")]
    if ordering["problems"]:
        entry.update(row="L1", reason="interlock reads are not the last bus traffic: " + "; ".join(ordering["problems"]))
        return entry
    pending = pending_operations(console)
    entry["pending"] = pending
    extra_ms = (console.clock() - marked) * 1000.0
    facts = {a: participant_facts(a, reads[a], view.get("now_us"), extra_ms,
                                  named=next((m for m in members if m.get("address") == a and "io_read" in m), None))
             for a in participants}
    entry["facts"] = facts
    result = isolation_decision(output, facts, max_age_ms=max_age_ms, named=named, pending=pending)
    entry.update(decision=result["decision"], row=result["row"], reason=result["reason"],
                 address=result["address"], participants=result["participants"])
    if active:
        entry.update(decision="refuse", row=None, reason="output already on: " + ", ".join(active))
    return entry


def interlock_ordering(participants, reads: dict, sent: list, drv: dict, named: dict | None) -> dict:
    """D14 ordering: between the first interlock read and the ON the console
    sent only the interlock's reads (with their releases), one local ``drv`` and
    one ``bench`` view; the reads got consecutive controller operation IDs and
    ``drv`` reports the last of them as the latest admitted operation, with no
    owner request pending (unless a named running member's operation runs)."""
    problems = []
    expected = [name for a in participants for name in INTERLOCK_READS if name in reads[a]] + ["drv", "bench"]
    others = [name for name in sent if name != "release"]
    if others != expected:
        problems.append("commands sent %s, expected %s" % (others, expected))
    ids = [reads[a][name].get("operation_id") for a in participants for name in INTERLOCK_READS if name in reads[a]]
    if not all(type(i) is int and i > 0 for i in ids) or any(b != a + 1 for a, b in zip(ids, ids[1:])):
        problems.append("read operation IDs %s are not consecutive" % ids)
    if not drv.get("ok") or (ids and drv.get("operation_id") != ids[-1]):
        problems.append("latest admitted operation %s, last interlock read %s" % (drv.get("operation_id"),
                                                                                  ids[-1] if ids else None))
    if not _members(named) and drv.get("pending"):
        problems.append("owner reports %s pending requests" % drv.get("pending"))
    return {"operation_ids": ids, "drv_operation_id": drv.get("operation_id"), "drv_pending": drv.get("pending"),
            "sent": sent, "problems": problems}


def bench_on(console, output: str, hold_ms: int, record: dict, *, timeout_s=5.0) -> dict:
    """The only harness function that sends ``bench on``; the interlock runs first.

    A refusal sends no ON, records the entry in ``record['interlock']`` and
    raises ``InterlockRefused``. After an ON the reply's ``on_us`` gives the
    actual read ages on the controller clock; an age above ``max_age_ms``
    switches every output off and fails. A prepared serial stop never waits
    for this function.
    """
    if type(hold_ms) is not int or not 1 <= hold_ms <= probe.BENCH_MAX_HOLD_MS:
        raise ValueError("hold must be 1..60000 ms")
    entry = interlock(console, output, record, timeout_s=timeout_s)
    record.setdefault("interlock", []).append(entry)
    if entry["decision"] != "admit":
        raise InterlockRefused(entry)
    reply = console.command("bench", host_args=("on", output, str(hold_ms)), timeout_s=timeout_s)
    entry["on"] = _bench_view(reply)
    if not reply.get("ok"):
        raise BenchError("bench on %s failed: %s" % (output, reply.get("result")))
    on_us = entry["on"]["outputs"][output]["on_us"]
    ages = {a: {name: round((on_us - r["started_us"]) / 1000.0, 3) for name, r in reads.items()}
            for a, reads in entry["reads"].items()}
    for address, view in (entry.get("named_io") or {}).items():
        if type(view.get("started_us")) is int:
            ages.setdefault(address, {})["io (pre-start)"] = round((on_us - view["started_us"]) / 1000.0, 3)
    entry["age_at_on_ms"] = ages
    if any(age > (NAMED_IO_MAX_AGE_MS if name == "io (pre-start)" else entry["max_age_ms"])
           for reads in ages.values() for name, age in reads.items()):
        entry["off_after_age"] = _bench_view(console.command("bench", host_args=("off",), timeout_s=timeout_s))
        raise BenchError("an interlock read was older than %s ms at the ON; all outputs switched off" % entry["max_age_ms"])
    return reply


def outputs_off(console, *, timeout_s=5.0) -> dict:
    """Switch every bench output off and confirm it with a fresh view."""
    off = console.command("bench", host_args=("off",), timeout_s=timeout_s)
    view = console.command("bench", host_args=(), timeout_s=timeout_s)
    active = [o.get("name") for o in view.get("outputs", []) if o.get("active")]
    return {"off": off.get("ok"), "view": _bench_view(view), "outputs_off": bool(view.get("ok")) and not active}


def input_agreement(address: int, io: dict, config: dict) -> str | None:
    """First difference between the nine I/O words and ``read config``'s inputs, or None."""
    if not io.get("ok") or not config.get("ok"):
        return "io read or read config failed"
    words = (io.get("observation") or {}).get("raw")
    inputs = (config.get("config") or {}).get("inputs")
    polarity = ((config.get("config") or {}).get("raw") or {}).get("input_polarity")
    if not (isinstance(words, list) and len(words) == 9):
        return "io read lacks nine words"
    if (not isinstance(inputs, list) or len(inputs) != 4 or type(polarity) is not int or
            not all(isinstance(e, dict) and e.get("known") is True and type(e.get("function")) is int for e in inputs)):
        return "read config lacks four known input functions"
    if [e["function"] for e in inputs] != words[1:5] or (polarity & 0xF) != (words[0] & 0xF):
        return "functions %s polarity %d against io words %s" % ([e["function"] for e in inputs], polarity, words[:5])
    return None


def check_input_agreement(console, *, timeout_s=5.0) -> dict:
    """Session-level check, outside any interlock age window: per input drive,
    ``io read`` and ``read config`` report the same four input functions and
    input polarity. The interlock decides from the I/O words alone."""
    rows, problems = [], []
    for address in FIXTURE["input_drives"]:
        io = console.command("io", driver_args=("read",), address=address, timeout_s=timeout_s)
        config = console.command("read-config", address=address, timeout_s=timeout_s)
        problem = input_agreement(address, io, config)
        rows.append({"address": address, "io_operation_id": io.get("operation_id"),
                     "config_operation_id": config.get("operation_id"),
                     "io": (io.get("observation") or {}).get("raw"), "problem": problem})
        if problem:
            problems.append("drive %d: %s" % (address, problem))
    return {"verdict": "FAIL" if problems else "PASS", "rows": rows, "problems": problems}


def before_flash_check(console, *, timeout_s=5.0) -> dict:
    """COMMON's rule before every flash or controller reset.

    Fresh ``io read``, ``read config`` and ``read state`` of every input
    drive: all stopped (not running, raw speed zero, alarm-free) and no input
    function 7-17. PASS only when every drive is readable and meets both.
    """
    rows = []
    for address in FIXTURE["input_drives"]:
        row = {"address": address, "problems": []}
        io = console.command("io", driver_args=("read",), address=address, timeout_s=timeout_s)
        config = console.command("read-config", address=address, timeout_s=timeout_s)
        state = console.command("read-state", address=address, timeout_s=timeout_s)
        row["operation_ids"] = [r.get("operation_id") for r in (io, config, state)]
        words = (io.get("observation") or {}).get("raw") if io.get("ok") else None
        inputs = ((config.get("config") or {}).get("inputs") or []) if config.get("ok") else []
        if not (isinstance(words, list) and len(words) == 9):
            row["problems"].append("io read failed")
        else:
            row["io"] = words
            if any(7 <= f <= 17 for f in words[1:5]):
                row["problems"].append("input function 7-17 assigned: %s" % words[1:5])
        functions = [e.get("function") for e in inputs]
        if len(functions) != 4 or not all(e.get("known") is True for e in inputs):
            row["problems"].append("read config lacks known input functions")
        elif any(7 <= f <= 17 for f in functions):
            row["problems"].append("config reports input function 7-17: %s" % functions)
        disagreement = input_agreement(address, io, config)
        if disagreement and not row["problems"]:
            row["problems"].append("io read and read config disagree: " + disagreement)
        if not probe.drive_reported_stopped(state):
            row["problems"].append("not reported stopped, alarm-free and at zero speed")
        row["verdict"] = "FAIL" if row["problems"] else "PASS"
        rows.append(row)
    view = console.command("bench", host_args=(), timeout_s=timeout_s)
    return {"verdict": "PASS" if all(r["verdict"] == "PASS" for r in rows) else "FAIL", "rows": rows,
            "bench": _bench_view(view) if view.get("ok") else None}


# --------------------------------------------------------------------------
# Run folders, scenario records and results

RUN_NAME = re.compile(r"(?P<date>\d{4}-\d{2}-\d{2})_wb(?P<prompt>\d{2})_(?P<slug>[a-z0-9][a-z0-9_]{0,47})")
RUN_FILES = ("scenario.json", "session.jsonl", "summary.json", "console.log",
             "baseline_before.json", "ending_reads.json", "result.json")


class RunFolder:
    """``build/bench/<YYYY-MM-DD>_wb<NN>_<slug>/``, created exclusively."""

    def __init__(self, date: str, prompt: str, slug: str, root: Path = DEFAULT_RUN_ROOT):
        name = "%s_wb%s_%s" % (date, prompt, slug)
        if not RUN_NAME.fullmatch(name):
            raise ValueError("run folder needs YYYY-MM-DD, a two-digit prompt and a lowercase slug")
        _date.fromisoformat(date)
        self.date, self.prompt, self.slug = date, prompt, slug
        self.path = Path(root) / name

    @classmethod
    def from_path(cls, path) -> "RunFolder":
        path = Path(path)
        match = RUN_NAME.fullmatch(path.name)
        if not match:
            raise ValueError("run folder name must be YYYY-MM-DD_wbNN_slug: " + path.name)
        return cls(match["date"], match["prompt"], match["slug"], path.parent)

    @property
    def name(self) -> str:
        return self.path.name

    def create(self) -> "RunFolder":
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.mkdir()  # FileExistsError: never reuse a run folder.
        return self

    def __truediv__(self, name: str) -> Path:
        return self.path / name

    def write_json(self, name: str, data) -> Path:
        target = self.path / name
        with target.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(data, indent=1, allow_nan=False) + "\n")
        return target


def _check_case(case: dict, where: str) -> list:
    problems = []
    if not isinstance(case, dict):
        return [where + " is not an object"]
    if type(case.get("id")) is not str or not case["id"]:
        problems.append(where + " lacks an id")
    if type(case.get("required")) is not bool:
        problems.append(where + " lacks required")
    if type(case.get("assertion")) is not str or not case["assertion"]:
        problems.append(where + " lacks an assertion")
    levels = case.get("required_levels")
    if not isinstance(levels, list) or not levels or any(level not in LEVELS for level in levels):
        problems.append(where + " has invalid required_levels")
    return problems


SCENARIO_KEYS = ("schema", "prompt", "slug", "date", "run", "image", "commit", "port", "drives", "terminals",
                 "commands", "cases", "running_members", "write_list", "bounds", "prepared_stop",
                 "restoration_plan", "left_changed", "expected_evidence", "abort_conditions", "cleanup",
                 "max_age_ms", "facts")


def check_scenario(data) -> list:
    """Schema problems of a scenario record (empty when valid)."""
    if not isinstance(data, dict):
        return ["scenario record is not an object"]
    problems = ["scenario lacks " + key for key in SCENARIO_KEYS if key not in data]
    if problems:
        return problems
    if data["schema"] != SCENARIO_SCHEMA:
        problems.append("scenario schema differs")
    if not isinstance(data["image"], dict) or set(data["image"]) != {"path", "sha256"} or \
            not re.fullmatch(r"[0-9A-Fa-f]{64}", str(data["image"].get("sha256"))):
        problems.append("scenario image needs path and SHA-256")
    if type(data["commit"]) is not str or not data["commit"]:
        problems.append("scenario lacks a commit")
    if not isinstance(data["drives"], list) or not all(
            isinstance(d, dict) and type(d.get("address")) is int and type(d.get("required_enabled")) is bool
            for d in data["drives"]):
        problems.append("scenario drives need address and required_enabled")
    if not isinstance(data["cases"], list):
        problems.append("scenario cases is not a list")
    else:
        for i, case in enumerate(data["cases"]):
            problems += _check_case(case, "scenario case %d" % i)
    for entry in data["write_list"] if isinstance(data["write_list"], list) else [None]:
        if not (isinstance(entry, dict) and type(entry.get("drive")) is int and type(entry.get("key")) is str and
                "candidate" in entry):
            problems.append("write list entry needs drive, key and candidate")
    for entry in data["left_changed"] if isinstance(data["left_changed"], list) else [None]:
        if not (isinstance(entry, dict) and type(entry.get("drive")) is int and type(entry.get("key")) is str and
                entry.get("storage") in STORAGE and type(entry.get("reason")) is str and entry["reason"]):
            problems.append("left-changed entry needs drive, key, storage and reason")
    if not isinstance(data["running_members"], list):
        problems.append("running_members is not a list")
    return problems


def write_scenario(folder: RunFolder, **fields) -> dict:
    """COMMON's scenario record, written before the port opens."""
    data = {"schema": SCENARIO_SCHEMA, "prompt": folder.prompt, "slug": folder.slug, "date": folder.date,
            "run": folder.name, **fields}
    problems = check_scenario(data)
    if problems:
        raise RecordError("; ".join(problems))
    folder.write_json("scenario.json", data)
    return data


RESULT_KEYS = ("schema", "prompt", "slug", "date", "commit", "image", "cases", "restoration", "deviations")
RESULT_CASE_KEYS = ("id", "required", "assertion", "required_levels", "verdicts", "reason", "evidence")
RESTORATION_KEYS = ("outputs_off", "fields", "drives", "port_closed")
FIELD_KEYS = ("drive", "key", "baseline", "ending", "restored", "uncertain", "storage", "reason")
DRIVE_KEYS = ("address", "stopped", "alarm", "enabled", "required_enabled")


def check_result(data) -> list:
    """Schema problems of ``result.json`` (empty when valid)."""
    if not isinstance(data, dict):
        return ["result is not an object"]
    problems = ["result lacks " + key for key in RESULT_KEYS if key not in data]
    if problems:
        return problems
    if data["schema"] != RESULT_SCHEMA:
        problems.append("result schema differs")
    if not isinstance(data["image"], dict) or set(data["image"]) != {"path", "sha256"}:
        problems.append("result image needs path and sha256")
    if not isinstance(data["cases"], list):
        return problems + ["result cases is not a list"]
    for i, case in enumerate(data["cases"]):
        where = "result case %d" % i
        if not isinstance(case, dict) or any(key not in case for key in RESULT_CASE_KEYS):
            problems.append(where + " lacks " + ", ".join(k for k in RESULT_CASE_KEYS if not isinstance(case, dict) or k not in case))
            continue
        problems += _check_case(case, where)
        verdicts = case["verdicts"]
        if not isinstance(verdicts, dict) or not verdicts:
            problems.append(where + " has no verdicts")
            continue
        for level, verdict in verdicts.items():
            if level not in LEVELS:
                problems.append("%s level %s is unknown" % (where, level))
            if verdict not in VERDICTS:
                problems.append("%s verdict %r is not PASS/FAIL/NOT RUN/BLOCKED/indeterminate" % (where, verdict))
            elif verdict in NEEDS_REASON and not (type(case["reason"]) is str and case["reason"].strip()):
                problems.append("%s verdict %s needs a reason" % (where, verdict))
        if not isinstance(case["evidence"], list) or any(type(p) is not str for p in case["evidence"]):
            problems.append(where + " evidence is not a list of paths")
    restoration = data["restoration"]
    if not isinstance(restoration, dict) or any(key not in restoration for key in RESTORATION_KEYS):
        problems.append("restoration needs outputs_off, fields, drives and port_closed")
    else:
        for field in restoration["fields"] if isinstance(restoration["fields"], list) else [None]:
            if not isinstance(field, dict) or any(key not in field for key in FIELD_KEYS):
                problems.append("restoration field lacks " + ", ".join(FIELD_KEYS))
        for drive in restoration["drives"] if isinstance(restoration["drives"], list) else [None]:
            if not isinstance(drive, dict) or any(key not in drive for key in DRIVE_KEYS):
                problems.append("restoration drive lacks " + ", ".join(DRIVE_KEYS))
    if not isinstance(data["deviations"], list):
        problems.append("deviations is not a list")
    return problems


def write_result(folder: RunFolder, *, commit: str, image: dict, cases: list, restoration: dict,
                 deviations: list) -> dict:
    """``result.json`` (schema ``motorcontrol-rs/wired-bench-result/1``)."""
    data = {"schema": RESULT_SCHEMA, "prompt": folder.prompt, "slug": folder.slug, "date": folder.date,
            "commit": commit, "image": dict(image), "cases": cases, "restoration": restoration,
            "deviations": deviations}
    problems = check_result(data)
    if problems:
        raise RecordError("; ".join(problems))
    folder.write_json("result.json", data)
    return data


def _resolve(folder: Path, path: str) -> Path | None:
    for base in (folder, ROOT):
        candidate = (base / path) if not Path(path).is_absolute() else Path(path)
        if candidate.exists():
            return candidate
    return None


def _field_id(entry: dict) -> tuple:
    return (entry.get("drive"), entry.get("key"), entry.get("index"))


def check_run(folder) -> dict:
    """Two results for one run folder: artifact integrity and session success.

    Integrity: complete folder, valid scenario and result schemas, equal image
    SHA-256 and commit in both and in the image file when it exists, at least
    one case, known verdicts with reasons where needed, existing evidence
    paths. Session success: every required case PASS at each required level;
    every scenario drive in the ending reads, stopped, alarm-free and enabled
    as required; no uncertain write; every written field restored and read
    back or planned as left changed; outputs off; port closed. Never writes.
    """
    folder = Path(folder)
    integrity, session = [], []
    missing = [name for name in RUN_FILES if not (folder / name).is_file()]
    if not folder.is_dir():
        missing = list(RUN_FILES)
    if missing:
        integrity.append("folder incomplete: missing " + ", ".join(missing))
    if not RUN_NAME.fullmatch(folder.name):
        integrity.append("folder name is not YYYY-MM-DD_wbNN_slug")
    loaded = {}
    for name in ("scenario.json", "result.json", "summary.json", "baseline_before.json", "ending_reads.json"):
        if (folder / name).is_file():
            try:
                loaded[name] = _load_json(folder / name)
            except (ValueError, UnicodeError) as error:
                integrity.append("%s is not valid JSON: %s" % (name, error))
    if (folder / "session.jsonl").is_file():
        try:
            with (folder / "session.jsonl").open("rb") as stream:
                for number, line in enumerate(stream, 1):
                    json.loads(line.decode("utf-8"), parse_constant=invalid_constant)
        except (ValueError, UnicodeError):
            integrity.append("session.jsonl line %d is not JSON" % number)
    scenario, result = loaded.get("scenario.json"), loaded.get("result.json")
    if scenario is not None:
        integrity += check_scenario(scenario)
    if result is not None:
        integrity += check_result(result)
    if isinstance(scenario, dict) and isinstance(result, dict) and not integrity:
        if str(scenario["image"]["sha256"]).upper() != str(result["image"]["sha256"]).upper():
            integrity.append("image SHA-256 differs between scenario.json and result.json")
        if scenario["image"]["path"] != result["image"]["path"]:
            integrity.append("image path differs between scenario.json and result.json")
        if scenario["commit"] != result["commit"]:
            integrity.append("commit differs between scenario.json and result.json")
        image = _resolve(folder, scenario["image"]["path"])
        if image is not None and sha256_file(image) != str(scenario["image"]["sha256"]).upper():
            integrity.append("image file %s has another SHA-256" % scenario["image"]["path"])
        if not result["cases"]:
            integrity.append("result has no case")
        planned = {c["id"]: c for c in scenario["cases"]}
        reported = {c["id"]: c for c in result["cases"]}
        if set(planned) != set(reported) or len(reported) != len(result["cases"]):
            integrity.append("result cases differ from the scenario record")
        for cid, case in reported.items():
            plan = planned.get(cid)
            if plan and any(plan[k] != case[k] for k in ("required", "assertion", "required_levels")):
                integrity.append("case %s differs from its scenario record" % cid)
            for path in case["evidence"]:
                if _resolve(folder, path) is None:
                    integrity.append("case %s evidence %s is missing" % (cid, path))
    integrity_ok = not integrity
    if integrity_ok:
        reported = {c["id"]: c for c in result["cases"]}
        for case in scenario["cases"]:
            if not case["required"]:
                continue
            verdicts = reported[case["id"]]["verdicts"]
            for level in case["required_levels"]:
                if verdicts.get(level) != "PASS":
                    session.append("required case %s is %s at %s" % (case["id"], verdicts.get(level, "absent"), level))
        ending = loaded.get("ending_reads.json")
        drives = (ending or {}).get("drives") if isinstance(ending, dict) else None
        restoration = result["restoration"]
        reported_drives = {d["address"]: d for d in restoration["drives"]}
        for drive in scenario["drives"]:
            address = drive["address"]
            state = (drives or {}).get(str(address)) if isinstance(drives, dict) else None
            if not isinstance(state, dict) or not isinstance(state.get("state"), dict):
                session.append("drive %d is missing from the ending reads" % address)
                continue
            s = state["state"]
            if s.get("ok") is not True or s.get("running") is not False or s.get("raw_speed") != 0:
                session.append("drive %d is not reported stopped in the ending reads" % address)
            if s.get("raw_alarm") != 0 or s.get("alarm_flag") is not False:
                session.append("drive %d reports an alarm in the ending reads" % address)
            if s.get("enabled") is not drive["required_enabled"]:
                session.append("drive %d enabled %s, required %s" % (address, s.get("enabled"), drive["required_enabled"]))
            r = reported_drives.get(address)
            if r is None or r["stopped"] is not True or r["alarm"] or r["enabled"] is not drive["required_enabled"]:
                session.append("result restoration does not show drive %d stopped, alarm-free and enabled as required" % address)
        fields = {_field_id(f): f for f in restoration["fields"]}
        for field in restoration["fields"]:
            if field["uncertain"]:
                session.append("uncertain write outstanding: drive %s %s" % (field["drive"], field["key"]))
        planned_left = {_field_id(f) for f in scenario["left_changed"]}
        for entry in scenario["write_list"]:
            ident = _field_id(entry)
            if ident in planned_left:
                continue
            field = fields.get(ident)
            if field is None:
                session.append("written field drive %s %s has no restoration entry" % (entry["drive"], entry["key"]))
            elif not field["restored"] or field["ending"] != field["baseline"]:
                session.append("written field drive %s %s not restored and read back" % (entry["drive"], entry["key"]))
        for field in restoration["fields"]:
            if not field["restored"] and _field_id(field) not in planned_left and \
                    not any(_field_id(e) == _field_id(field) for e in scenario["write_list"]):
                session.append("unplanned left-changed field: drive %s %s" % (field["drive"], field["key"]))
        if restoration["outputs_off"] is not True:
            session.append("bench outputs not confirmed off")
        if restoration["port_closed"] is not True:
            session.append("port not confirmed closed")
    return {"folder": str(folder),
            "integrity": {"verdict": "PASS" if integrity_ok else "FAIL", "problems": integrity},
            "session": {"verdict": ("PASS" if not session else "FAIL") if integrity_ok else "NOT RUN",
                        "problems": session if integrity_ok else ["integrity failed"]}}


def check_exit(report: dict) -> int:
    if report["integrity"]["verdict"] != "PASS":
        return 1
    return 0 if report["session"]["verdict"] == "PASS" else 2


# --------------------------------------------------------------------------
# Baseline reads, comparison and re-baselining

DRIVE_NAMES = ("direction", "subdivision", "word-order", "soft-limit", "over-limit", "interruption", "position-mode")
DRIVE_REGISTERS = (0x10, 0x11, 0x19, 0x18, 0x17, 0x51, 0x50)
TUNING_KEYS = {"filters": "filters", "current-loop": "current_loop", "la": "la", "collision": "collision"}
HOME_RAW = {"auxiliary": "raw_auxiliary", "method": "raw_method", "search-speed": "raw_search_speed",
            "return-speed": "raw_return_speed", "ramp": "raw_ramp_time"}
VELOCITY_RAW = {"speed": "raw_speed", "acceleration": "raw_acceleration_time", "deceleration": "raw_deceleration_time"}
PROFILE_INDEX = {"start-speed": 0, "acceleration": 1, "deceleration": 2, "speed": 3}
SETTINGS_READS = ("config", "driver", "io", "control", "tuning.filters", "tuning.current-loop", "tuning.la",
                  "tuning.collision",
                  *("segment.%s.%d" % (kind, n) for kind in ("position", "speed", "start") for n in range(1, 17)),
                  "home", "velocity", "motion-profile")
LIVE_READS = ("identity", "state")
ALL_READS = ("identity", "config", "state", *SETTINGS_READS[1:])


def _read_call(key: str):
    if key in ("identity", "config", "state"):
        return "read-" + key, {}
    if key in ("driver", "io", "control"):
        return key, {"driver_args": ("read",)}
    if key.startswith("tuning."):
        return "tuning", {"driver_args": (key.split(".", 1)[1], "read")}
    if key.startswith("segment."):
        _, kind, number = key.split(".")
        return "segment", {"driver_args": (kind, number, "read")}
    if key == "home":
        return "home-read", {}
    if key == "velocity":
        return "velocity-read", {}
    raise ValueError("unknown read " + key)


def read_text(key: str, address: int) -> str:
    """Console line of one read, as the baseline records it."""
    if key == "motion-profile":
        return "motion-profile read"
    if key in ("identity", "config", "state"):
        return "read %s %d" % (key, address)
    if key.startswith("tuning."):
        return "tuning %s read %d" % (key.split(".", 1)[1], address)
    if key.startswith("segment."):
        _, kind, number = key.split(".")
        return "segment %s %s read %d" % (kind, number, address)
    if key in ("home", "velocity"):
        return "%s read %d" % (key, address)
    return "%s read %d" % (key, address)


def raw_of(key: str, reply: dict):
    """The raw part of one read reply (01's baseline extraction)."""
    if key == "identity":
        return {k: v for k, v in reply["identity"].items() if k.startswith("raw_")}
    if key == "config":
        return reply["config"]["raw"]
    if key == "state":
        return [{k: v for k, v in b.items() if k == "block" or k.startswith("raw_") or k == "position_words"}
                for b in reply["state_blocks"]]
    if key == "driver":
        o = reply["observation"]
        return {"raw": o["raw"], "positive_words": o["positive_words"], "negative_words": o["negative_words"]}
    if key == "home":
        return {k: v for k, v in reply["homing_parameters"].items() if k.startswith("raw_")}
    if key == "velocity":
        return {k: v for k, v in reply["velocity_parameters"].items() if k.startswith("raw_")}
    if key == "motion-profile":
        return reply["current"]
    return reply["observation"]["raw"]


def read_group(console, address: int, key: str, *, timeout_s=5.0) -> dict:
    """One checked read; a refusal or failure gives ``raw`` None with the reason."""
    if key == "motion-profile":
        import bench_motion as motion
        try:
            reply = motion.profile_snapshot(console, "read", timeout_s=timeout_s)
            forget = console.command("motion-profile", host_args=("forget",), timeout_s=timeout_s)
            if reply["original"] != reply["current"]:
                return {"command": read_text(key, address), "operation_id": None, "raw": None,
                        "error": "profile original differs from current on a fresh read"}
            return {"command": read_text(key, address), "operation_id": None, "raw": raw_of(key, reply),
                    "note": "host profile session, not an operation: the console assigns no operation ID; "
                            "request id %s, FC03 rx %s" % (reply.get("id"), reply.get("rx_hex")),
                    "forgotten": forget.get("ok")}
        except BenchError as error:
            return {"command": read_text(key, address), "operation_id": None, "raw": None, "error": str(error)}
    name, kwargs = _read_call(key)
    reply = console.command(name, address=address, timeout_s=timeout_s, **kwargs)
    entry = {"command": read_text(key, address), "operation_id": reply.get("operation_id")}
    if not reply.get("ok"):
        entry.update(raw=None, error=str(reply.get("result", reply.get("outcome"))))
        return entry
    try:
        entry["raw"] = raw_of(key, reply)
    except (KeyError, TypeError) as error:
        entry.update(raw=None, error="reply lacks %s" % error)
    entry["reply"] = reply
    return entry


def read_drive(console, address: int, keys=ALL_READS, *, select=True, timeout_s=5.0) -> dict:
    """Read the named groups of one drive. Selected-axis reads (home,
    velocity, motion profile) need the drive selected; ``select`` does it."""
    if select:
        use_drive(console, address, timeout_s=timeout_s)
    out = {}
    for key in keys:
        out[key] = read_group(console, address, key, timeout_s=timeout_s)
    return out


def _route(command, facts, readback, order=None, requires=None):
    return {"command": command, "facts": facts, "readback": readback, "order": order, "requires": requires}


_CONFIGURE = ("CONFIGURE facts on X0-X3 (`io qualification install A xN F P configure MAPPING`, stored F/P), "
              "fresh reads, stopped (ProbeApp.cpp 2046-2047); without them refused before TX (subdivision measured in B06)")
_SEGMENT = ("SEGMENT facts on X0-X3 (io qualification ... segment, stored F/P), fresh reads, stopped; "
            "without them refused before TX (PT record speed measured in B06)")
_PAIR = "pair writes refused before TX until D10 (prompt 10 measures the encoding)"
_TUNING15 = "tuning writes not routed for this field (prompt 15)"
_CONTROL = "refused by the control gates; D7 permits an exact raw write-back, but no checked route exists yet"


def baseline_fields(address: int, raws: dict) -> list:
    """Per-field values and restoration dispositions in 01's schema.

    ``raws`` maps read keys to raw values; a missing read gives value None.
    The disposition text is 01's (build/bench/2026-11-18_wb01_baseline/emit.py).
    """
    a = address
    out = []

    def value_of(field):
        return field_value(field, raws.get(field["read"]))

    def add(key, register, read, restore=None, none_reason=None, index=None):
        item = {"key": key, "register": "0x%04X" % register, "index": index, "read": read, "value": None,
                "restore": None}
        item["value"] = value_of(item)
        item["restore"] = restore(item["value"]) if callable(restore) else restore
        if item["restore"] is None:
            item["none_reason"] = none_reason
        out.append(item)

    orders = {"subdivision": "stopped; a change resets the host scale and RAM-only origin (invalidateAxis); refresh read config after",
              "word-order": "restore before reading or comparing any pair field; it changes pair interpretation",
              "soft-limit": "after the limit pairs are back at their baseline (pairs have no route yet)"}
    for name, reg in zip(DRIVE_NAMES, DRIVE_REGISTERS):
        add("drive." + name, reg, "driver",
            lambda v, name=name: _route("driver set %s %s %d" % (name, v, a), _CONFIGURE, "driver", orders.get(name)))
    add("drive.positive-limit", 0x37, "driver", none_reason=_PAIR)
    add("drive.negative-limit", 0x39, "driver", none_reason=_PAIR)
    for name, reg in (("custom-node", 0x13), ("baud", 0x14), ("format", 0x15)):
        add("config." + name, reg, "config",
            none_reason="communication settings: no restoration route in this image; owned by prompt 17")
    out_facts = ("ASSIGN output facts (`io qualification install A yN F P C assign MAPPING LOAD LEVELS`), fresh "
                 "reads; LOAD indicator for drive 4 Y0 (connected), none elsewhere (unconnected)")
    for name, reg in zip(probe.IO_FIELDS, probe.IO_REGISTERS):
        if name == "input-polarity":
            restore = lambda v, name=name: _route(
                "io set %s %s %d" % (name, v, a), "ASSIGN facts per changed bit; connected assigned inputs need the "
                "status mapping that prompt 05 measures", "io", None, "05")
        elif name.startswith("x"):
            restore = lambda v, name=name: _route(
                "io set %s %s %d" % (name, v, a), "ASSIGN facts (`io qualification install A " + name +
                " F P assign MAPPING`), fresh reads; add protection-removal when leaving 2-6, triggers-inhibited when "
                "old or new >= 4", "io",
                "restore trigger functions (7-13) before any bench output turns on; one terminal per write")
        else:
            restore = lambda v, name=name: _route("io set %s %s %d" % (name, v, a), out_facts, "io")
        add("io." + name, reg, "io", restore)
    for i, name in enumerate(probe.CONTROL_FIELDS):
        if name == "lock-delay":
            add("control." + name, 0x100 + i, "control",
                lambda v: _route("control set lock-delay %s %d" % (v, a), "none (fresh `read identity A` gives the "
                                 "exact 0x4EEA/0x0029 identity check, ProbeApp.cpp 2088)", "control"))
        else:
            add("control." + name, 0x100 + i, "control", none_reason=_CONTROL)
    for i, name in enumerate(probe.TUNING_FIELDS["filters"]):
        if name in ("input-filter", "deviation-threshold"):
            admitted = "input-filter 2 or 3 from 2 or 3" if name == "input-filter" else "deviation threshold 1..4000 from 1..4000"
            add("filters." + name, 0x108 + i, "tuning.filters",
                lambda v, name=name, admitted=admitted: _route(
                    "tuning filters set %s %s %d" % (name, v, a), "CONFIGURE facts on the connected inputs (prompt 03); "
                    "value in the admitted set (" + admitted + ")", "tuning.filters", None, "03"))
        else:
            add("filters." + name, 0x108 + i, "tuning.filters", none_reason=_TUNING15)
    for group, base in (("current-loop", 0x10E), ("la", 0x112), ("collision", 0x122)):
        for i, name in enumerate(probe.TUNING_FIELDS[TUNING_KEYS[group]]):
            add("%s.%s" % (group, name), base + i, "tuning." + group,
                none_reason="current-loop, LA and collision tuning writes not routed (prompt 15)")
    for n in range(1, 17):
        base = 0x60 + 6 * (n - 1)
        for i, name in enumerate(("speed", "acceleration", "deceleration")):
            add("records.position." + name, base + 2 + i, "segment.position.%d" % n,
                lambda v, name=name, n=n: _route("segment position %d set %s %s %d" % (n, name, v, a), _SEGMENT,
                                                 "segment.position.%d" % n), index=n)
        add("records.position.target", base, "segment.position.%d" % n, none_reason=_PAIR, index=n)
        base = 0xC0 + 3 * (n - 1)
        for i, name in enumerate(("speed", "acceleration", "deceleration")):
            add("records.speed." + name, base + i, "segment.speed.%d" % n,
                lambda v, name=name, n=n: _route("segment speed %d set %s %s %d" % (n, name, v, a), _SEGMENT,
                                                 "segment.speed.%d" % n), index=n)
        if n == 1:
            add("records.start", 0x130, "segment.start.1", index=1,
                none_reason="start record 1 (0x0130) echoes and discards writes (FAIL, 16 November)")
        else:
            add("records.start", 0x130 + n - 1, "segment.start.%d" % n,
                lambda v, n=n: _route("segment start %d set value %s %d" % (n, v, a), _SEGMENT, "segment.start.%d" % n),
                index=n)
    add("homing.auxiliary", 0x30, "home",
        lambda v: _route("home auxiliary %s" % v, "CONFIGURE facts on the connected inputs (prompt 03); drive "
                         "selected (`useaddr A`), fresh `read config` and `read state`", "home", None, "03"))
    for name, reg in (("method", 0x31), ("search-speed", 0x32), ("return-speed", 0x33), ("ramp", 0x34)):
        add("homing." + name, reg, "home",
            none_reason="homing method, speeds and ramp are written only by home staging, which starts a home")
    add("homing.offset", 0x35, "home", none_reason="homing offset pair: " + _PAIR)
    for name, reg in (("speed", 0x1D), ("acceleration", 0x1E), ("deceleration", 0x1F)):
        add("jog." + name, reg, "velocity",
            none_reason="JOG speed and ramps 0x001D-0x001F are written only by a velocity start")
    add("profile.start-speed", 0x20, "motion-profile",
        lambda v: _route("motion-profile start-speed %s" % v, "CONFIGURE facts on the connected inputs (prompt 03); "
                         "drive selected; then `motion-profile start-speed relation RAW_WORD MIN_RPM` for the restored word",
                         "motion-profile", "after any profile stage; the relation must be stated before the next move", "03"))
    profile = raws.get("motion-profile")
    stage = ("motion-profile stage %s %s %s %s %s 0 %d" % (*profile[1:6], a)) if isinstance(profile, list) and \
        len(profile) == 6 else None
    stage_facts = ("drive A selected, fresh identity/config/state and `motion-profile read`, stopped; refused "
                   "before TX while a connected X terminal has a function (singleSettingPolicy, B11), needs 03. "
                   "Meanwhile `motion-profile restore` returns a same-binding snapshot (B07)")
    stage_order = "one stage command writes 0x0021-0x0025 together with their baseline values; word order 0"
    for name, reg in (("acceleration", 0x21), ("deceleration", 0x22), ("speed", 0x23), ("target", 0x24)):
        add("profile." + name, reg, "motion-profile",
            _route(stage, stage_facts, "motion-profile", stage_order, "03") if stage else None,
            none_reason=None if stage else "motion-profile read failed")
    return out


def field_value(field: dict, raw):
    """Current value of one baseline field from its read's raw value, or None."""
    if raw is None:
        return None
    key, read = field["key"], field["read"]
    group, _, name = key.partition(".")
    try:
        if read == "driver":
            if name == "positive-limit":
                return list(raw["positive_words"])
            if name == "negative-limit":
                return list(raw["negative_words"])
            return raw["raw"][DRIVE_NAMES.index(name)]
        if read == "config":
            return raw[name.replace("-", "_")]
        if read == "io":
            return raw[probe.IO_FIELDS.index(name)]
        if read == "control":
            return raw[probe.CONTROL_FIELDS.index(name)]
        if read.startswith("tuning."):
            return raw[probe.TUNING_FIELDS[TUNING_KEYS[group]].index(name)]
        if read.startswith("segment."):
            parts = key.split(".")[1:]
            if parts == ["start"]:
                return raw[0]
            if parts[1] == "target":
                return list(raw[3:5])
            return raw[("speed", "acceleration", "deceleration").index(parts[1])]
        if read == "home":
            return list(raw["raw_offset_words"]) if name == "offset" else raw[HOME_RAW[name]]
        if read == "velocity":
            return raw[VELOCITY_RAW[name]]
        if read == "motion-profile":
            return list(raw[4:6]) if name == "target" else raw[PROFILE_INDEX[name]]
    except (KeyError, IndexError, TypeError, ValueError):
        return None
    return None


def load_baseline(path=DEFAULT_BASELINE) -> dict:
    """Load and check a baseline file; every field has a route or a reason.

    A file whose latest history entry has no reason is refused.
    """
    data = _load_json(path)
    problems = []
    keys = {"schema", "date", "commit", "image", "run", "fixture", "history", "drives"}
    if not isinstance(data, dict) or set(data) != keys:
        raise RecordError("baseline keys differ from %s" % sorted(keys))
    if data["schema"] != BASELINE_SCHEMA:
        problems.append("baseline schema differs")
    if not isinstance(data["history"], list):
        problems.append("history is not a list")
    elif data["history"] and not (isinstance(data["history"][-1], dict) and
                                  type(data["history"][-1].get("reason")) is str and data["history"][-1]["reason"].strip()):
        problems.append("latest history entry has no reason")
    if not isinstance(data["drives"], dict) or not data["drives"]:
        problems.append("baseline has no drives")
    else:
        for address, drive in data["drives"].items():
            if not isinstance(drive, dict) or set(drive) != {"live", "reads", "fields"}:
                problems.append("drive %s lacks live/reads/fields" % address)
                continue
            seen = set()
            for field in drive["fields"]:
                ident = (field.get("key"), field.get("index"))
                if ident in seen:
                    problems.append("drive %s duplicate field %s" % (address, ident))
                seen.add(ident)
                route = field.get("restore")
                if route is None:
                    if not field.get("none_reason"):
                        problems.append("drive %s %s has neither route nor reason" % (address, field.get("key")))
                elif not (isinstance(route, dict) and set(route) == {"command", "facts", "readback", "order", "requires"}
                          and route["command"] and route["facts"]):
                    problems.append("drive %s %s has an incomplete route" % (address, field.get("key")))
                if field.get("read") not in drive["reads"]:
                    problems.append("drive %s %s names an unknown read" % (address, field.get("key")))
    if problems:
        raise RecordError("; ".join(problems))
    return data


def diff_fields(baseline: dict, reads: dict, only_reads=None) -> list:
    """Per-field comparison with the captured value.

    ``reads`` maps an address to ``{read_key: raw or None}``. A failed or
    missing read gives status ``unknown``, never "equal" or "default".
    Only drives present in ``reads`` are compared, and with ``only_reads``
    only fields of those read groups.
    """
    out = []
    for address, drive in baseline["drives"].items():
        raws = reads.get(int(address), reads.get(address))
        if raws is None:
            continue
        for field in drive["fields"]:
            if only_reads is not None and field["read"] not in only_reads:
                continue
            current = field_value(field, raws.get(field["read"]))
            status = "unknown" if current is None else "equal" if current == field["value"] else "differs"
            out.append({"drive": int(address), "key": field["key"], "index": field["index"],
                        "register": field["register"], "baseline": field["value"], "current": current,
                        "status": status})
    return out


def raws_from(reads: dict) -> dict:
    """``{key: raw}`` from ``read_drive`` output."""
    return {key: entry.get("raw") for key, entry in reads.items()}


def capture_baseline(console, addresses=None, *, date: str, commit: str, image: dict, run: str,
                     timeout_s=5.0) -> dict:
    """Fresh baseline in 01's schema (history empty). Every read must succeed."""
    addresses = list(addresses or FIXTURE["input_drives"])
    drives = {}
    for address in addresses:
        reads = read_drive(console, address, ALL_READS, timeout_s=timeout_s)
        failed = [key for key, entry in reads.items() if entry.get("raw") is None]
        if failed:
            raise BenchError("baseline read failed on drive %d: %s" % (address, ", ".join(failed)))
        clean = {key: {k: v for k, v in entry.items() if k in ("command", "operation_id", "raw", "note")}
                 for key, entry in reads.items()}
        drives[str(address)] = {"live": {k: clean[k] for k in LIVE_READS},
                                "reads": {k: clean[k] for k in SETTINGS_READS},
                                "fields": baseline_fields(address, raws_from(reads))}
    return {"schema": BASELINE_SCHEMA, "date": date, "commit": commit,
            "image": {"environment": "bench_s3_load_timer", "sha256": image["sha256"]},
            "run": run, "fixture": dict(FIXTURE["board"]), "history": [], "drives": drives}


def _disposition(field: dict) -> dict:
    return {"restore": field.get("restore"), "none_reason": field.get("none_reason")}


def compare_baselines(old: dict, new: dict) -> list:
    """Per-field value and disposition differences (live state and read IDs excluded)."""
    out = []
    for address in sorted(set(old["drives"]) | set(new["drives"]), key=int):
        a = {(f["key"], f["index"]): f for f in old["drives"].get(address, {}).get("fields", [])}
        b = {(f["key"], f["index"]): f for f in new["drives"].get(address, {}).get("fields", [])}
        for ident in sorted(set(a) | set(b), key=lambda i: (i[0], i[1] or 0)):
            before, after = a.get(ident), b.get(ident)
            if before is None or after is None:
                out.append({"drive": int(address), "key": ident[0], "index": ident[1], "change": "field",
                            "old": before, "new": after})
                continue
            if before["value"] != after["value"]:
                out.append({"drive": int(address), "key": ident[0], "index": ident[1], "change": "value",
                            "old": before["value"], "new": after["value"]})
            if _disposition(before) != _disposition(after) or before["register"] != after["register"] or \
                    before["read"] != after["read"]:
                out.append({"drive": int(address), "key": ident[0], "index": ident[1], "change": "disposition",
                            "old": _disposition(before), "new": _disposition(after)})
    return out


def rebaseline(old: dict, new: dict, reason: str, prompt: str) -> dict:
    """New baseline document: fresh values, old dispositions except where the
    flashed image now admits a route (old null, new route), and a history
    entry with the old header and every per-field change."""
    if type(reason) is not str or not reason.strip():
        raise RecordError("re-baselining needs a reason")
    if not re.fullmatch(r"\d{2}", str(prompt)):
        raise RecordError("re-baselining needs a two-digit prompt")
    doc = copy.deepcopy(new)
    changes = []
    for address, drive in doc["drives"].items():
        previous = {(f["key"], f["index"]): f for f in old["drives"].get(address, {}).get("fields", [])}
        for field in drive["fields"]:
            before = previous.get((field["key"], field["index"]))
            if before is None:
                changes.append({"drive": int(address), "key": field["key"], "index": field["index"], "change": "added"})
                continue
            if before["value"] != field["value"]:
                changes.append({"drive": int(address), "key": field["key"], "index": field["index"],
                                "change": "value", "old": before["value"], "new": field["value"]})
            if before.get("restore") is None and field.get("restore") is not None:
                changes.append({"drive": int(address), "key": field["key"], "index": field["index"],
                                "change": "disposition", "old": _disposition(before), "new": _disposition(field)})
            elif before.get("restore") is not None and field.get("restore") is not None:
                # Keep the old facts, readback, order and requires; the command carries the new value.
                field["restore"] = dict(before["restore"], command=field["restore"]["command"])
            else:
                field["restore"] = before.get("restore")
                field.pop("none_reason", None)
                if field["restore"] is None:
                    field["none_reason"] = before.get("none_reason")
    header = {k: old[k] for k in ("date", "commit", "image", "run", "fixture")}
    doc["history"] = list(old.get("history", [])) + [{"date": new["date"], "prompt": str(prompt), "reason": reason,
                                                      "previous": header, "changes": changes}]
    return doc


# --------------------------------------------------------------------------
# Restoration planning and restoration through ordinary setters

def route_purpose(key: str) -> str | None:
    """Fact purpose an ordinary route needs: assign, configure, segment or none."""
    if key.startswith("io."):
        return "assign"
    if key.startswith("records."):
        return "segment"
    if key == "control.lock-delay":
        return None
    return "configure"


def write_call(field: dict, value, address: int, profile_words=None) -> tuple[str, dict, str]:
    """(console command, kwargs, console text) of one ordinary setter."""
    key = field["key"]
    group, _, name = key.partition(".")
    if group == "io":
        return "io", {"driver_args": ("set", name, str(value)), "address": address}, "io set %s %s %d" % (name, value, address)
    if group == "drive":
        return "driver", {"driver_args": ("set", name, str(value)), "address": address}, \
            "driver set %s %s %d" % (name, value, address)
    if group == "control":
        return "control", {"driver_args": ("set", name, str(value)), "address": address}, \
            "control set %s %s %d" % (name, value, address)
    if group in TUNING_KEYS:
        return "tuning", {"driver_args": (group, "set", name, str(value)), "address": address}, \
            "tuning %s set %s %s %d" % (group, name, value, address)
    if group == "records":
        parts = key.split(".")[1:]
        index = str(field["index"])
        if parts == ["start"]:
            return "segment", {"driver_args": ("start", index, "set", "value", str(value)), "address": address}, \
                "segment start %s set value %s %d" % (index, value, address)
        return "segment", {"driver_args": (parts[0], index, "set", parts[1], str(value)), "address": address}, \
            "segment %s %s set %s %s %d" % (parts[0], index, parts[1], value, address)
    if key == "homing.auxiliary":
        return "home-auxiliary", {"host_args": (str(value),)}, "home auxiliary %s" % value
    if key == "profile.start-speed":
        return "motion-profile-start-speed", {"host_args": (str(value),)}, "motion-profile start-speed %s" % value
    if group == "profile" and profile_words is not None:
        words = [str(w) for w in profile_words[1:6]]
        return "motion-profile-stage", {"host_args": (*words, "0", str(address))}, \
            "motion-profile stage %s 0 %d" % (" ".join(words), address)
    raise RecordError("no ordinary setter for " + key)


def _input_assign_facts(old: int, new: int, connected=True) -> list:
    needed = []
    if connected and 2 <= old <= 6 and new != old:
        needed.append("protection-removal")
    if old >= 4 or new >= 4:
        needed.append("triggers-inhibited")
    return needed


def _custom_ok(words: list) -> bool:
    """Core custom dependency: a set custom bit needs its terminal's custom function (9/10)."""
    custom = words[8]
    return all(not (custom >> i) & 1 or words[6 + i] == 9 + i for i in range(2))


def _register_span(field: dict) -> tuple[int, int]:
    start = int(field["register"], 16)
    return start, 2 if isinstance(field["value"], list) else 1


def _experiment_problem(record: dict, field: dict, entry: dict, context: dict) -> str | None:
    """First reason an experimental restoration record does not admit ``field``."""
    if not isinstance(record, dict):
        return "record is not an object"
    if str(record.get("image_sha256", "")).upper() != str(context.get("image_sha256", "")).upper():
        return "image"
    if record.get("prompt") != context.get("prompt") or record.get("case") != entry.get("case"):
        return "case"
    if record.get("drive") != entry["drive"]:
        return "drive"
    if record.get("field") != {"key": field["key"], "index": field["index"]}:
        return "field"
    window = record.get("window") or {}
    start, width = _register_span(field)
    if not (type(window.get("start")) is int and type(window.get("count")) is int and
            window["start"] <= start and start + width <= window["start"] + window["count"]):
        return "window"
    if not isinstance(record.get("write"), dict) or not isinstance(record.get("readback"), dict):
        return "write or readback command missing"
    source = record.get("source") or {}
    seen = (context.get("source_reads") or {}).get(source.get("operation_id"))
    if seen is None or seen.get("address") != entry["drive"] or seen.get("window") != window or \
            seen.get("before_first_write") is not True:
        return "source operation ID"
    words = source.get("words")
    if words != seen.get("words") or not isinstance(words, list) or len(words) != window["count"]:
        return "source words"
    offset = start - window["start"]
    candidate = field["value"] if isinstance(field["value"], list) else [field["value"]]
    if words[offset:offset + width] != candidate:
        return "source words"
    return None


ORDER_RANK = {"drive.word-order": 0}
# Wired bench 06: the homing words 0x0031-0x0036 and the JOG words 0x001D-0x001F
# have no static route in the 01 baseline; `home restore` / `velocity restore`
# write back the exact words of a checked read of this run taken before the
# case's first write (the snapshot), then read them back. One restore covers
# every field of its window.
SNAPSHOT_ROUTES = {"home": {"command": "home-restore", "keys": ("homing.method", "homing.search-speed",
                                                                   "homing.return-speed", "homing.ramp", "homing.offset"),
                            "read_command": "home-read", "window": (0x31, 6)},
                   "velocity": {"command": "velocity-restore", "keys": ("jog.speed", "jog.acceleration", "jog.deceleration"),
                                "read_command": "velocity-read", "window": (0x1D, 3)}}
SNAPSHOT_IMAGE_LEVEL = "06"


def snapshot_route(key: str) -> str | None:
    """Snapshot route name (home or velocity) of a baseline field, or None."""
    return next((name for name, route in SNAPSHOT_ROUTES.items() if key in route["keys"]), None)


def snapshot_words(baseline: dict, address: int, name: str) -> list | None:
    """The baseline words of one snapshot window in register order, or None."""
    words = []
    for key in SNAPSHOT_ROUTES[name]["keys"]:
        field = baseline_field(baseline, address, key)
        if field is None or field["value"] is None:
            return None
        words.extend(field["value"] if isinstance(field["value"], list) else [field["value"]])
    return words


def _rank(key: str, candidate) -> int:
    if key in ORDER_RANK:
        return ORDER_RANK[key]
    if key.startswith("io.x") and type(candidate) is int and 7 <= candidate <= 13:
        return 1
    if key.startswith("io.x") or key == "io.input-polarity":
        return 2
    if key.startswith("io."):
        return 3
    if key == "drive.soft-limit":
        return 9
    if key == "profile.start-speed":
        return 7
    if key.startswith("profile."):
        return 6
    return 4


def baseline_field(baseline: dict, address: int, key: str, index=None) -> dict | None:
    drive = baseline["drives"].get(str(address)) or {}
    return next((f for f in drive.get("fields", []) if f["key"] == key and f["index"] == index), None)


def plan_entry(baseline: dict, entry: dict, image_level: str, facts: dict, *, restore: bool,
               experiments=(), context=None) -> tuple[list, dict | None]:
    """Steps for one field, or a refusal. ``restore`` plans candidate ->
    baseline; otherwise the case's own write baseline -> candidate through the
    same ordinary route and stated facts."""
    address = entry["drive"]
    drive = baseline["drives"].get(str(address)) or {}
    field = baseline_field(baseline, address, entry["key"], entry.get("index"))
    ident = {"drive": address, "key": entry["key"], "index": entry.get("index")}
    if field is None:
        return [], {**ident, "reason": "no baseline field"}
    old, new = (entry["candidate"], field["value"]) if restore else (field["value"], entry["candidate"])
    step = {**ident, "register": field["register"], "read": field["read"], "baseline": field["value"],
            "candidate": entry["candidate"], "target": new}
    route = field.get("restore")
    snapshot = snapshot_route(field["key"]) if route is None else None
    if route is None and snapshot and restore and (context or {}).get("snapshots") is not None:
        if int(image_level) < int(SNAPSHOT_IMAGE_LEVEL):
            return [], {**ident, "reason": "snapshot route requires prompt %s; image level %s" % (SNAPSHOT_IMAGE_LEVEL, image_level)}
        if snapshot not in context["snapshots"]:
            return [], {**ident, "reason": "no %s snapshot read stated before the first write" % snapshot}
        r = SNAPSHOT_ROUTES[snapshot]
        words = snapshot_words(baseline, address, snapshot)
        if words is None:
            return [], {**ident, "reason": "baseline %s words incomplete" % snapshot}
        return [{**step, "route": "snapshot", "snapshot": snapshot, "words": words, "purpose": None, "facts": {},
                 "write": {"command": r["command"], "snapshot": snapshot,
                           "text": "%s SNAPSHOT_ID (%s read of drive %d before the first write)" %
                                   (r["command"].replace("-", " "), snapshot, address)},
                 "readback": r["read_command"].replace("-read", ""), "rank": 5}], None
    if route is None:
        if not restore:
            return [], {**ident, "reason": "no ordinary setter: " + str(field.get("none_reason"))}
        problems = [(_experiment_problem(r, field, entry, context or {}), r) for r in experiments]
        admitted = next((r for problem, r in problems if problem is None), None)
        if admitted is None:
            reason = "no admissible route: " + str(field.get("none_reason"))
            if problems:
                reason += "; experimental record refused (%s)" % ", ".join(p for p, _ in problems)
            return [], {**ident, "reason": reason}
        return [{**step, "route": "experiment", "purpose": None, "facts": {},
                 "write": admitted["write"], "readback": admitted["readback"],
                 "experiment": {k: admitted[k] for k in ("image_sha256", "prompt", "case", "window", "source")},
                 "rank": 5}], None
    if route.get("requires") and int(route["requires"]) > int(image_level):
        return [], {**ident, "reason": "route requires prompt %s; image level %s" % (route["requires"], image_level)}
    purpose = route_purpose(field["key"])
    stated = facts.get(str(address)) or facts.get(address) or {}
    step_facts = {}
    if purpose is not None:
        if purpose not in stated:
            return [], {**ident, "reason": "route needs %s facts the scenario record does not state" % purpose}
        step_facts = copy.deepcopy(stated[purpose])
    name = field["key"].partition(".")[2]
    verb = "restoring" if restore else "writing"
    if purpose == "assign" and name in TERMINALS:
        if type(old) is not int or type(new) is not int:
            return [], {**ident, "reason": "input function values must be integers"}
        have = list(step_facts.get(name, []))
        missing = [f for f in _input_assign_facts(old, new) if f not in have]
        if missing:
            return [], {**ident, "reason": "%s %s needs stated %s" % (verb, name, ", ".join(missing))}
        step_facts = {name: have}
    elif purpose == "assign":
        terminals = list(TERMINALS) if name == "input-polarity" else ["y0", "y1"] if name in ("custom", "output-polarity") else [name]
        step_facts = {t: list(step_facts[t]) for t in terminals if t in step_facts}
        if name in ("y0", "y1") and name not in step_facts:
            return [], {**ident, "reason": "%s %s needs stated load and levels" % (verb, name)}
    profile = (drive.get("reads", {}).get("motion-profile") or {}).get("raw")
    try:
        command, kwargs, text = write_call(field, new, address, profile_words=profile if restore else None)
    except RecordError as error:
        return [], {**ident, "reason": str(error)}
    steps = [{**step, "route": "ordinary", "purpose": purpose, "facts": step_facts,
              "write": {"command": command, "kwargs": kwargs, "text": text},
              "route_text": route["command"], "readback": route["readback"], "order_note": route.get("order"),
              "rank": _rank(field["key"], old)}]
    if restore and text.split() != route["command"].split():
        steps[0]["note"] = "baseline route text %r differs from the generated command" % route["command"]
    if field["key"] == "profile.start-speed" and restore:
        minimum = stated.get("start_speed_min_rpm")
        if minimum is None:
            return [], {**ident, "reason": "start-speed relation needs a stated minimum rpm"}
        steps.append({**ident, "key": "profile.start-speed-relation", "route": "relation", "purpose": None,
                      "facts": {}, "baseline": field["value"], "candidate": None, "target": None, "read": None,
                      "register": field["register"],
                      "write": {"command": "motion-profile-start-speed-relation",
                                "kwargs": {"host_args": (str(field["value"]), str(minimum))},
                                "text": "motion-profile start-speed relation %s %s" % (field["value"], minimum)},
                      "readback": None, "rank": 8})
    return steps, None


def restoration_plan(baseline: dict, write_list: list, image_level: str, facts: dict, *,
                     experiments=(), context=None) -> dict:
    """Ordered restoration steps for every field a case writes.

    Each entry of ``write_list`` (drive, key, index, candidate) needs the
    baseline field's ordinary route (``requires`` not later than
    ``image_level``) with the facts the scenario states in ``facts``
    (``{address: {purpose: {terminal: [facts]}}}``), or an experimental
    record bound to the image, prompt, case, drive, FC10 window and a source
    read of this run (``context``). Fields without either are listed under
    ``refused`` and the plan is not admissible. Sends nothing.
    """
    steps, refused = [], []
    context = context or {}
    for entry in write_list:
        if entry["key"].startswith("profile.") and entry["key"] != "profile.start-speed" and any(
                s["drive"] == entry["drive"] and s["key"].startswith("profile.") and s["route"] == "ordinary" and
                s["key"] != "profile.start-speed" for s in steps):
            continue  # One stage command restores 0x0021-0x0025 together.
        name = snapshot_route(entry["key"])
        if name and any(s["drive"] == entry["drive"] and s.get("snapshot") == name for s in steps):
            continue  # One restore writes the whole window.
        new, refusal = plan_entry(baseline, entry, image_level, facts, restore=True,
                                  experiments=experiments, context=context)
        if refusal:
            refused.append(refusal)
        else:
            steps.extend(new)
    steps = _order_steps(baseline, steps, write_list, refused)
    for number, step in enumerate(steps, 1):
        step["order"] = number
    return {"admissible": not refused, "image_level": str(image_level), "steps": steps, "refused": refused}


def _order_steps(baseline, steps, write_list, refused):
    """Stable rank order; I/O writes ordered so each FC06 state passes the custom dependency."""
    steps = sorted(steps, key=lambda s: (s["rank"], s["drive"]))
    out = []
    for address in sorted({s["drive"] for s in steps}):
        own = [s for s in steps if s["drive"] == address]
        io = [s for s in own if s["key"].startswith("io.") and s["key"].partition(".")[2] in
              ("output-polarity", "y0", "y1", "custom")]
        if io:
            fields = {f["key"]: f for f in baseline["drives"][str(address)]["fields"] if f["key"].startswith("io.")}
            words = [fields["io." + name]["value"] for name in probe.IO_FIELDS]
            for entry in write_list:
                if entry["drive"] == address and entry["key"].startswith("io."):
                    words[probe.IO_FIELDS.index(entry["key"][3:])] = entry["candidate"]
            ordered, remaining = [], list(io)
            while remaining:
                for step in remaining:
                    trial = list(words)
                    trial[probe.IO_FIELDS.index(step["key"][3:])] = step["baseline"]
                    if _custom_ok(trial):
                        words = trial
                        ordered.append(step)
                        remaining.remove(step)
                        break
                else:
                    for step in remaining:
                        refused.append({"drive": address, "key": step["key"], "index": step["index"],
                                        "reason": "no FC06 order passes the custom-output dependency"})
                    remaining = []
            position = min(own.index(s) for s in io)
            own = [s for s in own if s not in io]
            own[position:position] = ordered
        out.extend(own)
    return out


def _uncertain(result: dict) -> bool:
    """A write result that cannot prove whether the drive executed it."""
    if result.get("ok"):
        return bool(result.get("uncertain"))
    if result.get("uncertain") or result.get("execution_unknown"):
        return True
    admission = result.get("result") in probe.ADMISSION_FAILURES and "state" not in result
    return not admission


def _prerequisites(console, step: dict, *, timeout_s: float, check=None) -> dict:
    """Fresh reads and stated facts one ordinary write needs, in the documented order.

    ``check(out)``, when given, sees the fresh reads before any record is
    installed and raises to stop the write. ASSIGN records use the status
    mapping ``step["mappings"][terminal]`` (default ``unknown``); an input
    polarity write installs one record per changed input bit, carrying that
    terminal's current function.
    """
    address, purpose = step["drive"], step["purpose"]
    out = {}
    group = step["read"]
    if group in ("tuning.filters", "control", "driver") or group.startswith("segment."):
        out["identity"] = _ok(console, "read-identity", address=address, timeout_s=timeout_s)
        out["config"] = _ok(console, "read-config", address=address, timeout_s=timeout_s)
    if group != "io":
        name, kwargs = _read_call(group) if group not in ("motion-profile",) else (None, None)
        if name:
            out["group"] = _ok(console, name, address=address, timeout_s=timeout_s, **kwargs)
    out["io"] = _ok(console, "io", driver_args=("read",), address=address, timeout_s=timeout_s)
    if purpose in ("configure", "segment"):
        if check is not None:
            check(out)
        out["facts"] = probe.install_input_facts(console, address, purpose,
                                                 facts={t: tuple(v) for t, v in step["facts"].items()},
                                                 timeout_s=timeout_s)
        if stage_step(step):
            # The stage writes against a live profile session of this binding;
            # a readback's read-and-forget leaves none.
            import bench_motion as motion
            out["profile"] = {k: v for k, v in motion.profile_snapshot(console, "read", timeout_s=timeout_s).items()
                              if k in ("original", "current", "session_ok")}
        return out
    out["config"] = _ok(console, "read-config", address=address, timeout_s=timeout_s)
    out["state"] = probe.observe_stopped(console, address=address, timeout_s=timeout_s)
    if check is not None:
        check(out)
    if purpose != "assign":
        return out
    words = list(out["io"]["observation"]["raw"])
    name = step["key"][3:]
    after = list(words)
    after[probe.IO_FIELDS.index(name)] = step["target"]
    mappings = step.get("mappings") or {}
    installs = []
    if name in TERMINALS:
        index = TERMINALS.index(name)
        tokens = (str(address), name, str(step["target"]), str((words[0] >> index) & 1), "assign",
                  mappings.get(name, "unknown"), *step["facts"].get(name, []))
        installs.append(tokens)
    elif name == "input-polarity":
        for index, terminal in enumerate(TERMINALS):
            if not ((words[0] ^ step["target"]) >> index) & 1:
                continue
            installs.append((str(address), terminal, str(words[1 + index]), str((step["target"] >> index) & 1),
                             "assign", mappings.get(terminal, "unknown"), *step["facts"].get(terminal, [])))
    else:
        terminals = [name] if name in ("y0", "y1") else \
            [t for i, t in enumerate(("y0", "y1")) if ((words[probe.IO_FIELDS.index(name)] ^ step["target"]) >> i) & 1 or
             (name == "custom" and after[6 + i] == 9 + i and t in step["facts"])]
        for terminal in terminals:
            i = ("y0", "y1").index(terminal)
            load_levels = step["facts"].get(terminal)
            if not load_levels or len(load_levels) != 2:
                raise BenchError("output %s needs stated load and levels" % terminal)
            installs.append((str(address), terminal, str(after[6 + i]), str((after[5] >> i) & 1),
                             str((after[8] >> i) & 1), "assign", "unknown", *load_levels))
    out["installs"] = []
    for tokens in installs:
        probe.io_qualification_arguments("io-qualification-install", tokens)
        out["installs"].append(_ok(console, "io-qualification-install", host_args=tokens, timeout_s=timeout_s))
    return out


def apply_write(console, step: dict, *, timeout_s=5.0, check=None) -> dict:
    """One ordinary write of the step's target after its fresh prerequisites
    and stated facts (``check`` as in ``_prerequisites``). Returns the
    outcome; never retries."""
    outcome = {"text": step["write"]["text"], "target": step["target"]}
    outcome["prerequisites"] = _summary(_prerequisites(console, step, timeout_s=timeout_s, check=check))
    result = call(console, step["write"], timeout_s=timeout_s)
    outcome["result"] = {k: result.get(k) for k in ("ok", "result", "outcome", "state", "operation_id",
                                                    "uncertain", "status", "detail", "reason")}
    outcome["written"] = bool(result.get("ok"))
    outcome["uncertain"] = _uncertain(result)
    outcome["reply"] = result
    return outcome


def _summary(value):
    if isinstance(value, dict):
        return {k: _summary(v) for k, v in value.items() if k not in ("steps", "evidence", "state_blocks")}
    if isinstance(value, list):
        return [_summary(v) for v in value]
    return value


def stage_step(step: dict) -> bool:
    """A `motion-profile stage` step: it writes 0x0021-0x0025 together."""
    return (step.get("write") or {}).get("command") == "motion-profile-stage"


def readback(console, step: dict, *, timeout_s=5.0):
    """Fresh config read, then the field's group read; returns (value, entry).
    A profile stage step compares the whole 0x0021-0x0025 window (its five
    words), not only the field the planner keyed it on."""
    address = step["drive"]
    if stage_step(step):
        # A stage leaves its profile session open; the read must start a fresh one.
        console.command("motion-profile", host_args=("forget",), timeout_s=timeout_s)
    console.command("read-config", address=address, timeout_s=timeout_s)
    entry = read_group(console, address, step["read"], timeout_s=timeout_s)
    if stage_step(step):
        raw = entry.get("raw")
        return (list(raw[1:6]) if isinstance(raw, list) and len(raw) == 6 else None), entry
    return field_value({"key": step["key"], "read": step["read"], "index": step["index"]}, entry.get("raw")), entry


def _target(step: dict):
    """The value restore compares with: the five baseline stage words of a stage step."""
    if stage_step(step):
        return [int(w) for w in step["write"]["kwargs"]["host_args"][:5]]
    return step["baseline"]


def snapshot_restore(console, step: dict, snapshots: dict, *, timeout_s=5.0) -> dict:
    """One `home restore` / `velocity restore` of the run's snapshot, polled to
    its end. Never resent; an unknown write stays unknown until a read."""
    name = step["snapshot"]
    source = (snapshots or {}).get(name) or {}
    if type(source.get("operation_id")) is not int or source.get("address") != step["drive"]:
        raise BenchError("no %s snapshot of drive %d for the restore" % (name, step["drive"]))
    command = SNAPSHOT_ROUTES[name]["command"]
    # The restore needs fresh stopped state and configuration of the selected drive.
    console.command("read-config", address=step["drive"], timeout_s=timeout_s)
    console.command("read-state", address=step["drive"], timeout_s=timeout_s)
    reply = console.command(command, host_args=(str(source["operation_id"]),), timeout_s=timeout_s)
    out = {"admission": reply, "written": False, "uncertain": False}
    if not reply.get("ok"):
        return out
    deadline = time.monotonic() + timeout_s
    while reply.get("pending") and time.monotonic() < deadline:
        time.sleep(.02)
        reply = console.command(command, host_args=("inspect",), timeout_s=timeout_s)
    out.update(final=reply, written=reply.get("execution") != "not_transmitted",
               uncertain=bool(reply.get("pending") or reply.get("unsettled") or reply.get("execution") == "unknown"),
               ok=bool(reply.get("session_ok")), error=reply.get("error"))
    return out


def _snapshot_read(console, step: dict, timeout_s: float):
    """Current words of the step's window from a fresh read of the selected drive."""
    name = step["snapshot"]
    console.command("read-config", address=step["drive"], timeout_s=timeout_s)
    entry = read_group(console, step["drive"], SNAPSHOT_ROUTES[name]["read_command"].replace("-read", ""),
                       timeout_s=timeout_s)
    raw = entry.get("raw")
    if raw is None:
        return None, entry
    if name == "home":
        return [raw["raw_method"], raw["raw_search_speed"], raw["raw_return_speed"], raw["raw_ramp_time"],
                *raw["raw_offset_words"]], entry
    return [raw["raw_speed"], raw["raw_acceleration_time"], raw["raw_deceleration_time"]], entry


def restore(console, plan: dict, *, timeout_s=5.0, snapshots=None) -> list:
    """Restore every planned field whose fresh value differs from the baseline.

    Writes only through the planned ordinary route (or bound experiment, or
    the run's snapshot restore, ``snapshots`` {name: {operation_id, address}}),
    each followed by a readback. A field equal to the captured value or
    whose current value is unknown is never written. An uncertain write or a
    readback mismatch leaves the field uncertain and ends the restoration;
    nothing is replayed.
    """
    if not plan.get("admissible"):
        raise RecordError("restoration plan is not admissible: " + "; ".join(r["reason"] for r in plan["refused"]))
    results = []
    stop = False
    for step in plan["steps"]:
        entry = {"drive": step["drive"], "key": step["key"], "index": step.get("index"),
                 "baseline": step["baseline"], "ending": None, "restored": False, "uncertain": False,
                 "storage": "RAM", "reason": None, "written": False}
        results.append(entry)
        if stop:
            entry["reason"] = "not attempted after an uncertain restoration write"
            continue
        if step["route"] == "relation":
            reply = call(console, step["write"], timeout_s=timeout_s)
            entry.update(restored=bool(reply.get("ok")), ending=step["baseline"] if reply.get("ok") else None,
                         reason=None if reply.get("ok") else "relation refused: %s" % reply.get("result"),
                         storage="RAM")
            continue
        use_drive(console, step["drive"], timeout_s=timeout_s)
        if step["route"] == "snapshot":
            current, _ = _snapshot_read(console, step, timeout_s)
            entry.update(key="%s.window" % step["snapshot"], baseline=step.get("words"), before=current)
            if current is None:
                entry["reason"] = "current words unknown; no write"
                continue
            if current == step.get("words"):
                entry.update(ending=current, restored=True, reason="already equal to the captured words; no write")
                continue
            source_words = ((snapshots or {}).get(step["snapshot"]) or {}).get("words")
            outcome = snapshot_restore(console, step, snapshots, timeout_s=timeout_s)
            entry["written"] = outcome["written"]
            entry["snapshot_words"] = source_words
            entry["write"] = {k: v for k, v in outcome.items() if k not in ("admission", "final")}
            entry["write"]["final"] = {k: (outcome.get("final") or {}).get(k) for k in
                                       ("operation_id", "snapshot_id", "words", "readback", "execution", "error")}
            ending, _ = _snapshot_read(console, step, timeout_s)
            entry["ending"] = ending
            if outcome["uncertain"]:
                entry.update(uncertain=True, reason="uncertain restore write; read back once, not replayed")
                stop = True
            elif not outcome["written"]:
                entry["reason"] = "restore refused before transmission: %s" % (outcome["admission"].get("result"))
            elif source_words is not None and source_words != step.get("words") and ending == source_words:
                # The run started away from the baseline: its own changes are undone,
                # the baseline words are not reachable through this route.
                entry["reason"] = ("pre-run snapshot words %s restored and read back; the baseline words %s "
                                   "differ and stay changed" % (source_words, step.get("words")))
            elif ending != step.get("words"):
                entry.update(uncertain=True, reason="readback differs from the captured words")
                stop = True
            else:
                entry["restored"] = True
            continue
        current, before = (readback(console, step, timeout_s=timeout_s) if step["route"] == "ordinary"
                           else _experiment_read(console, step, timeout_s))
        entry["before"] = current
        if current is None:
            entry["reason"] = "current value unknown; no write"
            continue
        if current == _target(step):
            entry.update(ending=current if not stage_step(step) else step["baseline"], restored=True,
                         reason="already equal to the captured value; no write")
            continue
        if step["route"] == "experiment":
            reply = call(console, step["write"], timeout_s=timeout_s)
            outcome = {"written": bool(reply.get("ok")), "uncertain": _uncertain(reply), "text": step["write"].get("text")}
        else:
            try:
                outcome = apply_write(console, step, timeout_s=timeout_s)
            except BenchError as error:
                entry["reason"] = "prerequisites refused before the write: %s" % error
                continue
        entry["written"] = outcome["written"]
        entry["write"] = {k: v for k, v in outcome.items() if k != "reply"}
        ending, _ = (readback(console, step, timeout_s=timeout_s) if step["route"] == "ordinary"
                     else _experiment_read(console, step, timeout_s))
        entry["ending"] = ending
        if outcome["uncertain"]:
            entry.update(uncertain=True, reason="uncertain restoration write; read back once, not replayed")
            stop = True
        elif not outcome["written"]:
            entry["reason"] = "restoration write refused before transmission"
        elif ending != _target(step):
            entry.update(uncertain=True, reason="readback differs from the captured value")
            stop = True
        else:
            entry["restored"] = True
            if stage_step(step):
                entry["ending"] = step["baseline"]
                entry["window"] = ending
    return results


def _experiment_read(console, step: dict, timeout_s: float):
    spec = step["readback"]
    reply = call(console, spec, timeout_s=timeout_s)
    if not reply.get("ok"):
        return None, reply
    value = reply
    for part in spec.get("words", "observation.raw").split("."):
        value = value.get(part) if isinstance(value, dict) else None
    if not isinstance(value, list):
        return None, reply
    start, width = _register_span({"register": step["register"], "value": step["baseline"]})
    offset = start - step["experiment"]["window"]["start"]
    words = value[offset:offset + width]
    return (words if isinstance(step["baseline"], list) else (words[0] if words else None)), reply


# --------------------------------------------------------------------------
# Input status mapping (wired bench 05)

MAPPINGS = ("after", "before")


def status_bit(mapping: str, polarity_bit: int, source_on: bool) -> int:
    """Status bit (0x0008) one terminal reports under a mapping hypothesis.

    ``after``: the bit is the level after the 0x0040 inversion; ``before``:
    the physical level. At polarity 0 both report the source.
    """
    if mapping not in MAPPINGS or polarity_bit not in (0, 1):
        raise ValueError("mapping is after or before and the polarity bit 0 or 1")
    return int(bool(source_on)) ^ (polarity_bit if mapping == "after" else 0)


def classify_mapping(off_bit, on_bit) -> str:
    """Class from the status bit at polarity 1 with the source off and on:
    off 1 / on 0 is ``after``, off 0 / on 1 is ``before``, anything else a
    ``contradiction``."""
    if (off_bit, on_bit) == (1, 0):
        return "after"
    if (off_bit, on_bit) == (0, 1):
        return "before"
    return "contradiction"


# --------------------------------------------------------------------------
# Correlation (COMMON Timing): intervals on the controller clock

def io_sample(address: int, state: dict) -> dict | None:
    """Input-status sample (0x0008) of one ``read state``: request start and reply end bounds."""
    if not state.get("ok"):
        return None
    status = _blocks(state).get(1) or {}
    step = next((s for s in state.get("steps") or [] if s.get("first") == 8), None)
    start = step.get("attempted_us") if step else state.get("started_us")
    end = step.get("latest_us") if step else state.get("serviced_us")
    if type(start) is not int or type(end) is not int or type(status.get("raw_inputs")) is not int:
        return None
    return {"address": address, "operation_id": state.get("operation_id"), "request_start_us": start,
            "reply_end_us": end, "uncertainty_us": 0, "raw_inputs": status["raw_inputs"]}


def output_interval(gpio_us: int, samples: list, bit: int, new_level: bool) -> dict:
    """Bracket a drive's input transition after an output edge at ``gpio_us``.

    The transition lies after the request start of the last sample showing the
    old level and before the reply end of the first sample showing the new
    level, each widened by the sample's uncertainty.
    """
    samples = sorted((s for s in samples if s), key=lambda s: s["request_start_us"])
    first_new = next((i for i, s in enumerate(samples) if bool(s["raw_inputs"] & bit) == new_level
                      and s["reply_end_us"] >= gpio_us), None)
    if first_new is None:
        return {"status": "not observed", "reason": "no sample shows the new level after the edge"}
    old = [s for s in samples[:first_new] if bool(s["raw_inputs"] & bit) != new_level]
    if not old:
        return {"status": "indeterminate", "reason": "no earlier sample shows the old level"}
    last_old, new = old[-1], samples[first_new]
    earliest = last_old["request_start_us"] - last_old["uncertainty_us"]
    latest = new["reply_end_us"] + new["uncertainty_us"]
    return {"status": "measured", "earliest_us": earliest, "latest_us": latest,
            "relative_us": [earliest - gpio_us, latest - gpio_us],
            "old_operation_id": last_old["operation_id"], "new_operation_id": new["operation_id"]}


def _instant_gaps(samples: list, start_us: int, end_us: int) -> list:
    """Upper bounds of the gaps between consecutive sample instants that cover
    ``start_us..end_us``: a sample's instant lies between its request start and
    reply end, so two neighbours are at most ``reply_end[i+1] - request_start[i]``
    apart; ``start_us`` itself bounds the first gap (nothing is reported before
    the edge). Empty when the samples do not reach ``end_us``."""
    after = sorted((s for s in samples if s and s["reply_end_us"] >= start_us), key=lambda s: s["request_start_us"])
    if not after or after[-1]["request_start_us"] < end_us:
        return []
    gaps = [after[0]["reply_end_us"] - start_us]
    for before, nxt in zip(after, after[1:]):
        if before["request_start_us"] > end_us:
            break
        gaps.append(nxt["reply_end_us"] - before["request_start_us"])
    return gaps


def timing_trial(samples: list, *, on_us: int, off_us: int, bit: int, window_end_us: int | None = None) -> dict:
    """One bench pulse against the input-status samples of one drive (wired bench 05).

    On and off intervals as ``output_interval`` gives them; nominal poll
    spacing (request start to request start) after the ON; ``observed`` when a
    sample after the ON shows the bit. ``missed`` needs no sample with the
    bit, every instant gap over ``on_us..window_end_us`` below the actual
    pulse width (``off_us - on_us``) and samples past the window end;
    otherwise ``indeterminate`` with the reason.
    """
    samples = sorted((s for s in samples if s), key=lambda s: s["request_start_us"])
    width_us = off_us - on_us
    after = [s for s in samples if s["reply_end_us"] >= on_us]
    starts = [s["request_start_us"] for s in after]
    spacing = [b - a for a, b in zip(starts, starts[1:])]
    observed = any(s["raw_inputs"] & bit for s in after)
    out = {"on_us": on_us, "off_us": off_us, "width_us": width_us, "samples": len(after),
           "on": output_interval(on_us, samples, bit, True), "off": output_interval(off_us, samples, bit, False),
           "spacing_us": {"min": min(spacing), "max": max(spacing), "median": sorted(spacing)[len(spacing) // 2]}
           if spacing else None, "observed": observed}
    if observed:
        out["class"] = "observed"
        return out
    if window_end_us is None:
        out.update({"class": "indeterminate", "reason": "no off-delay bound for this filter value"})
        return out
    gaps = _instant_gaps(samples, on_us, window_end_us)
    out["max_instant_gap_us"] = max(gaps) if gaps else None
    if not gaps:
        out.update({"class": "indeterminate", "reason": "samples do not reach the window end"})
    elif max(gaps) >= width_us:
        out.update({"class": "indeterminate", "reason": "an instant gap of %d us is not below the %d us pulse"
                    % (max(gaps), width_us)})
    else:
        out["class"] = "missed"
    return out


def _fc06(record: dict, owner: int):
    try:
        raw = bytes.fromhex(record.get("raw_hex", ""))
    except ValueError:
        return None
    if record.get("kind") != "TX" or len(raw) != 8 or raw[0] != owner or raw[1] != 6:
        return None
    register = int.from_bytes(raw[2:4], "big")
    if not 0x4B <= register <= 0x4F:
        return None
    return {"register": register, "value": int.from_bytes(raw[4:6], "big")}


def _traffic_gaps(traffic: list) -> list:
    ordered = sorted(traffic, key=lambda r: r["sequence"])
    gaps = []
    for before, after in zip(ordered, ordered[1:]):
        if after["sequence"] > before["sequence"] + 1:
            gaps.append({"after_sequence": before["sequence"], "before_sequence": after["sequence"],
                         "missing": after["sequence"] - before["sequence"] - 1,
                         "from_us": before["at_us"], "to_us": after["at_us"]})
    return gaps


def _merge_edges(bench_views: list) -> tuple[list, list]:
    """Unique ``aux1`` edges and the windows where edges were lost."""
    edges, lost, seen = [], [], set()
    previous = None
    for view in bench_views:
        sensed = view.get("input") or {}
        recent = [tuple(e) for e in sensed.get("recent") or []]
        new = [e for e in recent if e not in seen]
        if previous is not None:
            added = sensed.get("edges", 0) - previous["edges"]
            if added < 0 or sensed.get("overwritten", 0) > previous["overwritten"] or added > len(new):
                lost.append({"from_us": previous["now_us"], "to_us": view.get("now_us"),
                             "edges_added": added, "edges_seen": len(new)})
        for edge in new:
            seen.add(edge)
            edges.append({"at_us": edge[0], "active": edge[1]})
        previous = {"edges": sensed.get("edges", 0), "overwritten": sensed.get("overwritten", 0),
                    "now_us": view.get("now_us")}
    return sorted(edges, key=lambda e: e["at_us"]), lost


def correlate(traffic: list, bench_views: list, *, owner: int = FIXTURE["y0_owner"],
              pair_window_us: int = 50000, outputs=()) -> dict:
    """COMMON Timing intervals.

    ``outputs``: output edges ``{output, edge, gpio_us, bit, new_level,
    samples: {address: [io_sample...]}}``, each bracketed per drive by
    ``output_interval``. ``traffic`` (debug traffic records) and
    ``bench_views`` give each ``aux1`` edge against the request start and
    reply end of the nearest FC06 to 0x004B-0x004F on ``owner``. A sequence
    gap or an ``aux1`` overwrite over a pairing makes it indeterminate; an
    edge before the reply end is a measured relationship, not a fault.
    """
    result = {"outputs": [], "aux1": {"edges": [], "writes": [], "traffic_gaps": [], "lost_edge_windows": []}}
    for edge in outputs:
        row = {k: edge[k] for k in ("output", "edge", "gpio_us")}
        row["drives"] = {str(a): output_interval(edge["gpio_us"], samples, edge["bit"], edge["new_level"])
                         for a, samples in edge["samples"].items()}
        result["outputs"].append(row)
    traffic = [r for r in traffic if isinstance(r, dict) and type(r.get("sequence")) is int]
    gaps = _traffic_gaps(traffic)
    result["aux1"]["traffic_gaps"] = gaps
    writes = []
    by_transaction = {}
    for record in sorted(traffic, key=lambda r: r["sequence"]):
        by_transaction.setdefault(record.get("transaction"), []).append(record)
    for transaction, records in by_transaction.items():
        tx = next((r for r in records if _fc06(r, owner)), None)
        if tx is None:
            continue
        fc06 = _fc06(tx, owner)
        rx = next((r for r in records if r.get("kind") == "RX" and r["sequence"] > tx["sequence"]), None)
        write = {"transaction": transaction, **fc06, "request_start_us": tx["at_us"],
                 "reply_end_us": rx["end_us"] if rx else None,
                 "uncertainty_us": rx.get("uncertainty_us", 0) if rx else None, "edges": 0}
        writes.append(write)
    first_us = min((r["at_us"] for r in traffic), default=None)
    last_us = max((r["at_us"] for r in traffic), default=None)
    edges, lost = _merge_edges(bench_views)
    result["aux1"]["lost_edge_windows"] = lost
    for edge in edges:
        e = edge["at_us"]
        row = {"at_us": e, "active": edge["active"]}
        in_lost = any(w["from_us"] is not None and w["to_us"] is not None and w["from_us"] <= e <= w["to_us"]
                      for w in lost)

        def distance(w):
            end = w["reply_end_us"] if w["reply_end_us"] is not None else w["request_start_us"]
            return 0 if w["request_start_us"] <= e <= end else min(abs(e - w["request_start_us"]), abs(e - end))
        nearest = min(writes, key=distance, default=None)

        def gap_over(start, end):
            return any(g["from_us"] < end and g["to_us"] > start for g in gaps)
        if in_lost:
            row.update(status="indeterminate", reason="aux1 edges were overwritten in this window")
        elif nearest is None or distance(nearest) > pair_window_us:
            outside = first_us is None or not first_us <= e - pair_window_us or e + pair_window_us > last_us
            missing = outside or gap_over(e - pair_window_us, e + pair_window_us)
            row.update(status="indeterminate" if missing else "unpaired",
                       reason="traffic records missing near the edge" if missing else
                       "no FC06 to 0x004B-0x004F within %d us" % pair_window_us)
        elif nearest["reply_end_us"] is None:
            row.update(status="indeterminate", reason="the write's reply record is missing",
                       transaction=nearest["transaction"])
        elif gap_over(min(e, nearest["request_start_us"]), max(e, nearest["reply_end_us"])):
            row.update(status="indeterminate", reason="debug records missing inside the pairing window",
                       transaction=nearest["transaction"])
        else:
            unc = nearest["uncertainty_us"] or 0
            after_end = e - nearest["reply_end_us"]
            relation = ("after reply end" if after_end > unc else "before reply end" if after_end < -unc
                        else "at reply end within uncertainty")
            row.update(status="measured", transaction=nearest["transaction"], register=nearest["register"],
                       value=nearest["value"], after_request_start_us=e - nearest["request_start_us"],
                       after_reply_end_us=[after_end - unc, after_end + unc], relationship=relation)
            nearest["edges"] += 1
        result["aux1"]["edges"].append(row)
    result["aux1"]["writes"] = writes
    return result


# --------------------------------------------------------------------------
# Command line

def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="action", required=True)
    check = sub.add_parser("check", help="check one run folder: integrity, then session success; "
                                         "exit 0 both pass, 1 integrity failure, 2 session failure")
    check.add_argument("folder", type=Path)
    args = parser.parse_args(argv)
    report = check_run(args.folder)
    print(json.dumps(report, indent=1))
    return check_exit(report)


if __name__ == "__main__":
    sys.exit(main())
