"""Wired bench 16: alarms, failed stops and drive failures.

Alarm 5 (position tolerance) is provoked with a lowered deviation threshold
(prompt 15's table: 1..4000 ordinary) on drives 2 and 4 only (D15). Drive 4's
Y0 is wired to the board input ``aux1``, read inverted (Y0 on -> aux1
inactive). While it latches alarm 5, firmware 0x0029 does not answer Modbus
for 1.0-1.9 s; the move or velocity result then ends ``transport_error`` with
the start acknowledged and completion unknown. The reporting rule
(``classify_silence``): one explicit ``recover``, a ``read state`` at least
2.0 s after the last acknowledged write, alarm nonzero -> ``alarm_after_silence``,
no reply -> ``drive_silent``.

The HW-14 (F1) and Q05 (R2) reproduction attempts run above the default
position limits through the named install (``position limits set ... case``,
D6). Every failed stop is classified by the eligibility predicate of prompt
16 step 5 and fresh state; a moving eligible stop runs the release after stop
(F2). Every trial loop sends the prepared ``stop fast`` when it raises. Pure
analysis functions are unit-tested (``test/bench_alarms_test.py``).
"""
import random
import re
import time

import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stops
import bench_tuning as tuning
import bench_velocity as velocity

OWNER = records.FIXTURE["y0_owner"]
ALARM_DRIVES = (2, 4)              # D15 item 3: drives that have cleared alarm 5 with alarm-clear before.
RUNNING, ALARM, RELEASED = 4, 8, 16  # 0x0007 motion word bits (p68).
THRESHOLD_KEY = "filters.deviation-threshold"
ALARM_THRESHOLD = 1                # Prompt 15's lowest admitted value (A1, A2, A4, A5).
ALARM_STEPS, ALARM_RPM = 2000, 60
SILENCE_READS_S = (1.4, 1.7, 2.0)  # After the start ACK; each failed read needs one recovery (at most three).
CLASSIFY_AFTER_S = 2.0             # Implement 3: the deciding read is at least this long after the last ACK.
A3_DELAYS_S = (0.2, 0.5, 1.0)
POLL_S = 0.05                      # Implement 2: fixed read-state interval of the failure scenarios.
HW14 = {"subdivision": 4000, "rpm": 2000, "steps": 40000, "ramp": 500, "band_s": (0.362, 0.392), "trials": 30,
        "limits": (2000, 133334, 100), "case": "hw14"}
Q05 = {"subdivision": 51200, "steps": 12800, "ramp": 100, "rpm": 900, "control_rpm": 2000, "attempts": 3,
       "limits": (2000, 1706667, 100), "case": "q05", "watch_s": 3.0}
STOP_NORMAL_HEX, STOP_FAST_HEX = "0600270100", "0600270200"
TRAFFIC = re.compile(r'"kind":"TX","at_us":(\d+).*?"raw_hex":"([0-9A-Fa-f]*)"')

CASES = {
    "G1": dict(kind="regression", levels=["drive-reported", "sent"],
               assertion="prompt 15 hand-on: threshold 8, +2000 at 120 rpm: alarm 5 mid-move (motion 28); alarm-clear "
                         "after stop fast and one read refused axis_conflict with no frame; after two fresh reads at speed 0 "
                         "and the same position admitted and observed; enable and a tuning write refused until a stop "
                         "fast observes completion, then admitted; threshold back to 4000"),
    "A1": dict(kind="alarm", owner=True, levels=["drive-reported", "controller-timestamped"],
               assertion="drive 4, Y0 = 1, threshold 1, +2000 at 60 rpm x3: alarm 5; first poll 0 RX; aux1 inactive "
                         "(Y0 on) inside the silence bracket; alarm 5 read >= 2 s after the start ACK"),
    "A2": dict(kind="alarm", owner=False, levels=["drive-reported"],
               assertion="drive 2 (drive 1 deferred: owner away, D15), threshold 1, the same move x2: alarm 5, silence bracketed"),
    "A3": dict(kind="clear-start", levels=["drive-reported", "controller-timestamped"],
               assertion="A1 latched: stop fast, alarm-clear, a start 0.2, 0.5, 1.0 s after the clear's ACK; Y0 off "
                         "edge; whether each start is acted on (threshold 1 re-latches alarm 5 only when motion begins)"),
    "A4": dict(kind="alarm", owner=True, polarity=1, levels=["controller-timestamped"],
               assertion="drive 4, 0x004B bit 0 = 1: A1 once; the alarm edge is inverted (aux1 active at the alarm)"),
    "A5": dict(kind="velocity-alarm", levels=["sent", "drive-reported"],
               assertion="drive 4, threshold 1, velocity 60 rpm 1000 ms normal: the poll fails, the run's end stop is "
                         "held for one recovery and sent once; no resend; the sequence is retained in the result"),
    "F1": dict(kind="hw14", levels=["acknowledged", "drive-reported"],
               assertion="drive 4, subdivision 4000, limits 2000/133334/100 case hw14: +40000 at 2000 rpm, ramps 500, "
                         "stop normal swept through 0.362-0.392 s after the start, at most 30 tries; a creep that is "
                         "eligible and moving is a reproduction"),
    "F2": dict(kind="in-f1", levels=["drive-reported"],
               assertion="F1 reproduced: motor-release after-stop ID mechanics-permit-release triggers-inhibited; enable "
                         "and alarm-clear refused; stop fast; enable; +200 move exact"),
    "F3": dict(kind="not-run", levels=["controller-timestamped", "drive-reported"],
               reason="the shared-output interlock has no row for a drive that moves with no pending operation (creep "
                      "after a failed stop); adding one is new machinery (owner scope direction)",
               assertion="F1 reproduced again with X3 = 6 on drive 4: bench on mosfet2 200 after the failed stop"),
    "F4": dict(kind="not-run", levels=["drive-reported"], reason="deferred: owner away (D15; cable pull)",
               assertion="owner pulls drive 4's RS485 plug during a long run; stop fast; reconnect; step 5"),
    "R1": dict(kind="not-run", levels=["drive-reported"], reason="deferred: owner away (D15 item 3: HW-01 needed a power cycle)",
               assertion="HW-01 sequence x20, subdivision 1000, Y0 = 2"),
    "R2": dict(kind="q05", levels=["drive-reported", "controller-timestamped"],
               assertion="drive 4, subdivision 51200, limits 2000/1706667/100 case q05: 51200/100/900/12800 x3 with Y0 = 2 "
                         "and x3 with Y0 = 4, control 51200/100/2000/12800 x3; persistent RUNNING ended by stop fast"),
    "R3": dict(kind="not-run", levels=["drive-reported"], reason="deferred: owner away (D15 item 3: Q04 lost volatile settings)",
               assertion="Q04 move at most 3 times; 500/500 control"),
    "N1": dict(kind="not-run", levels=["drive-reported"],
               reason="Err1 needs a phase fault; Err2 exceeds the 24 V input and 36 V TVS ratings; Err3 needs lowering the "
                      "shared supply that also feeds the board and inputs",
               assertion="Err1, Err2, Err3 not provoked"),
}


# --------------------------------------------------------------------------
# Analysis (pure; unit-tested)

def silent_poll(terminal: dict) -> dict | None:
    """The failing poll of a move or velocity result that ended without a reply
    after an acknowledged start, or None: the trigger reply confirmed, the
    failure step transmitted with 0 RX bytes, completion not observed."""
    if not isinstance(terminal, dict):
        return None
    trigger, failure = terminal.get("trigger_evidence") or {}, terminal.get("failure_evidence") or {}
    if (terminal.get("outcome") != "transport_error" or terminal.get("execution") != "acknowledged" or
            terminal.get("completion") != "not_observed" or not trigger.get("response_confirmed") or
            not failure.get("tx_accepted") or failure.get("received_length") != 0):
        return None
    return {"ack_us": trigger.get("latest_us"), "failed_us": failure.get("delivered_us"),
            "polls": terminal.get("polls"), "first_poll": terminal.get("polls") == 0, "step": failure.get("step")}


def classify_silence(terminal: dict, recovered, reply: dict, ack_us=None) -> dict:
    """Implement 3 rule. ``terminal``: the move or velocity result; ``recovered``:
    whether the one explicit recovery succeeded; ``reply``: the read state taken
    at least CLASSIFY_AFTER_S after ``ack_us`` (the last acknowledged write).
    Returns a new dict; the terminal and reply are never changed."""
    poll = silent_poll(terminal)
    out = {"silent_poll": poll, "recovered": recovered}
    if poll is None:
        return dict(out, **{"class": "not_silence", "reason": "result is not a silent poll after an acknowledged start"})
    ack = poll["ack_us"] if ack_us is None else ack_us
    if not isinstance(reply, dict) or "ok" not in reply:
        return dict(out, **{"class": "malformed", "reason": "no read state reply"})
    started = _read_started(reply)
    if reply.get("ok") and (started is None or ack is None or started - ack < CLASSIFY_AFTER_S * 1e6):
        return dict(out, **{"class": "too_early", "reason": "the deciding read began less than 2.0 s after the ACK"})
    if not reply.get("ok"):
        return dict(out, **{"class": "drive_silent", "reason": reply.get("result") or reply.get("outcome")})
    s = stops.sample(reply)
    if s is None:
        return dict(out, **{"class": "malformed", "reason": "read state reply without its blocks"})
    if s["alarm"] or s["motion"] & ALARM:
        return dict(out, **{"class": "alarm_after_silence", "alarm": s["alarm"], "motion": s["motion"]})
    return dict(out, **{"class": "no_alarm", "motion": s["motion"]})


def _read_started(reply: dict):
    steps = reply.get("steps") or []
    return steps[0].get("attempted_us") if steps and isinstance(steps[0], dict) else None


def read_window(reply: dict) -> dict:
    """Request start of the first step and reply end (or failure time) of the last."""
    steps = reply.get("steps") or []
    if not steps:
        return {"ok": bool(reply.get("ok")), "start_us": None, "end_us": None, "result": reply.get("result")}
    last = steps[-1]
    return {"ok": bool(reply.get("ok")), "start_us": steps[0].get("attempted_us"),
            "end_us": last.get("latest_us") or last.get("delivered_us"), "result": reply.get("result") or reply.get("outcome")}


def silence_bracket(poll: dict | None, reads: list) -> dict | None:
    """Controller-clock bracket of the no-reply interval: it began after the ACK
    reply (ack_us) and by the failing poll's timeout (failed_us); it ended after
    the last unanswered request start and by the first answer's end. Durations
    in ms: [last unanswered - failed, first answer - ack]."""
    if not poll or poll.get("ack_us") is None:
        return None
    unanswered = [r["start_us"] for r in reads if not r["ok"] and r.get("start_us")]
    answered = [r["end_us"] for r in reads if r["ok"] and r.get("end_us")]
    last_silent = max([poll["failed_us"]] + unanswered) if poll.get("failed_us") else (max(unanswered) if unanswered else None)
    first_answer = min(answered) if answered else None
    out = {"ack_us": poll["ack_us"], "failed_us": poll.get("failed_us"), "last_silent_us": last_silent,
           "first_answer_us": first_answer}
    ms = lambda a, b: None if a is None or b is None else round((a - b) / 1000.0, 1)
    out["end_after_ack_ms"] = [ms(last_silent, poll["ack_us"]), ms(first_answer, poll["ack_us"])]
    out["duration_ms"] = [ms(last_silent, poll.get("failed_us")), ms(first_answer, poll["ack_us"])]
    return out


def edge_in(edges: list, active: bool, lo, hi):
    """The first aux1 edge with ``active`` in [lo, hi], or None."""
    if lo is None or hi is None:
        return None
    return next((e for e in sorted(edges, key=lambda e: e["at_us"]) if e["active"] == active and lo <= e["at_us"] <= hi),
                None)


def stop_eligible(terminal: dict, address: int) -> bool:
    """Prompt 16 step 5 from the result alone: an explicit, non-group ACTION stop
    on this address that FAILED with execution UNKNOWN or ACKNOWLEDGED,
    completion NOT_OBSERVED and accepted TX. The application also needs the
    held conflict, the selected target and the tuple; it decides before TX."""
    if not isinstance(terminal, dict):
        return False
    write = terminal.get("write_evidence") or {}
    return (terminal.get("action_kind") == "stop" and terminal.get("address") == address and terminal.get("ok") is False and
            terminal.get("state") == "failed" and terminal.get("execution") in ("unknown", "acknowledged") and
            terminal.get("completion") == "not_observed" and bool(write.get("tx_accepted")) and not terminal.get("group"))


def fresh_branch(samples: list) -> str:
    """Step 5 branch from the last two fresh reads: 'moving' (RUNNING and the
    position changed), 'stopped' or 'unavailable' (fewer than two reads)."""
    good = [s for s in samples if s]
    if len(good) < 2:
        return "unavailable"
    a, b = good[-2], good[-1]
    pa, pb = stops.position(a), stops.position(b)
    if (b["motion"] & RUNNING) and pa is not None and pb is not None and pa != pb:
        return "moving"
    return "stopped"


def stop_frames(lines: list, start_us: int, end_us: int) -> dict:
    """Stop writes (FC06 0x0027 = 0x0100 or 0x0200) in raw TX traffic lines, by address."""
    out = {}
    for line in lines:
        m = TRAFFIC.search(line)
        if not m or not start_us <= int(m.group(1)) <= end_us:
            continue
        raw = m.group(2).upper()
        if len(raw) == 16 and raw[2:12] in (STOP_NORMAL_HEX, STOP_FAST_HEX):
            key = "%d:%s" % (int(raw[:2], 16), "normal" if raw[2:12] == STOP_NORMAL_HEX else "fast")
            out[key] = out.get(key, 0) + 1
    return out


def hw14_delay(rng, band_s, correction_s):
    """A host delay whose stop request should leave inside ``band_s`` after the
    start frame: a uniform target minus the measured host-to-wire correction."""
    return max(0.0, rng.uniform(*band_s) - correction_s)


def started(terminal: dict):
    """Controller time the start request began (trigger evidence) or None."""
    e = (terminal or {}).get("trigger_evidence") or {}
    return e.get("earliest_us") if e.get("tx_accepted") else None


# --------------------------------------------------------------------------
# Live helpers

class Live(tuning.Live):
    """Run state of one alarms-and-failures run on one drive (``address``)."""

    def __init__(self, console, run, baseline, args, timeout):
        tuning.Live.__init__(self, console, run, baseline, args, timeout)
        self.rng = random.Random(args.seed if getattr(args, "seed", None) is not None else 16)
        self.correction_s = -0.02  # Stop request minus requested delay, after the start frame (measured per trial).
        self.reproductions = 0

    def offset_us(self):
        """Controller minus host clock (us), from one bench view."""
        v = self.view()
        return v["now_us"] - time.monotonic() * 1e6

    def wait_until(self, controller_us, offset_us):
        delay = (controller_us - offset_us) / 1e6 - time.monotonic()
        if delay > 0:
            time.sleep(delay)


def prepared_stop(live):
    return tuning.prepared_stop(live)


def pick(reply: dict, keys) -> dict:
    """Selected fields of one reply (the call is made once)."""
    return {k: reply.get(k) for k in keys}


def write(live, key, value):
    return tuning.write(live, key, value)


def recover(live):
    handle = live.console.begin("recover", timeout_s=max(live.timeout, 10))
    if not handle.accepted:
        return {"ok": False, "result": live.console.wait(handle).get("result")}
    t = live.console.wait(handle, release=True)
    return {"ok": bool(t.get("ok")), "outcome": t.get("outcome") or t.get("result")}


def read_raw(live, address=None):
    """One read state reply (kept as raw for the bracket) and its sample."""
    reply = live.call("read-state", address=address or live.address)
    s = stops.sample(reply)
    if s:
        live.samples.append(s)
    return reply, s


def silence(live, terminal, offset_us):
    """Bounded diagnostics after a silent poll: reads at SILENCE_READS_S after
    the start ACK, each failed one followed by one recovery (at most three in
    all), the last read deciding by the Implement 3 rule."""
    poll = silent_poll(terminal)
    out = {"poll": poll, "reads": [], "recoveries": []}
    if poll is None or poll.get("ack_us") is None:
        return out
    need = True
    last = None
    for at_s in SILENCE_READS_S:
        live.wait_until(poll["ack_us"] + at_s * 1e6, offset_us)
        if need:
            out["recoveries"].append(recover(live))
        last, _ = read_raw(live)
        out["reads"].append(dict(read_window(last), at_s=at_s))
        need = not last.get("ok")
    if need and len(out["recoveries"]) < 3:
        out["recoveries"].append(recover(live))
    out["bracket"] = silence_bracket(poll, out["reads"])
    first = out["recoveries"][0]["ok"] if out["recoveries"] else None
    out["classification"] = classify_silence(terminal, first, last or {}, poll["ack_us"])
    return out


def clear_alarm(live) -> dict:
    """stop fast, two fresh reads (the alarm-clear witness), alarm-clear, one
    read, enable when released. An alarm that alarm-clear does not clear raises."""
    out = {"stop": prepared_stop(live).get("outcome")}
    time.sleep(0.3)
    reads = [read_raw(live)[1], read_raw(live)[1]]
    out["before"] = [{k: s.get(k) for k in ("motion", "alarm", "speed")} if s else None for s in reads]
    reply = live.call("alarm-clear", address=live.address)
    out["clear"] = {k: reply.get(k) for k in ("ok", "result", "outcome", "completion", "operation_id")}
    out["clear_ack_us"] = (reply.get("write_evidence") or {}).get("latest_us")
    out["clear_tx_us"] = (reply.get("write_evidence") or {}).get("earliest_us")
    _, s = read_raw(live)
    out["after"] = {k: s.get(k) for k in ("motion", "alarm", "speed")} if s else None
    if s is None or s["alarm"] or s["motion"] & ALARM:
        prepared_stop(live)
        raise probe.BenchError("alarm on drive %d not cleared by alarm-clear; the run stops (D15)" % live.address)
    time.sleep(1.1)  # HW-15 (wired bench 16 A3): accepted alarm-clear bytes start the 1 s enable wait.
    if s["motion"] & RUNNING:
        out["reconcile"] = prepared_stop(live).get("outcome")
    if s["motion"] & RELEASED:
        out["enable"] = live.call("enable", address=live.address).get("outcome")
        time.sleep(1.1)
    return out


def reconcile(live) -> dict:
    """The prepared stop fast and a fresh stopped read (an uncertain result's conflict)."""
    return tuning.reconcile(live)


def move(live, steps, rpm, *, wait_terminal=True):
    """One finite relative move (ordinary advanced route); returns (handle, terminal or None)."""
    handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", str(rpm), "configured"),
                                address=live.address, timeout_s=max(live.timeout, 30))
    if not handle.accepted:
        return handle, {"refused": live.console.wait(handle).get("result")}
    if not wait_terminal:
        return handle, None
    return handle, live.console.wait(handle, release=True)


def settled_move(live, steps, rpm):
    """move() after the enable wait: a "busy" refusal (before any frame) is retried
    every 0.1 s for at most 1.5 s, since accepted enable or alarm-clear bytes hold
    starts for 1 s (HW-15)."""
    deadline = time.monotonic() + 1.5
    handle, terminal = move(live, steps, rpm)
    while terminal.get("refused") == "busy" and time.monotonic() < deadline:
        time.sleep(0.1)
        handle, terminal = move(live, steps, rpm)
    return handle, terminal


def alarm_trial(live) -> dict:
    """One alarm-5 latch: +ALARM_STEPS at ALARM_RPM with the lowered threshold,
    the silence diagnostics, Y0 edges (drive 4) and the clear."""
    tuning.headroom(live)
    tuning.motion_ready(live)
    offset = live.offset_us()
    v0 = live.view()
    try:
        _, terminal = settled_move(live, ALARM_STEPS, ALARM_RPM)
        trial = {"terminal": slim_terminal(terminal), "t0_us": v0["now_us"]}
        trial["silence"] = silence(live, terminal, offset)
        trial["clear"] = clear_alarm(live)
    except BaseException:
        prepared_stop(live)
        raise
    v1 = live.view()
    trial["t1_us"] = v1["now_us"]
    found, lost = live.edges()
    trial["edges"] = [e for e in found if trial["t0_us"] <= e["at_us"] <= trial["t1_us"]]
    trial["lost_edges"] = [w for w in lost if w.get("to_us") and w["to_us"] >= trial["t0_us"]]
    return trial


def slim_terminal(t: dict) -> dict:
    keys = ("ok", "state", "outcome", "status", "detail", "execution", "completion", "polls", "started_us",
            "trigger_evidence", "failure_evidence", "last_observation", "refused", "raw_alarm", "raw_motion",
            "position_limits", "operation_id", "uncertain", "running_observed", "stop_execution", "stop_completion",
            "stop_outcome", "stop_recovery", "write_evidence", "action_kind", "address", "needs_stop", "stop_due_us",
            "stop_write_evidence", "stop_failure_evidence", "interrupted_by_stop")
    return {k: t.get(k) for k in keys if k in (t or {})}


def alarm_edges(trial: dict, polarity: int) -> dict:
    """The alarm edge inside the silence bracket and the clear edge, for Y0 = 1."""
    bracket = (trial.get("silence") or {}).get("bracket") or {}
    on_active = polarity == 1  # Polarity 0: Y0 on at the alarm -> aux1 inactive.
    alarm = edge_in(trial.get("edges") or [], on_active, bracket.get("ack_us"), bracket.get("first_answer_us"))
    clear_tx = (trial.get("clear") or {}).get("clear_tx_us")
    clear = edge_in(trial.get("edges") or [], not on_active, clear_tx, trial.get("t1_us")) if clear_tx else None
    return {"alarm_edge_us": alarm and alarm["at_us"],
            "alarm_after_ack_ms": alarm and round((alarm["at_us"] - bracket["ack_us"]) / 1000.0, 1),
            "inside": alarm is not None,
            "clear_edge_after_tx_ms": clear and round((clear["at_us"] - clear_tx) / 1000.0, 1)}


# --------------------------------------------------------------------------
# Cases

def case_regression(live, out):
    if live.address not in ALARM_DRIVES:
        out["not_run"] = "alarm 5 only on drives 2 and 4 (D15)"
        return out
    base = tuning.baseline_value(live, THRESHOLD_KEY)
    write(live, THRESHOLD_KEY, 8)
    try:
        tuning.motion_ready(live)
        terminal, samples = tuning.relative(live, 2000, 120)
        out["move"] = slim_terminal(terminal)
        _, s = read_raw(live)
        out["after_move"] = s and {k: s[k] for k in ("motion", "alarm", "speed")}
        out["stop_1"] = prepared_stop(live).get("outcome")
        _, s = read_raw(live)
        latest = live.checked("drv").get("operation_id")
        reply = live.call("alarm-clear", address=live.address)
        out["clear_after_one_read"] = {"ok": reply.get("ok"), "result": reply.get("result"),
                                       "latest_before": latest, "latest_after": live.checked("drv").get("operation_id")}
        read_raw(live)
        reply = live.call("alarm-clear", address=live.address)
        out["clear_after_two_reads"] = {k: reply.get(k) for k in ("ok", "result", "outcome", "completion")}
        _, s = read_raw(live)
        out["after_clear"] = s and {k: s[k] for k in ("motion", "alarm", "speed")}
        out["enable_held"] = pick(live.call("enable", address=live.address), ("ok", "result"))
        try:
            out["tuning_held"] = tuning.refused_write(live, THRESHOLD_KEY, base)
        except probe.BenchError as refused:  # The setter's own prior read refuses first.
            out["tuning_held"] = {"accepted": False, "result": "axis_conflict" if "axis_conflict" in str(refused) else None,
                                  "error": str(refused)}
        out["stop_2"] = prepared_stop(live).get("outcome")
        out["stopped"] = probe.drive_reported_stopped(probe.observe_stopped(live.console, address=live.address,
                                                                            timeout_s=live.timeout))
        out["enable_after"] = pick(live.call("enable", address=live.address), ("ok", "outcome"))
        time.sleep(1.1)
    except BaseException:
        prepared_stop(live)
        raise
    finally:
        out["threshold_restored"] = write(live, THRESHOLD_KEY, base)
    return out


def case_alarm(live, cid, out):
    case = CASES[cid]
    if live.address not in ALARM_DRIVES:
        out["not_run"] = "alarm 5 only on drives 2 and 4 (D15)"
        return out
    if case.get("owner") and live.address != OWNER:
        out["not_run"] = "Y0 timing needs drive 4"
        return out
    if not case.get("owner") and live.address == OWNER and cid == "A2":
        out["not_run"] = "A2 runs on drive 2"
        return out
    base = tuning.baseline_value(live, THRESHOLD_KEY)
    polarity = case.get("polarity", 0)
    try:
        if live.address == OWNER:
            tuning.set_y0(live, 1)
            if polarity:
                write(live, "io.output-polarity", polarity)
        write(live, THRESHOLD_KEY, ALARM_THRESHOLD)
        out["trials"] = []
        trials = live.args.repetitions or (3 if cid == "A1" else 2 if cid == "A2" else 1)
        for _ in range(trials):
            out["trials"].append(alarm_trial(live))
            if live.address == OWNER:
                out["trials"][-1]["y0"] = alarm_edges(out["trials"][-1], polarity)
        if cid == "A1" and "A3" in live.args.cases:
            out["a3"] = clear_start_series(live)
    finally:
        write(live, THRESHOLD_KEY, base)
        if live.address == OWNER:
            if polarity:
                write(live, "io.output-polarity", tuning.baseline_value(live, "io.output-polarity"))
            tuning.set_y0(live, tuning.baseline_value(live, "io.y0"))
    out["check_move"] = check_move(live)
    return out


def check_move(live, steps=200):
    """A confirming +steps at 60 rpm after the threshold is back; drive-reported delta."""
    tuning.motion_ready(live)
    before = read_raw(live)[1]
    terminal, _ = tuning.relative(live, steps, 60)
    after = read_raw(live)[1]
    out = {"ok": terminal.get("ok"), "outcome": terminal.get("outcome")}
    if before and after:
        out["delta"] = stops.position(after) - stops.position(before)
        out["alarm"] = after["alarm"]
    if not terminal.get("ok"):
        out["reconcile"] = reconcile(live)
    return out


def clear_start_series(live) -> list:
    """A3: from a latched alarm 5 (threshold 1), stop fast, two reads, alarm-clear,
    one read, then a start at each delay after the clear's ACK. A start the drive
    acts on re-latches alarm 5 at once (threshold 1); one it ignores ends without
    RUNNING or alarm and is reconciled by stop fast."""
    rows = []
    latched = False
    for delay in live.args.delays or A3_DELAYS_S:
        if not latched:
            trial = alarm_trial_latched(live)
            if trial is None:
                rows.append({"delay_s": delay, "not_run": "no latched alarm"})
                continue
        tuning.motion_ready(live)
        out = {"delay_s": delay, "stop": prepared_stop(live).get("outcome")}
        time.sleep(0.3)
        read_raw(live)
        read_raw(live)
        offset = live.offset_us()
        v0 = live.view()
        reply = live.call("alarm-clear", address=live.address)
        e = reply.get("write_evidence") or {}
        out["clear"] = {"ok": reply.get("ok"), "outcome": reply.get("outcome"), "tx_us": e.get("earliest_us"),
                        "ack_us": e.get("latest_us")}
        _, s = read_raw(live)
        out["after_clear"] = s and {k: s[k] for k in ("motion", "alarm", "speed")}
        if not reply.get("ok") or s is None or s["alarm"]:
            prepared_stop(live)
            raise probe.BenchError("A3: alarm-clear did not clear drive %d" % live.address)
        live.wait_until(e["latest_us"] + delay * 1e6, offset)
        try:
            # Refusals are host decisions before any frame: the ordinary readiness
            # needs fresh reads after the clear (unavailable), and from image Q the
            # enable wait holds starts for 1 s (busy). The actual delay is recorded.
            refusals = []
            _, terminal = move(live, ALARM_STEPS, ALARM_RPM)
            while terminal.get("refused") and len(refusals) < 20:
                refusals.append({"result": terminal["refused"], "after_ack_ms": round(
                    ((time.monotonic() * 1e6 + offset) - e["latest_us"]) / 1000.0, 1)})
                if terminal["refused"] == "busy":
                    time.sleep(0.1)
                else:
                    tuning.motion_ready(live)
                _, terminal = move(live, ALARM_STEPS, ALARM_RPM)
            out["refusals"] = refusals
            out["refused_first"] = refusals[0]["result"] if refusals else None
            if terminal.get("refused"):
                out["acted"] = None
                rows.append(out)
                latched = False  # The clear stands; the next row latches again.
                continue
            out["terminal"] = slim_terminal(terminal)
            begin = started(terminal)
            out["start_after_ack_ms"] = None if begin is None else round((begin - e["latest_us"]) / 1000.0, 1)
            poll = silent_poll(terminal)
            out["acted"] = poll is not None or bool(terminal.get("running_observed"))
            if poll is not None:
                out["silence"] = silence(live, terminal, offset)
                latched = True
            else:
                out["reconcile"] = reconcile(live)
                latched = False
        except BaseException:
            prepared_stop(live)
            raise
        v1 = live.view()
        found, _ = live.edges()
        out["clear_edge_after_tx_ms"] = None
        edge = edge_in([x for x in found if v0["now_us"] <= x["at_us"] <= v1["now_us"]], True, e.get("earliest_us"),
                       v1["now_us"])
        if edge and e.get("earliest_us"):
            out["clear_edge_after_tx_ms"] = round((edge["at_us"] - e["earliest_us"]) / 1000.0, 1)
        rows.append(out)
    if latched:
        rows.append({"final_clear": clear_alarm(live)})
    return rows


def alarm_trial_latched(live):
    """Latch alarm 5 without clearing it (the next A3 row clears it)."""
    tuning.motion_ready(live)
    offset = live.offset_us()
    try:
        _, terminal = settled_move(live, ALARM_STEPS, ALARM_RPM)
        if silent_poll(terminal) is None:
            reconcile(live)
            return None
        silence(live, terminal, offset)
    except BaseException:
        prepared_stop(live)
        raise
    return terminal


def case_velocity_alarm(live, out):
    if live.address != OWNER:
        out["not_run"] = "A5 runs on drive 4"
        return out
    base = tuning.baseline_value(live, THRESHOLD_KEY)
    # The run uses the baseline JOG words (0x001D/3): a serial velocity start stages
    # them, and the alarm's recoveries end the binding of the velocity restore
    # snapshot, so running at the baseline words is what leaves them unchanged.
    rpm, acc, dec = (tuning.baseline_value(live, k) for k in records.SNAPSHOT_ROUTES["velocity"]["keys"])
    rpm = rpm - 65536 if rpm & 0x8000 else rpm
    try:
        out["snapshot"] = velocity.qualify_velocity(live, negative=False)  # Its velocity read is the restore snapshot.
        write(live, THRESHOLD_KEY, ALARM_THRESHOLD)
        out["words"] = velocity.qualify_velocity(live, negative=False)
        if out["words"][1:] != [acc, dec]:
            raise probe.BenchError("A5: stored JOG ramps %s differ from the baseline %s" % (out["words"][1:], [acc, dec]))
        if abs(rpm) > probe.VELOCITY_LEGACY_MAX_RPM:  # The D6 cell of prompt 14 names this one run.
            out["cell"] = live.checked("velocity-limits", host_args=velocity.cell_tokens(
                rpm, acc, dec, tuning.baseline_value(live, "drive.subdivision"), 1000, "normal"))
        live.read_state(live.address)
        offset = live.offset_us()
        v0 = live.view()
        handle = live.console.begin("velocity", velocity_args=(str(rpm), "rpm", "native", "1000", "configured", "normal"),
                                    address=live.address, timeout_s=max(live.timeout, 12))
        if not handle.accepted:
            out["refused"] = live.console.wait(handle).get("result")
            return out
        terminal = live.console.wait(handle, release=True)
        out["terminal"] = slim_terminal(terminal)
        out["silence"] = silence(live, terminal, offset)
        out["clear"] = clear_alarm(live)
        out["window_us"] = [v0["now_us"], live.view()["now_us"]]
    except BaseException:
        prepared_stop(live)
        raise
    finally:
        write(live, THRESHOLD_KEY, base)
    return out


# --- F1-F3: HW-14 ---------------------------------------------------------

def stage_profile(live, acceleration, deceleration, speed=60):
    """Stage the 0x0021/5 words (ramps, speed, target 0) through `motion-profile stage`."""
    tuning.motion_ready(live)
    plan = records.restoration_plan(live.baseline, [{"drive": live.address, "key": "profile.acceleration", "index": None,
                                                     "candidate": None}], live.args.image_level, live.facts)
    step = dict(plan["steps"][0])
    words = [str(acceleration), str(deceleration), str(speed), "0", "0", "0", str(live.address)]
    step["write"] = {"command": "motion-profile-stage", "kwargs": {"host_args": tuple(words)},
                     "text": "motion-profile stage " + " ".join(words)}
    step["target"] = [acceleration, deceleration, speed, 0, 0]
    outcome = records.apply_write(live.console, step, timeout_s=live.timeout)
    # One stage writes the whole window; the planner keys its restore on profile.acceleration.
    live.run.field({"drive": live.address, "key": "profile.acceleration", "index": None},
                   baseline=(records.baseline_field(live.baseline, live.address, "profile.acceleration") or {}).get("value"))
    ending, _ = records.readback(live.console, step, timeout_s=live.timeout)
    if outcome["uncertain"] or not outcome["written"] or ending != step["target"]:
        raise probe.BenchError("profile stage %s: written %s uncertain %s readback %s" % (
            words, outcome["written"], outcome["uncertain"], ending))
    return ending


def install_limits(live, spec) -> dict:
    rpm, rate, ramp = spec["limits"]
    return live.checked("position-limits", host_args=("set", str(rpm), str(rate), str(ramp), "case", spec["case"]))


def ensure_limits(live, spec) -> dict | None:
    """Reinstall the named limits if an expiry event (a configuration change) ended them; None when current."""
    view = live.checked("position-limits")
    if view.get("source") == "installed" and view.get("case") == spec["case"]:
        return None
    return install_limits(live, spec)


def hw14_trial(live, n) -> dict:
    """One F1 try: +40000 at 2000 rpm, stop normal at a random host delay; on a
    failed stop, step 5 (eligibility, two fresh reads, the branch)."""
    spec = HW14
    tuning.headroom(live)
    tuning.motion_ready(live)
    reinstalled = ensure_limits(live, spec)
    before = read_raw(live)[1]
    delay = hw14_delay(live.rng, spec["band_s"], live.correction_s)
    trial = {"n": n, "delay_s": round(delay, 4), "reinstalled": bool(reinstalled)}
    t0 = time.monotonic()
    handle, _ = move(live, spec["steps"], spec["rpm"], wait_terminal=False)
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle).get("result")
        return trial
    try:
        while time.monotonic() - t0 < delay:
            time.sleep(0.001)
        stop = live.console.begin("stop", stop_policy="normal", address=live.address, timeout_s=max(live.timeout, 10))
        stop_terminal = live.console.wait(stop) if stop.accepted else {"refused": live.console.wait(stop).get("result")}
        move_terminal = live.console.wait(handle, release=True)
        trial["move"] = slim_terminal(move_terminal)
        trial["stop"] = slim_terminal(stop_terminal)
        trial["stop_id"] = stop.operation_id if stop.accepted else None
        begin = started(move_terminal)
        stop_tx = ((stop_terminal or {}).get("write_evidence") or {}).get("earliest_us")
        if begin and stop_tx:
            trial["stop_after_start_ms"] = round((stop_tx - begin) / 1000.0, 1)
            live.correction_s = 0.7 * live.correction_s + 0.3 * ((stop_tx - begin) / 1e6 - delay)
        trial["stop_ok"] = bool(stop_terminal.get("ok"))
        if not trial["stop_ok"]:
            trial["eligible"] = stop_eligible(stop_terminal, live.address)
            reads = [read_raw(live)[1]]
            time.sleep(0.2)
            reads.append(read_raw(live)[1])
            trial["fresh"] = [s and {k: s[k] for k in ("motion", "speed", "position_words")} for s in reads]
            trial["branch"] = fresh_branch(reads)
        after = read_raw(live)[1]
        if before and after:
            trial["delta"] = stops.position(after) - stops.position(before)
    except BaseException:
        prepared_stop(live)
        raise
    return trial


def release_sequence(live, trial) -> dict:
    """F2: the release after the eligible, moving failed stop, then the documented reconciliation."""
    out = {}
    stop_id = trial.get("stop_id")
    try:
        reply = live.console.release_after_stop(stop_id, mechanics_permit_release=True, triggers_inhibited=True,
                                                timeout_s=max(live.timeout, 5))
        out["release"] = {k: reply.get(k) for k in ("ok", "result", "outcome", "completion", "raw_motion")}
        reads = [read_raw(live)[1]]
        time.sleep(0.3)
        reads.append(read_raw(live)[1])
        out["after_release"] = [s and {k: s[k] for k in ("motion", "speed", "position_words")} for s in reads]
        out["creep_after_release"] = fresh_branch(reads)
        out["enable_refused"] = pick(live.call("enable", address=live.address), ("ok", "result"))
        out["alarm_clear_refused"] = pick(live.call("alarm-clear", address=live.address), ("ok", "result"))
        out["stop_fast"] = prepared_stop(live).get("outcome")
        out["stopped"] = probe.drive_reported_stopped(probe.observe_stopped(live.console, address=live.address,
                                                                            timeout_s=live.timeout))
        out["enable"] = pick(live.call("enable", address=live.address), ("ok", "outcome"))
        time.sleep(1.1)
        out["check_move"] = check_move(live)
    except BaseException:
        prepared_stop(live)
        raise
    finally:
        if stop_id:
            live.call("release", operation_id=stop_id)
    return out


def stop_branch(live, trial) -> dict:
    """Step 5 for a failed stop that is not run through F2: moving -> one new
    stop fast at once; stopped -> one new explicit stop reconciles, then enable
    and a short move; unavailable -> raise (end live work)."""
    branch = trial.get("branch")
    out = {"branch": branch}
    if branch == "unavailable":
        prepared_stop(live)
        raise probe.BenchError("failed stop with drive %d unreadable; live work ends" % live.address)
    out["stop_fast"] = prepared_stop(live).get("outcome")
    out["stopped"] = probe.drive_reported_stopped(probe.observe_stopped(live.console, address=live.address,
                                                                        timeout_s=live.timeout))
    if trial.get("stop_id"):
        live.call("release", operation_id=trial["stop_id"])
    return out


def case_hw14(live, detail):
    """F1 with F2 at the first reproduction (F3 is NOT RUN, see CASES)."""
    f1 = detail.setdefault("F1", {})
    if live.address != OWNER:
        f1["not_run"] = "F1 runs on drive 4"
        return
    spec = HW14
    base_sub = tuning.baseline_value(live, "drive.subdivision")
    trials = f1.setdefault("trials", [])
    try:
        write(live, "drive.subdivision", spec["subdivision"])
        f1["profile"] = stage_profile(live, spec["ramp"], spec["ramp"])
        f1["limits"] = install_limits(live, spec)
        for n in range(spec["trials"]):
            trial = hw14_trial(live, n)
            trials.append(trial)
            if trial.get("refused"):
                break
            if trial.get("stop_ok"):
                if trial.get("stop_id"):
                    live.call("release", operation_id=trial["stop_id"])
                continue
            reproduced = trial.get("eligible") and trial.get("branch") == "moving"
            if reproduced:
                live.reproductions += 1
                detail["F2"] = release_sequence(live, trial)
                break  # Stop at the first reproduction (Implement 2).
            trial["step5"] = stop_branch(live, trial)
    finally:
        live.call("position-limits", host_args=("default",))
        write(live, "drive.subdivision", base_sub)


# --- R2: Q05 --------------------------------------------------------------

def q05_attempt(live, rpm) -> dict:
    """One Q05 move at ``rpm``: read-state sampling at POLL_S until the move ends or
    watch_s; persistent RUNNING past the move's end is ended by stop fast."""
    spec = Q05
    tuning.headroom(live)
    tuning.motion_ready(live)
    reinstalled = ensure_limits(live, spec)
    before = read_raw(live)[1]
    v0 = live.view()
    out = {"rpm": rpm, "reinstalled": bool(reinstalled)}
    handle, _ = move(live, spec["steps"], rpm, wait_terminal=False)
    if not handle.accepted:
        out["refused"] = live.console.wait(handle).get("result")
        return out
    samples = []
    began = time.monotonic()
    try:
        while handle.terminal is None and time.monotonic() - began < spec["watch_s"]:
            s = read_raw(live)[1]
            samples.append((round(time.monotonic() - began, 3), s))
            if not out.get("aux1_while_running") and time.monotonic() - began > 1.0 and s and s["motion"] & RUNNING:
                out["aux1_while_running"] = live.view()["input"]["active"]  # The Y0 level during the stuck RUNNING.
            time.sleep(POLL_S)
        stuck = handle.terminal is None
        late = [s for at, s in samples if at > 1.0 and s]
        out["stuck_running"] = stuck and bool(late) and all(s["motion"] & RUNNING for s in late)
        if stuck:
            out["stop"] = slim_terminal(live.call("stop", stop_policy="fast", address=live.address))
        terminal = live.console.wait(handle, release=True)
        out["terminal"] = slim_terminal(terminal)
        if not terminal.get("ok") and not stuck:
            out["reconcile"] = reconcile(live)
        after = [read_raw(live)[1] for _ in range(3)]
        out["after"] = [s and {k: s[k] for k in ("motion", "speed", "position_words")} for s in after]
        if out.get("stop") and not out["stop"].get("ok"):
            out["stop_eligible"] = stop_eligible(out["stop"], live.address)
            out["branch"] = fresh_branch(after)
            if out["branch"] != "stopped":
                prepared_stop(live)
                raise probe.BenchError("R2: drive %d still moving after stop fast" % live.address)
        if before and after[-1]:
            out["delta"] = stops.position(after[-1]) - stops.position(before)
    except BaseException:
        prepared_stop(live)
        raise
    v1 = live.view()
    found, _ = live.edges()
    out["edges"] = [e for e in found if v0["now_us"] <= e["at_us"] <= v1["now_us"]]
    out["samples"] = [(at, s and {k: s[k] for k in ("motion", "speed", "position_words", "outputs", "windows")})
                      for at, s in samples]
    return out


def case_q05(live, out):
    if live.address != OWNER:
        out["not_run"] = "R2 runs on drive 4"
        return out
    spec = Q05
    base_sub = tuning.baseline_value(live, "drive.subdivision")
    try:
        write(live, "drive.subdivision", spec["subdivision"])
        out["profile"] = stage_profile(live, spec["ramp"], spec["ramp"])
        out["limits"] = install_limits(live, spec)
        out["rows"] = []
        for y0 in (2, 4):
            tuning.set_y0(live, y0)
            for _ in range(spec["attempts"]):
                out["rows"].append(dict(q05_attempt(live, spec["rpm"]), y0=y0))
        tuning.set_y0(live, 2)
        for _ in range(spec["attempts"]):
            out["rows"].append(dict(q05_attempt(live, spec["control_rpm"]), y0=2, control=True))
    finally:
        tuning.set_y0(live, tuning.baseline_value(live, "io.y0"))
        live.call("position-limits", host_args=("default",))
        write(live, "drive.subdivision", base_sub)
    return out


def run_cases(live, args, cases):
    detail = {"cases": {}, "address": live.address}
    live.partial = detail["cases"]
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    live.call("position-limits", host_args=("default",))
    for cid in cases:
        kind = CASES[cid]["kind"]
        try:
            if kind == "not-run":
                detail["cases"][cid] = {"note": CASES[cid]["reason"]}
            elif kind == "regression":
                case_regression(live, detail["cases"].setdefault(cid, {}))
            elif kind == "alarm":
                case_alarm(live, cid, detail["cases"].setdefault(cid, {}))
            elif kind == "clear-start" and "A1" not in cases:
                detail["cases"][cid] = {"not_run": "A3 runs inside A1"}
            elif kind == "velocity-alarm":
                case_velocity_alarm(live, detail["cases"].setdefault(cid, {}))
            elif kind == "hw14":
                case_hw14(live, detail["cases"])
            elif kind == "q05":
                case_q05(live, detail["cases"].setdefault(cid, {}))
        except BaseException:
            prepared_stop(live)
            raise
    detail["relay_operations"] = live.relay_operations
    return detail


# --------------------------------------------------------------------------
# Plan and verdicts

def plan(args, baseline, facts_):
    """Scenario-record fields of one alarms-and-failures run, before any port access."""
    address = args.address
    keys = set()
    for cid in args.cases:
        kind = CASES[cid]["kind"]
        if kind in ("regression", "alarm", "velocity-alarm"):
            keys.add(THRESHOLD_KEY)
        if address == OWNER and kind in ("alarm", "q05"):
            keys.add("io.y0")
        if cid == "A4":
            keys.add("io.output-polarity")
        if kind in ("hw14", "q05"):
            keys.add("drive.subdivision")
    write_list = [{"drive": address, "key": k, "index": None, "candidate": None} for k in sorted(keys)]
    write_list += [{"drive": address, "key": k, "index": None, "candidate": None}
                   for k in ("profile.acceleration", "profile.deceleration", "profile.speed", "profile.target")]
    if "A5" in args.cases:
        write_list += [{"drive": address, "key": k, "index": None, "candidate": None}
                       for k in records.SNAPSHOT_ROUTES["velocity"]["keys"]]
    # A5's velocity run writes the JOG words; its first velocity read is the restore snapshot.
    restoration = records.restoration_plan(baseline, write_list, args.image_level, facts_,
                                           context={"snapshots": {"velocity": True}} if "A5" in args.cases else None)
    cases = [{"id": c, "required": True, "assertion": CASES[c]["assertion"], "required_levels": list(CASES[c]["levels"])}
             for c in args.cases]
    commands = ["position limits default (host, local)",
                "per case the setters of its fields (tuning filters set deviation-threshold, io set y0|output-polarity|x3, "
                "driver set subdivision, motion-profile stage) with their fresh reads and stated facts, each read back",
                "alarm trial: read config, read state, motion-profile read, start-speed relation, bench, move relative 2000 "
                "steps native 60 configured; recover and read state at 1.4, 1.7, 2.0 s after the start ACK (at most three "
                "recoveries); stop fast; read state x2; alarm-clear; read state; enable if released",
                "A3: alarm-clear, read state, move relative 2000 at 0.2/0.5/1.0 s after the clear ACK",
                "A5: velocity read, velocity qualification install, velocity 60 rpm native 1000 configured normal",
                "F1: position limits set 2000 133334 100 case hw14; move relative 40000 steps native 2000 configured; "
                "stop normal 4 at a random delay; failed stop: read state x2, step 5; F2 motor-release after-stop ID "
                "mechanics-permit-release triggers-inhibited, enable, alarm-clear, stop fast, enable, +200",
                "R2: position limits set 2000 1706667 100 case q05; move relative 12800 steps native 900|2000 configured; "
                "read state every 50 ms for 3 s; stop fast on persistent RUNNING",
                "restoration: stop fast, alarm-clear/enable if needed, position limits default, the plan"]
    bounds = {"address": address, "alarm_drives": list(ALARM_DRIVES), "alarm_move": [ALARM_STEPS, ALARM_RPM],
              "silence_reads_s": list(SILENCE_READS_S), "hw14": dict(HW14), "q05": dict(Q05),
              "exposure": "finite moves end by themselves; a creep or persistent RUNNING ends with stop fast within 2 s "
                          "of the failed stop's terminal; free shaft (D11)"}
    expected = {"drive-reported": "alarm words, motion words, positions, stop and move terminals (alarms.json)",
                "controller-timestamped": "aux1 edges against the silence bracket and the clear",
                "sent": "refusals leave the latest operation unchanged; raw TX stop frames",
                "acknowledged": "stop normal echoes and their wire times after the start frame"}
    return write_list, restoration, cases, commands, bounds, expected, []


def _verdicts(run, cid, levels, verdict, reason, ev=("alarms.json",)):
    for i, level in enumerate(levels):  # The case reason is recorded once.
        run.verdict(cid, level, verdict, reason if i == 0 else None, list(ev))


def case_verdicts(run, cid, detail, error):
    case = CASES[cid]
    cases = detail.get("cases") or {}
    out = cases.get(cid)
    if cid == "A3":
        out = (cases.get("A1") or {}).get("a3")
    levels = case["levels"]
    if case["kind"] == "not-run":
        _verdicts(run, cid, levels, "NOT RUN", case["reason"])
        return
    if not out:
        _verdicts(run, cid, levels, "NOT RUN", "not reached: %s" % (error or "no record"))
        return
    if isinstance(out, dict) and out.get("not_run"):
        _verdicts(run, cid, levels, "NOT RUN", out["not_run"])
        return
    if cid == "G1":
        ok_drive = (out.get("after_move") or {}).get("alarm") == 5 and (out.get("clear_after_two_reads") or {}).get("ok") \
            and (out.get("after_clear") or {}).get("alarm") == 0
        c1 = out.get("clear_after_one_read") or {}
        sent = c1.get("result") == "axis_conflict" and c1.get("latest_before") == c1.get("latest_after") and \
            (out.get("enable_held") or {}).get("result") == "axis_conflict" and \
            (out.get("tuning_held") or {}).get("result") == "axis_conflict" and (out.get("enable_after") or {}).get("ok")
        reason = "after move %s; clear after one read %s; after two %s; enable/tuning held %s/%s; stop %s; enable after %s" % (
            out.get("after_move"), c1.get("result"), out.get("clear_after_two_reads"), (out.get("enable_held") or {}).get("result"),
            (out.get("tuning_held") or {}).get("result"), out.get("stop_2"), out.get("enable_after"))
        run.verdict(cid, "drive-reported", "PASS" if ok_drive else "FAIL", reason, ["alarms.json"])
        run.verdict(cid, "sent", "PASS" if sent else "FAIL", None, ["alarms.json"])
        return
    if case["kind"] == "alarm":
        trials = out.get("trials") or []
        notes, drive_ok, edge_ok = [], bool(trials) and not error, bool(trials) and not error
        for t in trials:
            s = t.get("silence") or {}
            c = s.get("classification") or {}
            b = s.get("bracket") or {}
            # A1 expects the first poll to get 0 RX; A2 and A4 only the alarm after the silence.
            drive_ok = drive_ok and c.get("class") == "alarm_after_silence" and \
                (cid != "A1" or (s.get("poll") or {}).get("first_poll") is True)
            y0 = t.get("y0")
            if y0 is not None:
                edge_ok = edge_ok and y0.get("inside")
            notes.append("%s first_poll %s end %s ms edge %s" % (c.get("class"), (s.get("poll") or {}).get("first_poll"),
                                                                 b.get("end_after_ack_ms"), y0 and y0.get("alarm_after_ack_ms")))
        for i, level in enumerate(levels):
            ok = edge_ok if level == "controller-timestamped" else drive_ok
            run.verdict(cid, level, "PASS" if ok else "FAIL", "; ".join(notes) if i == 0 else None, ["alarms.json"])
        return
    if cid == "A3":
        rows = [r for r in out if "delay_s" in r]
        notes = ["%.1f s: start +%s ms acted %s, clear edge +%s ms" % (r["delay_s"], r.get("start_after_ack_ms"),
                                                                      r.get("acted"), r.get("clear_edge_after_tx_ms"))
                 for r in rows]
        recorded = bool(rows) and all(r.get("acted") is not None for r in rows if not r.get("not_run"))
        _verdicts(run, cid, levels, "PASS" if recorded and not error else "FAIL", "; ".join(notes))
        return
    if cid == "A5":
        c = ((out.get("silence") or {}).get("classification") or {}).get("class")
        t = out.get("terminal") or {}
        run.verdict(cid, "drive-reported", "PASS" if c == "alarm_after_silence" else "FAIL",
                    "classification %s; velocity %s/%s, stop %s %s %s" % (c, t.get("outcome"), t.get("execution"),
                                                                       t.get("stop_execution"), t.get("stop_completion"),
                                                                       t.get("stop_outcome")), ["alarms.json"])
        frames = out.get("stop_frames")
        run.verdict(cid, "sent", "PASS" if frames is not None and frames.get("%d:normal" % OWNER, 0) <= 1 else
                    "indeterminate", "stop frames in the window: %s" % frames, ["alarms.json", "console.log"])
        return
    if cid in ("F1", "F2"):
        f1 = cases.get("F1") or {}
        trials = f1.get("trials") or []
        failed = [t for t in trials if t.get("stop_ok") is False]
        if cid == "F1":
            reproduced = [t for t in failed if t.get("eligible") and t.get("branch") == "moving"]
            reason = "%d tries, %d failed stops (%s), %d eligible and moving" % (
                len(trials), len(failed), [(t["n"], t.get("eligible"), t.get("branch"), t.get("stop_after_start_ms"))
                                           for t in failed], len(reproduced))
            # A reproduction attempt is never a pass of the drive: PASS means reproduced
            # (eligible and moving); not reproduced is indeterminate (prompt 16 step 5).
            _verdicts(run, cid, levels, "PASS" if reproduced else "indeterminate" if trials and not error else "FAIL",
                      ("reproduced: " if reproduced else "not reproduced: ") + reason)
            return
        if cid == "F2":
            f2 = cases.get("F2")
            if not f2:
                _verdicts(run, cid, levels, "NOT RUN", "F1 not reproduced")
                return
            ok = (f2.get("release") or {}).get("ok") and (f2.get("enable_refused") or {}).get("result") == "axis_conflict" and \
                (f2.get("alarm_clear_refused") or {}).get("result") == "axis_conflict" and f2.get("stopped") and \
                (f2.get("check_move") or {}).get("delta") == 200
            _verdicts(run, cid, levels, "PASS" if ok else "FAIL", "%s" % {k: f2.get(k) for k in (
                "release", "creep_after_release", "enable_refused", "alarm_clear_refused", "stop_fast", "check_move")})
            return
        return
    if cid == "R2":
        rows = out.get("rows") or []
        notes = ["%s rpm Y0 %s: stuck %s, %s" % (r.get("rpm"), r.get("y0"), r.get("stuck_running"),
                                                 (r.get("terminal") or {}).get("outcome")) for r in rows]
        _verdicts(run, cid, levels, "PASS" if rows and not error else "FAIL", "; ".join(notes))
        return
