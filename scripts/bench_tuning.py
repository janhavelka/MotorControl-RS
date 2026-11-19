"""Wired bench 15: control and tuning settings with measured effects.

Reference move M: relative +2000 native steps at 120 rpm with the configured
ramps (100 ms), then a relative return to the position read before it. Drive
4's Y0 is wired to the board input ``aux1``, read inverted (Y0 on -> aux1
inactive): Y0 = 2 (running) brackets the move, Y0 = 4 (in position) the
arrival, Y0 = 5 (brake) the enable/release and lock transitions. Each case
changes one field through its ordinary setter (``control set`` or ``tuning
GROUP set``; wired bench 15 candidate rows, SettingCandidates.h), runs M,
restores the field through the same setter (the candidate row or the D7 exact
restore of the pinned read, whose admission is first checked without a frame
by ``tuning candidates check``), reads it back and runs M again.

Alarm 5 (deviation) is a stop condition except in T3 and T9, and only on drives
2 and 4 (D15): the move ends without a reply, the drive is read after 2 s, an
explicit recovery runs only when needed, then ``stop fast`` and ``alarm-clear``;
an alarm that ``alarm-clear`` does not clear ends the run. Every trial loop
sends the prepared ``stop fast`` when it raises. Pure analysis functions are
unit-tested (``test/bench_tuning_test.py``).
"""
import time

import bench_input_starts as starts
import bench_motion as motion
import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stops
import bench_switch
import bench_velocity as velocity

OWNER = records.FIXTURE["y0_owner"]
ALARM_DRIVES = (2, 4)          # D15 item 3: drives that have cleared alarm 5 with alarm-clear before.
M_STEPS, M_RPM, M_FAST_RPM = 2000, 120, 600
MOVES, RETURNS = 5, 2          # Per candidate: M five times, restore, M twice.
BASELINE_MOVES = 10
RUNNING, IN_POSITION, RELEASED, ALARM = 4, 1, 16, 8
BRAKE_WATCH_S = 1.5            # After a stop: aux1 watched this long beyond the lock time.
THRESHOLD_STEPS = 12           # T3 bisection bound per speed.
POLL_PAUSE_S = 0.02            # Between read-state polls of a move: bounds the evidence of long series.
SILENT_S = 1.0                 # Failed reads this long during a move: alarm 5 silence, recover before waiting.

# Field keys (planner keys) and the case values of prompt 15's table.
GAIN_KEYS = [("current-loop", n) for n in ("multiplier", "kp", "ki", "kc")]
LA_KEYS = [("la", n) for n in ("kp1", "kv1", "node1", "kp2", "kv2", "node2", "kvf", "position-ki")]

CASES = {
    "B": dict(kind="baseline", levels=["controller-timestamped", "drive-reported"],
              assertion="M x10 with Y0 = 2 and x10 with Y0 = 4 (move time, arrival after the running edge, final "
                        "position, IN_POSITION polls); Y0 = 5 around enable, release and a stop (lock delay)"),
    "T1": dict(kind="candidates", key="filters.arrival-window", values=(1, 50, 256), y0=4,
               levels=["controller-timestamped"], assertion="arrival window 1, 50, 256: arrival edge against the baseline spread"),
    "T2": dict(kind="candidates", key="filters.arrival-time", values=(0, 50, 200), y0=4,
               levels=["controller-timestamped"], assertion="arrival time 0, 50, 200: edge delay against the word"),
    "T3": dict(kind="threshold", levels=["drive-reported", "sent"],
               assertion="lowest deviation threshold with no alarm 5 for M at 120 and 600 rpm (<= 12 steps each); "
                         "8000 and 65535 once each after the D6 call; above 4000 refused without it, zero TX"),
    # Pulse mean 1 for the prompt's 0: a written 0 reads back 1 (twice on drive 4); the row starts at 1.
    "T4": dict(kind="candidates", keys=(("filters.pulse-low-pass", (1, 1024)), ("filters.pulse-mean", (1, 256))), y0=2,
               levels=["controller-timestamped"], assertion="pulse low-pass 1, 1024 and pulse mean 1, 256: serial moves "
                                                            "against the baseline spread"),
    "T5": dict(kind="gains", keys=GAIN_KEYS, y0=2, levels=["controller-timestamped"],
               assertion="each current-loop gain -25 % then +25 % of the pinned word: M completes; timing"),
    "T6": dict(kind="gains", keys=LA_KEYS, y0=2, levels=["controller-timestamped"],
               assertion="each LA field -25 % then +25 % of the pinned word: M completes; timing"),
    "T7": dict(kind="candidates", key="control.lock-delay", values=(1000, 3000), y0=5, brake=True,
               levels=["controller-timestamped"], assertion="lock time 1000, 3000: brake edge after the stop against the word"),
    "T8": dict(kind="candidates", key="control.lock-current", values=None, y0=5, brake=True, listed=(50,),
               levels=["drive-reported"], assertion="lock current 50 % of the pinned word (listed row), restore proven "
                                                    "first: nothing observable expected"),
    "T9": dict(kind="closed-loop", levels=["drive-reported"],
               assertion="closed-loop maximum 50, 25, 10 % of the pinned word, restore proven before each: M at 600 rpm, "
                         "ramp 100; lag or alarm 5 as the word falls; lowest passing word"),
    "T10": dict(kind="not-run", levels=["drive-reported"], reason="needs T11 (open-loop maximum under algorithm 1)",
                assertion="open-loop maximum under algorithm 1"),
    "T11": dict(kind="algorithm", levels=["sent"],
                assertion="control set algorithm 1 and 2 refuse before TX (no qualified forward transition)"),
    "T12": dict(kind="not-run", levels=["drive-reported"], reason="deferred: owner away (D15; encoder change supervision)",
                assertion="encoder 4000 -> 2000 in closed loop, one +200 move at 30 rpm"),
    "T13": dict(kind="collision", levels=["read back"],
                assertion="collision 200 and 50 written and read back, then the D7 exact restore of 0/0, proven first"),
    "T14": dict(kind="not-run", levels=["read back"],
                reason="no route reaches a codec-refused register: the FC03 0x003B/2 probe needs a ledger access field "
                       "and a bounded read route first (D8); not built in this change",
                assertion="FC03 0x003B/2 on drives 1, 2, 4"),
}


# --------------------------------------------------------------------------
# Analysis (pure; unit-tested)

def edge_pair(edges, t0, t1, first_active):
    """The first edge with ``active == first_active`` in [t0, t1] and the first
    opposite edge after it, as (first_us, second_us); None for a missing one."""
    first = next((e["at_us"] for e in edges if t0 <= e["at_us"] <= t1 and e["active"] == first_active), None)
    if first is None:
        return None, None
    second = next((e["at_us"] for e in edges if first < e["at_us"] <= t1 and e["active"] != first_active), None)
    return first, second


def y0_interval(trial: dict, y0: int):
    """Controller-clock interval of one move from aux1 (inverted): Y0 = 2 ON
    (aux1 inactive) to OFF, Y0 = 4 OFF (aux1 active) to ON again, in ms."""
    if y0 not in (2, 4):
        return None
    first, second = edge_pair(trial.get("edges") or [], trial["t0_us"], trial["t1_us"], y0 == 4)
    return None if first is None or second is None else (second - first) / 1000.0


def percent(word: int, value: int) -> int:
    """round(word x value / 100) half up, as the application's candidatePercent."""
    return (word * value + 50) // 100


def spread(values):
    v = [x for x in values if x is not None]
    if not v:
        return None
    return {"n": len(v), "min": round(min(v), 2), "max": round(max(v), 2), "mean": round(sum(v) / len(v), 2)}


def beyond(values, base) -> str:
    """'earlier', 'later' or 'within' the baseline [min, max]; 'unknown' without data."""
    s = spread(values)
    if s is None or base is None:
        return "unknown"
    if s["max"] < base["min"]:
        return "earlier"
    if s["min"] > base["max"]:
        return "later"
    return "within"


def bisect_next(passing: int, alarming: int):
    """Next candidate between the highest alarming and the lowest passing word, or None when adjacent."""
    if passing - alarming <= 1:
        return None
    return (passing + alarming) // 2


def first_in_position(samples, after_us):
    """Controller time (reply end) of the first sample with IN_POSITION and not
    RUNNING that follows a sample, after ``after_us``, that showed RUNNING or
    no IN_POSITION (the move left its position); None if never seen."""
    left = False
    for s in samples:
        end = s["windows"]["motion"][1]
        if end < after_us:
            continue
        if s["motion"] & RUNNING or not s["motion"] & IN_POSITION:
            left = True
        elif left:
            return end
    return None


def brake_edges(edges, stop_us, end_us):
    """aux1 edges after a stop (Y0 = 5), as ms after the stop with their level."""
    return [{"after_ms": round((e["at_us"] - stop_us) / 1000.0, 1), "active": e["active"]}
            for e in edges if stop_us <= e["at_us"] <= end_us]


# --------------------------------------------------------------------------
# Live helpers

class Live(starts.Live):
    """Run state of one control-and-tuning run on one drive (``address``)."""

    def __init__(self, console, run, baseline, args, timeout):
        starts.Live.__init__(self, console, run, baseline, args, timeout)
        self.facts = velocity.facts()
        self.address = args.address
        self.inverted = False
        self.relay_operations = 0
        self.alarms = []


def headroom(live):
    problem = bench_switch.headroom_problem(live.console)
    if problem:
        raise probe.BenchError(problem)


def prepared_stop(live):
    return live.call("stop", stop_policy="fast", address=live.address)


def baseline_value(live, key):
    return records.baseline_field(live.baseline, live.address, key)["value"]


def write(live, key, value):
    """One ordinary write with its planned facts, read back (bench_velocity.write_field)."""
    return velocity.write_field(live, key, value)


def setter_step(live, key, value):
    """The planned ordinary step of one field write (its route, facts and reads)."""
    entry = {"drive": live.address, "key": key, "index": None, "candidate": value}
    steps, refusal = records.plan_entry(live.baseline, entry, live.args.image_level, live.facts, restore=False)
    if refusal:
        raise probe.BenchError("no ordinary route for %s: %s" % (key, refusal["reason"]))
    return dict(steps[0], target=value, candidate=value)


def restore_check(live, key, value):
    """``tuning candidates check`` of one field and value after the same fresh
    reads and stated facts as the setter: the admission without a frame."""
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    records._prerequisites(live.console, setter_step(live, key, value), timeout_s=live.timeout)
    group, _, name = key.partition(".")
    tokens = ("check", "control" if group == "control" else group, name, str(value), str(live.address))
    reply = live.call("tuning-candidates", host_args=tokens)
    return {k: reply.get(k) for k in ("ok", "result", "admitted", "rule", "reason", "pin_known", "pin_operation_id",
                                      "pin_word", "core_status", "core_detail")}


def motion_ready(live):
    """Selected drive: config, a profile read (snapshot kept once) and the start-speed relation."""
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    live.checked("read-config", address=live.address)
    live.read_state(live.address)
    try:
        profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
    except probe.BenchError:
        live.checked("motion-profile", host_args=("forget",))
        profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
        live.snapshots["profile_replaced"] = True
    live.snapshots.setdefault("profile", {k: profile.get(k) for k in ("original", "current")})
    word = str(profile["current"][0])
    live.call("motion-profile-start-speed-relation", host_args=(word, word))


def recover_alarm(live) -> dict:
    """After a move: read; if silent, wait 2 s, one explicit recovery only when
    needed, read; an alarm: stop fast, alarm-clear (drives 2 and 4 only), and
    enable when released. An alarm that alarm-clear does not clear raises."""
    out = {}
    reply = live.call("read-state", address=live.address)
    if not reply.get("ok"):
        out["silent"] = reply.get("result")
        time.sleep(2.0)
        reply = live.call("read-state", address=live.address)
        if not reply.get("ok"):
            handle = live.console.begin("recover", timeout_s=max(live.timeout, 10))
            out["recover"] = live.console.wait(handle, release=True).get("ok") if handle.accepted else "refused"
            reply = live.call("read-state", address=live.address)
    s = stops.sample(reply)
    if s:
        live.samples.append(s)
    out["alarm"] = 0 if s is not None else None
    if s and (s["alarm"] or s["motion"] & ALARM):
        out["alarm"] = s["alarm"] or "flag"
        if live.address not in ALARM_DRIVES:
            prepared_stop(live)
            raise probe.BenchError("alarm %s on drive %d, which may not hold alarm 5 under D15" % (out["alarm"], live.address))
        out["stop"] = prepared_stop(live).get("outcome")
        time.sleep(0.5)
        # alarm-clear against a held conflict needs two fresh reads after it began:
        # the alarm with RELEASED at speed 0, the same position in both.
        live.read_state(live.address)
        live.read_state(live.address)
        out["alarm_clear"] = live.call("alarm-clear", address=live.address).get("outcome")
        s = stops.sample(live.call("read-state", address=live.address))
        if s is None or s["alarm"] or s["motion"] & ALARM:
            prepared_stop(live)
            raise probe.BenchError("alarm on drive %d not cleared by alarm-clear; tuning stops (D15)" % live.address)
        if s["motion"] & RELEASED:
            out["enable"] = live.call("enable", address=live.address).get("outcome")
            time.sleep(1.1)
        live.alarms.append(dict(out, at_us=live.view()["now_us"]))
    return out


def relative(live, steps, rpm):
    """One finite relative move with read-state polling to its end; the prepared stop if anything raises."""
    handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", str(rpm), "configured"),
                                address=live.address, timeout_s=max(live.timeout, 20))
    if not handle.accepted:
        return {"refused": live.console.wait(handle).get("result")}, []
    first = len(live.samples)
    silent_since = None
    try:
        for _ in range(1500):
            s = live.read_state(live.address)
            if handle.terminal is not None and (s is None or (not s["motion"] & RUNNING and s["speed"] == 0)):
                break
            # Alarm 5 silences the drive for 1.0-1.9 s and the bus owner then
            # needs recovery before the move can end (wired bench 15: waiting
            # for the move's terminal instead lost the console).
            silent_since = (silent_since or time.monotonic()) if s is None else None
            if silent_since is not None and time.monotonic() - silent_since > SILENT_S:
                break
            time.sleep(POLL_PAUSE_S)
        recovered = None
        if handle.terminal is None and silent_since is not None:
            time.sleep(2.0)
            recovery = live.console.begin("recover", timeout_s=max(live.timeout, 10))
            recovered = live.console.wait(recovery, release=True).get("ok") if recovery.accepted else "refused"
        terminal = live.console.wait(handle, release=True)
        if recovered is not None:
            terminal = dict(terminal, recovered_while_silent=recovered)
    except BaseException:
        prepared_stop(live)
        raise
    return terminal, live.samples[first:]


def reconcile(live) -> dict:
    """The prepared stop fast and a fresh stopped read: an uncertain move leaves
    an address conflict that only a checked stop settles (wired bench 15 T3)."""
    out = {"stop": prepared_stop(live).get("outcome")}
    st = probe.observe_stopped(live.console, address=live.address, timeout_s=live.timeout)
    out["stopped"] = probe.drive_reported_stopped(st)
    return out


def move_m(live, rpm=M_RPM, steps=M_STEPS, watch_brake_s=None, before_return=None):
    """M: +steps at ``rpm``, then the return to the position read before it.
    ``before_return(live)`` runs between them (T3 restores the threshold, so
    the return never runs at a tested word). A forward or return move that did
    not end checked is reconciled by the prepared stop. Returns the forward
    trial (edges, interval bounds, terminal summary, final position, first
    IN_POSITION sample) and the return summary."""
    headroom(live)
    motion_ready(live)
    before = live.read_state(live.address)
    start = stops.position(before) if before else None
    v0 = live.view()
    terminal, samples = relative(live, steps, rpm)
    for _ in range(3):
        s = live.read_state(live.address)
        if s:
            samples.append(s)
    trial = {"rpm": rpm, "steps": steps, "start_position": start, "t0_us": v0["now_us"]}
    if watch_brake_s:
        stop_us = live.view()["now_us"]
        deadline = time.monotonic() + watch_brake_s
        while time.monotonic() < deadline:
            live.read_state(live.address)
            time.sleep(0.1)
        trial["stop_view_us"] = stop_us
    v1 = live.view()
    trial["t1_us"] = v1["now_us"]
    trial["terminal"] = {k: terminal.get(k) for k in ("ok", "outcome", "status", "detail", "completion", "polls",
                                                      "started_us", "refused", "recovered_while_silent") if k in terminal}
    found, lost = live.edges()
    trial["edges"] = [e for e in found if trial["t0_us"] <= e["at_us"] <= trial["t1_us"]]
    trial["lost_edges"] = [w for w in lost if w["to_us"] and w["to_us"] >= trial["t0_us"]]
    good = [s for s in samples if s]
    trial["final_position"] = stops.position(good[-1]) if good else None
    started = (terminal.get("started_us") or trial["t0_us"])
    trial["in_position_us"] = first_in_position(good, started)
    trial["in_position_ms"] = None if trial["in_position_us"] is None else round((trial["in_position_us"] - started) / 1000.0, 1)
    trial["alarm"] = recover_alarm(live)
    if not terminal.get("ok"):
        trial["reconcile"] = reconcile(live)
    if before_return is not None:
        trial["before_return"] = before_return(live)
    # The return: to the position read before the move, whatever the forward move did.
    now = live.read_state(live.address)
    here = stops.position(now) if now else None
    trial["return"] = None
    if start is not None and here is not None and here != start:
        motion_ready(live)
        back, _ = relative(live, start - here, M_RPM)
        trial["return"] = {"steps": start - here, "ok": back.get("ok"), "outcome": back.get("outcome")}
        trial["return_alarm"] = recover_alarm(live)
        if not back.get("ok"):
            trial["return_reconcile"] = reconcile(live)
        end = live.read_state(live.address)
        trial["returned_to"] = stops.position(end) if end else None
    return trial


def moves(live, count, y0, rpm=M_RPM, brake=False):
    out = []
    for _ in range(count):
        t = move_m(live, rpm, watch_brake_s=(lock_delay_s(live) + BRAKE_WATCH_S) if brake else None)
        t["interval_ms"] = y0_interval(t, y0)
        if brake and t.get("stop_view_us"):
            t["brake_edges"] = brake_edges(t["edges"], t["stop_view_us"] - 1000000, t["t1_us"])
        out.append(t)
    return out


def lock_delay_s(live):
    reply = live.checked("control", driver_args=("read",), address=live.address)
    return (reply.get("observation") or {}).get("raw", [0] * 8)[7] / 1000.0


def set_y0(live, function):
    if velocity.io_words(live)[6] != function:
        write(live, "io.y0", function)


def summary(trials):
    return {"interval_ms": spread([t.get("interval_ms") for t in trials]),
            "in_position_ms": spread([t.get("in_position_ms") for t in trials]),
            "final_position": sorted({t.get("final_position") for t in trials}, key=lambda v: (v is None, v)),
            "alarms": sum(1 for t in trials if (t.get("alarm") or {}).get("alarm")),
            "refused": sum(1 for t in trials if t["terminal"].get("refused"))}


def slim(trial):
    return {k: v for k, v in trial.items() if k not in ("samples",)}


# --------------------------------------------------------------------------
# Cases

def case_baseline(live, out):
    if live.address == OWNER:
        for y0 in (2, 4):
            set_y0(live, y0)
            out["y0_%d" % y0] = [slim(t) for t in moves(live, BASELINE_MOVES, y0)]
        set_y0(live, 5)
        rows = []
        # Assigned while enabled, function 5 leaves Y0 off until the next enable
        # (wired bench 06 O8): the first cycle sets it, the second is measured.
        for label in ("motor-release", "enable", "motor-release", "enable"):
            v0 = live.view()["now_us"]
            reply = live.checked(label, address=live.address)
            for _ in range(3):
                live.read_state(live.address)
            time.sleep(1.1 if label == "enable" else 0.3)
            v1 = live.view()["now_us"]
            found, _ = live.edges()
            rows.append({"command": label, "started_us": reply.get("started_us"),
                         "edges": [e for e in found if v0 <= e["at_us"] <= v1]})
        out["brake_commands"] = rows
        out["brake_stop"] = [slim(t) for t in moves(live, 2, 5, brake=True)]
        set_y0(live, baseline_value(live, "io.y0"))
    else:
        out["polls"] = [slim(t) for t in moves(live, 3, None)]
    return out


def candidate_series(live, key, values, y0, rows, rpm=M_RPM, brake=False):
    """One field through its values into ``rows`` (kept when a later step
    raises): proof, change, M x5, restore, read back, M x2."""
    base = baseline_value(live, key)
    for value in values:
        proof = restore_check(live, key, base)
        row = {"key": key, "value": value, "restore_proof": proof}
        rows.append(row)
        if not proof.get("admitted"):
            row["not_run"] = "restore not proven: %s" % proof.get("reason")
            continue
        write(live, key, value)
        try:
            row["changed"] = [slim(t) for t in moves(live, MOVES, y0, rpm, brake)]
        finally:
            row["restore"] = {"readback": write(live, key, base), "rule": proof.get("rule")}
        row["returned"] = [slim(t) for t in moves(live, RETURNS, y0, rpm, brake)]
        row["summary"] = summary(row["changed"])
        if any((t.get("alarm") or {}).get("alarm") for t in row["changed"]):
            row["stopped"] = "alarm during the candidate; series ends"
            break
    return rows


def brake_setup(live):
    """Y0 = 5 assigned while enabled stays off until the next enable (wired bench
    06 O8): one release/enable cycle, then aux1 must read inactive (Y0 on)."""
    for label, wait_s in (("motor-release", 0.3), ("enable", 1.1)):
        live.checked(label, address=live.address)
        time.sleep(wait_s)
    view = live.view()
    if view["input"]["active"]:
        raise probe.BenchError("Y0 = 5 not on after the enable (aux1 active); brake edges cannot be observed")
    return {"aux1_active": view["input"]["active"], "now_us": view["now_us"]}


def case_candidates(live, cid, out):
    case = CASES[cid]
    if live.address != OWNER and case.get("y0"):
        out["not_run"] = "Y0 timing needs drive 4"
        return out
    set_y0(live, case["y0"])
    if case.get("brake"):
        out["brake_setup"] = brake_setup(live)
    out["baseline"] = [slim(t) for t in moves(live, MOVES, case["y0"], brake=case.get("brake", False))]
    try:
        if case.get("keys"):
            for key, values in case["keys"]:
                candidate_series(live, key, values, case["y0"], out.setdefault(key, []))
        else:
            values = case["values"]
            if values is None:
                pin = restore_check(live, case["key"], baseline_value(live, case["key"]))
                values = [percent(pin["pin_word"], p) for p in case["listed"]] if pin.get("pin_known") else []
            candidate_series(live, case["key"], values, case["y0"], out.setdefault(case["key"], []),
                             brake=case.get("brake", False))
    finally:
        set_y0(live, baseline_value(live, "io.y0"))
    return out


def case_gains(live, cid, out):
    case = CASES[cid]
    set_y0(live, case["y0"])
    out["baseline"] = [slim(t) for t in moves(live, MOVES, case["y0"])]
    try:
        for group, name in case["keys"]:
            key = "%s.%s" % (group, name)
            if live.args.fields and name not in live.args.fields:
                continue
            base = baseline_value(live, key)
            values = [percent(base, 75), percent(base, 125)]  # Decreases first (D15).
            candidate_series(live, key, values, case["y0"], out.setdefault(key, []))
            if any(r.get("stopped") for r in out[key]):
                out["stopped"] = key
                break
    finally:
        set_y0(live, baseline_value(live, "io.y0"))
    return out


def threshold_move(live, value, rpm):
    """M at ``rpm`` with the threshold ``value``; the baseline word is back before the return."""
    key = "filters.deviation-threshold"
    write(live, key, value)
    t = move_m(live, rpm, before_return=lambda l: write(l, key, baseline_value(l, key)))
    return {"value": value, "rpm": rpm, "alarm": (t.get("alarm") or {}).get("alarm"), "trial": slim(t)}


def case_threshold(live, out):
    if live.address not in ALARM_DRIVES:
        out["not_run"] = "alarm 5 only on drives 2 and 4 (D15)"
        return out
    key = "filters.deviation-threshold"
    base = baseline_value(live, key)
    out.update(restore_proof=restore_check(live, key, base), brackets={})
    try:
        for rpm in (M_RPM, M_FAST_RPM):
            passing, alarming, steps = base, 0, []
            out["brackets"][rpm] = {"steps": steps}
            first = threshold_move(live, base, rpm)
            steps.append(first)
            if first["alarm"]:
                out["brackets"][rpm] = {"steps": steps, "note": "alarm at the baseline word"}
                continue
            candidate = 1
            while len(steps) < THRESHOLD_STEPS and candidate is not None:
                row = threshold_move(live, candidate, rpm)
                steps.append(row)
                if row["alarm"]:
                    alarming = candidate
                else:
                    passing = candidate
                candidate = bisect_next(passing, alarming)
            out["brackets"][rpm].update(lowest_passing=passing, highest_alarming=alarming)
        write(live, key, base)
        # D6/D12: above 4000 refuses without the call, before any frame.
        out["without_call"] = refused_write(live, key, 8000)
        out["raised"] = []
        for value in (8000, 65535):
            call = live.checked("tuning-candidates", host_args=("threshold", str(value)))
            row = threshold_move(live, value, M_RPM)
            row["call"] = {k: call.get(k) for k in ("threshold_active", "threshold_value")}
            out["raised"].append(row)
        live.checked("tuning-candidates", host_args=("reset",))
        out["after_reset"] = refused_write(live, key, 65535)
    finally:
        live.call("tuning-candidates", host_args=("reset",))
        write(live, key, base)
    return out


def refused_write(live, key, value):
    """A setter expected to refuse before any frame, after the setter's own
    fresh reads and facts; the latest operation is taken right around it."""
    group, _, name = key.partition(".")
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    records._prerequisites(live.console, setter_step(live, key, value), timeout_s=live.timeout)
    command = "control" if group == "control" else "tuning"
    args = ("set", name, str(value)) if group == "control" else (group, "set", name, str(value))
    latest = live.checked("drv").get("operation_id")
    handle = live.console.begin(command, driver_args=args, address=live.address, timeout_s=live.timeout)
    if handle.accepted:
        terminal = live.console.wait(handle, release=True)
        return {"accepted": True, "terminal_ok": terminal.get("ok"), "latest_before": latest,
                "latest_after": live.checked("drv").get("operation_id")}
    reply = live.console.wait(handle)
    return {"accepted": False, "result": reply.get("result"), "latest_before": latest,
            "latest_after": live.checked("drv").get("operation_id")}


def case_closed_loop(live, out):
    if live.address not in ALARM_DRIVES:
        out["not_run"] = "alarm 5 possible; drives 2 and 4 only (D15)"
        return out
    key = "control.closed-maximum-current"
    y0 = 2 if live.address == OWNER else None
    if y0:
        set_y0(live, y0)
    base = baseline_value(live, key)
    out.update(rows=[], baseline=[slim(t) for t in moves(live, MOVES, y0, M_FAST_RPM)])
    try:
        for share in (50, 25, 10):
            value = percent(base, share)
            proof = restore_check(live, key, base)
            row = {"percent": share, "value": value, "restore_proof": proof}
            out["rows"].append(row)
            if not proof.get("admitted") or proof.get("rule") != "exact_restore":
                row["not_run"] = "restore not proven: %s %s" % (proof.get("rule"), proof.get("reason"))
                break
            write(live, key, value)
            try:
                row["changed"] = [slim(t) for t in moves(live, MOVES, y0, M_FAST_RPM)]
            finally:
                row["restore"] = write(live, key, base)
            row["summary"] = summary(row["changed"])
            row["returned"] = [slim(t) for t in moves(live, RETURNS, y0, M_FAST_RPM)]
            if row["summary"]["alarms"]:
                row["stopped"] = "alarm 5 at %d %%; the descent ends" % share
                break
    finally:
        if y0:
            set_y0(live, baseline_value(live, "io.y0"))
    return out


def case_algorithm(live):
    out = {"checks": {}, "writes": {}}
    for value in (1, 2):
        out["checks"][value] = restore_check(live, "control.algorithm", value)
        out["writes"][value] = refused_write(live, "control.algorithm", value)
    out["restore_proof"] = restore_check(live, "control.algorithm", baseline_value(live, "control.algorithm"))
    return out


def case_collision(live):
    out = {}
    for key, value in (("collision.threshold", 200), ("collision.current", 50)):
        base = baseline_value(live, key)
        proof = restore_check(live, key, base)
        row = {"value": value, "baseline": base, "restore_proof": proof}
        out[key] = row
        if not proof.get("admitted") or proof.get("rule") != "exact_restore":
            row["not_run"] = "D7 restore not proven: %s %s" % (proof.get("rule"), proof.get("reason"))
            continue
        row["readback"] = write(live, key, value)
        row["restored"] = write(live, key, base)
    return out


def run_cases(live, args, cases):
    detail = {"cases": {}, "address": live.address}
    live.partial = detail["cases"]
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    # The pinned reads (the D7 sources): every control and tuning group read once before any write.
    detail["pins"] = {}
    for group in ("control", "tuning.filters", "tuning.current-loop", "tuning.la", "tuning.collision"):
        entry = records.read_group(live.console, live.address, group, timeout_s=live.timeout)
        detail["pins"][group] = {"operation_id": entry.get("operation_id"), "raw": entry.get("raw")}
    live.call("tuning-candidates", host_args=("reset",))
    for cid in cases:
        kind = CASES[cid]["kind"]
        try:
            if kind == "not-run":
                detail["cases"][cid] = {"note": CASES[cid]["reason"]}
            elif kind == "baseline":
                case_baseline(live, detail["cases"].setdefault(cid, {}))
            elif kind == "candidates":
                case_candidates(live, cid, detail["cases"].setdefault(cid, {}))
            elif kind == "gains":
                case_gains(live, cid, detail["cases"].setdefault(cid, {}))
            elif kind == "threshold":
                case_threshold(live, detail["cases"].setdefault(cid, {}))
            elif kind == "closed-loop":
                case_closed_loop(live, detail["cases"].setdefault(cid, {}))
            elif kind == "algorithm":
                detail["cases"][cid] = case_algorithm(live)
            elif kind == "collision":
                detail["cases"][cid] = case_collision(live)
        except BaseException:
            prepared_stop(live)
            raise
    detail["alarms"] = live.alarms
    detail["relay_operations"] = live.relay_operations
    return detail


# --------------------------------------------------------------------------
# Plan and verdicts

def plan(args, baseline, facts_):
    """Scenario-record fields of one control-tuning run, before any port access."""
    address = args.address
    keys = set()
    for cid in args.cases:
        case = CASES[cid]
        if case.get("key"):
            keys.add(case["key"])
        for key, _ in case.get("keys", ()) if case["kind"] == "candidates" else ():
            keys.add(key)
        if case["kind"] == "gains":
            keys.update("%s.%s" % k for k in case["keys"])
        if case["kind"] == "threshold":
            keys.add("filters.deviation-threshold")
        if case["kind"] == "closed-loop":
            keys.add("control.closed-maximum-current")
        if case["kind"] == "collision":
            keys.update(("collision.threshold", "collision.current"))
        if address == OWNER and (case.get("y0") or case["kind"] in ("baseline", "closed-loop")):
            keys.add("io.y0")
    write_list = [{"drive": address, "key": k, "index": None, "candidate": None} for k in sorted(keys)]
    write_list += [{"drive": address, "key": k, "index": None, "candidate": None}
                   for k in ("profile.acceleration", "profile.deceleration", "profile.speed", "profile.target")]
    restoration = records.restoration_plan(baseline, write_list, args.image_level, facts_)
    cases = [{"id": c, "required": True, "assertion": CASES[c]["assertion"], "required_levels": list(CASES[c]["levels"])}
             for c in args.cases]
    commands = ["control read A; tuning filters|current-loop|la|collision read A (the pinned D7 sources)",
                "tuning candidates reset",
                "per candidate: tuning candidates check GROUP FIELD BASELINE A (restore proof, no frame); the setter "
                "(control set / tuning GROUP set) with its fresh reads and facts; M x5; the setter back to the "
                "baseline word; read back; M x2",
                "M: read config, read state, motion-profile read, start-speed relation, bench, move relative 2000 "
                "steps native RPM configured, read state to the end, bench; the return move to the start position",
                "alarm 5 (T3, T9; drives 2, 4): 2 s, recover only if needed, stop fast, alarm-clear, enable if released",
                "T3: deviation threshold bisection at 120 and 600 rpm; tuning candidates threshold 8000|65535; reset",
                "restoration: stop fast, alarm-clear/enable if needed, the plan (Y0, settings, profile stage)"]
    bounds = {"address": address, "move_steps": M_STEPS, "rpm": [M_RPM, M_FAST_RPM], "moves_per_candidate": MOVES,
              "returns": RETURNS, "threshold_steps": THRESHOLD_STEPS, "alarm_drives": list(ALARM_DRIVES),
              "exposure": "every move is finite (2000 steps); the controller's own position move ends it"}
    expected = {"controller-timestamped": "aux1 edges of Y0 2/4/5 per move (tuning.json)",
                "drive-reported": "final positions, IN_POSITION samples, alarms and recoveries",
                "read back": "setter readbacks; the D7 restore terminals name their source read",
                "sent": "refused setters leave the latest operation unchanged"}
    return write_list, restoration, cases, commands, bounds, expected, []


def _not_reached(run, cid, reason, ev):
    for level in CASES[cid]["levels"]:
        run.verdict(cid, level, "NOT RUN", reason, ev)


def case_verdicts(run, cid, detail, error):
    case = CASES[cid]
    out = (detail.get("cases") or {}).get(cid)
    ev = ["tuning.json"]
    if case["kind"] == "not-run":
        _not_reached(run, cid, case["reason"], ev)
        return
    if not out:
        _not_reached(run, cid, "not reached: %s" % (error or "no record"), ev)
        return
    if out.get("not_run"):
        _not_reached(run, cid, out["not_run"], ev)
        return
    kind = case["kind"]
    if kind == "algorithm":
        ok = all(not w["accepted"] and w["latest_before"] == w["latest_after"] for w in out["writes"].values()) and \
            all(not c.get("admitted") and c.get("reason") == "algorithm_refused" for c in out["checks"].values())
        run.verdict(cid, "sent", "PASS" if ok else "FAIL", "algorithm 1/2: %s" % {
            v: (w.get("result"), c.get("reason")) for (v, w), c in zip(out["writes"].items(), out["checks"].values())}, ev)
        return
    if kind == "collision":
        notes, ok = [], True
        for key, row in out.items():
            if row.get("not_run"):
                ok = False
                notes.append("%s %s" % (key, row["not_run"]))
                continue
            good = row.get("readback") == row["value"] and row.get("restored") == row["baseline"]
            ok = ok and good
            notes.append("%s %s read back %s, D7 restore to %s (source %s)" % (key, row["value"], row.get("readback"),
                                                                             row.get("restored"),
                                                                             row["restore_proof"].get("pin_operation_id")))
        run.verdict(cid, "read back", "PASS" if ok else "FAIL", "; ".join(notes), ev)
        return
    if kind == "baseline":
        sets = {k: v for k, v in out.items() if k.startswith("y0_") or k == "polls"}
        trials = [t for v in sets.values() for t in v]
        ok = trials and all(not t["terminal"].get("refused") and t["terminal"].get("ok") and
                            not (t.get("alarm") or {}).get("alarm") for t in trials)
        notes = ["%s: %s" % (k, summary(v)) for k, v in sets.items()]
        measured = all(t.get("interval_ms") is not None for k, v in sets.items() if k != "polls" for t in v)
        for level in case["levels"]:
            verdict = "PASS" if ok and (measured or level != "controller-timestamped") else "FAIL" if not ok else "indeterminate"
            run.verdict(cid, level, verdict, "; ".join(notes), ev)
        return
    if kind == "threshold":
        notes, ok = [], not error
        for rpm, b in (out.get("brackets") or {}).items():
            notes.append("%s rpm: lowest passing %s, highest alarming %s, %d steps" % (
                rpm, b.get("lowest_passing"), b.get("highest_alarming"), len(b.get("steps", []))))
            ok = ok and b.get("lowest_passing") is not None
        raised = out.get("raised") or []
        notes.append("raised: %s" % [(r["value"], r["alarm"]) for r in raised])
        run.verdict(cid, "drive-reported", "PASS" if ok and len(raised) == 2 else "FAIL", "; ".join(notes), ev)
        w = out.get("without_call") or {}
        sent = not w.get("accepted", True) and w.get("latest_before") == w.get("latest_after") and \
            not (out.get("after_reset") or {}).get("accepted", True)
        run.verdict(cid, "sent", "PASS" if sent else "FAIL", "8000 without the call %s, 65535 after reset %s" % (
            w.get("result"), (out.get("after_reset") or {}).get("result")), ev)
        return
    rows = out.get("rows") or [r for k, v in out.items() if isinstance(v, list) and k != "baseline" for r in v]
    base = summary(out.get("baseline") or [])
    notes, ok, missing = [], not error, False
    for row in rows:
        if row.get("not_run"):
            missing = True
            notes.append("%s %s NOT RUN (%s)" % (row.get("key", ""), row.get("value"), row["not_run"]))
            continue
        s = row.get("summary") or summary(row.get("changed") or [])
        restored = row.get("restore")
        readback = restored.get("readback") if isinstance(restored, dict) else restored
        trend = beyond([t.get("interval_ms") for t in row.get("changed") or []], base["interval_ms"])
        notes.append("%s %s: interval %s (%s baseline %s), in-position %s, alarms %d, restored %s" % (
            row.get("key", ""), row.get("value"), s["interval_ms"], trend, base["interval_ms"], s["in_position_ms"],
            s["alarms"], readback))
        ok = ok and readback is not None
    verdict = "NOT RUN" if missing and not [r for r in rows if not r.get("not_run")] else "PASS" if ok else "FAIL"
    for level in case["levels"]:
        run.verdict(cid, level, verdict, "; ".join(notes) or None, ev)
