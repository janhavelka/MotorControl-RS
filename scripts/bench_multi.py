#!/usr/bin/env python3
"""Wired bench 19: discovery past a silent address, group absolute targets and
the short per-drive regression.

One run folder per invocation (``build/bench/<date>_wb19_<slug>/``), one
console session, ordinary console routes only (``bench_probe.Console``):

- D1 probe and identity of drives 1, 2 and 4 with ``debug raw``: one checked
  reply per request; the runner's received bytes grow by exactly the expected
  reply lengths and nothing is discarded (no foreign bytes).
- D2 ``discover addresses 1 8 past-silent identity requests 32``: responders
  1, 2, 4 with identity, silent 3 and 5-8, one settle per silence except the
  last, no late byte; the scan ends in INTERLOCK with the boundary required;
  ordinary traffic and ``discover restore`` are refused until the operator's
  ``recover``; then restore and finish.
- D3 the same scan without ``past-silent``: today's interlock at address 3.
- MOTION (one session, cases G1-G4 and R1): per-member host coordinates
  (command scale 1000, a caller-qualified reference at the read position, the
  origin there) and absolute group plans; R1 per drive with all drives powered.
  The profile words 0x0021/5 are restored at the end through the stage route.

Every motion case is finite. What ends motion is the move's own end, the
prepared ``group stop fast`` and ``stop fast`` to every drive (sent if a trial
raises or loses the console), or a group stop of the case itself. No write is
replayed. Deferred and NOT RUN cases (B1-B5, K1, E1) are recorded in the
report; this tool sends nothing for them.
"""
import argparse
import json
import sys
import time

from pathlib import Path

import bench_input_starts as starts
import bench_motion as motion
import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stops
import bench_switch
from bench_scenarios import _Run, _finish, _preflight, _stop_restore
from bench_session import run_recorded

DRIVES = tuple(records.FIXTURE["input_drives"])  # 1, 2, 4
SCAN = ("addresses", "1", "8", "identity", "requests", "32")
PAST_SILENT = SCAN + ("past-silent",)
EXPECTED_SILENT = [3, 5, 6, 7, 8]
PROBE_REPLY, IDENTITY_REPLY = 7, 13         # FC03 replies of one and four words.
OFFSETS = {1: 2000, 2: -1500, 4: 500}       # G1 host targets (steps, motor frame, origin at the start).
LONG = {1: 6000, 2: -6000, 4: 6000}         # G3 targets: about 6 s at 60 rpm, stopped at 1.5 s.
MIXED = {1: ("absolute", 1000), 2: ("relative", -800), 4: ("absolute", 300)}  # G4.
RPM = 60
SCALE = "1000"                              # Host command steps per motor turn (ASSUMED, member only).
G1_RUNS = 10                                # Per start mode, alternating target and origin.
STOP_AFTER_S = 1.5
G3_LATER = 5                                # G3 diagnostic admissions after a refusal, 1 s apart.
R1_MOVE, R1_STOP_MOVE, R1_GROUP = 1000, 3000, 500
RUNNING_BIT = 4
STARTED = "started"                         # `stats`: runner transactions started.

CASES = {
    "D1": dict(levels=["drive-reported"], assertion="probe and read identity at 1, 2 and 4 with debug raw: one checked "
               "reply per request from the addressed drive; runner rx_bytes grow by exactly 7 per probe and 13 per "
               "identity read; no discarded byte, failed transaction or timeout"),
    "D2": dict(levels=["drive-reported"], assertion="discover addresses 1 8 past-silent identity: responders 1, 2, 4 with "
               "identity, silent 3 and 5-8, one settle per silence, no late byte, outcome complete, INTERLOCK with the boundary "
               "required; probe and discover restore refused before the operator recover; then restore (tuple and "
               "selection as before) and finish; a probe afterwards is admitted"),
    "D3": dict(levels=["drive-reported"], assertion="the same scan without past-silent: responders 1 and 2, no response at "
               "3, transport fault, INTERLOCK as today; restore refused until recover; then restore and finish"),
    "G1": dict(levels=["drive-reported"], assertion="host-coordinate absolute group plans (1: +2000, 2: -1500, 4: +500 "
               "steps from each member's origin), 10 staged and 10 sequential alternating with the origin: every member "
               "completes and ends at its own target"),
    "G2": dict(levels=["sent"], assertion="one member's absolute target equal to its current position with a current "
               "reference: the plan is refused before any frame (zero displacement), as documented"),
    "G3": dict(levels=["drive-reported"], assertion="group stop normal and group stop fast 1.5 s into a long absolute plan; "
               "every member stops short; the same absolute plan then brings every member to its own target"),
    "G4": dict(levels=["drive-reported"], assertion="mixed plan (1 absolute +1000, 2 relative -800, 4 absolute +300) and its "
               "return: each member ends where requested"),
    "R1": dict(levels=["drive-reported"], assertion="per drive with all drives powered: probe, identity, config and state "
               "reads; +1000 completes; +3000 stopped by stop fast 1 s in, standstill; a one-member group move of +500 "
               "completes"),
}
GROUPS = {"D1": ["D1"], "D2": ["D2"], "D3": ["D3"], "MOTION": ["G1", "G2", "G3", "G4", "R1"]}
EXPOSURE = {"D1": "none: reads only", "D2": "none: reads only", "D3": "none: reads only",
            "MOTION": "each finite move's own end (at most about 8 s at 60 rpm); the prepared group stop fast and "
                      "stop fast to every drive on any harness failure"}


# --------------------------------------------------------------------------
# Pure checks (unit-tested)

def stats_delta(before: dict, after: dict) -> dict:
    keys = ("started", "frames", "failed", "timeouts", "rx_bytes", "discarded")
    return {k: after[k] - before[k] for k in keys}


def d1_verdict(delta: dict, replies: list) -> tuple[str, str]:
    """PASS when every request has its own checked reply and the bus carried nothing else."""
    expected = {"started": 2 * len(DRIVES), "frames": 2 * len(DRIVES), "failed": 0, "timeouts": 0,
                "rx_bytes": (PROBE_REPLY + IDENTITY_REPLY) * len(DRIVES), "discarded": 0}
    problems = ["%s %+d, expected %+d" % (k, delta[k], v) for k, v in expected.items() if delta[k] != v]
    for reply in replies:
        if not reply["ok"] or reply["address"] != reply["answered"]:
            problems.append("%s at %d: ok %s, answered %s" % (reply["command"], reply["address"], reply["ok"], reply["answered"]))
    return ("PASS" if not problems else "FAIL"), "; ".join(problems) or "one checked reply per request; %s" % delta


def scan_summary(scan: dict) -> dict:
    return {"phase": scan["phase"], "outcome": scan["outcome"], "requests": scan["requests"],
            "findings": [{"address": f["target"][1], "outcome": f["outcome"], "raw_model": f["raw_model"],
                          "identity_known": f["identity_known"], "identity": f["identity"]} for f in scan["findings"]],
            "silent": scan.get("silent"), "settles": scan.get("settles"), "settle": scan.get("settle"),
            "late_bytes": scan.get("late_bytes"), "boundary_required": scan.get("boundary_required"),
            "boundary_crossed": scan.get("boundary_crossed"), "restored": scan["restored"]}


def d2_problems(ended: dict, refusals: dict, restored: dict) -> list:
    problems = []
    found = [(f["address"], f["outcome"], f["identity_known"], f["identity"][2]) for f in ended["findings"]]
    if found != [(a, 1, True, a) for a in DRIVES]:
        problems.append("findings %s" % found)
    if ended["silent"] != EXPECTED_SILENT:
        problems.append("silent %s" % ended["silent"])
    if (ended["settles"], ended["settle"], ended["late_bytes"]) != (len(EXPECTED_SILENT), "recovered", 0):
        problems.append("settles %s %s late %s" % (ended["settles"], ended["settle"], ended["late_bytes"]))
    if (ended["phase"], ended["outcome"], ended["boundary_required"], ended["boundary_crossed"], ended["restored"]) != \
            (4, 1, True, False, False):
        problems.append("end phase %s outcome %s boundary %s/%s restored %s" % (
            ended["phase"], ended["outcome"], ended["boundary_required"], ended["boundary_crossed"], ended["restored"]))
    for name, result in (("probe", "busy"), ("restore", "recovery_required")):
        if refusals.get(name) != result:
            problems.append("%s before the boundary: %s" % (name, refusals.get(name)))
    if refusals.get("frames") != 0:
        problems.append("frames during the refusals: %s" % refusals.get("frames"))
    if (restored["phase"], restored["restored"], restored["boundary_crossed"]) != (5, True, True):
        problems.append("after the boundary phase %s restored %s crossed %s" % (
            restored["phase"], restored["restored"], restored["boundary_crossed"]))
    return problems


def d3_problems(ended: dict, refusals: dict, restored: dict) -> list:
    problems = []
    found = [(f["address"], f["outcome"]) for f in ended["findings"]]
    if found != [(1, 1), (2, 1), (3, 5)]:
        problems.append("findings %s" % found)
    if (ended["phase"], ended["outcome"], ended["silent"], ended["settles"]) != (4, 6, [], 0):
        problems.append("end phase %s outcome %s silent %s settles %s" % (
            ended["phase"], ended["outcome"], ended["silent"], ended["settles"]))
    if refusals.get("restore") != "recovery_required":
        problems.append("restore before recover: %s" % refusals.get("restore"))
    if (restored["phase"], restored["restored"]) != (5, True):
        problems.append("after recover phase %s restored %s" % (restored["phase"], restored["restored"]))
    return problems


def endpoint_errors(targets: dict, positions: dict) -> dict:
    return {a: positions[a] - targets[a] for a in targets}


def member_targets(origins: dict, intents: dict, starts_at: dict) -> dict:
    """Native endpoint per member: origin + host value (absolute) or start + value (relative)."""
    out = {}
    for a, (kind, value) in intents.items():
        out[a] = origins[a] + value if kind == "absolute" else starts_at[a] + value
    return out


def g_verdict(trials: list, tolerance: int) -> tuple[str, str]:
    """PASS when every trial completed for every member within ``tolerance`` increments."""
    bad = []
    for t in trials:
        if not t.get("ok"):
            bad.append("%s: %s" % (t.get("label"), t.get("error") or t.get("state")))
            continue
        for a, e in t["errors"].items():
            if abs(e) > tolerance:
                bad.append("%s: drive %s error %+d" % (t["label"], a, e))
    spread = sorted(abs(e) for t in trials if t.get("ok") for e in t["errors"].values())
    return ("PASS" if not bad else "FAIL"), ("; ".join(bad) if bad else "%d trials, endpoint |error| max %d" %
                                              (len(trials), spread[-1] if spread else 0))


def stop_problems(stop_terminal: dict, states: dict, start: dict, end: dict) -> list:
    """Why a group stop mid-plan is not shown: the stop result not observed for
    every member, a member still running or at speed, or a member that never
    moved (its stop proves nothing). Empty means the stop evidence holds."""
    problems = []
    members = {m.get("address"): m for m in (stop_terminal or {}).get("members", [])}
    if not stop_terminal or stop_terminal.get("ok") is not True:
        problems.append("group stop %s" % ((stop_terminal or {}).get("state") or (stop_terminal or {}).get("result")))
    for a in DRIVES:
        m = members.get(a)
        if not m or m.get("completion") != "observed":
            problems.append("drive %d stop %s" % (a, m and m.get("completion")))
        st = states.get(a)
        if not st or st["motion"] & RUNNING_BIT or st["speed"]:
            problems.append("drive %d not reported stopped" % a)
        if start.get(a) is None or end.get(a) is None or start[a] == end[a]:
            problems.append("drive %d did not move" % a)
    return problems


def all_stopped(states: dict) -> bool:
    return all(states.get(a) and not states[a]["motion"] & RUNNING_BIT and not states[a]["speed"] for a in DRIVES)


# --------------------------------------------------------------------------
# Session helpers

class Live(starts.Live):
    def __init__(self, run, baseline, args):
        starts.Live.__init__(self, None, run, baseline, args, args.timeout)
        self.detail = {}

    def stats(self):
        return self.checked("stats")

    def inspect(self, limit_s=30.0):
        """Local scan inspection until INTERLOCK or TERMINAL (bounded)."""
        deadline = time.monotonic() + limit_s
        while True:
            scan = self.checked("discover", host_args=("inspect",))["scan"]
            if scan["phase"] in (4, 5) or time.monotonic() > deadline:
                return scan
            time.sleep(0.05)


def prepared_stops(live):
    """Group stop fast, then stop fast to every drive; never retried here.

    A drive stop refused ``unavailable`` was refused before TX: the firmware admits
    a stop only to the selected drive or to one with a checked reply since the last
    selection change or restart (wired bench 20 b4). One ``read state`` binds that
    drive and one new stop fast follows; nothing sent is ever replayed.
    """
    for name, kwargs in (("group-stop", {"host_args": ("fast",)}),
                         *(("stop", {"stop_policy": "fast", "address": a}) for a in DRIVES)):
        try:
            reply = live.call(name, **kwargs)
            if name == "stop" and reply.get("result") == "unavailable":
                live.call("read-state", address=kwargs["address"])
                live.call(name, **kwargs)
        except Exception as error:  # noqa: BLE001 - recorded, not retried.
            live.detail.setdefault("prepared_stop_errors", []).append("%s: %s" % (name, error))


def session(folder, live, work, *, debug=None):
    def run(console, record):
        live.console = console
        try:
            work(console)
        except BaseException:
            if console.synchronized and live.detail.get("motion"):
                prepared_stops(live)
            raise
        record["workload_verified"] = True

    return run_recorded(port=live.args.port, scenario="multi-drive", inputs={"group": live.args.group}, run=run,
                        address=1, timeout_s=live.timeout, firmware=live.args.firmware, refresh=False, debug=debug,
                        paths=dict(summary=folder / "summary.json", jsonl=folder / "session.jsonl",
                                   console=folder / "console.log"),
                        max_records=200000, max_bytes=64 * 1024 * 1024)


def baseline_before(live):
    _preflight(live.console, live.run, live.timeout)
    declarations = records.check_declarations(live.console, timeout_s=live.timeout)
    before = {a: records.read_drive(live.console, a, ("io", "state", "config"), select=False, timeout_s=live.timeout)
              for a in DRIVES}
    diff = records.diff_fields(live.baseline, {a: records.raws_from(r) for a, r in before.items()}, ("io",))
    live.run.folder.write_json("baseline_before.json", {"declarations": declarations, "diff": diff})
    if declarations["verdict"] != "PASS" or any(d["status"] != "equal" for d in diff):
        raise probe.BenchError("I/O words differ from the baseline or declarations disagree; no write")


def frames(live):
    return live.stats()[STARTED]


# --------------------------------------------------------------------------
# Discovery cases (reads only)

def case_d1(live):
    before = live.stats()
    replies = []
    for a in DRIVES:
        r = live.call("probe", address=a)
        replies.append({"command": "probe", "address": a, "ok": r.get("ok") and r.get("outcome") == "success",
                        "answered": int(r["rx_hex"][:2], 16) if r.get("rx_hex") else None, "raw_model": r.get("raw_model")})
        r = live.call("read-identity", address=a)
        node = ((r.get("identity") or {}).get("raw_active_node") if isinstance(r.get("identity"), dict) else
                r.get("raw_active_node"))
        replies.append({"command": "read-identity", "address": a, "ok": bool(r.get("ok")), "answered": node})
    after = live.stats()
    delta = stats_delta(before, after)
    verdict, reason = d1_verdict(delta, replies)
    live.detail.update(replies=replies, stats_before=before, stats_after=after, delta=delta)
    live.run.verdict("D1", "drive-reported", verdict, reason, ["multi.json"])


def boundary(live, scan_tokens, cid):
    """One scan, its end, the refusals before the operator boundary, recover, restore, finish."""
    before = frames(live)
    begun = live.checked("discover", host_args=scan_tokens)
    ended = live.inspect()
    refusals = {}
    if cid == "D2":
        mark = frames(live)
        refusals["probe"] = live.call("probe", address=4).get("result")
        refusals["frames"] = frames(live) - mark
    refusals["restore"] = live.call("discover", host_args=("restore",)).get("result")
    recovered = live.call("recover")
    restore = live.call("discover", host_args=("restore",))
    for _ in range(40):
        if restore.get("ok") or restore.get("result") != "recovery_required":
            break
        time.sleep(0.05)
        restore = live.call("discover", host_args=("restore",))
    restored = live.inspect()
    finished = live.call("discover", host_args=("finish",))
    after_probe = live.call("probe", address=1)
    detail = {"tokens": list(scan_tokens), "frames_before": before, "begin_result": begun.get("result"),
              "ended": scan_summary(ended), "refusals": refusals, "recover": {k: recovered.get(k) for k in ("ok", "result", "outcome")},
              "restore": restore.get("result"), "restored": scan_summary(restored), "finish": finished.get("result"),
              "probe_after": after_probe.get("outcome"), "scan_end": ended, "scan_restored": restored}
    live.detail[cid] = detail
    problems = (d2_problems if cid == "D2" else d3_problems)(detail["ended"], refusals, detail["restored"])
    if not finished.get("ok") or after_probe.get("outcome") != "success":
        problems.append("finish %s, probe after %s" % (finished.get("result"), after_probe.get("outcome")))
    live.run.verdict(cid, "drive-reported", "PASS" if not problems else "FAIL",
                     "; ".join(problems) or "findings %s, silent %s, settles %s" % (
                         [f["address"] for f in detail["ended"]["findings"]], detail["ended"]["silent"],
                         detail["ended"]["settles"]), ["multi.json"])


# --------------------------------------------------------------------------
# Motion cases

def stale_members(reply: dict) -> list:
    """Members whose own read succeeded but whose state the application did not keep as current."""
    return [m["address"] for m in reply.get("members", [])
            if m.get("read_state") == "succeeded" and m.get("status") == "OK" and not m.get("state_known")]


def group_state(live):
    """One group read; a read that succeeded on the wire but left a member's
    state not current (seen after a group stop, G3) is recorded and followed by
    one more explicit read. Any other failure stops the case."""
    reply = live.call("group-read-state", timeout_s=max(live.timeout, 10))
    if not reply.get("ok") and stale_members(reply):
        live.detail.setdefault("stale_reads", []).append({"after": live.detail.get("last_label"), "members": stale_members(reply),
                                                          "reply": reply})
        reply = live.call("group-read-state", timeout_s=max(live.timeout, 10))
    if not reply.get("ok"):
        raise probe.BenchError("group-read-state failed: %s" % [(m["address"], m.get("read_state"), m.get("state_known"))
                                                                for m in reply.get("members", [])])
    return reply, {m["address"]: m["raw_position"] for m in reply["members"]}


def install_coordinates(live):
    """Member scale, then a caller-qualified reference at the read position and the origin there."""
    live.checked("group-set", host_args=tuple(str(a) for a in DRIVES), timeout_s=max(live.timeout, 10))
    group_state(live)
    for a in DRIVES:
        live.checked("group-axis", host_args=(str(a), "config", "set", "command", SCALE))
    origins = dict(reference(live))
    for a in DRIVES:
        live.checked("group-axis", host_args=(str(a), "origin", str(origins[a])))
    return origins


def reference(live, positions=None):
    """Reinstall each member's command-coordinate reference from a fresh group read."""
    if positions is None:
        _, positions = group_state(live)
    show = live.checked("group-show")
    for m in show["members"]:
        a = m["address"]
        live.checked("group-reference", host_args=(
            str(a), str(show["declaration"]), str(show["binding_generation"]), str(m["configuration_generation"]),
            str(m["configuration_operation_id"]), str(m["motion_operation_id"]), str(positions[a]), "0",
            "command-coordinate-qualified"))
    return positions


def plan(live, intents, mode):
    live.checked("group-plan-begin", host_args=("start", mode, "on-failure", "stop", "fast"))
    for a in DRIVES:
        kind, value = intents[a]
        frame = "motor" if kind == "absolute" else "native"
        live.checked("group-plan-add", host_args=(str(a), kind, str(value), "steps", frame, str(RPM), "configured"))


def submit(live, intents, mode, label, *, stop=None):
    """One plan from fresh reads; optional group stop ``stop`` STOP_AFTER_S after acceptance."""
    headroom = bench_switch.headroom_problem(live.console)
    if headroom:
        raise probe.BenchError(headroom)
    live.detail["last_label"] = label
    starts_at = reference(live)
    plan(live, intents, mode)
    mark = frames(live)
    handle = live.console.begin("group-plan-submit", host_args=(), timeout_s=max(live.timeout, 40))
    trial = {"label": label, "mode": mode, "intents": {str(a): list(v) for a, v in intents.items()},
             "start": starts_at, "accepted": handle.accepted}
    if not handle.accepted:
        terminal = live.console.wait(handle)
        trial.update(ok=False, state="refused", error=terminal.get("result"), frames=frames(live) - mark,
                     terminal=terminal, status=live.call("group-status"))  # Member freshness, waits, reservations.
        live.call("group-plan-clear")
        return trial
    stop_handle = None
    if stop:
        time.sleep(STOP_AFTER_S)
        stop_handle = live.console.begin("group-stop", host_args=(stop,), timeout_s=max(live.timeout, 10))
    terminal = live.console.wait(handle, release=True)
    trial["terminal"] = terminal
    if stop:
        # Audit F2: no later plan unless the stop result and fresh reads show every member stopped.
        trial["stop"] = live.console.wait(stop_handle, release=True) if stop_handle.accepted else {"ok": False, "result": "refused"}
        states = member_states(live)
        _, positions = group_state(live)
        trial["end"] = positions
        trial["stop_problems"] = stop_problems(trial["stop"], states, starts_at, positions)
        if trial["stop_problems"]:
            end_on_uncertain_stop(live, trial["stop_problems"])
    else:
        _, positions = group_state(live)
        trial["end"] = positions
    trial["ok"] = terminal.get("ok") is True
    trial["state"] = terminal.get("state")
    return trial


def member_states(live) -> dict:
    """Fresh read state of every member: {address: sample or None}."""
    return {a: live.read_state(a) for a in DRIVES}


def end_on_uncertain_stop(live, problems):
    """Prepared stop fast to every member, standstill from fresh reads, then end the case."""
    prepared_stops(live)
    try:
        for a in DRIVES:
            probe.observe_stopped(live.console, address=a, timeout_s=live.timeout)
    except probe.BenchError as error:
        raise probe.BenchError("standstill not shown after the prepared stops (%s): live motion ends" % error)
    raise probe.BenchError("group stop evidence uncertain: %s; prepared stops sent, case ends" % "; ".join(problems))


def run_g1(live, origins):
    trials = []
    targets = {a: ("absolute", v) for a, v in OFFSETS.items()}
    home = {a: ("absolute", 0) for a in DRIVES}
    for mode in ("staged", "sequential"):
        for index in range(live.args.g1_runs):
            intents = targets if index % 2 == 0 else home
            t = submit(live, intents, mode, "G1 %s %d" % (mode, index + 1))
            native = member_targets(origins, intents, t["start"])
            t["targets"] = native
            if t.get("ok"):
                t["errors"] = endpoint_errors(native, t["end"])
            trials.append(t)
            if not t.get("ok"):
                break
    return trials


def run_g2(live, origins):
    """Member 1's absolute target is its read position while 2 and 4 would move: refused before any frame."""
    _, positions = group_state(live)
    intents = {1: ("absolute", positions[1] - origins[1]), 2: ("absolute", OFFSETS[2]), 4: ("absolute", OFFSETS[4])}
    return submit(live, intents, "staged", "G2")


def run_g3(live, origins):
    trials = []
    target = {a: ("absolute", v) for a, v in LONG.items()}
    for policy in ("normal", "fast"):
        stopped = submit(live, target, "staged", "G3 %s stopped" % policy, stop=policy)
        native = member_targets(origins, target, stopped["start"])
        stopped["short"] = {a: stopped["end"][a] - native[a] for a in DRIVES} if stopped.get("end") else None
        trials.append(stopped)
        resumed = submit(live, target, "staged", "G3 %s resumed" % policy)
        resumed["targets"] = native
        if resumed.get("ok"):
            resumed["errors"] = endpoint_errors(native, resumed["end"])
        # Diagnostic after a refusal before any frame: new admissions 1 s apart,
        # each from fresh reads, measure how long the member stays refused.
        # The first refusal keeps the case FAIL.
        later = []
        latest = resumed
        # Each repeat only on the latest attempt's evidence: refused with no frame
        # and every member read stopped since.
        while latest.get("state") == "refused" and not latest.get("frames") and len(later) < G3_LATER:
            time.sleep(1.0)
            states = member_states(live)
            if not all_stopped(states):
                end_on_uncertain_stop(live, ["a member is not reported stopped before a repeat"])
            latest = submit(live, target, "staged", "G3 %s later %d" % (policy, len(later) + 1))
            later.append(latest)
        if later:
            resumed["later"] = later
        trials.append(resumed)
        back = submit(live, {a: ("absolute", 0) for a in DRIVES}, "staged", "G3 %s return" % policy)
        back["targets"] = dict(origins)
        if back.get("ok"):
            back["errors"] = endpoint_errors(origins, back["end"])
        trials.append(back)
    return trials


def run_g4(live, origins):
    trials = []
    back = {1: ("absolute", 0), 2: ("relative", -MIXED[2][1]), 4: ("absolute", 0)}
    for label, intents in (("G4 mixed", MIXED), ("G4 return", back)):
        t = submit(live, intents, "sequential", label)
        native = member_targets(origins, intents, t["start"])
        t["targets"] = native
        if t.get("ok"):
            t["errors"] = endpoint_errors(native, t["end"])
        trials.append(t)
    return trials


def selected_ready(live, address):
    """Selected drive: config and state, a profile read and the stated start-speed relation."""
    records.use_drive(live.console, address, timeout_s=live.timeout)
    live.checked("read-config", address=address)
    live.read_state(address)
    profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
    word = str(profile["current"][0])
    live.call("motion-profile-start-speed-relation", host_args=(word, word))
    return profile


def selected_move(live, address, steps, *, stop_after=None):
    selected_ready(live, address)
    live.checked("io", driver_args=("read",), address=address)
    start = stops.position(live.read_state(address))
    handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", str(RPM), "configured"),
                                address=address, timeout_s=max(live.timeout, steps / (RPM * 1000 / 60) + 10))
    out = {"steps": steps, "start": start, "accepted": handle.accepted}
    if stop_after and handle.accepted:
        time.sleep(stop_after)
        out["stop"] = live.call("stop", stop_policy="fast", address=address).get("outcome")
    terminal = live.console.wait(handle, release=True)
    settled = probe.observe_stopped(live.console, address=address, timeout_s=live.timeout)
    end = stops.position(live.read_state(address))
    out.update(outcome=terminal.get("outcome"), ok=terminal.get("ok"), end=end,
               travel=None if end is None or start is None else end - start, settled=settled.get("ok"))
    live.checked("motion-profile", host_args=("forget",))
    return out


def run_r1(live):
    rows = []
    for a in DRIVES:
        row = {"address": a}
        row["probe"] = live.call("probe", address=a).get("outcome")
        row["identity"] = live.call("read-identity", address=a).get("ok")
        row["config"] = live.call("read-config", address=a).get("ok")
        row["state"] = live.call("read-state", address=a).get("ok")
        row["move"] = selected_move(live, a, R1_MOVE)
        row["stop_fast"] = selected_move(live, a, R1_STOP_MOVE, stop_after=1.0)
        live.checked("group-set", host_args=(str(a),), timeout_s=max(live.timeout, 10))
        _, before = group_state(live)
        handle = live.console.begin("group-move", host_args=("relative", str(R1_GROUP), "steps", "native", str(RPM),
                                                             "configured", "start", "staged", "on-failure", "stop", "fast"),
                                    timeout_s=max(live.timeout, 20))
        terminal = live.console.wait(handle, release=True) if handle.accepted else {"ok": False, "result": "refused"}
        _, after = group_state(live)
        row["group"] = {"ok": terminal.get("ok"), "state": terminal.get("state"), "travel": after[a] - before[a]}
        live.call("group-clear")
        rows.append(row)
    return rows


def r1_problems(rows):
    problems = []
    for r in rows:
        a = r["address"]
        if r["probe"] != "success" or not (r["identity"] and r["config"] and r["state"]):
            problems.append("%d reads %s/%s/%s/%s" % (a, r["probe"], r["identity"], r["config"], r["state"]))
        m = r["move"]
        if not m["ok"] or m["travel"] is None or abs(m["travel"] - R1_MOVE) > 2:
            problems.append("%d move %s travel %s" % (a, m["outcome"], m["travel"]))
        s = r["stop_fast"]
        if s["travel"] is None or not 0 < s["travel"] < R1_STOP_MOVE or not s["settled"]:
            problems.append("%d stop fast travel %s settled %s" % (a, s["travel"], s["settled"]))
        g = r["group"]
        if not g["ok"] or abs(g["travel"] - R1_GROUP) > 2:
            problems.append("%d group %s travel %s" % (a, g["state"], g["travel"]))
    return problems


def case_motion(live):
    selected = selected_cases(live.args)
    live.detail["motion"] = True
    origins = install_coordinates(live)
    live.detail["origins"] = origins
    if "G1" in selected:
        g1 = run_g1(live, origins)
        live.detail["G1"] = g1
        live.run.verdict("G1", "drive-reported", *g_verdict(g1, live.args.tolerance), ["multi.json"])
    if "G2" in selected:
        run_g2_case(live, origins)
    if "G3" in selected:
        run_g3_case(live, origins)
    if "G4" in selected:
        g4 = run_g4(live, origins)
        live.detail["G4"] = g4
        live.run.verdict("G4", "drive-reported", *g_verdict(g4, live.args.tolerance), ["multi.json"])
    live.call("group-clear")
    if "R1" in selected:
        rows = run_r1(live)
        live.detail["R1"] = rows
        problems = r1_problems(rows)
        live.run.verdict("R1", "drive-reported", "PASS" if not problems else "FAIL", "; ".join(problems) or
                         "reads, +%d, stop fast, group +%d on each drive" % (R1_MOVE, R1_GROUP), ["multi.json"])
    live.detail["motion"] = False


def run_g2_case(live, origins):
    g2 = run_g2(live, origins)
    live.detail["G2"] = g2
    refused = g2.get("state") == "refused" and g2.get("frames") == 0
    live.run.verdict("G2", "sent", "PASS" if refused else "FAIL",
                     "refused %s with %s frames" % (g2.get("error"), g2.get("frames")) if refused else
                     "plan %s (%s)" % (g2.get("state"), g2.get("error")), ["multi.json"])
    if g2.get("accepted") and g2.get("ok"):  # An admitted plan moved 2 and 4: bring them back first.
        submit(live, {a: ("absolute", 0) for a in DRIVES}, "staged", "G2 return")


def run_g3_case(live, origins):
    g3 = run_g3(live, origins)
    live.detail["G3"] = g3
    stopped = [t for t in g3 if t["label"].endswith("stopped")]
    others = [t for t in g3 if not t["label"].endswith("stopped")]
    verdict, reason = g_verdict(others, live.args.tolerance)
    short_ok = all(t.get("short") and all(abs(v) > 100 for v in t["short"].values()) for t in stopped)
    live.run.verdict("G3", "drive-reported", verdict if short_ok else "FAIL",
                     reason + "; stopped short by %s" % [t.get("short") for t in stopped], ["multi.json"])


# --------------------------------------------------------------------------
# Scenario record and main

def selected_cases(args):
    cases = GROUPS[args.group]
    return [c for c in cases if not args.cases or c in args.cases]


def write_list(group):
    if group != "MOTION":
        return []
    keys = ("profile.acceleration", "profile.deceleration", "profile.speed", "profile.target")
    return [{"drive": a, "key": k, "index": None, "candidate": None} for a in DRIVES for k in keys]


def scenario_record(args, image, commit, baseline):
    cases = [{"id": c, "required": True, "assertion": CASES[c]["assertion"], "required_levels": CASES[c]["levels"]}
             for c in selected_cases(args)]
    entries = write_list(args.group)
    plan_ = records.restoration_plan(baseline, entries, args.image_level, starts.facts())
    commands = {
        "D1": ["debug raw; stats; per drive 1, 2, 4: probe, read identity; stats"],
        "D2": ["stats; discover %s; discover inspect until INTERLOCK; stats; probe 4 (refused); stats; discover restore "
               "(refused); recover; discover restore; discover inspect; discover finish; probe 1" % " ".join(PAST_SILENT)],
        "D3": ["stats; discover %s; discover inspect until INTERLOCK; discover restore (refused); recover; discover "
               "restore; discover inspect; discover finish; probe 1" % " ".join(SCAN)],
        "MOTION": ["group set 1 2 4; group read state; group axis N config set command 1000; group read state; group "
                   "show; group reference N ... command-coordinate-qualified; group axis N origin POSITION",
                   "per plan: group read state, group show, group reference per member, group plan begin start MODE "
                   "on-failure stop fast, group plan add N absolute V steps motor 60 configured (or relative V steps "
                   "native), group plan submit, [group stop normal|fast 1.5 s in], group read state",
                   "R1 per drive: probe, read identity/config/state; move relative 1000; move relative 3000 and stop fast "
                   "1 s in; group set N; group move relative 500 steps native 60 configured start staged on-failure stop "
                   "fast; group clear",
                   "restore: stop fast 1, 2, 4; motion-profile stage of the baseline words per drive, read back"],
    }[args.group]
    bounds = {"exposure": EXPOSURE[args.group], "rpm": RPM, "offsets": OFFSETS, "long": LONG,
              "mixed": {str(a): list(v) for a, v in MIXED.items()}, "g1_runs_per_mode": args.g1_runs,
              "stop_after_s": STOP_AFTER_S, "r1": [R1_MOVE, R1_STOP_MOVE, R1_GROUP], "tolerance": args.tolerance}
    return dict(image=image, commit=commit, port=args.port,
                drives=[{"address": a, "required_enabled": True} for a in DRIVES], terminals=[],
                commands=commands, cases=cases, running_members=[], write_list=entries, bounds=bounds,
                prepared_stop=["group stop fast; stop fast 1, 2, 4 (every bench output drops first)"],
                restoration_plan=plan_, left_changed=[],
                expected_evidence={c: "multi.json and the session logs" for c in selected_cases(args)},
                abort_conditions=["an alarm in any read", "a refused, failed or uncertain write (no replay)",
                                  "a member that does not complete (failure policy stop fast)",
                                  "console framing or evidence failure"],
                cleanup=["prepared stops", "restoration plan, read back", "group clear", "bench outputs off",
                         "ending reads", "close COM13"],
                max_age_ms=records.MAX_AGE_MS, facts=starts.facts(), image_level=args.image_level)


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", default="COM13")
    parser.add_argument("--group", choices=tuple(GROUPS), required=True)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True, help="running image (SHA-256 recorded)")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--image-level", default="19")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--cases", nargs="+", choices=GROUPS["MOTION"], help="MOTION: only these cases (default all)")
    parser.add_argument("--g1-runs", type=int, default=G1_RUNS,
                        help="G1 plans per start mode, alternating target and origin (even, 2..%d)" % G1_RUNS)
    parser.add_argument("--tolerance", type=int, default=2,
                        help="endpoint tolerance in increments (drive 4's recorded feedback offset is 1-2)")
    args = parser.parse_args(argv)
    if args.port != records.FIXTURE["board"]["port"]:
        parser.error("the wired bench is on %s" % records.FIXTURE["board"]["port"])
    if not 2 <= args.g1_runs <= G1_RUNS or args.g1_runs % 2:
        parser.error("--g1-runs is even, 2..%d, so every member ends at its origin" % G1_RUNS)
    if args.cases and args.group != "MOTION":
        parser.error("--cases selects MOTION cases")
    return args


def main(argv=None):
    args = arguments(argv)
    folder = records.RunFolder.from_path(args.run_dir)
    if folder.prompt != "19":
        raise SystemExit("multi-drive run folders are wired bench 19 (YYYY-MM-DD_wb19_slug)")
    image = {"path": args.firmware.as_posix(), "sha256": records.sha256_file(args.firmware)}
    baseline = records.load_baseline()
    record = scenario_record(args, image, args.commit, baseline)
    folder.create()
    scenario = records.write_scenario(folder, **json.loads(json.dumps(record)))
    run = _Run(scenario, folder)
    live = Live(run, baseline, args)
    error = None

    def work(console):
        baseline_before(live)
        try:
            if args.group == "D1":
                case_d1(live)
            elif args.group in ("D2", "D3"):
                boundary(live, PAST_SILENT if args.group == "D2" else SCAN, args.group)
            else:
                case_motion(live)
        finally:
            if args.group == "MOTION" and console.synchronized:
                console.command("group-clear", timeout_s=live.timeout)
                _stop_restore(console, live, run, scenario["restoration_plan"], live.timeout)
            _finish(console, run, live.timeout)

    try:
        records.check_board(args.port)
        session(folder, live, work, debug="raw" if args.group == "D1" else None)
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append("run stopped: %s" % caught)
    folder.write_json("multi.json", {"group": args.group, "detail": live.detail, "events": live.events[-800:],
                                     "samples": live.samples[-2000:], "error": str(error) if error else None})
    for name in ("baseline_before.json", "ending_reads.json"):
        if not (folder / name).exists():
            folder.write_json(name, {"recorded": False, "reason": "no session reached it: %s" % error})
    cases = []
    for case in scenario["cases"]:
        state = run.cases[case["id"]]
        for level in case["required_levels"]:
            if level not in state["verdicts"]:
                run.verdict(case["id"], level, "NOT RUN", "not reached: %s" % (error or "no verdict"))
        cases.append({**case, "verdicts": state["verdicts"], "reason": state["reason"],
                      "evidence": [p for p in state["evidence"] + ["multi.json"] if (folder / p).exists()]})
    for entry in scenario["write_list"]:
        if (entry["drive"], entry["key"], entry.get("index")) not in run.fields:
            value = (records.baseline_field(baseline, entry["drive"], entry["key"]) or {}).get("value")
            window = run.fields.get((entry["drive"], "profile.acceleration", None))
            if window and window["restored"]:
                run.field(entry, baseline=value, ending=value, restored=True,
                          reason="restored and read back with the drive's 0x0021/5 stage window")
    records.write_result(folder, commit=args.commit, image=image, cases=cases,
                         restoration={"outputs_off": run.outputs_off, "fields": list(run.fields.values()),
                                      "drives": run.drives, "port_closed": True},
                         deviations=run.deviations)
    report = records.check_run(folder.path)
    print(json.dumps({"cases": {c["id"]: c["verdicts"] for c in cases}, "check": report}, indent=1))
    return records.check_exit(report)


if __name__ == "__main__":
    sys.exit(main())
