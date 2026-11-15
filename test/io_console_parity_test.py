"""Check actual IO core/formatter output with the shared strict Python parser."""
import copy
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console, MAX_LINE, unique_object
from bench_probe_test import check_fixture_routes

expected = {"read", "unknown_read", "full_update", "partial_lost_ack", "cancelled",
            "readback_mismatch", "deadline", "wrong_echo", "late_delivery", "unqualified",
            "unconfirmed_echo", "late_closure", "unconfirmed_read", "unconfirmed_readback",
            "unconfirmed_settled", "io_evidence_deadline"}
output = subprocess.check_output([sys.argv[1], "--io-fixtures"], text=True, timeout=10)
seen = set()
for line in output.splitlines():
    fixture = json.loads(line, object_pairs_hook=unique_object)
    name, record = fixture["case"], fixture["record"]
    assert name in expected and name not in seen, name
    assert len(json.dumps(record, separators=(",", ":"))) < MAX_LINE
    Console._check_driver(record, 1, None, "io")
    check_fixture_routes(record)
    mutations = [lambda r: r.update(active_settings_known=True),
                 lambda r: r.update(configuration_generation=0),
                 lambda r: r.update(address=2),
                 lambda r: r.update(driver_group="drive"),
                 lambda r: r.update(settlement="active"),
                 lambda r: r["evidence"][0].__setitem__(0, 1),
                 lambda r: r["evidence"][0].__setitem__(5, "00")]
    if name == "io_evidence_deadline":
        assert record["stationary_valid_until_us"] == 295
        mutations.append(lambda r: r.update(stationary_valid_until_us=310))
    if record["ok"]:
        mutations.append(lambda r: r["evidence"][-1].__setitem__(9, False))
    if name == "unconfirmed_settled":
        assert record["progress"][0][4] is False and record["progress"][0][9] == "unknown"
        mutations += [lambda r: r.update(echo_readback_policy=False),
                      lambda r: r["progress"][0].__setitem__(4, True),
                      lambda r: r["progress"][0].__setitem__(9, "acknowledged")]
    for mutate in mutations:
        broken = copy.deepcopy(record); mutate(broken)
        try:
            Console._check_driver(broken, 1, None, "io")
        except BenchError:
            pass
        else:
            raise AssertionError((name, "malformed provenance accepted"))
    seen.add(name)
assert seen == expected, (seen, expected)
print(f"PASS: {len(seen)} actual core/console IO terminals accepted by Python")

output = subprocess.check_output([sys.argv[1], "--io-qualification-fixtures"], text=True, timeout=10)
seen = set()
for line in output.splitlines():
    fixture = json.loads(line, object_pairs_hook=unique_object)
    name, record = fixture["case"], fixture["record"]
    assert name in ("absent", "member", "output") and name not in seen
    tokens = {"absent": ("1", "x0", "assign"), "member": ("member", "2", "x0", "finite-move"), "output": ("1", "y1", "assign")}[name]
    handle = SimpleNamespace(command="io-qualification-inspect", host_args=tokens)
    Console._check_io_qualification(handle, record)
    mutations = [lambda r: r.update(member=not r["member"]), lambda r: r.update(address=3),
                 lambda r: r.update(terminal="x1"), lambda r: r.update(purpose="segment"),
                 lambda r: r.update(operation_id=0), lambda r: r.update(bus_traffic=True),
                 lambda r: r.update(mapping="guessed"), lambda r: r.update(allowed_levels=4)]
    if name == "absent":
        mutations += [lambda r: r.update(valid=True), lambda r: r.update(target=1)]
    else:
        mutations += [lambda r: r.update(settings_operation_id=0), lambda r: r.update(protection_removal=True),
                      lambda r: r.update(observed_latest_us=1)]
        if name == "member":
            mutations += [lambda r: r.update(facts_supplied=False), lambda r: r.update(load="indicator")]
        else:
            mutations += [lambda r: r.update(load="undeclared"), lambda r: r.update(allowed_levels=0)]
    for mutate in mutations:
        broken = copy.deepcopy(record); mutate(broken)
        try:
            Console._check_io_qualification(handle, broken)
        except BenchError:
            pass
        else:
            raise AssertionError((name, "invalid I/O qualification accepted"))
    seen.add(name)
assert seen == {"absent", "member", "output"}
print("PASS: actual I/O qualification inspection accepted by Python")
