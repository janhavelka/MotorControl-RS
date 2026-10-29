"""Check actual bounded driver-settings outcomes through the Python parser."""
import copy
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console, MAX_LINE, unique_object
from bench_probe_test import check_fixture_routes

expected = {"read", "unknown_read", "full_update", "partial_lost_ack", "cancelled",
            "readback_mismatch", "deadline", "wrong_echo", "late_delivery", "unqualified",
            "unconfirmed_echo", "late_closure", "unconfirmed_read", "unconfirmed_readback"}
output = subprocess.check_output([sys.argv[1], "--driver-fixtures"], text=True, timeout=10)
seen = set()
for line in output.splitlines():
    fixture = json.loads(line, object_pairs_hook=unique_object)
    name, record = fixture["case"], fixture["record"]
    assert name in expected and name not in seen, name
    assert len(json.dumps(record, separators=(",", ":"))) < MAX_LINE
    Console._check_driver(record, 1, None)
    check_fixture_routes(record)
    for mutate in (lambda r: r.update(active_settings_known=True),
                   lambda r: r.update(configuration_generation=0),
                   lambda r: r.update(address=2),
                   lambda r: r["evidence"][0].__setitem__(0, 1),
                   lambda r: r["evidence"][0].__setitem__(5, "00")):
        broken = copy.deepcopy(record); mutate(broken)
        try:
            Console._check_driver(broken, 1, None)
        except BenchError:
            pass
        else:
            raise AssertionError((name, "malformed provenance accepted"))
    seen.add(name)
    if record["ok"]:
        broken = copy.deepcopy(record)
        broken["evidence"][-1][9] = False
        try:
            Console._check_driver(broken, 1, None)
        except BenchError:
            pass
        else:
            raise AssertionError((name, "unconfirmed response accepted as success"))
        for outcome in ("timing_unqualified", "unconfirmed_response", "deadline", "readback_mismatch"):
            broken = copy.deepcopy(record)
            broken.update(ok=False, state="failed", outcome=outcome, status="ILLEGAL_VALUE", detail=123456)
            try:
                Console._check_driver(broken, 1, None)
            except BenchError:
                pass
            else:
                raise AssertionError((name, "successful evidence disguised as failure", outcome))
assert seen == expected, (seen, expected)
print(f"PASS: {len(seen)} actual core/console driver terminals accepted by Python")

if len(sys.argv) > 2:
    expected = {"success", "prior_exception", "readback_mismatch", "lost_ack", "cancelled", "changed_generation"}
    output = subprocess.check_output([sys.argv[2], "--start-speed-fixtures"], text=True, timeout=20)
    seen = set()
    for line in output.splitlines():
        fixture = json.loads(line, object_pairs_hook=unique_object)
        name, record = fixture["case"], fixture["record"]
        assert name in expected and name not in seen, name
        assert len(json.dumps(record, separators=(",", ":"))) < MAX_LINE
        Console._check_driver(record, 1, ("3000",), "position_start_speed")
        check_fixture_routes(record)
        for mutate in (lambda r: r.update(physical_scaling_known=True),
                       lambda r: r.update(echo_readback_policy=True),
                       lambda r: r.update(requested_native=3001),
                       lambda r: r["evidence"][0].__setitem__(1, 0x21),
                       lambda r: r.update(driver_group="drive")):
            broken = copy.deepcopy(record); mutate(broken)
            try:
                Console._check_driver(broken, 1, ("3000",), "position_start_speed")
            except BenchError:
                pass
            else:
                raise AssertionError((name, "malformed start-speed evidence accepted"))
        if record["driver_kind"] == "update":
            for mutate in (lambda r: r["prior"].update(raw_hex="00"),
                           lambda r: r["prior"].update(raw=1),
                           lambda r: r["prior"].update(operation_id=0)):
                broken = copy.deepcopy(record); mutate(broken)
                try:
                    Console._check_driver(broken, 1, None, "position_start_speed")
                except BenchError:
                    pass
                else:
                    raise AssertionError((name, "missing prior provenance accepted"))
        seen.add(name)
    assert seen == expected, (seen, expected)
    print(f"PASS: {len(seen)} actual app/console start-speed terminals accepted by Python")

if len(sys.argv) > 3:
    expected = {"success", "prior_exception", "readback_mismatch", "lost_ack", "cancelled", "changed_generation"}
    output = subprocess.check_output([sys.argv[3], "--auxiliary-fixtures"], text=True, timeout=20)
    seen = set()
    for line in output.splitlines():
        fixture = json.loads(line, object_pairs_hook=unique_object)
        name, record = fixture["case"], fixture["record"]
        assert name in expected and name not in seen, name
        assert len(json.dumps(record, separators=(",", ":"))) < MAX_LINE
        Console._check_driver(record, 1, ("7",), "homing_auxiliary")
        check_fixture_routes(record)
        for mutate in (lambda r: r.update(active_policy_qualified=True),
                       lambda r: r.update(echo_readback_policy=True),
                       lambda r: r.update(requested_choice=12),
                       lambda r: r["evidence"][0].__setitem__(1, 0x31),
                       lambda r: r.update(driver_group="drive")):
            broken = copy.deepcopy(record); mutate(broken)
            try:
                Console._check_driver(broken, 1, ("7",), "homing_auxiliary")
            except BenchError:
                pass
            else:
                raise AssertionError((name, "malformed auxiliary evidence accepted"))
        if record["driver_kind"] == "update":
            for mutate in (lambda r: r["prior"].update(raw_hex="00"),
                           lambda r: r["prior"].update(raw=7),
                           lambda r: r["prior"].update(operation_id=0)):
                broken = copy.deepcopy(record); mutate(broken)
                try:
                    Console._check_driver(broken, 1, None, "homing_auxiliary")
                except BenchError:
                    pass
                else:
                    raise AssertionError((name, "missing prior provenance accepted"))
        seen.add(name)
    assert seen == expected, (seen, expected)
    print(f"PASS: {len(seen)} actual app/console auxiliary terminals accepted by Python")
