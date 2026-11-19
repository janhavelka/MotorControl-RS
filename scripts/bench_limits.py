"""Wired bench 11: limit inputs and soft limits (`limits` scenario).

Hardware limits: X1 (``relay2``) holds the power-up function 2 (positive
limit) and X2 (``relay1``) function 3 (negative limit) on drives 1, 2 and 4.
Each relay drives the same X pin of all three drives. One run folder runs one
or more cases (``--cases``) on drive 4, or on drives 1, 2 and 4 for the group
and stationary cases. Every ON goes through ``bench_records.bench_on`` (D14
interlock); a running member is named with its pre-start ``io read`` and the
effect ``limit``. Drive 4's Y0 = 2 (running) gives the controller-timestamped
edge unless ``--polls-only`` (the V-01 run: no I/O write reaches drive 4).

The analysis reuses prompt 07's witnesses (``bench_stop_inputs.classify``)
with the limit's own status bit: a move toward the limit is "input-stopped"
when it stopped short after the assertion with nothing else to explain it; a
move away is "not stopped" when it ran to its endpoint. Every trial records the
motion word bits 4-6, the alarm word and the position at the end of the hold,
after the release and after the prepared stop, and the recovery steps the
drive needed (alarm-clear, enable, release-and-enable), each followed by a
fresh read.

Soft-limit cases (S1-S7) need the production enable route of prompt 11 and
run through ``bench_soft_limits`` cases in the same scenario.
"""
import random
import time

import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stop

OWNER = records.FIXTURE["y0_owner"]
LIMITS = {"positive": {"output": "relay2", "terminal": "x1", "function": 2, "bit": 2},
          "negative": {"output": "relay1", "terminal": "x2", "function": 3, "bit": 4}}
RUNNING, IN_POSITION, HOMED, ALARM, RELEASED, POSITIVE_FLAG, NEGATIVE_FLAG = 4, 1, 2, 8, 16, 32, 64
POST_RELEASE_S = 2.0
MOVE_STEPS = 4000
Y0_FACTS = list(stop.Y0_FACTS)
PROFILE_KEYS = stop.PROFILE_KEYS

# Case table (prompt 11 bench step 4). trials: (limit, signed steps or rpm);
# runs repeat the trial list. kind selects the runner.
CASES = {
    "L1": dict(kind="move", trials=[("positive", MOVE_STEPS)], runs=3, hold_ms=3000, delay_s=1.0, over_limit=0,
               levels=["drive-reported", "controller-timestamped"],
               assertion="0x0017 = 0, +4000 move at 60 rpm, relay2 (X1, function 2) 3000 ms at ~1/3 travel: the drive "
                         "stops short; bits 4-6, alarm, position and recovery recorded"),
    "L2": dict(kind="move", trials=[("positive", -MOVE_STEPS)], runs=2, hold_ms=3000, delay_s=1.0, over_limit=0,
               levels=["drive-reported"],
               assertion="0x0017 = 0, -4000 move, X1 asserted mid-move (away from the positive limit): effect recorded, "
                         "reproduced twice"),
    "L3": dict(kind="move", trials=[("negative", -MOVE_STEPS), ("negative", MOVE_STEPS)], runs=2, hold_ms=3000,
               delay_s=1.0, over_limit=0, levels=["drive-reported"],
               assertion="0x0017 = 0, -4000 (toward) and +4000 (away) moves with relay1 (X2, function 3) mid-move: "
                         "mirror of L1/L2"),
    "L4": dict(kind="move", trials=[("positive", MOVE_STEPS), ("negative", -MOVE_STEPS)], runs=3, hold_ms=3000,
               delay_s=1.0, over_limit=1, levels=["drive-reported"],
               assertion="0x0017 = 1: L1 and the L3 toward case, 3 runs each; stop distance, bit 4 and alarm against L1"),
    "L6": dict(kind="recovery", trials=[("positive", MOVE_STEPS), ("negative", -MOVE_STEPS)], runs=1, hold_ms=3000,
               delay_s=1.0, over_limit=0, levels=["drive-reported"],
               assertion="after a toward stop with the limit held: deassert, then a 1000-step move toward and one away; "
                         "the recovery steps the drive needed recorded"),
    "L7": dict(kind="velocity", trials=[("positive", 60), ("negative", -60)], runs=2, hold_ms=1500, delay_s=0.0,
               over_limit=0, levels=["drive-reported"],
               assertion="velocity +60 rpm 1000 ms with relay2 at ~0.5 s, mirror -60 rpm with relay1: the drive stops; "
                         "the velocity result names the reason"),
    "L9": dict(kind="group", trials=[("positive", MOVE_STEPS)], runs=1, hold_ms=3000, delay_s=1.0, over_limit=0,
               levels=["drive-reported"],
               assertion="staged group 1, 2, 4, +4000, relay2 mid-move: every member stops short"),
    "L10": dict(kind="stationary", trials=[("positive", None), ("negative", None)], runs=1, hold_ms=2000,
                over_limit=0, levels=["drive-reported"],
                assertion="drives 1, 2, 4 stopped: relay2 then relay1 held 2 s each; status and alarm per drive, no motion"),
    "L5a": dict(kind="held", trials=[("positive", None), ("negative", None)], runs=1, hold_ms=4000, over_limit=0,
                levels=["sent"],
                assertion="drive 4 stopped, the limit line held: moves +500 and -500 and velocity +60 and -60 rpm "
                          "refused before TX (limit_input_active, no operation, debug traffic unchanged)"),
    # Soft limits through the production enable route (one flow per run; S4 its own run per 0x0017 value).
    "S1": dict(kind="soft", trials=[], runs=1, over_limit=0, levels=["read back"],
               assertion="home 35 (auxiliary 7, reference and soft-limits facts), pairs +500/-500, driver set soft-limit 1: "
                         "read back 1, host soft limits [-500, 500] installed only after the readback, flags clear"),
    "S2": dict(kind="soft", trials=[], runs=1, over_limit=0, levels=["drive-reported"],
               assertion="bound: absolute moves to +400 and -400 complete at their targets"),
    "S3": dict(kind="soft", trials=[], runs=1, over_limit=0, levels=["sent"],
               assertion="bound: absolute move to +700 refused before TX (soft_limit_endpoint, no operation, no traffic)"),
    "S4": dict(kind="soft-velocity", trials=[], runs=1, over_limit=0, levels=["drive-reported"],
               assertion="bound at the run's 0x0017: velocity +60 rpm 1000 ms from 0 stops near +500 with bit 5, then -60 rpm "
                         "near -500 with bit 6; result SOFT_LIMIT (29); final positions and flags recorded"),
    "S6": dict(kind="soft", trials=[], runs=1, over_limit=0, levels=["sent"],
               assertion="bound: position-clear makes the state unknown; the next move refused before TX "
                         "(soft_limit_state_unknown); disable, home and enable recover"),
    "S7": dict(kind="soft", trials=[], runs=1, over_limit=0, levels=["drive-reported"],
               assertion="disable read back (known disabled, host limits removed); absolute move to +700 completes"),
}


def case_drives(case: dict) -> list:
    return list(records.FIXTURE["input_drives"]) if case["kind"] in ("group", "stationary") else [OWNER]


def flags(s: dict | None) -> dict | None:
    """Motion word bits 3-6, the alarm word, the X status and the position of one sample."""
    if not s:
        return None
    m = s["motion"]
    return {"motion": m, "alarm": s["alarm"], "alarm_bit": bool(m & ALARM), "released": bool(m & RELEASED),
            "positive_flag": bool(m & POSITIVE_FLAG), "negative_flag": bool(m & NEGATIVE_FLAG),
            "running": bool(m & RUNNING), "homed": bool(m & HOMED), "inputs": s["inputs"], "speed": s["speed"],
            "position": stop.position(s), "address": s.get("address")}


class Live(stop.Live):
    """Run state of one limits run; the prompt 07 helpers use the same fields."""

    def __init__(self, console, run, baseline, args, facts, timeout, cid):
        self.console, self.run, self.baseline, self.args, self.facts = console, run, baseline, args, facts
        self.timeout = timeout
        self.case = dict(CASES[cid], y0=None if args.polls_only else 2)
        self.samples, self.views, self.events, self.trials, self.refusals = [], [], [], [], []
        self.snapshots = {}
        self.record = {"running_members": [], "max_age_ms": records.MAX_AGE_MS, "read_last": OWNER}
        self.rng = random.Random(11)
        self.written = set()
        self.over_limit = None

    def select(self, cid):
        self.case = dict(CASES[cid], y0=None if self.args.polls_only else 2)


def named(address, limit, operation_id, pre, console_id=None, console_address=None):
    spec = LIMITS[limit]
    member = {"output": spec["output"], "terminal": spec["terminal"], "effect": "limit", "function": spec["function"],
              "polarity": 0, "operation_id": operation_id, "address": address, "io_read": pre["io_read"],
              "between": list(pre["between"])}
    if console_id is not None:
        member.update(console_operation_id=console_id, console_address=console_address)
    return member


def pulse(live, limit, hold_ms, members):
    """One interlocked relay ON (the limit's line) with the named running members."""
    output = LIMITS[limit]["output"]
    live.record["running_members"] = members
    try:
        reply = records.bench_on(live.console, output, hold_ms, live.record)
    finally:
        live.record["running_members"] = []
    on = records._bench_view(reply)["outputs"][output]
    live.views.append(reply)
    return on["on_us"], on["on_us"] + hold_ms * 1000


def off_us(live, limit, on_us, hold_ms):
    output = LIMITS[limit]["output"]
    for v in reversed(live.views):
        o = next((x for x in v.get("outputs", []) if x.get("name") == output), None)
        if o and o.get("on_us") == on_us and not o.get("active"):
            return o.get("off_us")
    return on_us + hold_ms * 1000


def phase_flags(samples: list, on_us: int, off_us_: int) -> dict:
    """Last sample of the drive while the limit was held and the last one after the release."""
    held = [s for s in samples if s and on_us <= s["windows"]["motion"][0] and s["windows"]["motion"][1] <= off_us_]
    after = [s for s in samples if s and s["windows"]["motion"][0] >= off_us_]
    flagged = [s for s in samples if s and s["windows"]["motion"][0] >= on_us and s["motion"] & (POSITIVE_FLAG | NEGATIVE_FLAG)]
    return {"held_last": flags(held[-1]) if held else None, "released_last": flags(after[-1]) if after else None,
            "first_flag": flags(flagged[0]) if flagged else None,
            "flag_us": None if not flagged else flagged[0]["windows"]["motion"][1] - on_us,
            "released_seen": any(s["motion"] & RELEASED for s in samples if s and s["windows"]["motion"][0] >= on_us),
            "alarm_seen": sorted({s["alarm"] for s in samples if s and s["alarm"]})}


def recover(live, address=OWNER):
    """The recovery the drive needs after a limit stop, in order: alarm-clear,
    enable, release then enable; each step only while its condition holds,
    each followed by a fresh read. Returns the steps and the final flags."""
    rows = []
    s = flags(live.state(address))
    rows.append({"step": "start", "flags": s})
    if s and (s["alarm"] or s["alarm_bit"]):
        rows.append({"step": "alarm-clear", "outcome": live.call("alarm-clear", address=address).get("outcome")})
        s = flags(live.state(address))
        rows.append({"step": "after alarm-clear", "flags": s})
    if s and s["released"]:
        rows.append({"step": "enable", "outcome": live.call("enable", address=address).get("outcome")})
        time.sleep(1.1)
        s = flags(live.state(address))
        rows.append({"step": "after enable", "flags": s})
    if s and (s["positive_flag"] or s["negative_flag"]):
        for name in ("motor-release", "enable"):
            rows.append({"step": name, "outcome": live.call(name, address=address).get("outcome")})
            time.sleep(1.1 if name == "enable" else 0.2)
        s = flags(live.state(address))
        rows.append({"step": "after release and enable", "flags": s})
    needed = [r["step"] for r in rows if "outcome" in r]
    return {"steps": rows, "needed": needed or ["none"], "final": s}


def move_trial(live, limit, steps, hold_ms, delay_s, *, stimulus=True):
    """One finite move of drive 4 with the limit line ON at ``delay_s`` after
    the start acceptance (``stimulus``), observed through the hold, the release
    and POST_RELEASE_S; the prepared stop if still moving; then the recovery."""
    spec = LIMITS[limit]
    stop.motion_ready(live)
    pre = stop.pre_start(live)
    start_position = stop.position(pre["state"]) if pre["state"] else None
    handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", "60", "configured"),
                                address=OWNER, timeout_s=max(live.timeout, 40))
    toward = (steps > 0) == (limit == "positive")
    trial = {"limit": limit, "function": spec["function"], "input_bit": spec["bit"], "steps": steps, "toward": toward,
             "hold_ms": hold_ms, "y0": live.case.get("y0"), "start_position": start_position,
             "over_limit": live.over_limit, "stimulus": stimulus}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    accepted = time.monotonic()
    first = len(live.samples)
    stop.wait_running(live, accepted, delay_s)
    trial["operation_id"] = handle.operation_id
    if stimulus:
        try:
            on, off = pulse(live, limit, hold_ms, [named(OWNER, limit, handle.operation_id, pre)])
            trial["on_us"] = on
            stop.observe(live, [OWNER], hold_ms / 1000.0 + POST_RELEASE_S)
            trial["off_us"] = off_us(live, limit, on, hold_ms)
        except records.InterlockRefused as error:
            trial["interlock_refused"] = error.entry.get("reason")
            stop.observe(live, [OWNER], 5.0)
    else:
        stop.observe(live, [OWNER], abs(steps) / 1000.0 + 1.5)
    return finish_trial(live, trial, handle, first, [OWNER])


def finish_trial(live, trial, handle, first, addresses):
    last = live.samples[-1] if live.samples else None
    if last and stop.moving(last):
        trial["continued_motion"] = True
        trial["safety_stop_us"] = live.view()["now_us"]
        trial["safety"] = stop.reconcile(live, addresses)
    trial["terminal"] = t = live.console.wait(handle, release=True)
    trial["start_us"] = (t.get("trigger_evidence") or {}).get("latest_us")
    trial["samples"] = live.samples[first:]
    if trial.get("on_us"):
        trial["phases"] = phase_flags([s for s in trial["samples"] if s.get("address") in (OWNER, None)],
                                      trial["on_us"], trial["off_us"])
    found, _ = stop.edges(live)
    trial["edges"] = [e for e in found if e["at_us"] >= (trial.get("on_us") or 0) - 3000000]
    trial["analysis"] = stop.classify(trial) if trial.get("on_us") else None
    trial["terminal_summary"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                       "polls", "raw_motion", "uncertain", "stop_inputs",
                                                       "stop_completion", "stop_due_us", "limit_inputs")}
    # The prepared stop reconciles an uncertain or failed operation; the state
    # before it is in the samples, the state after it in the recovery.
    trial["before_reconcile"] = flags(live.state(OWNER))
    trial["reconcile"] = trial.get("safety") or stop.reconcile(live, [OWNER])
    trial["recovery"] = recover(live, OWNER)
    return trial


def velocity_trial(live, limit, rpm, hold_ms):
    spec = LIMITS[limit]
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    snap = live.checked("velocity-read", address=OWNER)
    if "velocity" not in live.snapshots:
        v = snap["velocity_parameters"]
        live.snapshots["velocity"] = {"operation_id": snap["operation_id"], "address": OWNER,
                                      "words": [v["raw_speed"], v["raw_acceleration_time"], v["raw_deceleration_time"]]}
    live.checked("velocity-qualification-install",
                 host_args=("rpm", "ramp", "inputs") + (("negative",) if rpm < 0 else ()))
    pre = stop.pre_start(live)
    handle = live.console.begin("velocity", velocity_args=(str(rpm), "rpm", "native", "1000", "configured", "normal"),
                                address=OWNER, timeout_s=max(live.timeout, 10))
    trial = {"limit": limit, "function": spec["function"], "input_bit": spec["bit"], "steps": None, "rpm": rpm,
             "toward": (rpm > 0) == (limit == "positive"), "hold_ms": hold_ms, "y0": live.case.get("y0"),
             "over_limit": live.over_limit, "start_position": stop.position(pre["state"]) if pre["state"] else None}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    accepted = time.monotonic()
    first = len(live.samples)
    stop.wait_running(live, accepted, 0.0)
    trial["operation_id"] = handle.operation_id
    try:
        on, off = pulse(live, limit, hold_ms, [named(OWNER, limit, handle.operation_id, pre)])
        trial["on_us"] = on
        stop.observe(live, [OWNER], hold_ms / 1000.0 + POST_RELEASE_S)
        trial["off_us"] = off_us(live, limit, on, hold_ms)
    except records.InterlockRefused as error:
        trial["interlock_refused"] = error.entry.get("reason")
        stop.observe(live, [OWNER], 2.0)
    trial = finish_trial(live, trial, handle, first, [OWNER])
    trial["competing_end_us"] = (trial.get("terminal") or {}).get("stop_due_us")
    if trial.get("on_us"):
        trial["analysis"] = stop.classify(trial)
    return trial


def group_trial(live, limit, steps, hold_ms, delay_s):
    """L9: one staged group move of drives 1, 2, 4 and one ON of the limit line,
    every member named with its pre-start io read (prompt 07 G1 shape)."""
    spec = LIMITS[limit]
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
        positions[address] = stop.position(s) if s else None
    handle = live.console.begin("group-move", host_args=("relative", str(steps), "steps", "native", "60", "configured",
                                                         "start", "staged", "on-failure", "leave"),
                                timeout_s=max(live.timeout, 40))
    trial = {"limit": limit, "function": spec["function"], "input_bit": spec["bit"], "steps": steps,
             "toward": (steps > 0) == (limit == "positive"), "hold_ms": hold_ms, "start_positions": positions}
    if not handle.accepted:
        trial["refused"] = live.console.wait(handle)
        return trial
    group_id = handle.operation_id
    accepted = time.monotonic()
    first = len(live.samples)
    index = 0
    while time.monotonic() - accepted < delay_s:
        live.state(members[index % len(members)])
        index += 1
    read_ids = [pres[a]["io_read"]["operation_id"] for a in members]
    named_members = []
    for index, address in enumerate(members):
        own = group_id + 1 + index
        between = [{"command": "io", "operation_id": i, "read": True} for i in read_ids if i > pres[address]["io_read"]["operation_id"]]
        between += [{"command": "read-state", "operation_id": i} for i in range(read_ids[-1] + 1, group_id)]
        between += [{"command": "group-move", "operation_id": i, "motion": True} for i in range(group_id, own)]
        named_members.append(named(address, limit, own, {"io_read": pres[address]["io_read"], "between": between},
                                   console_id=group_id, console_address=handle.address))
    try:
        on, off = pulse(live, limit, hold_ms, named_members)
        trial["on_us"] = on
        stop.observe(live, members, hold_ms / 1000.0 + POST_RELEASE_S + 1.0)
        trial["off_us"] = off_us(live, limit, on, hold_ms)
    except records.InterlockRefused as error:
        trial["interlock_refused"] = error.entry.get("reason")
        stop.observe(live, members, 5.0)
    moving_members = [a for a in members if any(s["address"] == a and stop.moving(s) for s in live.samples[-6:])]
    if moving_members:
        trial["continued_motion"] = moving_members
        trial["safety"] = live.call("group-stop", host_args=("fast",)).get("outcome")
    trial["terminal"] = t = live.console.wait(handle, release=True)
    trial["samples"] = live.samples[first:]
    trial["members"] = [{k: m.get(k) for k in ("address", "operation_id", "start", "state", "outcome", "status", "detail",
                                                "completion", "start_tx_end_us", "running_observed")}
                        for m in (t.get("members") or []) if isinstance(m, dict)]
    trial["reconcile"] = stop.reconcile(live, members)
    if trial.get("on_us"):
        trial["per_member"] = {}
        for address in members:
            mine = [s for s in trial["samples"] if s["address"] == address]
            member = next((m for m in trial["members"] if m.get("address") == address), {})
            trial["per_member"][address] = dict(stop.classify({
                "on_us": trial["on_us"], "off_us": trial["off_us"], "samples": mine, "steps": steps,
                "start_position": positions[address], "start_us": member.get("start_tx_end_us"), "terminal": member,
                "edges": [], "y0": None, "function": spec["function"], "input_bit": spec["bit"]}),
                phases=phase_flags(mine, trial["on_us"], trial["off_us"]))
        trial["recovery"] = {a: recover(live, a) for a in members}
    return trial


def stationary_trial(live, limit, hold_ms):
    """L10: every drive stopped; the limit line held; status, alarm and speed per drive; no motion."""
    members = list(records.FIXTURE["input_drives"])
    before = {a: flags(live.state(a)) for a in members}
    first = len(live.samples)
    trial = {"limit": limit, "hold_ms": hold_ms, "before": before}
    try:
        on, off = pulse(live, limit, hold_ms, [])
        trial["on_us"] = on
        stop.observe(live, members, hold_ms / 1000.0 + 1.0)
        trial["off_us"] = off_us(live, limit, on, hold_ms)
    except records.InterlockRefused as error:
        trial["interlock_refused"] = error.entry.get("reason")
    trial["samples"] = live.samples[first:]
    bit = LIMITS[limit]["bit"]
    trial["per_drive"] = {}
    for a in members:
        mine = [s for s in trial["samples"] if s["address"] == a]
        trial["per_drive"][a] = {"input_seen": any(s["inputs"] & bit for s in mine),
                                 "moved": any(stop.moving(s) for s in mine),
                                 "phases": phase_flags(mine, trial.get("on_us") or 0, trial.get("off_us") or 0)
                                 if trial.get("on_us") else None}
    trial["recovery"] = {a: recover(live, a) for a in members}
    return trial


def held_trial(live, limit, hold_ms):
    """L5a: the limit line held on stopped drives; each motion kind refused
    before TX, counted by the debug decoded traffic records (prompt 07 S5)."""
    trial = {"limit": limit, "hold_ms": hold_ms}
    stop.motion_ready(live)
    live.checked("velocity-read", address=OWNER)
    live.checked("velocity-qualification-install", host_args=("rpm", "ramp", "inputs", "negative"))
    stop.pre_start(live)
    live.checked("debug", host_args=("decoded",))
    try:
        try:
            on, _ = pulse(live, limit, hold_ms, [])
        except records.InterlockRefused as error:
            trial["interlock_refused"] = error.entry.get("reason")
            return trial
        trial["on_us"] = on
        trial["state"] = flags(live.state(OWNER))
        word = str(live.snapshots["profile"]["current"][0])
        live.call("motion-profile-start-speed-relation", host_args=(word, word))
        for label, name, kwargs in (
                ("move +500", "move-relative", {"move_args": ("500", "steps", "native", "60", "configured")}),
                ("move -500", "move-relative", {"move_args": ("-500", "steps", "native", "60", "configured")}),
                ("velocity +60", "velocity", {"velocity_args": ("60", "rpm", "native", "1000", "configured", "normal")}),
                ("velocity -60", "velocity", {"velocity_args": ("-60", "rpm", "native", "1000", "configured", "normal")})):
            stop.refused_before_tx(live, label, name, address=OWNER, **kwargs)
        view = live.view()
        o = next(x for x in view["outputs"] if x["name"] == LIMITS[limit]["output"])
        trial["still_held"] = bool(o["active"])
        remaining = (on + hold_ms * 1000 - view["now_us"]) / 1e6
        stop.observe(live, [OWNER], max(0.0, remaining) + 1.0)
    finally:
        live.checked("debug", host_args=("off",))
        trial["refusals"] = list(live.refusals)
        live.refusals.clear()
    trial["state_after"] = flags(live.state(OWNER))
    return trial


def recovery_trial(live, limit, steps, hold_ms, delay_s):
    """L6: a toward stop (L1 shape), then after the release a 1000-step move toward and one away."""
    trial = move_trial(live, limit, steps, hold_ms, delay_s)
    sign = 1 if steps > 0 else -1
    trial["follow"] = []
    for label, delta in (("toward", 1000 * sign), ("away", -1000 * sign)):
        follow = move_trial(live, limit, delta, hold_ms, delay_s, stimulus=False)
        trial["follow"].append({"label": label, "steps": delta, "refused": follow.get("refused"),
                                "terminal": follow.get("terminal_summary"),
                                "travel": None if not follow.get("samples") or follow.get("start_position") is None else
                                (next((stop.position(s) for s in reversed(follow["samples"]) if stop.position(s) is not None),
                                      None) or 0) - follow["start_position"],
                                "recovery": follow.get("recovery")})
    return trial


def set_over_limit(live, value):
    """0x0017 through `driver set over-limit` with CONFIGURE records, read back."""
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    live.state()
    config = live.checked("read-config", address=OWNER)["config"]["raw"]
    if config["over_limit_stop"] == value:
        return config["over_limit_stop"]
    stop.write_setting(live, OWNER, "drive.over-limit", value)
    return live.checked("read-config", address=OWNER)["config"]["raw"]["over_limit_stop"]


def run_cases(live, args, cases):
    detail = {"cases": {}, "polls_only": args.polls_only}
    live.partial = detail["cases"]  # Kept by the runner when a case stops early.
    if not args.polls_only:
        stop.write_setting(live, OWNER, "io.y0", 2)
    # The interlock's session-level input agreement (io read against read
    # config) cannot run on a moving drive (axis_conflict, prompt 07 deviation 1).
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    soft = [c for c in cases if CASES[c]["kind"] == "soft"]
    if soft:
        detail["soft"] = live.partial_soft = {}
        detail["soft"].update(run_soft(live, soft))
    if "S4" in cases:
        over = args.over_limit if args.over_limit is not None else 0
        detail["soft_velocity"] = run_soft_velocity(live, over)
    for cid in cases:
        if CASES[cid]["kind"] in ("soft", "soft-velocity"):
            continue
        live.select(cid)
        case = live.case
        over = args.over_limit if args.over_limit is not None else case["over_limit"]
        live.over_limit = set_over_limit(live, over) if case["kind"] not in ("group", "stationary") else over
        if live.over_limit != over:
            raise probe.BenchError("0x0017 read back %s, expected %s" % (live.over_limit, over))
        live.view()
        out = {"over_limit": live.over_limit, "trials": []}
        detail["cases"][cid] = out
        runs = args.repetitions or case["runs"]
        try:
            for index in range(runs):
                for limit, amount in case["trials"]:
                    if case["kind"] == "move":
                        trial = move_trial(live, limit, amount, case["hold_ms"], case["delay_s"])
                    elif case["kind"] == "recovery":
                        trial = recovery_trial(live, limit, amount, case["hold_ms"], case["delay_s"])
                    elif case["kind"] == "velocity":
                        trial = velocity_trial(live, limit, amount, case["hold_ms"])
                    elif case["kind"] == "held":
                        trial = held_trial(live, limit, case["hold_ms"])
                    elif case["kind"] == "group":
                        live.checked("group-set", host_args=tuple(str(a) for a in records.FIXTURE["input_drives"]),
                                     timeout_s=max(live.timeout, 10))
                        try:
                            trial = group_trial(live, limit, amount, case["hold_ms"], case["delay_s"])
                        finally:
                            live.call("group-clear")
                    else:
                        trial = stationary_trial(live, limit, case["hold_ms"])
                    out["trials"].append(dict(trial, index=index + 1))
                    if trial.get("continued_motion"):
                        raise probe.BenchError("motion continued after the stimulus; prepared stop sent; case ended")
        except probe.BenchError as error:
            out["error"] = str(error)
            raise
        out["summary"] = summarize(out["trials"])
    return detail


# --------------------------------------------------------------------------
# Analysis and verdicts (pure)

def summarize(trials: list) -> dict:
    rows = []
    for t in trials:
        a = t.get("analysis") or {}
        w = a.get("witnesses") or {}
        rows.append({"index": t.get("index"), "limit": t.get("limit"), "toward": t.get("toward"),
                     "steps": t.get("steps"), "rpm": t.get("rpm"), "class": a.get("class"),
                     "travel": w.get("travel"), "y0_edge_ms": None if w.get("y0_edge_us") is None else w["y0_edge_us"] / 1000.0,
                     "stop_bracket_ms": None if not w.get("stop_bracket_us") else [v / 1000.0 for v in w["stop_bracket_us"]],
                     "phases": t.get("phases"), "terminal": t.get("terminal_summary"),
                     "recovery": (t.get("recovery") or {}).get("needed") if isinstance(t.get("recovery"), dict) and
                     "needed" in t.get("recovery", {}) else None})
    return {"trials": len(trials), "rows": rows}


def move_verdict(trials: list) -> tuple[str, str | None]:
    """Toward trials input-stopped (stopped short after the assertion, not
    complete, no further start); away trials recorded (either class)."""
    if not trials:
        return "NOT RUN", "no trial"
    if any(t.get("continued_motion") for t in trials):
        return "FAIL", "motion continued after the stimulus; prepared stop sent"
    if any(t.get("refused") for t in trials):
        return "FAIL", "a trial was refused before transmission"
    if any(t.get("interlock_refused") for t in trials):
        return "indeterminate", "an interlock refused the stimulus"
    bad = [t.get("index") for t in trials if t.get("toward") and (t.get("analysis") or {}).get("class") != "input-stopped"]
    if bad:
        return "indeterminate", "toward trials %s not classified input-stopped" % bad
    return "PASS", None


def group_verdict(trials: list) -> tuple[str, str | None]:
    if not trials:
        return "NOT RUN", "no trial"
    for t in trials:
        if t.get("refused") or t.get("interlock_refused"):
            return "indeterminate", "trial %s: refused or no stimulus" % t.get("index")
        if t.get("continued_motion"):
            return "FAIL", "members %s kept moving" % t["continued_motion"]
        for address, member in (t.get("per_member") or {}).items():
            if member["class"] != "input-stopped":
                return "indeterminate", "member %s: %s" % (address, ", ".join(member["missing"]) or member["class"])
    return "PASS", None


def stationary_verdict(trials: list) -> tuple[str, str | None]:
    if not trials:
        return "NOT RUN", "no trial"
    for t in trials:
        if t.get("interlock_refused"):
            return "indeterminate", "interlock refused %s" % t["limit"]
        for address, row in t["per_drive"].items():
            if row["moved"]:
                return "FAIL", "drive %s moved with the %s limit held" % (address, t["limit"])
            if not row["input_seen"]:
                return "indeterminate", "drive %s did not report the %s input" % (address, t["limit"])
    return "PASS", None


def case_verdicts(run, cid, detail, error):
    case = CASES[cid]
    if case["kind"] in ("soft", "soft-velocity"):
        return soft_verdicts(run, cid, detail, error)
    out = (detail.get("cases") or {}).get(cid) or {}
    trials = out.get("trials", [])
    evidence = ["limits.json"]
    if case["kind"] == "held":
        bad = [(t["limit"], r["label"], r.get("reason"), r.get("tx_records")) for t in trials for r in t.get("refusals", [])
               if r.get("ok") or r.get("reason") != "limit_input_active" or r.get("tx_records") != 0]
        held = all(t.get("still_held") and len(t.get("refusals", [])) == 4 for t in trials)
        verdict = "NOT RUN" if not trials else "PASS" if not bad and held else "FAIL"
        for level in case["levels"]:
            run.verdict(cid, level, verdict, "; ".join(map(str, bad)) or None, evidence)
        return
    if case["kind"] in ("move", "recovery", "velocity"):
        verdict, reason = move_verdict(trials)
    elif case["kind"] == "group":
        verdict, reason = group_verdict(trials)
    else:
        verdict, reason = stationary_verdict(trials)
    if error and verdict == "PASS":
        verdict, reason = "indeterminate", "stopped early: %s" % error
    if not trials and error:
        verdict, reason = "NOT RUN", "stopped early: %s" % error
    for level in case["levels"]:
        if level == "controller-timestamped" and verdict == "PASS" and not any(
                (t.get("analysis") or {}).get("witnesses", {}).get("y0_edge_us") is not None for t in trials):
            run.verdict(cid, level, "NOT RUN", "no Y0 edge (polls only)", evidence)
            continue
        run.verdict(cid, level, verdict, reason, evidence)


# --------------------------------------------------------------------------
# Soft limits (S1-S4, S6, S7) on drive 4 through the production enable route

SOFT_LIMIT = 500
HOME_FACTS = ("mechanics", "rates", "ramp", "zero-offset", "auxiliary", "inputs", "reference", "soft-limits")
SOFT_CASES = ("S1", "S2", "S3", "S6", "S7")


def soft_state(live, address=OWNER):
    reply = live.call("read-state", address=address)
    s = stop.sample(reply)
    if s:
        live.samples.append(s)
    return flags(s)


def setting(live, key, value):
    """One ordinary DRIVE/home-auxiliary write with the run's CONFIGURE records; read back. Never retried."""
    entry = {"drive": OWNER, "key": key, "index": None, "candidate": value}
    steps, refusal = records.plan_entry(live.baseline, entry, live.args.image_level, live.facts, restore=False)
    if refusal:
        raise probe.BenchError("no ordinary route for %s: %s" % (key, refusal["reason"]))
    step = dict(steps[0], target=value, candidate=value)
    command, kwargs, text = records.write_call({"key": key, "index": None}, value, OWNER)
    step["write"] = {"command": command, "kwargs": kwargs, "text": text}
    outcome = records.apply_write(live.console, step, timeout_s=live.timeout)
    ending, _ = records.readback(live.console, step, timeout_s=live.timeout)
    live.run.field(entry, baseline=step["baseline"])
    reply = outcome["reply"]
    row = {"command": text, "written": outcome["written"], "uncertain": outcome["uncertain"], "readback": ending,
           "result": outcome["result"], "operation_id": reply.get("operation_id"), "reason": reply.get("reason")}
    live.events.append(row)
    if outcome["uncertain"] or not outcome["written"] or ending != value:
        live.run.field(entry, uncertain=outcome["uncertain"], reason="%s %s not confirmed; not replayed" % (key, value))
        raise probe.BenchError("%s %s: written %s, uncertain %s, readback %s" %
                               (key, value, outcome["written"], outcome["uncertain"], ending))
    return row


def axis_limits(live):
    """Host axis soft limits from `axis config` (host only, no traffic)."""
    reply = live.checked("axis", host_args=("config",))

    def find(item, key):
        if isinstance(item, dict):
            if key in item:
                return item[key]
            for v in item.values():
                found = find(v, key)
                if found is not None:
                    return found
        return None
    return {"known": find(reply, "soft_limits_known"), "limits": find(reply, "soft_limits"),
            "generation": find(reply, "configuration_generation")}


def homed_low(live):
    """A HOMED-low start makes method 35's completion observable (wired bench 06/10):
    release then enable, the 1 s settle, a fresh read."""
    s = soft_state(live)
    if s and s["homed"]:
        live.call("motor-release", address=OWNER)
        live.call("enable", address=OWNER)
        time.sleep(1.1)
        s = soft_state(live)
    return s


def home_35(live):
    """home 35 60 30 100 zero with the reference and soft-limits facts; the terminal."""
    homed_low(live)
    live.checked("read-config", address=OWNER)
    live.checked("read-state", address=OWNER)
    live.checked("home-read", address=OWNER)
    live.checked("home-qualification-install", host_args=("35", "60", "30", "100", *HOME_FACTS))
    handle = live.console.begin("home", home_args=("35", "60", "30", "100", "zero"), address=OWNER,
                                timeout_s=max(live.timeout, 40))
    terminal = live.console.wait(handle, release=True)
    row = {"command": "home 35 60 30 100 zero", "admitted": handle.accepted,
           "terminal": {k: terminal.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                     "uncertain", "result", "reason")}}
    if terminal.get("uncertain"):
        row["reconcile"] = live.call("stop", stop_policy="normal", address=OWNER).get("outcome")
    row["state"] = soft_state(live)
    live.events.append(row)
    return row


def bind(live):
    """S1: home, both pairs read, the enable route, its readback, then the host limits."""
    out = {"axis_before": axis_limits(live)}
    out["home"] = home_35(live)
    if not (out["home"]["terminal"].get("ok") and out["home"]["state"] and out["home"]["state"]["homed"]):
        raise probe.BenchError("home 35 did not complete with HOMED")
    live.checked("read-config", address=OWNER)
    driver = live.checked("driver", driver_args=("read",), address=OWNER)["observation"]
    out["pairs"] = {k: driver.get(k) for k in ("positive_words", "negative_words", "limits_decoded",
                                                "positive_limit", "negative_limit")}
    soft_state(live)
    out["axis_after_home"] = axis_limits(live)
    out["enable"] = setting(live, "drive.soft-limit", 1)
    live.checked("read-config", address=OWNER)
    out["state"] = soft_state(live)
    out["axis_after_enable"] = axis_limits(live)
    return out


def unbind(live):
    """Disable after a stop: the flag exit first when a soft-limit flag is set (wired bench 10)."""
    out = {"exit": []}
    s = soft_state(live)
    if s and (s["positive_flag"] or s["negative_flag"] or s["released"] or s["alarm"]):
        for name in ("stop", "alarm-clear", "motor-release", "enable"):
            reply = live.call(name, address=OWNER, **({"stop_policy": "fast"} if name == "stop" else {}))
            if name == "enable":
                time.sleep(1.1)
            out["exit"].append({"step": name, "outcome": reply.get("outcome"), "state": soft_state(live)})
    live.checked("read-config", address=OWNER)
    config = live.checked("read-config", address=OWNER)["config"]["raw"]
    if config["soft_limit_enable"] != 0:
        out["disable"] = setting(live, "drive.soft-limit", 0)
    out["state"] = soft_state(live)
    out["axis"] = axis_limits(live)
    return out


def absolute_move(live, target):
    """One absolute move; the terminal, the final position, or the refusal (no operation)."""
    stop.motion_ready(live)
    stop.pre_start(live)
    debug_before = live.checked("debug", host_args=()).get("observed")
    handle = live.console.begin("move-absolute", move_args=(str(target), "steps", "native", "60", "configured"),
                                address=OWNER, timeout_s=max(live.timeout, 40))
    row = {"target": target, "admitted": handle.accepted}
    if not handle.accepted:
        refused = live.console.wait(handle)
        row["refused"] = {k: refused.get(k) for k in ("ok", "result", "reason", "operation_id", "purpose")}
        row["debug_observed"] = [debug_before, live.checked("debug", host_args=()).get("observed")]
    else:
        t = live.console.wait(handle, release=True)
        row["terminal"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion", "uncertain")}
        if t.get("uncertain"):
            row["reconcile"] = stop.reconcile(live, [OWNER])
    row["state"] = soft_state(live)
    live.events.append(row)
    return row


def soft_velocity(live, rpm):
    live.checked("read-config", address=OWNER)
    snap = live.checked("velocity-read", address=OWNER)
    if "velocity" not in live.snapshots:
        v = snap["velocity_parameters"]
        live.snapshots["velocity"] = {"operation_id": snap["operation_id"], "address": OWNER,
                                      "words": [v["raw_speed"], v["raw_acceleration_time"], v["raw_deceleration_time"]]}
    live.checked("velocity-qualification-install",
                 host_args=("rpm", "ramp", "inputs") + (("negative",) if rpm < 0 else ()))
    start = stop.pre_start(live)["state"]
    first = len(live.samples)
    handle = live.console.begin("velocity", velocity_args=(str(rpm), "rpm", "native", "1000", "configured", "normal"),
                                address=OWNER, timeout_s=max(live.timeout, 10))
    row = {"rpm": rpm, "admitted": handle.accepted, "start": flags(start)}
    if not handle.accepted:
        row["refused"] = live.console.wait(handle)
        return row
    began = time.monotonic()
    while handle.terminal is None and time.monotonic() - began < 3.0:
        soft_state(live)
    t = live.console.wait(handle, release=True)
    row["terminal"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion", "uncertain",
                                             "needs_stop", "stop_completion", "stop_outcome", "limit_inputs")}
    row["samples"] = [flags(s) for s in live.samples[first:]]
    row["end"] = soft_state(live)
    if row["end"] and (row["end"]["running"] or row["end"]["speed"]):
        row["continued_motion"] = True
        row["safety"] = stop.reconcile(live, [OWNER])
    live.events.append({k: v for k, v in row.items() if k != "samples"})
    return row


def soft_snapshots(live):
    """First home and velocity reads of this binding (pinned in the firmware's snapshot ring)."""
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("read-config", address=OWNER)
    h = live.checked("home-read", address=OWNER)
    hp = h["homing_parameters"]
    live.snapshots["home"] = {"operation_id": h["operation_id"], "address": OWNER,
                              "words": [hp["raw_method"], hp["raw_search_speed"], hp["raw_return_speed"],
                                        hp["raw_ramp_time"], *hp["raw_offset_words"]],
                              "auxiliary": hp["raw_auxiliary"]}
    v = live.checked("velocity-read", address=OWNER)
    vp = v["velocity_parameters"]
    live.snapshots["velocity"] = {"operation_id": v["operation_id"], "address": OWNER,
                                  "words": [vp["raw_speed"], vp["raw_acceleration_time"], vp["raw_deceleration_time"]]}


def soft_setup(live):
    """Auxiliary 7, then the pairs +500/-500 while 0x0018 reads 0 (D10 windows)."""
    out = {}
    if live.snapshots["home"]["auxiliary"] != 7:
        out["auxiliary"] = setting(live, "homing.auxiliary", 7)
    out["positive"] = setting(live, "drive.positive-limit", [0, SOFT_LIMIT])
    out["negative"] = setting(live, "drive.negative-limit", [0xFFFF, (0x10000 - SOFT_LIMIT) & 0xFFFF])
    return out


def soft_restore(live):
    """0x0018 0 first, then pairs [0, 0], auxiliary 0; homing, velocity and profile
    words are left to the run's restoration plan (snapshot and stage routes)."""
    out = {"unbind": unbind(live)}
    for key in ("drive.positive-limit", "drive.negative-limit"):
        out[key] = setting(live, key, [0, 0])
    live.checked("read-config", address=OWNER)
    if live.checked("home-read", address=OWNER)["homing_parameters"]["raw_auxiliary"] != 0:
        out["auxiliary"] = setting(live, "homing.auxiliary", 0)
    return out


def run_soft(live, cases):
    """S1, S2, S3, S6, S7 in one flow (S4: run_soft_velocity)."""
    out = {"cases": {}}
    soft_snapshots(live)
    out["setup"] = soft_setup(live)
    try:
        s1 = out["cases"]["S1"] = bind(live)
        if "S2" in cases:
            out["cases"]["S2"] = [absolute_move(live, 400), absolute_move(live, -400)]
        if "S3" in cases:
            out["cases"]["S3"] = absolute_move(live, 700)
        if "S6" in cases:
            s6 = out["cases"]["S6"] = {}
            live.checked("read-config", address=OWNER)
            soft_state(live)
            live.checked("position-clear-qualification-install", host_args=("clear-semantics",))
            clear = live.call("position-clear", address=OWNER)
            s6["clear"] = {"ok": clear.get("ok"), "outcome": clear.get("outcome")}
            s6["state"] = soft_state(live)
            s6["move"] = absolute_move(live, 100)
            s6["recover"] = {"unbind": unbind(live), "bind": bind(live)}
        if "S7" in cases:
            s7 = out["cases"]["S7"] = {"unbind": unbind(live)}
            s7["move"] = absolute_move(live, 700)
        out["s1_ok"] = bool(s1)
    finally:
        out["restore"] = soft_restore(live)
    return out


def run_soft_velocity(live, over_limit):
    """S4 at one 0x0017 value: bind, +60 rpm 1000 ms, exit and disable; bind, -60 rpm, exit and disable."""
    out = {"over_limit": over_limit, "runs": []}
    soft_snapshots(live)
    out["setup"] = soft_setup(live)
    try:
        if over_limit != (live.checked("read-config", address=OWNER)["config"]["raw"]["over_limit_stop"]):
            out["over_limit_write"] = setting(live, "drive.over-limit", over_limit)
        for rpm in (60, -60):
            run = {"bind": bind(live)}
            run["velocity"] = soft_velocity(live, rpm)
            run["unbind"] = unbind(live)
            out["runs"].append(run)
            if run["velocity"].get("continued_motion"):
                raise probe.BenchError("velocity continued past the soft limit; prepared stop sent")
    finally:
        if live.checked("read-config", address=OWNER)["config"]["raw"]["over_limit_stop"] != 0:
            try:
                unbind(live)
            finally:
                out["over_limit_restore"] = setting(live, "drive.over-limit", 0)
        out["restore"] = soft_restore(live)
    return out


def near(move, tolerance=2):
    """Final position within a few increments of the target (moves report 99 for 100, Position.cpp)."""
    position = (move.get("state") or {}).get("position")
    return position is not None and abs(position - move["target"]) <= tolerance


def soft_verdicts(run, cid, detail, error):
    """Verdicts of the soft-limit cases from the recorded flow."""
    soft = detail.get("soft") or {}
    cases = soft.get("cases") or {}
    levels = CASES[cid]["levels"]
    evidence = ["limits.json"]

    def give(verdict, reason=None):
        for level in levels:
            run.verdict(cid, level, verdict, reason, evidence)
    if cid == "S4":
        runs = (detail.get("soft_velocity") or {}).get("runs") or []
        if len(runs) < 2:
            return give("NOT RUN" if not runs else "indeterminate", "stopped early: %s" % error if error else "incomplete")
        problems = []
        for r in runs:
            v = r["velocity"]
            t = v.get("terminal") or {}
            end = v.get("end") or {}
            bit = "positive_flag" if v["rpm"] > 0 else "negative_flag"
            if not v.get("admitted") or v.get("continued_motion"):
                problems.append("rpm %s not admitted or kept moving" % v["rpm"])
            elif not end.get(bit) or t.get("detail") != 29 or t.get("ok"):
                problems.append("rpm %s: flag %s, detail %s" % (v["rpm"], end.get(bit), t.get("detail")))
        return give("PASS" if not problems else "FAIL", "; ".join(problems) or None)
    c = cases.get(cid)
    if c is None:
        return give("NOT RUN", "stopped early: %s" % error if error else "case not in this run")
    if cid in ("S1",):
        before, after = c["axis_before"], c["axis_after_enable"]
        ok = (not before.get("known") and not c["axis_after_home"].get("known") and after.get("known") and
              after.get("limits") == [-SOFT_LIMIT, SOFT_LIMIT] and c["enable"]["readback"] == 1 and
              c["state"] and not c["state"]["positive_flag"] and not c["state"]["negative_flag"])
        return give("PASS" if ok else "FAIL", None if ok else "binding or readback not as required")
    if cid == "S2":
        ok = all(m.get("admitted") and (m.get("terminal") or {}).get("ok") and near(m) for m in c)
        return give("PASS" if ok else "FAIL", None if ok else "a move inside the limits did not complete")
    if cid == "S3":
        r = c.get("refused") or {}
        d = c.get("debug_observed") or [None, None]
        ok = not c.get("admitted") and r.get("reason") == "soft_limit_endpoint" and not r.get("operation_id") and d[0] == d[1]
        return give("PASS" if ok else "FAIL", None if ok else "not refused before TX by the soft-limit endpoint: %s" % r)
    if cid == "S6":
        r = (c.get("move") or {}).get("refused") or {}
        d = (c.get("move") or {}).get("debug_observed") or [None, None]
        rebound = (c.get("recover") or {}).get("bind") or {}
        ok = r.get("reason") == "soft_limit_state_unknown" and d[0] == d[1] and \
            (rebound.get("axis_after_enable") or {}).get("known")
        return give("PASS" if ok else "FAIL", None if ok else "position clear did not make the state unknown or recovery failed")
    if cid == "S7":
        m = c.get("move") or {}
        ok = m.get("admitted") and (m.get("terminal") or {}).get("ok") and near(m) and \
            not (c["unbind"].get("axis") or {}).get("known")
        return give("PASS" if ok else "FAIL", None if ok else "the move beyond +500 did not complete after the disable")
