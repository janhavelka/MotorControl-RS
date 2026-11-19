"""Real tuning core/console evidence checked by the strict Python transport."""
import copy
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console, MAX_LINE, TUNING_FIELDS, TUNING_GROUPS, tuning_arguments, wire_crc
from bench_probe_test import check_fixture_routes

rows = subprocess.check_output([sys.argv[1], "--tuning-fixtures"], text=True, timeout=10).splitlines()
assert len(rows) == 66
updates = {"filters": (3, 6, 4001, 6, 11, 511), "current_loop": (4097, 1025, 29, 1229),
           "la": (11, 33, 321, 16, 34, 321, 21, 36), "collision": (201, 51)}
slugs = {value: key for key, value in TUNING_GROUPS.items()}
seen = set()
for line in rows:
    fixture = json.loads(line)
    record = fixture["record"]
    group = record["driver_group"]
    seen.add(group)
    arguments = (slugs[group], "read") if record["driver_kind"] == "read" else (
        slugs[group], "set", *(token for field, value in zip(TUNING_FIELDS[group], updates[group])
                              for token in (field, str(value))))
    Console._check_driver(record, 1, arguments, "tuning")
    check_fixture_routes(record)
    assert record["command"] == record["type"] == "tuning" and len(line) < MAX_LINE
    if fixture["case"] in ("previous_settings_deadline", "previous_settings_late_echo"):
        assert record["stationary_valid_until_us"] == 250
        assert record["outcome"] == "deadline" and not record["ok"]
        first = dict(zip(record["progress_columns"], record["progress"][0]))
        assert not first["acknowledged"] and not first["readback_known"]
        if fixture["case"] == "previous_settings_late_echo":
            assert first["execution"] == "unknown" and record["uncertain"]
        else:
            assert first["execution"] == "not_transmitted" and not record["uncertain"]
        forged = copy.deepcopy(record)
        forged["stationary_valid_until_us"] = 280
        try:
            Console._check_driver(forged, 1, arguments, "tuning")
        except BenchError:
            pass
        else:
            raise AssertionError("previous-settings deadline replaced by identity deadline")
    if record["ok"] and record["driver_kind"] == "update":
        assert len(record["progress"]) == len(TUNING_FIELDS[group])
        assert record["completed_steps"] == 2 * len(TUNING_FIELDS[group])
    if record["ok"] and record["driver_kind"] == "read":
        # Every group must verify failure bytes as strictly as successful reads.
        columns = record["evidence_columns"]
        failed = copy.deepcopy(record)
        failed.update(ok=False, state="failed", outcome="reply_error", status="CRC_ERROR", detail=6,
                      completed_steps=len(failed["evidence"]) - 1, observation=None)
        last = failed["evidence"][-1]
        last[columns.index("status")] = "CRC_ERROR"
        last[columns.index("detail")] = last[columns.index("frame_error")] = 6
        valid_raw = bytes.fromhex(last[columns.index("raw_hex")])
        assert wire_crc(valid_raw) == 0
        try:
            Console._check_driver(failed, 1, arguments, "tuning")
        except BenchError:
            pass
        else:
            raise AssertionError("CRC-valid tuning reply relabeled as corrupt")
        corrupted = valid_raw[:-1] + bytes((valid_raw[-1] ^ 1,))
        last[columns.index("raw_hex")] = corrupted.hex()
        Console._check_driver(failed, 1, arguments, "tuning")
        for timing in ("unqualified", "late"):
            timed = copy.deepcopy(failed)
            row = timed["evidence"][-1]
            if timing == "unqualified":
                row[columns.index("qualified")] = False
                row[columns.index("earliest_us")] = row[columns.index("latest_us")] = 0
                outcome, detail = "timing_unqualified", 16
            else:
                row[columns.index("earliest_us")] = timed["deadline_us"] + 1
                row[columns.index("latest_us")] = timed["deadline_us"] + 2
                row[columns.index("delivered_us")] = timed["serviced_us"] = timed["deadline_us"] + 3
                outcome, detail = "deadline", 13
            try:
                Console._check_driver(timed, 1, arguments, "tuning")
            except BenchError:
                pass
            else:
                raise AssertionError("parser failure outranks tuning timing evidence")
            timed.update(outcome=outcome, status="ILLEGAL_VALUE", detail=detail)
            Console._check_driver(timed, 1, arguments, "tuning")
    mutations = [lambda r: r.update(driver_group="drive"), lambda r: r.update(active_settings_known=True),
                 lambda r: r.update(effects=1 << 31), lambda r: r["evidence"][0].__setitem__(1, 0x003B),
                 lambda r: r["evidence"][0].__setitem__(5, "0000")]
    if record["driver_kind"] == "read":
        mutations.extend((lambda r: r["observation"].update(physical_scaling_known=True),
                          lambda r: r["observation"].update(arrival_time_ms=10),
                          lambda r: r["observation"].update(node_rpm=320),
                          lambda r: r["observation"].update(raw=[0]),
                          lambda r: r["evidence"][0].__setitem__(r["evidence_columns"].index("execution_unknown"), True)))
    else:
        mutations.append(lambda r: r["progress"][0].__setitem__(3, 65535))
        if record["echo_readback_policy"] and record["ok"]:
            mutations.append(lambda r: r.update(echo_readback_policy=False))
    for mutate in mutations:
        broken = copy.deepcopy(record)
        mutate(broken)
        try:
            Console._check_driver(broken, 1, arguments, "tuning")
        except BenchError:
            pass
        else:
            raise AssertionError((fixture["case"], "corrupt tuning evidence accepted"))
    wrong_group = next(slug for slug, canonical in TUNING_GROUPS.items() if canonical != group)
    try:
        Console._check_driver(record, 1, (wrong_group, "read"), "tuning")
    except BenchError:
        pass
    else:
        raise AssertionError("tuning group correlation bypass")
assert seen == set(TUNING_FIELDS)

for arguments in (("filters", "set", "input-filter", "1/1"), ("filters", "set", "input-filter", "1.0"),
                  ("filters", "set", "input-filter", "-1"), ("la", "set", "node1", "65536"),
                  ("la", "set", "node1", "1", "node1", "2"), ("collision", "set", "0x003B", "200"),
                  ("bogus", "read"), ("filters", "set", "kp", "1")):
    try:
        tuning_arguments(arguments)
    except ValueError:
        pass
    else:
        raise AssertionError("invalid named integer grammar accepted")

# Actual accepted/terminal/inspection/release correlation for each native group.
from bench_probe_test import Clock, Serial, encoded, reply
import bench_probe
for group in TUNING_FIELDS:
    fixture = next(json.loads(row)["record"] for row in rows
                   if json.loads(row)["case"] == "read" and json.loads(row)["record"]["driver_group"] == group)
    retained = {}
    def handler(command_id, command, arguments):
        if command == "tuning":
            assert arguments == [slugs[group], "read", "1"]
            terminal = {**fixture, "id": command_id, "command_id": command_id, "operation_id": 101}
            retained.update(terminal)
            return encoded(reply(command_id, "tuning", result="accepted", address=1, operation_id=101)) + encoded(terminal)
        if command == "result":
            return encoded({**retained, "type": "reply", "id": command_id, "command": "result"})
        return Serial.normal(command_id, command, arguments)
    clock = Clock()
    port = Serial(handler)
    events = []
    console = bench_probe.Console(port, clock=clock, sleeper=clock.sleep,
                                  on_event=lambda event, **data: events.append((event, data)))
    console.identify(timeout_s=0.1)
    bench_probe.driver_read_campaign(console, timeout_s=0.1, address=1, command="tuning", driver_args=(slugs[group], "read"))
    assert events[-1][0] == "summary" and events[-1][1]["mode"] == "tuning-read"
    assert [line.decode().split()[1] for line in port.writes] == ["version", "tuning", "result", "release"]
    parsed = bench_probe.arguments(["--port", "fake", "--log", "unused.jsonl", "tuning", slugs[group], "read"])
    assert parsed.mode == "tuning" and parsed.driver_args == (slugs[group], "read")
# Wired bench 15 (D7): an exact restore names its source read and may carry a
# word outside the documented range (the bench's collision 0); without the
# source the same word is refused, and a read never carries the fields.
from bench_probe import tuning_candidates_arguments
collision = next(json.loads(row)["record"] for row in rows
                 if json.loads(row)["record"]["driver_group"] == "collision" and
                 json.loads(row)["record"]["driver_kind"] == "update" and json.loads(row)["record"]["ok"])
def framed(body: bytes) -> str:
    crc = 0xFFFF
    for byte in body:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return (body + bytes((crc & 0xFF, crc >> 8))).hex().upper()
single = copy.deepcopy(collision)
single["fields"], single["effects"] = 1, collision["effects"] & 1
single["progress"] = [row for row in collision["progress"] if row[0] == 1]
progress_columns = single["progress_columns"]
single["progress"][0][progress_columns.index("requested")] = 0
single["progress"][0][progress_columns.index("readback")] = 0
single["completed_steps"] = 2
single["evidence"] = collision["evidence"][:2]
raw_column = single["evidence_columns"].index("raw_hex")
single["evidence"][0][raw_column] = framed(bytes((1, 6, 0x01, 0x22, 0, 0)))
single["evidence"][1][raw_column] = framed(bytes((1, 3, 2, 0, 0)))
restore = dict(single, exact_restore_source=7, listed_current=False)
for record, admitted in ((restore, True), (dict(single, exact_restore_source=0), False)):
    try:
        Console._check_driver(record, 1, ("collision", "set", "threshold", "0"), "tuning")
    except BenchError:
        assert not admitted
    else:
        assert admitted, "an out-of-range word accepted without its restore source"
read = next(json.loads(row)["record"] for row in rows if json.loads(row)["record"]["driver_kind"] == "read")
try:
    Console._check_driver(dict(read, exact_restore_source=7), 1, None, "tuning")
except BenchError:
    pass
else:
    raise AssertionError("restore source on a read accepted")
assert tuning_candidates_arguments(("check", "control", "closed-maximum-current", "50", "4")) == {
    "request": "check", "group": "control_settings", "field": "closed-maximum-current", "value": 50, "address": 4}
assert tuning_candidates_arguments(("threshold", "8000")) == {"request": "threshold", "value": 8000}
for tokens in (("threshold", "4000"), ("threshold", "65536"), ("check", "filters", "kp", "1"), ("check", "bogus", "kp", "1"),
               ("check", "la", "kp1", "1", "248"), ("raise",)):
    try:
        tuning_candidates_arguments(tokens)
    except ValueError:
        pass
    else:
        raise AssertionError(("invalid candidates grammar accepted", tokens))
print("PASS: 66 real tuning terminals, snapshot deadlines, all20 fields, fullLA16steps, retained transport and the D7 restore record")
