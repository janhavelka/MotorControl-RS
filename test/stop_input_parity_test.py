"""Wired bench 07: actual application terminals of moves and velocity runs that
a stop/release input or another cause ended early, through the Python
validators. Forged witnesses are refused.

Arguments: move_app_test, idf_move_app_test, velocity_app_test, idf_velocity_app_test.
"""
import copy
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console, MAX_LINE, unique_object  # noqa: E402

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
    return len(found)


assert len(sys.argv) == 5, "move_app, idf_move_app, velocity_app and idf_velocity_app executables are required"
moves = sum(check_moves(e) for e in sys.argv[1:3])
runs = sum(check_velocity(e) for e in sys.argv[3:5])
print(f"PASS: {moves} move and {runs} velocity stop-input terminals accepted, forged witnesses refused")
