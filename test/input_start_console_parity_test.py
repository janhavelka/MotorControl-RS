"""Actual C++ input-start console replies (wired bench 08) checked by the strict
Python validator, and mutated replies refused: one vocabulary for console,
firmware and harness."""
import copy
import json
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Command, Console, MAX_LINE, input_start_arguments


def handle(command, line):
    tokens = tuple(line.split()[2:]) if command != "disarm" else tuple(line.split()[1:])
    if command == "arm-home":
        tokens = tuple(line.split()[2:])
    h = Command(None, 1, command, 0.0, 1.0, None, None, None)
    h.host_args = tokens
    return h


rows = subprocess.check_output([sys.argv[1], "--input-start-fixtures"], text=True, timeout=10).splitlines()
assert len(rows) == 9, len(rows)
seen = set()
for row in rows:
    fixture = json.loads(row)
    record, command, line = fixture["record"], fixture["command"], fixture["line"]
    assert len(row) < MAX_LINE and record["command"] == command, (fixture["case"], record["command"])
    h = handle(command, line)
    Console._check_input_start(h, record)
    seen.add(fixture["case"])
    mutations = [
        lambda r: r.update(result="accepted" if not r["ok"] else "invalid"),
        lambda r: r.update(bus_traffic=not r["bus_traffic"]),
        lambda r: r["session"].update(phase="sleeping"),
        lambda r: r["session"]["source"].update(evidence="electrical_edge"),
    ]
    session = record["session"]
    if session["source"]["triggered"]:
        mutations += [lambda r: r["session"]["source"].update(output="relay1"),
                      lambda r: r["session"]["source"].update(width_ms=5001),
                      lambda r: r["session"]["isolation"].update(admitted=False),
                      lambda r: r["session"]["isolation"]["rows"][0].update(row="L5"),
                      lambda r: r["session"]["isolation"]["rows"][0].update(io_age_us=1000001)]
    for index, member in enumerate(session["members"]):
        result = member["result"]
        if not result["known"]:
            continue
        if result["outcome"] == "attributed":
            mutations += [lambda r, i=index: r["session"]["members"][i]["result"].update(missing=["spacing"]),
                          lambda r, i=index: r["session"]["members"][i]["result"].update(change_end_us=0),
                          lambda r, i=index: r["session"]["members"][i]["result"].update(execution="not_observed"),
                          lambda r, i=index: r["session"]["members"][i]["result"].update(position_change=999)]
        if result["outcome"] == "indeterminate":
            mutations += [lambda r, i=index: r["session"]["members"][i]["result"].update(missing=[]),
                          lambda r, i=index: r["session"]["members"][i]["result"].update(state="succeeded")]
        if result["outcome"] == "stopped":
            mutations += [lambda r, i=index: r["session"]["members"][i]["result"].update(stopped_after_stop=False)]
        mutations += [lambda r, i=index: r["session"]["members"][i]["result"].update(effect="velocity_run",
                                                                                      completion="finite_stopped")]
    for mutate in mutations:
        broken = copy.deepcopy(record)
        mutate(broken)
        try:
            Console._check_input_start(h, broken)
        except BenchError:
            pass
        else:
            raise AssertionError((fixture["case"], "invalid input-start evidence accepted"))
    if record["ok"] and "operation_id" in input_start_arguments(command, h.host_args):
        wrong = handle(command, line.replace(" 12", " 13"))
        try:
            Console._check_input_start(wrong, record)
        except BenchError:
            pass
        else:
            raise AssertionError("session correlation bypass")
assert seen == {"arm_accepted", "armed", "trigger_accepted", "attributed", "indeterminate", "stopped",
                "trigger_refused", "home_unsupported", "disarmed"}, seen
for command, tokens in (("arm-position", ("x4", "relative", "1", "60")), ("arm-position", ("x3", "relative", "1", "60", "group", "group")),
                        ("arm-position", ("x3", "absolute", "1", "60", "group")), ("arm-position", ("x3", "relative", "1", "60", "poll-ms", "19")),
                        ("arm-trigger", ("12", "mosfet2")), ("arm-trigger", ("12", "mosfet9", "20")), ("arm-inspect", ()), ("disarm", ("0",))):
    try:
        input_start_arguments(command, tokens)
    except ValueError:
        pass
    else:
        raise AssertionError(("invalid grammar accepted", command, tokens))
print("PASS: 9 actual input-start console replies and their mutations")
