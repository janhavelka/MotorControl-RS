"""Wired bench 07: stop (5), emergency stop (6) and release (4) inputs on X3.

One case and one X3 function per run (prompt 07 case table). ``mosfet2``
drives X3 of drives 1, 2 and 4 in parallel. Phase A assigns the function on
drive 4 only (drives 1 and 2 keep X3 = 0); Phase B on all three. Drive 4's Y0
(function 2 running or 5 brake) is read on ``aux1``, inverted: Y0 off makes
``aux1`` active. Every ON goes through ``bench_records.bench_on`` (D14
interlock); a running member is named with its pre-start ``io read``.

The analysis (pure, tested with recorded replies) follows COMMON Timing and
Implement 3: each trial separates the witnesses (source commanded, input seen
active, motion seen before the stimulus, first stopped sample bracket, stopped
short, further start) and is "input-stopped" only when every one is present
and nothing else can explain the stop; otherwise it names what is missing.
"""
import random
import statistics
import time

import bench_motion as motion
import bench_outputs as bench_out
import bench_probe as probe
import bench_records as records

OWNER = records.FIXTURE["y0_owner"]
X3_BIT = 8
RUNNING, IN_POSITION, HOMED, RELEASED, ALARM = 4, 1, 2, 16, 8
POST_RELEASE_S = 2.0
EFFECT = {4: "release", 5: "stop", 6: "stop"}
Y0_FACTS = ["indicator", "either"]
X3_FACTS = ["triggers-inhibited", "protection-removal"]
PROFILE_KEYS = ("profile.acceleration", "profile.deceleration", "profile.speed", "profile.target")
MOVE_STEPS = 3000

# Case table (prompt 07). kind selects the runner; functions are the X3 values
# a run may use; y0 the drive-4 Y0 function; phase A (drive 4) or B (1, 2, 4).
CASES = {
    "S1": dict(kind="move", functions=(5, 6), runs=10, steps=3000, hold_ms=200, delay_s=1.0, y0=2, phase="A",
               levels=["controller-timestamped", "drive-reported"]),
    "S2": dict(kind="absolute", functions=(5, 6), runs=5, steps=3000, hold_ms=200, delay_s=1.0, y0=2, phase="A",
               levels=["controller-timestamped", "drive-reported"]),
    "S3": dict(kind="velocity", functions=(5, 6), runs=5, rpm=30, duration_ms=1000, hold_ms=200, delay_s=0.0, y0=2,
               phase="A", levels=["controller-timestamped", "drive-reported"]),
    "S4": dict(kind="home", functions=(5,), runs=3, methods=(33, 34), search=30, hold_ms=200, delay_s=0.0, y0=2,
               phase="A", levels=["drive-reported"]),
    "S5": dict(kind="held", functions=(5, 6, 4), runs=1, hold_ms=3000, y0=2, phase="A", levels=["sent"]),
    "S6": dict(kind="race", functions=(5, 6), runs=1, steps=3000, hold_ms=3000, y0=2, phase="A",
               levels=["drive-reported"]),
    "S7": dict(kind="serial", functions=(6,), runs=1, steps=3000, hold_ms=200, delay_s=1.0, y0=2, deceleration=1000,
               phase="A", levels=["controller-timestamped"]),
    "S8": dict(kind="move", functions=(5, 6), runs=5, steps=3000, hold_ms=200, delay_s=1.0, y0=2, phase="A",
               levels=["controller-timestamped", "drive-reported"]),
    "S9": dict(kind="width", functions=(5, 6), runs=3, steps=3000, widths=(5, 10, 12, 15, 20, 30, 50), delay_s=1.0, y0=2,
               phase="A", levels=["controller-timestamped", "drive-reported"]),
    "R1": dict(kind="release", functions=(4,), runs=10, hold_ms=2000, y0=5, phase="A",
               levels=["controller-timestamped", "drive-reported"]),
    "R2a": dict(kind="move", functions=(4,), runs=5, steps=1000, hold_ms=200, delay_s=0.05, y0=5, phase="A",
                levels=["controller-timestamped", "drive-reported"]),
    "R2b": dict(kind="move", functions=(4,), runs=10, steps=1000, hold_ms=200, delay_s=0.05, y0=2, phase="A",
                levels=["controller-timestamped", "drive-reported"]),
    "R3": dict(kind="enable", functions=(4,), runs=1, hold_ms=3000, y0=2, phase="A", levels=["drive-reported"]),
    # R2a diagnostics (wired bench 07 deviation): with Y0 = 5 a 200 ms release
    # pulse did not stop a move. R2d holds 1500 ms during a 3000-step move; R1s
    # pulses 200 ms on a stationary drive and records whether the brake output returns.
    "R2d": dict(kind="move", functions=(4,), runs=2, steps=3000, hold_ms=1500, delay_s=0.05, y0=5, phase="A",
                levels=["controller-timestamped", "drive-reported"]),
    "R1s": dict(kind="release", functions=(4,), runs=2, hold_ms=200, y0=5, phase="A",
                levels=["controller-timestamped", "drive-reported"]),
    "G1": dict(kind="group", functions=(5, 6), runs=5, steps=3000, hold_ms=200, delay_s=1.0, y0=2, mode="staged",
               phase="B", levels=["controller-timestamped", "drive-reported"]),
    # G2: while a sequential group starts its members the console answers busy,
    # so the interlock (and the ON) can only follow the last start (measured).
    "G2": dict(kind="group", functions=(5,), runs=1, steps=3000, hold_ms=200, delay_s=0.3, y0=2, mode="sequential",
               phase="B", levels=["sent", "drive-reported"]),
}
STIMULI = ("input", "stop normal", "stop fast")  # S8 per deceleration
# Measured on the bench (S1): the interlock and its view take about 0.45 s, so
# an ON lands delay_s + ~0.45 s after the start acceptance. A serial stop
# stimulus waits the same extra time, so every S8 stimulus hits the same point.
INTERLOCK_S = 0.45


def phase_drives(case: dict) -> list:
    return list(records.FIXTURE["input_drives"]) if case["phase"] == "B" else [OWNER]


# --------------------------------------------------------------------------
# Analysis (pure; unit-tested)

def sample(reply: dict) -> dict | None:
    """One read state as a sample: bench_outputs.state_sample plus the address."""
    s = bench_out.state_sample(reply)
    if s is not None:
        s = dict(s, address=reply.get("address"))
    return s


def position(s: dict) -> int | None:
    """Signed position from the feedback words, high word first (baseline word order 0)."""
    words = s.get("position_words")
    if not (isinstance(words, list) and len(words) == 2):
        return None
    value = (words[0] << 16) | words[1]
    return value - (1 << 32) if value & 0x80000000 else value


def running(s: dict) -> bool:
    return bool(s["motion"] & RUNNING)


def moving(s: dict) -> bool:
    """Running reported, or a nonzero speed word (the ramp after RUNNING clears)."""
    return running(s) or bool(s["speed"])


def first_edge(edges: list, after_us: int, active: bool, before_us: int | None = None) -> dict | None:
    for e in sorted(edges, key=lambda e: e["at_us"]):
        if e["at_us"] >= after_us and e["active"] == active and (before_us is None or e["at_us"] < before_us):
            return e
    return None


def classify(trial: dict) -> dict:
    """Implement 3 witnesses of one stimulated motion trial, and its class.

    ``trial``: on_us/off_us (source commanded), samples (this drive's read
    state, ordered), start_us (closure of the start reply), commanded steps
    (finite move; None for velocity/home), start_position, competing_end_us
    (velocity: the route's own stop due time; None otherwise), safety_stop_us
    (controller time the prepared stop was sent, or None), terminal (the
    operation's result), edges (aux1), y0 (drive 4 Y0 function or None),
    function (X3). Returns witnesses, class and the missing witnesses.
    """
    on_us, off_us = trial.get("on_us"), trial.get("off_us")
    samples = sorted((s for s in trial.get("samples", []) if s), key=lambda s: s["windows"]["motion"][0])
    w = {"source_commanded": type(on_us) is int and type(off_us) is int}
    missing = []
    if not w["source_commanded"]:
        return {"class": "indeterminate", "witnesses": w, "missing": ["source not commanded"]}
    start_us = trial.get("start_us") or 0
    before = [s for s in samples if start_us <= s["windows"]["motion"][0] and s["windows"]["motion"][1] <= on_us]
    w["motion_before"] = any(moving(s) for s in before)
    bit = trial.get("input_bit", X3_BIT)  # Wired bench 11: the limit input's own status bit.
    seen = [s for s in samples if s["inputs"] & bit and s["windows"]["motion"][1] >= on_us]
    w["input_seen"] = bool(seen)
    edge = None
    if trial.get("y0") == 2:
        edge = first_edge(trial.get("edges", []), on_us, True)  # Running output off: aux1 active.
    w["y0_edge_us"] = None if edge is None else edge["at_us"] - on_us
    # The first sample after the ON whose request started after it and shows no
    # running report, and the last earlier sample that showed running.
    after = [s for s in samples if s["windows"]["motion"][0] >= on_us]
    stopped = next((s for s in after if not running(s)), None)
    last_running = None
    for s in samples:
        if stopped is not None and s["windows"]["motion"][0] >= stopped["windows"]["motion"][0]:
            break
        if running(s):
            last_running = s
    if stopped is not None and last_running is not None:
        w["stop_bracket_us"] = [last_running["windows"]["motion"][0] - on_us, stopped["windows"]["motion"][1] - on_us]
    else:
        w["stop_bracket_us"] = None
    standstill = next((s for s in after if not moving(s)), None)
    w["standstill_us"] = None if standstill is None else standstill["windows"]["motion"][1] - on_us
    # Stopped short: the final position against the commanded endpoint.
    steps, origin = trial.get("steps"), trial.get("start_position")
    final = next((position(s) for s in reversed(samples) if position(s) is not None), None)
    w["travel"] = None if final is None or origin is None else final - origin
    w["stopped_short"] = None
    if steps is not None and w["travel"] is not None:
        w["stopped_short"] = abs(w["travel"]) < abs(steps) - 20
    # Any further start: running again after the first stopped sample.
    w["further_start"] = bool(stopped is not None and any(
        running(s) for s in samples if s["windows"]["motion"][0] > stopped["windows"]["motion"][1]))
    terminal = trial.get("terminal") or {}
    w["reported_complete"] = bool(terminal.get("ok"))
    w["faults"] = [s["alarm"] for s in samples if s["alarm"]]
    released = any(s["motion"] & RELEASED for s in samples if s["windows"]["motion"][0] >= on_us)
    w["released_seen"] = released
    # Discriminating stop: before the competing end condition.
    competing = trial.get("competing_end_us")
    safety = trial.get("safety_stop_us")
    discriminating = False
    if stopped is not None:
        end = stopped["windows"]["motion"][1]
        if competing is not None:
            discriminating = end < competing
        elif steps is not None:
            discriminating = bool(w["stopped_short"])
        else:
            discriminating = True  # Homing: no natural end before completion; completion is checked separately.
    w["discriminating"] = discriminating
    w["after_safety_stop"] = bool(safety is not None and stopped is not None and stopped["windows"]["motion"][1] > safety)
    if not w["motion_before"]:
        missing.append("no motion witnessed after the start and before the stimulus")
    if not (w["input_seen"] or w["y0_edge_us"] is not None):
        missing.append("neither the input bit nor a Y0 edge witnessed the stimulus")
    if stopped is None:
        missing.append("no stopped sample after the stimulus")
    elif not discriminating:
        missing.append("the stop is not separated from the competing end condition")
    if w["after_safety_stop"]:
        missing.append("first stopped sample after the prepared safety stop")
    if w["reported_complete"]:
        missing.append("the operation reported completion")
    if w["faults"] and trial.get("function") != 4:
        missing.append("alarm %s in the samples" % w["faults"][0])
    if trial.get("function") == 4 and not released:
        missing.append("release not sampled")
    if stopped is None and not missing[:-1] and samples and all(moving(s) for s in after):
        cls = "not stopped"
    else:
        cls = "input-stopped" if not missing else "indeterminate"
    if cls != "input-stopped" and stopped is not None and not discriminating and not w["input_seen"] and \
            not w["further_start"] and w["stopped_short"] is False:
        cls = "not stopped"  # Ran to its endpoint: the pulse had no effect.
    return {"class": cls, "witnesses": w, "missing": missing}


def latency_stats(values: list) -> dict | None:
    values = [v for v in values if v is not None]
    if not values:
        return None
    return {"n": len(values), "min": min(values), "median": statistics.median(values), "max": max(values)}


def summarize(trials: list) -> dict:
    """L1 (Y0 running-off edge - on_us), LR (Y0 brake-off edge - on_us) and L2 brackets, in ms."""
    l1 = [t["analysis"]["witnesses"].get("y0_edge_us") for t in trials if t.get("y0") == 2]
    lr = [t.get("lr_us") for t in trials if t.get("y0") == 5]
    brackets = [t["analysis"]["witnesses"].get("stop_bracket_us") for t in trials]
    brackets = [b for b in brackets if b]
    to_ms = lambda s: None if s is None else {k: (v / 1000.0 if k != "n" else v) for k, v in s.items()}
    classes = {}
    for t in trials:
        classes[t["analysis"]["class"]] = classes.get(t["analysis"]["class"], 0) + 1
    return {"trials": len(trials), "classes": classes,
            "l1_ms": to_ms(latency_stats(l1)), "lr_ms": to_ms(latency_stats(lr)),
            "l2_start_ms": to_ms(latency_stats([b[0] for b in brackets])),
            "l2_end_ms": to_ms(latency_stats([b[1] for b in brackets])),
            "travel": latency_stats([t["analysis"]["witnesses"].get("travel") for t in trials])}


# --------------------------------------------------------------------------
# Live sequencing

class Live:
    """Run state of one stop-inputs case: samples, views, trials, events."""

    def __init__(self, console, run, baseline, args, facts, timeout):
        self.console, self.run, self.baseline, self.args, self.facts = console, run, baseline, args, facts
        self.timeout = timeout
        self.case = CASES[args.case]
        self.samples, self.views, self.events, self.trials, self.refusals = [], [], [], [], []
        self.snapshots = {}
        self.record = {"running_members": [], "max_age_ms": records.MAX_AGE_MS, "read_last": OWNER}
        self.rng = random.Random(7)
        self.written = set()

    def call(self, name, **kwargs):
        reply = self.console.command(name, timeout_s=kwargs.pop("timeout_s", self.timeout), **kwargs)
        self.events.append({"command": name, "arguments": {k: v for k, v in kwargs.items()}, "ok": reply.get("ok"),
                            "result": reply.get("result"), "outcome": reply.get("outcome"),
                            "operation_id": reply.get("operation_id"), "reason": reply.get("reason"),
                            "started_us": reply.get("started_us")})
        return reply

    def checked(self, name, **kwargs):
        reply = self.call(name, **kwargs)
        if not reply.get("ok"):
            raise probe.BenchError("%s refused/failed: %s" % (name, reply.get("result", reply.get("outcome"))))
        return reply

    def state(self, address=OWNER):
        reply = self.call("read-state", address=address)
        s = sample(reply)
        if s:
            self.samples.append(s)
        return s

    def view(self):
        v = self.checked("bench", host_args=())
        self.views.append(v)
        return v


def wait_running(live, accepted, delay_s, address=OWNER, limit_s=0.5):
    """Sample until ``delay_s`` after acceptance and until one sample reports
    RUNNING (the witness before the stimulus), at most ``limit_s`` longer. A
    speed word alone is not enough: a read state's feedback step can follow
    the start write while its motion step preceded it."""
    seen = False
    while True:
        elapsed = time.monotonic() - accepted
        s = live.state(address)
        seen = seen or bool(s and running(s))
        if elapsed >= delay_s and (seen or elapsed >= delay_s + limit_s):
            return seen


def write_setting(live, address, key, value):
    """One ordinary setting write through its planned route and stated facts; read back."""
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
    live.events.append({"command": text, "ok": reply.get("ok"), "operation_id": reply.get("operation_id"),
                        "outcome": reply.get("outcome"), "readback": (reply.get("observation") or {}).get("raw")})
    if outcome["uncertain"] or not outcome["written"]:
        live.run.field(entry, uncertain=outcome["uncertain"], reason="write %s %s refused or uncertain; not replayed" % (key, value))
        raise probe.BenchError("write %s %s refused or uncertain: %s" % (key, value, outcome["result"]))
    return reply


def assign_x3(live, function):
    """X3 = function on the phase drives; Phase A requires X3 = 0 on 1 and 2 (shared line check)."""
    for address in phase_drives(live.case):
        write_setting(live, address, "io.x3", function)
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    check_line(live, function)


def check_line(live, function):
    """Step 3: fresh assignments, polarity and state of drives 1, 2 and 4."""
    rows = {}
    for address in records.FIXTURE["input_drives"]:
        io = live.checked("io", driver_args=("read",), address=address)
        words = io["observation"]["raw"]
        rows[address] = words
        expected = function if address in phase_drives(live.case) else 0
        if words[4] != expected or words[0] & X3_BIT:
            raise probe.BenchError("shared line check: drive %d X3 %d polarity %d, expected %d polarity 0"
                                   % (address, words[4], words[0] >> 3 & 1, expected))
        if any(7 <= f <= 13 for f in words[1:5]):
            raise probe.BenchError("shared line check: drive %d has a motion-starting input function" % address)
    live.events.append({"command": "shared line check", "io": rows})
    return rows


def pre_start(live, address=OWNER):
    """io read, read config, read state immediately before an operation: the
    named member's pre-start io read (row L3) and the reads in between."""
    io = live.checked("io", driver_args=("read",), address=address)
    config = live.checked("read-config", address=address)
    st = live.checked("read-state", address=address)
    s = sample(st)
    if s:
        live.samples.append(s)
    return {"io_read": {"ok": True, "operation_id": io["operation_id"], "started_us": io.get("started_us"),
                        "raw": io["observation"]["raw"]},
            "between": [{"command": "read-config", "operation_id": config["operation_id"]},
                        {"command": "read-state", "operation_id": st["operation_id"]}],
            "state": s}


def named(address, function, operation_id, pre, console_id=None, console_address=None):
    member = {"output": "mosfet2", "terminal": "x3", "effect": EFFECT[function], "function": function, "polarity": 0,
              "operation_id": operation_id, "address": address, "io_read": pre["io_read"], "between": list(pre["between"])}
    if console_id is not None:
        member.update(console_operation_id=console_id, console_address=console_address)
    return member


def pulse(live, hold_ms, members):
    """One interlocked mosfet2 ON with the named running members."""
    live.record["running_members"] = members
    try:
        reply = records.bench_on(live.console, "mosfet2", hold_ms, live.record)
    finally:
        live.record["running_members"] = []
    on = records._bench_view(reply)["outputs"]["mosfet2"]
    live.views.append(reply)
    return on["on_us"], on["on_us"] + hold_ms * 1000


def observe(live, addresses, seconds, every=3):
    """Back-to-back read state round robin for ``seconds``; a bench view every ``every`` reads."""
    began, index = time.monotonic(), 0
    while time.monotonic() - began < seconds:
        live.state(addresses[index % len(addresses)])
        index += 1
        if index % every == 0:
            live.view()
    live.view()


def off_us(live, on_us, hold_ms):
    """The output's own off time from the latest view after the hold."""
    for v in reversed(live.views):
        o = next((x for x in v.get("outputs", []) if x.get("name") == "mosfet2"), None)
        if o and o.get("on_us") == on_us and not o.get("active"):
            return o.get("off_us")
    return on_us + hold_ms * 1000


def edges(live):
    found, lost = records._merge_edges([records._bench_view(v) for v in live.views if v.get("ok")])
    return found, lost


def reconcile(live, addresses):
    """The prepared stop fast per drive (turns every bench output off), then a
    fresh stopped read; an uncertain axis after an input stop needs it."""
    out = {}
    for address in addresses:
        stop = live.call("stop", stop_policy="fast", address=address)
        st = probe.observe_stopped(live.console, address=address, timeout_s=live.timeout,
                                   on_sample=lambda r: live.samples.append(sample(r)) if sample(r) else None)
        out[address] = {"stop": stop.get("outcome"), "stopped": probe.drive_reported_stopped(st)}
    return out


def recover_released(live, address=OWNER):
    """After a release: explicit enable, the 1 s settle, then a fresh read."""
    st = live.state(address)
    if st and st["motion"] & RELEASED:
        live.call("enable", address=address)
        time.sleep(1.1)
        st = live.state(address)
    if st and (st["alarm"] or st["motion"] & ALARM):
        live.call("stop", stop_policy="fast", address=address)
        live.call("alarm-clear", address=address)
        st = live.state(address)
    return st


def motion_ready(live):
    """Selected drive 4: config, a profile read (snapshot kept once) and the start-speed relation."""
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    live.state()
    try:
        profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
    except probe.BenchError:
        live.checked("motion-profile", host_args=("forget",))
        profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
        live.snapshots["profile_replaced"] = True
    live.snapshots.setdefault("profile", {k: profile.get(k) for k in ("original", "current")})
    word = str(profile["current"][0])
    live.call("motion-profile-start-speed-relation", host_args=(word, word))
    return profile


def move_trial(live, function, steps, hold_ms, delay_s, *, absolute_target=None, stimulus="input",
               stop_delay_s=None):
    """One finite move with one stimulus at ``delay_s`` after the start
    acceptance: the input (mosfet2 HOLD) or a serial stop. Observed through the
    hold, the release and POST_RELEASE_S; the prepared stop if still moving."""
    motion_ready(live)
    pre = pre_start(live)
    start_position = position(pre["state"]) if pre["state"] else None
    if absolute_target is None:
        handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", "60", "configured"),
                                    address=OWNER, timeout_s=max(live.timeout, 40))
    else:
        handle = live.console.begin("move-absolute", move_args=(str(absolute_target), "steps", "native", "60", "configured"),
                                    address=OWNER, timeout_s=max(live.timeout, 40))
    trial = {"function": function, "steps": steps, "hold_ms": hold_ms, "stimulus": stimulus, "y0": live.case.get("y0"),
             "start_position": start_position, "absolute_target": absolute_target}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    accepted = time.monotonic()
    first = len(live.samples)
    wait_running(live, accepted, delay_s)
    trial["operation_id"] = handle.operation_id
    if stimulus == "input":
        try:
            on, off = pulse(live, hold_ms, [named(OWNER, function, handle.operation_id, pre)])
            trial.update(on_us=on)
            observe(live, [OWNER], hold_ms / 1000.0 + POST_RELEASE_S)
            trial["off_us"] = off_us(live, on, hold_ms)
        except records.InterlockRefused as error:
            trial["interlock_refused"] = error.entry.get("reason")
            observe(live, [OWNER], 1.0)
    else:
        while time.monotonic() - accepted < delay_s + INTERLOCK_S:
            live.state()
        view = live.view()
        stop = live.call("stop", stop_policy="normal" if stimulus == "stop normal" else "fast", address=OWNER)
        trial.update(on_us=view["now_us"], off_us=view["now_us"], serial_stop=stop.get("outcome"))
        observe(live, [OWNER], POST_RELEASE_S + 1.0)
    last = live.samples[-1] if live.samples else None
    if last and moving(last):
        trial["continued_motion"] = True
        trial["safety_stop_us"] = live.view()["now_us"]
        trial["safety"] = reconcile(live, [OWNER])
    trial["terminal"] = live.console.wait(handle, release=True)
    t = trial["terminal"]
    trial["start_us"] = ((t.get("trigger_evidence") or {}).get("latest_us"))
    trial["samples"] = live.samples[first:]
    trial["reconcile"] = reconcile(live, [OWNER]) if not trial.get("safety") else trial["safety"]
    found, _ = edges(live)
    trial["edges"] = [e for e in found if e["at_us"] >= (trial.get("on_us") or 0) - 3000000]
    if live.case.get("y0") == 5 and trial.get("on_us"):
        edge = first_edge(trial["edges"], trial["on_us"], True)
        trial["lr_us"] = None if edge is None else edge["at_us"] - trial["on_us"]
    trial["analysis"] = classify(trial)
    trial["terminal_summary"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                       "polls", "raw_motion", "uncertain", "stop_inputs")}
    if function == 4:
        recover_released(live)
    return trial


def velocity_trial(live, function, rpm, duration_ms, hold_ms, delay_s):
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    snap = live.checked("velocity-read", address=OWNER)
    if "velocity" not in live.snapshots:
        v = snap["velocity_parameters"]
        live.snapshots["velocity"] = {"operation_id": snap["operation_id"], "address": OWNER,
                                      "words": [v["raw_speed"], v["raw_acceleration_time"], v["raw_deceleration_time"]]}
    live.checked("velocity-qualification-install", host_args=("rpm", "ramp", "inputs"))
    pre = pre_start(live)
    handle = live.console.begin("velocity", velocity_args=(str(rpm), "rpm", "native", str(duration_ms), "configured", "normal"),
                                address=OWNER, timeout_s=max(live.timeout, 10))
    trial = {"function": function, "steps": None, "hold_ms": hold_ms, "y0": live.case.get("y0")}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    accepted = time.monotonic()
    first = len(live.samples)
    wait_running(live, accepted, delay_s)
    on, off = pulse(live, hold_ms, [named(OWNER, function, handle.operation_id, pre)])
    trial.update(on_us=on, operation_id=handle.operation_id)
    observe(live, [OWNER], hold_ms / 1000.0 + POST_RELEASE_S)
    trial["off_us"] = off_us(live, on, hold_ms)
    trial["terminal"] = t = live.console.wait(handle, release=True)
    trial["start_us"] = (t.get("trigger_evidence") or {}).get("latest_us")
    trial["competing_end_us"] = t.get("stop_due_us")
    trial["samples"] = live.samples[first:]
    last = trial["samples"][-1] if trial["samples"] else None
    if last and moving(last):
        trial["continued_motion"] = True
        trial["safety_stop_us"] = live.view()["now_us"]
        trial["safety"] = reconcile(live, [OWNER])
    found, _ = edges(live)
    trial["edges"] = [e for e in found if e["at_us"] >= on - 3000000]
    trial["analysis"] = classify(trial)
    trial["terminal_summary"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                       "stop_completion", "stop_inputs", "stop_due_us")}
    return trial


def home_trial(live, function, method, search, hold_ms, delay_s):
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    live.state()
    snap = live.checked("home-read", address=OWNER)
    if "home" not in live.snapshots:
        h = snap["homing_parameters"]
        live.snapshots["home"] = {"operation_id": snap["operation_id"], "address": OWNER,
                                  "words": [h["raw_method"], h["raw_search_speed"], h["raw_return_speed"], h["raw_ramp_time"],
                                            *h["raw_offset_words"]],
                                  "baseline_words": records.snapshot_words(live.baseline, OWNER, "home")}
    facts = ("mechanics", "rates", "ramp", "zero-offset", "auxiliary", "inputs", "index", "reference")
    live.checked("home-qualification-install", host_args=(str(method), str(search), "30", "100", *facts))
    pre = pre_start(live)
    handle = live.console.begin("home", home_args=(str(method), str(search), "30", "100", "zero"), address=OWNER,
                                timeout_s=max(live.timeout, 40))
    trial = {"function": function, "method": method, "steps": None, "hold_ms": hold_ms, "y0": live.case.get("y0")}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    accepted = time.monotonic()
    first = len(live.samples)
    wait_running(live, accepted, delay_s)
    on, off = pulse(live, hold_ms, [named(OWNER, function, handle.operation_id, pre)])
    trial.update(on_us=on, operation_id=handle.operation_id)
    observe(live, [OWNER], hold_ms / 1000.0 + POST_RELEASE_S)
    trial["off_us"] = off_us(live, on, hold_ms)
    last = live.samples[-1] if live.samples else None
    if last and moving(last):
        trial["continued_motion"] = True
        trial["safety_stop_us"] = live.view()["now_us"]
        trial["safety"] = reconcile(live, [OWNER])
    trial["terminal"] = t = live.console.wait(handle, release=True)
    trial["samples"] = live.samples[first:]
    trial["reconcile"] = reconcile(live, [OWNER]) if not trial.get("safety") else trial["safety"]
    found, _ = edges(live)
    trial["edges"] = [e for e in found if e["at_us"] >= on - 3000000]
    trial["analysis"] = classify(trial)
    trial["homed_reported"] = bool(t.get("ok"))
    trial["terminal_summary"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion", "polls")}
    return trial


def refused_before_tx(live, label, name, **kwargs):
    """One motion command expected to be refused before transmission: the
    reply's reason and the debug traffic counter before and after."""
    before = live.checked("debug", host_args=())
    reply = live.call(name, **kwargs)
    after = live.checked("debug", host_args=())
    entry = {"label": label, "ok": reply.get("ok"), "result": reply.get("result"), "reason": reply.get("reason"),
             "terminal": reply.get("terminal"), "purpose": reply.get("purpose"), "address": reply.get("address"),
             "observed_before": before.get("observed"), "observed_after": after.get("observed")}
    entry["tx_records"] = None if entry["observed_before"] is None else entry["observed_after"] - entry["observed_before"]
    live.refusals.append(entry)
    return entry


def held_trial(live, function, hold_ms):
    """S5: mosfet2 held; read state; every motion kind refused before TX."""
    motion_ready(live)
    live.checked("velocity-read", address=OWNER)
    live.checked("velocity-qualification-install", host_args=("rpm", "ramp", "inputs"))
    snap = live.checked("home-read", address=OWNER)
    live.snapshots.setdefault("home", {"operation_id": snap["operation_id"], "address": OWNER,
                                       "words": [snap["homing_parameters"][k] for k in ("raw_method", "raw_search_speed",
                                                 "raw_return_speed", "raw_ramp_time")] + snap["homing_parameters"]["raw_offset_words"],
                                       "baseline_words": records.snapshot_words(live.baseline, OWNER, "home")})
    live.checked("home-qualification-install", host_args=("35", "60", "30", "100", "mechanics", "rates", "ramp",
                                                          "zero-offset", "auxiliary", "inputs"))
    group = live.call("group-set", host_args=tuple(str(a) for a in records.FIXTURE["input_drives"]),
                      timeout_s=max(live.timeout, 10))
    trial = {"function": function, "hold_ms": hold_ms, "group_set": group.get("ok")}
    try:
        trial["configure_facts"] = bool(probe.install_input_facts(live.console, OWNER, "configure", timeout_s=live.timeout))
    except probe.BenchError as error:
        trial["configure_facts"] = str(error)
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    live.checked("debug", host_args=("decoded",))
    on, off = pulse(live, hold_ms, [])
    trial["on_us"] = on
    st = live.state()
    trial["state"] = st
    if group.get("ok"):
        live.call("group-read-state", timeout_s=max(live.timeout, 10))
    word = str(live.snapshots["profile"]["current"][0])
    live.call("motion-profile-start-speed-relation", host_args=(word, word))
    refused_before_tx(live, "move", "move-relative", move_args=("200", "steps", "native", "60", "configured"), address=OWNER)
    refused_before_tx(live, "velocity", "velocity", velocity_args=("30", "rpm", "native", "200", "configured", "normal"),
                      address=OWNER)
    refused_before_tx(live, "home 35", "home", home_args=("35", "60", "30", "100", "zero"), address=OWNER)
    profile = live.snapshots["profile"]["current"]
    refused_before_tx(live, "stage (motion-profile stage)", "motion-profile-stage",
                      host_args=(*[str(w) for w in profile[1:4]], "0", "1000", "0", str(OWNER)))
    if group.get("ok"):
        refused_before_tx(live, "group move (staged)", "group-move",
                          host_args=("relative", "200", "steps", "native", "60", "configured", "start", "staged",
                                     "on-failure", "leave"))
        refused_before_tx(live, "group move (sequential)", "group-move",
                          host_args=("relative", "200", "steps", "native", "60", "configured", "start", "sequential",
                                     "on-failure", "leave"))
    trial["still_held"] = None
    view = live.view()
    o = next(x for x in view["outputs"] if x["name"] == "mosfet2")
    trial["still_held"] = bool(o["active"])
    remaining = (on + hold_ms * 1000 - view["now_us"]) / 1e6
    observe(live, [OWNER], max(0.0, remaining) + POST_RELEASE_S)
    trial["off_us"] = off_us(live, on, hold_ms)
    live.checked("debug", host_args=("off",))
    if group.get("ok"):
        live.call("group-clear")
    trial["refusals"] = list(live.refusals)
    live.refusals.clear()
    if function == 4:
        trial["after"] = recover_released(live)
    return trial


def race_trial(live, function, steps, hold_ms):
    """S6: read state with X3 inactive (the interlock's), mosfet2 3000 ms, then
    a move on that read. One deliberate run; the drive decides."""
    motion_ready(live)
    pre = pre_start(live)
    on, off = pulse(live, hold_ms, [])
    start_position = position(pre["state"]) if pre["state"] else None
    first = len(live.samples)
    handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", "60", "configured"),
                                address=OWNER, timeout_s=max(live.timeout, 40))
    trial = {"function": function, "on_us": on, "hold_ms": hold_ms, "steps": steps, "start_position": start_position,
             "accepted": handle.accepted, "y0": live.case.get("y0")}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
    view = live.view()
    observe(live, [OWNER], max(0.0, (on + hold_ms * 1000 - view["now_us"]) / 1e6) + POST_RELEASE_S + 3.5)
    trial["off_us"] = off_us(live, on, hold_ms)
    last = live.samples[-1] if live.samples else None
    if last and moving(last):
        trial["continued_motion"] = True
        trial["safety"] = reconcile(live, [OWNER])
    if handle.accepted:
        trial["terminal"] = t = live.console.wait(handle, release=True)
        trial["terminal_summary"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                           "polls", "stop_inputs")}
    trial["samples"] = live.samples[first:]
    trial["reconcile"] = reconcile(live, [OWNER])
    ran = [s for s in trial["samples"] if moving(s)]
    trial["started"] = bool(ran)
    trial["first_running_us"] = ran[0]["windows"]["motion"][0] - on if ran else None
    final = next((position(s) for s in reversed(trial["samples"]) if position(s) is not None), None)
    trial["travel"] = None if final is None or start_position is None else final - start_position
    trial["outcome_class"] = ("ignored" if not ran else
                              "start after release" if ran[0]["windows"]["motion"][0] > trial["off_us"] else "start while held")
    if function == 4:
        recover_released(live)
    return trial


def release_trial(live, hold_ms):
    """R1: stationary drive, X3 = 4, Y0 = 5: mosfet2 HOLD; LR, released, re-enabled by itself or not."""
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.state()
    on, off = pulse(live, hold_ms, [])
    first = len(live.samples)
    observe(live, [OWNER], hold_ms / 1000.0 + POST_RELEASE_S)
    trial = {"function": 4, "on_us": on, "off_us": off_us(live, on, hold_ms), "hold_ms": hold_ms, "y0": 5}
    trial["samples"] = live.samples[first:]
    found, _ = edges(live)
    trial["edges"] = [e for e in found if e["at_us"] >= on - 1000000]
    edge = first_edge(trial["edges"], on, True)
    trial["lr_us"] = None if edge is None else edge["at_us"] - on
    rel = [s for s in trial["samples"] if s["motion"] & RELEASED]
    trial["released_seen"] = bool(rel)
    trial["release_bracket_us"] = None
    if rel:
        before = [s for s in live.samples[:first] + trial["samples"] if s["windows"]["motion"][1] <= rel[0]["windows"]["motion"][0]
                  and not s["motion"] & RELEASED]
        start = before[-1]["windows"]["motion"][0] if before else None
        trial["release_bracket_us"] = [None if start is None else start - on, rel[0]["windows"]["motion"][1] - on]
    last = trial["samples"][-1] if trial["samples"] else None
    trial["released_at_end"] = bool(last and last["motion"] & RELEASED)
    trial["re_enabled_by_itself"] = bool(rel) and not trial["released_at_end"]
    trial["brake_on_edge_after_release_us"] = None
    edge_on = first_edge(trial["edges"], trial["off_us"], False)
    if edge_on is not None:
        trial["brake_on_edge_after_release_us"] = edge_on["at_us"] - trial["off_us"]
    trial["after"] = recover_released(live)
    return trial


def enable_trial(live, hold_ms):
    """R3: mosfet2 3000 ms with X3 = 4; while held, read state, then enable
    (if admitted) and read the result; then after the release."""
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.state()
    on, off = pulse(live, hold_ms, [])
    trial = {"function": 4, "on_us": on, "hold_ms": hold_ms}
    first = len(live.samples)
    for _ in range(6):
        live.state()
    trial["before_enable"] = live.samples[-1]
    enable = live.call("enable", address=OWNER)
    trial["enable"] = {k: enable.get(k) for k in ("ok", "result", "outcome", "state", "execution", "completion",
                                                  "status", "detail", "reason", "started_us")}
    view = live.view()
    observe(live, [OWNER], max(0.0, (on + hold_ms * 1000 - view["now_us"]) / 1e6 - 0.2))
    trial["during_hold_after_enable"] = live.samples[-1]
    observe(live, [OWNER], 0.4 + POST_RELEASE_S)
    trial["off_us"] = off_us(live, on, hold_ms)
    trial["samples"] = live.samples[first:]
    held = [s for s in trial["samples"] if s["windows"]["motion"][1] < trial["off_us"] and
            enable.get("started_us") and s["windows"]["motion"][0] > enable["started_us"]]
    trial["released_after_enable_while_held"] = bool(held) and all(s["motion"] & RELEASED for s in held)
    trial["enabled_while_held"] = bool(held) and any(not s["motion"] & RELEASED for s in held)
    last = trial["samples"][-1] if trial["samples"] else None
    trial["released_after_release"] = bool(last and last["motion"] & RELEASED)
    trial["after"] = recover_released(live)
    return trial


def serial_trial(live, sub, steps, hold_ms, delay_s):
    """S7 (X3 = 6, deceleration 1000 ms): (a) stop normal, then mosfet2 after its
    ACK; (b) mosfet2, then stop fast; (c) stop fast while mosfet2 held."""
    motion_ready(live)
    pre = pre_start(live)
    start_position = position(pre["state"]) if pre["state"] else None
    handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", "60", "configured"),
                                address=OWNER, timeout_s=max(live.timeout, 40))
    trial = {"sub": sub, "function": 6, "steps": steps, "start_position": start_position, "y0": 2}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    accepted = time.monotonic()
    first = len(live.samples)
    wait_running(live, accepted, delay_s)
    member = named(OWNER, 6, handle.operation_id, pre)
    if sub == "a":
        stop = live.console.begin("stop", stop_policy="normal", address=OWNER, timeout_s=live.timeout)
        trial["stop_accepted"] = stop.accepted
        # The stop request turned every bench output off; ON after its ACK, the stop named.
        member2 = dict(member, operation_id=stop.operation_id)
        # Every operation the console admitted after the pre-start io read: the
        # reads of the delay (from the recorded events) and the move itself.
        first_id = pre["io_read"]["operation_id"]
        reads = [{"command": e["command"], "operation_id": e["operation_id"]} for e in live.events
                 if type(e.get("operation_id")) is int and first_id < e["operation_id"] < stop.operation_id]
        member2["between"] = sorted(reads + [{"command": "move-relative", "operation_id": handle.operation_id, "motion": True}],
                                    key=lambda b: b["operation_id"])
        try:
            on, off = pulse(live, hold_ms, [member2, member])
            trial.update(on_us=on)
        except Exception as error:  # noqa: BLE001
            trial["pulse_error"] = str(error)
        observe(live, [OWNER], hold_ms / 1000.0 + POST_RELEASE_S)
        trial["stop_terminal"] = live.console.wait(stop, release=True)
    elif sub == "b":
        on, off = pulse(live, hold_ms, [member])
        trial.update(on_us=on)
        stop = live.call("stop", stop_policy="fast", address=OWNER)
        trial["stop_fast"] = {k: stop.get(k) for k in ("ok", "outcome", "started_us")}
        observe(live, [OWNER], POST_RELEASE_S)
    else:
        on, off = pulse(live, 3000, [member])
        trial.update(on_us=on)
        live.state()
        stop = live.call("stop", stop_policy="fast", address=OWNER)
        trial["stop_fast"] = {k: stop.get(k) for k in ("ok", "outcome", "started_us")}
        view = live.view()
        o = next(x for x in view["outputs"] if x["name"] == "mosfet2")
        trial["output_after_stop"] = {k: o.get(k) for k in ("active", "off_us", "off_reason")}
        observe(live, [OWNER], POST_RELEASE_S)
        hold_ms = 3000
    trial["off_us"] = off_us(live, trial.get("on_us"), hold_ms) if trial.get("on_us") else None
    last = live.samples[-1] if live.samples else None
    if last and moving(last):
        trial["continued_motion"] = True
        trial["safety"] = reconcile(live, [OWNER])
    trial["terminal"] = t = live.console.wait(handle, release=True)
    trial["terminal_summary"] = {k: t.get(k) for k in ("ok", "outcome", "status", "detail", "interrupted_by_stop")}
    trial["samples"] = live.samples[first:]
    trial["reconcile"] = reconcile(live, [OWNER])
    found, _ = edges(live)
    trial["edges"] = [e for e in found if e["at_us"] >= (trial.get("on_us") or 0) - 3000000]
    final = next((position(s) for s in reversed(trial["samples"]) if position(s) is not None), None)
    trial["travel"] = None if final is None or start_position is None else final - start_position
    if trial.get("on_us"):
        trial["analysis"] = classify(dict(trial, hold_ms=hold_ms))
    return trial


def group_trial(live, function, steps, hold_ms, delay_s, mode):
    """G1 (staged) / G2 (sequential): one group move of drives 1, 2, 4 and one
    pulse on the shared X3, every member named with its pre-start io read."""
    members = list(records.FIXTURE["input_drives"])
    live.checked("group-read-state", timeout_s=max(live.timeout, 10))
    pres = {}
    for address in members:
        io = live.checked("io", driver_args=("read",), address=address)
        pres[address] = {"io_read": {"ok": True, "operation_id": io["operation_id"], "started_us": io.get("started_us"),
                                     "raw": io["observation"]["raw"]}}
    positions = {}
    for address in members:
        s = live.state(address)
        positions[address] = position(s) if s else None
    handle = live.console.begin("group-move", host_args=("relative", str(steps), "steps", "native", "60", "configured",
                                                         "start", mode, "on-failure", "leave"),
                                timeout_s=max(live.timeout, 40))
    trial = {"function": function, "mode": mode, "steps": steps, "hold_ms": hold_ms, "start_positions": positions}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    group_id = handle.operation_id
    accepted = time.monotonic()
    first = len(live.samples)
    index = 0
    while time.monotonic() - accepted < delay_s:  # Every member sampled before the stimulus.
        live.state(members[index % len(members)])
        index += 1
    # Member move operations follow the group operation's ID in member order
    # (GroupApp startGroupMove); a wrong assumption fails the L3 check (no ON).
    read_ids = [pres[a]["io_read"]["operation_id"] for a in members]
    named_members = []
    for index, address in enumerate(members):
        own = group_id + 1 + index
        between = [{"command": "io", "operation_id": i, "read": True} for i in read_ids if i > pres[address]["io_read"]["operation_id"]]
        between += [{"command": "read-state", "operation_id": i} for i in range(read_ids[-1] + 1, group_id)]
        between += [{"command": "group-move", "operation_id": i, "motion": True} for i in range(group_id, own)]
        pre = {"io_read": pres[address]["io_read"], "between": between}
        named_members.append(named(address, function, own, pre, console_id=group_id, console_address=handle.address))
    trial["named"] = named_members
    try:
        on, off = pulse(live, hold_ms, named_members)
        trial["on_us"] = on
        observe(live, members, hold_ms / 1000.0 + POST_RELEASE_S + 1.0)
        trial["off_us"] = off_us(live, on, hold_ms)
    except records.InterlockRefused as error:
        trial["interlock_refused"] = error.entry.get("reason")
        observe(live, members, 4.0)
    moving_members = [a for a in members if live.samples and any(
        s["address"] == a and moving(s) for s in live.samples[-6:])]
    if moving_members:
        trial["continued_motion"] = moving_members
        trial["safety"] = live.call("group-stop", host_args=("fast",)).get("outcome")
    trial["terminal"] = t = live.console.wait(handle, release=True)
    trial["samples"] = live.samples[first:]
    trial["reconcile"] = reconcile(live, members)
    trial["members"] = []
    for m in t.get("members", []) if isinstance(t.get("members"), list) else []:
        trial["members"].append({k: m.get(k) for k in ("address", "operation_id", "start", "state", "outcome", "status",
                                                       "detail", "completion", "start_tx_end_us", "running_observed")})
    found, _ = edges(live)
    trial["edges"] = [e for e in found if e["at_us"] >= (trial.get("on_us") or 0) - 3000000]
    if trial.get("on_us"):
        trial["per_member"] = {}
        for address in members:
            mine = [s for s in trial["samples"] if s["address"] == address]
            member = next((m for m in trial["members"] if m.get("address") == address), {})
            trial["per_member"][address] = classify({
                "on_us": trial["on_us"], "off_us": trial["off_us"], "samples": mine, "steps": steps,
                "start_position": positions[address], "start_us": member.get("start_tx_end_us"),
                "terminal": member, "edges": trial["edges"] if address == OWNER else [],
                "y0": 2 if address == OWNER else None, "function": function})
        starts = [m.get("start_tx_end_us") for m in trial["members"] if m.get("start_tx_end_us")]
        trial["starts_after_on"] = sum(1 for s in starts if s > trial["on_us"])
    return trial


# --------------------------------------------------------------------------
# Verdicts

def move_verdict(trials: list) -> tuple[str, str]:
    """S1/S2/R2b: every trial input-stopped (stopped short, not reported complete,
    no further start in the window); continued motion is FAIL."""
    if not trials:
        return "NOT RUN", "no trial"
    if any(t.get("continued_motion") for t in trials):
        return "FAIL", "motion continued after the stimulus; prepared stop sent"
    if any(t.get("refused") for t in trials):
        return "FAIL", "a trial was refused before transmission"
    bad = [i + 1 for i, t in enumerate(trials) if t["analysis"]["witnesses"].get("reported_complete") or
           t["analysis"]["witnesses"].get("further_start")]
    if bad:
        return "FAIL", "trials %s reported completion or started again" % bad
    open_ = [(i + 1, t["analysis"]["missing"]) for i, t in enumerate(trials) if t["analysis"]["class"] != "input-stopped"]
    if open_:
        return "indeterminate", "; ".join("trial %d: %s" % (i, ", ".join(m) or "not stopped") for i, m in open_)
    return "PASS", None


def set_deceleration(live, deceleration):
    """S7/S8: 0x0022 through `motion-profile stage` (CONFIGURE facts, one
    profile session); the other words are the current ones of the drive."""
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    live.state()
    profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
    live.snapshots.setdefault("profile", {k: profile.get(k) for k in ("original", "current")})
    current = list(profile["current"])
    if current[2] == deceleration:
        return current
    words = [current[1], deceleration, current[3], current[4], current[5]]
    step = {"drive": OWNER, "key": "profile.deceleration", "index": None, "read": "motion-profile", "purpose": "configure",
            "facts": {}, "baseline": current[2], "candidate": deceleration, "target": deceleration,
            "write": {"command": "motion-profile-stage", "kwargs": {"host_args": (*[str(w) for w in words], "0", str(OWNER))},
                      "text": "motion-profile stage %s 0 %d" % (" ".join(str(w) for w in words), OWNER)}}
    outcome = records.apply_write(live.console, step, timeout_s=live.timeout)
    live.events.append({"command": step["write"]["text"], "written": outcome["written"], "uncertain": outcome["uncertain"],
                        "result": outcome["result"]})
    live.written.add((OWNER, "profile"))
    if outcome["uncertain"] or not outcome["written"]:
        raise probe.BenchError("deceleration %d not written: %s" % (deceleration, outcome["result"]))
    live.checked("read-config", address=OWNER)
    profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
    if profile["current"][2] != deceleration:
        raise probe.BenchError("deceleration read back %s" % profile["current"][2])
    return list(profile["current"])


def brake_setup(live):
    """Y0 = 5 assigned while enabled stays off until the next enable (wired bench 06): one recorded cycle."""
    for command in ("motor-release", "enable"):
        live.checked(command, address=OWNER)
        for _ in range(3):
            live.state()
    time.sleep(1.1)


def run_case(live, args):
    """The trials of one case and X3 function; returns the detail record."""
    case, function = live.case, args.function
    detail = {"case": args.case, "function": function, "trials": [], "stimulus": args.stimulus or "input",
              "deceleration_requested": args.deceleration}
    if case.get("y0") is not None:
        write_setting(live, OWNER, "io.y0", case["y0"])
        if case["y0"] == 5:
            brake_setup(live)
    if args.case in ("S3", "S4", "S5"):
        # The restore snapshot is the first checked read of this binding: the
        # firmware pins it, later reads rotate through 7 slots (wired bench 06),
        # and a later `useaddr` makes every snapshot stale. Taken before any write.
        records.use_drive(live.console, OWNER, timeout_s=live.timeout)
        name = "velocity" if args.case == "S3" else "home"
        snap = live.checked("velocity-read" if name == "velocity" else "home-read", address=OWNER)
        if name == "velocity":
            v = snap["velocity_parameters"]
            words = [v["raw_speed"], v["raw_acceleration_time"], v["raw_deceleration_time"]]
        else:
            h = snap["homing_parameters"]
            words = [h["raw_method"], h["raw_search_speed"], h["raw_return_speed"], h["raw_ramp_time"], *h["raw_offset_words"]]
        live.snapshots[name] = {"operation_id": snap["operation_id"], "address": OWNER, "words": words,
                                "baseline_words": records.snapshot_words(live.baseline, OWNER, name)}
    if args.case in ("S4", "S5"):
        # The home qualification needs the stored auxiliary 7 (keep position,
        # set zero), as wired bench 06 O7; planned route, restored to 0.
        write_setting(live, OWNER, "homing.auxiliary", 7)
    assign_x3(live, function)
    live.view()
    kind, runs = case["kind"], args.repetitions
    if kind in ("move", "absolute", "width"):
        if args.case == "S8":
            detail["deceleration"] = set_deceleration(live, args.deceleration)
        widths = list(args.widths or case["widths"]) if kind == "width" else [case["hold_ms"]]
        stimulus = args.stimulus or "input"
        for width in widths:
            for index in range(runs):
                steps = case["steps"] if index % 2 == 0 else -case["steps"]
                target = None
                if kind == "absolute":
                    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
                    s = live.state()
                    target = position(s) + steps
                trial = move_trial(live, function, steps, width, case["delay_s"], absolute_target=target, stimulus=stimulus)
                trial.update(index=index + 1, width_ms=width if kind == "width" else None)
                detail["trials"].append(trial)
                if trial.get("continued_motion") and kind != "width":
                    raise probe.BenchError("motion continued after the stimulus; prepared stop sent; case ended")
    elif kind == "velocity":
        for index in range(runs):
            detail["trials"].append(dict(velocity_trial(live, function, case["rpm"], case["duration_ms"], case["hold_ms"],
                                                        case["delay_s"]), index=index + 1))
    elif kind == "home":
        for method in case["methods"]:
            for index in range(runs):
                detail["trials"].append(dict(home_trial(live, function, method, case["search"], case["hold_ms"],
                                                        case["delay_s"]), index=index + 1))
    elif kind == "held":
        detail["trials"].append(held_trial(live, function, case["hold_ms"]))
    elif kind == "race":
        detail["trials"].append(race_trial(live, function, case["steps"], case["hold_ms"]))
    elif kind == "release":
        for index in range(runs):
            detail["trials"].append(dict(release_trial(live, case["hold_ms"]), index=index + 1))
    elif kind == "enable":
        detail["trials"].append(enable_trial(live, case["hold_ms"]))
    elif kind == "serial":
        detail["deceleration"] = set_deceleration(live, case["deceleration"])
        for sub in (args.sub or "abc"):
            detail["trials"].append(serial_trial(live, sub, case["steps"], case["hold_ms"], case["delay_s"]))
    elif kind == "group":
        live.checked("group-set", host_args=tuple(str(a) for a in records.FIXTURE["input_drives"]),
                     timeout_s=max(live.timeout, 10))
        try:
            for index in range(runs):
                trial = group_trial(live, function, case["steps"] if index % 2 == 0 else -case["steps"], case["hold_ms"],
                                    case["delay_s"], case["mode"])
                detail["trials"].append(dict(trial, index=index + 1))
                if trial.get("continued_motion"):
                    raise probe.BenchError("group motion continued; group stop fast sent; case ended")
        finally:
            live.call("group-clear")
    detail["summary"] = summarize([t for t in detail["trials"] if "analysis" in t])
    return detail


def case_verdicts(run, cid, case, detail, error):
    """Verdict rows per the prompt 07 case table. Report-only rows (S6-S9, R1,
    R3, G1, G2) get PASS for a complete recording; their findings are judged in the report."""
    trials = detail.get("trials", [])
    evidence = ["stop_inputs.json"]
    stopped = "stopped early: %s" % error if error else None
    kind = case["kind"]
    if not trials:
        for level in case["levels"]:
            run.verdict(cid, level, "NOT RUN", stopped or "no trial ran", evidence)
        return
    if kind in ("move", "absolute", "velocity") and detail.get("stimulus") == "input" and cid not in ("S8",):
        verdict, reason = move_verdict(trials)
        if error and verdict == "PASS":
            verdict, reason = "indeterminate", stopped
        for level in case["levels"]:
            run.verdict(cid, level, verdict, reason, evidence)
        return
    if kind == "group":
        verdict, reason = group_verdict(trials)
        if error and verdict == "PASS":
            verdict, reason = "indeterminate", stopped
        for level in case["levels"]:
            run.verdict(cid, level, verdict, reason, evidence)
        return
    if kind == "home":
        homed = [t for t in trials if t.get("homed_reported")]
        cont = [t for t in trials if t.get("continued_motion")]
        early = [t for t in trials if not t.get("on_us")]
        verdict = "FAIL" if homed or cont else "indeterminate" if early or error else "PASS"
        run.verdict(cid, "drive-reported", verdict, None if verdict == "PASS" else
                    "%d of %d reported homed, %d continued, %d without a stimulus%s" % (
                        len(homed), len(trials), len(cont), len(early), "; " + stopped if stopped else ""), evidence)
        return
    if kind == "held":
        verdict, reason = held_verdict(trials[0].get("refusals", []), trials[0].get("function"))
        if error and verdict == "PASS":
            verdict, reason = "indeterminate", stopped
        run.verdict(cid, "sent", verdict, reason, evidence)
        return
    for level in case["levels"]:
        run.verdict(cid, level, "PASS" if not error else "indeterminate",
                    "recorded measurement (report-only case)" + ("; " + stopped if stopped else ""), evidence)


def group_verdict(trials: list) -> tuple[str, str | None]:
    """G1/G2: every member stopped short with its own failed outcome (never
    reported complete), no member started again in the window, and no start
    was sent after the ON. Per-member classes are reported alongside."""
    if not trials:
        return "NOT RUN", "no trial"
    problems = []
    for t in trials:
        if t.get("refused"):
            problems.append("trial %s refused before transmission" % t.get("index"))
            continue
        if t.get("interlock_refused") or not t.get("on_us"):
            problems.append("trial %s: no stimulus (%s)" % (t.get("index"), t.get("interlock_refused")))
            continue
        if t.get("continued_motion"):
            return "FAIL", "trial %s: members %s kept moving; group stop fast sent" % (t.get("index"), t["continued_motion"])
        if t.get("starts_after_on"):
            return "FAIL", "trial %s: %d starts sent after the ON" % (t.get("index"), t["starts_after_on"])
        for address, member in (t.get("per_member") or {}).items():
            w = member["witnesses"]
            term = next((m for m in t.get("members", []) if str(m.get("address")) == str(address)), {})
            if w.get("reported_complete") or term.get("state") == "succeeded":
                return "FAIL", "trial %s: member %s reported complete" % (t.get("index"), address)
            if w.get("further_start"):
                return "FAIL", "trial %s: member %s started again" % (t.get("index"), address)
            if not w.get("stopped_short"):
                problems.append("trial %s member %s: not stopped short (travel %s)" % (t.get("index"), address, w.get("travel")))
    if problems:
        return "indeterminate", "; ".join(problems)
    return "PASS", None


def held_verdict(refusals: list, function: int) -> tuple[str, str | None]:
    """S5: each motion command refused before TX (zero traffic records) with
    the stop/release reason (the profile stage with the settings reason
    input_active). An admitted command or traffic is FAIL; a refusal for
    another cause is indeterminate: it does not exercise the rule."""
    if not refusals:
        return "NOT RUN", "no command tried"
    expected = "release_input_active" if function == 4 else "stop_input_active"
    bad = [r for r in refusals if r["ok"] or r["tx_records"] != 0]
    if bad:
        return "FAIL", "; ".join("%s: ok %s, %s traffic records" % (r["label"], r["ok"], r["tx_records"]) for r in bad)
    other = [r for r in refusals if r["reason"] != ("input_active" if r["label"].startswith("stage") else expected)]
    if other:
        return "indeterminate", "; ".join("%s refused %s for another cause (reason %s, member %s)" % (
            r["label"], r["result"], r["reason"], r.get("address")) for r in other)
    return "PASS", None
