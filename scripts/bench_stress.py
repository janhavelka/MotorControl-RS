#!/usr/bin/env python3
"""Autonomous finite-motion stress run for the free-shaft ESS bench.

Runs randomized finite cases through the ordinary console, within the default
PositionLimits, and classifies every result. A console timeout runs a bounded
USB check, reopens the port and sends one new normal stop if communication
returns. It continues only after that stop proves observed standstill. A failed
USB check or stop ends the run, including with --recover. Other anomalies
capture the drive state with reads only and exit, unless --recover selects
bounded recovery (alarm clear, MCU reset, release/enable) with a verification ladder.
Never replays an uncertain write. A motor power cycle is not available here.
With --addresses, each case runs on one drive chosen at random and selected
with useaddr; --cross-reads also reads the other drives while one moves and
fails the run if any of them runs, alarms or changes position.

USB modes (wired bench 18): ``--usb slow-reader`` pauses host reads for
0.5-5 s inside some result waits while output flows (each wait is extended by
its pause); ``--usb saturate`` sends ``--count`` numbered no-motor commands at
the maximum rate, idle and then during a finite move with one ``stop fast``
among them, and reports the uncounted loss (sent - replied - input_dropped).
The host OS, pyserial and USB driver versions go into the summary and every
stall record.

Exit codes: 0 duration reached, 3 anomaly, 4 USB check or stop failed, 5 still
degraded after recovery (needs a motor power cycle).
"""
import argparse
import collections
import ctypes
import gzip
import json
import pathlib
import platform
import random
import re
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bench_records as records  # noqa: E402  (no pyserial import)

IDENTITY = (0x303A, 0x1001, "3C:0F:02:CD:6B:98")
HOME_FACTS = "mechanics rates ramp zero-offset auxiliary inputs"
RUNNING, HOMED, ARRIVED, ALARM_BIT, RELEASED = 4, 2, 1, 8, 16

# Soak plan (wired bench 20 item 3). Every bench edge names its role; the plan
# check compares the role with the input functions planned for drives 1, 2
# and 4 at that step (COMMON "Shared outputs"). Function codes: InputFunction.
DRIVES = tuple(records.FIXTURE["input_drives"])
OUTPUTS = dict(records.FIXTURE["outputs"])
TERMINALS = records.TERMINALS
MOTION_FUNCTIONS = frozenset(range(7, 18))  # Start, trigger, speed, JOG, PV, PT, home and selector (COMMON).
STOP_FUNCTIONS = frozenset({5, 6})          # Stop, emergency stop.
SWITCH_FUNCTIONS = frozenset({1, 2, 3})     # Origin, positive limit, negative limit.
INERT = frozenset({0})
# Functions that command no motion on an idle drive: none, origin and limit
# sensors, stops. Release (4) and 7-17 are never admitted outside the case
# (prompt 13 ran its PIN and trigger edges over the baseline origin and limit
# inputs of the other drives).
PASSIVE = INERT | SWITCH_FUNCTIONS | STOP_FUNCTIONS
# Role: (route of the edge, functions admitted on a member's switched terminal,
# functions admitted on every other drive on that line).
ROLES = {
    "attributed_start": ("arm_trigger", MOTION_FUNCTIONS, PASSIVE),
    "selector": ("bench_on", MOTION_FUNCTIONS, PASSIVE),
    "switch": ("bench_on", SWITCH_FUNCTIONS, PASSIVE),
    "stop": ("bench_on", STOP_FUNCTIONS, PASSIVE),
    "negative": ("bench_on", PASSIVE, PASSIVE),
}
RELAYS = ("relay1", "relay2")
RELAY_BUDGET = 10000         # Per relay for the whole set: 10 % of the FTR-B4 rated electrical minimum.
RELAY_MIN_INTERVAL_S = 2.0   # Per relay, no faster than the rated test rate (0.5 Hz).
RESET_CASES = ("mcu_reset", "simple_boot")
# Anomalies that can mean continued motion: the prepared stop fast goes out at once.
CONTINUED_MOTION = ("OTHER_DRIVE", "RUNNING_STUCK", "RUNNING_AFTER_STOP", "NOT_SETTLED")


def plan_check(steps, relay_used=None, relay_planned=None):
    """Rejections of a soak plan before it starts; an empty list admits it.

    A step has ``case`` and ``role``. ``serial`` steps switch no output. A
    ``reset`` step needs ``interlock`` True. An edge step names ``output``,
    ``route`` (``arm_trigger`` or ``bench_on``), ``members``, the members'
    ``function`` for starts and selectors, and ``assignments``
    {drive: {terminal: function}} of drives 1, 2 and 4 at that step.
    ``relay_used`` (operations since prompt 01) plus ``relay_planned`` must stay
    within RELAY_BUDGET per relay; a plan that switches a relay needs
    ``relay_used``.
    """
    problems = []
    for step in steps:
        name, role = step.get("case"), step.get("role")
        if role == "reset":
            if step.get("interlock") is not True:
                problems.append(f"{name}: reset without the before-reset interlock")
            continue
        if role == "serial":
            continue
        if role not in ROLES:
            problems.append(f"{name}: unknown edge role {role}")
            continue
        route, member_functions, other_functions = ROLES[role]
        if role == "attributed_start" and step.get("route") != "arm_trigger":
            problems.append(f"{name}: raw edge used as the start of an attributed operation")
            continue
        if step.get("route") != route:
            problems.append(f"{name}: route {step.get('route')} does not match role {role}")
            continue
        terminal = OUTPUTS.get(step.get("output"))
        assignments = step.get("assignments") or {}
        if terminal is None or any(set(assignments.get(d) or {}) != set(TERMINALS) for d in DRIVES):
            problems.append(f"{name}: output or planned assignments of drives {DRIVES} incomplete")
            continue
        members = set(step.get("members") or ())
        if role in ("attributed_start", "selector", "switch", "stop") and not members:
            problems.append(f"{name}: {role} edge without members")
            continue
        for drive in DRIVES:
            function = assignments[drive][terminal]
            if drive in members:
                admitted = function in member_functions and (
                    role not in ("attributed_start", "selector") or function == step.get("function"))
            else:
                admitted = function in other_functions
            if not admitted:
                problems.append(f"{name}: drive {drive} {terminal} = {function} does not match role {role}")
    relays = {s.get("output") for s in steps if s.get("output") in RELAYS} | set(relay_planned or ())
    if relays and relay_used is None:
        problems.append("relay operations since prompt 01 not stated")
    for relay in RELAYS:
        total = (relay_used or {}).get(relay, 0) + (relay_planned or {}).get(relay, 0)
        if total > RELAY_BUDGET:
            problems.append(f"{relay}: {total} operations exceed the budget of {RELAY_BUDGET}")
    return problems


def mix_plan(names):
    """Plan steps of a mix: resets carry the interlock this harness always runs."""
    return [dict(case=n, role="reset", interlock=True) if n in RESET_CASES else dict(case=n, role="serial")
            for n in names]


class RelayCounter:
    """Relay operations (``bench`` view ``ons``) of this run, on top of the
    operations recorded before it. ``ons`` counts from boot, so a lower count
    is a controller restart and the earlier session's count is kept."""

    def __init__(self, used=None):
        self.used = dict(used or {})
        self.run = {r: 0 for r in RELAYS}
        self.seen = {}
        self.last_on = {}

    def observe(self, outputs):
        for output in outputs or ():
            name, ons = output.get("name"), output.get("ons")
            if name not in RELAYS or type(ons) is not int:
                continue
            previous = self.seen.get(name)
            if previous is not None:
                self.run[name] += ons - previous if ons >= previous else ons
            self.seen[name] = ons

    def total(self, relay):
        return self.used.get(relay, 0) + self.run[relay]

    def admit(self, relay, now):
        """Seconds to wait before the next ON of ``relay``; Anomaly when the budget is spent."""
        if self.total(relay) + 1 > RELAY_BUDGET:
            raise Anomaly("RELAY_BUDGET", {"relay": relay, "total": self.total(relay)})
        return max(0.0, self.last_on.get(relay, float("-inf")) + RELAY_MIN_INTERVAL_S - now)

    def totals(self):
        return {r: dict(before=self.used.get(r, 0), run=self.run[r], total=self.total(r)) for r in RELAYS}


def declared_wiring_problem(reply, address):
    """Prompt 02's board declaration in force for ``address``: the table
    applied, its masks equal to the fixture, the bench output map applied and
    no host override. Returns the first problem, or None."""
    expected = records._expected_board(address)
    if any(key not in reply for key in ("board", "sources", "fixture", "bench_outputs")):
        return "wiring reply lacks the board declaration or the bench output map"
    if reply["fixture"] != "applied":
        return "board fixture table %s" % reply["fixture"]
    if reply["board"] != expected:
        return "board declaration %s differs from the fixture %s" % (reply["board"], expected)
    if reply.get("bench_output_map") != "applied" or reply["bench_outputs"] != records.expected_bench_outputs():
        return "bench output map differs from the fixture"
    if (reply.get("inputs") or []) + (reply.get("outputs") or []) != expected["inputs"] + expected["outputs"]:
        return "session declarations differ from the board table"
    return None


def option_problems(args):
    """Option combinations refused before the port opens."""
    problems = []
    if getattr(args, "declare_unconnected", False) and getattr(args, "wiring", "as-found") == "declared":
        problems.append("--declare-unconnected contradicts the declared wiring of prompt 02")
    if getattr(args, "cases", None) is not None and args.cases < 1:
        problems.append("--cases needs a positive count")
    return problems


def _serial():
    """pyserial, imported where a port opens, so the tests run without it."""
    import serial
    import serial.tools.list_ports  # noqa: F401  (binds serial.tools)
    return serial


def _pyserial_version():
    try:
        return _serial().__version__
    except ImportError:
        return None


class Anomaly(Exception):
    def __init__(self, kind, detail):
        super().__init__(f"{kind}: {detail}")
        self.kind, self.detail = kind, detail


class Unresponsive(Exception):
    pass


class Console:
    def __init__(self, port, out, ring=6000):
        found = [p for p in _serial().tools.list_ports.comports() if p.device == port]
        if not found or (found[0].vid, found[0].pid, found[0].serial_number) != IDENTITY:
            raise SystemExit(f"{port} is not the recorded bench device")
        self.name, self.out = port, out
        self.ring = collections.deque(maxlen=ring)
        self.buffer = b""
        self.next_id = 1
        self.terminals = {}
        self.replies = {}
        self.retained = []
        self.port = None
        self.pauses = []       # Slow-reader pauses: (host time, seconds).
        self.pause_chance = 0.0
        self.rng = random.Random(0)
        self.open()

    def open(self):
        self.port = _serial().Serial()
        self.port.port = self.name
        self.port.baudrate = 115200
        self.port.timeout = 0.005
        self.port.dtr = self.port.rts = False
        self.port.exclusive = True
        self.port.open()
        self.buffer = b""

    def close(self):
        if self.port:
            self.port.close()

    def reset_mcu(self):
        self.close()
        s = _serial().Serial()
        s.port = self.name
        s.baudrate = 115200
        s.dtr = False
        s.rts = True
        s.open()
        time.sleep(0.15)
        s.rts = False
        time.sleep(0.05)
        s.close()
        for _ in range(40):
            time.sleep(0.25)
            if any(p.device == self.name and p.serial_number == IDENTITY[2] for p in _serial().tools.list_ports.comports()):
                break
        time.sleep(3.0)
        self.terminals.clear()
        self.replies.clear()
        self.retained.clear()
        self.open()

    def pump(self, seconds=0.0):
        end = time.time() + seconds
        while True:
            data = self.port.read(8192)
            if data:
                self.buffer += data
                while b"\n" in self.buffer:
                    line, self.buffer = self.buffer.split(b"\n", 1)
                    text = line.decode("utf-8", "replace").rstrip()
                    if text:
                        self.feed(text)
            if time.time() >= end:
                return

    def feed(self, text):
        self.ring.append((round(time.time(), 3), text))
        if not text.startswith("{"):
            return
        try:
            item = json.loads(text)
        except ValueError:
            return
        ident = item.get("id")
        if ident is None:
            return
        if item.get("type") == "reply":
            self.replies[ident] = item
        else:
            self.terminals[ident] = item

    def send(self, command, wait=1.5):
        ident = self.next_id
        self.next_id += 1
        self.ring.append((round(time.time(), 3), f">> @{ident} {command}"))
        self.port.write(f"@{ident} {command}\n".encode())
        end = time.time() + wait
        while ident not in self.replies:
            self.pump(0.002)
            if time.time() > end:
                raise Unresponsive(command)
        reply = self.replies.pop(ident)
        reply["_id"] = ident
        op = reply.get("operation_id")
        if op and reply.get("result") == "accepted" and op not in self.retained:
            self.retained.append(op)
        return reply

    def simple(self, command, wait):
        """moveby and moveto answer with simple_move records instead of a reply. Returns the last one."""
        ident = self.next_id
        self.next_id += 1
        self.ring.append((round(time.time(), 3), f">> @{ident} {command}"))
        self.port.write(f"@{ident} {command}\n".encode())
        end = time.time() + wait
        while time.time() < end:
            self.pump(0.002)
            if ident in self.replies:
                return self.replies.pop(ident)
            item = self.terminals.get(ident)
            if item and not item.get("pending"):
                del self.terminals[ident]
                for key in ("operation_id", "move_operation_id"):
                    if item.get(key) and item[key] not in self.retained:
                        self.retained.append(item[key])
                return item
        return None

    def pause(self):
        """Slow reader: no host read for 0.5-5 s; returns the pause length."""
        seconds = round(self.rng.uniform(0.5, 5.0), 2)
        self.pauses.append((round(time.time(), 3), seconds))
        self.ring.append((round(time.time(), 3), f"-- host reads paused {seconds} s"))
        time.sleep(seconds)
        return seconds

    def terminal(self, reply, wait):
        ident = reply["_id"]
        end = time.time() + wait
        if self.pause_chance and self.rng.random() < self.pause_chance:
            end += self.pause()  # The wait is extended by its own pause.
        while ident not in self.terminals:
            self.pump(0.002)
            if time.time() > end:
                return None
        return self.terminals.pop(ident)

    def release_all(self):
        for op in list(self.retained):
            try:
                result = self.send(f"release {op}", 1.0).get("result")
            except Unresponsive:
                return
            if result in ("done", "invalid", "invalid_operation_id"):  # invalid: not retained, nothing to release
                self.retained.remove(op)


def host_info():
    """Host OS, Python, pyserial and the board's USB driver (Windows PnP), for stall records."""
    info = dict(os=platform.platform(), python=platform.python_version(),
                pyserial=_pyserial_version(), driver=None)
    if sys.platform == "win32":
        query = ("Get-CimInstance Win32_PnPSignedDriver | Where-Object { $_.HardwareID -like '*VID_303A&PID_1001*' } | "
                 "Select-Object DeviceName,DriverVersion,DriverProviderName,InfName | ConvertTo-Json -Compress")
        try:
            info["driver"] = subprocess.run(["powershell", "-NoProfile", "-Command", query], capture_output=True,
                                            text=True, timeout=30).stdout.strip() or None
        except (OSError, subprocess.SubprocessError) as error:
            info["driver"] = "unavailable: %s" % error
    return info


def saturation_loss(sent: int, replied: int, dropped_before, dropped_after):
    """Q13: commands neither answered nor counted by the firmware as dropped input."""
    dropped = None if dropped_before is None or dropped_after is None else dropped_after - dropped_before
    return dict(sent=sent, replied=replied, input_dropped=dropped,
                uncounted=None if dropped is None else sent - replied - dropped)


def keep_awake():
    """Holds off system sleep for this process only; changes no power setting."""
    if sys.platform == "win32":
        ctypes.windll.kernel32.SetThreadExecutionState(0x80000000 | 0x00000001)


def offset(delta, expected):
    """Distance between two 32-bit positions, so negative moves compare like positive ones."""
    d = (delta - expected) & 0xFFFFFFFF
    return min(d, 0x100000000 - d)


def state_fields(item):
    """Flatten one read-state terminal."""
    out = {}
    for block in item.get("state_blocks", []):
        for key in ("raw_motion", "raw_alarm", "raw_inputs", "raw_speed", "position_words", "raw_position"):
            if key in block:
                out[key] = block[key]
    words = out.get("position_words")
    if words:
        out["position"] = (words[0] << 16) | words[1]
    return out


def stop_confirmed(reply, item, address):
    """A fresh normal stop of ``address`` whose terminal proves observed standstill."""
    motion = (item or {}).get("raw_motion")
    return bool(
        reply.get("ok") and reply.get("result") == "accepted"
        and item and item.get("ok") is True and item.get("type") == "action"
        and item.get("id") == reply.get("_id") and item.get("command_id") == reply.get("_id")
        and reply.get("operation_id") and item.get("operation_id") == reply["operation_id"]
        and item.get("action_kind") == "stop" and item.get("stop_policy") == "normal"
        and item.get("target") == address and item.get("address") == address
        and item.get("state") == "succeeded" and item.get("outcome") == "observed"
        and item.get("execution") == "acknowledged" and item.get("completion") == "observed"
        and item.get("observation_known") is True and item.get("raw_alarm") == 0
        and type(motion) is int and 0 <= motion <= 0xFFFF and not motion & (RUNNING | ALARM_BIT))


class Run:
    address = 1  # Selected drive; tools built without __init__ keep node 1.

    def __init__(self, args):
        self.args = args
        self.addresses = args.addresses
        self.address = self.addresses[0]
        self.selected = None
        self.auxiliary_ready = set()
        self.positions = {}
        self.per_drive = collections.Counter()
        self.out = pathlib.Path(args.out)
        self.out.mkdir(parents=True, exist_ok=True)
        self.random = random.Random(args.seed)
        self.console = Console(args.port, self.out)
        self.cases = open(self.out / "cases.csv", "a", encoding="utf-8")
        self.counts = collections.Counter()
        self.anomalies = []
        self.usb_stalls = []
        self.health = None
        self.refused = None
        self.subdivision, self.ceiling = 1000, 2000
        self.started = time.time()
        self.moved = 0
        self.last = None
        self.host = host_info()
        self.completed = 0
        self.by_design = set()     # Drives that move by design in the current case.
        self.group_active = False  # A group operation of the current case may be moving.
        self.relays = RelayCounter(getattr(args, "relay_used", None))
        self.plan = None
        if getattr(args, "usb", "off") == "slow-reader":
            self.console.pause_chance = 0.3
            self.console.rng = random.Random(args.seed)

    # Reads
    def read(self, kind, wait=2.0):
        reply = self.console.send(f"read {kind}")
        if reply.get("result") == "results_full":
            self.console.release_all()
            reply = self.console.send(f"read {kind}")
        if not reply.get("ok"):
            raise Anomaly("READ_REFUSED", f"read {kind}: {reply.get('result')}")
        item = self.console.terminal(reply, wait)
        self.console.release_all()
        if item is None or not item.get("ok"):
            raise Anomaly("READ_FAILED", f"read {kind}: {item and item.get('outcome')}")
        return item

    def state(self, check=True):
        fields = state_fields(self.read("state"))
        self.last = fields
        if check and fields.get("raw_alarm"):
            raise Anomaly("ALARM", fields)
        return fields

    def read_other(self, address):
        reply = self.console.send(f"read state {address}")
        if reply.get("result") == "results_full":
            self.console.release_all()
            reply = self.console.send(f"read state {address}")
        if not reply.get("ok"):
            raise Anomaly("CROSS_READ_REFUSED", f"read state {address}: {reply.get('result')}")
        item = self.console.terminal(reply, 2.0)
        self.console.release_all()
        if item is None or not item.get("ok"):
            raise Anomaly("CROSS_READ_FAILED", f"read state {address}: {item and item.get('outcome')}")
        return state_fields(item)

    def cross_read(self):
        """One read of another drive: it must stay stopped, alarm-free and in place,
        unless the current case moves it by design (then alarm-free only)."""
        others = [a for a in getattr(self, "addresses", [self.address]) if a != self.address]
        if not getattr(self.args, "cross_reads", False) or not others:
            return
        other = self.random.choice(others)
        fields = self.read_other(other)
        self.counts["cross_read"] += 1
        known = self.positions.get(other)
        if other in getattr(self, "by_design", ()):
            if fields.get("raw_alarm"):
                raise Anomaly("OTHER_DRIVE", {"address": other, "fields": fields, "by_design": True,
                                              "moving": self.address})
            return
        if (fields.get("raw_alarm") or fields.get("raw_motion", 0) & RUNNING or
                (known is not None and fields.get("position") != known)):
            raise Anomaly("OTHER_DRIVE", {"address": other, "fields": fields, "expected_position": known,
                                          "moving": self.address})
        self.positions[other] = fields.get("position")

    def settled_position(self, hold=2.0, limit=8.0):
        """Position of the selected drive once unchanged for `hold` seconds.

        After a stop, firmware 0x0029 can still change its reported position by
        one increment for about 1.5 s. Other drives are compared with this value.
        """
        first = time.time()
        fields = self.state()
        since, position = time.time(), fields["position"]
        while time.time() - since < hold:
            if time.time() - first > limit:
                raise Anomaly("NOT_SETTLED", {"address": self.address, "position": position})
            self.console.pump(0.25)
            fields = self.state()
            if fields["position"] != position or fields.get("raw_speed"):
                since, position = time.time(), fields["position"]
                self.counts["settle_change"] += 1
        return position

    def select(self, address, force=False):
        """Local drive selection. The stress run never restores profiles, so its snapshot is released."""
        if address == self.selected and not force:
            return
        self.console.release_all()
        view = self.console.send("motion-profile inspect")
        for _ in range(100):
            if not view.get("pending"):
                break
            self.console.pump(0.01)
            view = self.console.send("motion-profile inspect")
        if view.get("saved"):
            forgotten = self.console.send("motion-profile forget")
            if not forgotten.get("ok"):
                raise Anomaly("SELECT", ("motion-profile forget", forgotten.get("result")))
        reply = self.console.send(f"useaddr {address}")
        if not reply.get("ok"):
            raise Anomaly("SELECT", (f"useaddr {address}", reply.get("result")))
        self.address = self.selected = address
        if getattr(self.args, "wiring", "as-found") == "declared":
            # Prompt 02's board table states every X terminal and drive 4's Y0
            # connected; useaddr restores it, so it is checked, never declared.
            problem = declared_wiring_problem(self.console.send("wiring"), address)
            if problem:
                raise Anomaly("SELECT", ("wiring", f"drive {address}: {problem}"))
        elif getattr(self.args, "declare_unconnected", False):
            # useaddr resets the host's wiring declarations to the board table or
            # the default. "Nothing wired" is a stated fixture fact, never a drive
            # change; it is refused when the immutable board declaration says a
            # terminal is connected, when the table is invalid, or when the image
            # does not report the board declaration.
            current = self.console.send("wiring")
            board = current.get("board")
            if "board" not in current or "fixture" not in current:
                raise Anomaly("SELECT", ("wiring", "reply lacks the board declaration"))
            if current.get("fixture") == "invalid":
                raise Anomaly("SELECT", ("wiring", "board fixture table invalid: " + str(current.get("fixture_error"))))
            if isinstance(board, dict) and 2 in board.get("inputs", []) + board.get("outputs", []):
                raise Anomaly("SELECT", ("wiring", f"board declares connected terminals on drive {address}"))
            for terminal in ("x0", "x1", "x2", "x3", "y0", "y1"):
                declared = self.console.send(f"wiring {terminal} unconnected")
                if not declared.get("ok"):
                    raise Anomaly("SELECT", (f"wiring {terminal} unconnected", declared.get("result")))
        self.console.send("probe", 3.0)
        self.console.pump(0.05)
        self.console.release_all()
        if address not in self.auxiliary_ready:
            self.prepare_auxiliary()
            self.auxiliary_ready.add(address)

    def after_reset(self):
        """A controller reset selects node 1 again; restore this run's selection."""
        self.selected = None
        self.auxiliary_ready.clear()
        self.select(self.address, force=True)

    def prepare(self):
        self.read("config")
        before = self.state()
        reply = self.console.send("motion-profile read")
        self.console.pump(0.05)
        for _ in range(100):
            self.console.pump(0.01)
            view = self.console.send("motion-profile inspect")
            if not view.get("pending"):
                if not view.get("session_ok", True):
                    raise Anomaly("PROFILE_READ", view.get("error"))
                break
        self.relation(view)
        return before

    def relation(self, view):
        """A profile stage on this address (a restoration, for example) locks
        moves until the start-speed relation is stated from a fresh profile
        read (CLI contract); state it for the stored word, as the scenarios do."""
        current = view.get("current") if isinstance(view, dict) else None
        if not (isinstance(current, list) and current):
            return
        reply = self.console.send(f"motion-profile start-speed relation {current[0]} {current[0]}")
        if not reply.get("ok"):
            raise Anomaly("START_SPEED_RELATION", reply.get("result"))

    def settle(self, limit):
        """Sample until not RUNNING. Returns samples; never sends a stop."""
        samples = []
        end = time.time() + limit
        while True:
            fields = self.state(check=False)
            samples.append((round(time.time() - self.t0, 3), fields.get("raw_motion"), fields.get("raw_speed"), fields.get("position"), fields.get("raw_alarm")))
            if fields.get("raw_alarm"):
                raise Anomaly("ALARM", samples[-8:])
            if not fields.get("raw_motion", 0) & RUNNING:
                return samples
            if time.time() > end:
                raise Anomaly("RUNNING_STUCK", samples[-8:])
            self.cross_read()
            self.console.pump(self.random.uniform(0.05, 0.25))

    def stop(self, policy):
        reply = self.console.send(f"stop {policy} {self.address}")
        if not reply.get("ok"):
            raise Anomaly("STOP_REFUSED", reply.get("result"))
        item = self.console.terminal(reply, 7.0)
        if item is None:
            raise Anomaly("STOP_NO_TERMINAL", policy)
        if not item.get("ok"):
            raise Anomaly("STOP_FAILED", {k: item.get(k) for k in ("outcome", "execution", "completion", "raw_motion", "raw_alarm")})
        self.console.pump(0.05)
        return item

    def start_move(self, steps, rpm):
        self.t0 = time.time()
        reply = self.console.send(f"move relative {steps} steps native {rpm} configured")
        if not reply.get("ok"):
            raise Anomaly("MOVE_REFUSED", reply.get("result"))
        return reply

    def duration(self, steps, rpm):
        return steps / (rpm * self.subdivision / 60) + 0.2

    @property
    def tolerance(self):
        """20 increments at subdivision 1000; one 4,000-count encoder count is subdivision/4000 increments."""
        return max(20, 2 * self.subdivision // 4000 + 8)

    def set_subdivision(self, value):
        """Typed drive setting with checked readback, RAM only: no save is issued."""
        self.read("config")
        self.state()
        reply = self.console.send(f"subdivision {value}", 3.0)
        item = self.console.terminal(reply, 8.0) if reply.get("ok") else None
        self.console.release_all()
        if not item or not item.get("ok") or item.get("readback") != value:
            raise Anomaly("SUBDIVISION", {"requested": value, "reply": reply.get("result"), "item": item})
        item = self.console.simple("settings", 5.0)
        self.console.release_all()
        self.subdivision = value
        self.ceiling = (item or {}).get("maximum_position_rpm") or 0
        if self.ceiling != min(2000, 3000000 // value):
            raise Anomaly("CEILING", {"subdivision": value, "reported": self.ceiling})

    def limit_rpm(self):
        return self.random.choice([self.ceiling, self.ceiling, self.ceiling - 1, min(600, self.ceiling)])

    def case_limit_plain(self):
        """At the policy ceiling: min(2000 rpm, 50,000 increments/s)."""
        turns = self.random.choice([0.25, 1, 5, 20])
        steps, rpm = int(turns * self.subdivision), self.limit_rpm()
        before = self.prepare()
        reply = self.start_move(steps, rpm)
        item = self.console.terminal(reply, self.duration(steps, rpm) + 5)
        self.check_terminal(item, "MOVE")
        after = self.state()
        delta = (after["position"] - before["position"]) & 0xFFFFFFFF
        if abs(delta - steps) > self.tolerance or after["raw_motion"] & RUNNING:
            raise Anomaly("POSITION", {"steps": steps, "rpm": rpm, "subdivision": self.subdivision, "delta": delta, "motion": after["raw_motion"], "speed": after.get("raw_speed")})
        self.moved += steps
        return f"sub={self.subdivision} steps={steps} rpm={rpm} rate={rpm * self.subdivision // 60} delta={delta}"

    def case_limit_interrupt(self):
        return f"sub={self.subdivision} " + self.interrupt_finish(5 * self.subdivision, self.limit_rpm())

    def case_limit_stop(self):
        steps, rpm = 10 * self.subdivision, self.limit_rpm()
        self.prepare()
        self.start_move(steps, rpm)
        self.console.pump(self.random.uniform(0.05, min(1.5, self.duration(steps, rpm) * 0.8)))
        policy = self.random.choice(["normal", "fast"])
        self.stop(policy)
        after = self.state()
        if after["raw_motion"] & RUNNING:
            raise Anomaly("RUNNING_AFTER_STOP", after)
        self.console.release_all()
        return f"sub={self.subdivision} policy={policy} rpm={rpm}"

    def check_terminal(self, item, kind, allow=()):
        if item is None:
            raise Anomaly(f"{kind}_NO_TERMINAL", "")
        outcome = item.get("outcome")
        if item.get("ok") or outcome in allow:
            return
        raise Anomaly(f"{kind}_FAILED", {k: item.get(k) for k in ("outcome", "execution", "completion", "raw_motion", "raw_alarm", "uncertain", "detail", "status")})

    # Cases
    def case_plain(self):
        steps = self.random.choice([100, 500, 1000, 3000]) * self.random.choice([1, -1])
        rpm = self.random.choice([60, 60, 120, 300])
        before = self.prepare()
        reply = self.start_move(steps, rpm)
        item = self.console.terminal(reply, self.duration(abs(steps), rpm) + 5)
        self.check_terminal(item, "MOVE")
        if item.get("position_mismatch"):
            self.counts["position_mismatch"] += 1
        after = self.state()
        delta = (after["position"] - before["position"]) & 0xFFFFFFFF
        if offset(delta, steps) > self.tolerance or after["raw_motion"] & RUNNING:
            raise Anomaly("POSITION", {"steps": steps, "delta": delta, "motion": after["raw_motion"], "speed": after.get("raw_speed")})
        self.moved += abs(steps)
        return f"steps={steps} rpm={rpm} delta={delta}"

    def case_interrupt_finish(self):
        """The failing shape: interrupt, let it finish, then a checked stop."""
        return self.interrupt_finish(self.random.choice([1000, 3000, 3000, 3000]), self.random.choice([60, 60, 60, 120]))

    def case_fast_interrupt(self):
        """The same shape near the rate limit: 2,000 rpm is 33,333 increments/s at subdivision 1000."""
        return self.interrupt_finish(self.random.choice([10000, 30000]), self.random.choice([600, 1000, 2000]))

    def case_fast_plain(self):
        steps = self.random.choice([3000, 10000, 30000]) * self.random.choice([1, -1])
        rpm = self.random.choice([600, 1000, 2000])
        before = self.prepare()
        reply = self.start_move(steps, rpm)
        item = self.console.terminal(reply, self.duration(abs(steps), rpm) + 5)
        self.check_terminal(item, "MOVE")
        after = self.state()
        delta = (after["position"] - before["position"]) & 0xFFFFFFFF
        if offset(delta, steps) > self.tolerance or after["raw_motion"] & RUNNING:
            raise Anomaly("POSITION", {"steps": steps, "rpm": rpm, "delta": delta, "motion": after["raw_motion"], "speed": after.get("raw_speed")})
        self.moved += abs(steps)
        return f"steps={steps} rpm={rpm} delta={delta}"

    def case_double_interrupt(self):
        """Interrupts the restarted move again. Refusal is a valid outcome and is counted."""
        steps, rpm = 3000, self.random.choice([60, 120])
        duration = self.duration(steps, rpm)
        before = self.prepare()
        reply = self.start_move(steps, rpm)
        current = reply["operation_id"]
        interrupts = 0
        for _ in range(self.random.choice([2, 2, 3])):
            self.console.pump(self.random.uniform(0.05, duration * 0.8))
            interrupt = self.console.send(f"position interrupt {current}")
            if not interrupt.get("ok"):
                self.counts["interrupt_refused"] += 1
                self.refused = interrupt.get("result")
                break
            interrupts += 1
            current = interrupt["operation_id"]
            self.console.terminal(interrupt, 2.0)
        self.settle(duration * (interrupts + 1) + 3)
        settled = self.state()
        delta = (settled["position"] - before["position"]) & 0xFFFFFFFF
        if not steps - 20 <= delta <= (interrupts + 1) * steps + 60:
            raise Anomaly("POSITION", {"steps": steps, "delta": delta, "interrupts": interrupts})
        self.stop("normal")
        after = self.state()
        if after["raw_motion"] & RUNNING:
            raise Anomaly("RUNNING_AFTER_STOP", after)
        self.console.release_all()
        self.moved += delta
        return f"rpm={rpm} interrupts={interrupts} delta={delta} refused={self.refused}"

    def reset_interlock(self):
        """COMMON "Before every flash or controller reset", read back just before
        the reset: drives 1, 2 and 4 stopped, alarm-free, at zero speed and with
        no input function 7-17. RESET_INTERLOCK otherwise; nothing is reset."""
        rows = {}
        for address in DRIVES:
            config = self.read(f"config {address}")
            inputs = (config.get("config") or {}).get("inputs") or []
            functions = [e.get("function") for e in inputs]
            fields = self.read_other(address)
            problems = []
            if len(functions) != 4 or not all(e.get("known") is True for e in inputs):
                problems.append("input functions not known")
            elif any(type(f) is not int or 7 <= f <= 17 for f in functions):
                problems.append(f"input function 7-17 assigned: {functions}")
            if (fields.get("raw_alarm") != 0 or fields.get("raw_motion", RUNNING) & RUNNING or
                    "raw_speed" not in fields or fields["raw_speed"]):
                problems.append("not reported stopped, alarm-free and at zero speed")
            rows[address] = dict(functions=functions, problems=problems)
        self.counts["reset_interlock"] += 1
        if any(r["problems"] for r in rows.values()):
            raise Anomaly("RESET_INTERLOCK", rows)
        return rows

    def case_mcu_reset(self):
        """Controller reset at standstill after the before-reset interlock; resets
        during motion stay wired bench 18's evidence."""
        moving = False
        steps, rpm = 3000, 60
        before = self.prepare()
        self.reset_interlock()
        self.console.reset_mcu()
        self.t0 = time.time()
        self.console.send("debug " + self.args.debug, 3.0)
        self.after_reset()
        self.console.pump(0.3)
        self.read("config")
        self.settle(self.duration(steps, rpm) + 3)
        after = self.state()
        delta = (after["position"] - before["position"]) & 0xFFFFFFFF
        expected = steps if moving else 0
        if abs(delta - expected) > 20:
            raise Anomaly("POSITION", {"expected": expected, "delta": delta, "motion": after["raw_motion"]})
        self.moved += delta
        self.prepare_auxiliary()
        return f"moving={moving} delta={delta}"

    def simple_ramps(self):
        rpm = min(self.random.choice([60, 120, 300, 600]), self.ceiling)
        accel, decel = self.random.choice([100, 200, 500, 1000, 2000]), self.random.choice([100, 200, 500, 1000, 2000])
        for command in (f"speed {rpm}", f"accel {accel}", f"decel {decel}"):
            if not self.console.send(command).get("ok"):
                raise Anomaly("SIMPLE_SETUP", command)
        return rpm, accel, decel

    def simple_move(self, command, steps, ramps):
        rpm, accel, decel = ramps
        self.t0 = time.time()
        item = self.console.simple(command, self.duration(abs(steps), rpm) + (accel + decel) / 1000 + 5)
        self.check_terminal(item, "SIMPLE")
        self.console.release_all()

    def case_simple(self):
        """The ordinary route: stated speed and ramps, then moveby, with ramps across the admitted range."""
        ramps = self.simple_ramps()
        before = self.prepare()  # Fresh profile read and start-speed relation, as before ordinary moves.
        steps = self.random.choice([100, 1000, 3000]) * self.random.choice([1, -1])
        self.simple_move(f"moveby {steps}", steps, ramps)
        after = self.state()
        delta = (after["position"] - before["position"]) & 0xFFFFFFFF
        expected = steps & 0xFFFFFFFF
        if offset(delta, expected) > self.tolerance or after["raw_motion"] & RUNNING:
            raise Anomaly("POSITION", {"steps": steps, "ramps": ramps, "delta": delta, "motion": after["raw_motion"]})
        self.moved += abs(steps)
        return f"moveby {steps} ramps={ramps}"

    def case_simple_boot(self):
        """The documented first use: after a controller reset, moveto relative to the RAM boot zero."""
        if 1 not in getattr(self, "addresses", [1]):
            return "skipped: the boot zero belongs to node 1"
        self.select(1)  # The firmware sets its boot zero only for the startup drive.
        self.reset_interlock()
        self.console.reset_mcu()
        self.console.send("debug " + self.args.debug, 3.0)
        self.after_reset()
        ramps = self.simple_ramps()
        zero = self.state()["position"]
        for target in sorted([self.random.randrange(-3000, 3000), self.random.randrange(-3000, 3000)]) + [0]:
            self.simple_move(f"moveto {target}", 3000, ramps)
            after = self.state()
            offset = (after["position"] - zero) & 0xFFFFFFFF
            if min((offset - target) & 0xFFFFFFFF, (target - offset) & 0xFFFFFFFF) > 20 or after["raw_motion"] & RUNNING:
                raise Anomaly("POSITION", {"target": target, "zero": zero, "offset": offset, "ramps": ramps, "motion": after["raw_motion"]})
        self.prepare_auxiliary()
        return f"zero={zero} ramps={ramps}"

    def case_position_clear(self):
        self.read("config")
        self.state()
        reply = self.console.send("position-clear qualification install clear-semantics")
        if not reply.get("ok"):
            raise Anomaly("CLEAR_INSTALL", reply.get("result"))
        reply = self.console.send("position-clear")
        if not reply.get("ok"):
            raise Anomaly("CLEAR_REFUSED", reply.get("result"))
        self.check_terminal(self.console.terminal(reply, 3), "CLEAR")
        after = self.state()
        if after["position"] != 0 or after["raw_motion"] & RUNNING:
            raise Anomaly("CLEAR_END", after)
        self.console.release_all()
        return "ok"

    def interrupt_finish(self, steps, rpm):
        duration = self.duration(steps, rpm)
        delay = self.random.uniform(0.05, 0.95) * duration
        before = self.prepare()
        reply = self.start_move(steps, rpm)
        original = reply["operation_id"]
        while time.time() - self.t0 < delay:
            self.console.pump(0.005)
        interrupt = self.console.send(f"position interrupt {original}")
        admitted = interrupt.get("ok")
        if admitted:
            self.console.terminal(interrupt, 2.0)
        else:
            self.counts["interrupt_refused"] += 1
            item = self.console.terminal(reply, duration + 5)
            self.check_terminal(item, "MOVE")
            self.console.release_all()
            return f"steps={steps} refused={interrupt.get('result')}"
        wait_before = self.random.choice(["sample", "sample", "silent"])
        samples = []
        if wait_before == "sample":
            samples = self.settle(duration * 2 + 3)
        else:
            self.console.pump(duration + 1.2)
            samples = self.settle(3)
        extra = self.random.uniform(0, 1.0)
        self.console.pump(extra)
        settled = self.state()
        delta = (settled["position"] - before["position"]) & 0xFFFFFFFF
        if not steps - self.tolerance <= delta <= 2 * steps + 3 * self.tolerance:
            raise Anomaly("POSITION", {"steps": steps, "rpm": rpm, "delta": delta, "delay": delay, "samples": samples[-6:]})
        self.stop("normal")
        after = self.state()
        if after["raw_motion"] & RUNNING:
            raise Anomaly("RUNNING_AFTER_STOP", after)
        self.console.release_all()
        self.moved += delta
        return f"steps={steps} rpm={rpm} delay={delay:.3f} mode={wait_before} delta={delta} samples={len(samples)}"

    def case_interrupt_stop(self):
        steps, rpm = 3000, self.random.choice([60, 120])
        self.prepare()
        reply = self.start_move(steps, rpm)
        original = reply["operation_id"]
        self.console.pump(self.random.uniform(0.05, 1.5))
        interrupt = self.console.send(f"position interrupt {original}")
        if interrupt.get("ok"):
            self.console.terminal(interrupt, 2.0)
        self.console.pump(self.random.uniform(0, 0.8))
        policy = self.random.choice(["normal", "fast"])
        self.stop(policy)
        after = self.state()
        if after["raw_motion"] & RUNNING:
            raise Anomaly("RUNNING_AFTER_STOP", after)
        self.console.release_all()
        return f"policy={policy}"

    def case_stop_moving(self):
        steps, rpm = 3000, self.random.choice([60, 120, 300])
        self.prepare()
        self.start_move(steps, rpm)
        self.console.pump(self.random.uniform(0.02, min(1.5, self.duration(steps, rpm) * 0.8)))
        policy = self.random.choice(["normal", "fast"])
        self.stop(policy)
        after = self.state()
        if after["raw_motion"] & RUNNING:
            raise Anomaly("RUNNING_AFTER_STOP", after)
        self.console.release_all()
        return f"policy={policy} rpm={rpm}"

    def case_home(self):
        method = self.random.choice([33, 34, 34, 33, 35])
        fields = self.state()
        if method == 35 and fields["raw_motion"] & HOMED:
            method = 34
        self.read("config")
        fields = self.state()
        reply = self.console.send("home read")
        if reply.get("ok"):
            self.console.terminal(reply, 2.0)
        self.console.release_all()
        index = " index" if method != 35 else ""
        reply = self.console.send(f"home qualification install {method} 60 30 100 {HOME_FACTS}{index}")
        if not reply.get("ok"):
            raise Anomaly("HOME_INSTALL", reply)
        self.t0 = time.time()
        reply = self.console.send(f"home {method} 60 30 100 zero")
        if not reply.get("ok"):
            raise Anomaly("HOME_REFUSED", reply.get("result"))
        interrupt = self.random.random() < 0.2 and method != 35
        if interrupt:
            self.console.pump(self.random.uniform(0.05, 0.6))
            self.stop(self.random.choice(["normal", "fast"]))
            self.console.terminal(reply, 2.0)
        else:
            item = self.console.terminal(reply, 35)
            self.check_terminal(item, "HOME")
            self.console.pump(0.02)
            after = self.state()
            if after["position"] != 0 or after["raw_motion"] & (RUNNING | HOMED) != HOMED:
                raise Anomaly("HOME_END", after)
        self.console.release_all()
        return f"method={method} stopped={interrupt}"

    def case_release_enable(self):
        self.read("config")
        self.state()
        for command in (f"motor-release {self.address}", f"enable {self.address}"):
            reply = self.console.send(command)
            if not reply.get("ok"):
                raise Anomaly("ACTION_REFUSED", (command, reply.get("result")))
            item = self.console.terminal(reply, 3)
            self.check_terminal(item, "ACTION")
        after = self.state()
        if after["raw_motion"] & (RUNNING | RELEASED):
            raise Anomaly("RELEASE_ENABLE", after)
        self.console.release_all()
        self.console.pump(1.1)  # Starts answer busy for 1 s after an enable (HW-15).
        return "ok"

    def prepare_auxiliary(self):
        if "home" in getattr(self.args, "skip", ()):
            return  # No home in the mix: the auxiliary word stays as found (D15 snapshot discipline).
        reply = self.console.send("home read")
        item = self.console.terminal(reply, 2) if reply.get("ok") else None
        self.console.release_all()
        words = (item or {}).get("homing_parameters") or item or {}
        if (words.get("raw_auxiliary") if isinstance(words, dict) else None) == 7:
            return
        self.read("config")
        self.state()
        reply = self.console.send("home auxiliary 7")
        if not reply.get("ok"):
            raise Anomaly("AUX", reply.get("result"))
        self.check_terminal(self.console.terminal(reply, 3), "AUX")
        self.console.release_all()

    # USB stall: keep the same handle first, because reconnecting hides where bytes were.
    def usb_stall(self, detail):
        c = self.console
        begun = time.time()
        arrivals = []
        probe_sent = False
        while time.time() - begun < 20:
            count = len(c.ring)
            c.pump(0.1)
            if len(c.ring) != count:
                arrivals.append(round(time.time() - begun, 2))
            if not probe_sent and time.time() - begun > 8:
                # Host to device traffic alone: does input wake the stuck output?
                probe_sent = True
                c.ring.append((round(time.time(), 3), ">> newline probe"))
                c.port.write(b"\n")
        stalled_only = not arrivals
        c.close()
        time.sleep(0.5)
        c.open()
        c.pump(1.5)
        drv = None
        try:
            reply = c.send("drv", 3.0)
            drv = {k: reply.get(k) for k in ("output_queued", "output_blocked", "output_short_writes", "reset_reason", "usb_stall")}
            status = c.send("status", 2.0)
            drv["uptime_ms"] = status.get("uptime_ms")
        except Unresponsive:
            drv = "still unresponsive after reopen"
        record = dict(when=time.strftime("%H:%M:%S"), detail=str(detail), recovered_on_same_handle=not stalled_only,
                      arrivals=arrivals[:10], drv=drv, stop_reply=None, stop_terminal=None,
                      stop_confirmed=False, host=getattr(self, "host", None))
        self.usb_stalls.append(record)
        self.counts["usb_stall"] += 1
        if not isinstance(drv, str):
            # One fresh stop per moving drive after reconnect (a group stop first
            # for group cases); never replay a request lost earlier.
            c.next_id += 1
            record["stops"] = []
            try:
                if getattr(self, "group_active", False):
                    reply = c.send("group stop normal", 10.0)
                    item = c.terminal(reply, 10.0) if reply.get("ok") else None
                    record["group_stop"] = dict(reply=reply, terminal=item,
                                                confirmed=bool(item and item.get("ok") is True))
                for address in [self.address] + sorted(set(getattr(self, "by_design", ())) - {self.address}):
                    reply = c.send(f"stop normal {address}", 3.0)
                    item = c.terminal(reply, 7.0) if reply.get("ok") and reply.get("result") == "accepted" else None
                    record["stops"].append(dict(address=address, reply=reply, terminal=item,
                                                confirmed=stop_confirmed(reply, item, address)))
                    if address == self.address:
                        record["stop_reply"], record["stop_terminal"] = reply, item
                record["stop_confirmed"] = (all(s["confirmed"] for s in record["stops"]) and
                                            record.get("group_stop", {}).get("confirmed", True))
            except Unresponsive as exc:
                record["stop_error"] = str(exc)
                record["stop_confirmed"] = False
        record["file"] = self.incident("USB_STALL", record).name
        self.summary()
        return record["stop_confirmed"]

    # Diagnostics
    def incident(self, name, detail):
        stamp = time.strftime("%Y%m%d_%H%M%S")
        path = self.out / f"incident_{stamp}_{name}.jsonl.gz"
        with gzip.open(path, "wt", encoding="utf-8") as f:
            f.write(json.dumps({"incident": name, "detail": str(detail), "counts": self.counts, "moved": self.moved,
                                "address": self.address, "by_design": sorted(getattr(self, "by_design", ()))}) + "\n")
            for entry in self.console.ring:
                f.write(json.dumps(entry) + "\n")
        with open(self.out / "incidents.jsonl", "a", encoding="utf-8") as index:  # Written as it happens.
            index.write(json.dumps(dict(when=time.strftime("%H:%M:%S"), incident=name, file=path.name)) + "\n")
        return path

    def postmortem(self):
        """Reads only. The drive stays exactly as it failed."""
        samples = []
        end = time.time() + 15
        while time.time() < end:
            try:
                f = self.state(check=False)
                samples.append((round(time.time() - self.started, 2), f.get("raw_motion"), f.get("raw_alarm"), f.get("raw_speed"), f.get("position")))
            except (Anomaly, Unresponsive) as exc:
                samples.append(str(exc))
                self.console.pump(0.5)
            self.console.pump(0.5)
        for command in ("drv", "memory", "stats"):
            try:
                self.console.send(command)
            except Unresponsive:
                pass
        self.console.pump(0.3)
        return samples

    def recover(self):
        """Bounded software recovery, then the verification ladder."""
        log = []
        log.append(("interlock", self.reset_interlock()))
        self.console.reset_mcu()
        log.append("mcu_reset")
        self.after_reset()
        self.console.pump(0.3)
        self.read("config")
        fields = self.state(check=False)
        log.append(("after_reset", fields.get("raw_motion"), fields.get("raw_alarm"), fields.get("position")))
        if fields.get("raw_alarm"):
            reply = self.console.send(f"alarm-clear {self.address}")
            self.console.terminal(reply, 3)
            self.console.release_all()
            log.append("alarm_clear")
        self.console.send(f"motor-release {self.address}")
        self.console.pump(0.8)
        self.console.send(f"enable {self.address}")
        self.console.pump(0.8)
        self.console.release_all()
        ladder = []
        healthy = True
        for steps in (100, 500, 1000, 3000):
            try:
                before = self.prepare()
                reply = self.start_move(steps, 60)
                item = self.console.terminal(reply, self.duration(steps, 60) + 5)
                self.check_terminal(item, "MOVE")
                after = self.state()
                delta = (after["position"] - before["position"]) & 0xFFFFFFFF
                ladder.append((steps, delta, after["raw_motion"]))
                if abs(delta - steps) > 20 or after["raw_motion"] & RUNNING:
                    healthy = False
                    break
            except (Anomaly, Unresponsive) as exc:
                ladder.append((steps, str(exc)))
                healthy = False
                break
            finally:
                self.console.release_all()
        log.append(("ladder", ladder, healthy))
        return healthy, log

    def sample_health(self):
        """Keeps the firmware's own counters, which show trouble that caused no failed case."""
        try:
            reply = self.console.send("drv", 2.0)
            self.health = {k: reply.get(k) for k in ("output_blocked", "output_short_writes", "input_dropped", "stop_evidence_overwritten", "reset_reason", "usb_stall")}
            self.health["memory"] = self.console.send("memory", 2.0)
            self.health["stats"] = self.console.send("stats", 2.0)
            bench = self.console.send("bench", 2.0)
            self.relays.observe(bench.get("outputs"))
            self.health["bench"] = {o.get("name"): {k: o.get(k) for k in ("active", "ons", "off_reason")}
                                    for o in bench.get("outputs") or ()}
            self.health["relay_ops"] = self.relays.totals()
            with open(self.out / "health.jsonl", "a", encoding="utf-8") as f:
                f.write(json.dumps(dict(when=time.strftime("%H:%M:%S"), cases=self.completed, **self.health)) + chr(10))
        except Unresponsive:
            pass

    def flood(self, count, stop_at=None):
        """Write ``count`` numbered `version` lines as fast as the port takes them,
        reading between writes; one `stop fast` replaces line ``stop_at``. Returns
        (first id, ids sent, stop id)."""
        c = self.console
        first, stop_id, sent = c.next_id, None, []
        for n in range(count):
            ident = c.next_id
            c.next_id += 1
            if n == stop_at:
                stop_id = ident
                line = f"@{ident} stop fast {self.address}\n"
            else:
                line = f"@{ident} version\n"
            c.port.write(line.encode())
            sent.append(ident)
            if n % 16 == 0:
                c.pump(0)
        c.pump(3.0)
        return first, sent, stop_id

    def saturate(self, count):
        """Q13: saturation idle, then during a finite move with one stop fast in the middle."""
        c = self.console
        out = {}
        self.console.send("debug off", 3.0)
        for phase in ("idle", "move"):
            before = c.send("drv", 3.0).get("input_dropped")
            row = {}
            if phase == "move":
                self.prepare()
                start = self.start_move(20000, 60)
                row["move_operation_id"] = start.get("operation_id")
                c.pump(1.0)
            begun = time.time()
            first, sent, stop_id = self.flood(count, stop_at=count // 2 if phase == "move" else None)
            row["seconds"] = round(time.time() - begun, 2)
            replied = sum(1 for i in sent if i in c.replies)
            after = c.send("drv", 3.0).get("input_dropped")
            row.update(saturation_loss(len(sent), replied, before, after))
            row["unanswered_ids"] = [i for i in sent if i not in c.replies][:40] if row.get("uncounted") else []
            with open(self.out / ("saturation_%s_ring.jsonl" % phase), "w", encoding="utf-8") as ring:
                for entry in c.ring:  # The last 6000 lines around the flood.
                    ring.write(json.dumps(entry) + chr(10))
            if stop_id is not None:
                reply = c.replies.get(stop_id)
                row["stop_reply"] = reply and {k: reply.get(k) for k in ("ok", "result", "operation_id")}
                item = c.terminals.get(stop_id)
                if item is None and reply and reply.get("ok"):
                    item = c.terminal(dict(reply, _id=stop_id), 7.0)
                row["stop_terminal"] = item and {k: item.get(k) for k in ("ok", "outcome", "execution", "completion")}
                state = self.state(check=False)
                row["after"] = {k: state.get(k) for k in ("raw_motion", "raw_speed", "position", "raw_alarm")}
                if state.get("raw_motion", 0) & RUNNING:
                    row["safety_stop"] = self.stop("fast").get("outcome")
            for i in sent:
                c.replies.pop(i, None)
            c.release_all()
            out[phase] = row
        self.saturation = out
        return out

    def safety_stop(self):
        """After a failed or interrupted run (and its read-only postmortem): one
        new stop fast per drive, so no harness exit leaves a drive moving
        (wired bench 12 deviation 1). Never a replay; failures are recorded."""
        self.safety_stops = []
        for address in self.addresses:
            try:
                reply = self.console.send(f"stop fast {address}", 3.0)
                if reply.get("result") == "unavailable":
                    # Refused before TX: the firmware admits an action, a stop included, only to the
                    # selected drive or to one with a checked reply since the last selection change
                    # or restart (wired bench 20 b4). One read binds it; the new stop is no replay.
                    bound = self.console.send(f"read state {address}", 3.0)
                    if bound.get("ok"):
                        self.console.terminal(bound, 2.0)
                    self.safety_stops.append((address, "unavailable, bound by read", bound.get("result")))
                    reply = self.console.send(f"stop fast {address}", 3.0)
                item = self.console.terminal(reply, 7.0) if reply.get("ok") else None
                self.safety_stops.append((address, reply.get("result"), (item or {}).get("outcome")))
            except Exception as error:  # A dead console cannot stop anything; say so.
                self.safety_stops.append((address, "failed", str(error)))
        try:  # Free the stop results, so a later strict session finds no foreign retained stop.
            self.console.release_all()
        except Exception as error:
            self.safety_stops.append((None, "release failed", str(error)))

    def summary(self, final=None):
        data = dict(elapsed_s=round(time.time() - self.started), moved_increments=self.moved, counts=dict(self.counts),
                    addresses=self.addresses, per_drive=dict(self.per_drive),
                    anomalies=self.anomalies, usb_stalls=self.usb_stalls, health=self.health, final=final, seed=self.args.seed,
                    host=getattr(self, "host", None), usb=getattr(self.args, "usb", "off"),
                    pauses=len(self.console.pauses), paused_s=round(sum(p for _, p in self.console.pauses), 1),
                    saturation=getattr(self, "saturation", None), safety_stops=getattr(self, "safety_stops", None),
                    completed=getattr(self, "completed", None), case_limit=getattr(self.args, "cases", None),
                    plan=getattr(self, "plan", None),
                    relay_ops=self.relays.totals() if hasattr(self, "relays") else None)
        (self.out / "summary.json").write_text(json.dumps(data, indent=1))
        return data

    def loop(self):
        cases = [("interrupt_finish", self.case_interrupt_finish, 50), ("plain", self.case_plain, 20),
                 ("interrupt_stop", self.case_interrupt_stop, 10), ("stop_moving", self.case_stop_moving, 10),
                 ("home", self.case_home, 7), ("release_enable", self.case_release_enable, 3)]
        if self.args.mix == "extended":
            cases = [("interrupt_finish", self.case_interrupt_finish, 25), ("plain", self.case_plain, 10),
                     ("interrupt_stop", self.case_interrupt_stop, 10), ("stop_moving", self.case_stop_moving, 10),
                     ("home", self.case_home, 7), ("release_enable", self.case_release_enable, 3),
                     ("fast_plain", self.case_fast_plain, 12), ("fast_interrupt", self.case_fast_interrupt, 12),
                     ("double_interrupt", self.case_double_interrupt, 10), ("mcu_reset", self.case_mcu_reset, 1),
                     ("simple", self.case_simple, 15), ("simple_boot", self.case_simple_boot, 2),
                     ("position_clear", self.case_position_clear, 3)]
        if self.args.mix == "limit":
            cases = [("limit_plain", self.case_limit_plain, 30), ("limit_interrupt", self.case_limit_interrupt, 30),
                     ("limit_stop", self.case_limit_stop, 15), ("interrupt_stop", self.case_interrupt_stop, 7),
                     ("home", self.case_home, 5), ("release_enable", self.case_release_enable, 3),
                     ("simple", self.case_simple, 10)]
        if self.args.only:
            cases = [c for c in cases if c[0] == self.args.only]
        cases = [c for c in cases if c[0] not in getattr(self.args, "skip", ())]
        names = [c[0] for c in cases]
        weights = [c[2] for c in cases]
        table = {c[0]: c[1] for c in cases}
        problems = plan_check(mix_plan(names), getattr(self.args, "relay_used", None))
        self.plan = dict(cases=names, weights=weights, problems=problems)
        if problems:
            raise SystemExit("plan check refused: " + "; ".join(problems))
        self.console.send("debug " + self.args.debug)
        self.console.pump(0.2)
        self.relays.observe(self.console.send("bench").get("outputs"))
        for address in reversed(self.addresses):  # Probe every drive, end on the first.
            self.select(address, force=True)
            self.positions[address] = self.settled_position() if getattr(self.args, "cross_reads", False) else self.state()["position"]
        self.console.pump(0.3)
        if self.args.subdivision:
            self.set_subdivision(self.args.subdivision)
        end = self.started + self.args.duration
        rest = time.time() + 1200
        written = 0
        limit = getattr(self.args, "cases", None)
        while time.time() < end and (limit is None or self.completed < limit):
            name = self.random.choices(names, weights)[0]
            begun = time.time()
            try:
                self.select(self.random.choice(self.addresses))
                note = table[name]()
                self.completed += 1
                self.by_design.clear()
                self.group_active = False
                self.counts[name] += 1
                self.per_drive[self.address] += 1
                self.positions[self.address] = (self.settled_position() if getattr(self.args, "cross_reads", False)
                                                else self.state()["position"])
                note = f"a={self.address} {note}"
                self.cases.write(f"{time.strftime('%H:%M:%S')},{name},ok,{time.time() - begun:.2f},{note}\n")
                self.cases.flush()
            except (Anomaly, Unresponsive) as exc:
                kind = exc.kind if isinstance(exc, Anomaly) else "CONSOLE_UNRESPONSIVE"
                detail = exc.detail if isinstance(exc, Anomaly) else str(exc)
                if kind == "CONSOLE_UNRESPONSIVE" and self.usb_stall(detail):
                    self.cases.write(f"{time.strftime('%H:%M:%S')},{name},usb_stall,{time.time() - begun:.2f},{detail}\n")
                    self.cases.flush()
                    continue
                self.counts["anomaly_" + kind] += 1
                self.cases.write(f"{time.strftime('%H:%M:%S')},{name},{kind},{time.time() - begun:.2f},a={self.address} {detail}\n")
                self.cases.flush()
                if kind in CONTINUED_MOTION:
                    self.safety_stop()  # COMMON: the prepared stop fast at once, before diagnosis.
                path = self.incident(kind, detail)
                postmortem = None
                if kind != "CONSOLE_UNRESPONSIVE":
                    postmortem = self.postmortem()
                    self.incident(kind + "_post", postmortem)
                entry = dict(when=time.strftime("%H:%M:%S"), case=name, address=self.address, kind=kind, detail=str(detail), file=path.name, postmortem=postmortem)
                self.anomalies.append(entry)
                self.summary()
                if kind == "CONSOLE_UNRESPONSIVE" or not self.args.recover:
                    return 4 if kind == "CONSOLE_UNRESPONSIVE" else 3
                try:
                    healthy, log = self.recover()
                except (Anomaly, Unresponsive) as err:
                    healthy, log = False, [str(err)]
                entry["recovery"] = log
                self.summary()
                if not healthy:
                    return 5
                self.prepare_auxiliary()
            self.console.pump(self.random.uniform(0.1, 1.2))
            if time.time() > rest:
                self.console.pump(60)
                rest = time.time() + 1200
            if self.completed // 100 != written:
                written = self.completed // 100
                self.sample_health()
                self.summary()
        self.sample_health()
        if self.args.subdivision and self.args.restore:
            self.set_subdivision(self.args.restore)
        return 0


def relay_totals(text):
    """``relay1=N,relay2=N``: operations of each relay before this run."""
    try:
        totals = {k: int(v) for k, v in (item.split("=", 1) for item in text.split(","))}
    except ValueError:
        raise argparse.ArgumentTypeError("relay1=N,relay2=N")
    if set(totals) != set(RELAYS) or any(v < 0 for v in totals.values()):
        raise argparse.ArgumentTypeError("relay1=N,relay2=N with non-negative counts")
    return totals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM13")
    parser.add_argument("--out", required=True)
    parser.add_argument("--duration", type=float, default=3600.0)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--debug", choices=("off", "raw", "decoded"), default="off")
    parser.add_argument("--recover", action="store_true")
    parser.add_argument("--mix", choices=("standard", "extended", "limit"), default="standard")
    parser.add_argument("--subdivision", type=int, help="drive subdivision for this run, set in RAM with readback")
    parser.add_argument("--restore", type=int, default=1000, help="subdivision restored after a clean run")
    parser.add_argument("--only", help="run one case repeatedly, for a targeted reproduction")
    parser.add_argument("--addresses", default="1", help="comma-separated ESS drives; each case picks one")
    parser.add_argument("--cross-reads", action="store_true", help="read the other drives while one moves")
    parser.add_argument("--usb", choices=("off", "slow-reader", "saturate"), default="off",
                        help="wired bench 18: pause host reads in result waits, or the Q13 saturation run")
    parser.add_argument("--count", type=int, default=10000, help="--usb saturate: commands per phase")
    parser.add_argument("--skip", nargs="+", default=(), help="cases left out of the mix (home under D15)")
    parser.add_argument("--cases", type=int, help="end the run after this many completed cases")
    parser.add_argument("--wiring", choices=("as-found", "declared"), default="as-found",
                        help="declared: require prompt 02's board declaration on every selected drive")
    parser.add_argument("--relay-used", type=relay_totals, default=None,
                        help="relay operations since prompt 01, relay1=N,relay2=N (relay budget)")
    parser.add_argument("--declare-unconnected", action="store_true",
                        help="state that no drive terminal is wired; declared again after each useaddr; "
                             "refused when the board table declares a terminal of that drive connected or is invalid")
    args = parser.parse_args()
    try:
        args.addresses = [int(a) for a in args.addresses.split(",")]
    except ValueError:
        parser.error("--addresses takes comma-separated integers")
    if (not args.addresses or len(set(args.addresses)) != len(args.addresses) or
            not all(1 <= a <= 247 for a in args.addresses)):
        parser.error("--addresses needs distinct drives within 1..247")
    if args.subdivision and len(args.addresses) > 1:
        parser.error("--subdivision applies to one drive only")
    for problem in option_problems(args):
        parser.error(problem)
    keep_awake()
    run = Run(args)
    code = 1
    try:
        if args.usb == "saturate":
            run.select(args.addresses[0], force=True)
            run.saturate(args.count)
            code = 0
        else:
            code = run.loop()
    finally:
        if code != 0:
            run.safety_stop()
        data = run.summary(final=code)
        run.console.close()
    print(json.dumps({k: data[k] for k in ("elapsed_s", "moved_increments", "counts", "final")}))
    for u in data["usb_stalls"]:
        print("usb_stall", u["when"], u["detail"][:60], "same_handle=", u["recovered_on_same_handle"], u["drv"])
    for a in data["anomalies"]:
        print(a["when"], a["case"], a["kind"], a["detail"][:400])
        print("  post", (a.get("postmortem") or [])[:3], "rec", a.get("recovery"))
    return code


if __name__ == "__main__":
    sys.exit(main())
