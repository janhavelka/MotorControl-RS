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

Exit codes: 0 duration reached, 3 anomaly, 4 USB check or stop failed, 5 still
degraded after recovery (needs a motor power cycle).
"""
import argparse
import collections
import ctypes
import gzip
import json
import pathlib
import random
import re
import sys
import time

import serial
import serial.tools.list_ports as list_ports

IDENTITY = (0x303A, 0x1001, "3C:0F:02:CD:6B:98")
HOME_FACTS = "mechanics rates ramp zero-offset auxiliary inputs"
RUNNING, HOMED, ARRIVED, ALARM_BIT, RELEASED = 4, 2, 1, 8, 16


class Anomaly(Exception):
    def __init__(self, kind, detail):
        super().__init__(f"{kind}: {detail}")
        self.kind, self.detail = kind, detail


class Unresponsive(Exception):
    pass


class Console:
    def __init__(self, port, out, ring=6000):
        found = [p for p in list_ports.comports() if p.device == port]
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
        self.open()

    def open(self):
        self.port = serial.Serial()
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
        s = serial.Serial()
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
            if any(p.device == self.name and p.serial_number == IDENTITY[2] for p in list_ports.comports()):
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

    def terminal(self, reply, wait):
        ident = reply["_id"]
        end = time.time() + wait
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


class Run:
    def __init__(self, args):
        self.args = args
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
        return before

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
            self.console.pump(self.random.uniform(0.05, 0.25))

    def stop(self, policy):
        reply = self.console.send(f"stop {policy} 1")
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

    def case_mcu_reset(self):
        """Controller reset while idle or during a finite move. The drive owns the move and finishes it."""
        moving = self.random.random() < 0.7
        steps, rpm = 3000, 60
        before = self.prepare()
        if moving:
            self.start_move(steps, rpm)
            self.console.pump(self.random.uniform(0.05, 2.5))
        self.console.reset_mcu()
        self.t0 = time.time()
        self.console.send("debug " + self.args.debug, 3.0)
        self.console.send("probe", 3.0)
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
        before = self.state()
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
        self.console.reset_mcu()
        self.console.send("debug " + self.args.debug, 3.0)
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
        for command in ("motor-release 1", "enable 1"):
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
                      stop_confirmed=False)
        self.usb_stalls.append(record)
        self.counts["usb_stall"] += 1
        if not isinstance(drv, str):
            # One fresh stop after reconnect; never replay a request lost earlier.
            c.next_id += 1
            try:
                reply = c.send("stop normal 1", 3.0)
                record["stop_reply"] = reply
                if reply.get("ok") and reply.get("result") == "accepted":
                    item = c.terminal(reply, 7.0)
                    record["stop_terminal"] = item
                    motion = (item or {}).get("raw_motion")
                    record["stop_confirmed"] = bool(
                        item and item.get("ok") is True and item.get("type") == "action"
                        and item.get("id") == reply.get("_id")
                        and item.get("command_id") == reply.get("_id")
                        and reply.get("operation_id") and item.get("operation_id") == reply["operation_id"]
                        and item.get("action_kind") == "stop" and item.get("stop_policy") == "normal"
                        and item.get("target") == 1 and item.get("address") == 1
                        and item.get("state") == "succeeded" and item.get("outcome") == "observed"
                        and item.get("execution") == "acknowledged" and item.get("completion") == "observed"
                        and item.get("observation_known") is True and item.get("raw_alarm") == 0
                        and type(motion) is int and 0 <= motion <= 0xFFFF
                        and not motion & (RUNNING | ALARM_BIT))
            except Unresponsive as exc:
                record["stop_error"] = str(exc)
        record["file"] = self.incident("USB_STALL", record).name
        self.summary()
        return record["stop_confirmed"]

    # Diagnostics
    def incident(self, name, detail):
        stamp = time.strftime("%Y%m%d_%H%M%S")
        path = self.out / f"incident_{stamp}_{name}.jsonl.gz"
        with gzip.open(path, "wt", encoding="utf-8") as f:
            f.write(json.dumps({"incident": name, "detail": str(detail), "counts": self.counts, "moved": self.moved}) + "\n")
            for entry in self.console.ring:
                f.write(json.dumps(entry) + "\n")
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
        self.console.reset_mcu()
        log.append("mcu_reset")
        self.console.send("probe")
        self.console.pump(0.3)
        self.read("config")
        fields = self.state(check=False)
        log.append(("after_reset", fields.get("raw_motion"), fields.get("raw_alarm"), fields.get("position")))
        if fields.get("raw_alarm"):
            reply = self.console.send("alarm-clear 1")
            self.console.terminal(reply, 3)
            self.console.release_all()
            log.append("alarm_clear")
        self.console.send("motor-release 1")
        self.console.pump(0.8)
        self.console.send("enable 1")
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
            with open(self.out / "health.jsonl", "a", encoding="utf-8") as f:
                f.write(json.dumps(dict(when=time.strftime("%H:%M:%S"), cases=self.counts.total(), **self.health)) + chr(10))
        except Unresponsive:
            pass

    def summary(self, final=None):
        data = dict(elapsed_s=round(time.time() - self.started), moved_increments=self.moved, counts=dict(self.counts),
                    anomalies=self.anomalies, usb_stalls=self.usb_stalls, health=self.health, final=final, seed=self.args.seed)
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
        names = [c[0] for c in cases]
        weights = [c[2] for c in cases]
        table = {c[0]: c[1] for c in cases}
        self.console.send("debug " + self.args.debug)
        self.console.pump(0.2)
        self.console.send("probe")
        self.console.pump(0.3)
        self.prepare_auxiliary()
        if self.args.subdivision:
            self.set_subdivision(self.args.subdivision)
        end = self.started + self.args.duration
        rest = time.time() + 1200
        written = 0
        while time.time() < end:
            name = self.random.choices(names, weights)[0]
            begun = time.time()
            try:
                note = table[name]()
                self.counts[name] += 1
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
                self.cases.write(f"{time.strftime('%H:%M:%S')},{name},{kind},{time.time() - begun:.2f},{detail}\n")
                self.cases.flush()
                path = self.incident(kind, detail)
                postmortem = None
                if kind != "CONSOLE_UNRESPONSIVE":
                    postmortem = self.postmortem()
                    self.incident(kind + "_post", postmortem)
                entry = dict(when=time.strftime("%H:%M:%S"), case=name, kind=kind, detail=str(detail), file=path.name, postmortem=postmortem)
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
            if self.counts.total() // 100 != written:
                written = self.counts.total() // 100
                self.sample_health()
                self.summary()
        self.sample_health()
        if self.args.subdivision and self.args.restore:
            self.set_subdivision(self.args.restore)
        return 0


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
    args = parser.parse_args()
    keep_awake()
    run = Run(args)
    code = 1
    try:
        code = run.loop()
    finally:
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
