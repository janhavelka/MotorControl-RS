"""Wired bench 09: input-started and serially interrupted moves (function 7).

Prompt 09 qualifies prompt 08's input-start route on the bench: single starts
on drive 4 (X0 = 7, ``mosfet1``) in both ``0x0050`` modes, the held level, the
shortest start pulse, stops of input-started motion (serial, group, X3),
disarm, shared starts of drives 1, 2 and 4, the start delay ``mosfet1`` ON ->
drive 4 Y0 running (``aux1``, read inverted), and the serial stored-profile
interruption (``position interrupt``, ``0x0027`` bit 3).

Every case row starts from a named state (``STATES``) entered by its write
list through the ordinary setters with stated facts and read back, then the
documented fresh-read sequence. Every ON goes through the firmware's own
trigger (``arm trigger``) or ``bench_records.bench_on`` (D14 interlock) after
the shared line check against the named state. Nothing is retried or
replayed; continued motion gets the prepared stop at once.

The analysis functions are pure and unit-tested with recorded replies. They
keep the evidence levels apart: the session result is drive-reported (its
samples), source and ``aux1`` edges are controller-timestamped GPIO and
interrupt times, and an interval is a bracket on the controller clock.
"""
import statistics
import time

import bench_motion as motion
import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stops

OWNER = records.FIXTURE["y0_owner"]
DRIVES = list(records.FIXTURE["input_drives"])
RUNNING, IN_POSITION, ALARM, RELEASED = 4, 1, 8, 16
POST_RELEASE_S = 2.0
START_OUTPUT, START_TERMINAL = "mosfet1", "x0"
STOP_OUTPUT, STOP_TERMINAL = "mosfet2", "x3"
FINITE_RPM = 60
# Prompt 09 bounds: moves at most 3000 steps at 60 rpm, velocity 30 rpm for at
# most 1000 ms, holds at most 5000 ms.
MAX_STEPS, MAX_HOLD_MS = 3000, 5000

# Named states (prompt 09 bench procedure step 3). x0/x3: input function per
# drive; mode: 0x0050 per drive (absent: left as read); y0: drive 4 Y0;
# members: drives the case may start through function 7.
_NA = {"x0": {1: 0, 2: 0, 4: 7}, "x3": {1: 0, 2: 0, 4: 0}, "y0": 2, "group": False, "members": [OWNER]}
STATES = {
    "NA-abs": dict(_NA, mode={4: 1}),
    "NA-rel": dict(_NA, mode={4: 0}),
    "NA-X3": dict(_NA, mode={4: 1}, x3={1: 0, 2: 0, 4: 5}),
    "NB": {"x0": {1: 7, 2: 7, 4: 7}, "x3": {1: 0, 2: 0, 4: 0}, "mode": {1: 1, 2: 1, 4: 1}, "y0": 2, "group": True,
           "members": list(DRIVES)},
    "NB-X3": {"x0": {1: 7, 2: 7, 4: 7}, "x3": {1: 5, 2: 5, 4: 5}, "mode": {1: 1, 2: 1, 4: 1}, "y0": 2, "group": True,
              "members": list(DRIVES)},
    "NC": {"x0": {1: 0, 2: 0, 4: 1}, "x3": {1: 0, 2: 0, 4: 0}, "mode": {}, "y0": 2, "group": False, "members": []},
    # The baseline I/O words: the short regression runs with no case assignment.
    "BASE": {"x0": {1: 1, 2: 1, 4: 1}, "x3": {1: 0, 2: 0, 4: 0}, "mode": {1: 0, 2: 0, 4: 0}, "y0": 0, "group": False,
             "members": []},
    "NC-Y4": {"x0": {1: 0, 2: 0, 4: 1}, "x3": {1: 0, 2: 0, 4: 0}, "mode": {}, "y0": 4, "group": False, "members": []},
}
# Facts stated for every state write and its restoration (scenario record).
ASSIGN_FACTS = {"x0": ["triggers-inhibited"], "x3": ["triggers-inhibited", "protection-removal"]}
CONFIGURE_FACTS = {"x0": ["triggers-inhibited"], "x3": ["triggers-inhibited"]}
Y0_FACTS = ["indicator", "either"]

# Case table (prompt 09 step 5). family selects the scenario; kind the runner.
CASES = {
    "A1": dict(family="single", state="NA-abs", kind="start", form="absolute", steps=2000, width_ms=50, runs=20,
               levels=["controller-timestamped", "drive-reported"]),
    "A2": dict(family="single", state="NA-rel", kind="start", form="relative", steps=500, width_ms=50, runs=10,
               levels=["controller-timestamped", "drive-reported"]),
    "A3": dict(family="single", state="NA-rel", kind="width", steps=200, widths=(5, 10, 12, 15, 20, 30, 50, 200),
               runs=3, levels=["controller-timestamped", "drive-reported"]),
    "A4": dict(family="single", state="NA-rel", kind="held", form="relative", steps=500, width_ms=2000, runs=1,
               levels=["controller-timestamped", "drive-reported"]),
    "A5": dict(family="single", state="NA-abs", kind="held", form="absolute", steps=1000, width_ms=2000, runs=1,
               levels=["controller-timestamped", "drive-reported"]),
    "A6": dict(family="single", state="NA-abs", states=("NA-abs", "NA-rel"), kind="serial-stop", steps=3000,
               width_ms=50, runs=3, policies=("normal", "fast"), stop_at_s=1.0,
               levels=["controller-timestamped", "drive-reported"]),
    # Diagnostic (wired bench 09, B2 SECOND_START after stops): back-to-back
    # read state of drive 4 for 1.5 s from the serial stop, so RUNNING, the
    # speed word and the position after the stop are visible sample by sample.
    "A6d": dict(family="single", state="NA-abs", states=("NA-abs",), kind="serial-stop", steps=3000, width_ms=50,
                runs=2, policies=("fast", "normal"), stop_at_s=1.0, diagnose_s=1.5, levels=["drive-reported"]),
    "A7": dict(family="single", state="NA-X3", kind="input-stop", steps=3000, width_ms=50, runs=5, stop_hold_ms=200,
               stop_at_s=1.0, levels=["controller-timestamped", "drive-reported"]),
    "A8": dict(family="single", state="NA-rel", kind="held-stop", steps=3000, width_ms=3000, runs=1, stop_at_s=1.0,
               levels=["controller-timestamped"]),
    "A9": dict(family="single", state="NA-abs", kind="disarm", steps=1000, width_ms=50, runs=1,
               levels=["sent", "drive-reported"]),
    "A10": dict(family="single", state="NA-abs", kind="disarm-inhibit", steps=1000, width_ms=50, runs=1,
                levels=["drive-reported"]),
    "A11": dict(family="single", state="NA-rel", kind="fixed-length", steps=300, rpm=30, duration_ms=1000,
                width_ms=50, at_s=0.3, runs=3, levels=["drive-reported"]),
    "B1": dict(family="shared", state="NB", kind="group-start", steps=2000, width_ms=50, runs=50,
               levels=["controller-timestamped", "drive-reported"]),
    "B2": dict(family="shared", state="NB", kind="group-stop", steps=3000, width_ms=50, runs=5,
               policies=("normal", "fast"), stop_at_s=1.0, levels=["drive-reported"]),
    # Diagnostic (wired bench 09, B2 on image C: drives 1 and 2 still reported a
    # further change after standstill): back-to-back read state of drives 1 and 2
    # for 2 s from the group stop.
    "B2d": dict(family="shared", state="NB", kind="group-stop", steps=3000, width_ms=50, runs=2, policies=("fast", "normal"),
                stop_at_s=1.0, diagnose_s=2.0, diagnose_members=(1, 2), levels=["drive-reported"]),
    "B3": dict(family="shared", state="NB-X3", kind="group-input-stop", steps=3000, width_ms=50, runs=3,
               stop_hold_ms=200, stop_at_s=1.0, levels=["controller-timestamped", "drive-reported"]),
    "I1": dict(family="interrupt", state="NC", kind="interrupt", form="relative", steps=3000, at_s=1.0, runs=10,
               levels=["drive-reported"]),
    "I2": dict(family="interrupt", state="NC", kind="interrupt", form="absolute", steps=3000, at_s=1.0, runs=5,
               levels=["drive-reported"]),
    "I3": dict(family="interrupt", state="NC", kind="interrupt", form="relative", steps=1000,
               at_s=(0.85, 0.95, 1.05), runs=3, levels=["sent", "drive-reported"]),
    "I4": dict(family="interrupt", state="NC-Y4", kind="interrupt", forms=("relative", "absolute"), steps=3000,
               at_s=1.0, runs=5, levels=["controller-timestamped", "drive-reported"]),
    # Short regression (prompt 09 step 7): quick reads of 1, 2, 4, one serial
    # finite move and stop fast on each, one staged group move.
    "R": dict(family="shared", state="BASE", kind="regression", steps=1000, stop_at_s=0.5, levels=["drive-reported"]),
    # Restoration only: the baseline I/O state, no motion; the run's restoration
    # restores the 0x0021/5 words of 1, 2, 4 (the last run of a controller session).
    "Z": dict(family="shared", state="BASE", kind="restore-only", levels=["read back"]),
    # Diagnostic (wired bench 09): final feedback of plain serial absolute moves
    # against their target on each drive, to separate a drive positioning or
    # feedback offset from the input start (B1 endpoints differed by 1-2).
    "E1": dict(family="interrupt", state="NC", kind="serial-endpoint", steps=2000, runs=6, levels=["drive-reported"]),
}
FAMILIES = {"input-start-single": "single", "input-start-shared": "shared", "position-interrupt": "interrupt"}


def case_states(case: dict) -> list:
    return list(case.get("states") or [case["state"]])


def expected_io(state: dict, address: int, current: list | None = None) -> list:
    """The nine I/O words a drive reads in ``state``; X1/X2 and the rest keep
    the baseline functions (2, 3) and zero polarity."""
    words = list(current) if current else [0, 1, 2, 3, 0, 0, 0, 0, 0]
    words[0], words[2], words[3] = 0, 2, 3
    words[1], words[4] = state["x0"][address], state["x3"][address]
    words[5], words[7], words[8] = 0, 0, 0
    words[6] = state["y0"] if address == OWNER else 0
    return words


def write_list(cases: list) -> list:
    """Every field the selected cases' states change, with the most demanding candidate."""
    out = {}
    def add(address, key, value):
        ident = (address, key)
        if ident not in out:
            out[ident] = {"drive": address, "key": key, "index": None, "candidate": value}
        elif key.startswith("io.x") and value >= 4:
            out[ident]["candidate"] = max(out[ident]["candidate"], value)
    for cid in cases:
        case = CASES[cid]
        for name in case_states(case):
            state = STATES[name]
            for address in DRIVES:
                add(address, "io.x0", state["x0"][address])
                add(address, "io.x3", state["x3"][address])
                if address in state["mode"]:
                    add(address, "drive.position-mode", state["mode"][address])
            add(OWNER, "io.y0", state["y0"])
        moving = DRIVES if case["family"] == "shared" or case["kind"] == "serial-endpoint" else [OWNER]
        for address in moving:
            for key in stops.PROFILE_KEYS:
                add(address, key, None)
        if case["kind"] == "fixed-length":
            for key in records.SNAPSHOT_ROUTES["velocity"]["keys"]:
                add(OWNER, key, None)
    return list(out.values())


def facts() -> dict:
    """Facts the scenario record states per drive and purpose (static for the set)."""
    out = {}
    for address in DRIVES:
        assign = {t: list(f) for t, f in ASSIGN_FACTS.items()}
        if address == OWNER:
            assign["y0"] = list(Y0_FACTS)
        out[str(address)] = {"assign": assign, "configure": {t: list(f) for t, f in CONFIGURE_FACTS.items()}}
    return out


# --------------------------------------------------------------------------
# Analysis (pure; unit-tested)

def stats(values: list, scale: float = 1.0) -> dict | None:
    values = [v for v in values if v is not None]
    if not values:
        return None
    return {"n": len(values), "min": min(values) / scale, "median": statistics.median(values) / scale,
            "max": max(values) / scale}


def running_on_edges(edges: list, after_us: int, before_us: int | None = None) -> list:
    """drive 4 Y0 running-on edges (``aux1`` read inverted: Y0 on -> inactive)."""
    return [e for e in sorted(edges, key=lambda e: e["at_us"])
            if e["at_us"] >= after_us and not e["active"] and (before_us is None or e["at_us"] < before_us)]


def start_delay_us(edges: list, on_us: int, limit_us: int = 1000000) -> int | None:
    """D = first Y0 running-on edge after the source ON minus the ON, within ``limit_us``."""
    found = running_on_edges(edges, on_us, on_us + limit_us)
    return found[0]["at_us"] - on_us if found else None


def member_result(session: dict, address: int) -> dict:
    member = next((m for m in session.get("members", []) if m.get("address") == address), {})
    return member.get("result") or {"known": False}


def start_trial(session: dict, edges: list, owner: int = OWNER) -> dict:
    """Per-member summary of one armed start: outcome, missing witnesses,
    first-change bracket relative to the ON, position change against the
    expectation, spacing; D for the Y0 owner; second starts from Y0 edges."""
    source = session.get("source") or {}
    on_us, off_us = source.get("on_us") or 0, source.get("off_us") or 0
    out = {"on_us": on_us, "off_us": off_us, "source": source.get("state"), "members": {},
           "isolation_rows": [(r.get("address"), r.get("row")) for r in (session.get("isolation") or {}).get("rows", [])],
           "admitted": (session.get("isolation") or {}).get("admitted"), "error": session.get("error"),
           "capacity": session.get("capacity")}
    for member in session.get("members", []):
        r = member.get("result") or {"known": False}
        a = member.get("address")
        entry = {"known": r.get("known"), "outcome": r.get("outcome"), "missing": r.get("missing"),
                 "further": r.get("further"), "completion": r.get("completion"), "running_observed": r.get("running_observed"),
                 "position_change": r.get("position_change"), "expected_change": r.get("expected_change"),
                 "position_matches": r.get("position_matches"), "samples": r.get("samples"),
                 "lost": r.get("lost_samples"), "late": r.get("late_samples"), "spacing_us": r.get("spacing_us"),
                 "stop_id": r.get("stop_id"), "stopped_after_stop": r.get("stopped_after_stop")}
        if r.get("known") and r.get("change_observed") and on_us:
            entry["bracket_us"] = [r["change_start_us"] - on_us, r["change_end_us"] - on_us]
        if r.get("known") and r.get("expected_known") and type(r.get("position_change")) is int:
            # Final feedback against the staged endpoint or displacement, no tolerance.
            entry["endpoint_error"] = r["position_change"] - r["expected_change"]
        out["members"][a] = entry
    if on_us:
        out["d_us"] = start_delay_us(edges, on_us)
        hold_end = off_us or on_us
        out["y0_on_during_hold"] = len(running_on_edges(edges, on_us, hold_end + 1))
        out["y0_on_after_release"] = len(running_on_edges(edges, hold_end + 1, hold_end + int(POST_RELEASE_S * 1e6) + 1))
    return out


def spread(trial: dict) -> dict | None:
    """Bounds of the first-change spread between members of one shared start:
    each member's start lies in its bracket [s, e]; the spread is at most
    max(e) - min(s) and at least max(0, max(s) - min(e)). Sampling bounds it;
    it is not a physical skew measurement."""
    brackets = [m["bracket_us"] for m in trial["members"].values() if m.get("bracket_us")]
    if len(brackets) < 2 or len(brackets) != len(trial["members"]):
        return None
    upper = max(b[1] for b in brackets) - min(b[0] for b in brackets)
    lower = max(0, max(b[0] for b in brackets) - min(b[1] for b in brackets))
    return {"lower_us": lower, "upper_us": upper}


def summarize_starts(trials: list) -> dict:
    """D, per-member first-change brackets, position change per start, outcome
    counts and the achieved sample spacing over a set of start trials."""
    outcomes, per_member, d = {}, {}, []
    spreads = []
    for t in trials:
        a = t.get("analysis")
        if not a:
            continue
        if a.get("d_us") is not None:
            d.append(a["d_us"])
        s = spread(a)
        if s:
            spreads.append(s)
        for address, m in a["members"].items():
            outcomes.setdefault(str(address), {})
            outcomes[str(address)][m.get("outcome")] = outcomes[str(address)].get(m.get("outcome"), 0) + 1
            pm = per_member.setdefault(str(address), {"bracket_start": [], "bracket_end": [], "change": [], "error": [],
                                                      "spacing_min": [], "spacing_median": [], "spacing_max": [],
                                                      "late": 0, "lost": 0, "samples": 0})
            if m.get("bracket_us"):
                pm["bracket_start"].append(m["bracket_us"][0]); pm["bracket_end"].append(m["bracket_us"][1])
            pm["change"].append(m.get("position_change"))
            pm["error"].append(m.get("endpoint_error"))
            sp = m.get("spacing_us") or {}
            if sp.get("count"):
                pm["spacing_min"].append(sp["min"]); pm["spacing_median"].append(sp["median"]); pm["spacing_max"].append(sp["max"])
            pm["late"] += m.get("late") or 0
            pm["lost"] += m.get("lost") or 0
            pm["samples"] += m.get("samples") or 0
    members = {}
    for address, pm in per_member.items():
        members[address] = {"bracket_start_ms": stats(pm["bracket_start"], 1000.0),
                            "bracket_end_ms": stats(pm["bracket_end"], 1000.0),
                            "changes": sorted(set(c for c in pm["change"] if c is not None)),
                            "endpoint_errors": {str(e): pm["error"].count(e) for e in sorted(set(
                                e for e in pm["error"] if e is not None))},
                            "spacing_ms": {"min": stats(pm["spacing_min"], 1000.0), "median": stats(pm["spacing_median"], 1000.0),
                                           "max": stats(pm["spacing_max"], 1000.0)},
                            "late_samples": pm["late"], "lost_samples": pm["lost"], "samples": pm["samples"]}
    return {"trials": len(trials), "outcomes": outcomes, "d_ms": stats(d, 1000.0), "members": members,
            "spread_upper_ms": stats([s["upper_us"] for s in spreads], 1000.0),
            "spread_lower_ms": stats([s["lower_us"] for s in spreads], 1000.0)}


def start_verdicts(trials: list, members: list, tolerance: int = 0) -> dict:
    """A1/A2/B1 per evidence level.

    controller-timestamped: one source ON after an admitted isolation decision,
    the first-change bracket ending after the ON, and (Y0 owner) the running-on
    edge D. drive-reported: every member ``attributed`` with no further start
    and its final feedback equal to the staged endpoint or displacement, within
    ``tolerance`` increments (prompt 09: none; the wired soak passes its
    measured steady offset). A second start, unattributed motion or no start is
    FAIL at both levels; a missing witness is indeterminate."""
    out = {}
    if not trials:
        return {lv: ("NOT RUN", "no trial") for lv in ("controller-timestamped", "drive-reported")}
    fails, open_, endpoint = [], [], []
    controller_open = []
    for t in trials:
        if t.get("refused"):
            open_.append("trial %s refused before the ON: %s" % (t.get("index"), t["refused"]))
            continue
        a = t.get("analysis")
        if not a:
            open_.append("trial %s has no session result" % t.get("index"))
            continue
        if t.get("continued_motion"):
            fails.append("trial %s: motion continued after the session; prepared stop sent" % t.get("index"))
        if OWNER in members and a.get("d_us") is None:
            controller_open.append("trial %s: no Y0 running-on edge within 1 s of the ON" % t.get("index"))
        for address in members:
            m = a["members"].get(address) or a["members"].get(str(address)) or {}
            outcome = m.get("outcome")
            if outcome == "attributed":
                if m.get("further") == "seen":
                    fails.append("trial %s drive %s: further start" % (t.get("index"), address))
                error = m.get("endpoint_error")
                if (error is None or abs(error) > tolerance) if tolerance else \
                        (error not in (0, None) or not m.get("position_matches")):
                    endpoint.append("trial %s drive %s: change %s, expected %s" % (
                        t.get("index"), address, m.get("position_change"), m.get("expected_change")))
                if not m.get("bracket_us") or m["bracket_us"][1] <= 0:
                    controller_open.append("trial %s drive %s: no change bracket after the ON" % (t.get("index"), address))
            elif outcome in ("indeterminate", "deadline"):
                open_.append("trial %s drive %s: %s %s" % (t.get("index"), address, outcome, m.get("missing")))
            else:
                fails.append("trial %s drive %s: %s %s" % (t.get("index"), address, outcome, m.get("missing")))
    if fails:
        return {lv: ("FAIL", "; ".join(fails)) for lv in ("controller-timestamped", "drive-reported")}
    if open_:
        return {lv: ("indeterminate", "; ".join(open_)) for lv in ("controller-timestamped", "drive-reported")}
    out["controller-timestamped"] = ("indeterminate", "; ".join(controller_open)) if controller_open else ("PASS", None)
    out["drive-reported"] = ("FAIL", "attributed, single start; final feedback differs from the staged target "
                             "(%s): " % ("tolerance %d increments" % tolerance if tolerance else "no tolerance") +
                             "; ".join(endpoint)) if endpoint else ("PASS", None)
    return out


def count_starts(samples: list, before: int | None, after: int | None, steps: int, y0_edges: int | None = None) -> dict:
    """A3: starts of one unarmed pulse with a +steps relative move staged.

    ``starts`` counts RUNNING episodes in the samples (drive-reported); when
    ``y0_edges`` (Y0 running-on edges, controller-timestamped) is given the two
    must agree, else the count is unclear. The position change is reported, not
    judged: drive 4's feedback alternates by one increment around the commanded
    position (wired bench 09, A2/A3: +201/+199 pairs summing exactly)."""
    episodes, running = 0, False
    for s in samples:
        now = bool(s["motion"] & RUNNING)
        if now and not running:
            episodes += 1
        running = now
    change = None if before is None or after is None else after - before
    starts = episodes if y0_edges is None or y0_edges == episodes else None
    if starts == 0 and change not in (0, None):
        starts = None  # A position change without a RUNNING sample or edge: a start completed unseen.
    return {"running_episodes": episodes, "y0_edges": y0_edges, "position_change": change, "starts": starts}


def width_table(trials: list) -> dict:
    """Starts per width; the shortest width from which every longer tested
    width gave a single start in every trial."""
    table = {}
    for t in trials:
        row = table.setdefault(t["width_ms"], {"trials": 0, "single": 0, "none": 0, "multiple": 0, "unclear": 0,
                                               "changes": []})
        row["trials"] += 1
        count = t.get("count") or {}
        n = count.get("starts")
        key = "single" if n == 1 else "none" if n == 0 else "multiple" if isinstance(n, int) and n > 1 else "unclear"
        row[key] += 1
        row["changes"].append(count.get("position_change"))
    reliable = [w for w in sorted(table) if table[w]["single"] == table[w]["trials"] and table[w]["trials"]]
    floor = None
    for w in sorted(table, reverse=True):
        if w in reliable:
            floor = w
        else:
            break
    return {"widths": {str(w): table[w] for w in sorted(table)}, "single_widths": reliable, "reliable_floor_ms": floor}


def held_analysis(trial: dict) -> dict:
    """A4/A5: starts during the hold and after the falling edge, from the
    session's samples (further start) and the Y0 running-on edges."""
    a = trial.get("analysis") or {}
    m = (a.get("members") or {}).get(OWNER) or (a.get("members") or {}).get(str(OWNER)) or {}
    return {"width_ms": trial.get("width_ms"), "outcome": m.get("outcome"), "further": m.get("further"),
            "missing": m.get("missing"), "position_change": m.get("position_change"),
            "expected_change": m.get("expected_change"), "y0_on_during_hold": a.get("y0_on_during_hold"),
            "y0_on_after_release": a.get("y0_on_after_release"), "source": a.get("source"),
            "on_us": a.get("on_us"), "off_us": a.get("off_us")}


def stop_classification(trial: dict, steps: int, stimulus_us: int | None, owner_edges: bool = True) -> dict:
    """Input-started motion stopped by a serial, group or X3 stop: moving
    witnessed before the stimulus, stopped short of the expected change, no
    further start through the window (session) and no second Y0 running-on
    edge after the stimulus. Otherwise indeterminate with the missing witness."""
    a = trial.get("analysis") or {}
    out = {}
    for address, m in (a.get("members") or {}).items():
        missing = []
        moving_before = bool(trial.get("moving_witness", {}).get(str(address)) or
                             trial.get("moving_witness", {}).get(address))
        if not moving_before:
            missing.append("no moving witness before the stop")
        change = m.get("position_change")
        short = change is not None and abs(change) < abs(steps) - 20
        if not short:
            missing.append("not stopped short (change %s of %s)" % (change, steps))
        further = m.get("further") == "seen" or m.get("outcome") == "second_start"
        if owner_edges and str(address) == str(OWNER) and stimulus_us:
            later = running_on_edges(trial.get("edges", []), stimulus_us + 1)
            further = further or bool(later)
        if further:
            missing.append("a further start followed")
        # A witnessed move that ran to its end, or any further start, is FAIL; a
        # missing moving witness alone leaves the cause open (indeterminate).
        cls = "stopped" if not missing else "fail" if further or (moving_before and not short) else "indeterminate"
        out[address] = {"class": cls, "missing": missing, "outcome": m.get("outcome"), "change": change}
    return out


def interrupt_analysis(trial: dict) -> dict:
    """I1-I4: final position against start + steps (relative: the original
    displacement), against the interrupt sample position + steps (a restart of
    the stored displacement) and against the absolute target; Y0 edges after
    the interrupt TX; ARRIVED (0x0007 bit 0) transitions."""
    samples = trial.get("samples") or []
    final = next((stops.position(s) for s in reversed(samples) if stops.position(s) is not None), None)
    start, at_interrupt, steps = trial.get("start_position"), trial.get("interrupt_position"), trial.get("steps")
    out = {"final": final, "start": start, "interrupt_position": at_interrupt}
    if trial.get("form") == "relative":
        if final is not None and start is not None:
            out["minus_start"] = final - start
            out["ends_at_start_plus_steps"] = final - start == steps
        if final is not None and at_interrupt is not None:
            out["minus_interrupt"] = final - at_interrupt
            out["ends_at_interrupt_plus_steps"] = final - at_interrupt == steps
    else:
        out["target"] = trial.get("target")
        out["ends_at_target"] = final is not None and final == trial.get("target")
    tx = trial.get("interrupt_tx_us")
    arrived, previous = [], None
    for s in sorted(samples, key=lambda s: s["windows"]["motion"][0]):
        bit = bool(s["motion"] & IN_POSITION)
        if previous is not None and bit != previous[0]:
            arrived.append({"to": int(bit), "bracket_us": [previous[1], s["windows"]["motion"][1]]})
        previous = (bit, s["windows"]["motion"][0])
    out["arrived_transitions"] = arrived
    edges = trial.get("edges") or []
    if tx:
        out["edges_after_interrupt"] = [{"at_us": e["at_us"] - tx, "active": e["active"]} for e in edges if e["at_us"] >= tx]
    out["running_episodes"] = count_starts(samples, None, None, 1)["running_episodes"]
    return out


# --------------------------------------------------------------------------
# Live sequencing

class Live:
    """Run state: console, scenario record facts, events, samples, bench views."""

    def __init__(self, console, run, baseline, args, timeout):
        self.console, self.run, self.baseline, self.args, self.timeout = console, run, baseline, args, timeout
        self.facts = facts()
        self.events, self.samples, self.views = [], [], []
        self.record = {"running_members": [], "max_age_ms": records.MAX_AGE_MS}
        self.current = None  # Name of the named state the drives are in.
        self.snapshots = {}
        self.written = set()
        self.group = False

    def call(self, name, **kwargs):
        reply = self.console.command(name, timeout_s=kwargs.pop("timeout_s", self.timeout), **kwargs)
        self.events.append({"command": name, "arguments": {k: v for k, v in kwargs.items()}, "ok": reply.get("ok"),
                            "result": reply.get("result"), "reason": reply.get("reason"),
                            "operation_id": reply.get("operation_id"), "started_us": reply.get("started_us")})
        return reply

    def checked(self, name, **kwargs):
        reply = self.call(name, **kwargs)
        if not reply.get("ok"):
            raise probe.BenchError("%s refused/failed: %s %s" % (name, reply.get("result", reply.get("outcome")),
                                                                 reply.get("reason") or ""))
        return reply

    def read_state(self, address):
        reply = self.call("read-state", address=address)
        s = stops.sample(reply)
        if s:
            self.samples.append(s)
        return s

    def state(self, address=OWNER):
        """bench_stop_inputs helpers (wait_running) read state through this name."""
        return self.read_state(address)

    def view(self):
        v = self.checked("bench", host_args=())
        self.views.append(v)
        return v

    def edges(self):
        found, lost = records._merge_edges([records._bench_view(v) for v in self.views if v.get("ok")])
        return found, lost


def write_setting(live, address, key, value):
    """One state write through its planned ordinary route with the stated facts; read back."""
    entry = {"drive": address, "key": key, "index": None, "candidate": value}
    steps, refusal = records.plan_entry(live.baseline, entry, live.args.image_level, live.facts, restore=False)
    if refusal:
        raise probe.BenchError("no ordinary route for %s: %s" % (key, refusal["reason"]))
    step = dict(steps[0], target=value, candidate=value)
    command, kwargs, text = records.write_call({"key": key, "index": None}, value, address)
    step["write"] = {"command": command, "kwargs": kwargs, "text": text}
    records.use_drive(live.console, address, timeout_s=live.timeout)
    outcome = records.apply_write(live.console, step, timeout_s=live.timeout)
    live.run.field(entry, baseline=step["baseline"])
    live.written.add((address, key))
    reply = outcome["reply"]
    ending, _ = records.readback(live.console, step, timeout_s=live.timeout)
    live.events.append({"command": text, "ok": reply.get("ok"), "operation_id": reply.get("operation_id"),
                        "outcome": reply.get("outcome"), "readback": ending})
    if outcome["uncertain"] or not outcome["written"]:
        live.run.field(entry, uncertain=outcome["uncertain"], reason="write %s %s refused or uncertain; not replayed" % (key, value))
        raise probe.BenchError("write %s %s on %d refused or uncertain: %s" % (key, value, address, outcome["result"]))
    if ending != value:
        live.run.field(entry, uncertain=True, reason="write %s %s read back %s" % (key, value, ending))
        raise probe.BenchError("write %s %s on %d read back %s" % (key, value, address, ending))
    live.run.field(entry, ending=ending)
    return ending


def read_line(live, addresses=None):
    """Fresh io read (nine words) and the 0x0050 mode of each drive."""
    out = {}
    for address in (DRIVES if addresses is None else addresses):
        io = live.checked("io", driver_args=("read",), address=address)
        out[address] = {"io": list(io["observation"]["raw"])}
    return out


def read_modes(live):
    out = {}
    for address in DRIVES:
        reply = live.checked("driver", driver_args=("read",), address=address)
        out[address] = reply["observation"]["raw"][6]
    return out


def enter_state(live, name):
    """Write the differing fields of the named state in a safe order: start
    functions removed first, X3, Y0, the position mode, function 7 last; each
    read back. Ends with a line check and clears the cached input agreement."""
    state = STATES[name]
    if live.group and not state["group"]:
        live.call("group-clear")
        live.group = False
    current = read_line(live)
    modes = read_modes(live)
    plan = []
    for address in DRIVES:
        words = current[address]["io"]
        if words[1] == 7 and state["x0"][address] != 7:
            plan.append((address, "io.x0", state["x0"][address]))
    for address in DRIVES:
        if current[address]["io"][4] != state["x3"][address]:
            plan.append((address, "io.x3", state["x3"][address]))
    if current[OWNER]["io"][6] != state["y0"]:
        plan.append((OWNER, "io.y0", state["y0"]))
    for address, mode in sorted(state["mode"].items()):
        if modes[address] != mode:
            plan.append((address, "drive.position-mode", mode))
    for address in DRIVES:
        words = current[address]["io"]
        if words[1] != state["x0"][address] and not (words[1] == 7 and state["x0"][address] != 7):
            plan.append((address, "io.x0", state["x0"][address]))
    done = []
    for address, key, value in plan:
        write_setting(live, address, key, value)
        done.append({"drive": address, "key": key, "value": value})
    # The session-level io/config agreement, while no drive is reserved: an
    # armed member refuses its io read (axis_conflict), so a later interlock
    # during a session could not compute it.
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    live.current = name
    line = line_check(live, name)
    if state["group"]:
        live.checked("group-set", host_args=tuple(str(a) for a in DRIVES), timeout_s=max(live.timeout, 10))
        live.group = True
        for address in DRIVES:
            live.call("group-reference", host_args=(str(address), "revoke"))
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    entry = {"state": name, "writes": done, "line": line, "modes_before": modes}
    live.events.append({"command": "enter state", **entry})
    return entry


def line_check(live, name, skip=(), rows=None):
    """Step 4: fresh assignments, polarity and outputs of drives 1, 2 and 4
    equal the named state; no drive outside the state's members holds 7-13 on
    X0 or X3. ``skip`` (reserved armed members) are compared from the
    firmware trigger's isolation rows instead (``rows``)."""
    state = STATES[name]
    reads = read_line(live, [a for a in DRIVES if a not in skip])
    problems = []
    for address in DRIVES:
        want = expected_io(state, address)
        if address in reads:
            got = reads[address]["io"]
            if got != want:
                problems.append("drive %d io %s, state %s wants %s" % (address, got, name, want))
        elif rows is not None:
            row = next((r for r in rows if r.get("address") == address), None)
            if row is None or row.get("io") != want[:5]:
                problems.append("drive %d trigger row io %s, state %s wants %s" % (
                    address, None if row is None else row.get("io"), name, want[:5]))
        for terminal, index in (("x0", 1), ("x3", 4)):
            function = (reads.get(address) or {}).get("io", want)[index]
            if 7 <= function <= 13 and address not in state["members"]:
                problems.append("drive %d outside the case holds %d on %s" % (address, function, terminal))
    out = {"state": name, "reads": {a: r["io"] for a, r in reads.items()}, "problems": problems}
    live.events.append({"command": "line check", **out})
    if problems:
        raise probe.BenchError("shared line check: " + "; ".join(problems))
    return out


def reference_reads(console, address, timeout):
    """The documented fresh sequence after the last write (docs/ess_input_start.md),
    selected axis: read config, driver read (0x0050), motion-profile read, the
    start-speed relation of the stored word, read state. The profile session
    stays open: ``bench_records.read_group`` forgets it after its read, which
    unbinds the move readiness (the arm then refuses ``move_readiness``;
    measured on the bench, wired bench 09). Raises on a refused read."""
    records.use_drive(console, address, timeout_s=timeout)
    out = {}
    for name, kwargs in (("read-config", {}), ("driver", {"driver_args": ("read",)})):
        reply = console.command(name, address=address, timeout_s=timeout, **kwargs)
        if not reply.get("ok"):
            raise probe.BenchError("%s refused/failed: %s" % (name, reply.get("result", reply.get("outcome"))))
        out[name] = {"operation_id": reply.get("operation_id"),
                     "raw": (reply.get("observation") or {}).get("raw") if name == "driver" else None}
    console.command("motion-profile", host_args=("forget",), timeout_s=timeout)
    profile = motion.profile_snapshot(console, "read", timeout_s=timeout)
    out["motion-profile"] = list(profile["current"])
    word = str(profile["current"][0])
    relation = console.command("motion-profile-start-speed-relation", host_args=(word, word), timeout_s=timeout)
    out["relation"] = relation.get("ok")
    state = console.command("read-state", address=address, timeout_s=timeout)
    if not state.get("ok"):
        raise probe.BenchError("read state refused/failed: %s" % state.get("result", state.get("outcome")))
    out["read-state"] = state
    return out


def selected_reference(live, address=OWNER):
    """reference_reads, then the FINITE_MOVE record with triggers inhibited
    (installed last: a later record for the terminal replaces it)."""
    reads = reference_reads(live.console, address, live.timeout)
    live.events.append({"command": "reference reads", "address": address, "driver": reads["driver"]["raw"],
                        "profile": reads["motion-profile"], "relation": reads["relation"]})
    s = stops.sample(reads["read-state"])
    if s:
        live.samples.append(s)
    out = {"driver": reads["driver"]["raw"], "profile": reads["motion-profile"], "relation": reads["relation"],
           "position": stops.position(s) if s else None, "state": s}
    live.checked("io-qualification-install",
                 host_args=(str(address), START_TERMINAL, "7", "0", "finite-move", "unknown", "triggers-inhibited"))
    return out


def group_reference(live):
    """Members after group set: group read state, driver read (0x0050) per
    member, then the member FINITE_MOVE records. A member record captures the
    member's checked state: before group read state it is refused
    ``reads_required`` (measured, wired bench 09)."""
    out = {"driver": {}}
    reply = live.checked("group-read-state", timeout_s=max(live.timeout, 10))
    out["group_read_state"] = reply.get("operation_id")
    for address in DRIVES:
        out["driver"][address] = live.checked("driver", driver_args=("read",), address=address)["observation"]["raw"][6]
    for address in DRIVES:
        live.checked("io-qualification-install", host_args=("member", str(address), START_TERMINAL, "7", "0", "finite-move",
                                                            "unknown", "triggers-inhibited"))
    out["positions"] = {}
    for address in DRIVES:
        s = live.read_state(address)
        out["positions"][address] = stops.position(s) if s else None
    return out


def inspect(live, session):
    return live.checked("arm-inspect", host_args=(str(session),))["session"]


def wait_phase(live, session, done, limit_s):
    deadline = time.monotonic() + limit_s
    while True:
        s = inspect(live, session)
        if done(s) or time.monotonic() >= deadline:
            return s
        time.sleep(0.05)


def compact(session):
    return {"phase": session.get("phase"), "error": session.get("error"), "source": (session.get("source") or {}).get("state"),
            "members": {m["address"]: {k: (m.get("result") or {}).get(k) for k in ("outcome", "running_observed",
                                                                                     "change_observed", "position_change")}
                        for m in session.get("members", [])}}


def arm(live, form, value, group=False, rpm=FINITE_RPM, poll_ms=50, window_ms=10000):
    tokens = [START_TERMINAL, form, str(value), str(rpm)] + (["group"] if group else []) + \
        ["poll-ms", str(poll_ms), "window-ms", str(window_ms)]
    reply = live.call("arm-position", host_args=tuple(tokens))
    if not reply.get("ok"):
        return None, reply
    session = reply["session"]["operation_id"]
    armed = wait_phase(live, session, lambda s: s["phase"] != "staging", 5.0)
    return session, armed


def finish_session(live, session):
    """Disarm if still active, then release; both results recorded."""
    out = {}
    s = inspect(live, session)
    if s["phase"] != "done":
        out["disarm"] = live.call("disarm", host_args=(str(session),)).get("result")
    out["release"] = live.call("release", operation_id=session).get("ok")
    return out


def settle_stopped(live, members, group=False):
    """After a session: fresh stopped reads of the members; if one still
    moves, the prepared stop at once (``group stop fast`` in NB) and a fresh
    stopped read. Returns (continued, reads)."""
    moving = []
    reads = {}
    for address in members:
        s = live.read_state(address)
        reads[address] = s
        if s is None or stops.moving(s):
            moving.append(address)
    if moving:
        if group:
            live.call("group-stop", host_args=("fast",), timeout_s=max(live.timeout, 10))
        else:
            for address in moving:
                live.call("stop", stop_policy="fast", address=address)
        for address in members:
            probe.observe_stopped(live.console, address=address, timeout_s=live.timeout,
                                  on_sample=lambda r: live.samples.append(stops.sample(r)) if stops.sample(r) else None)
    return moving, reads


def armed_trial(live, form, value, width_ms, *, group=False, hook=None, limit_s=None, poll_ms=50, settle=True):
    """One armed start: references, arm, line check, firmware trigger, observation
    to done with bench views (aux1 edges), optional hook(live, session_view,
    seconds_since_on, trial) for stops, then settle, release."""
    trial = {"form": form, "value": value, "width_ms": width_ms, "group": group}
    trial["reference"] = group_reference(live) if group else selected_reference(live)
    session, armed = arm(live, form, value, group=group, poll_ms=poll_ms)
    if session is None:
        trial["refused"] = "arm %s %s" % (armed.get("result"), armed.get("reason"))
        trial["arm_reply"] = {k: armed.get(k) for k in ("result", "reason", "refused_address")}
        return trial
    trial["session_id"] = session
    try:
        if armed["phase"] != "armed":
            trial["refused"] = "session %s: %s" % (armed["phase"], armed.get("error"))
            return trial
        trial["armed"] = {"members": [{k: m.get(k) for k in ("address", "staged_words", "expected_displacement",
                                                             "expected_endpoint", "baseline_position", "position_mode")}
                                      for m in armed.get("members", [])]}
        if group and not live.snapshots.get("memory_three_armed"):
            mem = live.call("memory")
            live.snapshots["memory_three_armed"] = {k: v for k, v in mem.items() if k not in ("type", "profile", "id")}
        state = STATES[live.current]
        trial["line"] = line_check(live, live.current, skip=state["members"])
        live.view()
        trigger = live.call("arm-trigger", host_args=(str(session), START_OUTPUT, str(width_ms)))
        trial["trigger"] = {k: trigger.get(k) for k in ("ok", "result", "reason", "refused_address")}
        decided = wait_phase(live, session, lambda s: s["phase"] != "triggering", 5.0)
        isolation = decided.get("isolation") or {}
        trial["isolation"] = {"admitted": isolation.get("admitted"), "row": isolation.get("row"),
                              "reason": isolation.get("reason"),
                              "rows": [{k: r.get(k) for k in ("address", "member", "row", "reason", "io", "state",
                                                               "io_age_us", "state_age_us", "io_started_us")}
                                       for r in isolation.get("rows", [])]}
        if not trigger.get("ok") or not isolation.get("admitted"):
            trial["refused"] = "trigger %s %s (%s %s)" % (trigger.get("result"), trigger.get("reason"),
                                                          isolation.get("row"), isolation.get("reason"))
            return trial
        # Members' I/O words as the firmware read them just before the ON (step 4).
        trial["line_rows"] = line_rows_check(live.current, trial["isolation"]["rows"])
        on_us = (decided.get("source") or {}).get("on_us")
        trial["on_us"] = on_us
        began = time.monotonic()
        timeline = []
        limit = limit_s or (width_ms / 1000.0 + 10.0)
        # A bench view every ~0.2 s (16 recent aux1 edges per view; a trial makes
        # at most a few) and the session view every other turn: the session
        # replies of three members are ~5 kB and the run's evidence store is
        # bounded (wired bench 09: 10 Hz inspection filled 16 MiB in 3.5 min).
        turn, s = 0, decided
        while True:
            v = live.view()
            if turn % 2 == 0:
                s = inspect(live, session)
                timeline.append(dict(compact(s), now_us=v.get("now_us")))
            if hook is not None:
                hook(live, s, time.monotonic() - began, trial)
            if s["phase"] == "done" or time.monotonic() - began > limit:
                break
            turn += 1
            time.sleep(0.15)
        trial["timeline"] = timeline
        final = inspect(live, session)
        trial["session"] = final
        found, lost = live.edges()
        trial["edges"] = [e for e in found if e["at_us"] >= (on_us or 0) - 2000000]
        trial["lost_edge_windows"] = lost
        trial["analysis"] = start_trial(final, trial["edges"])
    finally:
        trial["finish"] = finish_session(live, session)
        if settle:
            members = STATES[live.current]["members"] if group else [OWNER]
            moving, reads = settle_stopped(live, members, group=group)
            if moving:
                trial["continued_motion"] = moving
            trial["after"] = {a: (stops.position(s) if s else None) for a, s in reads.items()}
    return trial


def line_rows_check(name, rows):
    state = STATES[name]
    problems = []
    for row in rows:
        want = expected_io(state, row["address"])[:5]
        if row.get("io") != want:
            problems.append("drive %s trigger row io %s, state %s wants %s" % (row["address"], row.get("io"), name, want))
    if problems:
        raise probe.BenchError("shared line check at the trigger: " + "; ".join(problems))
    return {"rows": len(rows), "problems": problems}


def unarmed_pulse(live, output, width_ms, named):
    """One deliberate unarmed ON through the D14 interlock (bench_records.bench_on)."""
    live.record["running_members"] = named
    try:
        reply = records.bench_on(live.console, output, width_ms, live.record)
    finally:
        live.record["running_members"] = []
    live.views.append(reply)
    o = records._bench_view(reply)["outputs"][output]
    return o["on_us"]


def start_member(address=OWNER):
    return {"address": address, "output": START_OUTPUT, "terminal": START_TERMINAL, "function": 7, "polarity": 0,
            "effect": records.START_EFFECT}


def sample_for(live, address, seconds, every=3, pause_s=0.04):
    """read state about every 70 ms for ``seconds`` and a bench view every
    ``every`` reads. The pause bounds the evidence volume: back-to-back reads
    filled the 64 MiB store during I3 (wired bench 09)."""
    began, index = time.monotonic(), 0
    while time.monotonic() - began < seconds:
        live.read_state(address)
        index += 1
        if index % every == 0:
            live.view()
        time.sleep(pause_s)
    live.view()


def stage_only(live, form, value):
    """Stage the 0x0021 words through the arm (no start word), then disarm untriggered.
    The documented fresh reads come first (the arm needs 0x0050 from a current driver read)."""
    selected_reference(live)
    session, armed = arm(live, form, value)
    if session is None:
        raise probe.BenchError("staging arm refused: %s %s" % (armed.get("result"), armed.get("reason")))
    words = [m.get("staged_words") for m in armed.get("members", [])]
    finish = finish_session(live, session)
    if armed["phase"] != "armed":
        raise probe.BenchError("staging arm ended %s: %s" % (armed["phase"], armed.get("error")))
    return {"session": session, "staged_words": words, "finish": finish}


# ----------------------------------------------------------------- cases

def run_start(live, case):
    trials = []
    p0 = None
    for index in range(case["runs"]):
        if case["form"] == "absolute":
            if p0 is None:
                p0 = selected_reference(live)["position"]
            value = p0 + case["steps"] if index % 2 == 0 else p0
        else:
            value = case["steps"]
        trial = armed_trial(live, case["form"], value, case["width_ms"])
        trial["index"] = index + 1
        trials.append(trial)
        if trial.get("continued_motion"):
            raise probe.BenchError("motion continued after an input start; prepared stop sent; case ended")
    return {"trials": trials, "summary": summarize_starts(trials), "p0": p0}


def run_width(live, case):
    live.snapshots.setdefault("stage", []).append(stage_only(live, "relative", case["steps"]))
    trials = []
    for width in case["widths"]:
        for index in range(case["runs"]):
            records.use_drive(live.console, OWNER, timeout_s=live.timeout)
            line_check(live, live.current)
            before = live.read_state(OWNER)
            first = len(live.samples)
            live.view()
            trial = {"width_ms": width, "index": index + 1, "before": stops.position(before) if before else None}
            try:
                trial["on_us"] = unarmed_pulse(live, START_OUTPUT, width, [start_member()])
            except records.InterlockRefused as error:
                trial["interlock_refused"] = error.entry.get("reason")
                trials.append(trial)
                continue
            sample_for(live, OWNER, width / 1000.0 + 1.0 + POST_RELEASE_S)
            moving, reads = settle_stopped(live, [OWNER])
            after = reads[OWNER]
            trial["after"] = stops.position(after) if after else None
            trial["continued_motion"] = moving or None
            trial["samples"] = live.samples[first:]
            found, _ = live.edges()
            trial["y0_on_edges"] = len(running_on_edges(found, trial["on_us"], trial["on_us"] + 3000000))
            trial["count"] = count_starts(trial["samples"], trial["before"], trial["after"], case["steps"],
                                          trial["y0_on_edges"])
            trial["d_us"] = start_delay_us(found, trial["on_us"])
            trials.append(trial)
    return {"trials": trials, "table": width_table(trials)}


def run_held(live, case):
    trials = []
    for index in range(case["runs"]):
        if case["form"] == "absolute":
            value = selected_reference(live)["position"] + case["steps"]
        else:
            value = case["steps"]
        trial = armed_trial(live, case["form"], value, case["width_ms"], limit_s=case["width_ms"] / 1000.0 + 12.0)
        trial["index"] = index + 1
        trial["held"] = held_analysis(trial)
        trials.append(trial)
    return {"trials": trials, "summary": summarize_starts(trials)}


def _moving_witness(s, trial):
    """A RUNNING sample of the session or a Y0 running-on edge after the ON."""
    w = trial.setdefault("moving_witness", {})
    for m in s.get("members", []):
        r = m.get("result") or {}
        if r.get("running_observed"):
            w[m["address"]] = w.get(m["address"]) or "running_sample"
    return w


def run_serial_stop(live, case):
    trials = []
    for name in case_states(case):
        if live.current != name:
            enter_state(live, name)
        for policy in case["policies"]:
            for index in range(case["runs"]):
                sent = {}

                def hook(live_, s, since_on, trial, policy=policy, sent=sent):
                    w = _moving_witness(s, trial)
                    if not sent and since_on >= case["stop_at_s"]:
                        found, _ = live_.edges()
                        if trial.get("on_us") and running_on_edges(found, trial["on_us"]):
                            w[OWNER] = w.get(OWNER) or "y0_running_edge"
                        view = live_.view()
                        if case.get("diagnose_s"):
                            # The stop runs while drive 4 is read back to back from its admission on.
                            handle = live_.console.begin("stop", stop_policy=policy, address=OWNER,
                                                         timeout_s=max(live_.timeout, 10))
                            first = len(live_.samples)
                            began = time.monotonic()
                            while time.monotonic() - began < case["diagnose_s"]:
                                live_.read_state(OWNER)
                            trial["diagnostic_samples"] = live_.samples[first:]
                            sent["reply"] = live_.console.wait(handle, release=True)
                        else:
                            sent["reply"] = live_.call("stop", stop_policy=policy, address=OWNER)
                        sent["at_us"] = view.get("now_us")
                        trial["stop"] = {"policy": policy, "result": sent["reply"].get("result"),
                                         "operation_id": sent["reply"].get("operation_id"), "at_us": sent["at_us"],
                                         "witness_before": dict(w)}

                if name == "NA-abs":
                    p = selected_reference(live)["position"]
                    value = p + (case["steps"] if index % 2 == 0 else -case["steps"])
                    form = "absolute"
                else:
                    value, form = case["steps"] if index % 2 == 0 else -case["steps"], "relative"
                trial = armed_trial(live, form, value, case["width_ms"], hook=hook)
                trial.update(index=index + 1, policy=policy, state=name)
                trial["moving_witness"] = (trial.get("stop") or {}).get("witness_before", {})
                trial["classification"] = stop_classification(trial, case["steps"], (trial.get("stop") or {}).get("at_us"))
                trials.append(trial)
                if trial.get("continued_motion"):
                    raise probe.BenchError("motion continued after the serial stop; prepared stop sent; case ended")
    return {"trials": trials}


def session_member(session_view, address, terminal_function=5):
    row = next((r for r in (session_view.get("isolation") or {}).get("rows", []) if r.get("address") == address), None)
    if row is None:
        return None
    return {"address": address, "output": STOP_OUTPUT, "terminal": STOP_TERMINAL, "function": terminal_function,
            "polarity": 0, "effect": "stop",
            "session_io": {"session": session_view["operation_id"], "raw": list(row["io"]), "started_us": row["io_started_us"]},
            "session_phase": session_view.get("phase")}


def _input_stop_hook(case, members):
    def hook(live, s, since_on, trial):
        w = _moving_witness(s, trial)
        found, _ = live.edges()
        if trial.get("on_us") and running_on_edges(found, trial["on_us"]):
            w[OWNER] = w.get(OWNER) or "y0_running_edge"
        if trial.get("stop_pulse") is None and since_on >= case["stop_at_s"]:
            named = [session_member(s, a) for a in members]
            trial["witness_before"] = dict(w)
            try:
                on = unarmed_pulse(live, STOP_OUTPUT, case["stop_hold_ms"], [n for n in named if n])
                trial["stop_pulse"] = {"on_us": on, "hold_ms": case["stop_hold_ms"]}
            except records.InterlockRefused as error:
                trial["stop_pulse"] = {"refused": error.entry.get("reason"), "row": error.entry.get("row")}
    return hook


def run_input_stop(live, case, group=False):
    trials = []
    members = DRIVES if group else [OWNER]
    for index in range(case["runs"]):
        if group:
            form, value = "absolute-by", case["steps"] if index % 2 == 0 else -case["steps"]
        else:
            p = selected_reference(live)["position"]
            form, value = "absolute", p + (case["steps"] if index % 2 == 0 else -case["steps"])
        trial = armed_trial(live, form, value, case["width_ms"], group=group, hook=_input_stop_hook(case, members))
        trial["index"] = index + 1
        trial["moving_witness"] = trial.get("witness_before", {})
        trial["classification"] = stop_classification(trial, case["steps"], (trial.get("stop_pulse") or {}).get("on_us"))
        trials.append(trial)
        if trial.get("continued_motion"):
            raise probe.BenchError("motion continued after the X3 stop input; prepared stop sent; case ended")
    return {"trials": trials}


def run_held_stop(live, case):
    trials = []
    for index in range(case["runs"]):
        sent = {}

        def hook(live_, s, since_on, trial):
            w = _moving_witness(s, trial)
            if not sent and since_on >= case["stop_at_s"]:
                found, _ = live_.edges()
                if trial.get("on_us") and running_on_edges(found, trial["on_us"]):
                    w[OWNER] = w.get(OWNER) or "y0_running_edge"
                view = live_.view()
                sent["reply"] = live_.call("stop", stop_policy="fast", address=OWNER)
                after = live_.view()
                out = next((o for o in after.get("outputs", []) if o.get("name") == START_OUTPUT), {})
                trial["stop"] = {"policy": "fast", "result": sent["reply"].get("result"), "at_us": view.get("now_us"),
                                 "witness_before": dict(w), "output_after": {k: out.get(k) for k in
                                                                             ("active", "off_us", "off_reason")}}
        trial = armed_trial(live, "relative", case["steps"], case["width_ms"], hook=hook,
                            limit_s=case["width_ms"] / 1000.0 + 10.0)
        trial["index"] = index + 1
        trial["moving_witness"] = (trial.get("stop") or {}).get("witness_before", {})
        trial["classification"] = stop_classification(trial, case["steps"], (trial.get("stop") or {}).get("at_us"))
        trials.append(trial)
    return {"trials": trials}


def run_disarm(live, case):
    out = {}
    # (a) Disarm before the trigger, then the trigger: refused with no port call.
    p = selected_reference(live)["position"]
    session, armed = arm(live, "absolute", p + case["steps"])
    if session is None:
        raise probe.BenchError("A9 arm refused: %s %s" % (armed.get("result"), armed.get("reason")))
    before = live.view()
    disarm = live.call("disarm", host_args=(str(session),))
    trigger = live.call("arm-trigger", host_args=(str(session), START_OUTPUT, str(case["width_ms"])))
    after = live.view()
    ons = lambda v: next((o.get("ons") for o in v.get("outputs", []) if o.get("name") == START_OUTPUT), None)
    s = inspect(live, session)
    out["before_trigger"] = {"disarm": disarm.get("result"), "trigger": {k: trigger.get(k) for k in ("ok", "result", "reason")},
                             "bus_traffic": trigger.get("bus_traffic"), "ons_before": ons(before), "ons_after": ons(after),
                             "phase": s.get("phase"), "source": (s.get("source") or {}).get("state")}
    live.call("release", operation_id=session)
    moving, reads = settle_stopped(live, [OWNER])
    out["before_trigger"]["position_after"] = stops.position(reads[OWNER]) if reads[OWNER] else None
    out["before_trigger"]["position_before"] = p
    # (b) Disarm while moving: the move continues to its target.
    p = selected_reference(live)["position"]
    target = p + case["steps"]
    trial = {"target": target, "start": p}
    disarmed = {}

    def hook(live_, s, since_on, trial_):
        w = _moving_witness(s, trial_)
        if not disarmed and w:
            disarmed["reply"] = live_.call("disarm", host_args=(str(s["operation_id"]),)).get("result")
            disarmed["at_s"] = since_on
    # No settle: a disarm is not a stop, and the move must end at its target by itself.
    t = armed_trial(live, "absolute", target, case["width_ms"], hook=hook, settle=False)
    first = len(live.samples)
    sample_for(live, OWNER, 2.5)
    probe.observe_stopped(live.console, address=OWNER, timeout_s=live.timeout,
                          on_sample=lambda r: live.samples.append(stops.sample(r)) if stops.sample(r) else None)
    end = live.read_state(OWNER)
    t.update(disarm=disarmed, final=stops.position(end) if end else None, after_samples=live.samples[first:])
    t["reached_target"] = t["final"] == target
    out["while_moving"] = t
    return out


def run_disarm_inhibit(live, case):
    """A10: disarm is not inhibition; removing function 7 is."""
    out = {}
    p0 = selected_reference(live)["position"]
    out["p0"] = p0
    out["stage1"] = stage_only(live, "absolute", p0 + case["steps"])
    line_check(live, live.current)
    first = len(live.samples)
    live.read_state(OWNER)
    on = unarmed_pulse(live, START_OUTPUT, case["width_ms"], [start_member()])
    sample_for(live, OWNER, 2.5 + POST_RELEASE_S)
    moving, reads = settle_stopped(live, [OWNER])
    out["first_pulse"] = {"on_us": on, "label": "deliberate unarmed start (D13)", "continued_motion": moving or None,
                          "final": stops.position(reads[OWNER]) if reads[OWNER] else None,
                          "samples": live.samples[first:]}
    out["first_pulse"]["reached"] = out["first_pulse"]["final"] == p0 + case["steps"]
    out["first_pulse"]["in_position"] = bool(reads[OWNER] and reads[OWNER]["motion"] & IN_POSITION)
    out["first_pulse"]["running_seen"] = any(stops.running(s) for s in out["first_pulse"]["samples"])
    # Stage P0, remove function 7, pulse again: the drive must stay.
    out["stage2"] = stage_only(live, "absolute", p0)
    write_setting(live, OWNER, "io.x0", 0)
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    state = dict(STATES[live.current], x0={**STATES[live.current]["x0"], OWNER: 0}, members=[])
    STATES["A10-x0-none"] = state
    try:
        line_check(live, "A10-x0-none")
        first = len(live.samples)
        before = live.read_state(OWNER)
        on = unarmed_pulse(live, START_OUTPUT, case["width_ms"], [])
        sample_for(live, OWNER, 1.0 + POST_RELEASE_S)
        moving, reads = settle_stopped(live, [OWNER])
        final = stops.position(reads[OWNER]) if reads[OWNER] else None
        out["second_pulse"] = {"on_us": on, "label": "deliberate unarmed pulse with X0 = 0", "final": final,
                               "continued_motion": moving or None, "samples": live.samples[first:],
                               "before": stops.position(before) if before else None,
                               # Unchanged against the read just before this pulse, no RUNNING
                               # (wired bench 09: drive 4 rests one increment short of its targets).
                               "stayed": before is not None and final == stops.position(before) and not any(
                                   stops.running(s) for s in live.samples[first:])}
    finally:
        write_setting(live, OWNER, "io.x0", 7)  # Cleanup: the state's function 7 again.
        STATES.pop("A10-x0-none", None)
        live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    return out


def run_fixed_length(live, case):
    """A11: function 7 during serial speed mode (p29 method 1)."""
    out = {"trials": []}
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    snap = live.checked("velocity-read", address=OWNER)
    v = snap["velocity_parameters"]
    live.snapshots["velocity"] = {"operation_id": snap["operation_id"], "address": OWNER,
                                  "words": [v["raw_speed"], v["raw_acceleration_time"], v["raw_deceleration_time"]],
                                  "baseline_words": records.snapshot_words(live.baseline, OWNER, "velocity")}
    for index in range(case["runs"]):
        out.setdefault("stages", []).append(stage_only(live, "relative", case["steps"]))
        records.use_drive(live.console, OWNER, timeout_s=live.timeout)
        live.checked("read-config", address=OWNER)
        live.checked("velocity-read", address=OWNER)
        live.checked("velocity-qualification-install", host_args=("rpm", "ramp", "inputs"))
        line_check(live, live.current)  # Before the pre-start io read: only reads may follow it (row L3).
        pre = stops.pre_start(live)
        handle = live.console.begin("velocity", velocity_args=(str(case["rpm"]), "rpm", "native", str(case["duration_ms"]),
                                                               "configured", "normal"),
                                    address=OWNER, timeout_s=max(live.timeout, 10))
        trial = {"index": index + 1, "rpm": case["rpm"], "duration_ms": case["duration_ms"]}
        if not handle.accepted:
            trial["refused"] = live.console.wait(handle)
            out["trials"].append(trial)
            continue
        accepted = time.monotonic()
        first = len(live.samples)
        stops.wait_running(live, accepted, case["at_s"])
        named = {"address": OWNER, "output": START_OUTPUT, "terminal": START_TERMINAL, "function": 7, "polarity": 0,
                 "effect": "fixed-length", "operation_id": handle.operation_id, "io_read": pre["io_read"],
                 "between": list(pre["between"])}
        try:
            trial["on_us"] = unarmed_pulse(live, START_OUTPUT, case["width_ms"], [named])
            trial["label"] = "deliberate unarmed pulse during serial velocity (p29 method 1)"
        except records.InterlockRefused as error:
            trial["interlock_refused"] = error.entry.get("reason")
        sample_for(live, OWNER, 2.0 + POST_RELEASE_S)
        trial["terminal"] = {k: v for k, v in live.console.wait(handle, release=True).items()
                             if k in ("ok", "state", "outcome", "status", "detail", "completion", "stop_completion",
                                      "stop_due_us", "stop_inputs")}
        moving, reads = settle_stopped(live, [OWNER])
        trial["continued_motion"] = moving or None
        samples = live.samples[first:]
        trial["samples"] = samples
        edge = None
        if trial.get("on_us"):
            edge = next((s for s in samples if s["windows"]["feedback"][1] <= trial["on_us"]), None)
            before = [s for s in samples if s["windows"]["feedback"][1] <= trial["on_us"]]
            edge = before[-1] if before else None
        trial["position_at_edge"] = stops.position(edge) if edge else None
        trial["final"] = stops.position(reads[OWNER]) if reads[OWNER] else None
        if trial["position_at_edge"] is not None and trial["final"] is not None:
            trial["after_edge"] = trial["final"] - trial["position_at_edge"]
        out["trials"].append(trial)
    return out


def run_group_start(live, case):
    trials = []
    for index in range(case["runs"]):
        value = case["steps"] if index % 2 == 0 else -case["steps"]
        trial = armed_trial(live, "absolute-by", value, case["width_ms"], group=True)
        trial["index"] = index + 1
        trials.append(trial)
        if trial.get("continued_motion"):
            raise probe.BenchError("group motion continued after an input start; group stop fast sent; case ended")
    return {"trials": trials, "summary": summarize_starts(trials)}


def run_group_stop(live, case):
    trials = []
    for policy in case["policies"]:
        for index in range(case["runs"]):
            sent = {}

            def hook(live_, s, since_on, trial, policy=policy, sent=sent):
                w = _moving_witness(s, trial)
                if not sent and since_on >= case["stop_at_s"]:
                    view = live_.view()
                    if case.get("diagnose_s"):
                        handle = live_.console.begin("group-stop", host_args=(policy,), timeout_s=max(live_.timeout, 10))
                        first = len(live_.samples)
                        began, turn = time.monotonic(), 0
                        members = case["diagnose_members"]
                        while time.monotonic() - began < case["diagnose_s"]:
                            live_.read_state(members[turn % len(members)])
                            turn += 1
                        trial["diagnostic_samples"] = live_.samples[first:]
                        sent["reply"] = live_.console.wait(handle, release=True)
                    else:
                        sent["reply"] = live_.call("group-stop", host_args=(policy,), timeout_s=max(live_.timeout, 10))
                    trial["stop"] = {"policy": policy, "result": sent["reply"].get("result"), "at_us": view.get("now_us"),
                                     "witness_before": dict(w),
                                     "members": [{k: m.get(k) for k in ("address", "state", "outcome", "completion")}
                                                 for m in (sent["reply"].get("members") or [])]}
            value = case["steps"] if index % 2 == 0 else -case["steps"]
            trial = armed_trial(live, "absolute-by", value, case["width_ms"], group=True, hook=hook)
            trial.update(index=index + 1, policy=policy)
            trial["moving_witness"] = (trial.get("stop") or {}).get("witness_before", {})
            trial["classification"] = stop_classification(trial, case["steps"], (trial.get("stop") or {}).get("at_us"))
            trials.append(trial)
            if trial.get("continued_motion"):
                raise probe.BenchError("group motion continued after the group stop; group stop fast sent; case ended")
    return {"trials": trials}


def run_interrupt(live, case):
    out = {"trials": []}
    forms = case.get("forms") or [case["form"]]
    delays = case["at_s"] if isinstance(case["at_s"], tuple) else None
    for form in forms:
        for index in range(case["runs"]):
            at_s = delays[index % len(delays)] if delays else case["at_s"]
            steps = case["steps"] if index % 2 == 0 else -case["steps"]
            out["trials"].append(interrupt_trial(live, form, steps, at_s, index + 1))
            t = out["trials"][-1]
            if t.get("continued_motion"):
                raise probe.BenchError("motion continued after the interruption trial; prepared stop sent; case ended")
    return out


def interrupt_trial(live, form, steps, at_s, index):
    """One serial move, ``position interrupt`` at ``at_s``, observation to rest
    and 2 s after; the prepared stop fast settles the possible-motion interlock."""
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    live.call("motion-profile", host_args=("forget",))
    profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
    word = str(profile["current"][0])
    live.call("motion-profile-start-speed-relation", host_args=(word, word))
    before = live.read_state(OWNER)
    start = stops.position(before) if before else None
    trial = {"index": index, "form": form, "steps": steps, "at_s": at_s, "start_position": start}
    line_check(live, live.current)
    if form == "absolute":
        trial["target"] = start + steps
        handle = live.console.begin("move-absolute", move_args=(str(start + steps), "steps", "native", str(FINITE_RPM),
                                                                "configured"), address=OWNER, timeout_s=max(live.timeout, 40))
    else:
        handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", str(FINITE_RPM), "configured"),
                                    address=OWNER, timeout_s=max(live.timeout, 40))
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    trial["original_id"] = handle.operation_id
    accepted = time.monotonic()
    first = len(live.samples)
    live.view()
    stops.wait_running(live, accepted, at_s)
    last = live.samples[-1] if live.samples else None
    trial["interrupt_position"] = stops.position(last) if last else None
    trial["interrupt_sample_running"] = bool(last and stops.running(last))
    view = live.view()
    trial["interrupt_tx_us"] = view.get("now_us")
    interrupt = live.console.begin("position-interrupt", host_args=(str(handle.operation_id),),
                                   timeout_s=max(live.timeout, 10))
    trial["interrupt_accepted"] = interrupt.accepted
    sample_for(live, OWNER, abs(steps) / 1000.0 + 1.5 + POST_RELEASE_S)
    trial["original"] = {k: v for k, v in live.console.wait(handle, release=False).items()
                         if k in ("ok", "state", "outcome", "status", "detail", "completion", "execution", "relation",
                                  "uncertain", "polls")}
    trial["interrupt"] = {k: v for k, v in live.console.wait(interrupt, release=False).items()
                          if k in ("ok", "result", "state", "outcome", "status", "detail", "completion", "execution",
                                   "relation", "uncertain", "operation_id", "trigger_evidence")}
    trial["samples"] = live.samples[first:]
    found, _ = live.edges()
    trial["edges"] = [e for e in found if e["at_us"] >= trial["interrupt_tx_us"] - 4000000]
    trial["analysis"] = interrupt_analysis(trial)
    # The prepared stop settles the possible-motion interlock of an ACK-only replacement.
    if trial["samples"] and stops.moving(trial["samples"][-1]):
        trial["continued_motion"] = True
    trial["settle"] = stops.reconcile(live, [OWNER])
    for h in (interrupt, handle):
        if getattr(h, "operation_id", None):
            live.call("release", operation_id=h.operation_id)
    return trial


def run_regression(live, case):
    out = {"reads": {}, "moves": [], "group": None}
    for address in DRIVES:
        r = {k: live.call(name, address=address).get("ok") for k, name in
             (("identity", "read-identity"), ("config", "read-config"), ("state", "read-state"))}
        out["reads"][address] = r
    for address in DRIVES:
        reads = reference_reads(live.console, address, live.timeout)
        s = stops.sample(reads["read-state"])
        start = stops.position(s) if s else None
        handle = live.console.begin("move-relative", move_args=(str(case["steps"]), "steps", "native", str(FINITE_RPM),
                                                                "configured"), address=address, timeout_s=max(live.timeout, 40))
        trial = {"address": address, "start": start, "accepted": handle.accepted}
        if handle.accepted:
            began = time.monotonic()
            seen = False
            while time.monotonic() - began < case["stop_at_s"] or not seen:
                x = live.read_state(address)
                seen = seen or bool(x and stops.running(x))
                if time.monotonic() - began > case["stop_at_s"] + 1.0:
                    break
            stop = live.call("stop", stop_policy="fast", address=address)
            trial["stop"] = {k: stop.get(k) for k in ("ok", "result", "outcome", "completion")}
            trial["running_seen"] = seen
            t = live.console.wait(handle, release=True)
            trial["terminal"] = {k: t.get(k) for k in ("ok", "state", "outcome", "completion", "detail")}
            end = probe.observe_stopped(live.console, address=address, timeout_s=live.timeout)
            x = stops.sample(end)
            trial["final"] = stops.position(x) if x else None
            trial["travel"] = None if trial["final"] is None or start is None else trial["final"] - start
            trial["stopped"] = probe.drive_reported_stopped(end)
        else:
            trial["refused"] = live.console.wait(handle)
        out["moves"].append(trial)
    live.checked("group-set", host_args=tuple(str(a) for a in DRIVES), timeout_s=max(live.timeout, 10))
    live.group = True
    live.checked("group-read-state", timeout_s=max(live.timeout, 10))
    handle = live.console.begin("group-move", host_args=("relative", str(case["steps"]), "steps", "native", str(FINITE_RPM),
                                                         "configured", "start", "staged", "on-failure", "stop", "fast"),
                                timeout_s=max(live.timeout, 40))
    t = live.console.wait(handle, release=True) if handle.accepted else live.console.wait(handle)
    out["group"] = {"accepted": handle.accepted, "ok": t.get("ok"), "state": t.get("state"),
                    "members": [{k: m.get(k) for k in ("address", "state", "outcome", "completion")}
                                for m in (t.get("members") or [])]}
    live.call("group-clear")
    live.group = False
    return out


def run_serial_endpoint(live, case):
    """E1: plain serial absolute moves of +/-steps on each drive; the final
    feedback after the move's own completion and 1 s at rest, against the target."""
    out = {"trials": []}
    for address in DRIVES:
        for index in range(case["runs"]):
            reads = reference_reads(live.console, address, live.timeout)
            s = stops.sample(reads["read-state"])
            start = stops.position(s) if s else None
            target = start + (case["steps"] if index % 2 == 0 else -case["steps"])
            handle = live.console.begin("move-absolute", move_args=(str(target), "steps", "native", str(FINITE_RPM),
                                                                    "configured"), address=address,
                                        timeout_s=max(live.timeout, 40))
            trial = {"address": address, "index": index + 1, "start": start, "target": target}
            if not handle.accepted:
                trial["refused"] = live.console.wait(handle)
                out["trials"].append(trial)
                continue
            t = live.console.wait(handle, release=True)
            trial["terminal"] = {k: t.get(k) for k in ("ok", "state", "outcome", "completion", "raw_motion", "polls")}
            first = live.read_state(address)
            time.sleep(1.0)
            last = live.read_state(address)
            trial["final_at_completion"] = stops.position(first) if first else None
            trial["final"] = stops.position(last) if last else None
            trial["error"] = None if trial["final"] is None else trial["final"] - target
            trial["in_position"] = bool(last and last["motion"] & IN_POSITION)
            out["trials"].append(trial)
            if last is None or stops.moving(last):
                live.call("stop", stop_policy="fast", address=address)
                raise probe.BenchError("drive %d still moving after a serial endpoint trial; stop fast sent" % address)
    errors = {}
    for t in out["trials"]:
        if t.get("error") is not None:
            key = "%d %s" % (t["address"], "+" if t["target"] > t["start"] else "-")
            errors.setdefault(key, {}).setdefault(str(t["error"]), 0)
            errors[key][str(t["error"])] += 1
    out["errors"] = errors
    return out


RUNNERS = {"restore-only": lambda live, case: {"note": "no motion; the run's restoration restores the profile words"},
           "regression": run_regression, "serial-endpoint": run_serial_endpoint, "start": run_start, "width": run_width, "held": run_held, "serial-stop": run_serial_stop,
           "input-stop": run_input_stop, "held-stop": run_held_stop, "disarm": run_disarm,
           "disarm-inhibit": run_disarm_inhibit, "fixed-length": run_fixed_length, "group-start": run_group_start,
           "group-stop": run_group_stop, "group-input-stop": lambda live, case: run_input_stop(live, case, group=True),
           "interrupt": run_interrupt}


def run_case(live, cid):
    case = dict(CASES[cid])
    if getattr(live.args, "repetitions", None):
        case["runs"] = live.args.repetitions  # A pilot or a shortened rerun; recorded in the scenario bounds.
    if live.current != case_states(case)[0]:
        enter_state(live, case_states(case)[0])
    detail = RUNNERS[case["kind"]](live, case)
    detail["runs"] = case.get("runs")
    # Row cleanup: no session left, bench outputs off, every drive read stopped.
    off = records.outputs_off(live.console, timeout_s=live.timeout)
    stopped = {}
    for address in DRIVES:
        s = live.read_state(address)
        stopped[address] = bool(s) and not stops.moving(s) and not s["alarm"]
    detail["cleanup"] = {"outputs_off": off["outputs_off"], "stopped": stopped}
    return detail


# --------------------------------------------------------------------------
# Verdicts

def case_verdicts(cid, detail, error, tolerance=0):
    """(level -> verdict, reason) per the prompt 09 case table; ``tolerance``
    (increments) applies to the A1/A2/B1 endpoint only."""
    case = CASES[cid]
    levels = case["levels"]
    stopped_early = "stopped early: %s" % error if error else None
    def all_levels(verdict, reason):
        return {level: (verdict, reason) for level in levels}
    trials = detail.get("trials", [])
    kind = case["kind"]
    if kind in ("start", "group-start"):
        out = start_verdicts(trials, DRIVES if kind == "group-start" else [OWNER], tolerance)
        runs = detail.get("runs", case["runs"])
        if len(trials) < runs or error:
            out = {lv: (v if v != "PASS" else "indeterminate", r or "%d of %d trials; %s" % (len(trials), runs, stopped_early))
                   for lv, (v, r) in out.items()}
        return {lv: out[lv] for lv in levels}
    if kind == "width":
        table = detail.get("table") or {}
        if not trials:
            return all_levels("NOT RUN", stopped_early or "no trial")
        unclear = [t for t in trials if t.get("interlock_refused") or (t.get("count") or {}).get("starts") is None]
        if unclear:
            return all_levels("indeterminate", "%d trials without a start count" % len(unclear))
        multiple = [t for t in trials if (t.get("count") or {}).get("starts", 0) > 1]
        if multiple:
            return all_levels("PASS", "recorded; %d pulses gave more than one start" % len(multiple))
        return all_levels("PASS", "recorded; shortest width with 3/3 single starts from it up: %s ms" % table.get("reliable_floor_ms"))
    if kind == "held":
        if not trials or not trials[0].get("analysis"):
            return all_levels("NOT RUN" if not trials else "indeterminate", stopped_early or "no session result")
        held = trials[0]["held"]
        out = all_levels("PASS", "recorded (report-only): %s" % held)
        if held.get("further") == "not_excluded" and "drive-reported" in out:
            # Prompt 09 A5: a second start at a reached target shows only as RUNNING or a Y0 edge.
            out["drive-reported"] = ("indeterminate", "a repeated start at the reached endpoint is not excluded by "
                                     "position; no RUNNING sample or Y0 running edge showed one: %s" % held)
        return out
    if kind in ("serial-stop", "input-stop", "group-stop", "group-input-stop", "held-stop"):
        if not trials:
            return all_levels("NOT RUN", stopped_early or "no trial")
        fails, open_ = [], []
        for t in trials:
            if t.get("refused"):
                open_.append("trial %s refused: %s" % (t.get("index"), t["refused"]))
                continue
            if t.get("continued_motion"):
                fails.append("trial %s continued" % t.get("index"))
            for address, c in (t.get("classification") or {}).items():
                if c["class"] == "fail":
                    fails.append("trial %s drive %s: %s" % (t.get("index"), address, c["missing"]))
                elif c["class"] != "stopped":
                    open_.append("trial %s drive %s: %s" % (t.get("index"), address, c["missing"]))
            if kind == "held-stop":
                o = (t.get("stop") or {}).get("output_after") or {}
                if o.get("off_reason") != "motor_stop" or o.get("active"):
                    fails.append("trial %s: output after the stop %s" % (t.get("index"), o))
            if kind in ("input-stop", "group-input-stop") and not (t.get("stop_pulse") or {}).get("on_us"):
                open_.append("trial %s: no X3 pulse (%s)" % (t.get("index"), t.get("stop_pulse")))
        if fails:
            return all_levels("FAIL", "; ".join(fails))
        if open_:
            return all_levels("indeterminate", "; ".join(open_))
        return all_levels("PASS", stopped_early)
    if kind == "disarm":
        a, b = detail.get("before_trigger") or {}, detail.get("while_moving") or {}
        sent_ok = a.get("trigger", {}).get("ok") is False and a.get("ons_before") == a.get("ons_after") and \
            not a.get("bus_traffic") and a.get("position_after") == a.get("position_before")
        drive_ok = bool(b.get("reached_target")) and bool((b.get("disarm") or {}).get("reply"))
        return {"sent": ("PASS" if sent_ok else "FAIL", None if sent_ok else "refusal %s" % a),
                "drive-reported": ("PASS" if drive_ok else "FAIL" if b else "NOT RUN",
                                   None if drive_ok else "disarm while moving: %s, final %s, target %s" % (
                                       b.get("disarm"), b.get("final"), b.get("target")))}
    if kind == "disarm-inhibit":
        first, second = detail.get("first_pulse") or {}, detail.get("second_pulse") or {}
        ok = bool(first.get("reached")) and bool(second.get("stayed"))
        return all_levels("PASS" if ok else "FAIL" if second else "NOT RUN",
                          None if ok else "first %s, second %s" % (first.get("final"), second.get("final")))
    if kind == "fixed-length":
        if not trials:
            return all_levels("NOT RUN", stopped_early or "no trial")
        return all_levels("PASS", "recorded: after-edge travel %s" % [t.get("after_edge") for t in trials])
    if kind == "restore-only":
        return all_levels("PASS" if not error else "FAIL", "restoration result in the run record" if not error else stopped_early)
    if kind == "regression":
        moves, group = detail.get("moves") or [], detail.get("group") or {}
        reads_ok = all(all(v for v in r.values()) for r in (detail.get("reads") or {}).values()) and detail.get("reads")
        moves_ok = len(moves) == len(DRIVES) and all(m.get("accepted") and m.get("stopped") and m.get("running_seen")
                                                     for m in moves)
        group_ok = bool(group.get("ok"))
        ok = bool(reads_ok and moves_ok and group_ok)
        return all_levels("PASS" if ok else "FAIL" if moves or group else "NOT RUN",
                          None if ok else "reads %s, moves %s, group %s" % (bool(reads_ok), moves, group))
    if kind == "serial-endpoint":
        if not trials:
            return all_levels("NOT RUN", stopped_early or "no trial")
        refused = [t for t in trials if t.get("refused") or not (t.get("terminal") or {}).get("ok")]
        if refused:
            return all_levels("indeterminate", "%d moves refused or failed" % len(refused))
        return all_levels("PASS", "recorded (diagnostic): final - target per drive and direction %s" % detail.get("errors"))
    if kind == "interrupt":
        if not trials:
            return all_levels("NOT RUN", stopped_early or "no trial")
        refused = [t for t in trials if t.get("refused") or not t.get("interrupt_accepted")]
        out = all_levels("PASS", "recorded")
        if case["form"] == "absolute" if "form" in case else False:
            bad = [t["index"] for t in trials if not (t.get("analysis") or {}).get("ends_at_target")]
            out = all_levels("FAIL" if bad else "PASS", "trials %s did not end at the target" % bad if bad else None)
        if refused:
            out = {level: (v if v == "FAIL" else "indeterminate", "%d interrupts not accepted" % len(refused))
                   for level, (v, _) in out.items()}
        return out
    return all_levels("NOT RUN", "unknown case kind")
