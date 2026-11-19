"""Wired bench 13: PT segment execution with PIN selection (function 12).

The named layout on an armed drive is X0 = 12 PT trigger (``mosfet1``), X1 =
14 PIN0 (``relay2``), X2 = 15 PIN1 (``relay1``), X3 = 16 PIN2 (``mosfet2``);
the relays are swapped against the plan (D2). Record = PIN2*4 + PIN1*2 + PIN0
+ 1 (function manual p22-23), so records 1-8 are selectable.

Every trial: the selection-consequence check over all sixteen stored records
(no trigger while any endpoint the drive could start lies outside the travel
bound), the PIN outputs through ``bench_records.bench_on`` (D14 interlock with
named selection members), at least ``SETTLE_S`` after the last PIN change, a
``read state`` that shows the PIN levels, the fresh reads and FINITE_MOVE
records the arm needs, ``arm segment``, the firmware's own trigger, and an
observation loop that sends the prepared ``stop fast`` (``group stop fast`` for
a group) when the position leaves the expected path band, when the evidence
budget runs low, or when the loop raises. The PINs are held until every armed
drive stops; a stop request drops every bench output in the firmware.

Pure analysis functions are unit-tested (``test/bench_pt_test.py``).
"""
import time

import bench_input_starts as starts
import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stops
import bench_switch

DRIVES = list(records.FIXTURE["input_drives"])
TRIGGER_OUTPUT, TRIGGER_TERMINAL = "mosfet1", "x0"
PIN_OUTPUTS = ("relay2", "relay1", "mosfet2")       # PIN0, PIN1, PIN2 (FIXTURE outputs, D2).
PIN_TERMINALS = ("x1", "x2", "x3")
LAYOUT = {"x0": 12, "x1": 14, "x2": 15, "x3": 16}
SELECTION = {"outputs": list(PIN_OUTPUTS), "trigger": TRIGGER_OUTPUT}
# Settle after the last PIN change: relay operate about 3 ms plus bounce, the
# 10 ms level rule (hardware manual p9) and the input delay up to 32.4 ms
# measured at filter 2 (wired bench 05), rounded up to 60 ms (Implement 5: >= 50).
SETTLE_S = 0.06
TRIGGER_WIDTH_MS = 50
SUBDIVISION = 1000
MAX_RPM = 300
REL_BOUND, ABS_BOUND = 4000, 8000
# W0 (bench step 2): record 1 = [0x0001,0x0000] at 300 rpm; records 2-16 = +100 at 15 rpm.
W0_WORDS, W0_OTHER_WORDS = [0x0001, 0x0000], [0x0000, 0x0064]
W0_RPM, W0_OTHER_RPM, W0_GUARD = 300, 15, 400
W0_ORDER0, W0_SWAPPED = 65536, 1
BASE_TARGET_WORDS = [0x0000, 0x1388]
# The path band (W0 guard): the drive may start without delay and reach full
# speed at once (upper edge), or run up to PATH_LAG_S behind (function 7 start
# delay 13-15 ms, wired bench 09; function 12's is unmeasured; a 100 ms ramp of
# unknown reference speed; margin; lower edge); each edge +-W0_GUARD. A 15 rpm
# mis-selection (250 increments/s) leaves the band within about 0.6 s.
PATH_LAG_S = 0.4
ENDPOINT_TOLERANCE = 2   # P1 rule: within +-2 increments (drive feedback offsets of 1-2, wired bench 09 E1).
POST_S = 2.5             # After the trigger OFF the context watches 2 s for a further start.

CASES = {
    "W0a": dict(kind="w0a", levels=["drive-reported"],
                assertion="record 1 [0x0001,0x0000] started by its trigger moves +65536 (word order 0) or +1 (swapped)"),
    "W0b": dict(kind="not-run", levels=["drive-reported"],
                assertion="record 1 [0xFFFF,0xFF38] moves -200 (two's complement)"),
    "P1": dict(kind="table", mode=0, levels=["drive-reported"],
               assertion="records 1-8 relative each move their own target within +-2"),
    # P2 ran on image C (wired bench 13): with 0x0050 = 1 every record moved its
    # target from the trigger position, so the arm now refuses absolute (P2r).
    "P2": dict(kind="table", mode=1, repeat=3, levels=["drive-reported"],
               assertion="records 1-8 absolute reach their targets; a repeated trigger moves 0"),
    "P2r": dict(kind="absolute", levels=["sent"],
                assertion="arm segment absolute refuses pt_absolute_unresolved before any frame or pulse"),
    "P3": dict(kind="sentinel", levels=["drive-reported"], assertion="no record 9-16 executes in P1/P2"),
    "P5": dict(kind="held", record=2, width_ms=2000, levels=["drive-reported"],
               assertion="a 2000 ms trigger starts one run or recorded repeats"),
    "P8": dict(kind="stop", record=8, levels=["drive-reported", "controller-timestamped"],
               assertion="stop normal and stop fast mid-record stop it; PINs and trigger drop; nothing restarts"),
    "P9": dict(kind="group", mode=0, levels=["drive-reported"],
               assertion="records 1-8 on 1, 2, 4 at once reach each drive's own table"),
    "P10": dict(kind="group-stop", record=8, levels=["drive-reported"],
                assertion="group stop mid-record stops every member; PINs drop; nothing restarts"),
    "P12": dict(kind="ceiling", levels=["sent"], assertion="a record at 2500 rpm refuses the arm before any frame or pulse"),
}
# Tables on the D10 path (bench step 3), only the non-negative claim: drive 2
# gets +200*N (its -400*N table needs the signed claim; NOT RUN).
TABLES = {1: {"step": 400, "rpm": 60}, 2: {"step": 200, "rpm": 90}, 4: {"step": 300, "rpm": 120}}


# --------------------------------------------------------------------------
# Analysis (pure; unit-tested)

def signed(words) -> int:
    value = (int(words[0]) << 16) | int(words[1])
    return value - (1 << 32) if value & 0x80000000 else value


def pin_levels(record: int) -> dict:
    """PIN output -> level for record 1..8 (bit b of record-1 on PIN b)."""
    if not 1 <= record <= 8:
        raise ValueError("records 1..8 only (three selectors)")
    return {o: bool(((record - 1) >> b) & 1) for b, o in enumerate(PIN_OUTPUTS)}


def selected_record(raw_inputs: int) -> int:
    """Record the X1..X3 status bits select (polarity 0: bit = level)."""
    return 1 + sum(((raw_inputs >> (1 + b)) & 1) << b for b in range(3))


def layout_problem(io_words: list) -> str | None:
    """Why a drive's I/O words are not the named three-selector layout, or None."""
    if not (isinstance(io_words, list) and len(io_words) == 9):
        return "io read lacks nine words"
    if io_words[0] != 0:
        return "input polarity %d is not 0" % io_words[0]
    want = [LAYOUT[t] for t in records.TERMINALS]
    if list(io_words[1:5]) != want:
        return "inputs %s are not the PT layout %s" % (list(io_words[1:5]), want)
    return None


def endpoints(targets: dict, position: int, mode: int) -> dict:
    """Endpoint of every stored record 1..16 the drive could start."""
    return {n: (t if mode else position + t) for n, t in targets.items()}


def selection_problem(targets: dict, position: int, mode: int, bounds: dict) -> str | None:
    """The first stored record whose endpoint leaves the travel bound, or None.
    ``bounds``: record -> (low, high) displacement for relative runs, or the
    absolute (low, high) target window for absolute runs."""
    if sorted(targets) != list(range(1, 17)):
        return "not every record 1-16 is known"
    for n, end in sorted(endpoints(targets, position, mode).items()):
        low, high = bounds[n]
        value = end if mode else end - position
        if not low <= value <= high:
            return "record %d endpoint %d (%s %d) is outside [%d, %d]" % (
                n, end, "target" if mode else "displacement", value, low, high)
    return None


def table_bounds(mode: int) -> dict:
    return {n: ((-ABS_BOUND, ABS_BOUND) if mode else (-REL_BOUND, REL_BOUND)) for n in range(1, 17)}


def w0_bounds() -> dict:
    """W0 (bench step 3 selection consequences): record 1 within its path, records 2-16 +100."""
    out = {n: (0, 100) for n in range(2, 17)}
    out[1] = (0, W0_ORDER0 + W0_GUARD)
    return out


def rate(rpm: int) -> float:
    return rpm * SUBDIVISION / 60.0


def path_band(t_low_s: float, t_high_s: float, displacement: int, rpm: int, guard: int = W0_GUARD) -> tuple:
    """(low, high) position change allowed for a sample bracketed [t_low_s, t_high_s]
    after the trigger ON, for a positive finite run of ``displacement`` at ``rpm``."""
    v = rate(rpm)
    high = min(displacement, v * max(0.0, t_high_s)) + guard
    low = min(displacement, v * max(0.0, t_low_s - PATH_LAG_S)) - guard
    return low, high


def guard_problem(change: int, t_low_s: float, t_high_s: float, displacement: int, rpm: int, guard: int = W0_GUARD):
    low, high = path_band(t_low_s, t_high_s, displacement, rpm, guard)
    if change < low or change > high:
        return "position change %d outside the path band [%d, %d] at %.3f..%.3f s" % (change, low, high, t_low_s, t_high_s)
    return None


def w0a_verdict(delta) -> tuple:
    """(verdict, order) of one W0a run from its position change."""
    if delta is None:
        return "indeterminate", None
    if abs(delta - W0_ORDER0) <= ENDPOINT_TOLERANCE:
        return "PASS", 0
    if abs(delta - W0_SWAPPED) <= ENDPOINT_TOLERANCE:
        return "PASS", 1
    return "FAIL", None


def fallback_gate(targets: dict, position: int, mode: int) -> str | None:
    """Fallback without D10 (bench step 3): every endpoint inside the bound and
    records 1-8 each moving at least 200. None when P1/P3 could run on speeds."""
    problem = selection_problem(targets, position, mode, table_bounds(mode))
    if problem:
        return problem
    short = [n for n in range(1, 9) if abs(endpoints(targets, position, mode)[n] - position) < 200]
    return "records %s move less than 200" % short if short else None


def run_verdict(run: dict) -> tuple:
    """PASS when the run started once and ended within tolerance of its expected
    change; a repeated absolute trigger passes when it moved nothing."""
    if run.get("refused"):
        return "FAIL", "refused: %s" % run["refused"]
    r = run.get("result") or {}
    change, expected = r.get("position_change"), run.get("expected_change")
    if run.get("repeat"):
        if change is None or abs(change) > ENDPOINT_TOLERANCE or r.get("running_observed"):
            return "FAIL", "repeat: change %s running %s outcome %s" % (change, r.get("running_observed"), r.get("outcome"))
        return "PASS", None
    if r.get("outcome") not in ("attributed",) or change is None or expected is None:
        return "FAIL", "outcome %s change %s expected %s" % (r.get("outcome"), change, expected)
    if abs(change - expected) > ENDPOINT_TOLERANCE:
        return "FAIL", "change %d, expected %d" % (change, expected)
    return "PASS", None


# --------------------------------------------------------------------------
# Live sequencing

class Live(starts.Live):
    """Run state of one PT run."""

    def __init__(self, console, run, baseline, args, timeout):
        starts.Live.__init__(self, console, run, baseline, args, timeout)
        self.facts = pt_facts()
        self.record["selection"] = dict(SELECTION)
        self.trials = []
        self.targets = {}  # address -> {record: target} from the latest reads


def pt_facts() -> dict:
    """Facts the scenario record states per drive and purpose."""
    assign = {"x0": ["triggers-inhibited"], "x1": ["triggers-inhibited", "protection-removal"],
              "x2": ["triggers-inhibited", "protection-removal"], "x3": ["triggers-inhibited"],
              "y0": ["indicator", "either"]}
    segment = {t: ["triggers-inhibited"] for t in records.TERMINALS}
    return {str(a): {"assign": dict(assign), "segment": dict(segment), "configure": dict(segment)} for a in DRIVES}


def write_field(live, address, key, value, index=None):
    """One ordinary write with its planned facts, read back; raises on anything else."""
    entry = {"drive": address, "key": key, "index": index, "candidate": value}
    steps, refusal = records.plan_entry(live.baseline, entry, live.args.image_level, live.facts, restore=False)
    if refusal:
        raise probe.BenchError("no ordinary route for %s: %s" % (key, refusal["reason"]))
    step = dict(steps[0], target=value, candidate=value)
    command, kwargs, text = records.write_call({"key": key, "index": index}, value, address)
    step["write"] = {"command": command, "kwargs": kwargs, "text": text}
    records.use_drive(live.console, address, timeout_s=live.timeout)
    outcome = records.apply_write(live.console, step, timeout_s=live.timeout)
    live.run.field(entry, baseline=step["baseline"])
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


def read_records(live, address) -> dict:
    """segment position 1..16 read; returns record -> raw words (speed, acc, dec, hi, lo)."""
    out = {}
    for n in range(1, 17):
        reply = live.checked("segment", driver_args=("position", str(n), "read"), address=address)
        out[n] = list(reply["observation"]["raw"][:5])
    live.targets[address] = {n: signed(w[3:5]) for n, w in out.items()}
    return out


def experiment_target(live, address, n, words):
    """W0 image only: one experiment pair write of record n (D4/D10 exception), read back."""
    records.use_drive(live.console, address, timeout_s=live.timeout)
    live.checked("read-config", address=address)
    state = live.checked("read-state", address=address)
    if not probe.drive_reported_stopped(state):
        raise probe.BenchError("drive %d is not reported stopped" % address)
    first = 0x0060 + 6 * (n - 1)
    reply = live.call("driver-experiment", host_args=("pair", str(first), str(words[0]), str(words[1])))
    row = {"record": n, "words": list(words), "admitted": reply.get("ok"), "result": reply.get("result")}
    if not reply.get("ok"):
        raise probe.BenchError("experiment write of record %d %s refused: %s" % (n, words, reply.get("result")))
    deadline = time.monotonic() + 5
    final = reply
    while final.get("pending") and time.monotonic() < deadline:
        time.sleep(.02)
        final = live.call("driver-experiment", host_args=("inspect",))
    row.update(final={k: final.get(k) for k in ("operation_id", "first", "words", "readback", "execution", "error",
                                                 "session_ok", "unsettled")})
    entry = {"drive": address, "key": "records.position.target", "index": n, "candidate": list(words)}
    live.run.field(entry, baseline=BASE_TARGET_WORDS)
    if final.get("pending") or final.get("unsettled") or final.get("execution") == "unknown" or not final.get("session_ok"):
        live.run.field(entry, uncertain=True, reason="experiment write of record %d uncertain or failed; not replayed" % n)
        raise probe.BenchError("experiment write of record %d %s: %s" % (n, words, row["final"]))
    live.run.field(entry, ending=list(final.get("readback") or []))
    live.events.append({"command": "experiment record %d" % n, **row})
    return row


def promoted_target(live, address, n, value):
    """Production image (after the W0 promotion): segment position N set target VALUE, read back."""
    entry = {"drive": address, "key": "records.position.target", "index": n,
             "candidate": [(value >> 16) & 0xFFFF, value & 0xFFFF]}
    steps, refusal = records.plan_entry(live.baseline, entry, live.args.image_level, live.facts, restore=False)
    if refusal:
        raise probe.BenchError("no route for record %d target: %s" % (n, refusal["reason"]))
    step = dict(steps[0], target=entry["candidate"], candidate=entry["candidate"])
    command, kwargs, text = records.write_call({"key": entry["key"], "index": n}, entry["candidate"], address)
    step["write"] = {"command": command, "kwargs": kwargs, "text": text}
    records.use_drive(live.console, address, timeout_s=live.timeout)
    outcome = records.apply_write(live.console, step, timeout_s=live.timeout)
    live.run.field(entry, baseline=step["baseline"])
    ending, _ = records.readback(live.console, step, timeout_s=live.timeout)
    live.events.append({"command": text, "ok": outcome["written"], "readback": ending})
    if outcome["uncertain"] or not outcome["written"] or ending != entry["candidate"]:
        live.run.field(entry, uncertain=outcome["uncertain"], reason="target write %s read back %s" % (value, ending))
        raise probe.BenchError("record %d target %d on %d: written %s uncertain %s readback %s" % (
            n, value, address, outcome["written"], outcome["uncertain"], ending))
    live.run.field(entry, ending=ending)


def io_words(live, address) -> list:
    return list(live.checked("io", driver_args=("read",), address=address)["observation"]["raw"])


def assign_layout(live, address):
    """Step 4: X1/X2 limits to 14/15 (protection removal), X0 1 -> 12, X3 0 -> 16, read back."""
    words = io_words(live, address)
    for terminal in ("x1", "x2", "x0", "x3"):
        if words[1 + records.TERMINALS.index(terminal)] != LAYOUT[terminal]:
            write_field(live, address, "io." + terminal, LAYOUT[terminal])
    problem = layout_problem(io_words(live, address))
    if problem:
        raise probe.BenchError("drive %d after assignment: %s" % (address, problem))
    # The interlock's session-level io/config agreement after the assignment.
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)


def restore_layout(live, address):
    """The trigger first (12 -> baseline), then X3, X1, X2 back to the baseline words."""
    words = io_words(live, address)
    for terminal in ("x0", "x3", "x1", "x2"):
        field = records.baseline_field(live.baseline, address, "io." + terminal)
        if words[1 + records.TERMINALS.index(terminal)] != field["value"]:
            write_field(live, address, "io." + terminal, field["value"])
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)


def outputs_off(live):
    live.call("bench", host_args=("off",))


def select_record(live, record, members):
    """PIN outputs for ``record`` through the interlock, named selection members,
    then at least SETTLE_S and a read state of every member showing the levels.
    Returns {"changed": [...], "last_change_us": int|None, "levels": {...}}."""
    want = pin_levels(record)
    view = live.view()
    current = {o["name"]: o["active"] for o in view["outputs"]}
    if current.get(TRIGGER_OUTPUT):
        raise probe.BenchError("the PT trigger output is on")
    out = {"record": record, "changed": [], "last_change_us": None}
    for output in PIN_OUTPUTS:
        if current.get(output) and not want[output]:
            reply = live.checked("bench", host_args=("off", output))
            live.views.append(reply)
            out["changed"].append({"output": output, "on": False, "at_us": reply.get("now_us")})
            out["last_change_us"] = reply.get("now_us")
    for b, output in enumerate(PIN_OUTPUTS):
        if want[output] and not current.get(output):
            named = [{"address": a, "output": output, "terminal": PIN_TERMINALS[b], "function": 14 + b, "polarity": 0,
                      "effect": records.SELECT_EFFECT, "trigger_terminal": TRIGGER_TERMINAL} for a in members]
            live.record["running_members"] = named
            try:
                reply = records.bench_on(live.console, output, 60000, live.record, timeout_s=live.timeout)
            finally:
                live.record["running_members"] = []
            live.views.append(reply)
            on_us = records._bench_view(reply)["outputs"][output]["on_us"]
            out["changed"].append({"output": output, "on": True, "at_us": on_us})
            out["last_change_us"] = on_us
    time.sleep(SETTLE_S)
    out["levels"] = {}
    for address in members:
        s = live.read_state(address)
        got = selected_record(s["inputs"]) if s else None
        out["levels"][address] = {"inputs": s["inputs"] if s else None, "record": got}
        if got != record:
            raise probe.BenchError("drive %d PIN levels select record %s, want %d (inputs %s); no trigger" % (
                address, got, record, s["inputs"] if s else None))
    return out


def arm_reads(live, address, record, group=False):
    """The fresh reads and FINITE_MOVE records of one member (the arm needs them).
    Returns the record words and the feedback position."""
    if not group:
        records.use_drive(live.console, address, timeout_s=live.timeout)
    live.checked("driver", driver_args=("read",), address=address)
    if not group:
        live.checked("read-config", address=address)
    words = live.checked("segment", driver_args=("position", str(record), "read"), address=address)["observation"]["raw"][:5]
    s = live.read_state(address)
    for i, terminal in enumerate(records.TERMINALS):
        tokens = (("member",) if group else ()) + (str(address), terminal, str(LAYOUT[terminal]), "0", "finite-move",
                                                   "unknown", "triggers-inhibited")
        live.checked("io-qualification-install", host_args=tokens)
    return list(words), stops.position(s) if s else None


def headroom(live):
    """Wired bench 12 lesson: a trial starts only with evidence headroom left."""
    problem = bench_switch.headroom_problem(live.console)
    if problem:
        raise probe.BenchError(problem)


def prepared_stop(live, members, group):
    """The prepared stop: group stop fast, or stop fast per member; every output drops first."""
    if group:
        return live.call("group-stop", host_args=("fast",), timeout_s=max(live.timeout, 10))
    return [live.call("stop", stop_policy="fast", address=a) for a in members]


def pt_trial(live, record, mode, members, *, group=False, width_ms=TRIGGER_WIDTH_MS, bounds=None, guard=None,
             window_ms=10000, hook=None, poll_ms=50):
    """One PT run of ``record`` on ``members``; returns the trial record.
    ``guard`` = (displacement, rpm) turns on the W0 path guard for the first member."""
    headroom(live)
    trial = {"record": record, "mode": mode, "members": list(members), "group": group, "width_ms": width_ms}
    trial["selection"] = select_record(live, record, members)
    reads, positions = {}, {}
    if group:
        live.checked("group-read-state", timeout_s=max(live.timeout, 10))
    for address in members:
        reads[address], positions[address] = arm_reads(live, address, record, group=group)
        targets = dict(live.targets.get(address) or {})
        targets[record] = signed(reads[address][3:5])
        live.targets[address] = targets
        problem = selection_problem(targets, positions[address], mode, bounds or table_bounds(mode))
        if problem:
            trial["refused"] = "selection consequence on drive %d: %s" % (address, problem)
            return trial
    trial["record_words"], trial["start"] = reads, positions
    tokens = [TRIGGER_TERMINAL, "absolute" if mode else "relative", str(record)] + (["group"] if group else []) + \
        ["poll-ms", str(poll_ms), "window-ms", str(window_ms)]
    reply = live.call("arm-segment", host_args=tuple(tokens))
    if not reply.get("ok"):
        trial["refused"] = "arm %s %s (drive %s)" % (reply.get("result"), reply.get("reason"), reply.get("refused_address"))
        return trial
    session = reply["session"]["operation_id"]
    trial["session_id"] = session
    trial["armed"] = [{k: m.get(k) for k in ("address", "record", "record_words", "record_target", "expected_displacement",
                                             "expected_endpoint", "baseline_position", "selector_terminals")}
                      for m in reply["session"].get("members", [])]
    trial["expected"] = {m["address"]: (m["expected_displacement"] if not mode else m["expected_endpoint"] - m["baseline_position"])
                         for m in reply["session"].get("members", [])}
    try:
        live.view()
        trigger = live.call("arm-trigger", host_args=(str(session), TRIGGER_OUTPUT, str(width_ms)))
        trial["trigger"] = {k: trigger.get(k) for k in ("ok", "result", "reason")}
        decided = starts.wait_phase(live, session, lambda s: s["phase"] != "triggering", 5.0)
        isolation = decided.get("isolation") or {}
        trial["isolation"] = {"admitted": isolation.get("admitted"), "row": isolation.get("row"),
                              "reason": isolation.get("reason"),
                              "rows": [{k: r.get(k) for k in ("address", "member", "row", "reason", "io", "state")}
                                       for r in isolation.get("rows", [])]}
        if not trigger.get("ok") or not isolation.get("admitted"):
            trial["refused"] = "trigger %s %s (%s %s)" % (trigger.get("result"), trigger.get("reason"),
                                                          isolation.get("row"), isolation.get("reason"))
            return trial
        on_us = (decided.get("source") or {}).get("on_us")
        trial["on_us"] = on_us
        if trial["selection"].get("last_change_us") and on_us:
            trial["pin_to_trigger_ms"] = round((on_us - trial["selection"]["last_change_us"]) / 1000.0, 1)
        began, timeline, s = time.monotonic(), [], decided
        limit = window_ms / 1000.0 + 5.0
        while True:
            s = starts.inspect(live, session)
            for m in s.get("members", []):
                r = m.get("result") or {}
                timeline.append({"address": m["address"], "change": r.get("position_change"),
                                 "request_us": r.get("last_request_us"), "reply_us": r.get("last_reply_us"),
                                 "outcome": r.get("outcome")})
                if guard and m["address"] == members[0] and r.get("known") and r.get("last_request_us", 0) >= on_us:
                    problem = guard_problem(r["position_change"], (r["last_request_us"] - on_us) / 1e6,
                                            (r["last_reply_us"] - on_us) / 1e6, guard[0], guard[1])
                    if problem:
                        trial["guard_stop"] = {"problem": problem, "stop": prepared_stop(live, members, group),
                                               "at_s": round(time.monotonic() - began, 3)}
                        raise probe.BenchError("path guard: " + problem)
            if hook is not None:
                hook(live, s, time.monotonic() - began, trial)
            if s["phase"] == "done" or time.monotonic() - began > limit:
                break
            time.sleep(0.12)
        trial["timeline"] = timeline
        final = starts.inspect(live, session)
        trial["session"] = final
        trial["results"] = {m["address"]: m.get("result") for m in final.get("members", [])}
    except BaseException:
        # A harness failure must never leave a drive moving (wired bench 12 deviation 1).
        if "guard_stop" not in trial:
            try:
                trial["exception_stop"] = prepared_stop(live, members, group)
            except Exception as error:  # noqa: BLE001 - keep the original error
                trial["exception_stop"] = "failed: %s" % error
        raise
    finally:
        trial["finish"] = starts.finish_session(live, session)
        outputs_off(live)
        moving, after = starts.settle_stopped(live, members, group=group)
        if moving:
            trial["continued_motion"] = moving
        trial["after"] = {a: (stops.position(x) if x else None) for a, x in after.items()}
    return trial


def return_to(live, address, target, rpm=300):
    """Serial absolute move back to the trial start (bench step 3)."""
    records.use_drive(live.console, address, timeout_s=live.timeout)
    _motion_ready(live, address)
    handle = live.console.begin("move-absolute", move_args=(str(target), "steps", "native", str(rpm), "configured"),
                                address=address, timeout_s=max(live.timeout, 40))
    row = {"target": target, "admitted": handle.accepted}
    t = live.console.wait(handle, release=True)
    row["terminal"] = {k: t.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion", "uncertain")}
    s = live.read_state(address)
    row["position"] = stops.position(s) if s else None
    if not handle.accepted or not (t.get("ok")):
        raise probe.BenchError("return move of drive %d to %s failed: %s" % (address, target, row))
    return row


def _motion_ready(live, address):
    """Config, state, a profile read and the start-speed relation of the selected drive."""
    import bench_motion as motion
    live.checked("read-config", address=address)
    live.read_state(address)
    live.checked("motion-profile", host_args=("forget",))
    profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
    word = str(profile["current"][0])
    live.call("motion-profile-start-speed-relation", host_args=(word, word))
    # FINITE_MOVE records for the functions the drive holds now (the layout or the baseline words).
    words = io_words(live, address)
    for i, terminal in enumerate(records.TERMINALS):
        live.checked("io-qualification-install", host_args=(str(address), terminal, str(words[1 + i]), "0",
                                                            "finite-move", "unknown", "triggers-inhibited"))


# --------------------------------------------------------------------------
# Cases

def w0a(live, address, out):
    """W0a on one drive into ``out`` (kept if a later step raises): snapshot, W0
    words through the experiment builder (X0 not 12 yet), speeds, layout, one
    guarded run, layout restored, return to the start."""
    out["address"] = address
    records.use_drive(live.console, address, timeout_s=live.timeout)
    out["before"] = read_records(live, address)
    out["driver"] = live.checked("driver", driver_args=("read",), address=address)["observation"]["raw"]
    if out["driver"][1] != SUBDIVISION or out["driver"][2] != 0 or out["driver"][3] != 0:
        raise probe.BenchError("drive %d subdivision/0x0019/0x0018 %s are not 1000/0/0" % (address, out["driver"][1:4]))
    if out["driver"][6] != 0:
        write_field(live, address, "drive.position-mode", 0)
    for n in range(1, 17):
        want = W0_WORDS if n == 1 else W0_OTHER_WORDS
        if out["before"][n][3:5] != want:
            experiment_target(live, address, n, want)
        rpm = W0_RPM if n == 1 else W0_OTHER_RPM
        if out["before"][n][0] != rpm:
            write_field(live, address, "records.position.speed", rpm, index=n)
    out["after_writes"] = read_records(live, address)
    assign_layout(live, address)
    try:
        trial = pt_trial(live, 1, 0, [address], bounds=w0_bounds(), guard=(W0_ORDER0, W0_RPM), window_ms=25000)
        out["trial"] = trial
        r = (trial.get("results") or {}).get(address) or {}
        out["delta"] = r.get("position_change")
        out["verdict"], out["order"] = w0a_verdict(out["delta"]) if not trial.get("refused") else ("FAIL", None)
    finally:
        restore_layout(live, address)
        for n in range(1, 17):
            entry = {"drive": address, "key": "records.position.target", "index": n}
            if (address, "records.position.target", n) in live.run.fields:
                live.run.field(entry, reason="left changed (RAM): W0 words; restored after the promotion by the promoted "
                                             "setter, or by the experiment baseline candidate if W0 fails (bench step 2)")
    start = (out.get("trial") or {}).get("start", {}).get(address)
    if start is not None and not out["trial"].get("refused"):
        out["return"] = return_to(live, address, start)
    return out


def setup_table(live, address, mode):
    """D10 path tables: records 1-8 = step*N at the drive's speed (promoted setter);
    records 9-16 keep W0's +100 at 15 rpm (read and checked); 0x0050; layout."""
    table = TABLES[address]
    before = read_records(live, address)
    for n in range(1, 17):
        target = table["step"] * n if n <= 8 else 100
        rpm = table["rpm"] if n <= 8 else W0_OTHER_RPM
        if signed(before[n][3:5]) != target:
            promoted_target(live, address, n, target)
        if before[n][0] != rpm:
            write_field(live, address, "records.position.speed", rpm, index=n)
    words = live.checked("driver", driver_args=("read",), address=address)["observation"]["raw"]
    if words[6] != mode:
        write_field(live, address, "drive.position-mode", mode)
    read_records(live, address)
    assign_layout(live, address)


def run_table(live, address, mode, repeat=None):
    """Records 1-8 once each on one drive (relative or absolute), each run
    returned to its start (relative) by a serial absolute move."""
    runs = []
    order = list(range(1, 9)) + ([repeat] if repeat else [])
    for index, n in enumerate(order):
        trial = pt_trial(live, n, mode, [address])
        trial["index"] = index + 1
        trial["expected_change"] = (trial.get("expected") or {}).get(address)
        trial["repeat"] = repeat is not None and index == len(order) - 1
        trial["verdict"] = run_verdict(dict(trial, result=(trial.get("results") or {}).get(address)))
        runs.append(trial)
        if trial.get("continued_motion"):
            raise probe.BenchError("motion continued after a PT run; prepared stop sent")
        if mode == 0 and trial.get("start", {}).get(address) is not None and not trial.get("refused"):
            trial["return"] = return_to(live, address, trial["start"][address])
    return runs


def move_near_zero(live, addresses):
    """Absolute runs: bring each drive within 1000 of 0 first (serial absolute move, 300 rpm)."""
    rows = []
    for address in addresses:
        records.use_drive(live.console, address, timeout_s=live.timeout)
        s = live.read_state(address)
        position = stops.position(s) if s else None
        if position is None:
            raise probe.BenchError("drive %d position unknown" % address)
        if abs(position) > 1000:
            rows.append(dict(return_to(live, address, 0), address=address, start=position))
    return rows


def run_held(live, address, record, width_ms):
    """P5: one trigger held ``width_ms``; runs are counted from RUNNING episodes."""
    trial = pt_trial(live, record, 0, [address], width_ms=width_ms, window_ms=12000)
    trial["expected_change"] = (trial.get("expected") or {}).get(address)
    r = (trial.get("results") or {}).get(address) or {}
    trial["held"] = {"outcome": r.get("outcome"), "further": r.get("further"), "change": r.get("position_change"),
                     "expected": trial["expected_change"],
                     "repeats": None if r.get("position_change") is None or not trial["expected_change"] else
                     round(r["position_change"] / trial["expected_change"], 2)}
    if trial.get("start", {}).get(address) is not None and not trial.get("refused"):
        trial["return"] = return_to(live, address, trial["start"][address])
    return trial


def stop_hook(policy, at_s, members, group):
    """A stop request at ``at_s`` after the observation began, once a RUNNING sample was seen."""
    def hook(live, s, since, trial):
        if trial.get("stop_sent") or since < at_s:
            return
        seen = all(((m.get("result") or {}).get("running_observed")) for m in s.get("members", []))
        if not seen and since < at_s + 1.0:
            return
        view = live.view()
        if group:
            reply = live.call("group-stop", host_args=(policy,), timeout_s=max(live.timeout, 10))
        else:
            reply = live.call("stop", stop_policy=policy, address=members[0])
        after = live.view()
        trial["stop_sent"] = {"policy": policy, "at_s": round(since, 3), "running_seen": seen, "view_us": view.get("now_us"),
                              "reply": {k: (reply.get(k) if isinstance(reply, dict) else None)
                                        for k in ("ok", "result", "outcome", "operation_id")},
                              "outputs_after": {o["name"]: {"active": o["active"], "off_reason": o.get("off_reason"),
                                                            "off_us": o.get("off_us")} for o in after.get("outputs", [])}}
    return hook


def stop_verdict(trial, members) -> tuple:
    """Stopped short with the stop recorded, every output off after the stop, no further start."""
    sent = trial.get("stop_sent")
    if trial.get("refused") or not sent:
        return "FAIL", "no stop sent: %s" % trial.get("refused")
    problems = []
    for address in members:
        r = (trial.get("results") or {}).get(address) or {}
        change, expected = r.get("position_change"), (trial.get("expected") or {}).get(address)
        if r.get("outcome") != "stopped" or r.get("further") == "seen":
            problems.append("drive %d outcome %s further %s" % (address, r.get("outcome"), r.get("further")))
        if change is None or expected is None or not 0 < change < expected:
            problems.append("drive %d change %s of %s is not short" % (address, change, expected))
    on = [n for n, o in sent["outputs_after"].items() if o["active"]]
    if on:
        problems.append("outputs still on after the stop: %s" % on)
    if trial.get("continued_motion"):
        problems.append("continued motion %s" % trial["continued_motion"])
    return ("FAIL", "; ".join(problems)) if problems else ("PASS", None)


def run_ceiling(live, address):
    """P12: record 1 at 2500 rpm (storage admitted 0..3000); the arm refuses before
    any frame or pulse; record 1 back to its table speed."""
    out = {}
    before = read_records(live, address)[1][0]
    write_field(live, address, "records.position.speed", 2500, index=1)
    try:
        out["selection"] = select_record(live, 1, [address])
        arm_reads(live, address, 1)
        debug = live.checked("debug", host_args=())
        view = live.view()
        ons = {o["name"]: o["ons"] for o in view["outputs"]}
        drv = live.checked("drv")
        reply = live.call("arm-segment", host_args=(TRIGGER_TERMINAL, "relative", "1"))
        drv_after = live.checked("drv")
        view_after = live.view()
        out.update(reply={k: reply.get(k) for k in ("ok", "result", "reason", "bus_traffic", "operation_id")},
                   latest_before=drv.get("operation_id"), latest_after=drv_after.get("operation_id"),
                   ons_before=ons, ons_after={o["name"]: o["ons"] for o in view_after["outputs"]},
                   debug_mode=debug.get("mode"))
    finally:
        outputs_off(live)
        write_field(live, address, "records.position.speed", before, index=1)
    return out


def declare_group(live):
    """group set 1 2 4 with the member references revoked. The return moves select
    each drive in turn, which ends the declaration's binding: declare it per trial."""
    live.checked("group-set", host_args=tuple(str(a) for a in DRIVES), timeout_s=max(live.timeout, 10))
    for address in DRIVES:
        live.call("group-reference", host_args=(str(address), "revoke"))


def run_group(live, records_order, trials, hook=None):
    """P9/P10 into ``trials`` (kept if a later trial raises): drives 1, 2, 4 as one
    group, one relative record per trial for every member, each member returned
    to its start by a serial absolute move."""
    for n in records_order:
        declare_group(live)
        trial = pt_trial(live, n, 0, DRIVES, group=True, hook=hook, window_ms=12000)
        trial["verdicts"] = {a: run_verdict(dict(trial, expected_change=(trial.get("expected") or {}).get(a),
                                                 result=(trial.get("results") or {}).get(a))) for a in DRIVES}
        trials.append(trial)
        if hook is not None:
            trial["verdict"] = stop_verdict(trial, DRIVES)
        if trial.get("continued_motion"):
            raise probe.BenchError("motion continued after a group PT run; prepared stop sent")
        if not trial.get("refused"):
            live.call("group-clear")
            trial["returns"] = {a: return_to(live, a, trial["start"][a]).get("position") for a in DRIVES
                                if (trial.get("start") or {}).get(a) is not None}
    return trials


def run_absolute(live, address):
    """P2r: an absolute PT arm refuses before any frame or trigger pulse."""
    records.use_drive(live.console, address, timeout_s=live.timeout)
    view = live.view()
    ons = {o["name"]: o["ons"] for o in view["outputs"]}
    drv = live.checked("drv")
    reply = live.call("arm-segment", host_args=(TRIGGER_TERMINAL, "absolute", "1"))
    drv_after = live.checked("drv")
    after = live.view()
    return {"reply": {k: reply.get(k) for k in ("ok", "result", "reason", "bus_traffic", "operation_id")},
            "latest_before": drv.get("operation_id"), "latest_after": drv_after.get("operation_id"),
            "ons_before": ons, "ons_after": {o["name"]: o["ons"] for o in after["outputs"]}}


def run_cases(live, args, cases):
    detail = {"cases": {}, "address": args.address}
    live.partial = detail["cases"]
    live.record["input_agreement"] = records.check_input_agreement(live.console, timeout_s=live.timeout)
    for cid in cases:
        case = CASES[cid]
        out = detail["cases"][cid] = {"trials": []}
        kind = case["kind"]
        if kind == "w0a":
            for address in (args.drives or [args.address]):
                trial = {}
                out["trials"].append(trial)
                w0a(live, address, trial)
        elif kind == "table":
            setup_table(live, args.address, case["mode"])
            try:
                if case["mode"] == 1:
                    out["approach"] = move_near_zero(live, [args.address])
                out["trials"] = run_table(live, args.address, case["mode"], case.get("repeat"))
            finally:
                restore_layout(live, args.address)
        elif kind == "sentinel":
            out["trials"] = [t for c in ("P1", "P2") for t in (detail["cases"].get(c) or {}).get("trials", [])]
        elif kind == "held":
            setup_table(live, args.address, 0)
            try:
                out["trials"] = [run_held(live, args.address, case["record"], case["width_ms"])]
            finally:
                restore_layout(live, args.address)
        elif kind == "stop":
            setup_table(live, args.address, 0)
            try:
                for policy in ("normal", "fast"):
                    t = pt_trial(live, case["record"], 0, [args.address], hook=stop_hook(policy, 1.2, [args.address], False))
                    t["verdict"] = stop_verdict(t, [args.address])
                    out["trials"].append(t)
                    if t.get("start", {}).get(args.address) is not None:
                        t["return"] = return_to(live, args.address, t["start"][args.address])
            finally:
                restore_layout(live, args.address)
        elif kind in ("group", "group-stop"):
            for address in DRIVES:
                setup_table(live, address, 0)
            try:
                if kind == "group":
                    run_group(live, range(1, 9), out["trials"])
                else:
                    run_group(live, [case["record"]], out["trials"], hook=stop_hook("fast", 0.6, DRIVES, True))
            finally:
                live.call("group-clear")
                for address in DRIVES:
                    restore_layout(live, address)
        elif kind == "absolute":
            out["trials"] = [run_absolute(live, args.address)]
        elif kind == "ceiling":
            setup_table(live, args.address, 0)
            try:
                out["trials"] = [run_ceiling(live, args.address)]
            finally:
                restore_layout(live, args.address)
        else:
            out["note"] = "NOT RUN in this prompt"
    return detail


def case_verdicts(run, cid, detail, error):
    case = CASES[cid]
    out = (detail.get("cases") or {}).get(cid)
    if case["kind"] == "not-run":
        for level in case["levels"]:
            run.verdict(cid, level, "NOT RUN", "no drive-side stop on a free terminal (D15 safeguard for 13)", ["pt.json"])
        return
    if not out or not out.get("trials"):
        for level in case["levels"]:
            run.verdict(cid, level, "NOT RUN", "not reached: %s" % (error or (out or {}).get("note") or "no trial"), ["pt.json"])
        return
    trials = out["trials"]
    kind = case["kind"]
    if kind == "w0a":
        values = [t.get("verdict") for t in trials]
        verdict = "PASS" if values and all(v == "PASS" for v in values) and len({t.get("order") for t in trials}) == 1 else \
            "FAIL" if "FAIL" in values else "indeterminate"
        run.verdict(cid, "drive-reported", verdict, "; ".join("drive %s delta %s order %s" % (
            t.get("address"), t.get("delta"), t.get("order")) for t in trials), ["pt.json"])
        return
    if kind == "sentinel":
        bad = []
        for t in trials:
            r = (t.get("results") or {}).get(t["members"][0]) or {}
            change = r.get("position_change")
            if change is not None and not t.get("repeat") and abs(change - 100) <= ENDPOINT_TOLERANCE:
                bad.append(t["record"])
        run.verdict(cid, "drive-reported", "FAIL" if bad else "PASS",
                    "records %s moved +100 (sentinel)" % bad if bad else "%d runs, no +100 sentinel move" % len(trials),
                    ["pt.json"])
        return
    if kind == "held":
        t = trials[0]
        run.verdict(cid, "drive-reported", "PASS" if not t.get("refused") and t.get("held", {}).get("outcome") else "FAIL",
                    "held 2000 ms: %s" % t.get("held"), ["pt.json"])
        return
    if kind in ("stop", "group-stop"):
        values = [t.get("verdict", ("FAIL", "no verdict")) for t in trials]
        verdict = "PASS" if all(v[0] == "PASS" for v in values) else "FAIL"
        notes = "; ".join("%s: %s" % ((t.get("stop_sent") or {}).get("policy"), v[1]) for t, v in zip(trials, values) if v[1])
        for level in case["levels"]:
            run.verdict(cid, level, verdict, notes or "%d stops" % len(trials), ["pt.json"])
        return
    if kind == "group":
        bad = ["record %s drive %s: %s" % (t["record"], a, v[1]) for t in trials for a, v in t.get("verdicts", {}).items()
               if v[0] != "PASS"]
        run.verdict(cid, "drive-reported", "FAIL" if bad or len(trials) != 8 else "PASS",
                    "; ".join(bad) if bad else "%d records x %d drives" % (len(trials), len(DRIVES)), ["pt.json"])
        return
    if kind in ("ceiling", "absolute"):
        t = trials[0]
        reply = t.get("reply") or {}
        want = "record_rate_ceiling" if kind == "ceiling" else "pt_absolute_unresolved"
        ok = (reply.get("ok") is False and reply.get("reason") == want and reply.get("bus_traffic") is False
              and t.get("latest_before") == t.get("latest_after") and
              t.get("ons_before", {}).get(TRIGGER_OUTPUT) == t.get("ons_after", {}).get(TRIGGER_OUTPUT))
        run.verdict(cid, "sent", "PASS" if ok else "FAIL", "arm %s %s; latest operation %s -> %s; mosfet1 ons %s -> %s" % (
            reply.get("result"), reply.get("reason"), t.get("latest_before"), t.get("latest_after"),
            t.get("ons_before", {}).get(TRIGGER_OUTPUT), t.get("ons_after", {}).get(TRIGGER_OUTPUT)), ["pt.json"])
        return
    values = [t.get("verdict", ("FAIL", "no verdict"))[0] for t in trials]
    notes = ["record %s: %s" % (t["record"], t.get("verdict", ("", ""))[1]) for t in trials if t.get("verdict", ("FAIL",))[0] != "PASS"]
    verdict = "PASS" if values and all(v == "PASS" for v in values) else "FAIL"
    for level in case["levels"]:
        run.verdict(cid, level, verdict, "; ".join(notes) if notes else "%d runs" % len(trials), ["pt.json"])


def plan(args, baseline, facts):
    """Scenario-record fields of one pt-segments run (bench step 1), before any port access."""
    drives = sorted(set(args.drives or [args.address]) | (set(DRIVES) if {"P9", "P10"} & set(args.cases) else set()))
    write_list, left = [], []
    for address in drives:
        for terminal in records.TERMINALS:
            write_list.append({"drive": address, "key": "io." + terminal, "index": None, "candidate": LAYOUT[terminal]})
        write_list.append({"drive": address, "key": "drive.position-mode", "index": None, "candidate": 1})
        for n in range(1, 17):
            write_list.append({"drive": address, "key": "records.position.speed", "index": n, "candidate": 15})
            target = {"drive": address, "key": "records.position.target", "index": n, "candidate": [0, 100]}
            if int(args.image_level) >= 13:
                write_list.append(target)
            else:
                left.append(dict(target, storage="RAM", reason="W0 words through the experiment builder (D4/D10, AGENTS "
                                 "exception for 13 W0); restored after the promotion by the promoted setter, or by the "
                                 "experiment baseline candidate [0,5000] if W0 fails"))
        for key in ("profile.acceleration", "profile.deceleration", "profile.speed", "profile.target"):
            write_list.append({"drive": address, "key": key, "index": None, "candidate": 0})
    restoration = records.restoration_plan(baseline, write_list, args.image_level, facts)
    cases = [{"id": c, "required": True, "assertion": CASES[c]["assertion"], "required_levels": list(CASES[c]["levels"])}
             for c in args.cases]
    commands = ["io read, read config, read state of 1, 2, 4 (baseline I/O words required)",
                "W0 image: driver experiment pair FIRST W0 W1 per record (X0 not 12), segment position N set speed V",
                "production image: segment position N set target V (promoted, non-negative, order 0), speed, driver set "
                "position-mode, io set x1 14 / x2 15 / x0 12 / x3 16 with ASSIGN facts, each read back",
                "per trial: selection-consequence check over records 1-16; interlocked bench on of the PIN outputs (named "
                "selection members); 60 ms; read state shows the PIN levels; driver read, read config, segment position N "
                "read, read state, io qualification install x0..x3 finite-move; arm segment x0 MODE N [group]; arm trigger "
                "ID mosfet1 50; arm inspect loop with the path guard (W0) -> stop fast; bench off; settle",
                "relative runs: serial absolute move back to the trial start (300 rpm)",
                "restoration: layout back (x0 first), then the plan: records, position mode, profile words"]
    bounds = {"subdivision": SUBDIVISION, "record_rpm_max": MAX_RPM, "relative_bound": REL_BOUND, "absolute_bound": ABS_BOUND,
              "w0_guard": W0_GUARD, "w0_path_lag_s": PATH_LAG_S, "settle_s": SETTLE_S, "trigger_width_ms": TRIGGER_WIDTH_MS,
              "exposure": "the path guard and every stop run in the harness: if the harness, link or controller fails, a "
                          "W0a run continues to its own end (+65536 at 300 rpm, about 13.2 s; or the swapped +1) and a "
                          "table run to its record's end (at most 3200 increments); D11"}
    expected = {"drive-reported": "session samples (position change, RUNNING, completion, further start) per member and run",
                "controller-timestamped": "PIN and trigger GPIO times, stop request and output-off times",
                "sent": "P12: no operation and no trigger ON after the refused arm"}
    return write_list, restoration, cases, commands, bounds, expected, left
