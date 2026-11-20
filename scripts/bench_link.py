#!/usr/bin/env python3
"""Wired bench 18: link loss, controller reset, reflash and host faults.

One case per run folder (``build/bench/<date>_wb18_<slug>/``), several
console sessions per case where a controller reset, a reflash or a killed
harness process splits it. Every live step uses the ordinary console routes
through ``bench_probe.Console`` sessions (``bench_session.run_recorded``); the
fault itself happens between sessions, with the port closed:

- ``rts_reset``: a new handle with DTR low and RTS high for 150 ms resets the
  ESP32-S3 through its USB-Serial/JTAG (the bench_stress sequence);
- ``reflash``: the exact esptool command of the running image (G2);
- a child process that starts a velocity run and is killed (H2).

Cases (drive 4, drives 1 and 2 idle and read before and after):

- R0 regression on a new image: one finite move, the withdrawn persistence
  routes refuse before TX (runner frame count unchanged), `velocity limits`
  reports the stop budget.
- RV explicit ``recover`` during a velocity run: refused ``busy`` before any
  frame, the run ends with its own stop (item 3).
- H1 long finite move; the harness stops reading for 10 s, then resumes.
- H2 velocity run (baseline JOG words through a one-run cell); the harness
  process is killed mid-run; the run's own stop, read after reconnect.
- W1 velocity at the baseline JOG words, soft limits bound after home 35
  (the drive-side end if the controller never returns); RTS reset 0.3 s after
  the start; no traffic for T s; probe, read state, stop fast.
- W2 long finite move; RTS reset mid-move; the drive reaches the target alone.
- W3 drive stopped; RTS reset; a move before fresh reads (refused, zero TX);
  stop fast 4 (admitted without a probe); fresh reads.
- G1 finite move with X0-X3 = 5 (stop); RTS reset; motion or standstill.
- G2 G1 with a reflash of the same image instead of the reset.
- PR profile words of drives 1, 2 and 4 back to the baseline after the
  stress and saturation runs (stage route, read back).
- L1-L4, U1: refused before any traffic (deferred: owner away, D15).

Before every reset or reflash the session runs ``before_flash_check`` (all
three drives stopped, no input function 7-17); the drive under test then
starts its motion, which is the deliberate deviation of W1, W2, G1 and G2
(D11). Exposure and what ends motion are in each scenario record. No
uncertain write is replayed; every trial sends the prepared ``stop fast`` if
it raises.
"""
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports

import bench_limits as limits
import bench_probe as probe
import bench_records as records
import bench_stop_inputs as stops
import bench_velocity as velocity
from bench_scenarios import _Run, _finish, _preflight, _stop_restore
from bench_session import run_recorded

OWNER = records.FIXTURE["y0_owner"]
DRIVES = tuple(records.FIXTURE["input_drives"])
RUNNING, HOMED, ALARM, RELEASED, SOFT_POSITIVE, SOFT_NEGATIVE = 4, 2, 8, 16, 32, 64
JOG = (120, 100, 100)               # Drive 4 baseline 0x001D/3: the run stages exactly these words.
CELL_MS = 5000                      # Run duration of the one-run cells; the reset comes long before.
RESET_AFTER_S = 0.3                 # W1/G1 reset delay after the start reply.
SOFT_LIMIT = 150000                 # W1 bound: 75 s at 2000 increments/s, beyond the 60 s window.
HOME35 = ("35", "120", "60", "100")  # Search, return and ramp equal the baseline words; only 0x0031 differs.
HOME24 = ("24", "120", "60", "100")  # Stages the baseline homing words 24/120/60/100/0/0 exactly.
HOME_FACTS = ("mechanics", "rates", "ramp", "zero-offset", "auxiliary", "inputs")
STOP_INPUT = 5
MOVES = {"R0": (2000, 60), "H1": (15000, 60), "W2": (10000, 60), "G1": (12000, 60), "G2": (40000, 60)}
FRAME_COUNTER = "started"           # `stats` runner frames started (wired bench 17 refusals).
OWNER_ABSENT = "deferred: owner away (D15)"

CASES = {
    "R0": dict(levels=["drive-reported", "sent"], assertion="new image: +2000 at 60 rpm completes; `persistence marker read` "
               "replies marker_setter_unavailable and `persistence begin factory-restore` recommissioning_unavailable "
               "with the runner frame count unchanged; velocity limits report the stop budget"),
    "RV": dict(levels=["sent", "drive-reported"], assertion="velocity 60 rpm 1000 ms normal: `recover` while it runs "
               "replies busy with no frame and the binding unchanged; the run ends with its own stop, observed"),
    "H1": dict(levels=["drive-reported", "controller-timestamped"], assertion="+15000 at 60 rpm; no host reads for 10 s; the terminal "
               "arrives intact after the pause; output counters before and after"),
    "H2": dict(levels=["drive-reported", "sent"], assertion="velocity 120 rpm 5000 ms normal (cell) started by a child "
               "process killed 1.5 s later; after reconnect the run's terminal shows one acknowledged stop and standstill"),
    "W1": dict(levels=["drive-reported"], assertion="velocity 120 rpm (cell), soft limits +-150000 bound after home 35; "
               "RTS reset 0.3 s after the start; no traffic for T s; probe and read state show whether the drive still "
               "runs (the watchdog answer for T); stop fast; standstill"),
    "W2": dict(levels=["drive-reported"], assertion="+10000 at 60 rpm; RTS reset about 3 s in; final position equals the "
               "target within 2 increments; no alarm"),
    "W3": dict(levels=["sent", "drive-reported"], assertion="drive 4 stopped, interlock passed; RTS reset; the first "
               "request, a move, is refused before any frame; stop fast 4 is admitted without a probe; standstill"),
    "G1": dict(levels=["drive-reported"], assertion="X0-X3 = 5 on drive 4; +12000 at 60 rpm; RTS reset 0.3 s in; "
               "motion or standstill as observed; a pulse is credited only when natural completion, a drive fault and "
               "loss of communication are excluded"),
    "G2": dict(levels=["drive-reported"], assertion="X0-X3 = 5 on drive 4; +40000 at 60 rpm; reflash the same image with "
               "esptool; motion or standstill as observed over the download-mode window"),
    "PR": dict(levels=["read back"], assertion="profile words 0x0021/5 of drives 1, 2 and 4 written back to the baseline "
               "through the stage route after the stress and saturation runs, each read back"),
    "L1": dict(levels=["drive-reported"], deferred=True, assertion="owner pulls drive 4's RS485 plug for 10 s during velocity"),
    "L2": dict(levels=["drive-reported"], deferred=True, assertion="owner pulls the trunk at the controller for 10 s"),
    "L3": dict(levels=["drive-reported"], deferred=True, assertion="drive 4 plug pulled 5 s during a long move"),
    "L4": dict(levels=["drive-reported"], deferred=True, assertion="trunk pulled 5 s during a group move on 1, 2, 4"),
    "U1": dict(levels=["drive-reported"], deferred=True, assertion="owner unplugs USB for 10 s during motion"),
}
EXPOSURE = {
    "R0": "finite move's own end (2 s)", "RV": "the run's own timed stop (1 s plus its budget)",
    "H1": "finite move's own end (15 s)",
    "H2": "the controller's own end stop at 5 s; if it fails, until the next explicit stop",
    "W1": "rotation at 120 rpm until the rebooted controller's stop fast (boot + T + about 1 s); if it never "
          "returns, the drive's own soft limit at +150000 (75 s at 2000 increments/s)",
    "W2": "finite move's own end (10 s)", "W3": "none: the drive is stopped", "PR": "none: no motion",
    "G1": "finite move's own end (12 s)",
    "G2": "finite move's own end (40 s, outlasting the flash and boot); a failed flash leaves only the move's end",
}


# --------------------------------------------------------------------------
# Pure analysis (unit-tested)

def position(sample: dict | None):
    return None if not sample else stops.position(sample)


def watchdog_answer(window_s: float, after: dict | None, before: dict | None) -> dict:
    """W1 reading at the end of a silent window: running (with travel) or stopped,
    and whether a soft-limit flag explains a stop."""
    if not after:
        return {"window_s": window_s, "answer": "unreadable"}
    motion = after["motion"]
    travel = None if before is None or position(after) is None or position(before) is None else \
        position(after) - position(before)
    flags = bool(motion & (SOFT_POSITIVE | SOFT_NEGATIVE))
    if motion & RUNNING:
        answer = "running"
    elif flags:
        answer = "stopped_at_soft_limit"
    else:
        answer = "stopped"
    return {"window_s": window_s, "answer": answer, "travel": travel, "motion": motion, "alarm": after["alarm"],
            "speed": after["speed"]}


def move_outcome(start: int | None, steps: int, samples: list, tolerance: int = 2) -> dict:
    """Finite move across a controller fault: did it run to its target alone?"""
    finals = [s for s in samples if s]
    if start is None or not finals:
        return {"class": "unreadable"}
    last = finals[-1]
    travel = position(last) - start
    moving_seen = any(s["motion"] & RUNNING for s in finals)
    stopped = not last["motion"] & RUNNING and not last["speed"]
    if last["alarm"] or last["motion"] & ALARM:
        cls = "drive_fault"
    elif not stopped:
        cls = "still_running"
    elif abs(travel - steps) <= tolerance:
        cls = "completed"
    else:
        cls = "stopped_short"
    return {"class": cls, "travel": travel, "steps": steps, "moving_after_fault": moving_seen,
            "final_motion": last["motion"], "final_alarm": last["alarm"]}


def refusals_pass(rows: list) -> bool:
    """R0: both withdrawn routes refused with their named reason (in ``result``
    or ``reason``) and the runner frame count unchanged."""
    want = {"marker read": "marker_setter_unavailable", "factory restore begin": "recommissioning_unavailable"}
    return len(rows) == 2 and all(r.get("ok") is False and want.get(r["name"]) in (r.get("result"), r.get("reason")) and
                                  r["frames"][0] == r["frames"][1] for r in rows)


def pulse_credit(outcome: dict, marker_intact: bool, watchdog_ruled_out: bool) -> dict:
    """Item 4 rule: credit a stop-input pulse only when natural completion, a
    drive fault and loss of communication are all excluded."""
    cls = outcome.get("class")
    if cls == "completed":
        return {"verdict": "no_pulse_detected", "reason": "the move ran to its target: no pulse above the detection "
                "limit stopped it"}
    if cls == "stopped_short":
        if not marker_intact:
            return {"verdict": "indeterminate", "reason": "stopped short but the RAM marker changed (drive restart?)"}
        if not watchdog_ruled_out:
            return {"verdict": "indeterminate", "reason": "stopped short; loss of communication not excluded"}
        return {"verdict": "pulse_credited", "reason": "stopped short with alarm 0 and the RAM marker intact; W2 shows a "
                "move continues across a reset"}
    return {"verdict": "indeterminate", "reason": "move %s" % cls}


# --------------------------------------------------------------------------
# Live state shared by the sessions of one case

class Live(velocity.Live):
    """One case on drive 4: the sessions share run, events, samples and snapshots."""

    def __init__(self, run, baseline, args):
        args.address = OWNER
        velocity.Live.__init__(self, None, run, baseline, args, args.timeout)
        self.facts = _facts()
        self.detail = {}
        self.timeline = []

    def mark(self, event, **fields):
        self.timeline.append(dict(event=event, host_s=round(time.time(), 3), **fields))

    def stats(self):
        return self.console.command("stats", timeout_s=self.timeout).get(FRAME_COUNTER)


def session(folder: records.RunFolder, label: str, live: Live, work, *, after_snapshot=True, debug=None):
    """One console session of the case; ``work(console)`` runs inside it.
    The first session's files are the run folder's session files."""
    first = label == "session"
    paths = dict(summary=folder / ("summary.json" if first else label + "_summary.json"),
                 jsonl=folder / ("session.jsonl" if first else label + ".jsonl"),
                 console=folder / ("console.log" if first else label + "_console.log"))

    def run(console, record):
        live.console = console
        try:
            work(console)
        except BaseException:
            if console.synchronized:
                prepared_stop(live)
            raise
        record["workload_verified"] = True

    return run_recorded(port=live.args.port, scenario="link-loss", inputs={"case": live.args.case, "session": label},
                        run=run, address=OWNER, timeout_s=live.timeout, firmware=live.args.firmware, refresh=False,
                        debug=debug, paths=paths, after_snapshot=after_snapshot,
                        max_records=200000, max_bytes=64 * 1024 * 1024)


def prepared_stop(live):
    try:
        return live.call("stop", stop_policy="fast", address=OWNER)
    except Exception as error:  # A failed prepared stop is recorded; it is never retried here.
        live.mark("prepared stop failed", error=str(error))
        return {"ok": False}


def wait_port(port: str, present: bool, limit_s: float) -> float:
    """Seconds until the board's port is (or is no longer) enumerated."""
    began = time.monotonic()
    serial_number = records.FIXTURE["board"]["serial"]
    while time.monotonic() - began < limit_s:
        found = any(p.device == port and (p.serial_number or "").upper() == serial_number for p in list_ports.comports())
        if found == present:
            return time.monotonic() - began
        time.sleep(0.05)
    raise probe.BenchError("%s did not %s within %.0f s" % (port, "return" if present else "disappear", limit_s))


def rts_reset(live) -> dict:
    """Controller reset: DTR low, RTS high for 150 ms on a new handle (bench_stress)."""
    port = serial.Serial()
    port.port, port.baudrate = live.args.port, 115200
    port.dtr, port.rts = False, True
    t0 = time.time()
    port.open()
    time.sleep(0.15)
    port.rts = False
    time.sleep(0.05)
    try:
        port.close()
    except serial.SerialException:
        pass
    live.mark("rts reset", at=round(t0, 3))
    time.sleep(0.5)  # The port drops while the chip re-enumerates.
    back = wait_port(live.args.port, True, 15.0)
    time.sleep(2.0)  # Boot banner and console start (bench_stress waits 3 s).
    return {"reset_host_s": round(t0, 3), "port_back_s": round(back, 2)}


def reflash(live) -> dict:
    """The prepared esptool command of the running image (G2); its log stays in the folder."""
    flash = Path(live.args.flash_dir)
    files = {"0x0000": flash / "bootloader.bin", "0x8000": flash / "partitions.bin", "0xe000": flash / "boot_app0.bin",
             "0x10000": flash / live.args.firmware.name}
    command = [live.args.esptool, "--chip", "esp32s3", "--port", live.args.port, "--baud", "921600",
               "--before", "default-reset", "--after", "hard-reset", "write-flash", "-z", "--flash-mode", "dio",
               "--flash-freq", "80m", "--flash-size", "detect"]
    for offset, path in files.items():
        command += [offset, str(path)]
    t0 = time.time()
    live.mark("reflash start", command=command)
    with open(live.run.folder / "reflash.log", "w", encoding="utf-8") as log:
        code = subprocess.call(command, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    done = time.time()
    live.mark("reflash end", code=code)
    back = wait_port(live.args.port, True, 30.0)
    time.sleep(1.0)
    return {"command": command, "code": code, "flash_s": round(done - t0, 2), "start_host_s": round(t0, 3),
            "port_back_s": round(back, 2)}


# --------------------------------------------------------------------------
# Steps inside a session

def baseline_before(live, console):
    _preflight(console, live.run, live.timeout)
    declarations = records.check_declarations(console, timeout_s=live.timeout)
    before = {a: records.read_drive(console, a, ("io", "state", "config"), select=False, timeout_s=live.timeout)
              for a in DRIVES}
    diff = records.diff_fields(live.baseline, {a: records.raws_from(r) for a, r in before.items()}, ("io",))
    live.run.folder.write_json("baseline_before.json", {
        "declarations": declarations, "diff": diff,
        "drives": {str(a): {k: {x: e.get(x) for x in ("command", "operation_id", "raw", "error")} for k, e in r.items()}
                   for a, r in before.items()}})
    if declarations["verdict"] != "PASS" or any(d["status"] != "equal" for d in diff):
        raise probe.BenchError("I/O words differ from the baseline or declarations disagree; no write")


def interlock(live, label):
    check = records.before_flash_check(live.console, timeout_s=live.timeout)
    live.detail.setdefault("interlock", {})[label] = check
    if check["verdict"] != "PASS":
        raise probe.BenchError("before_flash_check %s: %s" % (label, [r["problems"] for r in check["rows"]]))
    return check


def state(live, address=OWNER):
    return live.read_state(address)


def start_move(live, steps, rpm):
    """Selected drive 4: config, profile snapshot, start-speed relation, pre-start reads, one move."""
    stops.motion_ready(live)
    stops.pre_start(live)
    before = live.samples[-1] if live.samples else None
    handle = live.console.begin("move-relative", move_args=(str(steps), "steps", "native", str(rpm), "configured"),
                                address=OWNER, timeout_s=max(live.timeout, steps / (rpm * 1000 / 60) + 10))
    live.mark("move begin", steps=steps, rpm=rpm, accepted=handle.accepted, operation_id=handle.operation_id)
    if not handle.accepted:
        raise probe.BenchError("move refused: %s" % live.console.wait(handle).get("result"))
    return handle, before


def start_velocity(live, duration_ms=CELL_MS):
    """Velocity at the baseline JOG words through a one-run cell (D6)."""
    rpm, acc, dec = JOG
    velocity.qualify_velocity(live, negative=False)
    sub = records.baseline_field(live.baseline, OWNER, "drive.subdivision")["value"]
    live.checked("velocity-limits", host_args=velocity.cell_tokens(rpm, acc, dec, sub, duration_ms, "normal"))
    stops.pre_start(live)
    before = live.samples[-1] if live.samples else None
    handle = live.console.begin("velocity", velocity_args=(str(rpm), "rpm", "native", str(duration_ms), "configured", "normal"),
                                address=OWNER, timeout_s=max(live.timeout, duration_ms / 1000 + 8))
    live.mark("velocity begin", accepted=handle.accepted, operation_id=handle.operation_id)
    if not handle.accepted:
        raise probe.BenchError("velocity refused: %s" % live.console.wait(handle).get("result"))
    return handle, before


def set_inputs(live, value_of):
    """X3 first, then X0-X2 (ASSIGN facts), each read back."""
    for terminal in ("x3", "x0", "x1", "x2"):
        velocity.write_field(live, "io." + terminal, value_of(terminal))


def soft_setup(live):
    """Auxiliary 7 and the pairs +-SOFT_LIMIT while 0x0018 reads 0 (prompt 11)."""
    live.checked("read-config", address=OWNER)
    h = live.checked("home-read", address=OWNER)["homing_parameters"]
    if h["raw_auxiliary"] != 7:
        limits.setting(live, "homing.auxiliary", 7)
    limits.setting(live, "drive.positive-limit", [SOFT_LIMIT >> 16, SOFT_LIMIT & 0xFFFF])
    negative = (-SOFT_LIMIT) & 0xFFFFFFFF
    limits.setting(live, "drive.negative-limit", [negative >> 16, negative & 0xFFFF])


def home(live, words, *, soft_limits):
    """One home with the stated facts; returns the terminal row."""
    limits.homed_low(live)
    live.checked("read-config", address=OWNER)
    live.checked("read-state", address=OWNER)
    live.checked("home-read", address=OWNER)
    facts = HOME_FACTS + (("reference", "soft-limits") if soft_limits else ())
    live.checked("home-qualification-install", host_args=(*words, *facts))
    return live.console.begin("home", home_args=(*words, "zero"), address=OWNER, timeout_s=max(live.timeout, 40))


def bind(live):
    """Disable if enabled, home 35 (120/60/100), driver read, the enable route (prompt 11)."""
    out = {"unbind": limits.unbind(live)}
    handle = home(live, HOME35, soft_limits=True)
    t = live.console.wait(handle, release=True)
    out["home"] = {k: t.get(k) for k in ("ok", "outcome", "completion", "uncertain")}
    s = limits.soft_state(live)
    if not t.get("ok") or not s or not s["homed"]:
        raise probe.BenchError("home 35 did not complete with HOMED: %s" % out["home"])
    live.checked("read-config", address=OWNER)
    live.checked("driver", driver_args=("read",), address=OWNER)
    out["enable"] = limits.setting(live, "drive.soft-limit", 1)
    live.checked("read-config", address=OWNER)
    out["axis"] = limits.axis_limits(live)
    return out


def restore_homing_words(live) -> dict:
    """home 24 120 60 100 stages the baseline homing words; the search is stopped
    at once (no origin edge is wanted). Read back with `home read`."""
    out = {}
    handle = home(live, HOME24, soft_limits=False)
    if not handle.accepted:
        raise probe.BenchError("home 24 refused: %s" % live.console.wait(handle).get("result"))
    began = time.monotonic()
    while time.monotonic() - began < 3.0:  # Until the trigger is on the wire (RUNNING) or the home ends.
        s = state(live)
        if handle.terminal is not None or (s and s["motion"] & RUNNING):
            break
    out["stop"] = prepared_stop(live).get("outcome")
    t = live.console.wait(handle, release=True)
    out["home"] = {k: t.get(k) for k in ("ok", "outcome", "completion", "status")}
    live.checked("read-config", address=OWNER)
    h = live.checked("home-read", address=OWNER)["homing_parameters"]
    out["words"] = [h["raw_method"], h["raw_search_speed"], h["raw_return_speed"], h["raw_ramp_time"], *h["raw_offset_words"]]
    out["stopped"] = probe.drive_reported_stopped(probe.observe_stopped(live.console, address=OWNER, timeout_s=live.timeout))
    return out


def raw_refusals(live, commands) -> list:
    """Console lines outside the harness grammar (the withdrawn marker route),
    through bench_stress's identity-checked raw console: `stats` runner frames
    before and after each line."""
    import bench_stress
    console = bench_stress.Console(live.args.port, live.run.folder.path)
    out = []
    try:
        console.pump(0.5)
        for name, line in commands:
            before = console.send("stats", 2.0).get(FRAME_COUNTER)
            reply = console.send(line, 2.0)
            after = console.send("stats", 2.0).get(FRAME_COUNTER)
            out.append({"name": name, "line": line, "ok": reply.get("ok"), "result": reply.get("result"),
                        "reason": reply.get("reason"), "frames": [before, after], "reply": reply})
    finally:
        console.close()
        with open(live.run.folder / "raw_console.log", "w", encoding="utf-8") as log:
            for when, text in console.ring:
                log.write("%.3f %s\n" % (when, text))
    return out


def post_boot(live):
    """After a reset or reflash: select drive 4 (no traffic), probe, read state."""
    records.use_drive(live.console, OWNER, timeout_s=live.timeout)
    live.checked("probe", address=OWNER)
    return state(live)


def sample_until_stopped(live, limit_s):
    out = []
    began = time.monotonic()
    while time.monotonic() - began < limit_s:
        s = state(live)
        out.append(s)
        if s and not s["motion"] & RUNNING and not s["speed"]:
            break
        time.sleep(0.25)
    return out


# --------------------------------------------------------------------------
# Cases

def case_r0(folder, live):
    d = live.detail

    def work(console):
        baseline_before(live, console)
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        steps, rpm = MOVES["R0"]
        handle, before = start_move(live, steps, rpm)
        t = console.wait(handle, release=True)
        after = state(live)
        d["move"] = {"ok": t.get("ok"), "outcome": t.get("outcome"), "completion": t.get("completion"),
                     "travel": None if not before or not after else position(after) - position(before)}
        d["velocity_limits"] = {k: console.command("velocity-limits", host_args=(), timeout_s=live.timeout).get(k)
                                for k in ("source", "max_rpm", "poll_ms", "stop_budget_ms")}
        _stop_restore(console, live, live.run, live.plan, live.timeout, owner=OWNER)
        _finish(console, live.run, live.timeout)

    session(folder, "session", live, work)
    d["refusals"] = raw_refusals(live, (("marker read", "persistence marker read"),
                                        ("factory restore begin", "persistence begin factory-restore")))
    move = d.get("move") or {}
    live.run.verdict("R0", "drive-reported", "PASS" if move.get("ok") and move.get("travel") in range(MOVES["R0"][0] - 2, MOVES["R0"][0] + 3)
                     else "FAIL", "move %s" % move, ["link.json"])
    r = d.get("refusals") or []
    sent = refusals_pass(r)
    live.run.verdict("R0", "sent", "PASS" if sent else "FAIL", "refusals %s; limits %s" % (
        [(x["name"], x["result"], x["reason"], x["frames"]) for x in r], d.get("velocity_limits")), ["link.json"])


def case_rv(folder, live):
    d = live.detail

    def work(console):
        baseline_before(live, console)
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        velocity.qualify_velocity(live, negative=False)
        stops.pre_start(live)
        handle = console.begin("velocity", velocity_args=("60", "rpm", "native", "1000", "configured", "normal"),
                               address=OWNER, timeout_s=max(live.timeout, 10))
        if not handle.accepted:
            raise probe.BenchError("velocity refused: %s" % console.wait(handle).get("result"))
        time.sleep(0.4)
        frames = live.stats()
        wiring = console.command("wiring", timeout_s=live.timeout)
        reply = console.command("recover", timeout_s=live.timeout)
        d["recover"] = {"ok": reply.get("ok"), "result": reply.get("result"), "frames": [frames, live.stats()],
                        "generation": [wiring.get("generation"), console.command("wiring", timeout_s=live.timeout).get("generation")]}
        t = console.wait(handle, release=True)
        d["run"] = {k: t.get(k) for k in ("ok", "outcome", "execution", "completion", "needs_stop", "stop_execution",
                                          "stop_completion", "stop_outcome")}
        d["after"] = state(live)
        _stop_restore(console, live, live.run, live.plan, live.timeout, owner=OWNER)
        _finish(console, live.run, live.timeout)

    session(folder, "session", live, work)
    rec, run = d.get("recover") or {}, d.get("run") or {}
    # The run's own 50 ms polls continue around the call, so the runner frame
    # count is no witness here; a recovery would bump the binding generation.
    sent = rec.get("result") == "busy" and rec.get("ok") is False and len(set(rec.get("generation") or [1, 2])) == 1
    live.run.verdict("RV", "sent", "PASS" if sent else "FAIL", "recover %s" % rec, ["link.json"])
    drive = run.get("ok") and run.get("stop_completion") == "observed" and d.get("after") and not d["after"]["motion"] & RUNNING
    live.run.verdict("RV", "drive-reported", "PASS" if drive else "FAIL", "run %s" % run, ["link.json"])


def case_h1(folder, live):
    d = live.detail

    def work(console):
        baseline_before(live, console)
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        d["drv_before"] = {k: console.command("drv", timeout_s=live.timeout).get(k)
                           for k in ("output_blocked", "output_short_writes", "input_dropped", "usb_stall")}
        steps, rpm = MOVES["H1"]
        handle, before = start_move(live, steps, rpm)
        live.mark("pause reads", seconds=10)
        time.sleep(10.0)  # No read of the port: the board's output backs up.
        live.mark("resume reads")
        t = console.wait(handle, release=True)
        after = state(live)
        d["move"] = {"ok": t.get("ok"), "outcome": t.get("outcome"), "completion": t.get("completion"),
                     "travel": None if not before or not after else position(after) - position(before)}
        d["drv_after"] = {k: console.command("drv", timeout_s=live.timeout).get(k)
                          for k in ("output_blocked", "output_short_writes", "input_dropped", "usb_stall")}
        _stop_restore(console, live, live.run, live.plan, live.timeout, owner=OWNER)
        _finish(console, live.run, live.timeout)

    session(folder, "session", live, work, debug="raw")
    move = d.get("move") or {}
    live.run.verdict("H1", "drive-reported", "PASS" if move.get("ok") and abs((move.get("travel") or 0) - MOVES["H1"][0]) <= 2
                     else "FAIL", "move %s" % move, ["link.json"])
    live.run.verdict("H1", "controller-timestamped", "PASS" if d.get("drv_after") else "FAIL",
                     "drv before %s after %s" % (d.get("drv_before"), d.get("drv_after")), ["link.json"])


CHILD = r'''
import sys
sys.path.insert(0, %(scripts)r)
import bench_link as link
link.child_velocity(%(folder)r, %(args)r)
'''


def child_velocity(folder_path, argv):
    """H2 child: one session that starts the velocity run, writes a marker and waits to be killed."""
    args = arguments(argv)
    folder = records.RunFolder.from_path(folder_path)
    run = _Run(json.loads((folder / "scenario.json").read_text(encoding="utf-8")), folder)
    live = Live(run, records.load_baseline(), args)

    def work(console):
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        handle, before = start_velocity(live)
        (folder / "child_started.json").write_text(json.dumps({"operation_id": handle.operation_id, "before": before,
                                                               "host_s": time.time()}), encoding="utf-8")
        time.sleep(60)  # Killed before this ends.

    session(folder, "child", live, work, after_snapshot=False)


def case_h2(folder, live):
    d = live.detail

    def setup(console):
        baseline_before(live, console)
        interlock(live, "before")

    session(folder, "session", live, setup)
    code = CHILD % {"scripts": str(Path(__file__).resolve().parent), "folder": str(folder.path),
                    "args": sys.argv[1:]}
    child = subprocess.Popen([sys.executable, "-c", code])
    marker = folder / "child_started.json"
    began = time.monotonic()
    while not marker.exists() and time.monotonic() - began < 60 and child.poll() is None:
        time.sleep(0.05)
    if not marker.exists():
        child.kill()
        raise probe.BenchError("H2 child did not start the run (exit %s)" % child.poll())
    started = json.loads(marker.read_text(encoding="utf-8"))
    time.sleep(1.5)
    child.kill()
    killed = time.time()
    child.wait(10)
    live.mark("child killed", at=round(killed, 3), after_start_s=round(killed - started["host_s"], 2))
    d["started"] = started
    time.sleep(max(0.0, started["host_s"] + CELL_MS / 1000 + 7.0 - time.time()))  # Past the run's end and budget.

    def after(console):
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        result = console.command("result", operation_id=started["operation_id"], timeout_s=live.timeout)
        d["run"] = {k: result.get(k) for k in ("ok", "pending", "outcome", "execution", "completion", "needs_stop",
                                               "stop_execution", "stop_completion", "stop_outcome", "steps_sent")}
        d["after"] = state(live)
        _stop_restore(console, live, live.run, live.plan, live.timeout, owner=OWNER)
        _finish(console, live.run, live.timeout)

    session(folder, "after", live, after)
    run, s = d.get("run") or {}, d.get("after")
    live.run.verdict("H2", "drive-reported", "PASS" if s and not s["motion"] & RUNNING and not s["speed"] and
                     run.get("stop_completion") == "observed" else "FAIL", "run %s; after %s" % (run, s), ["link.json"])
    live.run.verdict("H2", "sent", "PASS" if run.get("stop_execution") == "acknowledged" else "FAIL",
                     "stop execution %s (one FC06; the run never resends)" % run.get("stop_execution"), ["link.json"])


def case_w1(folder, live, windows):
    d = live.detail
    d["windows"] = []

    def setup(console):
        baseline_before(live, console)
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        soft_setup(live)

    session(folder, "session", live, setup)
    for n, window in enumerate(windows):
        row = {"window_s": window}
        d["windows"].append(row)

        def start(console, row=row):
            records.use_drive(console, OWNER, timeout_s=live.timeout)
            interlock(live, "w1_%d" % window)
            row["bind"] = bind(live)
            handle, before = start_velocity(live)
            row["before"] = before
            row["start_host_s"] = time.time()
            time.sleep(RESET_AFTER_S)

        session(folder, "start_%d" % n, live, start, after_snapshot=False)
        row["reset"] = rts_reset(live)
        time.sleep(max(0.0, row["reset"]["reset_host_s"] + window - time.time()))

        def end(console, row=row):
            row["end_host_s"] = time.time()
            s = post_boot(live)
            row["answer"] = watchdog_answer(window, s, row.get("before"))
            row["stop"] = prepared_stop(live).get("outcome")
            row["stopped"] = probe.drive_reported_stopped(probe.observe_stopped(console, address=OWNER, timeout_s=live.timeout))
            row["after"] = state(live)

        session(folder, "end_%d" % n, live, end)

    def restore(console):
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        d["unbind"] = limits.unbind(live)
        try:  # A refused or failed staging still lets the plan restore every other field.
            d["homing_words"] = restore_homing_words(live)
        except probe.BenchError as error:
            d["homing_words"] = {"error": str(error)}
            live.run.deviations.append("homing words not restored: %s" % error)
            prepared_stop(live)
        _stop_restore(console, live, live.run, live.plan, live.timeout, owner=OWNER)
        _finish(console, live.run, live.timeout)

    session(folder, "restore", live, restore)
    words = (d.get("homing_words") or {}).get("words")
    live.run.field({"drive": OWNER, "key": "homing.method", "index": None},
                   baseline=24, ending=words[0] if words else None, restored=bool(words) and words[0] == 24,
                   reason="home 35 staged 35/120/60/100/0/0 for the soft-limit bound; home 24 120 60 100 staged the "
                          "baseline words back (search stopped at once); read back %s" % words)
    notes, ok = [], bool(d["windows"])
    for row in d["windows"]:
        a = row.get("answer") or {}
        ok = ok and a.get("answer") in ("running", "stopped", "stopped_at_soft_limit") and row.get("stopped")
        notes.append("%s s: %s, travel %s (motion %s, alarm %s)" % (row["window_s"], a.get("answer"), a.get("travel"),
                                                                  a.get("motion"), a.get("alarm")))
    live.run.verdict("W1", "drive-reported", "PASS" if ok else "FAIL", "; ".join(notes), ["link.json"])


def reset_move_case(folder, live, cid, *, inputs=False, flash=False):
    """W2, G1, G2: a finite move across a reset or reflash."""
    d = live.detail
    steps, rpm = MOVES[cid]

    def start(console):
        baseline_before(live, console)
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        if inputs:
            set_inputs(live, lambda t: STOP_INPUT)
        interlock(live, "before")
        handle, before = start_move(live, steps, rpm)
        d["before"] = before
        d["start_host_s"] = time.time()
        time.sleep(3.0 if cid == "W2" else RESET_AFTER_S)

    session(folder, "session", live, start, after_snapshot=False)
    d["fault"] = reflash(live) if flash else rts_reset(live)

    def after(console):
        d["first"] = post_boot(live)
        d["first_host_s"] = time.time()
        d["samples"] = [d["first"]] + sample_until_stopped(live, steps / (rpm * 1000 / 60) + 10)
        io = live.checked("io", driver_args=("read",), address=OWNER)["observation"]["raw"]
        d["io_marker"] = io
        _stop_restore(console, live, live.run, live.plan, live.timeout, owner=OWNER)
        _finish(console, live.run, live.timeout)

    session(folder, "after", live, after)
    outcome = move_outcome(position(d.get("before")), steps, d.get("samples") or [])
    d["outcome"] = outcome
    if cid == "W2":
        live.run.verdict(cid, "drive-reported", "PASS" if outcome["class"] == "completed" else "FAIL",
                         "%s; first read %.1f s after the fault" % (outcome, d["first_host_s"] - d["fault"].get(
                             "reset_host_s", d["fault"].get("start_host_s", 0))), ["link.json"])
        return
    marker = d.get("io_marker") == [0, STOP_INPUT, STOP_INPUT, STOP_INPUT, STOP_INPUT, 0, 0, 0, 0] or \
        (isinstance(d.get("io_marker"), list) and d["io_marker"][1:5] == [STOP_INPUT] * 4)
    credit = pulse_credit(outcome, marker, watchdog_ruled_out=True)
    d["credit"] = credit
    verdict = "PASS" if credit["verdict"] in ("no_pulse_detected", "pulse_credited") else "indeterminate"
    live.run.verdict(cid, "drive-reported", verdict, "%s: %s; %s; I/O marker %s" % (
        credit["verdict"], credit["reason"], outcome, d.get("io_marker")), ["link.json"])


def case_pr(folder, live):
    """Profile words back to the baseline on 1, 2, 4 (stage route, read back).
    A stress run leaves `debug raw` on and its last profile session open (it
    never restores profiles); both are cleared first, with no bus traffic."""
    def work(console):
        console.command("debug", host_args=("off",), timeout_s=live.timeout)
        console.command("motion-profile", host_args=("forget",), timeout_s=live.timeout)
        baseline_before(live, console)
        _stop_restore(console, live, live.run, live.plan, live.timeout, owner=OWNER)
        _finish(console, live.run, live.timeout)

    session(folder, "session", live, work)
    # One stage step per drive writes and compares the whole 0x0021/5 window.
    fields = [f for f in live.run.fields.values() if f["key"].startswith("profile.")]
    ok = sorted(f["drive"] for f in fields) == sorted(DRIVES) and all(f["restored"] and not f["uncertain"] for f in fields)
    live.run.verdict("PR", "read back", "PASS" if ok else "FAIL",
                     "; ".join("%d %s %s" % (f["drive"], f["key"], f["ending"]) for f in fields), ["link.json"])


def case_w3(folder, live):
    d = live.detail

    def start(console):
        baseline_before(live, console)
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        interlock(live, "before")
        d["before"] = state(live)

    session(folder, "session", live, start, after_snapshot=False)
    d["fault"] = rts_reset(live)

    def after(console):
        records.use_drive(console, OWNER, timeout_s=live.timeout)
        frames = live.stats()
        handle = console.begin("move-relative", move_args=("1000", "steps", "native", "60", "configured"),
                               address=OWNER, timeout_s=live.timeout)
        refused = console.wait(handle) if not handle.accepted else None
        if handle.accepted:
            prepared_stop(live)
        d["move"] = {"accepted": handle.accepted, "result": (refused or {}).get("result"),
                     "frames": [frames, live.stats()]}
        frames = live.stats()
        stop = console.command("stop", stop_policy="fast", address=OWNER, timeout_s=live.timeout)
        d["stop"] = {k: stop.get(k) for k in ("ok", "outcome", "execution", "completion")}
        d["stop"]["frames"] = [frames, live.stats()]
        d["after"] = state(live)
        _stop_restore(console, live, live.run, live.plan, live.timeout, owner=OWNER)
        _finish(console, live.run, live.timeout)

    session(folder, "after", live, after)
    m, s = d.get("move") or {}, d.get("stop") or {}
    sent = m.get("accepted") is False and m.get("frames", [0, 1])[0] == m.get("frames", [0, 1])[1] and s.get("ok")
    live.run.verdict("W3", "sent", "PASS" if sent else "FAIL", "move %s; stop %s" % (m, s), ["link.json"])
    a = d.get("after")
    live.run.verdict("W3", "drive-reported", "PASS" if a and not a["motion"] & RUNNING and not a["speed"] and not a["alarm"]
                     else "FAIL", "after %s" % a, ["link.json"])


# --------------------------------------------------------------------------
# Plan, scenario record and main

def write_list(cid):
    keys = ("profile.acceleration", "profile.deceleration", "profile.speed", "profile.target")
    profile = [{"drive": OWNER, "key": k, "index": None, "candidate": None} for k in keys]
    if cid == "PR":
        return [{"drive": a, "key": k, "index": None, "candidate": None} for a in DRIVES for k in keys]
    out = []
    if cid in MOVES:
        out += profile
    if cid in ("G1", "G2"):
        out += [{"drive": OWNER, "key": "io." + t, "index": None, "candidate": STOP_INPUT} for t in records.TERMINALS]
    if cid == "W1":
        out += [{"drive": OWNER, "key": k, "index": None, "candidate": None}
                for k in ("homing.auxiliary", "drive.positive-limit", "drive.negative-limit", "drive.soft-limit")]
    return out


def scenario_record(args, image, commit, baseline):
    cid = args.case
    case = CASES[cid]
    entries = write_list(cid)
    plan = records.restoration_plan(baseline, entries, args.image_level, _facts())
    commands = {"R0": ["baseline_before; motion-profile read; start-speed relation; io read, read config, read state; "
                       "move relative 2000 steps native 60 configured; read state",
                       "stats; persistence marker read; stats; persistence begin factory-restore; stats; velocity limits"],
                "RV": ["velocity read; velocity qualification install rpm ramp inputs; pre-start reads; "
                       "velocity 60 rpm native 1000 configured normal; stats; wiring; recover; stats; wiring; read state"],
                "H1": ["debug raw; drv; move relative 15000 steps native 60 configured; no host read for 10 s; "
                       "the move terminal; read state; drv"],
                "H2": ["session: before_flash_check", "child: velocity read, qualification, velocity limits qualify "
                       "120 100 100 1000 5000 normal, velocity 120 rpm native 5000 configured normal; killed 1.5 s later",
                       "after 12 s: result ID; read state"],
                "W1": ["setup: home auxiliary 7, driver set positive-limit 150000, negative-limit -150000",
                       "per window T in %s: before_flash_check; soft-limit disable if set; home 35 120 60 100 zero with "
                       "reference and soft-limits facts; driver read; driver set soft-limit 1; velocity cell 120 100 100 "
                       "1000 5000 normal; velocity; 0.3 s; RTS reset; silence until T s after the reset; useaddr 4; probe; "
                       "read state; stop fast; read state" % list(args.windows),
                       "restore: soft-limit 0; home 24 120 60 100 zero, stopped at once; plan (pairs, auxiliary, profile)"],
                "W2": ["before_flash_check; move relative 10000 steps native 60 configured; 3 s; RTS reset; "
                       "useaddr 4; probe; read state until stopped"],
                "W3": ["before_flash_check; RTS reset; useaddr 4; stats; move relative 1000 (refused); stats; stop fast; "
                       "stats; read state"],
                "G1": ["io set x3, x0, x1, x2 5 with ASSIGN facts; before_flash_check; move relative 12000 steps native "
                       "60 configured; 0.3 s; RTS reset; useaddr 4; probe; read state until stopped; io read"],
                "G2": ["io set x3, x0, x1, x2 5; before_flash_check; move relative 40000 steps native 60 configured; "
                       "0.3 s; esptool write-flash of the running image; useaddr 4; probe; read state until stopped; io read"],
                "PR": ["per drive 1, 2, 4: motion-profile stage 100 100 60 0 5000 0 A with its fresh reads, read back"],
                }.get(cid, ["none: refused before any traffic (%s)" % OWNER_ABSENT])
    bounds = {"case": cid, "exposure": EXPOSURE.get(cid, "none"), "moves": MOVES.get(cid), "jog": JOG,
              "cell_ms": CELL_MS, "soft_limit": SOFT_LIMIT if cid == "W1" else None,
              "windows_s": list(args.windows) if cid == "W1" else None, "reset_after_s": RESET_AFTER_S}
    return dict(image=image, commit=commit, port=args.port,
                drives=[{"address": a, "required_enabled": True} for a in DRIVES],
                terminals=list(records.TERMINALS) if cid in ("G1", "G2") else [],
                commands=commands,
                cases=[{"id": cid, "required": True, "assertion": case["assertion"], "required_levels": case["levels"]}],
                running_members=[], write_list=entries, bounds=bounds,
                prepared_stop=["stop fast 4 (every bench output drops first); read state until stopped"],
                restoration_plan=plan, left_changed=[],
                expected_evidence={level: "link.json and the session logs" for level in case["levels"]},
                abort_conditions=["an alarm in any read", "a refused, failed or uncertain write (no replay)",
                                  "a failed before_flash_check (no reset, no flash)", "console framing or evidence failure"],
                cleanup=["stop fast 4", "restoration plan, read back", "bench outputs off", "ending reads", "close COM13"],
                max_age_ms=records.MAX_AGE_MS, facts=_facts(), image_level=args.image_level,
                fault={"reset": "RTS high 150 ms, DTR low (bench_stress)", "esptool": args.esptool,
                       "flash_dir": str(args.flash_dir) if args.flash_dir else None})


def _facts():
    both = ["triggers-inhibited", "protection-removal"]
    return {str(a): {"assign": {t: list(both) for t in records.TERMINALS} | {"y0": ["indicator", "either"]},
                     "configure": {t: ["triggers-inhibited"] for t in records.TERMINALS},
                     "segment": {t: ["triggers-inhibited"] for t in records.TERMINALS},
                     "start_speed_min_rpm": 30} for a in DRIVES}


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", default="COM13")
    parser.add_argument("--case", choices=tuple(CASES), required=True)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True, help="running image (SHA-256 recorded)")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--image-level", default="18")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--windows", type=float, nargs="+", default=[10.0], help="W1 silent windows (s)")
    parser.add_argument("--flash-dir", type=Path, help="G2: folder with bootloader, partitions, boot_app0 and the image")
    parser.add_argument("--esptool", default=r"C:\pio\penv\Scripts\esptool.exe")
    args = parser.parse_args(argv)
    if args.port != records.FIXTURE["board"]["port"]:
        parser.error("the wired bench is on %s" % records.FIXTURE["board"]["port"])
    if args.case == "G2" and not args.flash_dir:
        parser.error("G2 needs --flash-dir with the prepared image files")
    if any(not 1 <= w <= 120 for w in args.windows):
        parser.error("W1 windows lie within 1..120 s")
    return args


def main(argv=None):
    args = arguments(argv)
    folder = records.RunFolder.from_path(args.run_dir)
    if folder.prompt != "18":
        raise SystemExit("link-loss run folders are wired bench 18 (YYYY-MM-DD_wb18_slug)")
    image = {"path": args.firmware.as_posix(), "sha256": records.sha256_file(args.firmware)}
    baseline = records.load_baseline()
    record = scenario_record(args, image, args.commit, baseline)
    print(json.dumps({"case": args.case, "record": record}, indent=1, default=str), flush=True)
    folder.create()
    scenario = records.write_scenario(folder, **json.loads(json.dumps(record)))
    run = _Run(scenario, folder)
    live = Live(run, baseline, args)
    live.plan = scenario["restoration_plan"]
    error = None
    try:
        if CASES[args.case].get("deferred"):
            for level in CASES[args.case]["levels"]:
                run.verdict(args.case, level, "NOT RUN", OWNER_ABSENT)
        else:
            records.check_board(args.port)
            runners = {"R0": case_r0, "RV": case_rv, "H1": case_h1, "H2": case_h2, "W3": case_w3, "PR": case_pr,
                       "W1": lambda f, lv: case_w1(f, lv, args.windows),
                       **{c: (lambda f, lv, c=c: reset_move_case(f, lv, c, inputs=c != "W2", flash=c == "G2"))
                          for c in ("W2", "G1", "G2")}}
            runners[args.case](folder, live)
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append("case stopped: %s" % caught)
    folder.write_json("link.json", {"case": args.case, "detail": live.detail, "timeline": live.timeline,
                                    "events": live.events[-600:], "samples": live.samples[-2000:],
                                    "error": str(error) if error else None})
    for name in ("baseline_before.json", "ending_reads.json", "session.jsonl", "summary.json", "console.log"):
        if not (folder / name).exists():
            if name.endswith(".json"):
                folder.write_json(name, {"recorded": False, "reason": "no session reached it: %s" % (error or OWNER_ABSENT)})
            else:
                (folder / name).write_text("", encoding="utf-8")
    state_ = run.cases[args.case]
    for level in CASES[args.case]["levels"]:
        if level not in state_["verdicts"]:
            run.verdict(args.case, level, "NOT RUN", "not reached: %s" % (error or "no verdict"))
    cases = [{**scenario["cases"][0], "verdicts": state_["verdicts"], "reason": state_["reason"],
              "evidence": [p for p in state_["evidence"] + ["link.json"] if (folder / p).exists()]}]
    for entry in scenario["write_list"]:
        ident = (entry["drive"], entry["key"], entry.get("index"))
        if ident not in run.fields:
            value = (records.baseline_field(baseline, entry["drive"], entry["key"]) or {}).get("value")
            window = run.fields.get((entry["drive"], "profile.acceleration", None))
            if entry["key"].startswith("profile.") and window and window["restored"]:
                run.field(entry, baseline=value, ending=value, restored=True,
                          reason="restored and read back with the drive's 0x0021/5 stage window")
            else:
                run.field(entry, baseline=value, ending=value, restored=True, reason="not written")
    records.write_result(folder, commit=args.commit, image=image, cases=cases,
                         restoration={"outputs_off": run.outputs_off, "fields": list(run.fields.values()),
                                      "drives": run.drives, "port_closed": True},
                         deviations=run.deviations)
    report = records.check_run(folder.path)
    print(json.dumps(report, indent=1))
    return records.check_exit(report)


if __name__ == "__main__":
    sys.exit(main())
