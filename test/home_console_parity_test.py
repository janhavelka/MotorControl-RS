"""Validate actual core/console homing terminals and reject forged provenance."""
import copy
import json
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console, MAX_LINE, unique_object, wire_crc
from bench_probe_test import check_fixture_routes

expected = {"current_origin", "index_origin", "stale_homed", "lost_trigger_ack", "cancelled", "deadline", "unconfirmed_trigger", "exception", "late_trigger_echo",
            "timing_unqualified", "drive_fault", "unexpected_activity", "nonzero_position", "delayed_completion_deadline", "wrong_function_exception"}
seen = set()
for line in subprocess.check_output([sys.argv[1], "--home-fixtures"], text=True, timeout=10).splitlines():
    fixture = json.loads(line, object_pairs_hook=unique_object)
    name, record = fixture["case"], fixture["record"]
    assert name in expected and name not in seen
    assert len(json.dumps(record, separators=(",", ":"))) < MAX_LINE
    Console._check_home(record, 1, None)
    check_fixture_routes(record)
    if name == "exception":
        assert record["status"] == "EXCEPTION" and record["execution"] == "rejected"
    if name == "wrong_function_exception":
        assert record["status"] == "FRAME_ERROR" and record["execution"] == "unknown"
    if record["ok"]:
        # A final zero reply closed on time can be delivered after the retained
        # request deadline. Its delivery never changes the physical outcome.
        delayed = copy.deepcopy(record)
        delayed["serviced_us"] = delayed["zero_evidence"][11] = record["deadline_us"] + 20
        Console._check_home(delayed, 1, None)
    for mutate in (lambda r: r.update(address=2), lambda r: r.update(configuration_generation=0),
                   lambda r: r["prerequisites"]["qualified_parameters"].__setitem__(0, 34),
                   lambda r: r["prerequisites"]["qualified_parameters"].__setitem__(1, 61),
                   lambda r: r["prerequisites"]["qualified_parameters"].__setitem__(2, 31),
                   lambda r: r["prerequisites"]["qualified_parameters"].__setitem__(3, 101),
                   lambda r: r["staging_words"].__setitem__(4, 1), lambda r: r["staging_evidence"].__setitem__(4, 8)):
        broken = copy.deepcopy(record); mutate(broken)
        try: Console._check_home(broken, 1, None)
        except BenchError: pass
        else: raise AssertionError((name, "forged homing accepted"))
    if record["ok"]:
        for mutate in (lambda r: r.update(homed_low_observed=False), lambda r: r["zero_evidence"].__setitem__(6, False),
                       lambda r: r["staging_evidence"].__setitem__(11, r["trigger_evidence"][11] + 1),
                       lambda r: r.update(ok=False, state="failed", completion="not_observed", outcome="deadline", uncertain=True)):
            broken = copy.deepcopy(record); mutate(broken)
            try: Console._check_home(broken, 1, None)
            except BenchError: pass
            else: raise AssertionError((name, "false transition accepted"))
        if record["method"] != 35:
            for field in ("activity_evidence", "low_evidence"):
                for index, value in ((3, 1), (6, 0x78)):
                    broken = copy.deepcopy(record)
                    raw = bytearray.fromhex(broken[field][2]); raw[index] = value
                    raw[-2:] = wire_crc(raw[:-2]).to_bytes(2, "little")
                    broken[field][2] = raw.hex()
                    try: Console._check_home(broken, 1, None)
                    except BenchError: pass
                    else: raise AssertionError((name, "faulted transition accepted", field))
            for field in ("activity_evidence", "low_evidence", "completion_evidence", "zero_evidence"):
                broken = copy.deepcopy(record); broken[field][0] = 1
                try: Console._check_home(broken, 1, None)
                except BenchError: pass
                else: raise AssertionError((name, "uncorrelated transition accepted", field))
    seen.add(name)
assert seen == expected
print(f"PASS: {len(seen)} actual core/console homing terminals accepted by Python")

expected_reads = {"success", "unknown", "implemented", "unresolved", "exception", "cancelled", "deadline"}
seen_reads = set()
for line in subprocess.check_output([sys.argv[1], "--home-read-fixtures"], text=True, timeout=10).splitlines():
    fixture = json.loads(line, object_pairs_hook=unique_object)
    name, record = fixture["case"], fixture["record"]
    assert name in expected_reads and name not in seen_reads, name
    assert len(json.dumps(record, separators=(",", ":"))) < MAX_LINE
    Console._check_typed_read(record, "homing_parameters", 1)
    check_fixture_routes(record, live_command="home-read")
    if record["ok"]:
        value = record["homing_parameters"]
        assert value["offset_word_order_resolution"] == 9
        assert value["offset_signed_resolution"] == 7
        assert value["offset_scale_resolution"] == 5
        assert value["method_support"] == {"success": "unimplemented", "unknown": "unknown",
                                            "implemented": "implemented", "unresolved": "unresolved"}[name]
        for mutate in (lambda r: r["homing_parameters"].update(raw_offset_words=[1, 2]),
                       lambda r: r["homing_parameters"].update(offset_word_order_resolution=0),
                       lambda r: r["homing_parameters"].update(offset_signed_resolution=0),
                       lambda r: r["homing_parameters"].update(offset_scale_resolution=0),
                       lambda r: r["homing_parameters"].update(method_support="unsupported"),
                       lambda r: r.update(configuration_generation=0)):
            broken = copy.deepcopy(record); mutate(broken)
            try: Console._check_typed_read(broken, "homing_parameters", 1)
            except BenchError: pass
            else: raise AssertionError((name, "forged homing read accepted"))
    seen_reads.add(name)
assert seen_reads == expected_reads, (seen_reads, expected_reads)
print(f"PASS: {len(seen_reads)} actual core/console homing-parameter read terminals accepted by Python")

assert len(sys.argv) == 4, "both real application frontends are required"
frontend_records = []
for executable in sys.argv[2:]:
    records = [json.loads(line, object_pairs_hook=unique_object) for line in
               subprocess.check_output([executable, "--emit-console-results"], text=True, timeout=20).splitlines() if line.startswith("{")]
    records = [r for r in records if r.get("home") is True and r.get("command") == "result"]
    assert len(records) == 5 and [r["method"] for r in records] == [33, 34, 35, 35, 35]
    for record in records:
        Console._check_home(record, 1, None)
        assert record["qualification"]["observe_inputs"] and record["inputs_observed"]
        assert len(json.dumps(record, separators=(",", ":"))) < MAX_LINE
        for mutate in (lambda r: r["qualification"].update(qualified_us=r["started_us"] + 1),
                       lambda r: r["qualification"].update(readback_observed_us=r["started_us"] + 1),
                       lambda r: r["qualification"].update(observe_inputs=False),
                       lambda r: r["prerequisites"].update(raw_motion=r["prerequisites"]["raw_motion"] | 0x8000),
                       lambda r: r.update(qualification_revoked=not r["qualification_revoked"])):
            broken = copy.deepcopy(record); mutate(broken)
            try: Console._check_home(broken, 1, None)
            except BenchError: pass
            else: raise AssertionError("forged application qualification accepted")
        if record["ok"]:
            fields = ("completion_evidence",) if record["method"] == 35 else ("activity_evidence", "low_evidence", "completion_evidence")
            for field in fields:
                for contradiction in ("unknown_motion", "assigned_input", "unknown_input"):
                    broken = copy.deepcopy(record)
                    raw = bytearray.fromhex(broken[field][2])
                    if contradiction == "unknown_motion": raw[5] |= 0x80
                    else:
                        raw[8] = 1 if contradiction == "assigned_input" else 16
                        if contradiction == "assigned_input": broken["qualification"]["input_functions"][0] = 1
                    raw[-2:] = wire_crc(raw[:-2]).to_bytes(2, "little")
                    broken[field][2] = raw.hex()
                    if field == "completion_evidence":
                        broken["last_observation"] = copy.deepcopy(broken[field])
                        broken["raw_motion"] = int.from_bytes(raw[5:7], "big")
                        broken["raw_inputs"] = int.from_bytes(raw[7:9], "big")
                    try: Console._check_home(broken, 1, None)
                    except BenchError: pass
                    else: raise AssertionError((field, contradiction, "contradictory transition accepted"))
    assert [r["ok"] for r in records] == [True, True, True, False, False]
    assert records[3]["qualification_revoked"] and records[3]["zero_evidence"] == records[3]["failure_evidence"]
    assert records[4]["outcome"] == "reply_error" and records[4]["detail"] == 15 and records[4]["raw_motion"] == 0x8003
    # Checked unknown status precedes input/activity failure classification.
    for detail, motion, inputs in ((13, 0x8003, 16), (31, 0x8004, 0)):
        broken = copy.deepcopy(records[4]); broken["detail"] = detail
        raw = bytearray.fromhex(broken["failure_evidence"][2])
        raw[5:7] = motion.to_bytes(2, "big"); raw[7:9] = inputs.to_bytes(2, "big")
        raw[-2:] = wire_crc(raw[:-2]).to_bytes(2, "little")
        broken["failure_evidence"][2] = raw.hex()
        broken["last_observation"] = copy.deepcopy(broken["failure_evidence"])
        broken.update(raw_motion=motion, raw_inputs=inputs)
        try: Console._check_home(broken, 1, None)
        except BenchError: pass
        else: raise AssertionError("unknown status relabelled as a lower-priority failure")
    for outcome, detail in (("observation_limit", 26), ("deadline", 20)):
        broken = copy.deepcopy(records[4]); broken.update(outcome=outcome, detail=detail)
        if outcome == "deadline": broken["deadline_us"] = broken["serviced_us"]
        try: Console._check_home(broken, 1, None)
        except BenchError: pass
        else: raise AssertionError("checked unknown status relabelled as exhausted observation budget")
    frontend_records.append(records)
assert frontend_records[0] == frontend_records[1], "Arduino/IDF home evidence differs"
print("PASS: both real application frontends, methods33/34/35, revoked final-zero and unqualified status evidence")
