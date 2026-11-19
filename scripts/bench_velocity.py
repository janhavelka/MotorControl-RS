"""Wired bench 14: velocity limits, the JOG setter, speed/JOG inputs, PV records
and the phase C qualification cells.

Every serial run above the legacy envelope (+-60 rpm, 1..1000 ms) is admitted
only by its own ``velocity limits qualify`` cell (D6); the cell is consumed by
that run. Input-started runs (functions 8-10, 13) use the stored words: the JOG
words through ``velocity jog set``, PV records through ``segment speed N set``.
Each case states one purpose per terminal; ``plan_problem`` refuses a plan that
gives a terminal two (prompt 14 Implement 1). A dead-man stop is X3 = 5 with
input polarity 1, held inactive by ``mosfet2`` on a renewed hold; it is set up
with ``mosfet2`` on, so the stop never asserts while a function is changed.

Velocity speed is the feedback position difference over the controller times
of the feedback reads (window midpoints), compared with |rpm| x subdivision /
60. Every trial loop sends the prepared ``stop fast`` when it raises or the
console fails (wired bench 12 deviation 1). Pure analysis functions are
unit-tested (``test/bench_velocity_test.py``).
"""
import time

import bench_input_starts as starts
import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stops
import bench_switch

DRIVES = list(records.FIXTURE["input_drives"])
SUBDIVISION = 1000
LEGACY_RPM = 60
DEADMAN_OUTPUT, DEADMAN_TERMINAL = "mosfet2", "x3"
DEADMAN_HOLD_MS = 1000     # Renewed by every trial-loop pass; when renewals stop the stop asserts within 1 s.
TRIGGER = {"x0": "mosfet1", "x3": "mosfet2"}
PIN_OUTPUTS = ("relay2", "relay1", "mosfet2")   # PIN0, PIN1, PIN2 (D2: relays swapped against the plan).
SETTLE_S = 0.06
SLOPE_TOLERANCE_B, SLOPE_TOLERANCE_C = 0.05, 0.10
CREEP_AFTER_S = 0.5        # Phase C3: RUNNING or moving this long after the ramp is a creep.
STANDSTILL_SPEED = 6       # r/min noise read at rest (wired bench 12).
ALARM_BIT = 8              # 0x0007 bit 3.
DEADMAN_SETUP_HOLD_MS = 60000  # While no run is armed; the run phase renews with DEADMAN_HOLD_MS.

# One purpose per terminal per layout: (purpose, function, polarity).
LAYOUTS = {
    "baseline": {},
    "deadman": {"x3": ("deadman", 5, 1)},
    "jog+deadman": {"x0": ("jog+", 9, 0), "x3": ("deadman", 5, 1)},
    "speed+deadman": {"x0": ("speed", 8, 0), "x3": ("deadman", 5, 1)},
    "jog-": {"x3": ("jog-", 10, 0)},
    "jog+": {"x0": ("jog+", 9, 0)},
    "jog+stop": {"x0": ("jog+", 9, 0), "x3": ("stop", 5, 0)},
    "jog+inverted": {"x0": ("jog+", 9, 0), "x3": ("inverted", 0, 1)},
    "speed": {"x0": ("speed", 8, 0)},
    "pin3": {"x0": ("pv", 13, 0), "x1": ("pin0", 14, 0), "x2": ("pin1", 15, 0), "x3": ("pin2", 16, 0)},
    "pv2-deadman": {"x0": ("pv", 13, 0), "x1": ("pin0", 14, 0), "x2": ("pin1", 15, 0), "x3": ("deadman", 5, 1)},
}

CASES = {
    "R1": dict(kind="refuse", levels=["sent"], uses=[], layout="baseline",
               assertion="velocity 61 rpm and 1001 ms refuse outside_velocity_limits before any frame"),
    "R2": dict(kind="cell", levels=["drive-reported", "sent"], uses=[], layout="baseline",
               assertion="a 120 rpm 2000 ms cell admits its run once; the same run and 121 rpm refuse after it"),
    "R3": dict(kind="jog-setter", levels=["read back"], uses=[], layout="baseline",
               assertion="velocity jog set 60 reads back [60,R,R]; 120 refuses outside_velocity_limits"),
    "R4": dict(kind="deadman-velocity", levels=["drive-reported"], uses=[("x3", "deadman")], layout="deadman",
               assertion="velocity 30 rpm 1000 ms stops before its timed stop when the dead-man lapses at ~300 ms"),
    "R5": dict(kind="deadman-jog", levels=["drive-reported"], uses=[("x0", "jog+"), ("x3", "deadman")],
               layout="jog+deadman", assertion="JOG+ width 3000 stops before the width ends when the dead-man lapses"),
    "R6": dict(kind="deadman-pv", levels=["drive-reported"], uses=[("x0", "pv"), ("x1", "pin0"), ("x2", "pin1"),
                                                                     ("x3", "deadman")], layout="pv2-deadman",
               assertion="an edge PV run (0x0051 = 1) stops at the dead-man lapse before the prepared stop at 2 s"),
    "V1": dict(kind="ramps", levels=["drive-reported"], uses=[], layout="baseline",
               assertion="+-60 rpm, ramps 100/300/500, 0x0020 0 and 30: ramp time against the word and speed"),
    # J0 (diagnostic, R5 no_start): JOG+ on X0 without the dead-man, to separate the X3 stop assignment.
    "J0": dict(kind="jog", function=9, terminal="x0", levels=["drive-reported"], uses=[("x0", "jog+")], layout="jog+",
               assertion="JOG+ on X0 with X3 = 0: does an input JOG start at all (0x001D +60 and -60)"),
    # J0b (diagnostic): JOG+ with X3 = 5 at polarity 0 (inactive with mosfet2 off), to separate the inversion.
    "J0b": dict(kind="jog", function=9, terminal="x0", levels=["drive-reported"], uses=[("x0", "jog+"), ("x3", "stop")],
                layout="jog+stop", assertion="JOG+ on X0 with X3 = 5 at polarity 0, mosfet2 off"),
    # J0c (diagnostic): JOG+ with X3 = 0 at polarity 1 (the inversion without the stop function).
    "J0c": dict(kind="jog", function=9, terminal="x0", levels=["drive-reported"], uses=[("x0", "jog+"), ("x3", "inverted")],
                layout="jog+inverted", assertion="JOG+ on X0 with X3 = 0 at polarity 1"),
    "J1": dict(kind="jog", function=9, terminal="x0", levels=["drive-reported"], uses=[("x0", "jog+"), ("x3", "deadman")],
               layout="jog+deadman", assertion="JOG+ with 0x001D +60 and -60: positive both times, ends with the level"),
    # J3n: the speed input without a dead-man (the jog+deadman layout did not start JOG+, R5).
    "J3n": dict(kind="jog", function=8, terminal="x0", levels=["drive-reported"], uses=[("x0", "speed")], layout="speed",
                assertion="speed input with 0x001D +60 and -60 without a dead-man (D11): direction follows the sign"),
    "J2": dict(kind="jog", function=10, terminal="x3", levels=["drive-reported"], uses=[("x3", "jog-")], layout="jog-",
               assertion="JOG- on mosfet2 with 0x001D +60 and -60: negative both times (no dead-man, D11)"),
    "J3": dict(kind="jog", function=8, terminal="x0", levels=["drive-reported"], uses=[("x0", "speed"), ("x3", "deadman")],
               layout="speed+deadman", assertion="speed input with 0x001D +60 and -60: direction follows the sign"),
    "J4": dict(kind="not-run", levels=["drive-reported"], uses=[("x0", "jog+"), ("x3", "jog-")], layout="baseline",
               reason="a second bench ON during a JOG run needs an interlock row for a running input-started member",
               assertion="JOG+ with JOG- and the X1 limit pressed during the run"),
    "PV1": dict(kind="pv-table", levels=["drive-reported"], uses=[("x0", "pv"), ("x1", "pin0"), ("x2", "pin1"), ("x3", "pin2")],
                layout="pin3", assertion="records 1-8 at 12..54 rpm, level mode, width 1000: each its own slope within 5 %"),
    "PV2": dict(kind="not-run", levels=["drive-reported"], uses=[], layout="baseline",
                reason="a PIN change during a held run needs an interlock row for a running member's selector",
                assertion="PIN change during a held run"),
    "PV3": dict(kind="pv-edge", levels=["drive-reported"], uses=[("x0", "pv"), ("x1", "pin0"), ("x2", "pin1"), ("x3", "deadman")],
                layout="pv2-deadman", assertion="edge mode: width 50 then the prepared stop after 1 s; width 3000; what ends a run"),
    "PV4": dict(kind="not-run", levels=["drive-reported"], uses=[], layout="baseline",
                reason="the negative PV experiment needs an exact-target write route for 0xFFE2 (none since prompt 13); "
                       "negative PV stays unsupported", assertion="record 2 at -30 reverses"),
    "PV5": dict(kind="not-run", levels=["drive-reported"], uses=[], layout="baseline",
                reason="the second ON (mosfet2, X3 = 12) during an edge PV run needs an interlock row for a running member",
                assertion="PV edge, then a PT trigger switches to fixed length"),
    "PV6": dict(kind="not-run", levels=["drive-reported"], uses=[], layout="baseline",
                reason="the PV arm is delivered for the selected axis only (no group PV arm)",
                assertion="one shared level run on drives 1, 2, 4"),
    "C1": dict(kind="cells", levels=["drive-reported"], uses=[], layout="baseline",
               assertion="120..3000 rpm x ramps 100, 500 x signs, subdivision 1000, 2 s, 3 trials; abort rules per row"),
    "C2": dict(kind="duration", levels=["drive-reported"], uses=[], layout="baseline",
               assertion="300 rpm for 10 s, 2 trials"),
    "C3": dict(kind="creep", levels=["drive-reported"], uses=[], layout="baseline",
               assertion="subdivision 6000, 500-1000 rpm, ramp 500, stop normal at 0.9 of the ramp and at constant speed"),
    "C4": dict(kind="stop-budget", levels=["drive-reported"], uses=[], layout="baseline",
               assertion="highest passing C1 speed with deceleration 2000: the stop inside its budget"),
}
C1_SPEEDS = (120, 300, 600, 1200, 2000, 3000)
C1_RAMPS = (100, 500)
C1_TRIALS = 3
C3_SPEEDS = (500, 600, 700, 1000)
C3_TRIALS = 10
C3_SUBDIVISION = 6000
CELL_SAMPLE_PAUSE_S = 0.1


# --------------------------------------------------------------------------
# Analysis (pure; unit-tested)

def plan_problem(uses) -> str | None:
    """One terminal, one purpose (prompt 14 Implement 1). ``uses`` is a list of
    (terminal, purpose); a terminal named with two purposes refuses the plan."""
    seen = {}
    for terminal, purpose in uses:
        if terminal not in records.TERMINALS:
            return "unknown terminal %s" % terminal
        if seen.get(terminal, purpose) != purpose:
            return "%s has two purposes: %s and %s" % (terminal, seen[terminal], purpose)
        seen[terminal] = purpose
    return None


def layout_uses(layout: str) -> list:
    return [(t, spec[0]) for t, spec in LAYOUTS[layout].items()]


def rate(rpm: int, subdivision: int = SUBDIVISION) -> float:
    """Commanded increments per second."""
    return abs(rpm) * subdivision / 60.0


def feedback_time(sample: dict) -> float:
    """Controller time (s) of one sample's feedback read: the middle of its window."""
    low, high = sample["windows"]["feedback"]
    return (low + high) / 2e6


def points(samples) -> list:
    """(time s, position) of every sample with a position."""
    out = []
    for s in samples:
        p = stops.position(s)
        if p is not None and s.get("windows"):
            out.append((feedback_time(s), p))
    return out


def slope(pts) -> float | None:
    """Least-squares increments per second of (t, position) points."""
    if len(pts) < 2:
        return None
    n = len(pts)
    mt = sum(t for t, _ in pts) / n
    mp = sum(p for _, p in pts) / n
    den = sum((t - mt) ** 2 for t, _ in pts)
    if den <= 0:
        return None
    return sum((t - mt) * (p - mp) for t, p in pts) / den


def steady_slope(samples, start_s: float, end_s: float) -> float | None:
    """Slope of the samples whose feedback time lies within [start_s, end_s]."""
    return slope([(t, p) for t, p in points(samples) if start_s <= t <= end_s])


def slope_verdict(measured: float | None, rpm: int, subdivision: int, tolerance: float) -> tuple:
    if measured is None:
        return "indeterminate", "too few samples at constant speed"
    want = rate(rpm, subdivision) * (1 if rpm > 0 else -1)
    error = abs(measured - want) / abs(want)
    return ("PASS" if error <= tolerance else "FAIL"), "slope %.0f /s against %.0f /s (%.1f %%)" % (measured, want, 100 * error)


def ramp_fit(pts, v: float, v0: float = 0.0, t_low: float | None = None, t_high: float | None = None) -> dict | None:
    """Fit an acceleration ramp: position constant before t0, then speed rising
    linearly from v0 to v over T, then constant. v, v0 in increments/s; grid
    search over t0 (within [t_low, t_high] when given) and T in 0..2.0 s.
    Returns {t0, ramp_s, rms} or None."""
    if len(pts) < 4 or not v:
        return None
    t_first, p_first = pts[0]
    lo = t_first if t_low is None else t_low
    hi = (pts[-1][0] if t_high is None else t_high)
    best = None
    steps = max(1, int((hi - lo) / 0.005))
    for i in range(steps + 1):
        t0 = lo + i * (hi - lo) / steps
        for k in range(0, 201):
            ramp = k * 0.01
            error = 0.0
            for t, p in pts:
                dt = t - t0
                if dt <= 0:
                    model = 0.0
                elif dt < ramp:
                    model = v0 * dt + (v - v0) * dt * dt / (2 * ramp)
                else:
                    model = v0 * ramp + (v - v0) * ramp / 2 + v * (dt - ramp)
                error += (p - p_first - model) ** 2
            if best is None or error < best[0]:
                best = (error, t0, ramp)
    return {"t0": best[1], "ramp_s": best[2], "rms": (best[0] / len(pts)) ** 0.5}


def creeping(samples_after) -> bool:
    """RUNNING or a moving position/speed word in the samples taken after the ramp."""
    pts = points(samples_after)
    moved = len(pts) >= 2 and abs(pts[-1][1] - pts[0][1]) > 2
    return moved or any(stops.running(s) or abs(signed16(s["speed"])) > STANDSTILL_SPEED for s in samples_after)


# VelocityError details of a run another cause ended before its timed stop (wired bench 07).
STOP_INPUT_DETAIL, STOPPED_EARLY_DETAIL = 26, 27


def stopped_before(samples, start_s: float, due_s: float) -> float | None:
    """Feedback time of the first sample after the run moved that reports
    standstill (RUNNING clear, speed word at rest, position unchanged since the
    previous sample) before ``due_s``, or None."""
    moved, previous = False, None
    for s in samples:
        t, p = feedback_time(s), stops.position(s)
        if t <= start_s or p is None:
            continue
        if t >= due_s:
            break
        rest = not stops.running(s) and abs(signed16(s["speed"])) <= STANDSTILL_SPEED
        if not rest:
            moved = True
        elif moved and previous == p:
            return t
        previous = p
    return None


def signed16(word: int) -> int:
    return word - 65536 if word & 0x8000 else word


def velocity_outcome(terminal: dict) -> str:
    """The velocity route's own result: ok, or the status/detail it names."""
    if terminal.get("ok"):
        return "observed"
    return "%s/%s" % (terminal.get("outcome"), terminal.get("detail"))


def cell_tokens(rpm, acceleration, deceleration, subdivision, duration_ms, stop="normal") -> tuple:
    return ("qualify", str(rpm), str(acceleration), str(deceleration), str(subdivision), str(duration_ms), stop)


# --------------------------------------------------------------------------
# Live sequencing

class Live(starts.Live):
    """Run state of one velocity run (one drive, its address)."""

    def __init__(self, console, run, baseline, args, timeout):
        starts.Live.__init__(self, console, run, baseline, args, timeout)
        self.facts = facts()
        self.address = args.address
        self.deadman_on_us = None
        self.relay_operations = 0
        self.inverted = False  # X3 holds input polarity 1 (a dead-man layout).


def facts() -> dict:
    """Facts the scenario record states per drive and purpose."""
    assign = {"x0": ["triggers-inhibited"], "x1": ["triggers-inhibited", "protection-removal"],
              "x2": ["triggers-inhibited", "protection-removal"], "x3": ["triggers-inhibited", "protection-removal"],
              "y0": ["indicator", "either"]}
    segment = {t: ["triggers-inhibited"] for t in records.TERMINALS}
    out = {str(a): {"assign": dict(assign), "segment": dict(segment), "configure": dict(segment)} for a in DRIVES}
    for a in DRIVES:
        out[str(a)]["start_speed_min_rpm"] = 30
    return out


def headroom(live):
    problem = bench_switch.headroom_problem(live.console)
    if problem:
        raise probe.BenchError(problem)


def prepared_stop(live):
    """The prepared independent stop: stop fast (every bench output drops first)."""
    live.deadman_on_us = None
    return live.call("stop", stop_policy="fast", address=live.address)


def write_field(live, key, value, index=None, mappings=None):
    """One ordinary write with its planned facts (and stated mappings), read back."""
    address = live.address
    entry = {"drive": address, "key": key, "index": index, "candidate": value}
    steps, refusal = records.plan_entry(live.baseline, entry, live.args.image_level, live.facts, restore=False)
    if refusal:
        raise probe.BenchError("no ordinary route for %s: %s" % (key, refusal["reason"]))
    step = dict(steps[0], target=value, candidate=value)
    if mappings:
        step["mappings"] = dict(mappings)
    if live.inverted:
        step["input_mapping"] = "recorded"  # CONFIGURE/SEGMENT records with the dead-man X3 inverted.
    command, kwargs, text = records.write_call({"key": key, "index": index}, value, address)
    step["write"] = {"command": command, "kwargs": kwargs, "text": text}
    records.use_drive(live.console, address, timeout_s=live.timeout)
    outcome = records.apply_write(live.console, step, timeout_s=live.timeout)
    live.run.field(entry, baseline=step["baseline"])
    ending, _ = records.readback(live.console, step, timeout_s=live.timeout)
    live.events.append({"command": text, "ok": outcome["written"], "readback": ending})
    if outcome["uncertain"] or not outcome["written"] or ending != value:
        live.run.field(entry, uncertain=outcome["uncertain"], reason="write %s %s read back %s" % (key, value, ending))
        raise probe.BenchError("write %s %s on %d: written %s uncertain %s readback %s" % (
            key, value, address, outcome["written"], outcome["uncertain"], ending))
    live.run.field(entry, ending=ending)
    return ending


def io_words(live) -> list:
    return list(live.checked("io", driver_args=("read",), address=live.address)["observation"]["raw"])


def deadman_on(live):
    """mosfet2 on with the setup hold: through the interlock when it is off (X3
    holds no start function on any drive), a renewal when it is on."""
    view = {o["name"]: o for o in live.view()["outputs"]}
    if view[DEADMAN_OUTPUT]["active"]:
        live.deadman_on_us = view[DEADMAN_OUTPUT]["on_us"]
        return deadman_renew(live, DEADMAN_SETUP_HOLD_MS)
    live.record["selection"] = {"outputs": [*PIN_OUTPUTS[:2], DEADMAN_OUTPUT], "trigger": "mosfet1"}
    reply = records.bench_on(live.console, DEADMAN_OUTPUT, DEADMAN_SETUP_HOLD_MS, live.record, timeout_s=live.timeout)
    live.views.append(reply)
    live.deadman_on_us = records._bench_view(reply)["outputs"][DEADMAN_OUTPUT]["on_us"]
    return reply


def deadman_renew(live, hold_ms=DEADMAN_HOLD_MS):
    """Move the dead-man's due time to now + hold_ms (bench_renew: never a new ON)."""
    if live.deadman_on_us is None:
        raise probe.BenchError("the dead-man output is not held")
    return records.bench_renew(live.console, DEADMAN_OUTPUT, hold_ms, live.deadman_on_us, timeout_s=live.timeout)


def deadman_lapse_at(at_s):
    """A hook that renews the dead-man until ``at_s`` after the start and then
    lets it lapse at about ``at_s`` (one last renewal with the remaining hold)."""
    state = {}

    def hook(live, elapsed, trial):
        if "lapse_at_s" in state or live.deadman_on_us is None:
            return
        remaining_ms = int((at_s - elapsed) * 1000)
        if remaining_ms > DEADMAN_HOLD_MS // 2:
            deadman_renew(live)
            return
        deadman_renew(live, max(1, remaining_ms))
        state["lapse_at_s"] = at_s
        state["last_renewal_s"] = round(elapsed, 3)
        live.deadman_on_us = None
        trial["lapse"] = state
    return hook


def set_layout(live, layout):
    """Write the layout's functions and X3 polarity, read back. A dead-man X3
    (5, polarity 1) is reached as: polarity 1 with X3 = 0, mosfet2 on, then 5."""
    want = LAYOUTS[layout]
    deadman = want.get("x3", (None,))[0] == "deadman"
    words = io_words(live)
    if want.get("x3", (None,))[0] == "inverted" and not (words[0] >> 3) & 1:
        write_field(live, "io.input-polarity", words[0] | 8)  # X3 = 0: a no-function inversion needs no mapping.
        live.inverted = True
    if deadman:
        live.checked("read-identity", address=live.address)  # The recorded AFTER_INVERSION mapping needs it.
        if words[4] != 0 and words[4] != 5:
            write_field(live, "io.x3", 0)
        if not (words[0] >> 3) & 1:
            write_field(live, "io.input-polarity", words[0] | 8, mappings={"x3": "recorded"})
        live.inverted = True
        deadman_on(live)
        if io_words(live)[4] != 5:
            write_field(live, "io.x3", 5, mappings={"x3": "recorded"})
    for terminal in ("x1", "x2", "x3", "x0"):
        spec = want.get(terminal)
        if spec is None or spec[0] in ("deadman", "inverted"):
            continue
        if io_words(live)[1 + records.TERMINALS.index(terminal)] != spec[1]:
            write_field(live, "io." + terminal, spec[1])
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)


def restore_layout(live):
    """Functions back to the baseline words, the trigger first; X3 polarity last
    (X3 = 0 then), with mosfet2 on while X3 still holds 5 with polarity 1."""
    words = io_words(live)
    base = {t: records.baseline_field(live.baseline, live.address, "io." + t)["value"] for t in records.TERMINALS}
    polarity = records.baseline_field(live.baseline, live.address, "io.input-polarity")["value"]
    for terminal in ("x0", "x1", "x2"):
        if io_words(live)[1 + records.TERMINALS.index(terminal)] != base[terminal]:
            write_field(live, "io." + terminal, base[terminal])
    words = io_words(live)
    if words[4] != base["x3"]:
        inverted = (words[0] >> 3) & 1
        if inverted:
            live.checked("read-identity", address=live.address)
            deadman_on(live)
        write_field(live, "io.x3", base["x3"], mappings={"x3": "recorded"} if inverted else None)
    if io_words(live)[0] != polarity:
        write_field(live, "io.input-polarity", polarity)
    live.inverted = bool((io_words(live)[0] >> 3) & 1)
    live.call("bench", host_args=("off",))
    live.deadman_on_us = None
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)


def install_motion_facts(live, mapping_x3="unknown"):
    """Fresh reads and FINITE_MOVE records of every terminal (current function and polarity)."""
    live.checked("read-config", address=live.address)
    live.checked("driver", driver_args=("read",), address=live.address)
    words = io_words(live)
    live.read_state(live.address)
    for i, terminal in enumerate(records.TERMINALS):
        polarity = (words[0] >> i) & 1
        mapping = mapping_x3 if terminal == "x3" else "unknown"
        live.checked("io-qualification-install", host_args=(str(live.address), terminal, str(words[1 + i]), str(polarity),
                                                            "finite-move", mapping, "triggers-inhibited"))


def velocity_snapshot(live):
    """The velocity read the restoration's velocity restore uses (first of the run)."""
    reply = live.checked("velocity-read", address=live.address)
    v = reply["velocity_parameters"]
    words = [v["raw_speed"], v["raw_acceleration_time"], v["raw_deceleration_time"]]
    if "velocity" not in live.snapshots:
        live.snapshots["velocity"] = {"operation_id": reply["operation_id"], "address": live.address, "words": words}
    return words


def jog_set(live, speed, acceleration=100, deceleration=100):
    """velocity jog set (0x001D/3, no start), polled to its end, read back by its own readback."""
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    live.checked("read-config", address=live.address)
    velocity_snapshot(live)
    live.read_state(live.address)
    words = io_words(live)
    for i, terminal in enumerate(records.TERMINALS):
        if 8 <= words[1 + i] <= 10:
            live.checked("io-qualification-install", host_args=(str(live.address), terminal, str(words[1 + i]),
                                                                str((words[0] >> i) & 1), "configure",
                                                                "recorded" if (words[0] >> i) & 1 else "unknown",
                                                                "triggers-inhibited"))
    reply = live.call("velocity-jog", host_args=("set", str(speed), str(acceleration), str(deceleration)))
    entry = {"drive": live.address, "key": "jog.speed", "index": None, "candidate": speed}
    live.run.field(entry, baseline=records.baseline_field(live.baseline, live.address, "jog.speed")["value"])
    if not reply.get("ok"):
        return {"admitted": False, "result": reply.get("result"), "reason": reply.get("reason")}
    final = reply
    deadline = time.monotonic() + 5
    while final.get("pending") and time.monotonic() < deadline:
        time.sleep(0.02)
        final = live.call("velocity-jog", host_args=("inspect",))
    out = {"admitted": True, "words": final.get("words"), "readback": final.get("readback"), "ok": final.get("session_ok"),
           "execution": final.get("execution"), "error": final.get("error"), "unsettled": final.get("unsettled")}
    if final.get("pending") or final.get("unsettled") or not final.get("session_ok"):
        live.run.field(entry, uncertain=True, reason="JOG setter %s did not read back: %s" % (speed, out))
        raise probe.BenchError("JOG setter %s: %s" % (speed, out))
    live.run.field(entry, ending=final.get("readback"))
    return out


def qualify_velocity(live, negative=True):
    """read config (a JOG write expires it), velocity read, velocity qualification install (the stored ramps)."""
    live.checked("read-config", address=live.address)
    words = velocity_snapshot(live)
    live.checked("velocity-qualification-install", host_args=("rpm", "ramp", "inputs") + (("negative",) if negative else ()))
    return words


def velocity_run(live, rpm, duration_ms, stop="normal", cell=None, sample=True, hook=None, extra_s=1.5, pause_s=0.0):
    """One serial run with read-state sampling; the prepared stop fast if anything
    raises. ``cell`` = (acc, dec, subdivision) arms a qualification cell first."""
    headroom(live)
    trial = {"rpm": rpm, "duration_ms": duration_ms, "stop": stop}
    if cell is not None:
        trial["cell"] = live.call("velocity-limits", host_args=cell_tokens(rpm, *cell, duration_ms, stop))
        if not trial["cell"].get("ok"):
            trial["refused"] = "cell %s %s" % (trial["cell"].get("result"), trial["cell"].get("reason"))
            return trial
    live.read_state(live.address)
    first = len(live.samples)
    # The terminal follows the duration and the stop budget (3 s normal, 2 s direct): wired bench 14
    # C2 lost the console when a 10 s run outlasted a fixed 10 s response deadline.
    handle = live.console.begin("velocity", velocity_args=(str(rpm), "rpm", "native", str(duration_ms), "configured", stop),
                                address=live.address, timeout_s=max(live.timeout, duration_ms / 1000.0 + 6.0))
    if not handle.accepted:
        trial["refused"] = {k: v for k, v in live.console.wait(handle).items() if k in ("result", "reason", "ok")}
        return trial
    began = time.monotonic()
    trial["operation_id"] = handle.operation_id
    try:
        limit = duration_ms / 1000.0 + extra_s + 4.0
        while handle.terminal is None and time.monotonic() - began < limit:
            if sample:
                live.read_state(live.address)
            if hook is not None:
                hook(live, time.monotonic() - began, trial)
            if not sample or pause_s:
                time.sleep(pause_s or 0.05)
        t = live.console.wait(handle, release=True)
        trial["terminal"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                     "stop_completion", "stop_due_us", "uncertain")}
        trial["start_us"] = (t.get("trigger_evidence") or {}).get("latest_us")
        trial["stop_due_us"] = t.get("stop_due_us")
        trial["outcome"] = velocity_outcome(t)
    except BaseException:
        trial["exception_stop"] = prepared_stop(live).get("result")
        raise
    finally:
        trial["samples"] = live.samples[first:]
    return trial


def settle(live, seconds=1.0):
    """Samples for ``seconds``; if still moving at the end, the prepared stop."""
    end = time.monotonic() + seconds
    reads = []
    while time.monotonic() < end:
        s = live.read_state(live.address)
        if s:
            reads.append(s)
        time.sleep(0.05)
    last = reads[-1] if reads else None
    moving = last is None or bench_switch.still_moving(last)
    out = {"samples": reads, "moving": moving}
    if moving:
        out["stop"] = prepared_stop(live).get("result")
        s = None
        for wait_s in (0.5, 1.0, 2.0):
            time.sleep(wait_s)
            s = live.read_state(live.address)
            if s is not None and not bench_switch.still_moving(s):
                break
        out["after_stop"] = s
        if s is None or bench_switch.still_moving(s):
            # A RUNNING that stop fast does not clear (HW-01 type) ends the run (prompt 14 step 4).
            raise probe.BenchError("drive %d still moving after the prepared stop fast" % live.address)
    return out


def run_slope(trial, rpm, subdivision, ramp_ms, tolerance):
    """Slope over the constant-speed part: from the start plus the ramp (+0.2 s) to the stop due time."""
    if not trial.get("start_us") or not trial.get("stop_due_us"):
        return "indeterminate", "no start or stop time"
    start = trial["start_us"] / 1e6 + ramp_ms / 1000.0 + 0.2
    end = trial["stop_due_us"] / 1e6 - 0.05
    trial["slope"] = steady_slope(trial["samples"], start, end)
    return slope_verdict(trial["slope"], rpm, subdivision, tolerance)


# ---------------------------------------------------------------- the cases

def case_r1(live):
    qualify_velocity(live)  # The limits decide only after the stated velocity facts.
    live.read_state(live.address)
    drv = live.checked("drv").get("operation_id")
    out = {}
    for name, args in (("61rpm", ("61", "rpm", "native", "1000", "configured", "normal")),
                       ("1001ms", ("30", "rpm", "native", "1001", "configured", "normal"))):
        r = refused_request(live, args)
        out[name] = {"accepted": r["accepted"], **{k: (r.get("refused") or {}).get(k) for k in ("result", "reason")}}
    out["latest_before"], out["latest_after"] = drv, live.checked("drv").get("operation_id")
    return out


def case_r2(live):
    qualify_velocity(live)
    out = {"cell": (100, 100, SUBDIVISION)}
    run = velocity_run(live, 120, 2000, cell=(100, 100, SUBDIVISION))
    run["settle"] = settle(live)
    out["run"] = run
    out["limits_after"] = live.checked("velocity-limits")
    qualify_velocity(live)
    live.read_state(live.address)
    drv = live.checked("drv").get("operation_id")
    out["again"] = refused_request(live, ("120", "rpm", "native", "2000", "configured", "normal"))
    out["rpm121"] = refused_request(live, ("121", "rpm", "native", "2000", "configured", "normal"))
    out["latest_before"], out["latest_after"] = drv, live.checked("drv").get("operation_id")
    return out


def refused_request(live, args):
    """One velocity request expected to refuse before any frame; the prepared stop if it was admitted."""
    handle = live.console.begin("velocity", velocity_args=args, address=live.address, timeout_s=live.timeout)
    if handle.accepted:
        prepared_stop(live)
        return {"accepted": True}
    reply = live.console.wait(handle)
    return {"accepted": False, "refused": {"ok": reply.get("ok"), "result": reply.get("result"), "reason": reply.get("reason")}}


def case_r3(live):
    out = {"set60": jog_set(live, 60), "set120": jog_set(live, 120)}
    out["read"] = velocity_snapshot(live)
    return out


def case_r4(live):
    set_layout(live, "deadman")
    trials = []
    try:
        deadman_on(live)
        install_motion_facts(live, "recorded")
        qualify_velocity(live)
        deadman_renew(live)
        trial = velocity_run(live, 30, 1000, hook=deadman_lapse_at(0.3))
        trial["settle"] = settle(live)
        trials.append(trial)
    finally:
        restore_layout(live)
    return {"trials": trials}


def arm_trial(live, kind_tokens, output, width_ms, hook=None, window_ms=10000, deadman=False, stop_at_s=None):
    """arm velocity|pv, the firmware trigger, inspect until done; prepared stop on any exception."""
    headroom(live)
    trial = {"arm": list(kind_tokens), "output": output, "width_ms": width_ms}
    reply = live.call("arm-velocity" if kind_tokens[0] == "velocity" else "arm-pv",
                      host_args=tuple(kind_tokens[1:]) + ("poll-ms", "50", "window-ms", str(window_ms)))
    if not reply.get("ok"):
        trial["refused"] = "arm %s %s" % (reply.get("result"), reply.get("reason"))
        return trial
    session = reply["session"]["operation_id"]
    trial["armed"] = {k: reply["session"]["members"][0].get(k) for k in
                      ("function", "stored_rpm", "expected_direction", "staged_words", "record", "record_words", "layout",
                       "baseline_position")}
    try:
        if deadman:
            deadman_renew(live, 3000)  # Covers the trigger's interlock reads; the loop renews with the run hold.
        trigger = live.call("arm-trigger", host_args=(str(session), output, str(width_ms)))
        decided = starts.wait_phase(live, session, lambda s: s["phase"] != "triggering", 5.0)
        isolation = decided.get("isolation") or {}
        trial["isolation"] = {"admitted": isolation.get("admitted"), "row": isolation.get("row"), "reason": isolation.get("reason")}
        if not trigger.get("ok") or not isolation.get("admitted"):
            trial["refused"] = "trigger %s %s (%s %s)" % (trigger.get("result"), trigger.get("reason"), isolation.get("row"),
                                                         isolation.get("reason"))
            return trial
        trial["on_us"] = (decided.get("source") or {}).get("on_us")
        began, timeline = time.monotonic(), []
        limit = window_ms / 1000.0 + 5.0
        while True:
            if deadman and live.deadman_on_us is not None and hook is None:
                deadman_renew(live)
            s = starts.inspect(live, session)
            m = (s.get("members") or [{}])[0]
            r = m.get("result") or {}
            timeline.append({"change": r.get("position_change"), "request_us": r.get("last_request_us"),
                             "reply_us": r.get("last_reply_us"), "outcome": r.get("outcome"),
                             "running": r.get("running_observed")})
            elapsed = time.monotonic() - began
            if hook is not None:
                hook(live, elapsed, trial)
            if stop_at_s is not None and elapsed >= stop_at_s and "stop_sent" not in trial:
                trial["stop_sent"] = {"at_s": round(elapsed, 3), "result": prepared_stop(live).get("result")}
            if s["phase"] == "done" or elapsed > limit:
                break
            time.sleep(0.1)
        trial["timeline"] = timeline
        final = starts.inspect(live, session)
        trial["result"] = (final.get("members") or [{}])[0].get("result")
        trial["source"] = final.get("source")
    except BaseException:
        trial["exception_stop"] = prepared_stop(live).get("result")
        raise
    finally:
        trial["finish"] = starts.finish_session(live, session)
        trial["settle"] = settle(live, 0.6)
    return trial


def timeline_slope(trial, after_on_s=0.25, before_off_s=0.05) -> float | None:
    """Slope of the session's own samples during the level, from position_change at the sample brackets."""
    on = trial.get("on_us")
    off = ((trial.get("source") or {}).get("off_us")) or 0
    if not on:
        return None
    pts = []
    for e in trial.get("timeline", []):
        if e.get("change") is None or not e.get("request_us") or not e.get("reply_us"):
            continue
        t = (e["request_us"] + e["reply_us"]) / 2e6
        if t >= on / 1e6 + after_on_s and (not off or t <= off / 1e6 - before_off_s):
            pts.append((t, e["change"]))
    dedup = sorted(set(pts))
    return slope(dedup)


def case_r5(live):
    set_layout(live, "jog+deadman")
    trials = []
    try:
        deadman_on(live)
        jog_set(live, 30)
        install_motion_facts(live, "recorded")
        velocity_snapshot(live)
        trial = arm_trial(live, ("velocity", "x0"), "mosfet1", 3000, hook=deadman_lapse_at(1.0), deadman=True)
        trials.append(trial)
    finally:
        restore_layout(live)
    return {"trials": trials}


def set_pv_record(live, n, speed):
    for key, value in (("records.speed.speed", speed), ("records.speed.acceleration", 100), ("records.speed.deceleration", 100)):
        if records.baseline_field(live.baseline, live.address, key, n) is None:
            raise probe.BenchError("no baseline field %s %d" % (key, n))
    write_field(live, "records.speed.speed", speed, index=n)


def select_pins(live, record, count):
    """PIN outputs for ``record`` (PIN0 relay2, PIN1 relay1, PIN2 mosfet2 when count is 3), interlocked, then settle."""
    want = {PIN_OUTPUTS[b]: bool(((record - 1) >> b) & 1) for b in range(count)}
    view = {o["name"]: o["active"] for o in live.view()["outputs"]}
    for output, on in want.items():
        if view.get(output) and not on:
            live.views.append(live.checked("bench", host_args=("off", output)))
    for b, (output, on) in enumerate(want.items()):
        if on and not view.get(output):
            named = [{"address": live.address, "output": output, "terminal": records.TERMINALS[1 + b], "function": 14 + b,
                      "polarity": 0, "effect": records.SELECT_EFFECT, "trigger_terminal": "x0"}]
            live.record["running_members"] = named
            try:
                reply = records.bench_on(live.console, output, 60000, live.record, timeout_s=live.timeout)
            finally:
                live.record["running_members"] = []
            if output.startswith("relay"):
                live.relay_operations += 1
            live.views.append(reply)
    time.sleep(SETTLE_S)


def pv_reads(live, record, mapping_x3="unknown"):
    install_motion_facts(live, mapping_x3)
    live.checked("segment", driver_args=("speed", str(record), "read"), address=live.address)
    live.read_state(live.address)


def case_r6(live, out_key="R6"):
    write_field(live, "drive.interruption", 1)
    set_layout(live, "pv2-deadman")
    trials = []
    try:
        deadman_on(live)
        set_pv_record(live, 2, 30)
        live.record["selection"] = {"outputs": [*PIN_OUTPUTS[:2], DEADMAN_OUTPUT], "trigger": "mosfet1"}
        select_pins(live, 2, 2)
        pv_reads(live, 2, "recorded")
        trial = arm_trial(live, ("pv", "x0", "2", "pv2-deadman"), "mosfet1", 50, hook=deadman_lapse_at(0.5), deadman=True,
                          stop_at_s=2.0, window_ms=8000)
        trials.append(trial)
    finally:
        live.call("bench", host_args=("off",))
        restore_layout(live)
        write_field(live, "drive.interruption", 0)
    return {"trials": trials}


def case_v1(live):
    trials = []
    start_speed = records.baseline_field(live.baseline, live.address, "profile.start-speed")["value"]
    try:
        for initial in (0, start_speed):
            if stored_start_speed(live) != initial:
                write_start_speed(live, initial)
            for ramp in (100, 300, 500):
                jog_set(live, 30, ramp, ramp)
                for rpm in (60, -60):
                    qualify_velocity(live)
                    trial = velocity_run(live, rpm, 1000)
                    trial.update(ramp=ramp, start_speed=initial)
                    trial["settle"] = settle(live, 0.8)
                    pts = points(trial["samples"])
                    v = rate(rpm) * (1 if rpm > 0 else -1)
                    v0 = rate(initial) * (1 if rpm > 0 else -1)
                    if pts and trial.get("start_us"):
                        window = [(t, p) for t, p in pts if t <= trial["stop_due_us"] / 1e6]
                        trial["fit_v0_0"] = ramp_fit(window, v, 0.0, trial["start_us"] / 1e6 - 0.05, trial["start_us"] / 1e6 + 0.2)
                        if initial:
                            trial["fit_v0_start"] = ramp_fit(window, v, v0, trial["start_us"] / 1e6 - 0.05,
                                                             trial["start_us"] / 1e6 + 0.2)
                    trial["slope_verdict"] = run_slope(trial, rpm, SUBDIVISION, ramp, SLOPE_TOLERANCE_B)
                    trials.append(trial)
    finally:
        if stored_start_speed(live) != start_speed:
            write_start_speed(live, start_speed)
    return {"trials": trials}


def stored_start_speed(live):
    """0x0020 from a fresh profile read; the host profile session is forgotten again
    (a later readback opens its own)."""
    import bench_motion as motion
    live.checked("read-config", address=live.address)
    live.checked("motion-profile", host_args=("forget",))
    word = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)["current"][0]
    live.checked("motion-profile", host_args=("forget",))
    return word


def write_start_speed(live, value):
    """motion-profile start-speed with CONFIGURE facts, then the start-speed relation the moves need."""
    write_field(live, "profile.start-speed", value)
    live.call("motion-profile-start-speed-relation", host_args=(str(value), str(max(value, 1))))


def case_jog(live, cid):
    case = CASES[cid]
    deadman = "deadman" in case["layout"]
    set_layout(live, case["layout"])
    trials = []
    try:
        for stored in (60, -60):
            if deadman:
                deadman_on(live)
            jog_set(live, stored)
            install_motion_facts(live, "recorded" if deadman else "unknown")
            velocity_snapshot(live)
            trial = arm_trial(live, ("velocity", case["terminal"]), TRIGGER[case["terminal"]], 1000, deadman=deadman)
            trial["stored"] = stored
            trials.append(trial)
    finally:
        restore_layout(live)
    return {"trials": trials}


def case_pv1(live):
    if read_interruption(live) != 0:
        write_field(live, "drive.interruption", 0)
    set_layout(live, "pin3")
    trials = []
    try:
        for n in range(1, 9):
            set_pv_record(live, n, pv_speed(n))
        live.record["selection"] = {"outputs": list(PIN_OUTPUTS), "trigger": "mosfet1"}
        for n in range(1, 9):
            select_pins(live, n, 3)
            pv_reads(live, n)
            trial = arm_trial(live, ("pv", "x0", str(n), "pin3"), "mosfet1", 1000)
            trial.update(record=n, rpm=pv_speed(n), slope=timeline_slope(trial))
            trials.append(trial)
    finally:
        live.call("bench", host_args=("off",))
        restore_layout(live)
    return {"trials": trials}


def pv_speed(n: int) -> int:
    """PV1 record speeds: 12, 18, ..., 54 rpm (distinct, inside the 60 rpm input limit)."""
    return 6 * (n + 1)


def read_interruption(live):
    reply = live.checked("driver", driver_args=("read",), address=live.address)
    return (reply.get("observation") or {}).get("raw", [None] * 6)[5]


def case_pv3(live):
    write_field(live, "drive.interruption", 1)
    set_layout(live, "pv2-deadman")
    trials = []
    try:
        set_pv_record(live, 2, 30)
        live.record["selection"] = {"outputs": [*PIN_OUTPUTS[:2], DEADMAN_OUTPUT], "trigger": "mosfet1"}
        for width, stop_at in ((50, 1.0), (3000, 4.0)):
            deadman_on(live)  # The prepared stop dropped every output; on again through the interlock.
            select_pins(live, 2, 2)
            pv_reads(live, 2, "recorded")
            trial = arm_trial(live, ("pv", "x0", "2", "pv2-deadman"), "mosfet1", width, deadman=True, stop_at_s=stop_at,
                              window_ms=8000)
            trial.update(width=width, stop_at_s=stop_at)
            trials.append(trial)
        # A second edge after disarm: one 50 ms bench pulse with no session (no arm).
        trials.append({"second_edge": "not run: an unarmed function-13 pulse needs a named PV start member in the "
                                      "interlock (the D13 start effect is function 7 only)"})
    finally:
        live.call("bench", host_args=("off",))
        restore_layout(live)
        write_field(live, "drive.interruption", 0)
    return {"trials": trials}


# --------------------------------------------- phase C (drive 2, D15 safeguard)

def alarm_check(live) -> dict:
    """After a run: a fresh read; if the drive is silent or the owner needs
    recovery (alarm 5 makes the drive silent 1.0-1.9 s), wait 2 s, one explicit
    recovery, read again. An alarm is cleared with alarm-clear only on drives 2
    and 4 (D15 item 3); nothing is resent."""
    out = {}
    reply = live.call("read-state", address=live.address)
    if not reply.get("ok"):
        out["silent"] = reply.get("result")
        time.sleep(2.0)
        if reply.get("result") == "recovery_required" or not live.call("read-state", address=live.address).get("ok"):
            handle = live.console.begin("recover", timeout_s=max(live.timeout, 10))
            out["recover"] = live.console.wait(handle, release=True).get("ok") if handle.accepted else "refused"
        reply = live.call("read-state", address=live.address)
    s = stops.sample(reply)
    if s:
        live.samples.append(s)
    out["alarm"] = None if s is None else 0
    if s and (s["alarm"] or s["motion"] & ALARM_BIT):
        out["alarm"] = s["alarm"] or "flag"
        if live.address in (2, 4):
            time.sleep(0.5)
            out["alarm_clear"] = live.call("alarm-clear", address=live.address).get("result")
            probe.observe_stopped(live.console, address=live.address, timeout_s=live.timeout)
    return out


def cell_trial(live, rpm, ramp, subdivision, duration_ms, stop="normal", deceleration=None, creep=False):
    deceleration = ramp if deceleration is None else deceleration
    qualify_velocity(live)
    # Phase C samples about every 0.15 s: enough points at constant speed, and the
    # evidence of about 250 runs stays inside the session budget.
    trial = velocity_run(live, rpm, duration_ms, stop=stop, cell=(ramp, deceleration, subdivision),
                         extra_s=deceleration / 1000.0 + 2.0, pause_s=CELL_SAMPLE_PAUSE_S)
    trial.update(ramp=ramp, deceleration=deceleration, subdivision=subdivision)
    if trial.get("refused"):
        return trial
    check = alarm_check(live) if not (trial.get("terminal") or {}).get("ok") else {}
    after = settle(live, CREEP_AFTER_S + 0.3 if creep else 0.6)
    trial["after"] = {"moving": after["moving"], "stop": after.get("stop")}
    late = [x for x in after["samples"] if after["samples"] and
            feedback_time(x) >= feedback_time(after["samples"][0]) + CREEP_AFTER_S]
    trial["creep"] = creep and (after["moving"] or creeping(late))
    check = check or alarm_check(live)
    trial["alarm"] = check.get("alarm")
    trial["alarm_check"] = check
    return trial


def case_c1(live, out, only=None):
    """C1 rows (ramp, sign) in order; ``only`` limits them (a resumed run). Rows
    are appended to ``out`` as they run, so an abort keeps every finished trial."""
    rows = out.setdefault("rows", [])
    for ramp in C1_RAMPS:
        if only and not any(r == ramp for r, _ in only):
            continue
        jog_set(live, 30, ramp, ramp)
        for sign in (1, -1):
            if only and (ramp, sign) not in only:
                continue
            row = {"ramp": ramp, "sign": sign, "cells": [], "aborted": None}
            rows.append(row)
            for speed in C1_SPEEDS:
                if row["aborted"]:
                    break
                cell = {"rpm": sign * speed, "trials": []}
                row["cells"].append(cell)
                for _ in range(C1_TRIALS):
                    trial = cell_trial(live, sign * speed, ramp, SUBDIVISION, 2000)
                    verdict = run_slope(trial, sign * speed, SUBDIVISION, ramp, SLOPE_TOLERANCE_C) if not trial.get("refused") else \
                        ("FAIL", "refused %s" % trial.get("refused"))
                    trial["verdict"] = verdict
                    cell["trials"].append(slim(trial))
                    problem = abort_problem(trial)
                    if problem:
                        row["aborted"] = {"rpm": sign * speed, "problem": problem}
                        break
                cell["verdict"] = "PASS" if all(t["verdict"][0] == "PASS" for t in cell["trials"]) and \
                    len(cell["trials"]) == C1_TRIALS else "FAIL"
    return out


def abort_problem(trial) -> str | None:
    if trial.get("refused"):
        return "refused: %s" % trial["refused"]
    if trial.get("alarm"):
        return "alarm %s" % trial["alarm"]
    if trial.get("after", {}).get("moving"):
        return "unresolved stop (moving after the run)"
    if trial.get("verdict", ("PASS",))[0] == "FAIL":
        return "slope: %s" % trial["verdict"][1]
    if trial.get("terminal") and not trial["terminal"].get("ok"):
        return "run outcome %s" % trial.get("outcome")
    return None


def slim(trial) -> dict:
    """A trial without its raw samples (kept in the session log), with the sample count."""
    out = {k: v for k, v in trial.items() if k != "samples"}
    out["sample_count"] = len(trial.get("samples") or [])
    pts = points(trial.get("samples") or [])
    out["positions"] = [[round(t, 4), p] for t, p in pts]
    return out


def case_c2(live):
    jog_set(live, 30, 100, 100)
    trials = []
    for _ in range(2):
        trial = cell_trial(live, 300, 100, SUBDIVISION, 10000)
        trial["verdict"] = run_slope(trial, 300, SUBDIVISION, 100, SLOPE_TOLERANCE_C) if not trial.get("refused") else ("FAIL", "refused")
        trials.append(slim(trial))
        if abort_problem(trial):
            break
    return {"trials": trials}


def case_c3(live, out):
    base = records.baseline_field(live.baseline, live.address, "drive.subdivision")["value"]
    rows = out.setdefault("rows", [])
    try:
        write_field(live, "drive.subdivision", C3_SUBDIVISION)
        jog_set(live, 30, 500, 500)
        for speed in C3_SPEEDS:
            for sign in (1, -1):
                for point, duration in (("ramp_0.9", 450), ("constant", 2000)):
                    row = {"rpm": sign * speed, "stop_point": point, "duration_ms": duration, "trials": [], "creeps": 0}
                    rows.append(row)
                    for _ in range(C3_TRIALS):
                        trial = cell_trial(live, sign * speed, 500, C3_SUBDIVISION, duration, creep=True)
                        row["trials"].append(slim(trial))
                        if trial.get("creep"):
                            row["creeps"] += 1
                        if row["creeps"] >= 3 or trial.get("alarm") or trial.get("refused"):
                            row["ended"] = "3 creeps" if row["creeps"] >= 3 else "alarm %s" % trial.get("alarm") \
                                if trial.get("alarm") else "refused"
                            break
    finally:
        write_field(live, "drive.subdivision", base)
    return out


def case_c4(live, highest):
    if not highest:
        return {"trials": [], "note": "no passing C1 speed"}
    jog_set(live, 30, 100, 2000)
    trials = []
    for _ in range(2):
        trial = cell_trial(live, highest, 100, SUBDIVISION, 2000, deceleration=2000)
        trial["verdict"] = stop_budget_verdict(trial)
        trials.append(slim(trial))
        if abort_problem(trial):
            break
    return {"trials": trials, "rpm": highest}


def stop_budget_verdict(trial) -> tuple:
    if trial.get("refused"):
        return "FAIL", "refused"
    t = trial.get("terminal") or {}
    if not t.get("ok"):
        return "FAIL", "the run's stop was not observed inside its budget (%s)" % trial.get("outcome")
    return "PASS", "stop observed inside the 3 s normal budget"


def highest_passing(c1) -> int | None:
    best = None
    for row in (c1 or {}).get("rows", []):
        if row["sign"] != 1 or row["ramp"] != 100:
            continue
        for cell in row["cells"]:
            if cell.get("verdict") == "PASS":
                best = max(best or 0, cell["rpm"])
    return best


# ---------------------------------------------------------------- run, verdicts

def run_cases(live, args, cases):
    detail = {"cases": {}, "address": live.address}
    live.partial = detail["cases"]
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    velocity_snapshot(live)  # The restoration's velocity restore source (D15 item 4).
    live.checked("velocity-limits", host_args=("reset",))
    for cid in cases:
        case = CASES[cid]
        problem = plan_problem(case["uses"] + layout_uses(case["layout"]))
        if problem:
            detail["cases"][cid] = {"refused_plan": problem}
            continue
        kind = case["kind"]
        try:
            if kind == "not-run":
                detail["cases"][cid] = {"note": case["reason"]}
            elif kind == "refuse":
                detail["cases"][cid] = case_r1(live)
            elif kind == "cell":
                detail["cases"][cid] = case_r2(live)
            elif kind == "jog-setter":
                detail["cases"][cid] = case_r3(live)
            elif kind == "deadman-velocity":
                detail["cases"][cid] = case_r4(live)
            elif kind == "deadman-jog":
                detail["cases"][cid] = case_r5(live)
            elif kind == "deadman-pv":
                detail["cases"][cid] = case_r6(live)
            elif kind == "ramps":
                detail["cases"][cid] = case_v1(live)
            elif kind == "jog":
                detail["cases"][cid] = case_jog(live, cid)
            elif kind == "pv-table":
                detail["cases"][cid] = case_pv1(live)
            elif kind == "pv-edge":
                detail["cases"][cid] = case_pv3(live)
            elif kind == "cells":
                case_c1(live, detail["cases"].setdefault(cid, {}), parse_rows(args.c1_rows))
            elif kind == "duration":
                detail["cases"][cid] = case_c2(live)
            elif kind == "creep":
                case_c3(live, detail["cases"].setdefault(cid, {}))
            elif kind == "stop-budget":
                detail["cases"][cid] = case_c4(live, highest_passing(detail["cases"].get("C1")) or args.c4_rpm)
        finally:
            live.call("velocity-limits", host_args=("reset",))
    detail["relay_operations"] = live.relay_operations
    return detail


def parse_rows(tokens) -> list | None:
    """``--c1-rows RAMP:SIGN ...`` (for example 500:-1) as (ramp, sign) pairs, or None for all."""
    if not tokens:
        return None
    out = []
    for token in tokens:
        ramp, _, sign = token.partition(":")
        if int(ramp) not in C1_RAMPS or sign not in ("1", "-1"):
            raise ValueError("C1 rows are RAMP:SIGN with ramp %s and sign 1 or -1" % (C1_RAMPS,))
        out.append((int(ramp), int(sign)))
    return out


def jog_verdict(trial, want_direction) -> tuple:
    if trial.get("refused"):
        return "FAIL", trial["refused"]
    r = trial.get("result") or {}
    change = r.get("position_change")
    if r.get("outcome") != "attributed" or not change:
        return "FAIL", "outcome %s change %s" % (r.get("outcome"), change)
    if (change > 0) != (want_direction > 0):
        return "FAIL", "change %d, direction %d" % (change, want_direction)
    if (trial.get("settle") or {}).get("moving"):
        return "FAIL", "still moving after the session"
    return "PASS", "change %+d, completion %s" % (change, r.get("completion"))


def case_verdicts(run, cid, detail, error):
    case = CASES[cid]
    out = (detail.get("cases") or {}).get(cid)
    ev = ["velocity.json"]
    if case["kind"] == "not-run":
        for level in case["levels"]:
            run.verdict(cid, level, "NOT RUN", case["reason"], ev)
        return
    if not out or (not out.get("trials") and case["kind"] not in ("refuse", "cell", "jog-setter", "cells", "creep")):
        for level in case["levels"]:
            run.verdict(cid, level, "NOT RUN", "not reached: %s" % (error or (out or {}).get("refused_plan") or "no trial"), ev)
        return
    kind = case["kind"]
    if kind == "refuse":
        ok = all(not out[k]["accepted"] and out[k]["reason"] == "outside_velocity_limits" for k in ("61rpm", "1001ms")) and \
            out["latest_before"] == out["latest_after"]
        run.verdict(cid, "sent", "PASS" if ok else "FAIL", "61 rpm %s, 1001 ms %s; latest operation %s -> %s" % (
            out["61rpm"], out["1001ms"], out["latest_before"], out["latest_after"]), ev)
        return
    if kind == "cell":
        r = out["run"]
        first = bool(r.get("terminal", {}).get("ok")) and not r.get("refused")
        slope_v = run_slope(r, 120, SUBDIVISION, 100, SLOPE_TOLERANCE_C) if first else ("FAIL", "not run")
        run.verdict(cid, "drive-reported", "PASS" if first and slope_v[0] == "PASS" else "FAIL",
                    "cell run %s; %s; consumed %s" % (r.get("outcome"), slope_v[1], out["limits_after"].get("consumed_operation_id")), ev)
        refused = all(not out[k]["accepted"] and out[k]["refused"]["reason"] == "outside_velocity_limits"
                      for k in ("again", "rpm121")) and out["latest_before"] == out["latest_after"]
        run.verdict(cid, "sent", "PASS" if refused else "FAIL", "again %s, 121 rpm %s; latest %s -> %s" % (
            out["again"].get("refused"), out["rpm121"].get("refused"), out["latest_before"], out["latest_after"]), ev)
        return
    if kind == "jog-setter":
        s60, s120 = out["set60"], out["set120"]
        ok = s60.get("ok") and s60.get("readback") == [60, 100, 100] and not s120.get("admitted") and \
            s120.get("reason") == "outside_velocity_limits"
        run.verdict(cid, "read back", "PASS" if ok else "FAIL", "60: %s; 120: %s" % (s60.get("readback"), s120.get("reason")), ev)
        return
    if kind in ("deadman-velocity", "deadman-jog", "deadman-pv"):
        t = out["trials"][0]
        if kind == "deadman-velocity":
            term = t.get("terminal") or {}
            early = term.get("detail") in (STOP_INPUT_DETAIL, STOPPED_EARLY_DETAIL)
            standstill = stopped_before(t.get("samples") or [], (t.get("start_us") or 0) / 1e6, (t.get("stop_due_us") or 0) / 1e6)
            ok = not t.get("refused") and early and standstill is not None and not (t.get("settle") or {}).get("moving")
            note = "outcome %s, lapse at %s s, drive-reported standstill %s s before the timed stop" % (
                t.get("outcome"), (t.get("lapse") or {}).get("lapse_at_s"),
                None if standstill is None else round((t["stop_due_us"] / 1e6) - standstill, 3))
        else:
            r = t.get("result") or {}
            ok = not t.get("refused") and r.get("completion") == "velocity_stopped" and not (t.get("settle") or {}).get("moving")
            ok = ok and (deadman_before_width(t) if kind == "deadman-jog" else stopped_before_stop(t))
            note = "%s; outcome %s completion %s change %s; lapse at %s s; prepared stop %s" % (
                t.get("refused") or "armed", r.get("outcome"), r.get("completion"), r.get("position_change"),
                (t.get("lapse") or {}).get("lapse_at_s"), (t.get("stop_sent") or {}).get("at_s"))
        run.verdict(cid, "drive-reported", "PASS" if ok else "FAIL", note, ev)
        return
    if kind == "ramps":
        notes = []
        bad = 0
        for t in out["trials"]:
            v = t.get("slope_verdict", ("indeterminate", ""))
            bad += v[0] != "PASS"
            fit = t.get("fit_v0_0") or {}
            notes.append("%+d rpm ramp %d start %d: ramp %.2f s, %s" % (t["rpm"], t["ramp"], t["start_speed"],
                                                                     fit.get("ramp_s", -1), v[1]))
        run.verdict(cid, "drive-reported", "PASS" if not bad and len(out["trials"]) == 12 else "FAIL", "; ".join(notes), ev)
        return
    if kind == "jog":
        values = []
        for t in out["trials"]:
            direction = 1 if case["function"] == 9 else -1 if case["function"] == 10 else (1 if t["stored"] > 0 else -1)
            values.append((t["stored"], jog_verdict(t, direction)))
        verdict = "PASS" if len(values) == 2 and all(v[1][0] == "PASS" for v in values) else "FAIL"
        run.verdict(cid, "drive-reported", verdict, "; ".join("0x001D %+d: %s" % (s, v[1]) for s, v in values), ev)
        return
    if kind == "pv-table":
        notes, bad = [], 0
        for t in out["trials"]:
            r = t.get("result") or {}
            s = t.get("slope")
            want = rate(t["rpm"])
            good = not t.get("refused") and r.get("outcome") == "attributed" and s is not None and \
                abs(s - want) <= SLOPE_TOLERANCE_B * want
            bad += not good
            notes.append("record %d %d rpm: %s slope %s/%.0f" % (t["record"], t["rpm"], t.get("refused") or r.get("outcome"),
                                                                 None if s is None else round(s), want))
        run.verdict(cid, "drive-reported", "PASS" if not bad and len(out["trials"]) == 8 else "FAIL", "; ".join(notes), ev)
        return
    if kind == "pv-edge":
        notes, bad = [], 0
        for t in out["trials"]:
            if "second_edge" in t:
                notes.append(t["second_edge"])
                continue
            r = t.get("result") or {}
            ended = (t.get("stop_sent") is not None or r.get("completion") == "velocity_stopped") and \
                not (t.get("settle") or {}).get("moving")
            bad += t.get("refused") is not None or not ended
            notes.append("width %d: %s, stop at %s s, outcome %s completion %s change %s" % (
                t["width"], t.get("refused") or "armed", (t.get("stop_sent") or {}).get("at_s"), r.get("outcome"),
                r.get("completion"), r.get("position_change")))
        run.verdict(cid, "drive-reported", "PASS" if not bad else "FAIL", "; ".join(notes), ev)
        return
    if kind == "cells":
        cells = [(row, c) for row in out["rows"] for c in row["cells"]]
        passed = [c["rpm"] for _, c in cells if c.get("verdict") == "PASS"]
        aborted = [r["aborted"] for r in out["rows"] if r.get("aborted")]
        run.verdict(cid, "drive-reported", "PASS" if cells and not aborted and len(passed) == len(cells) else "FAIL",
                    "%d cells, %d passed; aborted rows %s" % (len(cells), len(passed), aborted), ev)
        return
    if kind == "duration":
        values = [t["verdict"] for t in out["trials"]]
        run.verdict(cid, "drive-reported", "PASS" if len(values) == 2 and all(v[0] == "PASS" for v in values) else "FAIL",
                    "; ".join(v[1] for v in values), ev)
        return
    if kind == "creep":
        rows = out["rows"]
        creeps = sum(r["creeps"] for r in rows)
        ended = [(r["rpm"], r["stop_point"], r.get("ended")) for r in rows if r.get("ended")]
        run.verdict(cid, "drive-reported", "PASS" if rows and not creeps and not ended else "FAIL",
                    "%d rows, %d runs, %d creeps, ended %s" % (len(rows), sum(len(r["trials"]) for r in rows), creeps, ended), ev)
        return
    if kind == "stop-budget":
        values = [t["verdict"] for t in out["trials"]]
        run.verdict(cid, "drive-reported", "PASS" if len(values) == 2 and all(v[0] == "PASS" for v in values) else "FAIL",
                    "%s rpm: %s" % (out.get("rpm"), "; ".join(v[1] for v in values)), ev)


def deadman_before_width(trial) -> bool:
    """The JOG run reached its final position before the trigger width ended,
    while the trigger output was still on (its OFF at the width's end)."""
    r = trial.get("result") or {}
    on, width = trial.get("on_us"), trial.get("width_ms")
    off = (trial.get("source") or {}).get("off_us") or 0
    end = last_change_time(trial)
    return bool(on and width and r.get("completion") == "velocity_stopped" and off >= on + width * 1000 - 5000 and
                end is not None and end < on / 1e6 + width / 1000.0)


def stopped_before_stop(trial) -> bool:
    """An edge PV run reached its final position before the prepared stop was sent
    (``stop_sent.at_s`` after the observation began, which follows the ON)."""
    end, on = last_change_time(trial), trial.get("on_us")
    stop = (trial.get("stop_sent") or {}).get("at_s")
    return end is not None and bool(on) and (stop is None or end < on / 1e6 + stop)


def last_change_time(trial) -> float | None:
    """Controller time (s) of the first sample showing the final position (the run's end)."""
    final = None
    for e in trial.get("timeline", []):
        if e.get("change") is not None:
            final = e["change"]
    if final is None:
        return None
    for e in trial.get("timeline", []):
        if e.get("change") == final and e.get("reply_us"):
            return e["reply_us"] / 1e6
    return None


def plan(args, baseline, facts_):
    """Scenario-record fields of one velocity-jog-pv run (bench step 1), before any port access."""
    address = args.address
    write_list = []
    layouts = {CASES[c]["layout"] for c in args.cases}
    for terminal in records.TERMINALS:
        functions = [LAYOUTS[name][terminal][1] for name in sorted(layouts) if terminal in LAYOUTS[name]]
        if functions:
            write_list.append({"drive": address, "key": "io." + terminal, "index": None, "candidate": functions[0]})
    if any(spec[2] for name in layouts for spec in LAYOUTS[name].values()):
        write_list.append({"drive": address, "key": "io.input-polarity", "index": None, "candidate": 8})
    for key in ("jog.speed", "jog.acceleration", "jog.deceleration"):
        write_list.append({"drive": address, "key": key, "index": None, "candidate": 30})
    if {"R6", "PV1", "PV3"} & set(args.cases):
        write_list.append({"drive": address, "key": "drive.interruption", "index": None, "candidate": 1})
        for n in range(1, 9):
            write_list.append({"drive": address, "key": "records.speed.speed", "index": n, "candidate": pv_speed(n)})
    if "V1" in args.cases:
        write_list.append({"drive": address, "key": "profile.start-speed", "index": None, "candidate": 0})
    if "C3" in args.cases:
        write_list.append({"drive": address, "key": "drive.subdivision", "index": None, "candidate": C3_SUBDIVISION})
    restoration = records.restoration_plan(baseline, write_list, args.image_level, facts_,
                                           context={"snapshots": {"velocity": True}})
    cases = [{"id": c, "required": True, "assertion": CASES[c]["assertion"], "required_levels": list(CASES[c]["levels"])}
             for c in args.cases]
    plans = {c: {"uses": CASES[c]["uses"], "layout": CASES[c]["layout"],
                 "problem": plan_problem(CASES[c]["uses"] + layout_uses(CASES[c]["layout"]))} for c in args.cases}
    commands = ["velocity read A (the restoration's velocity restore source); velocity limits reset",
                "R1: velocity 61 rpm native 1000 / 30 rpm native 1001 configured normal (refused)",
                "R2: velocity limits qualify 120 100 100 1000 2000 normal; velocity 120 rpm native 2000 configured normal; "
                "the same again; 121",
                "R3: velocity jog set 60 100 100; velocity jog set 120 100 100",
                "dead-man layouts: io set input-polarity 8 (X3 = 0), interlocked bench on mosfet2 700, io set x3 5 "
                "(recorded mapping), renewals every 0.2 s; a lapse asserts the stop",
                "J/R5: velocity jog set +-60|30; io qualification install finite-move; arm velocity xN; arm trigger ID OUT W",
                "PV: segment speed N set speed V; driver set interruption 0|1; PIN outputs through the interlock; "
                "arm pv x0 N pin3|pv2-deadman; arm trigger ID mosfet1 W; prepared stop fast",
                "C: per run velocity limits qualify RPM R R SUB MS normal; velocity RPM rpm native MS configured normal; "
                "read state sampling; alarm-clear after alarm 5 (drive 2 only)",
                "restoration: layout back (trigger first, X3 with mosfet2 on, polarity last), then the plan"]
    bounds = {"address": address, "legacy_rpm": LEGACY_RPM, "deadman_hold_ms": DEADMAN_HOLD_MS, "deadman_renewal": "every trial-loop pass (about 0.1 s)",
              "c1_speeds": C1_SPEEDS, "c1_ramps": C1_RAMPS, "c3_speeds": C3_SPEEDS, "c3_subdivision": C3_SUBDIVISION,
              "plans": plans,
              "exposure": {"serial": "the controller's own timed stop ends a serial run (controller and bus needed, not the "
                                     "harness); a dead-man (X3 = 5 inverted) ends it when renewals stop",
                           "level": "the trigger width lapses in the controller (8-10, PV level); with a dead-man also its lapse",
                           "edge_pv": "nothing but a stop; the dead-man or the prepared stop",
                           "no_deadman": "J2 and a controller hang with outputs on: rotation until the controller, link or "
                                         "owner recovers (D11)"}}
    expected = {"drive-reported": "velocity results, read-state samples (position vs controller time), session results",
                "read back": "JOG setter readback", "sent": "no operation after the refused requests"}
    return write_list, restoration, cases, commands, bounds, expected, []


def trials_from_session(entries) -> list:
    """Serial velocity trials rebuilt from a run's session log (``session.jsonl``
    entries): each velocity terminal with the read-state samples taken while it
    ran. Used when a run stopped before its case record was written."""
    trials, samples, current = [], [], None
    for e in entries:
        if e.get("event") != "reply":
            continue
        r = e.get("response") or {}
        if r.get("type") == "read" and r.get("command") == "read-state":
            s = stops.sample(r)
            if s is not None:
                samples.append(s)
        elif r.get("type") == "reply" and r.get("command") == "velocity" and r.get("ok"):
            current = {"operation_id": r.get("operation_id"), "first": len(samples)}
        elif r.get("type") == "velocity" and current and r.get("operation_id") == current["operation_id"]:
            words = r.get("staging_words") or [0, 0, 0]
            trials.append({"operation_id": r["operation_id"], "rpm": r.get("native_rpm"), "ramp": words[1],
                           "deceleration": words[2], "duration_ms": (r.get("requested") or {}).get("duration_us", 0) // 1000,
                           "terminal": {k: r.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                                "stop_completion")},
                           "outcome": velocity_outcome(r), "start_us": (r.get("trigger_evidence") or {}).get("latest_us"),
                           "stop_due_us": r.get("stop_due_us"), "alarm": r.get("raw_alarm"),
                           "samples": samples[current["first"]:]})
            current = None
    return trials
