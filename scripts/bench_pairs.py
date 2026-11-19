"""Wired bench 10: paired-value encodings (`paired-values` scenario).

One case per run folder (prompt 10 case tables):

- P1 soft limits at order 0 on drive 4: positive 0x00010002, negative -500.
- P2 the sixteen PT target pairs on drive 4 (distinct asymmetric words,
  segment 1 also -500).
- P3 word order 1 on drive 4: the position words, P1's positive limit and P2's
  segment 1 at 0x0019 = 1; storage only.
- P4 P1 and P2 segment 1 on drives 1 and 2 at order 0.
- E1 home offset coordinate on drive 4: method 35, auxiliary 6, +500 and -500
  at order 0, then +500 at order 1.
- E2 home offset motion on drive 4 (auxiliary 3), only after an unambiguous E1.
- E3P / E3N soft-limit stop trials on drive 4 (independent setups).

Cases P1-P4, P3D, E1-E3 and the restorations RB, E3X, E2R ran on the
experimental images A-A3 (their routes were removed or replaced after the
measurement); R10 is the production regression of the promoted forms:

- R10 on the production image: the soft-limit setter `driver set
  positive-limit|negative-limit` (one FC10 and a two-word readback each),
  decoded driver reads, a nonzero home offset with method 35 and auxiliary 6
  from a HOMED-low start (motor-release then enable), refusal of the
  experiment route, and restoration through the same routes.

Every experiment write goes through the experimental image's named admission
(`driver experiment ...`, decision D4); restoration uses the exact words of a
checked read of the same drive taken before the first write (D7) or the
ordinary routes. Nothing is replayed; an uncertain write ends the case. The
scenario record is written before the port opens; result.json and both run
checks follow. Live serial needs pyserial; nothing here infers a drive fact.
"""
import argparse
import json
from pathlib import Path
import sys
import time

import bench_probe as probe
import bench_records as records
from bench_session import run_recorded

OWNER = 4
P1_POSITIVE, P1_NEGATIVE = [0x0001, 0x0002], [0xFFFF, 0xFE0C]
E3_POSITIVE, E3_NEGATIVE = [0x0000, 0x012C], [0xFFFF, 0xFED4]
SEGMENT_WORDS = {n: [n, 0x1000 + n] for n in range(1, 17)}
MINUS_500 = [0xFFFF, 0xFE0C]
LIMIT_FIRST = {"positive": 0x37, "negative": 0x39}
HOME_FACTS = ("mechanics", "rates", "ramp", "zero-offset", "auxiliary", "inputs")
PREPARED_STOP = ["stop fast A (prepared; sent at once on unexpected motion)"]
ABORTS = ["any readback mismatch (FAIL, words kept, no retry)", "an uncertain write (case ends, nothing replayed)",
          "motion in a storage or coordinate case (prepared stop fast)",
          "E2: more than 1000 steps of travel or RUNNING beyond 3 s (stop fast)",
          "E3: no stop by the limit within the 1000 ms run (velocity route's own timed stop)"]

CASES = {
    "P1": {"drives": [OWNER], "levels": ["acknowledged", "read back"],
           "assertion": "drive 4 at 0x0019 = 0: positive limit 0x00010002 and negative -500 are acknowledged and read back "
                        "exactly ([1,2], [65535,65036]); restored to the pre-run words and read back"},
    "P2": {"drives": [OWNER], "levels": ["acknowledged", "read back"],
           "assertion": "drive 4 at 0x0019 = 0: segments 1-16 pulse pairs [n, 0x1000+n] and segment 1 -500 are "
                        "acknowledged and read back exactly; restored to the pre-run words and read back"},
    "P3": {"drives": [OWNER], "levels": ["read back", "drive-reported"],
           "assertion": "drive 4 at 0x0019 = 1: position words swapped at the same position; P1 positive limit and "
                        "P2 segment 1 read back exactly; restored; 0x0019 = 0 read back"},
    "P4": {"drives": [1, 2], "levels": ["acknowledged", "read back"],
           "assertion": "drives 1 and 2 at 0x0019 = 0: P1 limits and P2 segment 1 read back exactly as on drive 4; "
                        "each drive restored before the next is selected"},
    "E1": {"drives": [OWNER], "levels": ["acknowledged", "drive-reported"],
           "assertion": "method 35, auxiliary 6: home with +500 and -500 at 0x0019 = 0 ends with 0x000A = the offset; "
                        "at 0x0019 = 1 a +500 home shows whether the offset follows the word order"},
    "E2": {"drives": [OWNER], "levels": ["acknowledged", "drive-reported", "controller-timestamped"],
           "assertion": "method 35, auxiliary 3: home with +500 then -500; RUNNING and Y0 running edges show whether the "
                        "drive moves by the offset; end coordinates recorded"},
    "E3P": {"drives": [OWNER], "levels": ["read back", "drive-reported"],
            "assertion": "home 35/aux 7 to 0, limits +300/-300 at order 0, 0x0018 = 1; velocity +30 rpm 1000 ms "
                         "stops near +300 with 0x0007 bit 5"},
    "P3D": {"drives": [OWNER], "levels": ["drive-reported"],
            "assertion": "diagnostic: position words 0x000A/0B and the active target 0x0024/25 at 0x0019 = 0 and 1, "
                         "three spaced state reads each, profile read each; 0x0019 restored to 0"},
    "E3X": {"drives": [OWNER], "levels": ["drive-reported", "read back"],
            "assertion": "step 8 exit of an interrupted E3 trial: stop fast, alarm-clear, motor-release, enable; the "
                         "soft-limit flag clears or not (recorded); if it clears, 0x0018 = 0 and the limit words and "
                         "auxiliary return to the baseline, read back"},
    "E2R": {"drives": [OWNER], "levels": ["read back"],
            "assertion": "restoration after the interrupted E2 run in the same binding: homing words from the binding's "
                         "pinned home read (--home-snapshot), auxiliary 0 and Y0 0 through the ordinary routes, read back"},
    "R10": {"drives": [OWNER], "levels": ["acknowledged", "read back", "drive-reported"],
            "assertion": "production image: soft limits +300/-300 through the typed setter (FC10 each, readback, decoded "
                         "read) and back to [0,0]; home 35 with auxiliary 6 and offsets +500, -500 from a HOMED-low "
                         "start end at the offset; the experiment route refused before TX; homing words, auxiliary "
                         "and limits restored"},
    "RB": {"drives": [OWNER], "levels": ["acknowledged", "read back"],
           "assertion": "limit and PT pulse pairs of drive 4 that differ from the 01 baseline are written back to the "
                        "baseline words through the named baseline restoration candidates and read back"},
    "E3N": {"drives": [OWNER], "levels": ["read back", "drive-reported"],
            "assertion": "home 35/aux 7 to 0, limits +300/-300 at order 0, 0x0018 = 1; velocity -30 rpm 1000 ms "
                         "stops near -300 with 0x0007 bit 6"},
}


def signed(words, order=0):
    high, low = (words[0], words[1]) if order == 0 else (words[1], words[0])
    value = high << 16 | low
    return value - (1 << 32) if value & 0x80000000 else value


class Live:
    """Run state of one case: replies, rows, snapshots, fields."""

    def __init__(self, console, run, timeout):
        self.console, self.run, self.timeout = console, run, timeout
        self.events, self.rows = [], []

    def call(self, name, **kwargs):
        reply = self.console.command(name, timeout_s=self.timeout, **kwargs)
        self.events.append({"command": name, "arguments": {k: list(v) if isinstance(v, tuple) else v
                                                           for k, v in kwargs.items()},
                            "ok": reply.get("ok"), "result": reply.get("result"), "outcome": reply.get("outcome"),
                            "operation_id": reply.get("operation_id")})
        return reply

    def checked(self, name, **kwargs):
        reply = self.call(name, **kwargs)
        if not reply.get("ok"):
            raise probe.BenchError("%s refused/failed: %s" % (name, reply.get("result", reply.get("outcome"))))
        return reply

    def select(self, address):
        records.use_drive(self.console, address, timeout_s=self.timeout)

    def rebind(self, address):
        """Select another drive and back: a new binding whose first checked read of
        each pair window is pinned in the firmware's snapshot ring (slot 0), so the
        case's pre-run source survives any number of later reads (P3 first run:
        three later driver reads evicted the source from the rotating slots)."""
        other = 1 if address != 1 else 4
        records.use_drive(self.console, other, timeout_s=self.timeout)
        records.use_drive(self.console, address, timeout_s=self.timeout)

    def fresh(self, address):
        """Config then state of the selected drive (the experiment admission's reads)."""
        config = self.checked("read-config", address=address)
        state = self.checked("read-state", address=address)
        if not probe.drive_reported_stopped(state):
            raise probe.BenchError("drive %d is not reported stopped and alarm-free" % address)
        return config, state

    def config_raw(self, address):
        return self.checked("read-config", address=address)["config"]["raw"]

    def no_pt_trigger(self):
        """io read of drives 1, 2, 4: no input assigned function 12 (PT trigger)."""
        rows = {}
        for address in records.FIXTURE["input_drives"]:
            words = self.checked("io", driver_args=("read",), address=address)["observation"]["raw"]
            rows[address] = words
            if 12 in words[1:5]:
                raise probe.BenchError("drive %d has an input with function 12: %s" % (address, words[1:5]))
        return rows

    def driver(self, address):
        reply = self.checked("driver", driver_args=("read",), address=address)
        o = reply["observation"]
        return reply["operation_id"], {"raw": o["raw"], "positive": o["positive_words"], "negative": o["negative_words"]}

    def segment(self, address, n):
        reply = self.checked("segment", driver_args=("position", str(n), "read"), address=address)
        return reply["operation_id"], reply["observation"]["raw"][3:5]

    def experiment(self, tokens, label):
        """One experiment session: admission, then inspect until it ends. Never resent."""
        reply = self.call("driver-experiment", host_args=tuple(str(t) for t in tokens))
        row = {"label": label, "tokens": [str(t) for t in tokens], "admitted": reply.get("ok"),
               "result": reply.get("result")}
        self.rows.append(row)
        if not reply.get("ok"):
            row["final"] = reply
            return row
        deadline = time.monotonic() + 5
        final = reply
        while final.get("pending") and time.monotonic() < deadline:
            time.sleep(.02)
            final = self.call("driver-experiment", host_args=("inspect",))
        row.update(final={k: final.get(k) for k in ("operation_id", "snapshot_id", "kind", "first", "count", "words",
                                                     "readback", "execution", "error", "session_ok", "unsettled",
                                                     "write_evidence", "read_evidence")},
                   ok=final.get("session_ok") is True, uncertain=bool(final.get("pending") or final.get("unsettled") or
                                                                      final.get("execution") == "unknown"))
        if row["uncertain"]:
            raise probe.BenchError("experiment write %s uncertain; not replayed" % label)
        return row

    def pair(self, address, first, words, label):
        self.fresh(address)
        return self.experiment(("pair", first, words[0], words[1]), label)

    def pair_restore(self, address, first, snapshot, label):
        self.fresh(address)
        return self.experiment(("pair", first, "restore", snapshot), label)

    def setting(self, address, key, value, baseline, facts):
        """One ordinary DRIVE/home-auxiliary write with CONFIGURE records; read back."""
        entry = {"drive": address, "key": key, "index": None, "candidate": value}
        steps, refusal = records.plan_entry(baseline, entry, "10", facts, restore=False)
        if refusal:
            raise probe.BenchError("no ordinary route for %s: %s" % (key, refusal["reason"]))
        step = dict(steps[0], target=value, candidate=value)
        command, kwargs, text = records.write_call({"key": key, "index": None}, value, address)
        step["write"] = {"command": command, "kwargs": kwargs, "text": text}
        outcome = records.apply_write(self.console, step, timeout_s=self.timeout)
        ending, _ = records.readback(self.console, step, timeout_s=self.timeout)
        row = {"label": text, "written": outcome["written"], "uncertain": outcome["uncertain"], "readback": ending,
               "result": outcome["result"]}
        self.rows.append(row)
        if outcome["uncertain"] or not outcome["written"] or ending != value:
            raise probe.BenchError("%s %s: written %s, uncertain %s, readback %s" %
                                   (key, value, outcome["written"], outcome["uncertain"], ending))
        return row

    def position(self, address):
        state = self.checked("read-state", address=address)
        blocks = records._blocks(state)
        return blocks.get(2, {}).get("position_words"), blocks.get(0, {}).get("raw_motion"), state

    def home(self, address, offset, facts):
        """home read, qualification with the stated facts, home 35 60 30 100 OFFSET; wait for the terminal."""
        self.checked("read-config", address=address)
        self.checked("read-state", address=address)
        self.checked("home-read", address=address)
        self.checked("home-qualification-install", host_args=("35", "60", "30", "100", *facts))
        handle = self.console.begin("home", home_args=("35", "60", "30", "100", str(offset) if offset else "zero"),
                                    address=address, timeout_s=max(self.timeout, 40))
        row = {"label": "home 35 60 30 100 %s" % (offset or "zero"), "admitted": handle.accepted}
        terminal = self.console.wait(handle, release=True)
        row["terminal"] = {k: terminal.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                        "setup_execution", "execution",
                                                        "staging_words", "raw_position_words", "offset",
                                                        "expected_position", "end_known", "end_position",
                                                        "running_observed", "polls", "uncertain", "result")}
        self.rows.append(row)
        if terminal.get("uncertain"):
            # Method 35 does not move (auxiliary 6/7). A completion the polls did
            # not see (06: HOMED clears for about 13 ms from a homed state) leaves
            # the trigger uncertain: it is never resent; a normal stop reconciles
            # the application's conflict and a fresh read records the state.
            row["reconcile"] = self.call("stop", stop_policy="normal", address=address).get("outcome")
        return row

    def home_bounded(self, address, offset, facts, limit_steps=1000, limit_s=3.0):
        """E2: a home that may run the offset, sampled by read state while it is
        pending; the prepared stop fast at once if a sample shows more than
        limit_steps of travel or RUNNING beyond limit_s. Never resent."""
        self.checked("read-config", address=address)
        start = state_flags(self, address)
        self.checked("home-read", address=address)
        self.checked("home-qualification-install", host_args=("35", "60", "30", "100", *facts))
        handle = self.console.begin("home", home_args=("35", "60", "30", "100", str(offset)), address=address,
                                    timeout_s=max(self.timeout, 40))
        row = {"label": "home 35 60 30 100 %s (bounded)" % offset, "admitted": handle.accepted, "start": start,
               "samples": []}
        began = time.monotonic()
        while handle.accepted and handle.terminal is None and time.monotonic() - began < 10:
            sample = state_flags(self, address)
            sample["t_s"] = round(time.monotonic() - began, 3)
            row["samples"].append(sample)
            travel = abs(sample["position"] - start["position"]) if None not in (sample["position"], start["position"]) else 0
            if (sample["running"] and sample["t_s"] > limit_s) or travel > limit_steps:
                row["safety_stop"] = self.call("stop", stop_policy="fast", address=address).get("outcome")
                break
            time.sleep(.05)
        terminal = self.console.wait(handle, release=True)
        row["terminal"] = {k: terminal.get(k) for k in ("ok", "state", "outcome", "status", "detail", "completion",
                                                        "setup_execution", "execution", "staging_words",
                                                        "raw_position_words", "offset", "expected_position", "end_known",
                                                        "end_position", "running_observed", "polls", "uncertain", "result")}
        if terminal.get("uncertain"):
            row["reconcile"] = self.call("stop", stop_policy="normal", address=address).get("outcome")
        row["end"] = state_flags(self, address)
        self.rows.append(row)
        return row

    def clear_position(self, address):
        """Diagnostic: does a position clear also clear HOMED (a low baseline for the next home 35)?"""
        self.checked("read-config", address=address)
        self.checked("read-state", address=address)
        self.checked("position-clear-qualification-install", host_args=("clear-semantics",))
        reply = self.call("position-clear", address=address)
        row = {"label": "position-clear", "ok": reply.get("ok"), "outcome": reply.get("outcome")}
        self.rows.append(row)
        return row


def scenario_record(args, case_id, image, commit):
    case = CASES[case_id]
    cases = [{"id": case_id, "required": True, "assertion": case["assertion"], "required_levels": case["levels"]}]
    drives = case["drives"]
    write_list, left = [], []
    for address in drives:
        if case_id == "R10":
            write_list += [{"drive": address, "key": "homing.window", "candidate": "home staging"},
                           {"drive": address, "key": "homing.auxiliary", "candidate": 6}]
        if case_id in ("P1", "P3", "P4", "E3P", "E3N", "RB", "E3X", "R10"):
            write_list += [{"drive": address, "key": "drive.positive-limit", "candidate": "experiment"},
                           {"drive": address, "key": "drive.negative-limit", "candidate": "experiment"}]
        if case_id in ("P2", "P3", "P4"):
            for n in (range(1, 17) if case_id == "P2" else (1,)):
                write_list.append({"drive": address, "key": "segment.position.%d.pulses" % n, "candidate": "experiment"})
        if case_id in ("P3", "P3D"):
            write_list.append({"drive": address, "key": "drive.word-order", "candidate": 1})
        if case_id in ("E1", "E2", "E3P", "E3N"):
            write_list += [{"drive": address, "key": "homing.window", "candidate": "home staging"},
                           {"drive": address, "key": "homing.auxiliary",
                            "candidate": {"E1": 6, "E2": 3}.get(case_id, 7)}]
        if case_id == "E1":
            write_list.append({"drive": address, "key": "drive.word-order", "candidate": 1})
        if case_id == "E2":
            write_list.append({"drive": address, "key": "io.y0", "candidate": 2})
        if case_id == "E2R":
            write_list += [{"drive": address, "key": "homing.window", "candidate": "pinned home read"},
                           {"drive": address, "key": "homing.auxiliary", "candidate": 0},
                           {"drive": address, "key": "io.y0", "candidate": 0}]
        if case_id == "E3X":
            write_list += [{"drive": address, "key": "drive.soft-limit", "candidate": 0},
                           {"drive": address, "key": "homing.auxiliary", "candidate": 0}]
        if case_id in ("E3P", "E3N"):
            write_list += [{"drive": address, "key": "drive.soft-limit", "candidate": 1},
                           {"drive": address, "key": "jog.window", "candidate": "velocity staging"}]
    plan = {"routes": {
        "drive.positive-limit / drive.negative-limit": "driver experiment pair 55|57 restore SNAPSHOT (D7: exact words of "
                                                       "this run's driver read before the first write)",
        "segment.position.N.pulses": "driver experiment pair FIRST restore SNAPSHOT (D7: segment position N read before "
                                     "the first write)",
        "drive.word-order": "driver set word-order 0 with CONFIGURE records (03), 0x0018 known 0",
        "drive.soft-limit": "driver set soft-limit 0 with CONFIGURE records (refused while a flag is set: step 8 exit)",
        "homing.window": "home restore SNAPSHOT (06; home read of this run before the first write)",
        "homing.auxiliary": "home auxiliary 0 with CONFIGURE records",
        "jog.window": "velocity restore SNAPSHOT (06; velocity read of this run before the first write)",
        "io.y0": "io set y0 0 with ASSIGN facts indicator either"},
        "order": "restore each drive's words before selecting another drive, resetting or flashing"}
    return dict(image=image, commit=commit, port=args.port,
                drives=[{"address": a, "required_enabled": True} for a in drives],
                terminals=["y0"] if case_id == "E2" else [],
                commands=["see bench_pairs.py case %s" % case_id], cases=cases, running_members=[],
                write_list=write_list,
                bounds={"offsets": [500, -500], "velocity_rpm": 30, "velocity_ms": 1000, "travel_turns": 0.5,
                        "limits": [300, -300], "e2_abort_steps": 1000, "e2_abort_running_s": 3},
                prepared_stop=PREPARED_STOP, restoration_plan=plan, left_changed=left,
                expected_evidence={level: "per case table, prompt 10" for level in CASES[case_id]["levels"]},
                abort_conditions=ABORTS,
                cleanup=["restoration per field, read back", "bench outputs off", "ending reads of every case drive",
                         "close COM13"],
                max_age_ms=records.MAX_AGE_MS, facts={str(a): {"configure": {}} for a in drives},
                experiment={"decision": "D4", "prompt": "10", "date": "2026-11-19", "image_sha256": image["sha256"],
                            "candidates": {"P1": [P1_POSITIVE, P1_NEGATIVE], "P2": SEGMENT_WORDS,
                                           "P2_minus_500": MINUS_500, "E3": [E3_POSITIVE, E3_NEGATIVE],
                                           "E1_offsets": [500, -500]}})


class Result:
    def __init__(self, case_id):
        self.case_id = case_id
        self.verdicts, self.reasons, self.fields, self.drives = {}, [], [], []
        self.outputs_off = False
        self.deviations = []
        self.data = {}

    def verdict(self, level, value, reason=None):
        self.verdicts[level] = value
        if reason:
            self.reasons.append(reason)

    def field(self, drive, key, baseline, ending, restored, reason=None, uncertain=False):
        self.fields.append({"drive": drive, "key": key, "index": None, "baseline": baseline, "ending": ending,
                            "restored": restored, "uncertain": uncertain, "storage": "RAM", "reason": reason})


# --------------------------------------------------------------------------
# Cases

def limit_case(live, result, address, candidates):
    """Write the two limits, verify, restore to the pre-run words, verify."""
    live.rebind(address)
    live.no_pt_trigger()
    live.fresh(address)
    source, before = live.driver(address)
    out = {"source": source, "before": before, "writes": []}
    rows = []
    for name, words in (("positive", candidates[0]), ("negative", candidates[1])):
        row = live.pair(address, LIMIT_FIRST[name], words, "%s limit %s" % (name, words))
        rows.append(row)
        out["writes"].append(row)
        if not row.get("admitted"):
            break
    live.fresh(address)
    _, after = live.driver(address)
    out["after"] = after
    for name in ("positive", "negative"):
        restore = live.pair_restore(address, LIMIT_FIRST[name], source, "restore %s limit" % name)
        out["writes"].append(restore)
    live.fresh(address)
    _, ending = live.driver(address)
    out["ending"] = ending
    for name in ("positive", "negative"):
        result.field(address, "drive.%s-limit" % name, before[name], ending[name], ending[name] == before[name],
                     None if ending[name] == before[name] else "limit words not restored")
    return out, rows


def run_p1(live, result, drives=(OWNER,)):
    data = {}
    for address in drives:
        out, rows = limit_case(live, result, address, (P1_POSITIVE, P1_NEGATIVE))
        data[address] = out
        ack = all(r.get("final", {}).get("execution") == "acknowledged" for r in rows) and len(rows) == 2
        exact = out["after"]["positive"] == P1_POSITIVE and out["after"]["negative"] == P1_NEGATIVE and \
            all(r.get("ok") for r in rows)
        data[address].update(acknowledged=ack, exact=exact)
    return data


def segment_case(live, result, address, plan):
    """plan: list of (n, words); read every record first (snapshots), write, verify, restore, verify."""
    live.rebind(address)
    live.no_pt_trigger()
    out = {"sources": {}, "before": {}, "writes": [], "after": {}, "ending": {}}
    for n in sorted({n for n, _ in plan}):
        live.fresh(address)
        out["sources"][n], out["before"][n] = live.segment(address, n)
    for n, words in plan:
        row = live.pair(address, 0x60 + 6 * (n - 1), words, "segment %d %s" % (n, words))
        row["segment"], row["candidate"] = n, words
        out["writes"].append(row)
        live.fresh(address)
        out["after"][n] = live.segment(address, n)[1]
        row["independent_readback"] = out["after"][n]
    for n in sorted(out["sources"]):
        restore = live.pair_restore(address, 0x60 + 6 * (n - 1), out["sources"][n], "restore segment %d" % n)
        out["writes"].append(restore)
        live.fresh(address)
        out["ending"][n] = live.segment(address, n)[1]
        result.field(address, "segment.position.%d.pulses" % n, out["before"][n], out["ending"][n],
                     out["ending"][n] == out["before"][n],
                     None if out["ending"][n] == out["before"][n] else "pulse words not restored")
    return out


def run_p2(live, result):
    plan = [(n, SEGMENT_WORDS[n]) for n in range(1, 17)] + [(1, MINUS_500)]
    out = segment_case(live, result, OWNER, plan)
    writes = [r for r in out["writes"] if "segment" in r]
    out["per_segment"] = [{"segment": r["segment"], "candidate": r["candidate"], "ack": r.get("final", {}).get("execution"),
                           "readback": r.get("final", {}).get("readback"), "independent": r.get("independent_readback"),
                           "pass": r.get("ok") is True and r.get("independent_readback") == r["candidate"]}
                          for r in writes]
    return out


def run_p3(live, result, baseline):
    address = OWNER
    facts = {str(address): {"configure": {}}}
    live.rebind(address)
    live.no_pt_trigger()
    live.fresh(address)
    out = {}
    source, before = live.driver(address)
    out["limit_source"], out["limit_before"] = source, before
    live.fresh(address)
    out["segment_source"], out["segment_before"] = live.segment(address, 1)
    out["position_order0"] = live.position(address)[0]
    out["order_set"] = live.setting(address, "drive.word-order", 1, baseline, facts)
    out["position_order1"] = live.position(address)[0]
    out["writes"] = [live.pair(address, 0x37, P1_POSITIVE, "positive limit at order 1"),
                     live.pair(address, 0x60, SEGMENT_WORDS[1], "segment 1 at order 1")]
    live.fresh(address)
    _, out["limit_after"] = live.driver(address)
    live.fresh(address)
    out["segment_after"] = live.segment(address, 1)[1]
    out["restores"] = [live.pair_restore(address, 0x37, source, "restore positive limit (order 1)"),
                       live.pair_restore(address, 0x60, out["segment_source"], "restore segment 1 (order 1)")]
    out["order_restore"] = live.setting(address, "drive.word-order", 0, baseline, facts)
    live.fresh(address)
    _, out["limit_ending"] = live.driver(address)
    live.fresh(address)
    out["segment_ending"] = live.segment(address, 1)[1]
    out["position_order0_after"] = live.position(address)[0]
    out["order_ending"] = live.config_raw(address)["word_order"]
    result.field(address, "drive.positive-limit", before["positive"], out["limit_ending"]["positive"],
                 out["limit_ending"]["positive"] == before["positive"])
    result.field(address, "drive.negative-limit", before["negative"], out["limit_ending"]["negative"],
                 out["limit_ending"]["negative"] == before["negative"])
    result.field(address, "segment.position.1.pulses", out["segment_before"], out["segment_ending"],
                 out["segment_ending"] == out["segment_before"])
    result.field(address, "drive.word-order", 0, out["order_ending"], out["order_ending"] == 0)
    return out


def run_p4(live, result):
    data = {}
    for address in (1, 2):
        out, rows = limit_case(live, result, address, (P1_POSITIVE, P1_NEGATIVE))
        seg = segment_case(live, result, address, [(1, SEGMENT_WORDS[1])])
        data[address] = {"limits": out, "segment": seg,
                         "acknowledged": all(r.get("final", {}).get("execution") == "acknowledged" for r in rows) and
                         all(r.get("final", {}).get("execution") == "acknowledged" for r in seg["writes"] if "segment" in r),
                         "exact": out["after"]["positive"] == P1_POSITIVE and out["after"]["negative"] == P1_NEGATIVE and
                         seg["after"].get(1) == SEGMENT_WORDS[1]}
    return data


def home_snapshot(live, address):
    reply = live.checked("home-read", address=address)
    h = reply["homing_parameters"]
    return {"operation_id": reply["operation_id"], "address": address, "auxiliary": h["raw_auxiliary"],
            "words": [h["raw_method"], h["raw_search_speed"], h["raw_return_speed"], h["raw_ramp_time"],
                      *h["raw_offset_words"]]}


def home_restore(live, address, snapshot):
    """06's home restore of the run's pre-run words; read back."""
    live.checked("read-config", address=address)
    live.checked("read-state", address=address)
    reply = live.call("home-restore", host_args=(str(snapshot["operation_id"]),))
    deadline = time.monotonic() + 5
    while reply.get("pending") and time.monotonic() < deadline:
        time.sleep(.02)
        reply = live.call("home-restore", host_args=("inspect",))
    live.checked("read-config", address=address)
    after = home_snapshot(live, address)
    row = {"label": "home restore %d" % snapshot["operation_id"], "result": reply.get("result"),
           "session_ok": reply.get("session_ok"), "error": reply.get("error"), "words": after["words"]}
    live.rows.append(row)
    return after


def run_e1(live, result, baseline):
    address = OWNER
    facts = {str(address): {"configure": {}}}
    live.select(address)
    live.fresh(address)
    out = {"snapshot": home_snapshot(live, address)}
    out["position_before"] = live.position(address)[0]
    out["auxiliary_set"] = live.setting(address, "homing.auxiliary", 6, baseline, facts)
    homes = []
    for offset in (500, -500):
        row = live.home(address, offset, HOME_FACTS + ("offset",))
        row["state_after"] = state_flags(live, address)
        row["position_after"] = row["state_after"]["position_words"]
        row["clear"] = live.clear_position(address)
        row["state_after_clear"] = state_flags(live, address)
        homes.append(row)
    out["order0"] = homes
    out["order_set"] = live.setting(address, "drive.word-order", 1, baseline, facts)
    row = live.home(address, 500, HOME_FACTS + ("offset",))
    row["state_after"] = state_flags(live, address)
    row["position_after"] = row["state_after"]["position_words"]
    row["clear"] = live.clear_position(address)
    row["state_after_clear"] = state_flags(live, address)
    out["order1"] = row
    out["order_restore"] = live.setting(address, "drive.word-order", 0, baseline, facts)
    out["position_order0_after"] = live.position(address)[0]
    after = home_restore(live, address, out["snapshot"])
    out["home_restore"] = after
    out["auxiliary_restore"] = live.setting(address, "homing.auxiliary", out["snapshot"]["auxiliary"], baseline, facts)
    ending = home_snapshot(live, address)
    out["ending"] = ending
    result.field(address, "homing.window", out["snapshot"]["words"], ending["words"],
                 ending["words"] == out["snapshot"]["words"],
                 None if ending["words"] == out["snapshot"]["words"] else "homing words not restored")
    result.field(address, "homing.auxiliary", out["snapshot"]["auxiliary"], ending["auxiliary"],
                 ending["auxiliary"] == out["snapshot"]["auxiliary"])
    order = live.config_raw(address)["word_order"]
    result.field(address, "drive.word-order", 0, order, order == 0)
    return out


def run_e2(live, result, baseline):
    """Auxiliary 3 (runs the offset): Y0 = 2 for a running witness, +500 then -500."""
    address = OWNER
    facts = {str(address): {"configure": {}, "assign": {"y0": ["indicator", "either"]}}}
    live.select(address)
    live.fresh(address)
    out = {"snapshot": home_snapshot(live, address)}
    # E1's drive-reported witness in this controller session gates auxiliary 3
    # (the firmware keeps it in RAM): auxiliary 6 homes of +500 and -500 at order 0.
    out["e1_auxiliary"] = live.setting(address, "homing.auxiliary", 6, baseline, facts)
    witness = []
    for offset in (500, -500):
        row = live.home(address, offset, HOME_FACTS + ("offset",))
        row["state_after"] = state_flags(live, address)
        witness.append(row)
    out["e1_witness"] = witness
    out["auxiliary_set"] = live.setting(address, "homing.auxiliary", 3, baseline, facts)
    out["y0_set"] = live.setting(address, "io.y0", 2, baseline, facts)
    live.checked("bench", host_args=("clear",))
    homes = []
    for offset in (500, -500):
        # A HOMED-low start (wired bench 10 E3 exit: motor-release clears HOMED), so
        # the operation's own completion is observable.
        released = {"release": live.call("motor-release", address=address).get("outcome"),
                    "enable": live.call("enable", address=address).get("outcome")}
        time.sleep(1.1)  # The application's enable settle (HW-15) before a start.
        released["state"] = state_flags(live, address)
        row = live.home_bounded(address, offset, HOME_FACTS + ("offset", "offset-motion"))
        row["release_enable"] = released
        row["position_before"], row["position_after"] = row["start"]["position_words"], row["end"]["position_words"]
        row["aux1"] = live.checked("bench", host_args=())
        homes.append(row)
        if row.get("safety_stop") or not row["admitted"]:
            break
    out["homes"] = homes
    out["y0_restore"] = live.setting(address, "io.y0", 0, baseline, facts)
    after = home_restore(live, address, out["snapshot"])
    out["auxiliary_restore"] = live.setting(address, "homing.auxiliary", out["snapshot"]["auxiliary"], baseline, facts)
    ending = home_snapshot(live, address)
    out["ending"] = ending
    result.field(address, "homing.window", out["snapshot"]["words"], ending["words"], ending["words"] == out["snapshot"]["words"])
    result.field(address, "homing.auxiliary", out["snapshot"]["auxiliary"], ending["auxiliary"],
                 ending["auxiliary"] == out["snapshot"]["auxiliary"])
    y0 = live.checked("io", driver_args=("read",), address=address)["observation"]["raw"][6]
    result.field(address, "io.y0", 0, y0, y0 == 0)
    return out


def velocity_snapshot(live, address):
    reply = live.checked("velocity-read", address=address)
    v = reply["velocity_parameters"]
    return {"operation_id": reply["operation_id"],
            "words": [v["raw_speed"], v["raw_acceleration_time"], v["raw_deceleration_time"]]}


def state_flags(live, address):
    words, motion, state = live.position(address)
    blocks = records._blocks(state)
    m = blocks.get(0, {})
    return {"position_words": words, "position": signed(words) if words else None, "raw_motion": motion,
            "raw_alarm": m.get("raw_alarm"), "running": m.get("running"), "released": m.get("released"),
            "positive_soft_limit": bool(motion & 32) if motion is not None else None,
            "negative_soft_limit": bool(motion & 64) if motion is not None else None,
            "homed": bool(motion & 2) if motion is not None else None,
            "raw_speed": blocks.get(2, {}).get("raw_speed")}


def run_e3(live, result, baseline, direction):
    """One independent E3 trial (direction +1 or -1). Steps 1-4 must all pass
    before the velocity is started; otherwise the trial is NOT RUN."""
    address = OWNER
    facts = {str(address): {"configure": {}}}
    live.rebind(address)
    out = {"direction": direction, "steps": {}}
    live.no_pt_trigger()
    live.fresh(address)
    config = live.config_raw(address)
    flags = state_flags(live, address)
    out["steps"]["1_fresh"] = {"config": config, "state": flags}
    if config["soft_limit_enable"] != 0 or config["word_order"] != 0 or flags["positive_soft_limit"] or \
            flags["negative_soft_limit"] or flags["raw_alarm"]:
        out["not_run"] = "step 1: 0x0018/0x0019 not 0 or a soft-limit flag or alarm"
        return out
    source, before = live.driver(address)
    out["limit_source"], out["limit_before"] = source, before
    out["home_snapshot"] = home_snapshot(live, address)
    out["velocity_snapshot"] = velocity_snapshot(live, address)
    if out["home_snapshot"]["auxiliary"] != 7:
        out["auxiliary_set"] = live.setting(address, "homing.auxiliary", 7, baseline, facts)
    home = live.home(address, 0, HOME_FACTS)
    out["steps"]["2_home"] = home
    flags = state_flags(live, address)
    out["steps"]["2_state"] = flags
    acknowledged = home["terminal"].get("setup_execution") == "acknowledged" and \
        home["terminal"].get("execution") == "acknowledged"
    if not acknowledged or not flags["homed"] or flags["position_words"] != [0, 0]:
        out["not_run"] = "step 2: home 35 with auxiliary 7 not acknowledged or not read back at 0 with HOMED"
        return finish_e3(live, result, out, baseline, facts)
    writes = [live.pair(address, 0x37, E3_POSITIVE, "E3 positive limit +300"),
              live.pair(address, 0x39, E3_NEGATIVE, "E3 negative limit -300")]
    out["steps"]["3_limits"] = writes
    live.fresh(address)
    _, after = live.driver(address)
    out["steps"]["3_readback"] = after
    if not all(w.get("ok") for w in writes) or after["positive"] != E3_POSITIVE or after["negative"] != E3_NEGATIVE or \
            after["raw"][2] != 0:
        out["not_run"] = "step 3: limit words or 0x0019 not read back"
        return finish_e3(live, result, out, baseline, facts)
    live.fresh(address)
    live.driver(address)  # The admission compares a driver read newer than the config read.
    enable = live.experiment(("limit-enable",), "E3 0x0018 = 1")
    out["steps"]["4_enable"] = enable
    config = live.config_raw(address)
    out["steps"]["4_readback"] = config["soft_limit_enable"]
    if not enable.get("ok") or config["soft_limit_enable"] != 1:
        out["not_run"] = "step 4: 0x0018 = 1 not read back"
        return finish_e3(live, result, out, baseline, facts)
    # Step 5: velocity 30 rpm for 1000 ms in the trial's direction.
    live.checked("read-config", address=address)
    live.checked("velocity-read", address=address)
    live.checked("velocity-qualification-install", host_args=("rpm", "ramp", "inputs") + (("negative",) if direction < 0 else ()))
    live.checked("read-state", address=address)
    live.checked("io", driver_args=("read",), address=address)
    handle = live.console.begin("velocity", velocity_args=(str(30 * direction), "rpm", "native", "1000", "configured", "normal"),
                                address=address, timeout_s=max(live.timeout, 10))
    out["velocity_admitted"] = handle.accepted
    terminal = live.console.wait(handle, release=True)
    out["velocity"] = {k: terminal.get(k) for k in ("ok", "result", "state", "outcome", "status", "detail", "completion",
                                                    "stop_completion", "stop_due_us", "raw_motion", "uncertain")}
    samples = []
    for _ in range(4):
        samples.append(state_flags(live, address))
        time.sleep(.1)
    out["after_samples"] = samples
    last = samples[-1]
    if last["running"] or last["raw_speed"]:
        out["safety_stop"] = live.call("stop", stop_policy="fast", address=address).get("outcome")
        out["continued_motion"] = True
    return finish_e3(live, result, out, baseline, facts)


def soft_limit_exit(live, address):
    """Step 8: stop fast, alarm-clear, motor-release then enable (1 s settle),
    each followed by a state read; stops at the first state with no flag."""
    rows = [{"step": "start", "state": state_flags(live, address)}]
    flags = rows[0]["state"]
    if not (flags["positive_soft_limit"] or flags["negative_soft_limit"] or flags["released"] or flags["raw_alarm"]):
        return rows
    for name in ("stop", "alarm-clear", "motor-release", "enable"):
        kwargs = {"stop_policy": "fast"} if name == "stop" else {}
        reply = live.call(name, address=address, **kwargs)
        rows.append({"step": name, "ok": reply.get("ok"), "outcome": reply.get("outcome"), "result": reply.get("result")})
        if name == "enable":
            time.sleep(1.0)
        state = state_flags(live, address)
        rows.append({"step": name + " state", "state": state})
        if name != "motor-release" and not (state["positive_soft_limit"] or state["negative_soft_limit"] or
                                            state["released"] or state["raw_alarm"]):
            break
    return rows


def run_e3x(live, result, baseline, address=OWNER):
    """Exit and restoration of an interrupted E3 trial in the same binding (no
    rebind): step 8, then 0x0018 = 0, limits to the baseline words (named
    candidates), auxiliary to its baseline."""
    facts = {str(address): {"configure": {}}}
    out = {"exit": soft_limit_exit(live, address)}
    flags = state_flags(live, address)
    out["exit_state"] = flags
    config = live.config_raw(address)
    out["config_before"] = config
    if config["soft_limit_enable"] != 0:
        if flags["positive_soft_limit"] or flags["negative_soft_limit"]:
            out["blocked"] = "soft-limit flag still set after the step 8 actions: owner power cycle needed (G3)"
        else:
            out["soft_limit_restore"] = live.setting(address, "drive.soft-limit", 0, baseline, facts)
    config = live.config_raw(address)
    result.field(address, "drive.soft-limit", 0, config["soft_limit_enable"], config["soft_limit_enable"] == 0,
                 out.get("blocked"))
    if config["soft_limit_enable"] == 0:
        live.fresh(address)
        _, before = live.driver(address)
        out["limits_before"] = before
        for name in ("positive", "negative"):
            if before[name] != [0, 0]:
                out.setdefault("writes", []).append(live.pair(address, LIMIT_FIRST[name], [0, 0], "baseline %s limit" % name))
        live.fresh(address)
    else:
        live.checked("read-config", address=address)
    _, ending = live.driver(address)
    out["limits_ending"] = ending
    for name in ("positive", "negative"):
        result.field(address, "drive.%s-limit" % name, [0, 0], ending[name], ending[name] == [0, 0], out.get("blocked"))
    if not out.get("blocked"):
        live.checked("read-config", address=address)
        current = home_snapshot(live, address)
        if current["auxiliary"] != 0:
            out["auxiliary_restore"] = live.setting(address, "homing.auxiliary", 0, baseline, facts)
        current = home_snapshot(live, address)
        out["home_ending"] = current
        result.field(address, "homing.auxiliary", 0, current["auxiliary"], current["auxiliary"] == 0)
    return out


def finish_e3(live, result, out, baseline, facts):
    """Step 8 exit and restoration per the step 2 table. A flag that will not
    clear leaves 0x0018 and the limit words changed (owner power cycle)."""
    address = OWNER
    out["exit"] = soft_limit_exit(live, address)
    flags = state_flags(live, address)
    out["exit_state"] = flags
    config = live.config_raw(address)
    if config["soft_limit_enable"] != 0:
        if flags["positive_soft_limit"] or flags["negative_soft_limit"]:
            out["blocked"] = "soft-limit flag still set after the step 8 actions: owner power cycle needed (G3)"
        else:
            live.setting(address, "drive.soft-limit", 0, baseline, facts)
    config = live.config_raw(address)
    result.field(address, "drive.soft-limit", 0, config["soft_limit_enable"], config["soft_limit_enable"] == 0,
                 out.get("blocked"))
    if config["soft_limit_enable"] == 0 and "limit_source" in out:
        for name in ("positive", "negative"):
            live.pair_restore(address, LIMIT_FIRST[name], out["limit_source"], "restore %s limit" % name)
    if "limit_source" in out:
        live.checked("read-config", address=address)
        _, ending = live.driver(address)
        for name in ("positive", "negative"):
            result.field(address, "drive.%s-limit" % name, out["limit_before"][name], ending[name],
                         ending[name] == out["limit_before"][name], out.get("blocked"))
    if "home_snapshot" in out and not out.get("blocked"):
        after = home_restore(live, address, out["home_snapshot"])
        if after["auxiliary"] != out["home_snapshot"]["auxiliary"]:
            live.setting(address, "homing.auxiliary", out["home_snapshot"]["auxiliary"], baseline, facts)
        ending = home_snapshot(live, address)
        result.field(address, "homing.window", out["home_snapshot"]["words"], ending["words"],
                     ending["words"] == out["home_snapshot"]["words"])
        result.field(address, "homing.auxiliary", out["home_snapshot"]["auxiliary"], ending["auxiliary"],
                     ending["auxiliary"] == out["home_snapshot"]["auxiliary"])
    if "velocity_snapshot" in out and not out.get("blocked"):
        current = velocity_snapshot(live, address)
        if current["words"] != out["velocity_snapshot"]["words"]:
            live.checked("read-config", address=address)
            live.checked("read-state", address=address)
            reply = live.call("velocity-restore", host_args=(str(out["velocity_snapshot"]["operation_id"]),))
            deadline = time.monotonic() + 5
            while reply.get("pending") and time.monotonic() < deadline:
                time.sleep(.02)
                reply = live.call("velocity-restore", host_args=("inspect",))
            current = velocity_snapshot(live, address)
        result.field(address, "jog.window", out["velocity_snapshot"]["words"], current["words"],
                     current["words"] == out["velocity_snapshot"]["words"])
    return out


# --------------------------------------------------------------------------
# Verdicts

def judge(case_id, result, data):
    v = result.verdict
    if case_id in ("P1", "P4"):
        rows = data if case_id == "P1" else data
        ack = all(d.get("acknowledged") for d in rows.values())
        exact = all(d.get("exact") for d in rows.values())
        v("acknowledged", "PASS" if ack else "FAIL", None if ack else "an experiment write was not acknowledged")
        v("read back", "PASS" if exact else "FAIL", None if exact else "readback differs from the candidate words")
    elif case_id == "E2R":
        restored = all(f["restored"] for f in result.fields)
        v("read back", "PASS" if restored else "FAIL", None if restored else "restoration readback differs")
    elif case_id == "E3X":
        cleared = not data.get("blocked")
        v("drive-reported", "PASS", "recorded: flag %s after the step 8 actions" % ("cleared" if cleared else "still set"))
        restored = all(f["restored"] for f in result.fields)
        v("read back", "PASS" if restored else "BLOCKED" if not cleared else "FAIL",
          None if restored else data.get("blocked") or "restoration readback differs")
    elif case_id == "P3D":
        p0 = [r["position"] for r in data["order0"] if "position" in r]
        p1 = [r["position"] for r in data["order1"] if "position" in r]
        swapped = all(w == [p0[0][1], p0[0][0]] for w in p1)
        v("drive-reported", "PASS", "recorded: order 1 position words %s against order 0 %s (swapped: %s)" % (p1, p0, swapped))
    elif case_id == "R10":
        writes = data["limit_writes"]
        ack = all(r["written"] and not r["uncertain"] for r in writes) and all(
            h["terminal"].get("setup_execution") == "acknowledged" and h["terminal"].get("execution") == "acknowledged"
            for h in data["homes"])
        d = data["decoded"]
        readback = (d["positive_words"] == [0, 300] and d["negative_words"] == [65535, 65236] and d["limits_decoded"] is True and
                    d["positive_limit"] == 300 and d["negative_limit"] == -300 and
                    all(f["restored"] for f in result.fields))
        homes_ok = all(h["terminal"].get("ok") and h["state_after"]["position"] == h["terminal"].get("offset")
                       for h in data["homes"])
        refused = data["experiment_refusal"]["result"] == "unsupported" and \
            data["experiment_refusal"]["observed"][0] == data["experiment_refusal"]["observed"][1]
        v("acknowledged", "PASS" if ack else "FAIL", None if ack else "a production write or home was not acknowledged")
        v("read back", "PASS" if readback and refused else "FAIL",
          None if readback and refused else "decoded readback, restoration or the experiment refusal differs")
        v("drive-reported", "PASS" if homes_ok else "FAIL", None if homes_ok else "an offset home did not end at its offset")
    elif case_id == "RB":
        v("acknowledged", "PASS" if data["acknowledged"] else "FAIL", None if data["acknowledged"] else "a restoration write was not acknowledged")
        v("read back", "PASS" if data["exact"] else "FAIL", None if data["exact"] else "a pair differs from the baseline after restoration")
    elif case_id == "P2":
        ok = all(s["pass"] for s in data["per_segment"]) and len(data["per_segment"]) == 17
        ack = all(s["ack"] == "acknowledged" for s in data["per_segment"])
        v("acknowledged", "PASS" if ack else "FAIL", None if ack else "a segment write was not acknowledged")
        v("read back", "PASS" if ok else "FAIL", None if ok else "a segment readback differs")
    elif case_id == "P3":
        p0, p1 = data["position_order0"], data["position_order1"]
        swapped = p0 is not None and p1 is not None and [p0[1], p0[0]] == p1
        exact = data["limit_after"]["positive"] == P1_POSITIVE and data["segment_after"] == SEGMENT_WORDS[1] and \
            all(w.get("ok") for w in data["writes"])
        v("read back", "PASS" if exact else "FAIL", None if exact else "order 1 readback differs")
        v("drive-reported", "PASS" if swapped else "FAIL",
          None if swapped else "position words at order 1 %s are not the swapped order 0 words %s" % (p1, p0))
    elif case_id == "E1":
        homes = data["order0"]
        ack = all(h["terminal"].get("staging_words", [None] * 6)[4:] == ([0, 500] if h["terminal"].get("offset") == 500
                                                                        else MINUS_500) and
                  h["terminal"].get("setup_execution") == "acknowledged" for h in homes)
        reported = all(h["position_after"] == ([0, 500] if h["terminal"].get("offset") == 500 else MINUS_500)
                       for h in homes)
        v("acknowledged", "PASS" if ack else "FAIL", None if ack else "an order-0 offset home did not complete as observed")
        v("drive-reported", "PASS" if reported else "FAIL",
          None if reported else "order-0 coordinates differ from the offsets")
    elif case_id == "E2":
        homes = data["homes"]
        ack = len(homes) == 2 and all(h["terminal"].get("setup_execution") == "acknowledged" and
                                      h["terminal"].get("execution") == "acknowledged" for h in homes)
        v("acknowledged", "PASS" if ack else "FAIL", None if ack else "an auxiliary 3 home was refused or not acknowledged")
        ran = [any(s.get("running") for s in h["samples"]) for h in homes]
        ends = [h["end"]["position"] for h in homes]
        v("drive-reported", "PASS" if len(homes) == 2 and not any(h.get("safety_stop") for h in homes) else "FAIL",
          "recorded: RUNNING sampled %s, end coordinates %s, completions %s" %
          (ran, ends, [h["terminal"].get("outcome") for h in homes]))
        v("controller-timestamped", "PASS", "recorded: aux1 views per home (Y0 running witness)")
    elif case_id in ("E3P", "E3N"):
        if data.get("not_run"):
            for level in CASES[case_id]["levels"]:
                v(level, "NOT RUN", data["not_run"])
        else:
            v("read back", "PASS")
            flag = "positive_soft_limit" if case_id == "E3P" else "negative_soft_limit"
            stopped = any(s.get(flag) for s in data.get("after_samples", []))
            positions = [s.get("position") for s in data.get("after_samples", []) if s.get("position") is not None]
            near = bool(positions) and abs(positions[0] - (300 if case_id == "E3P" else -300)) <= 50
            v("drive-reported", "PASS" if stopped and near else "FAIL",
              None if stopped and near else "no stop near the limit with its flag (positions %s)" % positions)


def run_rb(live, result, baseline, address=OWNER):
    """Baseline restoration of the experiment pairs (named candidates [0,0] and [0,5000])."""
    live.rebind(address)
    live.no_pt_trigger()
    live.fresh(address)
    _, before = live.driver(address)
    out = {"before": before, "writes": [], "segments": {}}
    for name in ("positive", "negative"):
        field = records.baseline_field(baseline, address, "drive.%s-limit" % name)
        if before[name] != field["value"]:
            row = live.pair(address, LIMIT_FIRST[name], field["value"], "baseline %s limit" % name)
            out["writes"].append(row)
    live.fresh(address)
    _, after = live.driver(address)
    out["after"] = after
    for name in ("positive", "negative"):
        field = records.baseline_field(baseline, address, "drive.%s-limit" % name)
        result.field(address, "drive.%s-limit" % name, field["value"], after[name], after[name] == field["value"])
    for n in range(1, 17):
        live.fresh(address)
        _, words = live.segment(address, n)
        base = baseline["drives"][str(address)]["reads"]["segment.position.%d" % n]["raw"][3:5]
        out["segments"][n] = {"before": words, "baseline": base}
        if words != base:
            out["writes"].append(live.pair(address, 0x60 + 6 * (n - 1), base, "baseline segment %d" % n))
            live.fresh(address)
            out["segments"][n]["after"] = live.segment(address, n)[1]
    ack = all(w.get("final", {}).get("execution") == "acknowledged" for w in out["writes"])
    out["acknowledged"] = ack
    out["exact"] = all(f["restored"] for f in result.fields) and \
        all(v.get("after", v["before"]) == v["baseline"] for v in out["segments"].values())
    return out


def run_p3d(live, result, baseline, address=OWNER):
    """Does 0x0019 = 1 change the words the drive reports? Reads only, plus the
    order write and its restoration (03 CONFIGURE route)."""
    import bench_motion as motion
    facts = {str(address): {"configure": {}}}
    live.rebind(address)
    live.fresh(address)
    out = {"order0": [], "order1": [], "order0_after": []}
    def sample(key):
        for _ in range(3):
            out[key].append({"position": live.position(address)[0],
                             "config_order": live.config_raw(address)["word_order"]})
            time.sleep(.2)
        profile = motion.profile_snapshot(live.console, "read", timeout_s=live.timeout)
        live.call("motion-profile", host_args=("forget",))
        out[key].append({"profile": profile.get("current")})
    sample("order0")
    out["set"] = live.setting(address, "drive.word-order", 1, baseline, facts)
    sample("order1")
    out["restore"] = live.setting(address, "drive.word-order", 0, baseline, facts)
    sample("order0_after")
    order = live.config_raw(address)["word_order"]
    result.field(address, "drive.word-order", 0, order, order == 0)
    return out


def run_e2r(live, result, baseline, snapshot_id, address=OWNER):
    """No rebind: the binding's pinned home read is the D7 source."""
    facts = {str(address): {"configure": {}, "assign": {"y0": ["indicator", "either"]}}}
    live.select(address)
    live.fresh(address)
    before = home_snapshot(live, address)
    out = {"before": before, "snapshot_id": snapshot_id}
    target = {"operation_id": snapshot_id, "address": address}
    after = home_restore(live, address, target)
    out["after_restore"] = after
    if after["auxiliary"] != 0:
        out["auxiliary_restore"] = live.setting(address, "homing.auxiliary", 0, baseline, facts)
    io = live.checked("io", driver_args=("read",), address=address)["observation"]["raw"]
    if io[6] != 0:
        out["y0_restore"] = live.setting(address, "io.y0", 0, baseline, facts)
    ending = home_snapshot(live, address)
    out["ending"] = ending
    words = [35, 60, 30, 100, 0, 0]
    result.field(address, "homing.window", words, ending["words"], ending["words"] == words)
    result.field(address, "homing.auxiliary", 0, ending["auxiliary"], ending["auxiliary"] == 0)
    y0 = live.checked("io", driver_args=("read",), address=address)["observation"]["raw"][6]
    result.field(address, "io.y0", 0, y0, y0 == 0)
    return out


def run_r10(live, result, baseline, address=OWNER):
    """Production regression of the promoted forms on the production image."""
    facts = {str(address): {"configure": {}}}
    live.rebind(address)
    live.no_pt_trigger()
    live.fresh(address)
    out = {}
    _, before = live.driver(address)
    out["limits_before"] = before
    # The experiment route is gone from the production image: refused before TX.
    debug_before = live.checked("debug", host_args=()).get("observed")
    refusal = live.call("driver-experiment", host_args=("pair", "96", "1", "4097"))
    debug_after = live.checked("debug", host_args=()).get("observed")
    out["experiment_refusal"] = {"ok": refusal.get("ok"), "result": refusal.get("result"),
                                 "observed": [debug_before, debug_after]}
    rows = []
    for name, words in (("positive", [0x0000, 0x012C]), ("negative", [0xFFFF, 0xFED4])):
        rows.append(live.setting(address, "drive.%s-limit" % name, words, baseline, facts))
    live.fresh(address)
    reply = live.checked("driver", driver_args=("read",), address=address)
    out["decoded"] = {k: reply["observation"].get(k) for k in ("positive_words", "negative_words", "limits_decoded",
                                                               "positive_limit", "negative_limit", "signed_limits")}
    out["limit_writes"] = rows
    for name in ("positive", "negative"):
        live.setting(address, "drive.%s-limit" % name, before[name], baseline, facts)
    live.fresh(address)
    _, ending = live.driver(address)
    out["limits_ending"] = ending
    for name in ("positive", "negative"):
        result.field(address, "drive.%s-limit" % name, before[name], ending[name], ending[name] == before[name])
    # Production nonzero offset: auxiliary 6, method 35, HOMED-low start for an observable completion.
    snapshot = home_snapshot(live, address)
    out["home_snapshot"] = snapshot
    out["auxiliary_set"] = live.setting(address, "homing.auxiliary", 6, baseline, facts)
    homes = []
    for offset in (500, -500):
        released = {"release": live.call("motor-release", address=address).get("outcome"),
                    "enable": live.call("enable", address=address).get("outcome")}
        time.sleep(1.1)
        released["state"] = state_flags(live, address)
        row = live.home(address, offset, HOME_FACTS + ("offset",))
        row["release_enable"] = released
        row["state_after"] = state_flags(live, address)
        homes.append(row)
    out["homes"] = homes
    after = home_restore(live, address, snapshot)
    out["home_restore"] = after
    if after["auxiliary"] != snapshot["auxiliary"]:
        live.setting(address, "homing.auxiliary", snapshot["auxiliary"], baseline, facts)
    ending_home = home_snapshot(live, address)
    out["home_ending"] = ending_home
    result.field(address, "homing.window", snapshot["words"], ending_home["words"], ending_home["words"] == snapshot["words"])
    result.field(address, "homing.auxiliary", snapshot["auxiliary"], ending_home["auxiliary"],
                 ending_home["auxiliary"] == snapshot["auxiliary"])
    return out


def run_case(console, record, args, case_id, folder, baseline):
    live = Live(console, folder, args.timeout)
    result = Result(case_id)
    view = live.checked("bench", host_args=())
    if any(o["active"] for o in view["outputs"]):
        raise probe.BenchError("a bench output is already on")
    original = live.checked("wiring")["address"]
    before = {}
    for address in CASES[case_id]["drives"]:
        before[address] = records.read_drive(console, address, ("identity", "config", "state", "io", "driver"),
                                             select=True, timeout_s=args.timeout)
    folder.write_json("baseline_before.json", {"bench": records._bench_view(view),
                                               "drives": {str(a): {k: {"operation_id": r.get("operation_id"), "raw": r.get("raw")}
                                                                   for k, r in reads.items()} for a, reads in before.items()}})
    data, error = None, None
    try:
        if case_id == "P1":
            data = run_p1(live, result)
        elif case_id == "P2":
            data = run_p2(live, result)
        elif case_id == "P3":
            data = run_p3(live, result, baseline)
        elif case_id == "P4":
            data = run_p4(live, result)
        elif case_id == "E1":
            data = run_e1(live, result, baseline)
        elif case_id == "E2":
            data = run_e2(live, result, baseline)
        elif case_id in ("E3P", "E3N"):
            data = run_e3(live, result, baseline, 1 if case_id == "E3P" else -1)
        elif case_id == "RB":
            data = run_rb(live, result, baseline)
        elif case_id == "R10":
            data = run_r10(live, result, baseline)
        elif case_id == "P3D":
            data = run_p3d(live, result, baseline)
        elif case_id == "E3X":
            data = run_e3x(live, result, baseline)
        elif case_id == "E2R":
            data = run_e2r(live, result, baseline, args.home_snapshot)
        judge(case_id, result, data)
    except probe.BenchError as failure:
        error = str(failure)
        result.deviations.append("case stopped: " + error)
        for level in CASES[case_id]["levels"]:
            result.verdicts.setdefault(level, "FAIL")
        result.reasons.append(error)
    folder.write_json("pairs.json", {"case": case_id, "data": data, "rows": live.rows, "events": live.events[-400:],
                                     "error": error})
    off = records.outputs_off(console, timeout_s=args.timeout)
    result.outputs_off = off["outputs_off"]
    ending = {"bench": off["view"], "drives": {}}
    for address in CASES[case_id]["drives"]:
        state = console.command("read-state", address=address, timeout_s=args.timeout)
        blocks = records._blocks(state)
        m, f = blocks.get(0, {}), blocks.get(2, {})
        s = {"ok": state.get("ok") is True, "running": m.get("running"), "enabled": m.get("enabled"),
             "raw_alarm": m.get("raw_alarm"), "alarm_flag": m.get("alarm_flag"), "raw_speed": f.get("raw_speed"),
             "raw_motion": m.get("raw_motion"), "position_words": f.get("position_words")}
        ending["drives"][str(address)] = {"state": s}
        result.drives.append({"address": address, "stopped": s["ok"] and s["running"] is False and s["raw_speed"] == 0,
                              "alarm": bool(s["raw_alarm"]) or s["alarm_flag"] is not False, "enabled": s["enabled"],
                              "required_enabled": True})
    records.use_drive(console, original, timeout_s=args.timeout)
    folder.write_json("ending_reads.json", ending)
    record["workload_verified"] = error is None
    _LAST["result"] = result
    if error is not None:
        raise probe.BenchError(error)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM13")
    parser.add_argument("--case", choices=tuple(CASES), required=True)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True, help="flashed image (SHA-256 recorded)")
    parser.add_argument("--commit", required=True, help="source commit string of the image")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--home-snapshot", type=int, help="E2R: operation ID of the binding's pinned home read")
    args = parser.parse_args(argv)
    if args.port != records.FIXTURE["board"]["port"]:
        parser.error("the wired bench is on %s" % records.FIXTURE["board"]["port"])
    folder = records.RunFolder.from_path(args.run_dir)
    if folder.prompt != "10":
        parser.error("paired-values run folders are wired bench 10 (YYYY-MM-DD_wb10_slug)")
    image = {"path": str(args.firmware.as_posix()), "sha256": records.sha256_file(args.firmware)}
    baseline = records.load_baseline()
    folder.create()
    scenario = records.write_scenario(folder, **scenario_record(args, args.case, image, args.commit))
    failure = None
    try:
        run_recorded(port=args.port, scenario="paired-values", inputs={"case": args.case},
                               run=lambda console, rec: run_case(console, rec, args, args.case, folder, baseline),
                               address=CASES[args.case]["drives"][0], timeout_s=args.timeout, firmware=args.firmware,
                               refresh=False, paths={"summary": folder / "summary.json", "jsonl": folder / "session.jsonl",
                                                     "console": folder / "console.log"},
                               max_bytes=64 * 1024 * 1024)
    except Exception as error:  # The summary and evidence stay; the result records the failure.
        failure = str(error)
    result = _LAST.get("result")
    if result is None:
        result = Result(args.case)
        for level in CASES[args.case]["levels"]:
            result.verdict(level, "FAIL", failure or "no result")
    evidence = ["session.jsonl", "console.log"] + (["pairs.json"] if (folder / "pairs.json").is_file() else [])
    case = dict(scenario["cases"][0], verdicts=result.verdicts, reason="; ".join(result.reasons) or None,
                evidence=evidence)
    covered = {(f["drive"], f["key"]) for f in result.fields}
    for entry in scenario["write_list"]:
        if (entry["drive"], entry["key"]) not in covered:
            # Planned but never reached (or equal and not written): nothing changed it in this run.
            result.fields.append({"drive": entry["drive"], "key": entry["key"], "index": None, "baseline": None,
                                  "ending": None, "restored": failure is None, "uncertain": False, "storage": "RAM",
                                  "reason": "not written in this run" if failure is None else
                                            "no restoration entry after the case stopped: " + failure})
    restoration = {"outputs_off": result.outputs_off, "fields": result.fields, "drives": result.drives,
                   "port_closed": True}
    records.write_result(folder, commit=args.commit, image=image, cases=[case], restoration=restoration,
                         deviations=result.deviations + ([failure] if failure else []))
    report = records.check_run(folder.path)
    print(json.dumps(report, indent=1))
    return records.check_exit(report)


_LAST = {}

if __name__ == "__main__":
    sys.exit(main())
