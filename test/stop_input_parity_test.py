"""Wired bench 07: actual application terminals of moves and velocity runs that
a stop/release input or another cause ended early, through the Python
validators. Forged witnesses are refused.

Wired bench 11: LIMIT_INPUT and SOFT_LIMIT terminals derived from those actual
records (the stop witness moved to a limit terminal, or the stopped report given
a soft-limit flag with its CRC recomputed) are accepted; forged ones are refused.

Arguments: move_app_test, idf_move_app_test, velocity_app_test, idf_velocity_app_test.
"""
import copy
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console, MAX_LINE, unique_object, wire_crc  # noqa: E402

MOVE = {"stop_input": (False, 24, True), "stopped_short": (False, 25, False),
        "stopped_short_unassigned": (False, 25, False), "completion_with_stop_input": (True, 0, False)}
VELOCITY = {"stop_input": (False, 26, True), "stopped_early": (False, 27, False),
            "input_then_timed_stop": (False, 26, True), "success_with_stop_input": (True, 0, False)}


def records(executable):
    output = subprocess.check_output([executable, "--emit-stop-input-results"], text=True, timeout=60)
    out = {}
    for line in output.splitlines():
        fixture = json.loads(line, object_pairs_hook=unique_object)
        assert fixture["case"] not in out, fixture["case"]
        assert len(json.dumps(fixture["record"], separators=(",", ":"))) <= MAX_LINE
        out[fixture["case"]] = fixture["record"]
    return out


def refused(check, record, change):
    forged = copy.deepcopy(record)
    change(forged)
    try:
        check(forged)
    except BenchError:
        return True
    return False


def as_limit(record, detail):
    """Move the recorded stop witness to an equivalent limit terminal."""
    out = copy.deepcopy(record)
    out["limit_inputs"] = dict(mask=out["stop_inputs"]["mask"], active_low=out["stop_inputs"]["active_low"], observed=True)
    out["limit_input_evidence"] = out["stop_input_evidence"]
    out["stop_input_evidence"] = copy.deepcopy(record["limit_input_evidence"])
    out["stop_inputs"].update(mask=0, active_low=0, observed=False)
    out["detail"] = detail
    return out


def flagged(raw_hex, motion):
    """Set the motion word of a retained observation and recompute its CRC."""
    raw = bytearray.fromhex(raw_hex)[:-2]
    raw[5:7] = motion.to_bytes(2, "big")
    return (bytes(raw) + wire_crc(bytes(raw)).to_bytes(2, "little")).hex().upper()


def as_soft_limit(record, motion, detail, names):
    out = copy.deepcopy(record)
    step = out["failure_evidence"]["step"]
    for name in names:
        if out[name]["step"] == step:
            out[name]["raw_hex"] = flagged(out[name]["raw_hex"], motion)
    out["raw_motion"] = motion
    out["detail"] = detail
    return out


def check_moves(executable):
    found = records(executable)
    assert set(found) == set(MOVE), (set(found), set(MOVE))
    def check(r):
        Console._check_move(r, 1, None, "relative")
    for name, (ok, detail, observed) in MOVE.items():
        record = found[name]
        check(record)
        assert record["ok"] is ok and record["detail"] == detail, (name, record["ok"], record["detail"])
        assert record["stop_inputs"]["observed"] is observed, name
        assert record["completion"] == ("observed" if ok else "not_observed"), name
    stop = found["stop_input"]
    assert stop["stop_inputs"]["mask"] == 8 and stop["stop_inputs"]["raw_inputs"] == 8
    assert refused(check, stop, lambda r: r["stop_inputs"].update(observed=False)), "unwitnessed STOP_INPUT accepted"
    assert refused(check, stop, lambda r: r.update(ok=True, state="succeeded", completion="observed",
                                                    outcome="observed", status="OK", detail=0)), "stopped move reported complete"
    short = found["stopped_short"]
    assert refused(check, short, lambda r: r["stop_inputs"].update(stopped_polls=1)), "one stopped poll ended the move"
    assert refused(check, short, lambda r: r["stop_inputs"].update(mask=0)), "observation size not bound to the mask"
    for record in found.values():
        assert record["limit_inputs"] == dict(mask=0, active_low=0, observed=False), "unassigned limit reports a witness"
    unassigned = found["stopped_short_unassigned"]
    assert refused(check, unassigned, lambda r: r["limit_inputs"].update(mask=2)), "observation size not bound to the limit mask"
    assert refused(check, unassigned, lambda r: r["limit_inputs"].update(observed=True)), "unmasked limit witness accepted"
    assert refused(check, short, lambda r: r["limit_inputs"].update(mask=2, active_low=4)), "active-low outside the limit mask"
    limit = as_limit(stop, 26)
    check(limit)
    assert refused(check, limit, lambda r: r["limit_inputs"].update(observed=False)), "unwitnessed LIMIT_INPUT accepted"
    assert refused(check, limit, lambda r: r.update(limit_input_evidence=copy.deepcopy(r["stop_input_evidence"]))), \
        "LIMIT_INPUT without its report accepted"
    assert refused(check, limit, lambda r: r["limit_inputs"].update(mask=4)), "limit report without a masked input accepted"
    assert refused(check, as_limit(short, 26), lambda r: None), "LIMIT_INPUT with an empty witness accepted"
    for bit in (0x20, 0x40):
        soft = as_soft_limit(unassigned, bit, 27, ("failure_evidence", "last_observation"))
        check(soft)
        assert refused(check, soft, lambda r: r.update(detail=22)), "soft-limit flag reported as a drive fault"
        assert refused(check, as_soft_limit(unassigned, bit | 16, 27, ("failure_evidence", "last_observation")),
                       lambda r: None), "released reported as SOFT_LIMIT"
    check(as_soft_limit(unassigned, 0x30, 22, ("failure_evidence", "last_observation")))
    return len(found)


def check_velocity(executable):
    found = records(executable)
    assert set(found) == set(VELOCITY), (set(found), set(VELOCITY))
    def check(r):
        Console._check_velocity(r, 1, None)
    for name, (ok, detail, observed) in VELOCITY.items():
        record = found[name]
        check(record)
        assert record["ok"] is ok and record["detail"] == detail, (name, record["ok"], record["detail"])
        assert record["stop_inputs"]["observed"] is observed, name
        assert record["stop_completion"] == "observed" and not record["needs_stop"], name
    assert found["stopped_early"]["stop_inputs"]["stopped_early"] is True
    # The negative trace: input reported, no stop before the timed stop.
    assert found["input_then_timed_stop"]["stop_inputs"]["stopped_early"] is False
    stop = found["stop_input"]
    assert refused(check, stop, lambda r: r["stop_inputs"].update(observed=False)), "unwitnessed STOP_INPUT accepted"
    assert refused(check, found["stopped_early"], lambda r: r["stop_inputs"].update(stopped_early=False)), \
        "unwitnessed STOPPED_EARLY accepted"
    for record in found.values():
        assert record["limit_inputs"] == dict(mask=0, active_low=0, observed=False), "unassigned limit reports a witness"
    assert refused(check, stop, lambda r: r["limit_inputs"].update(observed=True)), "unmasked limit witness accepted"
    limit = as_limit(stop, 28)
    check(limit)
    assert refused(check, limit, lambda r: r["limit_inputs"].update(observed=False)), "unwitnessed LIMIT_INPUT accepted"
    assert refused(check, limit, lambda r: r.update(limit_input_evidence=copy.deepcopy(r["stopped_evidence"]))), \
        "LIMIT_INPUT report differs from the failure report"
    early = found["stopped_early"]
    names = ("failure_evidence", "last_observation", "stopped_evidence")
    for bit in (0x20, 0x40):
        check(as_soft_limit(early, bit, 29, names))
    assert refused(check, early, lambda r: r.update(detail=29)), "SOFT_LIMIT without a flag accepted"
    assert refused(check, as_soft_limit(early, 0x28, 29, names), lambda r: None), "alarm reported as SOFT_LIMIT"
    return len(found)


assert len(sys.argv) == 5, "move_app, idf_move_app, velocity_app and idf_velocity_app executables are required"
moves = sum(check_moves(e) for e in sys.argv[1:3])
runs = sum(check_velocity(e) for e in sys.argv[3:5])
print(f"PASS: {moves} move and {runs} velocity stop-input terminals accepted, forged witnesses refused")
