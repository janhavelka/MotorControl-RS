"""Wired bench 12: switch homing with host-driven (virtual) switches.

The harness emulates the switches a home searches for. The firmware ``bench``
command stays GPIO-only; the position comes from ordinary ``read state``
samples of the homing drive. Origin is X0 (``mosfet1``), positive limit X1
(``relay2``), negative limit X2 (``relay1``); each line reaches X0-X2 of drives
1, 2 and 4 in parallel.

Window mode: the origin is active while the position lies in ``[a, b]``; a
limit is active beyond its threshold. A level change is decided from the
current sample and switched at once: ``bench off`` for a deassertion, the D14
interlock and ``bench on`` for an assertion (the homing drive is a named
running member). A home is a long operation, so the member's pre-start
``io read`` is bounded by the home's observation deadline
(``HOME_IO_BOUND_MS``) instead of the 10 s move bound; the axis reservation
refuses every settings write to it until the home ends.

Bound (prompt 12, Implement 1). A crossing of boundary ``b`` happened between
the previous sample that showed the old side (from its request start ``t0``)
and the deciding one (to its reply end ``t1``). The drive's input changes by
``t_gpio + d_out + d_in``, so the emulated edge lies within
``dx = v_max * (t_gpio + d_out + d_in - t0)`` of ``b`` on the travel side,
``v_max`` the larger of the commanded rate and the fastest observed speed.
Preflight: ``v_max * (G + D + d)`` at most a quarter of the narrowest window
or gap, and every window wider than ``2 * v_max * G``, with the stated budget
``G`` (worst sample gap) and ``D`` (worst decision-to-GPIO time). Live: a gap
over ``G``, a delay over ``D``, a speed over the stated ceiling or a window
crossed between two samples invalidates that edge and takes the stop path
(``stop fast``, then fresh stopped state). A travel guard sends ``stop fast``
outside ``[start - B, start + B]``. Holds are three loop cycles and renewed
while a level stays asserted; a lapsed hold only deasserts that switch.

Results are emulated-switch evidence: drive-reported (status, position) and
controller-timestamped (GPIO times, sample windows). There is no independent
switch position, shaft observation or electrical timing.
"""
import time

import bench_limits as limits
import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stop

SUBDIVISION = 1000
INDEX_PERIOD = 1000          # Increments per revolution at subdivision 1000 (internal Z once per turn).
LINES = {"origin": ("mosfet1", "x0", 1), "positive": ("relay2", "x1", 2), "negative": ("relay1", "x2", 3)}
OUTPUT_ROLE = {v[0]: k for k, v in LINES.items()}
# Prompt 05 (filter 2): input reported at most 32.4 ms after the GPIO edge (on),
# 19.5 ms (off), relay operate time included. One stated value for every edge.
D_EDGE_MS = 35.0
EDGE_MARGIN = 20             # Increments added to every bound (position read resolution, stated).
SPEED_CEILING = 1.25         # Live ceiling: 1.25 x the commanded search rate (sample-time noise, stated).
SPEED_BASELINE_US = 200000   # Speeds from samples at least 200 ms apart.
JUMP_SLACK = 300             # A step this far beyond the ceiling is a coordinate change, not motion.
GUARD = 12000                # B, increments from the start.
HOME_IO_BOUND_MS = 31000     # Named home member's pre-start io read: the home's 30 s deadline plus 1 s.
# Wired bench 12 deviation 1: the session evidence cap was reached during a
# search and the console then refused every command, the guard's stop and the
# restoration included. A trial starts only with this much evidence left: about
# three times the largest measured trial (H4: 16 MiB over 13.5 trials) plus the
# restoration.
TRIAL_EVIDENCE_BYTES = 4 * 1024 * 1024
TRIAL_EVIDENCE_RECORDS = 6000
SEGMENT_MIN = 100            # A direction segment needs this much monotonic travel.
FINAL_MARGIN = 50            # Final-position tolerance (deceleration at L, read resolution), stated.
WINDOW = 1000
APPROACH = 3000
LIMIT_GAP = 4000

# Method rows (docs/ess_homing.md). family, first inactive search direction,
# internal Z, the reference switch and its final edge, and the expected
# direction sequence per branch (signs follow the manual).
_POS = {7: ("+-", "+-", "-"), 8: ("+-+", "+-+", "-+"), 9: ("+-", "+-+-", "+-"), 10: ("+", "+-+", "+")}
_NEG = {11: ("-+", "-+", "+"), 12: ("-+-", "-+-", "+-"), 13: ("-+", "-+-+", "-+"), 14: ("-", "-+-", "-")}
_ORIGIN = {3: ("+-", "-"), 4: ("+", "-+"), 5: ("-+", "+"), 6: ("-", "+-")}
METHODS = {}
for _m, (_inactive, _active) in {1: ("-+", "+"), 2: ("+-", "-"), 17: ("-+", "+")}.items():
    METHODS[_m] = {"family": "limit", "limit": "positive" if _m == 2 else "negative", "direction": 1 if _m == 2 else -1,
                   "index": _m != 17, "reference": "limit", "paths": {"approaching": _inactive, "active": _active}}
for _base, _index in ((0, True), (16, False)):
    for _m, (_inactive, _active) in _ORIGIN.items():
        METHODS[_m + _base] = {"family": "origin", "limit": None, "direction": 1 if _inactive[0] == "+" else -1,
                               "index": _index, "reference": "origin",
                               "paths": {"approaching": _inactive, "active": _active}}
    for _m, (_a, _b, _c) in _POS.items():
        METHODS[_m + _base] = {"family": "positive", "limit": "positive", "direction": 1, "index": _index,
                               "reference": "origin", "paths": {"approaching": _a, "beyond": _b, "active": _c}}
    for _m, (_a, _b, _c) in _NEG.items():
        METHODS[_m + _base] = {"family": "negative", "limit": "negative", "direction": -1, "index": _index,
                               "reference": "origin", "paths": {"approaching": _a, "beyond": _b, "active": _c}}
BRANCHES = ("approaching", "beyond", "active")


def rate(word: int, subdivision: int = SUBDIVISION) -> float:
    """Commanded increments/s of a native speed word (ProbeApp policy: word x subdivision / 60)."""
    return word * subdivision / 60.0


# --------------------------------------------------------------------------
# Geometry and decisions (pure)

def geometry(method: int, branch: str, start: int, *, window=WINDOW, approach=APPROACH, gap=LIMIT_GAP,
             guard=GUARD, origin=True) -> dict:
    """Switch layout relative to the start ``s`` (prompt 12 bench step 2).

    approaching: origin [s + d*approach, s + d*(approach + window)], the
    family's limit ``gap`` beyond it. active: origin centred on the start, the
    limit ``gap`` beyond its far edge. beyond: the limit 500 ahead, the origin
    ending ``gap`` before it (behind the start). Limit methods: the limit
    ``approach`` ahead (approaching) or 500 behind the start (active).
    ``origin=False`` leaves the origin out (H6: a switch that never asserts).
    The other limit is never asserted.
    """
    row = METHODS[method]
    if branch not in row["paths"]:
        raise ValueError("method %d has no %s branch" % (method, branch))
    d = row["direction"]
    g = {"method": method, "branch": branch, "start": start, "direction": d, "origin": None,
         "positive": None, "negative": None, "guard": [start - guard, start + guard]}

    def span(p, q):
        return [min(p, q), max(p, q)]
    if row["family"] == "limit":
        g[row["limit"]] = start + d * approach if branch == "approaching" else start - d * 500
        return g
    if branch == "approaching":
        near, far = start + d * approach, start + d * (approach + window)
    elif branch == "active":
        near, far = start - d * (window // 2), start + d * (window // 2)
    else:
        far = start + d * (500 - gap)
        near = far - d * window
    if origin:
        g["origin"] = span(near, far)
    if row["limit"]:
        g[row["limit"]] = (start + d * 500) if branch == "beyond" else far + d * gap
    return g


def levels(g: dict, position: int) -> dict:
    """Output levels for one position: origin inside its window, a limit at or beyond its threshold."""
    out = {"mosfet1": False, "relay2": False, "relay1": False}
    if g["origin"] is not None:
        out["mosfet1"] = g["origin"][0] <= position <= g["origin"][1]
    if g["positive"] is not None:
        out["relay2"] = position >= g["positive"]
    if g["negative"] is not None:
        out["relay1"] = position <= g["negative"]
    return out


def boundaries(g: dict) -> list:
    out = []
    if g["origin"] is not None:
        out += [("mosfet1", g["origin"][0]), ("mosfet1", g["origin"][1])]
    for name, output in (("positive", "relay2"), ("negative", "relay1")):
        if g[name] is not None:
            out.append((output, g[name]))
    return out


def narrowest(g: dict) -> float | None:
    """Narrowest window or gap of the layout (origin width, origin-to-limit gaps); None for a limit alone."""
    widths = []
    if g["origin"] is not None:
        a, b = g["origin"]
        widths.append(b - a)
        if g["positive"] is not None:
            widths.append(abs(g["positive"] - b))
        if g["negative"] is not None:
            widths.append(abs(a - g["negative"]))
    return min(widths) if widths else None


def preflight(g: dict, v_max: float, budget: dict) -> str | None:
    """None when the layout fits the budget; else the refusal (before the trigger)."""
    seconds = (budget["gap_ms"] + budget["decision_ms"] + D_EDGE_MS) / 1000.0
    need = v_max * seconds
    width = narrowest(g)
    if width is not None and need > width / 4.0:
        return "v_max %.0f/s x (G %s + D %s + d %s ms) = %.0f increments exceeds a quarter of the narrowest %d" % (
            v_max, budget["gap_ms"], budget["decision_ms"], D_EDGE_MS, need, width)
    if g["origin"] is not None and g["origin"][1] - g["origin"][0] <= 2 * v_max * budget["gap_ms"] / 1000.0:
        return "origin window %d is not wider than 2 x v_max x G = %.0f" % (
            g["origin"][1] - g["origin"][0], 2 * v_max * budget["gap_ms"] / 1000.0)
    return None


def window_mid(s: dict, key: str = "feedback") -> float:
    w = s["windows"][key]
    return (w[0] + w[1]) / 2.0


def skipped(g: dict, before: int, after: int) -> list:
    """Outputs whose level would have changed twice between two positions (a window crossed unseen)."""
    out = []
    if g["origin"] is not None:
        a, b = g["origin"]
        lo, hi = min(before, after), max(before, after)
        if lo < a and hi > b:
            out.append("mosfet1")
    return out


def edge_bound(t0_us: int, t_gpio_us: int, v_max: float) -> float:
    """dx: how far past the boundary, on the travel side, the emulated edge can lie."""
    return v_max * (t_gpio_us + D_EDGE_MS * 1000.0 - t0_us) / 1e6 + EDGE_MARGIN


def speeds(samples: list) -> list:
    """(time_us, increments/s) of each sample against the latest earlier sample of the
    same coordinate frame at least SPEED_BASELINE_US before it."""
    out = []
    for i, s in enumerate(samples):
        for p in reversed(samples[:i]):
            if p.get("frame") != s.get("frame"):
                break
            dt = window_mid(s) - window_mid(p)
            if dt >= SPEED_BASELINE_US:
                out.append((window_mid(s), abs(s["track"] - p["track"]) / (dt / 1e6)))
                break
    return out


def segments(samples: list, minimum: int = SEGMENT_MIN) -> list:
    """Direction segments of the tracked coordinate: sign, start/end position and time, median speed."""
    out = []
    pivot = None
    for s in samples:
        x, t = s["track"], window_mid(s)
        if pivot is None:
            pivot = (x, t)
            continue
        if out and out[-1]["sign"] * (x - out[-1]["end"]) >= 0:
            out[-1].update(end=x, end_us=t)
            pivot = (x, t)
            continue
        base = out[-1]["end"] if out else pivot[0]
        if abs(x - base) >= minimum:
            sign = 1 if x > base else -1
            if out and out[-1]["sign"] == sign:
                out[-1].update(end=x, end_us=t)
            else:
                origin_x, origin_t = (out[-1]["end"], out[-1]["end_us"]) if out else pivot
                out.append({"sign": sign, "start": origin_x, "end": x, "start_us": origin_t, "end_us": t})
    for seg in out:
        span = (seg["end_us"] - seg["start_us"]) / 1e6
        seg["speed"] = abs(seg["end"] - seg["start"]) / span if span > 0 else None
    return out


def path_string(segs: list) -> str:
    return "".join("+" if s["sign"] > 0 else "-" for s in segs)


def classify_speed(speed: float | None, v_high: float, v_low: float) -> str | None:
    if speed is None:
        return None
    return "H" if abs(speed - v_high) < abs(speed - v_low) else "L"


def final_check(g: dict, edges: list, final: int, segs: list) -> dict:
    """Final position against the method's last reference edge.

    The reference switch is the origin (or the limit for 1, 2, 17). Its last
    emulated edge before completion is the reference edge; the drive stops on
    the travel side within dx plus FINAL_MARGIN, or within one revolution more
    when the method uses internal Z.
    """
    row = METHODS[g["method"]]
    output = "mosfet1" if row["reference"] == "origin" else LINES[row["limit"]][0]
    mine = [e for e in edges if e["output"] == output and e.get("switched")]
    if not mine or not segs:
        return {"ok": False, "reason": "no reference edge or no travel", "edge": None}
    edge = mine[-1]
    direction = segs[-1]["sign"]
    distance = direction * (final - edge["boundary"])
    limit = (edge["dx"] or 0) + FINAL_MARGIN + (INDEX_PERIOD if row["index"] else 0)
    ok = -FINAL_MARGIN <= distance <= limit and edge.get("valid", False)
    return {"ok": ok, "edge": {k: edge.get(k) for k in ("output", "level", "boundary", "dx", "valid")},
            "distance": distance, "limit": limit, "direction": direction,
            "reason": None if ok else "final %d is %d past boundary %d (allowed %d..%d)%s" % (
                final, distance, edge["boundary"], -FINAL_MARGIN, limit, "" if edge.get("valid") else "; edge invalid")}


def branch_verdict(trial: dict) -> dict:
    """Verdicts per evidence level for one branch trial (pure)."""
    expected = METHODS[trial["method"]]["paths"][trial["branch"]]
    out = {"expected_path": expected, "observed_path": trial.get("path")}
    if trial.get("refused"):
        reason = "refused before the trigger: %s" % trial["refused"]
        return dict(out, **{"drive-reported": "NOT RUN", "controller-timestamped": "NOT RUN", "reason": reason})
    invalid = [e for e in trial.get("edges", []) if e.get("switched") and not e.get("valid")]
    stamped = "indeterminate" if invalid or trial.get("stop_path") else "PASS"
    terminal = trial.get("terminal") or {}
    problems = []
    if not terminal.get("ok"):
        problems.append("home %s (%s)" % (terminal.get("outcome"), terminal.get("status")))
    if trial.get("path") != expected:
        problems.append("path %s, expected %s" % (trial.get("path"), expected))
    final = trial.get("final") or {}
    if not final.get("ok"):
        problems.append(final.get("reason") or "final check missing")
    if trial.get("stop_path"):
        drive = "indeterminate"
        problems.insert(0, "stop path: %s" % trial["stop_path"])
    elif invalid:
        drive = "indeterminate"
        problems.insert(0, "edge budget failed: %s" % "; ".join(", ".join(e["problems"]) for e in invalid))
    else:
        drive = "PASS" if not problems else "FAIL"
    return dict(out, **{"drive-reported": drive, "controller-timestamped": stamped,
                        "reason": "; ".join(problems) or None})


# --------------------------------------------------------------------------
# Live sequencing

class Live(stop.Live):
    """Run state of one switch run (drive ``address``)."""

    def __init__(self, console, run, baseline, args, facts, timeout, address):
        self.console, self.run, self.baseline, self.args, self.facts = console, run, baseline, args, facts
        self.timeout, self.address = timeout, address
        self.case = {"y0": None}
        self.samples, self.views, self.events, self.trials, self.refusals = [], [], [], [], []
        self.snapshots = {}
        self.record = {"running_members": [], "max_age_ms": records.MAX_AGE_MS, "read_last": address}
        self.written = set()

    def state(self, address=None):
        return stop.Live.state(self, self.address if address is None else address)


class Switcher:
    """The virtual switch of one home: samples, decisions, GPIO and holds."""

    def __init__(self, live, g, v_high, v_low, budget, home_id, pre):
        self.live, self.console, self.g = live, live.console, g
        self.v_high, self.v_low, self.budget = v_high, v_low, budget
        self.ceiling = v_high * SPEED_CEILING
        self.v_seen = 0.0
        self.home_id, self.pre = home_id, pre
        self.address = live.address
        self.samples, self.edges, self.events = [], [], []
        self.level = {"mosfet1": False, "relay2": False, "relay1": False}
        self.on_us = {}
        self.hold_ms = int(3 * (budget["gap_ms"] + budget["decision_ms"]))
        self.renewed = {}
        self.offset = 0          # Coordinate-change compensation (tracked = raw + offset).
        self.frame = 0
        self.frozen = False
        self.stop_path = None
        self.freeze_after = None  # (output, count): hold levels after that many edges of it (H8/H9 offset motion).

    # Samples ----------------------------------------------------------------
    def _add(self, reply, source):
        s = stop.sample(reply)
        if not s or s.get("address") != self.address:
            return None
        raw = stop.position(s)
        if raw is None:
            return None
        s = dict(s, raw_position=raw, source=source)
        if self.samples:
            prev = self.samples[-1]
            dt = (window_mid(s) - window_mid(prev)) / 1e6
            step = abs(raw + self.offset - prev["track"])
            if dt > 0 and step > self.ceiling * dt + JUMP_SLACK:
                # The drive set a new coordinate (auxiliary 0-2, 6-8): keep the travel continuous.
                self.offset = prev["track"] - raw
                self.frame += 1
                self.frozen = True
                self.events.append({"event": "coordinate change", "raw": raw, "track": prev["track"],
                                    "at_us": s["windows"]["feedback"][0]})
        s.update(track=raw + self.offset, frame=self.frame)
        self.samples.append(s)
        self.live.samples.append(s)
        return s

    def sample(self):
        return self._add(self.live.call("read-state", address=self.address), "loop")

    def _capture(self):
        command = self.console.command
        own = "command" in vars(self.console)

        def capturing(name, **kwargs):
            reply = command(name, **kwargs)
            if name == "read-state" and kwargs.get("address") == self.address:
                self._add(reply, "interlock")
            return reply
        self.console.command = capturing

        def undo():
            if own:
                self.console.command = command
            else:
                del self.console.command
        return undo

    # Outputs ----------------------------------------------------------------
    def _member(self, output):
        role = OUTPUT_ROLE[output]
        _, terminal, function = LINES[role]
        return {"output": output, "terminal": terminal, "effect": "origin" if role == "origin" else "limit",
                "function": function, "polarity": 0, "operation_id": self.home_id, "address": self.address,
                "io_read": self.pre["io_read"], "between": list(self.pre["between"]),
                "io_age_bound_ms": HOME_IO_BOUND_MS}

    def assert_output(self, output, running=True):
        """Interlocked ON; returns the GPIO time on the controller clock."""
        self.live.record["running_members"] = [self._member(output)] if running else []
        undo = self._capture()
        try:
            reply = records.bench_on(self.console, output, self.hold_ms, self.live.record, timeout_s=self.live.timeout)
        finally:
            undo()
            self.live.record["running_members"] = []
        self.live.views.append(reply)
        on = records._bench_view(reply)["outputs"][output]
        self.level[output] = True
        self.on_us[output] = on["on_us"]
        self.renewed[output] = time.monotonic()
        return on["on_us"]

    def deassert(self, output):
        reply = self.live.checked("bench", host_args=("off", output))
        self.live.views.append(reply)
        o = records._bench_view(reply)["outputs"][output]
        self.level[output] = False
        return o.get("off_us") or reply.get("now_us")

    def renew(self):
        """Renew each asserted output before its hold lapses (one cycle budget ahead)."""
        cycle = (self.budget["gap_ms"] + self.budget["decision_ms"]) / 1000.0
        for output, on in list(self.level.items()):
            elapsed = time.monotonic() - self.renewed.get(output, 0)
            if not on or elapsed < cycle:
                continue
            if elapsed > self.hold_ms / 1000.0 - 0.5:
                # Too late to renew safely: switch it off; a later cycle re-asserts through the interlock.
                self.deassert(output)
                self.events.append({"event": "hold lapsed", "output": output, "elapsed_s": round(elapsed, 3)})
                continue
            reply = records.bench_renew(self.console, output, self.hold_ms, self.on_us[output], timeout_s=self.live.timeout)
            self.live.views.append(reply)
            self.renewed[output] = time.monotonic()

    # Decisions --------------------------------------------------------------
    def step(self):
        """One cycle: a sample, the guard, the live checks and any level change. False ends the loop."""
        s = self.sample()
        if s is None:
            return True
        x = s["track"]
        lo, hi = self.g["guard"]
        if not lo <= x <= hi:
            self.stop("travel guard: %d outside [%d, %d]" % (x, lo, hi))
            return False
        for _, v in speeds(self.samples[-8:]):
            self.v_seen = max(self.v_seen, v)
        if self.frozen:
            self.renew()  # Levels stay as they were; a coordinate change ends switching, not the holds.
            return True
        prev = next((p for p in reversed(self.samples[:-1]) if p["frame"] == s["frame"]), None)
        if prev is not None and skipped(self.g, prev["track"], x):
            self.edges.append({"output": "mosfet1", "level": None, "position": x, "switched": False, "valid": False,
                               "problems": ["window crossed between two samples"], "boundary": None})
            self.stop("window crossed between two samples (%d to %d)" % (prev["track"], x))
            return False
        want = levels(self.g, x)
        for output in ("mosfet1", "relay2", "relay1"):
            if want[output] == self.level[output]:
                continue
            edge = {"output": output, "level": want[output], "position": x, "sample": len(self.samples) - 1,
                    "t1_us": s["windows"]["feedback"][1], "problems": []}
            edge["boundary"] = self._boundary(output, x, prev)
            if prev is not None:
                edge["t0_us"] = prev["windows"]["feedback"][0]
                gap_ms = (edge["t1_us"] - edge["t0_us"]) / 1000.0
                edge["gap_ms"] = round(gap_ms, 1)
                if gap_ms > self.budget["gap_ms"]:
                    edge["problems"].append("gap %.1f ms over G %s" % (gap_ms, self.budget["gap_ms"]))
            else:
                edge["t0_us"] = None
                edge["problems"].append("no earlier sample")
            if self.v_seen > self.ceiling:
                edge["problems"].append("speed %.0f over ceiling %.0f" % (self.v_seen, self.ceiling))
            try:
                gpio = self.assert_output(output) if want[output] else self.deassert(output)
            except (records.InterlockRefused, probe.BenchError) as error:
                edge.update(switched=False, valid=False, error=str(error))
                self.edges.append(edge)
                self.stop("switching %s failed: %s" % (output, error))
                return False
            edge.update(switched=True, t_gpio_us=gpio)
            edge["delay_ms"] = round((gpio - edge["t1_us"]) / 1000.0, 1)
            if edge["delay_ms"] > self.budget["decision_ms"]:
                edge["problems"].append("decision %.1f ms over D %s" % (edge["delay_ms"], self.budget["decision_ms"]))
            v = max(self.v_high, self.v_seen)
            edge["v_max"] = round(v, 1)
            edge["dx"] = round(edge_bound(edge["t0_us"], gpio, v), 1) if edge["t0_us"] else None
            edge["valid"] = not edge["problems"]
            self.edges.append(edge)
            if not edge["valid"]:
                self.stop("edge %s %s invalid: %s" % (output, "on" if want[output] else "off", "; ".join(edge["problems"])))
                return False
            if self.freeze_after and output == self.freeze_after[0] and \
                    sum(1 for e in self.edges if e["output"] == output and e.get("switched")) >= self.freeze_after[1]:
                self.frozen = True
                self.events.append({"event": "levels held after the reference edge", "output": output})
        self.renew()
        return True

    def _boundary(self, output, x, prev):
        if output == "mosfet1":
            a, b = self.g["origin"]
            return a if abs(x - a) <= abs(x - b) else b
        return self.g["positive" if output == "relay2" else "negative"]

    def stop(self, reason):
        """The stop path: stop fast (outputs off first), then fresh stopped state."""
        self.stop_path = reason
        self.events.append({"event": "stop path", "reason": reason})
        reply = self.live.call("stop", stop_policy="fast", address=self.address)
        self.events.append({"event": "stop fast", "outcome": reply.get("outcome"), "ok": reply.get("ok")})
        for output in self.level:
            self.level[output] = False
        try:
            st = probe.observe_stopped(self.console, address=self.address, timeout_s=self.live.timeout,
                                       on_sample=lambda r: self._add(r, "stop"))
            self.events.append({"event": "stopped", "ok": probe.drive_reported_stopped(st)})
        except probe.BenchError as error:
            self.events.append({"event": "standstill not confirmed", "error": str(error)})

    def release_all(self):
        off = records.outputs_off(self.console, timeout_s=self.live.timeout)
        for output in self.level:
            self.level[output] = False
        return off


# --------------------------------------------------------------------------
# One branch

HOME_FACTS = ("mechanics", "rates", "ramp", "zero-offset", "auxiliary", "inputs")


def home_facts(method: int, *, reference=True, offset=False, soft_limits=False) -> tuple:
    facts = list(HOME_FACTS)
    if METHODS[method]["index"]:
        facts.append("index")
    if reference:
        facts.append("reference")
    if offset:
        facts.append("offset")
    if soft_limits:
        facts.append("soft-limits")
    return tuple(facts)


def headroom_problem(console) -> str | None:
    """Why the session evidence cannot hold one more trial and its restoration, or None."""
    budget = getattr(console, "evidence_budget", None)
    if budget is None:
        return None
    left_bytes, left_records = budget.max_bytes - budget.bytes, budget.max_records - budget.records
    if budget.exhausted or left_bytes < TRIAL_EVIDENCE_BYTES or left_records < TRIAL_EVIDENCE_RECORDS:
        return "evidence headroom %d bytes, %d records is below one trial (%d, %d); no further home" % (
            left_bytes, left_records, TRIAL_EVIDENCE_BYTES, TRIAL_EVIDENCE_RECORDS)
    return None


def still_moving(s: dict) -> bool:
    """RUNNING reported, or a speed word (r/min) beyond the standstill noise: up to 5 was read at
    rest on drive 1 after a stop, 12 and 16 while searching at L and H."""
    speed = s["speed"] - 65536 if s["speed"] & 0x8000 else s["speed"]
    return bool(s["motion"] & 4) or abs(speed) > 6


def initial_outputs(g: dict, start: int) -> list:
    return [o for o, on in levels(g, start).items() if on]


def run_branch(live, method, branch, *, search, returning, ramp=100, offset=0, budget, origin=True,
               stop_during=None, soft_limits=False, foreign_limit=None, freeze_after=None):
    """One home of ``method`` from ``branch``, emulated switches, analysis inputs.

    The stored auxiliary choice is the run's (``home auxiliary`` before); the
    homing words are staged by the home itself. ``stop_during`` ('normal')
    sends that serial stop mid-search, then stop fast (H7).
    """
    address = live.address
    problem = headroom_problem(live.console)
    if problem:
        raise probe.BenchError(problem)
    trial = {"method": method, "branch": branch, "search": search, "return": returning, "ramp": ramp,
             "offset": offset, "budget": dict(budget), "origin": origin}
    records.use_drive(live.console, address, timeout_s=live.timeout)
    live.checked("read-config", address=address)
    first = stop.sample(live.checked("read-state", address=address))
    start = stop.position(first)
    g = geometry(method, branch, start, origin=origin)
    if foreign_limit is not None:
        # L8 (prompt 11, handed to 12): a limit the method does not use, asserted ahead of the search.
        g[foreign_limit] = start + g["direction"] * 1000
        trial["foreign_limit"] = foreign_limit
    trial["geometry"] = g
    v_high, v_low = rate(search), rate(returning)
    trial["rates"] = {"high": v_high, "low": v_low}
    refusal = preflight(g, max(v_high, v_low), budget)
    trial["preflight"] = refusal
    if refusal:
        trial["refused"] = refusal
        return trial
    home = live.checked("home-read", address=address)
    h = home["homing_parameters"]
    trial["auxiliary"] = h["raw_auxiliary"]
    if "home" not in live.snapshots:
        live.snapshots["home"] = {"operation_id": home["operation_id"], "address": address,
                                  "words": [h["raw_method"], h["raw_search_speed"], h["raw_return_speed"], h["raw_ramp_time"],
                                            *h["raw_offset_words"]],
                                  "baseline_words": records.snapshot_words(live.baseline, address, "home")}
    sw = Switcher(live, g, v_high, v_low, budget, None, None)
    sw.freeze_after = freeze_after
    trial["switch"] = sw
    # Initially active switches: interlocked ON before the home (drive stopped, no named member).
    for output in initial_outputs(g, start):
        sw.assert_output(output, running=False)
        trial.setdefault("initial", []).append(output)
    facts = home_facts(method, offset=offset != 0, soft_limits=soft_limits)
    live.checked("read-config", address=address)
    sw.renew()
    live.checked("read-state", address=address)
    live.checked("home-read", address=address)
    sw.renew()
    installed = live.call("home-qualification-install", host_args=(str(method), str(search), str(returning), str(ramp), *facts))
    if not installed.get("ok"):
        trial["refused"] = "home qualification install: %s" % installed.get("result")
        sw.release_all()
        return trial
    pre = stop.pre_start(live, address)
    sw.renew()
    args = (str(method), str(search), str(returning), str(ramp), "zero" if offset == 0 else str(offset))
    handle = live.console.begin("home", home_args=args, address=address, timeout_s=max(live.timeout, 45))
    trial["command"] = "home " + " ".join(args)
    if not handle.accepted:
        trial["refused"] = "home: %s" % (live.console.wait(handle).get("result"))
        sw.release_all()
        return trial
    sw.home_id, sw.pre = handle.operation_id, pre
    trial["operation_id"] = handle.operation_id
    began = time.monotonic()
    stopped = None
    try:
        while handle.terminal is None and time.monotonic() - began < 40:
            if not sw.step():
                break
            if stop_during and stopped is None and len(sw.samples) > 2 and time.monotonic() - began > 2.0:
                stopped = live.call("stop", stop_policy=stop_during, address=address)
                trial["serial_stop"] = {"policy": stop_during, "outcome": stopped.get("outcome"), "ok": stopped.get("ok")}
                sw.frozen = True  # The serial stop turned every output off; no further edges.
                for output in sw.level:
                    sw.level[output] = False
    except BaseException:
        # The search may still run: the prepared stop while the console still works.
        if live.console.synchronized:
            try:
                live.call("stop", stop_policy="fast", address=address)
            except BaseException:
                pass
        raise
    if handle.terminal is None and sw.stop_path is None and stopped is None:
        sw.stop("no home terminal within 40 s")
    terminal = live.console.wait(handle, release=True)
    if stop_during:
        trial["stop_fast"] = live.call("stop", stop_policy="fast", address=address).get("outcome")
    for _ in range(3):
        last = sw.sample()
    # Wired bench 12 H6: the home's observation deadline ends observation, not the
    # search. A drive still moving after any terminal takes the stop path.
    if last is not None and still_moving(last) and sw.stop_path is None and not stop_during:
        sw.stop("still moving after the home terminal (%s)" % terminal.get("outcome"))
        trial["stopped_after_terminal"] = True
    trial["outputs_off"] = sw.release_all()
    st = probe.observe_stopped(live.console, address=address, timeout_s=live.timeout, on_sample=lambda r: sw._add(r, "end"))
    trial["stopped"] = probe.drive_reported_stopped(st)
    trial["terminal"] = {k: terminal.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                     "uncertain", "end_known", "end_position", "expected_position",
                                                     "expected_known", "polls", "raw_motion", "raw_inputs", "result")}
    return finish_trial(trial, sw)


def finish_trial(trial: dict, sw) -> dict:
    """Pure part of a branch: path, speeds, edges, final check (sw only supplies records)."""
    samples = [s for s in sw.samples if s["source"] in ("loop", "interlock", "stop", "end")]
    samples.sort(key=window_mid)
    segs = segments(samples)
    for seg in segs:
        seg["rate"] = classify_speed(seg["speed"], sw.v_high, sw.v_low)
    trial["segments"] = segs
    trial["path"] = path_string(segs)
    trial["edges"] = sw.edges
    trial["events"] = sw.events
    trial["stop_path"] = sw.stop_path
    trial["v_seen"] = round(sw.v_seen, 1)
    trial["samples"] = [{k: s.get(k) for k in ("raw_position", "track", "frame", "motion", "inputs", "speed", "source")} |
                        {"window": s["windows"]["feedback"], "motion_window": s["windows"]["motion"]} for s in samples]
    pre_change = [s for s in samples if s["frame"] == 0]
    tracked_final = samples[-1]["track"] if samples else None
    trial["tracked_final"] = tracked_final
    trial["raw_final"] = samples[-1]["raw_position"] if samples else None
    trial["coordinate_changes"] = [e for e in sw.events if e["event"] == "coordinate change"]
    trial["final"] = final_check(sw.g, sw.edges, tracked_final, segs) if samples else {"ok": False, "reason": "no samples"}
    gaps = [(b["windows"]["feedback"][1] - a["windows"]["feedback"][0]) / 1000.0
            for a, b in zip(pre_change, pre_change[1:]) if a["source"] == b["source"] == "loop"]
    trial["loop_gap_ms"] = {"max": round(max(gaps), 1) if gaps else None,
                            "median": round(sorted(gaps)[len(gaps) // 2], 1) if gaps else None, "n": len(gaps)}
    trial["decision_ms"] = [e.get("delay_ms") for e in sw.edges]
    del trial["switch"]
    trial["verdict"] = branch_verdict(trial)
    return trial


# --------------------------------------------------------------------------
# Cases (prompt 12 bench step 3) and the run

CASES = {
    "H0": dict(kind="budget", methods=(19,), branches=("approaching",), levels=["controller-timestamped"],
               assertion="budget pilot: method 19 from before the window under a generous budget; the loop's worst "
                         "sample gap G and the worst decision-to-GPIO time D are measured (report only)"),
    "H1": dict(kind="branches", methods=(17, 1, 2), branches=("approaching", "active"),
               levels=["drive-reported", "controller-timestamped"],
               assertion="limit-reference methods 17, 1, 2 from inactive and active limit: phase order, HOMED "
                         "completion, final position within dx of the limit release (one turn more with Z)"),
    "H2": dict(kind="branches", methods=(19, 20, 21, 22), branches=("approaching", "active"),
               levels=["drive-reported", "controller-timestamped"],
               assertion="origin methods 19-22 from inactive and active origin: phase order, completion, final edge"),
    "H3": dict(kind="branches", methods=(3, 4, 5, 6), branches=("approaching", "active"),
               levels=["drive-reported", "controller-timestamped"],
               assertion="origin methods 3-6 with internal Z: final position within one revolution past the edge"),
    "H4": dict(kind="branches", methods=(23, 24, 25, 26, 7, 8, 9, 10), branches=BRANCHES,
               levels=["drive-reported", "controller-timestamped"],
               assertion="positive-limit family 23-26 and 7-10: approaching, beyond and active branches"),
    "H5": dict(kind="branches", methods=(27, 28, 29, 30, 11, 12, 13, 14), branches=BRANCHES,
               levels=["drive-reported", "controller-timestamped"],
               assertion="negative-limit family 27-30 and 11-14: approaching, beyond and active branches"),
    "H6": dict(kind="no-switch", methods=(20,), branches=("approaching",), levels=["drive-reported"],
               assertion="method 20 with the origin never asserted: the travel guard stops the search; no reference"),
    "H7": dict(kind="stop", methods=(24,), branches=("approaching",), levels=["drive-reported"],
               assertion="method 24 mid-search: stop normal, then stop fast; outputs off, the home fails, no reference"),
    "H8": dict(kind="auxiliary", methods=(20,), branches=("approaching",), offsets=(0,), levels=["drive-reported"],
               assertion="method 20, offset 0, auxiliary 0-11: end coordinate per choice"),
    "H9": dict(kind="auxiliary", methods=(20,), branches=("approaching",), offsets=(2000, -2000),
               levels=["drive-reported"],
               assertion="method 20, offset +-2000, auxiliary 0-11: offset motion for 0-5 and end coordinate"),
    "H12": dict(kind="homed-output", methods=(24,), branches=("approaching",), levels=["controller-timestamped"],
                assertion="drive 4, Y0 = 3 (homing complete): the aux1 edge of the home's completion lies between "
                          "the last HOMED-low and the first HOMED-high sample, else indeterminate"),
    "L8": dict(kind="foreign-limit", methods=(20,), branches=("approaching",), levels=["drive-reported"],
               assertion="prompt 11 L8: method 20 (origin only) with the positive limit X1 asserted 1000 increments "
                         "into the search: the drive's reaction and the home's ending; no reference"),
    "H13": dict(kind="soft-limits", methods=(24,), branches=("approaching",), levels=["drive-reported"],
                assertion="drive 4: method 24 with auxiliary 7 and the soft-limits fact, pairs +500/-500, driver set "
                          "soft-limit 1 through the enable route; absolute moves to +400 and -400 complete"),
}
Y0_HOMED = 3
AUX_RULE = {0: "offset", 1: "zero", 2: "negative", 3: "observed", 4: "observed", 5: "observed"}
DOCUMENTED_DELTA = {3: 0, 4: 1, 5: -1}  # actual, actual + offset, actual - offset (times the offset).


def ensure_auxiliary(live, value):
    """The stored 0x0030 choice through `home auxiliary` (CONFIGURE route), read back; never retried."""
    current = live.checked("home-read", address=live.address)["homing_parameters"]["raw_auxiliary"]
    if current != value:
        stop.write_setting(live, live.address, "homing.auxiliary", value)
    return live.checked("home-read", address=live.address)["homing_parameters"]["raw_auxiliary"]


def auxiliary_result(trial: dict, choice: int) -> dict:
    """End coordinate against the documented choice (function manual p72)."""
    t = trial.get("terminal") or {}
    offset = trial["offset"]
    rule = AUX_RULE[choice % 6]
    end = t.get("end_position") if t.get("end_known") else None
    tracked = trial.get("tracked_final")
    out = {"choice": choice, "rule": rule, "offset": offset, "end": end, "tracked": tracked,
           "home_ok": bool(t.get("ok")), "runs_offset": choice <= 5}
    if end is None:
        out.update(documented=False, reason="no end coordinate (home %s)" % t.get("outcome"))
        return out
    if rule == "observed":
        delta = end - tracked if tracked is not None else None
        expected = DOCUMENTED_DELTA[choice % 6] * offset
        out.update(delta=delta, expected_delta=expected,
                   documented=delta is not None and abs(delta - expected) <= FINAL_MARGIN)
    else:
        expected = offset if rule == "offset" else 0 if rule == "zero" else -offset
        out.update(expected=expected, documented=end == expected)
    return out


def run_cases(live, args, cases):
    """Every requested case in order; each trial's record is kept as it ends."""
    detail = {"cases": {}, "budget": {"gap_ms": args.budget_gap_ms, "decision_ms": args.budget_decision_ms},
              "rates": {"search": args.search, "return": args.return_speed}, "address": live.address}
    live.partial = detail["cases"]
    budget = detail["budget"]
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    records.use_drive(live.console, live.address, timeout_s=live.timeout)
    # The restoration snapshot: the first home read, before any write of the run.
    home = live.checked("home-read", address=live.address)
    h = home["homing_parameters"]
    live.snapshots["home"] = {"operation_id": home["operation_id"], "address": live.address,
                              "words": [h["raw_method"], h["raw_search_speed"], h["raw_return_speed"], h["raw_ramp_time"],
                                        *h["raw_offset_words"]],
                              "baseline_words": records.snapshot_words(live.baseline, live.address, "home"),
                              "auxiliary": h["raw_auxiliary"]}
    for cid in cases:
        case = CASES[cid]
        out = detail["cases"][cid] = {"trials": []}
        methods = [m for m in case["methods"] if not args.methods or m in args.methods]
        branches = [b for b in case["branches"] if not args.branches or b in args.branches]
        runs = args.repetitions or 1
        try:
            if case["kind"] == "auxiliary":
                choices = args.choices if args.choices else list(range(12))
                for offset in case["offsets"]:
                    for choice in choices:
                        ensure_auxiliary(live, choice)
                        # Method 20 from before the window: the reference is the first origin ON; the
                        # levels are held after it so the offset motion of 0-5 creates no edge.
                        trial = run_branch(live, 20, "approaching", search=args.search, returning=args.return_speed,
                                           offset=offset, budget=budget, freeze_after=("mosfet1", 1))
                        trial["auxiliary_result"] = auxiliary_result(trial, choice)
                        out["trials"].append(trial)
                        if trial.get("stop_path"):
                            raise probe.BenchError("stop path: %s" % trial["stop_path"])
                continue
            if case["kind"] == "homed-output":
                out["trials"].append(homed_output(live, args, budget))
                continue
            if case["kind"] == "soft-limits":
                out["trials"].append(soft_limit_home(live, args, budget))
                continue
            if case["kind"] == "foreign-limit":
                ensure_auxiliary(live, args.auxiliary)
                trial = run_branch(live, 20, "approaching", search=args.search, returning=args.return_speed,
                                   budget=budget, foreign_limit="positive")
                out["trials"].append(trial)
                continue
            ensure_auxiliary(live, args.auxiliary)
            for index in range(runs):
                for method in methods:
                    for branch in branches:
                        if branch not in METHODS[method]["paths"]:
                            continue
                        trial = run_branch(live, method, branch, search=args.search, returning=args.return_speed,
                                           budget=budget, origin=case["kind"] != "no-switch",
                                           stop_during="normal" if case["kind"] == "stop" else None)
                        trial["index"] = index + 1
                        out["trials"].append(trial)
                        path = trial.get("stop_path") or ""
                        if path and case["kind"] != "no-switch" and not path.startswith("edge"):
                            raise probe.BenchError("stop path: %s" % path)
        except probe.BenchError as error:
            out["error"] = str(error)
            raise
    return detail


def homed_output(live, args, budget):
    """H12: Y0 = 3 on drive 4, then one switch home; the aux1 edge against the HOMED samples."""
    if live.address != records.FIXTURE["y0_owner"]:
        raise probe.BenchError("H12 runs on the Y0 owner, drive %d" % records.FIXTURE["y0_owner"])
    ensure_auxiliary(live, args.auxiliary)
    stop.write_setting(live, live.address, "io.y0", Y0_HOMED)
    live.checked("bench", host_args=("clear",))
    trial = run_branch(live, 24, "approaching", search=args.search, returning=args.return_speed, budget=budget)
    view = live.view()
    found, lost = records._merge_edges([records._bench_view(v) for v in live.views if v.get("ok")])
    trial["aux1"] = homed_edge(trial, found)
    trial["aux1"]["lost_windows"] = lost
    trial["aux1"]["now_us"] = view.get("now_us")
    return trial


def homed_edge(trial: dict, edges: list) -> dict:
    """The aux1 edge of HOMED rising (Y0 on -> aux1 inactive, inverted) inside
    [last HOMED-low sample request start, first HOMED-high sample reply end]."""
    samples = [s for s in trial.get("samples", []) if s.get("motion") is not None]
    rise = None
    for old, new in zip(samples, samples[1:]):
        if not old["motion"] & 2 and new["motion"] & 2:
            rise = [old["motion_window"][0], new["motion_window"][1]]
    out = {"bracket_us": rise, "edge_us": None, "verdict": "indeterminate"}
    if rise is None:
        out["reason"] = "no HOMED rise between two samples"
        return out
    near = [e for e in edges if e.get("active") is False and rise[0] - 2000000 <= e["at_us"] <= rise[1] + 2000000]
    if not near:
        out["reason"] = "no aux1 inactive edge near the HOMED rise"
        return out
    edge = min(near, key=lambda e: abs(e["at_us"] - (rise[0] + rise[1]) / 2))
    out["edge_us"] = edge["at_us"]
    out["after_low_ms"] = round((edge["at_us"] - rise[0]) / 1000.0, 1)
    out["before_high_ms"] = round((rise[1] - edge["at_us"]) / 1000.0, 1)
    inside = rise[0] <= edge["at_us"] <= rise[1]
    out["verdict"] = "PASS" if inside else "indeterminate"
    out["reason"] = None if inside else "aux1 edge outside the HOMED bracket"
    return out


def soft_limit_home(live, args, budget):
    """H13 on drive 4: auxiliary 7 and pairs (prompt 11 setup), a method 24 switch home with
    the soft-limits fact, the enable route, absolute moves inside, then prompt 11's restore."""
    if live.address != records.FIXTURE["y0_owner"]:
        raise probe.BenchError("H13 runs on drive %d (prompt 11 soft-limit helpers)" % records.FIXTURE["y0_owner"])
    out = {"method": 24, "branch": "approaching", "offset": 0, "setup": limits.soft_setup(live)}
    try:
        trial = run_branch(live, 24, "approaching", search=args.search, returning=args.return_speed, budget=budget,
                           soft_limits=True)
        out["home"] = trial
        if not (trial.get("terminal") or {}).get("ok"):
            out["error"] = "switch home did not complete"
            return out
        live.checked("read-config", address=live.address)
        out["pairs"] = live.checked("driver", driver_args=("read",), address=live.address)["observation"].get("limits_decoded")
        out["axis_before"] = limits.axis_limits(live)
        out["enable"] = limits.setting(live, "drive.soft-limit", 1)
        live.checked("read-config", address=live.address)
        out["axis_after"] = limits.axis_limits(live)
        out["moves"] = [limits.absolute_move(live, 400), limits.absolute_move(live, -400)]
    finally:
        out["restore"] = limits.soft_restore(live)
    return out


def case_verdicts(run, cid, detail, error):
    """Verdicts per evidence level of one case from its trials (one reason line)."""
    case = CASES[cid]
    out = (detail.get("cases") or {}).get(cid)
    levels = case["levels"]
    if not out or not out["trials"]:
        reason = "not reached: %s" % (error or (out or {}).get("error") or "no trial")
        for level in levels:
            run.verdict(cid, level, "NOT RUN", reason, ["switch.json"])
        return
    trials = out["trials"]
    if case["kind"] == "homed-output":
        a = trials[0].get("aux1") or {}
        run.verdict(cid, "controller-timestamped", a.get("verdict", "indeterminate"),
                    "bracket %s, edge %s (%s ms after the low sample start, %s ms before the high reply end)%s" % (
                        a.get("bracket_us"), a.get("edge_us"), a.get("after_low_ms"), a.get("before_high_ms"),
                        "; " + a["reason"] if a.get("reason") else ""), ["switch.json"])
        return
    if case["kind"] == "foreign-limit":
        t = trials[0]
        g = t.get("geometry") or {}
        segs = t.get("segments") or []
        stop_at = segs[-1]["end"] if segs else None
        home = t.get("terminal") or {}
        reason = "home %s (detail %s), path %s, stopped at %s against the limit at %s, stop path %s" % (
            home.get("outcome"), home.get("detail"), t.get("path"), stop_at, g.get("positive"), t.get("stop_path"))
        ok = not home.get("ok") and t.get("stopped") and stop_at is not None and g.get("positive") is not None and \
            stop_at - g["positive"] <= 600
        run.verdict(cid, "drive-reported", "PASS" if ok else "FAIL", reason, ["switch.json"])
        return
    if case["kind"] == "soft-limits":
        t = trials[0]
        moves = t.get("moves") or []
        home_ok = bool(((t.get("home") or {}).get("terminal") or {}).get("ok"))
        enabled = bool((t.get("axis_after") or {}).get("known"))
        done = len(moves) == 2 and all((m.get("terminal") or {}).get("ok") and limits.near(m) for m in moves)
        run.verdict(cid, "drive-reported", "PASS" if home_ok and enabled and done else "FAIL",
                    "home %s, limits known %s %s, moves %s" % (
                        home_ok, enabled, (t.get("axis_after") or {}).get("limits"),
                        [(m.get("target"), (m.get("state") or {}).get("position")) for m in moves]), ["switch.json"])
        return
    if case["kind"] == "budget":
        t = trials[0]
        # Report only: PASS when the loop gaps and at least one decision-to-GPIO time were measured.
        ok = (t.get("loop_gap_ms") or {}).get("max") is not None and any(d is not None for d in t.get("decision_ms", []))
        run.verdict(cid, "controller-timestamped", "PASS" if ok else "indeterminate",
                    "G max %s ms (median %s), D %s ms" % ((t.get("loop_gap_ms") or {}).get("max"),
                                                        (t.get("loop_gap_ms") or {}).get("median"),
                                                        [d for d in t.get("decision_ms", []) if d is not None]),
                    ["switch.json"])
        return
    if case["kind"] in ("no-switch", "stop"):
        bad = [t for t in trials if (t.get("terminal") or {}).get("ok") or not t.get("stopped")]
        reason = "; ".join("%s %s: home %s, stopped %s, stop path %s" % (
            t["method"], t["branch"], (t.get("terminal") or {}).get("outcome"), t.get("stopped"), t.get("stop_path"))
            for t in trials)
        run.verdict(cid, "drive-reported", "FAIL" if bad else "PASS", reason, ["switch.json"])
        return
    if case["kind"] == "auxiliary":
        results = [t.get("auxiliary_result") or {} for t in trials]
        failed = [r for r in results if not r.get("home_ok")]
        differs = [r for r in results if r.get("home_ok") and not r.get("documented")]
        reason = "; ".join("aux %s offset %s: end %s (%s)%s" % (
            r.get("choice"), r.get("offset"), r.get("end"), r.get("rule"),
            "" if r.get("documented") else " differs from the documented coordinate") for r in results)
        if differs:
            reason += "; recorded, not documented: %d" % len(differs)
        run.verdict(cid, "drive-reported", "FAIL" if failed else "PASS", reason, ["switch.json"])
        return
    for level in levels:
        values = [t["verdict"][level] for t in trials]
        verdict = ("FAIL" if "FAIL" in values else "indeterminate" if "indeterminate" in values else
                   "NOT RUN" if all(v == "NOT RUN" for v in values) else "PASS")
        notes = ["%d %s#%s: %s%s" % (t["method"], t["branch"], t.get("index", 1), t["verdict"][level],
                                     " (%s)" % t["verdict"]["reason"] if t["verdict"]["reason"] and
                                     t["verdict"][level] != "PASS" else "") for t in trials]
        run.verdict(cid, level, verdict, "; ".join(notes) if verdict != "PASS" else "%d trials" % len(trials),
                    ["switch.json"])
