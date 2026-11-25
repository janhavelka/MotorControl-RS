#!/usr/bin/env python3
"""Wired bench 20: the wired soak (K1/K2 wired share) on drives 1, 2 and 4.

One run folder (``build/bench/<date>_wb20_<slug>/``), one console session
(``bench_probe.Console``) and the existing case runners, so the shared-line
checks, the arm-trigger isolation and the restorations are the qualified ones:

- input starts through prompt 08's route (``bench_input_starts.run_case``,
  one trial per case): A2 relative and A1 absolute starts on drive 4 with the
  Y0 = 2 / ``aux1`` bracket, B1 the group start with per-member results, B2
  group stops of an armed group, A7 the X3 stop input (function 5,
  ``mosfet2``) of an input-started move, I1 the serial stored-profile interrupt;
- group moves (``bench_multi.submit``): relative and absolute plans, staged and
  sequential, each followed by its return plan, and group stop normal/fast of a
  long absolute plan;
- the PT trigger with its PIN selector (``bench_pt.run_regression``, prompt 13
  G1): on drive 1, 2 or 4, stored records 1-8 set to that drive's
  non-negative table through the typed PT setter, the layout X0 = 12, X1-X3 =
  PIN0-PIN2, record 2 selected by PIN0 (``relay2``), armed and triggered, then
  a trigger refused for a short PIN hold; the layout, the sixteen record words
  and the position mode restored on the same drive inside the case. The relay
  rate (one ON per 2 s) and budget apply; each case's ``relay2`` operations are
  counted from ``bench`` views;
- velocity inside prompt 14's envelope (``bench_velocity.velocity_run`` with
  the qualified 100/100 ms cell at subdivision 1000, 120-600 rpm, 2 s) on
  drives 2 and 4 (D15 safeguard 14), its words restored by ``velocity
  restore`` on the same drive at once (D15 item 4).

Before the soak the plan check (``bench_stress.plan_check``) validates every
edge's role against the planned assignments of all three drives. After every
case: group clear, the baseline I/O state through the ordinary setters with
readback (``enter_state BASE``), a per-field diff of the I/O words of 1, 2
and 4 against the 01 baseline and fresh stopped, alarm-free reads. The
profile words 0x0021/5 are restored once at the end through the stage route:
restoring them after each case revokes the start-speed relation and refuses
group moves until a controller restart. Any exception or failed case sends
the prepared ``group stop fast`` and ``stop fast`` to every drive at once,
writes its incident file and ends the soak; nothing is retried or replayed.
Relay operations are counted from ``bench`` views; only the PT case
switches a relay. NOT RUN for K1, not in the soak driver: the limit-switch
pulse (prompt 11) and prompt 07's own stop-input procedure (A7 is the stop
input). Exit codes: 0 duration, case limit, evidence headroom or relay budget
reached; 3 anomaly.
"""
import argparse
import json
import math
import random
import sys
import time
from pathlib import Path

import bench_input_starts as starts
import bench_multi as multi
import bench_probe as probe
import bench_pt as pt
import bench_records as records
import bench_stop_inputs as stops
import bench_stress as stress
import bench_switch
import bench_velocity as velocity
from bench_scenarios import _Run, _finish, _preflight, _stop_restore
from bench_session import run_recorded

DRIVES = tuple(records.FIXTURE["input_drives"])
VELOCITY_DRIVES = (2, 4)           # D15 safeguard 14: velocity phase on drive 2 (and 4), not drive 1.
VELOCITY_RPM = (120, 300, 600)     # Inside 14's C1 cells: ramps 100 ms, subdivision 1000, 2 s.
VELOCITY_MS = 2000
RELATIVE = {1: 1000, 2: -800, 4: 500}
PT_DRIVES = tuple(sorted(pt.TABLES))      # Non-negative tables only (prompt 13, D15 safeguard 13).
PT_RECORD = pt.CASES["G1"]["record"]      # Record 2: PIN0 on, the others off.
PT_RELAY = pt.PIN_OUTPUTS[0]              # relay2 on X1.
PT_RELAY_OPS_MAX = 3                      # relay2 ONs one regression may make (two selections, one renewal).
SHORT_HOLD = "a PIN output's hold ends before the trigger pulse and its margin"
TOLERANCE = 2                      # Endpoint increments; drive 4's recorded feedback offset is 1-2.
EVIDENCE_RECORDS_PER_MIB = 4096   # Measured 27,659 records in 17.7 MB (b2 smoke): about 1,600 per MiB.
SNAPSHOTS = {"snapshots": {"velocity": True}}  # Velocity words: velocity restore of the case's own snapshot.
# name: (family, weight, runner argument). Starts name their prompt 09 case.
CASES = {
    "start_relative": ("starts", 12, "A2"), "start_absolute": ("starts", 8, "A1"),
    "start_group": ("starts", 10, "B1"), "group_stop_armed": ("starts", 4, "B2"),
    "stop_input": ("starts", 6, "A7"), "interrupt": ("starts", 6, "I1"),
    "group_relative_staged": ("group", 6, ("relative", "staged", None)),
    "group_relative_sequential": ("group", 6, ("relative", "sequential", None)),
    "group_absolute_staged": ("group", 6, ("absolute", "staged", None)),
    "group_absolute_sequential": ("group", 6, ("absolute", "sequential", None)),
    "group_stop_normal": ("group", 3, ("absolute", "staged", "normal")),
    "group_stop_fast": ("group", 3, ("absolute", "staged", "fast")),
    "velocity": ("velocity", 10, None),
    "pt_trigger": ("pt", 6, None),
}


# --------------------------------------------------------------------------
# Pure parts (unit-tested)

def assignments(state_name):
    """Planned input functions of drives 1, 2 and 4 in a prompt 09 named state:
    X0/X3 from the state, X1/X2 as the 01 baseline (positive, negative limit)."""
    state = starts.STATES[state_name]
    return {a: {"x0": state["x0"][a], "x1": 2, "x2": 3, "x3": state["x3"][a]} for a in DRIVES}


def pt_assignments(address):
    """Prompt 13's layout on ``address``; the other drives keep the 01 baseline words."""
    planned = assignments("BASE")
    planned[address] = dict(pt.LAYOUT)
    return planned


def plan_steps(names):
    """``bench_stress.plan_check`` steps: each edge of each case with its role."""
    steps = []
    for name in names:
        family, _, argument = CASES[name]
        if family == "pt":
            for address in PT_DRIVES:  # The case picks one of them.
                planned = pt_assignments(address)
                steps.append(dict(case=name, role="selector", route="bench_on", output=PT_RELAY, members=[address],
                                  function=pt.LAYOUT["x1"], assignments=planned))
                steps.append(dict(case=name, role="attributed_start", route="arm_trigger", output=pt.TRIGGER_OUTPUT,
                                  members=[address], function=pt.LAYOUT["x0"], assignments=planned))
            continue
        if family != "starts":
            steps.append(dict(case=name, role="serial"))
            continue
        case = starts.CASES[argument]
        state = starts.STATES[starts.case_states(case)[0]]
        planned = assignments(starts.case_states(case)[0])
        if case["kind"] == "interrupt":
            steps.append(dict(case=name, role="serial"))
            continue
        steps.append(dict(case=name, role="attributed_start", route="arm_trigger", output=starts.START_OUTPUT,
                          members=list(state["members"]), function=7, assignments=planned))
        if case["kind"] in ("input-stop", "group-input-stop"):
            steps.append(dict(case=name, role="stop", route="bench_on", output=starts.STOP_OUTPUT,
                              members=[a for a in DRIVES if planned[a]["x3"] == 5], assignments=planned))
    return steps


def start_problems(name, cid, detail, error=None):
    """Every non-PASS level of the prompt 09 verdicts of one start case."""
    verdicts = starts.case_verdicts(cid, detail, error)
    return ["%s %s %s: %s" % (name, level, verdict, reason) for level, (verdict, reason) in verdicts.items()
            if verdict != "PASS"]


def group_problems(name, trials, tolerance=TOLERANCE):
    """A refused or failed plan, an endpoint off target, or uncertain stop evidence."""
    out = []
    for t in trials:
        if t.get("stop_problems"):
            out.append("%s %s: stop %s" % (name, t["label"], t["stop_problems"]))
        elif not t.get("stopped_by_case") and not t.get("ok"):
            out.append("%s %s: plan %s %s" % (name, t["label"], t.get("state"), t.get("error")))
        bad = {a: e for a, e in (t.get("errors") or {}).items() if abs(e) > tolerance}
        if bad:
            out.append("%s %s: endpoint errors %s" % (name, t["label"], bad))
    return out


def velocity_problems(name, detail):
    run = detail.get("run") or {}
    out = []
    if run.get("refused"):
        out.append("%s: refused %s" % (name, run["refused"]))
    elif (run.get("terminal") or {}).get("ok") is not True:
        out.append("%s: terminal %s" % (name, run.get("terminal")))
    if (run.get("settle") or {}).get("moving"):
        out.append("%s: still moving after the run" % name)
    return out + restored_problems(name, detail.get("restored", []))


def pt_problems(name, trial):
    """The record run (prompt 13 run verdict) and the refused short-hold trigger."""
    if not trial:
        return ["%s: no trial" % name]
    out = []
    verdict, reason = (trial.get("run") or {}).get("verdict") or ("FAIL", "no run")
    if verdict != "PASS":
        out.append("%s record %d: %s" % (name, PT_RECORD, reason))
    trigger = trial.get("trigger") or {}
    if not (trigger.get("admitted") is False and trigger.get("reason") == SHORT_HOLD and
            trial.get("ons_before") is not None and trial.get("ons_before") == trial.get("ons_after")):
        out.append("%s: short PIN hold trigger %s, mosfet1 ons %s -> %s" % (
            name, trigger, trial.get("ons_before"), trial.get("ons_after")))
    if trial.get("continued_motion"):
        out.append("%s: continued motion %s" % (name, trial["continued_motion"]))
    return out


def relay_problems(name, operations, seconds):
    """More relay ONs than the case plans, or faster than one per RELAY_MIN_INTERVAL_S."""
    out = []
    if operations > PT_RELAY_OPS_MAX:
        out.append("%s: %d %s operations, planned at most %d" % (name, operations, PT_RELAY, PT_RELAY_OPS_MAX))
    if operations > 1 and seconds < stress.RELAY_MIN_INTERVAL_S * (operations - 1):
        out.append("%s: %d %s operations in %.1f s" % (name, operations, PT_RELAY, seconds))
    return out


def restored_problems(name, results):
    return ["%s: %s %s not restored (%s)" % (name, r.get("key"), r.get("index"), r.get("reason")) for r in results
            if not r.get("restored") or r.get("uncertain")]


def restoration_problems(diff, states):
    """Per-field I/O differences from the baseline and drives not stopped or alarmed."""
    out = ["drive %s %s %s: %s" % (d.get("drive"), d.get("group"), d.get("key"), d.get("status"))
           for d in diff if d.get("status") != "equal"]
    out += ["drive %d not read stopped and alarm-free" % a for a, s in states.items()
            if not s or stops.moving(s) or s.get("alarm")]
    return out


# --------------------------------------------------------------------------
# Soak

class Soak:
    """One soak session: the case loop, bookkeeping and per-case restoration."""

    def __init__(self, live, vlive, args, folder, ptlive=None):
        self.live, self.vlive, self.ptlive, self.args, self.folder = live, vlive, ptlive, args, folder
        self.random = random.Random(args.seed)
        self.names = [n for n in CASES if not args.only or n in args.only]
        self.relays = stress.RelayCounter(args.relay_used)
        self.counts, self.anomalies, self.health = {}, [], []
        self.completed, self.ended = 0, None
        self.velocity_plans, self.pt_plans = {}, {}
        self.runners = {"starts": self.run_start, "group": self.run_group, "velocity": self.run_velocity,
                        "pt": self.run_pt}

    def relay_planned(self):
        """Worst case relay2 operations of a case-limited soak: its PT share at PT_RELAY_OPS_MAX each."""
        if self.args.cases is None or "pt_trigger" not in self.names:
            return {}
        share = CASES["pt_trigger"][1] / sum(CASES[n][1] for n in self.names)
        # Twice the expected share, as a margin for the random draw; the loop also stops at the budget.
        return {PT_RELAY: math.ceil(2 * self.args.cases * share * PT_RELAY_OPS_MAX)}

    def plan_check(self):
        return stress.plan_check(plan_steps(self.names), self.args.relay_used, self.relay_planned())

    # Runners: each returns (detail, problems).
    def share_declarations(self, declarations):
        """Board declarations for every interlock of the session, read once before any arm.

        ``bench_records.interlock`` otherwise reads them on its first ON: inside an
        armed session that selects other drives and forgets the armed drive's
        profile snapshot (wired bench 20 A7: ``motion-profile forget`` busy, the
        X3 stop pulse never sent).
        """
        if declarations.get("verdict") != "PASS":
            raise probe.BenchError("declaration agreement failed: %s" % declarations.get("problems"))
        for live in (self.live, self.vlive, self.ptlive):
            if live is not None:
                live.record["declarations"] = declarations

    def run_start(self, name, cid):
        detail = starts.run_case(self.live, cid)
        detail["verdicts"] = {level: list(v) for level, v in starts.case_verdicts(cid, detail, None).items()}
        return detail, start_problems(name, cid, detail)

    def run_group(self, name, argument):
        form, mode, stop = argument
        live = self.live
        if live.current != "BASE":
            starts.enter_state(live, "BASE")
        origins = multi.install_coordinates(live)
        live.group = True
        if form == "relative":
            plans = [({a: ("relative", v) for a, v in RELATIVE.items()}, None),
                     ({a: ("relative", -v) for a, v in RELATIVE.items()}, None)]
        else:
            target = multi.LONG if stop else multi.OFFSETS
            plans = [({a: ("absolute", v) for a, v in target.items()}, stop), ({a: ("absolute", 0) for a in DRIVES}, None)]
        trials = []
        for index, (intents, stop_policy) in enumerate(plans):
            t = multi.submit(live, intents, mode, "%s %d" % (name, index + 1), stop=stop_policy)
            t["stopped_by_case"] = bool(stop_policy)
            if t.get("ok") and not stop_policy:
                t["errors"] = multi.endpoint_errors(multi.member_targets(origins, intents, t["start"]), t["end"])
            trials.append(t)
            if not t.get("ok") and not stop_policy:
                break
        return {"trials": trials}, group_problems(name, trials)

    def run_velocity(self, name, _argument):
        v = self.vlive
        v.address = self.random.choice(VELOCITY_DRIVES)
        v.snapshots.pop("velocity", None)
        records.use_drive(v.console, v.address, timeout_s=v.timeout)
        velocity.qualify_velocity(v)
        rpm = self.random.choice(VELOCITY_RPM)
        detail = {"address": v.address, "rpm": rpm}
        try:
            run = velocity.velocity_run(v, rpm, VELOCITY_MS, cell=(100, 100, velocity.SUBDIVISION))
            run["settle"] = velocity.settle(v)
            detail["run"] = run
        finally:
            # D15 item 4: the snapshot restored on the same drive before another selection.
            detail["restored"] = [{k: r.get(k) for k in ("key", "restored", "uncertain", "reason")} for r in
                                  _stop_restore(v.console, v, v.run, self.velocity_plans[v.address], v.timeout,
                                                owner=v.address)]
            v.snapshots.pop("velocity", None)
        return detail, velocity_problems(name, detail)

    def run_pt(self, name, _argument):
        """One prompt 13 G1 regression; layout, records and mode restored on the same drive."""
        p = self.ptlive
        if self.live.current != "BASE" or self.live.group:
            raise probe.BenchError("the PT layout starts only from the baseline I/O state without a group")
        address = self.random.choice(PT_DRIVES)
        time.sleep(self.relays.admit(PT_RELAY, time.monotonic()))
        before = self.relays.total(PT_RELAY)
        began = time.monotonic()
        detail = {"address": address, "record": PT_RECORD}
        try:
            pt.setup_table(p, address, 0)
            detail["trial"] = pt.run_regression(p, address, PT_RECORD)
        finally:
            try:
                pt.outputs_off(p)
                pt.restore_layout(p, address)  # The trigger function first.
            finally:
                detail["restored"] = [{k: r.get(k) for k in ("key", "index", "restored", "uncertain", "reason")}
                                      for r in _stop_restore(p.console, p, p.run, self.pt_plans[address], p.timeout,
                                                             owner=address)]
                self.relays.observe(p.call("bench", host_args=()).get("outputs"))
                self.relays.last_on[PT_RELAY] = time.monotonic()  # Upper bound of the last ON.
        detail["relay_operations"] = self.relays.total(PT_RELAY) - before
        return detail, (pt_problems(name, detail.get("trial")) + restored_problems(name, detail["restored"]) +
                        relay_problems(name, detail["relay_operations"], time.monotonic() - began))

    # Bookkeeping.
    def trim(self, keep=2000):
        """Bounded host memory over hours: recent events and samples only; the
        bench views (aux1 edges) belong to the finished case."""
        for live in (self.live, self.vlive, self.ptlive):
            if live is None:
                continue
            del live.events[:-keep]
            del live.samples[:-keep]
            live.views.clear()

    def restore_case(self):
        """Group clear, the baseline I/O state with readback, then the per-field diff and fresh reads."""
        live = self.live
        if live.group:
            live.call("group-clear")
            live.group = False
        if live.current != "BASE":
            starts.enter_state(live, "BASE")
        reads = {a: records.read_drive(live.console, a, ("io",), select=False, timeout_s=live.timeout) for a in DRIVES}
        diff = records.diff_fields(live.baseline, {a: records.raws_from(r) for a, r in reads.items()}, ("io",))
        states = {a: live.read_state(a) for a in DRIVES}
        return restoration_problems(diff, states)

    def sample_health(self):
        sample = {"when": time.strftime("%H:%M:%S"), "cases": self.completed}
        for name in ("stats", "memory", "drv"):
            reply = self.live.call(name)
            sample[name] = {k: v for k, v in reply.items() if k not in ("type", "id", "profile")}
        view = self.live.call("bench", host_args=())
        self.relays.observe(view.get("outputs"))
        sample["bench"] = {o.get("name"): {k: o.get(k) for k in ("active", "ons", "off_reason")}
                           for o in view.get("outputs") or ()}
        sample["relay_ops"] = self.relays.totals()
        self.health.append(sample)
        with open(self.folder / "health.jsonl", "a", encoding="utf-8") as f:
            f.write(json.dumps(sample) + "\n")

    def incident(self, name, detail, problems, error=None):
        """Written as it happens: the case, its problems and the recent session evidence."""
        index = len(self.anomalies) + 1
        path = self.folder / ("incident_%02d_%s.json" % (index, name))
        path.write_text(json.dumps({"case": name, "problems": problems, "error": error, "detail": detail,
                                    "events": self.live.events[-300:], "samples": self.live.samples[-300:]},
                                   default=str, indent=1), encoding="utf-8")
        entry = {"when": time.strftime("%H:%M:%S"), "case": name, "problems": problems, "error": error,
                 "file": path.name}
        with open(self.folder / "incidents.jsonl", "a", encoding="utf-8") as f:
            f.write(json.dumps(entry) + "\n")
        self.anomalies.append(entry)
        return entry

    def loop(self):
        """Weighted cases until the duration, the case limit or the evidence headroom; 3 on an anomaly."""
        weights = [CASES[n][1] for n in self.names]
        self.relays.observe(self.live.call("bench", host_args=()).get("outputs"))
        self.live.checked("velocity-limits", host_args=("reset",))
        end = time.monotonic() + self.args.duration
        while True:
            if time.monotonic() >= end:
                self.ended = "duration"
                break
            if self.args.cases is not None and self.completed >= self.args.cases:
                self.ended = "case limit"
                break
            headroom = bench_switch.headroom_problem(self.live.console)
            if headroom:
                self.ended = "evidence headroom: " + headroom
                break
            name = self.random.choices(self.names, weights)[0]
            family, _, argument = CASES[name]
            if family == "pt" and self.relays.total(PT_RELAY) + PT_RELAY_OPS_MAX > stress.RELAY_BUDGET:
                self.ended = "relay budget"
                break
            detail, problems, error = None, [], None
            began = time.monotonic()
            try:
                detail, problems = self.runners[family](name, argument)
                problems = problems + self.restore_case()
            except Exception as caught:  # noqa: BLE001 - every failure ends the soak with its evidence.
                error = "%s: %s" % (type(caught).__name__, caught)
            self.case_record(name, detail, problems, error, began)
            if error or problems:
                multi.prepared_stops(self.live)  # COMMON: the prepared stops at once, before diagnosis.
                self.incident(name, detail, problems, error)
                self.ended = "anomaly"
                return 3
            self.completed += 1
            self.counts[name] = self.counts.get(name, 0) + 1
            self.trim()
            if self.completed % self.args.health_every == 0:
                self.sample_health()
        self.sample_health()
        return 0

    def case_record(self, name, detail, problems, error, began):
        """One line per case in ``cases.jsonl``: duration, problems, start verdicts and the evidence used."""
        budget = getattr(self.live.console, "evidence_budget", None)
        entry = {"when": time.strftime("%H:%M:%S"), "case": name, "seconds": round(time.monotonic() - began, 1),
                 "problems": problems, "error": error, "verdicts": (detail or {}).get("verdicts"),
                 "evidence_bytes": getattr(budget, "bytes", None), "evidence_records": getattr(budget, "records", None)}
        with open(self.folder / "cases.jsonl", "a", encoding="utf-8") as f:
            f.write(json.dumps(entry, default=str) + "\n")

    def summary(self):
        return {"completed": self.completed, "counts": self.counts, "ended": self.ended, "anomalies": self.anomalies,
                "relay_ops": self.relays.totals(), "health_samples": len(self.health), "seed": self.args.seed,
                "case_limit": self.args.cases, "duration_s": self.args.duration, "cases": self.names}


# --------------------------------------------------------------------------
# Session

def write_list():
    """Every field the soak changes: the start states' I/O words, mode and profile
    words (prompt 09), the group members' profile words and the velocity words."""
    cids = sorted({CASES[n][2] for n in CASES if CASES[n][0] == "starts"})
    entries = {(e["drive"], e["key"]): e for e in starts.write_list(cids) + multi.write_list("MOTION")}
    for a in VELOCITY_DRIVES:
        for key in records.SNAPSHOT_ROUTES["velocity"]["keys"]:
            entries.setdefault((a, key), {"drive": a, "key": key, "index": None, "candidate": None})
    out = list(entries.values())
    for a in PT_DRIVES:
        out += [e for e in pt_entries(a) if (e["drive"], e["key"]) not in entries or e["index"] is not None]
    return out


def pt_entries(address):
    """Fields one PT case changes: the layout, records 1-16 target and speed, the position mode."""
    out = [{"drive": address, "key": "io." + t, "index": None, "candidate": pt.LAYOUT[t]} for t in records.TERMINALS]
    for n in range(1, 17):
        out.append({"drive": address, "key": "records.position.target", "index": n, "candidate": None})
        out.append({"drive": address, "key": "records.position.speed", "index": n, "candidate": None})
    return out + [{"drive": address, "key": "drive.position-mode", "index": None, "candidate": 0}]


def pt_plan(baseline, address, image_level):
    """The in-case restoration of records and mode on ``address`` (the layout goes back through restore_layout)."""
    return records.restoration_plan(baseline, [e for e in pt_entries(address) if not e["key"].startswith("io.")],
                                    image_level, pt.pt_facts())


def final_plan(plan):
    """The ending restoration without the velocity snapshot steps: each velocity
    case restored its words on its own drive at once (D15 item 4)."""
    return dict(plan, steps=[s for s in plan["steps"] if not s.get("snapshot")])


def merge_facts(*sources):
    """Union of scenario facts ({drive: {purpose: {terminal: [facts]}}}), lists in first-seen order."""
    out = {}
    for source in sources:
        for key, value in source.items():
            if isinstance(value, dict):
                out[key] = merge_facts(out.get(key, {}), value)
            elif isinstance(value, list):
                out[key] = list(out.get(key, [])) + [v for v in value if v not in out.get(key, [])]
            else:
                out.setdefault(key, value)
    return out


def soak_facts():
    """The facts of the prompt 09 states and of prompt 13's layout and segments."""
    return merge_facts(starts.facts(), pt.pt_facts())


def scenario_record(args, image, baseline):
    entries = write_list()
    cid = args.case_id
    return dict(image=image, commit=args.commit, port=args.port,
                drives=[{"address": a, "required_enabled": True} for a in DRIVES], terminals=["x0", "x1", "x2", "x3", "y0"],
                commands=["per case: bench_input_starts.run_case (enter state, arm position/trigger/inspect, disarm), "
                          "bench_multi.submit (group plans, group stop), bench_velocity.velocity_run (velocity cell, "
                          "velocity restore on the same drive), bench_pt.setup_table + run_regression (records 1-8, layout, PIN0 on relay2, arm segment, arm trigger; restore_layout and the records/mode plan on the same drive); then group clear, enter state BASE, io diff, read state 1 2 4",
                          "restore: stop fast 1, 2, 4; restoration plan (profile stage, I/O words, velocity restore)"],
                cases=[{"id": cid, "required": True, "assertion": "wired soak share: zero anomalies, restoration clean",
                        "required_levels": ["drive-reported", "controller-timestamped"]}],
                running_members=[], write_list=entries,
                bounds={"duration_s": args.duration, "cases": args.cases, "weights": {n: c[1] for n, c in CASES.items()},
                        "velocity": {"drives": VELOCITY_DRIVES, "rpm": VELOCITY_RPM, "ms": VELOCITY_MS},
                        "relative": RELATIVE, "offsets": multi.OFFSETS, "long": multi.LONG,
                        "exposure": "finite moves end themselves; velocity ends by the controller's stop after 2 s; if "
                                    "the controller or harness dies during velocity, rotation continues until a "
                                    "restarted controller stops it (D11)"},
                prepared_stop=["group stop fast; stop fast 1, 2, 4 (every bench output drops first)"],
                restoration_plan=records.restoration_plan(baseline, entries, args.image_level, soak_facts(),
                                                         context=SNAPSHOTS),
                left_changed=[], expected_evidence={cid: "soak.json, health.jsonl, incident files, session logs"},
                abort_conditions=["any non-PASS case verdict or exception", "an I/O word differing from the baseline after "
                                  "a case", "a drive not read stopped and alarm-free after a case"],
                cleanup=["prepared stops", "restoration plan, read back", "group clear", "bench outputs off",
                         "ending reads", "close COM13"],
                max_age_ms=records.MAX_AGE_MS, facts=soak_facts(), image_level=args.image_level)


def session(folder, live, work, evidence_mib):
    """``bench_multi.session`` with the soak's evidence budget: the console streams
    every line to the JSONL and keeps none in host memory, and the soak trims its
    own events, so the budget bounds disk use only (about 2.5 MB per case)."""
    def run(console, record):
        live.console = console
        try:
            work(console)
        except BaseException:
            if console.synchronized:
                multi.prepared_stops(live)
            raise
        record["workload_verified"] = True

    return run_recorded(port=live.args.port, scenario="wired-soak", inputs={"case": live.args.case_id}, run=run,
                        address=1, timeout_s=live.timeout, firmware=live.args.firmware, refresh=False,
                        paths=dict(summary=folder / "summary.json", jsonl=folder / "session.jsonl",
                                   console=folder / "console.log"),
                        max_records=evidence_mib * EVIDENCE_RECORDS_PER_MIB, max_bytes=evidence_mib * 1024 * 1024)


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", default="COM13")
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True, help="running image (SHA-256 recorded)")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--case-id", choices=("K1", "K2", "smoke"), default="K1",
                        help="smoke: a short live check of the driver, not a K1/K2 result")
    parser.add_argument("--image-level", default="20")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--duration", type=float, default=3600.0)
    parser.add_argument("--cases", type=int, help="end after this many completed cases")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--only", nargs="+", choices=tuple(CASES), help="only these cases")
    parser.add_argument("--health-every", type=int, default=25)
    parser.add_argument("--evidence-mib", type=int, default=512,
                        help="session evidence budget; about 2.5 MB per case, so 512 MiB is about 2 h")
    parser.add_argument("--relay-used", type=stress.relay_totals, required=True,
                        help="relay operations since prompt 01, relay1=N,relay2=N")
    args = parser.parse_args(argv)
    if args.port != records.FIXTURE["board"]["port"]:
        parser.error("the wired bench is on %s" % records.FIXTURE["board"]["port"])
    if args.cases is not None and args.cases < 1 or args.health_every < 1 or not 16 <= args.evidence_mib <= 2048:
        parser.error("--cases and --health-every need positive counts; --evidence-mib 16..2048")
    args.repetitions = 1  # One trial per start case (bench_input_starts.run_case).
    return args


def main(argv=None):
    args = arguments(argv)
    folder = records.RunFolder.from_path(args.run_dir)
    if folder.prompt != "20":
        raise SystemExit("soak run folders are wired bench 20 (YYYY-MM-DD_wb20_slug)")
    image = {"path": args.firmware.as_posix(), "sha256": records.sha256_file(args.firmware)}
    baseline = records.load_baseline()
    folder.create()
    scenario = records.write_scenario(folder, **json.loads(json.dumps(scenario_record(args, image, baseline))))
    run = _Run(scenario, folder)
    live = multi.Live(run, baseline, args)
    vlive = velocity.Live(None, run, baseline, argparse.Namespace(**vars(args), address=VELOCITY_DRIVES[0]), args.timeout)
    ptlive = pt.Live(None, run, baseline, args, args.timeout)
    soak = Soak(live, vlive, args, folder.path, ptlive)
    problems = soak.plan_check()
    folder.write_json("plan_check.json", {"steps": plan_steps(soak.names), "problems": problems})
    if problems:
        raise SystemExit("plan check refused: " + "; ".join(problems))
    code, error = 3, None

    def work(console):
        nonlocal code
        live.console = vlive.console = ptlive.console = console
        live.detail["motion"] = True
        multi.baseline_before(live)
        soak.share_declarations(records.check_declarations(console, timeout_s=live.timeout))
        starts.enter_state(live, "BASE")
        soak.velocity_plans = {a: records.restoration_plan(
            baseline, [e for e in scenario["write_list"] if e["drive"] == a and records.snapshot_route(e["key"])],
            args.image_level, starts.facts(), context=SNAPSHOTS) for a in VELOCITY_DRIVES}
        soak.pt_plans = {a: pt_plan(baseline, a, args.image_level) for a in PT_DRIVES}
        try:
            code = soak.loop()
        finally:
            if console.synchronized:
                console.command("group-clear", timeout_s=live.timeout)
                _stop_restore(console, live, run, final_plan(scenario["restoration_plan"]), live.timeout)
            _finish(console, run, live.timeout)

    try:
        records.check_board(args.port)
        session(folder, live, work, args.evidence_mib)
    except (Exception, KeyboardInterrupt) as caught:
        error = caught
        run.deviations.append("run stopped: %s" % caught)
    folder.write_json("soak.json", dict(soak.summary(), error=str(error) if error else None))
    verdict = "PASS" if code == 0 and error is None and not soak.anomalies else "FAIL"
    reason = "%d cases, ended by %s, %d anomalies" % (soak.completed, soak.ended, len(soak.anomalies))
    for level in scenario["cases"][0]["required_levels"]:
        run.verdict(args.case_id, level, verdict, reason)
    for entry in scenario["write_list"]:  # One stage restores 0x0021-0x0025 together (as bench_multi).
        if entry["key"].startswith("profile.") and (entry["drive"], entry["key"], entry.get("index")) not in run.fields:
            window = run.fields.get((entry["drive"], "profile.acceleration", None))
            if window and window["restored"]:
                value = (records.baseline_field(baseline, entry["drive"], entry["key"]) or {}).get("value")
                run.field(entry, baseline=value, ending=value, restored=True,
                          reason="restored and read back with the drive's 0x0021/5 stage window")
    state = run.cases[args.case_id]
    cases = [{**scenario["cases"][0], "verdicts": state["verdicts"], "reason": state["reason"],
              "evidence": [p for p in ["soak.json", "health.jsonl", "plan_check.json"] if (folder / p).exists()]}]
    records.write_result(folder, commit=args.commit, image=image, cases=cases,
                         restoration={"outputs_off": run.outputs_off, "fields": list(run.fields.values()),
                                      "drives": run.drives, "port_closed": True},
                         deviations=run.deviations)
    report = records.check_run(folder.path)
    print(json.dumps({"soak": soak.summary(), "check": report}, indent=1, default=str))
    return code if error is None else 3


if __name__ == "__main__":
    sys.exit(main())
