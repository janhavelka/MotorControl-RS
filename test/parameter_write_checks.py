"""Shared parity check of actual restore and D9 experiment console replies (wired bench 06)."""
import copy
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console, MAX_LINE, unique_object


def _tokens(command, record, requested):
    if record["request"] == "inspect":
        return ("inspect",)
    return requested[record["id"]]


def check_parameter_write_replies(executable, flag, command, requested, expected_errors):
    """Run ``executable flag``, check each reply line with the harness validator
    and reject forged variants. ``requested`` maps a request ID to the write
    tokens (a callable of the record for dynamic snapshot IDs)."""
    seen = set()
    lines = subprocess.check_output([executable, flag], text=True, timeout=60).splitlines()
    records = [json.loads(line, object_pairs_hook=unique_object) for line in lines if line.startswith("{")]
    assert records, "no restore replies"
    for record in records:
        assert record["command"] == command and len(json.dumps(record, separators=(",", ":"))) < MAX_LINE
        tokens = ("inspect",) if record["request"] == "inspect" else requested[record["id"]](record)
        handle = SimpleNamespace(command=command, host_args=tokens)
        Console._check_parameter_write(handle, record)
        seen.add(record["error"] if record["kind"] != "none" else "empty")
        if record["kind"] == "none" or record["pending"]:
            continue
        forged = []
        if record["session_ok"]:
            forged += [lambda r: r["readback"].__setitem__(0, (r["readback"][0] + 1) & 0xFFFF),
                       lambda r: r.update(execution="unknown"), lambda r: r.update(unsettled=True),
                       lambda r: r.update(readback_known=False, readback=[])]
        else:
            forged += [lambda r: r.update(session_ok=True)]
        if record["unsettled"]:
            forged += [lambda r: r.update(unsettled=False)]
        forged += [lambda r: r.update(first=r["first"] + 1), lambda r: r.update(count=r["count"] - 1),
                   lambda r: r.update(error="bogus")]
        for mutate in forged:
            broken = copy.deepcopy(record); mutate(broken)
            try:
                Console._check_parameter_write(handle, broken)
            except BenchError:
                pass
            else:
                raise AssertionError((command, record["id"], "forged session accepted"))
    assert set(expected_errors) <= seen, (seen, expected_errors)
    return len(records)
