"""Wired-bench records: interlock truth table, run folders, run checks,
baseline planning/restoration and edge correlation (prompt 04)."""
import copy
import hashlib
import io
import json
from contextlib import redirect_stdout
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import bench_probe as probe  # noqa: E402
import bench_records as records  # noqa: E402

CASES = json.loads((ROOT / "test" / "line_isolation_cases.json").read_text(encoding="utf-8"))
BASELINE = records.load_baseline(records.DEFAULT_BASELINE)
POWER_UP = [0, 1, 2, 3, 0, 0, 0, 0, 0]
MAPPING = {"mosfet1": 0, "relay2": 1, "relay1": 2, "mosfet2": 3}


class Pending:
    def __init__(self, operation_id, address):
        self.operation_id, self.address, self.accepted, self.terminal, self.command = \
            operation_id, address, True, None, "move-relative"


class FakeBench:
    """Three drives, the four bench outputs and aux1 behind the console API.

    Each command advances the controller clock 20 ms. Settings come from the
    committed baseline. Status bits follow ``mapping`` (output -> X index);
    drive 4's custom Y0 drives aux1 inverted.
    """

    def __init__(self, on_event=None, *, mapping=None, fixture="applied", older_image=False, board=None):
        self.on_event = on_event or (lambda *a, **k: None)
        self.synchronized = True
        self.calls = []
        self.now_us = 5_000_000
        self.host_s = 0.0
        self.selected = 1
        self.mapping = dict(mapping or MAPPING)
        self.fixture, self.older_image, self.board_override = fixture, older_image, board
        self.pending = {}
        self.operation = 100
        self.debug_mode = "off"
        self.sequence = 0
        self.transaction = 0
        self.config_functions = {}  # address -> functions read config reports instead of the I/O words
        self.owner_pending = 0       # owner requests pending, as drv reports them
        self.foreign_operations = 0  # operations admitted outside this console after the last read
        self.fail_write = None     # (key, value) -> uncertain write
        self.discard_write = None  # (key, value) -> acknowledged, stored value unchanged
        self.settings = {}
        for address, drive in BASELINE["drives"].items():
            self.settings[int(address)] = {k: copy.deepcopy(v["raw"]) for k, v in {**drive["live"], **drive["reads"]}.items()}
        self.state = {a: dict(running=False, raw_alarm=0, alarm_flag=False, enabled=True, silent=False, age_ms=0,
                              position=[0, 7000 + a]) for a in self.settings}
        self.outputs = {name: dict(name=name, gpio=1, active=False, hold_ms=0, ons=0, on_us=0, off_us=1000,
                                   due_us=0, off_reason="boot") for name in probe.BENCH_OUTPUTS}
        self.aux_active, self.edges, self.edge_count, self.overwritten = True, [], 0, 0

    # Console surface used by bench_session.run_recorded
    def emit(self, event, **fields):
        self.on_event(event, **fields)

    def clock(self):
        return self.host_s

    def sleep(self, seconds):
        self.host_s += seconds

    def drain_startup(self, seconds):
        pass

    def identify(self, **kwargs):
        return self.command("version")

    def io(self, address):
        return self.settings[address]["io"]

    def _tick(self):
        self.now_us += 20000
        self.host_s += 0.02

    def _next(self):
        self.operation += 1
        return self.operation

    def command(self, name, *, timeout_s=3.0, address=None, host_args=None, driver_args=None, **kwargs):
        self.calls.append((name, dict(address=address, host_args=host_args, driver_args=driver_args, **kwargs)))
        self._tick()
        result = self._command(name, address, tuple(host_args or ()), tuple(driver_args or ()), kwargs)
        result.setdefault("command", name)
        self.emit("line", text=json.dumps({"command": name, "ok": result.get("ok")}))
        return result

    def _started(self, address):
        return self.now_us - 15000 - int(self.state[address]["age_ms"] * 1000)

    def _inputs(self, address):
        bits = 0
        for output, index in self.mapping.items():
            if self.outputs[output]["active"]:
                bits |= 1 << index
        return bits

    def _state(self, address):
        s, io = self.state[address], self.io(address)
        custom_on = io[6] == 9 and io[8] & 1
        started = self._started(address)
        return dict(ok=True, state="succeeded", operation_id=self._next(), started_us=started,
                    serviced_us=self.now_us, address=address, state_blocks=[
                        dict(block=0, raw_alarm=s["raw_alarm"], alarm_known=True, raw_motion=0, unknown_motion_bits=0,
                             running=s["running"], alarm_flag=s["alarm_flag"], released=False, enabled=s["enabled"]),
                        dict(block=1, raw_inputs=self._inputs(address), raw_outputs=1 if custom_on else 0,
                             unknown_input_bits=0, unknown_output_bits=0),
                        dict(block=2, position_words=list(s["position"]), raw_speed=0, raw_position=s["position"][1],
                             raw_encoder_counts=None)],
                    steps=[dict(first=6, attempted_us=started, latest_us=started + 8000),
                           dict(first=8, attempted_us=started + 8000, latest_us=started + 16000),
                           dict(first=10, attempted_us=started + 16000, latest_us=started + 24000)])

    def _traffic(self, address, register, value):
        if self.debug_mode != "raw":
            return
        frame = bytes([address, 6, register >> 8, register & 255, value >> 8, value & 255])
        frame += probe.wire_crc(frame).to_bytes(2, "little")
        self.transaction += 1
        start = self.now_us - 12000
        for kind, at, raw, s, e in (("TX", start, frame, 0, 0), ("TX_END", start + 700, b"", 0, start + 700),
                                    ("RX", start + 4000, frame, start + 3300, start + 4000)):
            self.sequence += 1
            self.emit("traffic", response=dict(type="traffic", sequence=self.sequence, transaction=self.transaction,
                                                   kind=kind, at_us=at, start_us=s, end_us=e, uncertainty_us=50,
                                                   raw_hex=raw.hex().upper(), length=len(raw)))
        if address == records.FIXTURE["y0_owner"]:
            io = self.io(address)
            active = not (io[6] == 9 and io[8] & 1)  # Sinking wiring, read inverted.
            if active != self.aux_active:
                self.aux_active = active
                self.edge_count += 1
                self.edges = (self.edges + [[start + 5500, active]])[-16:]

    def _bench(self, tokens):
        request = probe.bench_arguments(tokens)
        if request["op"] == "on":
            o = self.outputs[request["output"]]
            o.update(active=True, hold_ms=request["hold_ms"], ons=o["ons"] + 1, on_us=self.now_us - 500,
                     due_us=self.now_us + request["hold_ms"] * 1000, off_reason="none")
        elif request["op"] in ("off", "off_all"):
            for name, o in self.outputs.items():
                if request["op"] == "off_all" or name == request["output"]:
                    if o["active"]:
                        o.update(active=False, off_us=self.now_us - 500, due_us=0, off_reason="request")
        elif request["op"] == "clear":
            self.edges, self.edge_count, self.overwritten = [], 0, 0
        return dict(ok=True, result="done", ready=True, now_us=self.now_us, bus_traffic=False, motion_command=False,
                    outputs=[dict(o) for o in self.outputs.values()],
                    input=dict(name="aux1", gpio=17, known=True, active=self.aux_active, edges=self.edge_count,
                               overwritten=self.overwritten, recent=[list(e) for e in self.edges]))

    def _wiring(self):
        board = self.board_override or {"inputs": [2, 2, 2, 2], "outputs": [2 if self.selected == 4 else 1, 1]}
        reply = dict(ok=True, result="done", target=self.selected, address=self.selected, generation=1,
                     inputs=list(board["inputs"]), outputs=list(board["outputs"]))
        if not self.older_image:
            reply.update(sources=["board"] * 6, board=board if self.fixture == "applied" else None, fixture=self.fixture)
            if self.fixture == "invalid":
                reply.update(fixture_error="duplicate_address", inputs=[0] * 4, outputs=[0] * 2,
                             sources=["invalid_table"] * 6)
        return reply

    def _group_write(self, address, read, index, value, register):
        if address != self.selected:
            return dict(ok=False, result="unavailable", operation_id=0)
        key = None
        if self.fail_write and self.fail_write == (read, index, value):
            return dict(ok=False, state="failed", outcome="transport_error", uncertain=True, operation_id=self._next())
        if not (self.discard_write and self.discard_write == (read, index, value)):
            self.settings[address][read][index] = value
        if read == "io":
            self._traffic(address, register, value)
        return dict(ok=True, state="succeeded", operation_id=self._next(), uncertain=False,
                    observation=dict(raw=list(self.settings[address][read])),
                    evidence_columns=probe.DRIVER_EVIDENCE_COLUMNS,
                    evidence=[[0, register, 1, True, 0, "", 8, 8, True, True, True, False, self.now_us - 8000,
                               self.now_us - 7900, self.now_us - 7000, 1, "OK", 0, 0, self.now_us - 12000]])

    def _command(self, name, address, host, driver, kwargs):
        if name == "version":
            return dict(ok=True, product="MotorControl-RS", protocol=3, outstanding_capacity=10)
        if name in ("host", "config", "load", "memory", "stats", "status", "health"):
            return dict(ok=True, active=dict(baud=115200, format="8N1"), blocked=False, workload_us=0,
                        owner_delay_us=0, console_bytes=0)
        if name == "drv":
            return dict(ok=True, recovery_required=False, pending=self.owner_pending, busy=False,
                        operation_id=self.operation + self.foreign_operations)
        if name == "debug":
            if host:
                self.debug_mode = host[0]
            return dict(ok=True, mode=self.debug_mode, observed=self.sequence, emitted=self.sequence, dropped=0,
                        missed=0, overwritten=0)
        if name == "wiring":
            return self._wiring()
        if name == "useaddr":
            self.selected = address
            return dict(ok=True, result="done", address=address, operation_id=0)
        if name == "motion-profile":
            words = self.settings[self.selected]["motion-profile"]
            return dict(ok=True, result="done", pending=False, restore_unsettled=False, saved=host[0] == "read",
                        restored=False, session_ok=True, original=list(words), current=list(words), rx_hex="00", id=1)
        if name == "bench":
            return self._bench(host)
        if name == "stop":
            for o in self.outputs.values():
                if o["active"]:
                    o.update(active=False, off_reason="motor_stop", due_us=0)
            self.state[address]["running"] = False
            return dict(ok=True, operation_id=self._next())
        if name == "io-qualification-install":
            probe.io_qualification_arguments(name, host)
            return dict(ok=True, result="done")
        if address is not None and address in self.state and self.state[address]["silent"]:
            return dict(ok=False, state="failed", outcome="transport_error", status="NO_RESPONSE", operation_id=self._next())
        if name == "read-state":
            return self._state(address)
        if name == "read-identity":
            return dict(ok=True, operation_id=self._next(), started_us=self._started(address),
                        identity=dict(self.settings[address]["identity"]))
        if name == "read-config":
            raw = dict(self.settings[address]["config"], input_polarity=self.io(address)[0])
            functions = self.config_functions.get(address, self.io(address)[1:5])
            return dict(ok=True, operation_id=self._next(), started_us=self._started(address), config=dict(
                raw=raw, inputs=[dict(function=f, known=True, inverted=False, wiring=2) for f in functions]))
        if name in ("home-read", "velocity-read"):
            key = "homing_parameters" if name == "home-read" else "velocity_parameters"
            group = "home" if name == "home-read" else "velocity"
            return dict(ok=True, operation_id=self._next(), **{key: dict(self.settings[address][group])})
        groups = {"io": "io", "control": "control", "driver": "driver"}
        if name in groups or name in ("tuning", "segment"):
            if name == "tuning":
                read, tokens = "tuning." + driver[0], driver[1:]
            elif name == "segment":
                read, tokens = "segment.%s.%s" % driver[:2], driver[2:]
            else:
                read, tokens = name, driver
            raw = self.settings[address][read]
            if tokens == ("read",):
                observation = dict(raw=copy.deepcopy(raw["raw"] if read == "driver" else raw))
                if read == "driver":
                    observation.update(positive_words=raw["positive_words"], negative_words=raw["negative_words"])
                observation["unknown_input_polarity_bits"] = 0
                return dict(ok=True, operation_id=self._next(), started_us=self._started(address), observation=observation)
            field, value = tokens[1], int(tokens[2])
            if read == "io":
                index = probe.IO_FIELDS.index(field)
                return self._group_write(address, "io", index, value, probe.IO_REGISTERS[index])
            if read.startswith("tuning."):
                index = probe.TUNING_FIELDS[records.TUNING_KEYS[driver[0]]].index(field)
                return self._group_write(address, read, index, value, 0x108 + index)
            raise AssertionError("fake has no setter for " + read)
        raise AssertionError("fake has no command " + name)


def run_folder(root, name="2026-11-19_wb04_fixture", **overrides):
    """A complete, passing run folder; ``overrides`` replace top-level parts."""
    folder = records.RunFolder.from_path(Path(root) / name).create()
    image = Path(root) / "image.bin"
    if not image.exists():
        image.write_bytes(b"firmware image")
    image_record = {"path": str(image), "sha256": records.sha256_file(image)}
    case = {"id": "H01", "required": True, "assertion": "the map equals FIXTURE", "required_levels": ["drive-reported"]}
    optional = {"id": "H01.extra", "required": False, "assertion": "optional", "required_levels": ["local"]}
    scenario = dict(image=image_record, commit="abc123", port="COM13", drives=[{"address": 1, "required_enabled": True}],
                    terminals=["x0"], commands=["io set x3 7 1"], cases=[case, optional], running_members=[],
                    write_list=[{"drive": 1, "key": "io.x3", "index": None, "candidate": 7}], bounds={},
                    prepared_stop=["stop fast 1"], restoration_plan={"admissible": True, "steps": []},
                    left_changed=[], expected_evidence={}, abort_conditions=[], cleanup=[], max_age_ms=300, facts={})
    scenario.update(overrides.pop("scenario", {}))
    records.write_scenario(folder, **scenario)
    for name_ in ("session.jsonl",):
        (folder / name_).write_text('{"event": "line"}\n', encoding="utf-8")
    (folder / "summary.json").write_text("{}\n", encoding="utf-8")
    (folder / "console.log").write_text("> @1 version\n", encoding="utf-8")
    folder.write_json("baseline_before.json", {"drives": {}})
    ending = overrides.pop("ending", {"drives": {"1": {"state": dict(ok=True, running=False, raw_speed=0, raw_alarm=0,
                                                                     alarm_flag=False, enabled=True)}}})
    folder.write_json("ending_reads.json", ending)
    cases = overrides.pop("cases", [dict(case, verdicts={"drive-reported": "PASS"}, reason=None, evidence=["session.jsonl"]),
                                    dict(optional, verdicts={"local": "FAIL"}, reason=None, evidence=[])])
    restoration = {"outputs_off": True, "port_closed": True,
                   "fields": [{"drive": 1, "key": "io.x3", "index": None, "baseline": 0, "ending": 0, "restored": True,
                               "uncertain": False, "storage": "RAM", "reason": None}],
                   "drives": [{"address": 1, "stopped": True, "alarm": False, "enabled": True, "required_enabled": True}]}
    restoration.update(overrides.pop("restoration", {}))
    records.write_result(folder, commit=scenario["commit"], image=image_record, cases=cases, restoration=restoration,
                         deviations=[])
    return folder


def snapshot(path):
    return {p.relative_to(path).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(Path(path).rglob("*")) if p.is_file()}


class DeclarationTest(unittest.TestCase):
    def test_matching_board_table_passes_and_selection_is_restored(self):
        bench = FakeBench()
        result = records.check_declarations(bench)
        self.assertEqual(result["verdict"], "PASS", result["problems"])
        self.assertEqual([r["address"] for r in result["rows"]], [1, 2, 4])
        self.assertEqual(bench.selected, 1)
        self.assertFalse(any(name.startswith("read") or name == "bench" for name, _ in bench.calls))

    def test_invalid_table_fails(self):
        result = records.check_declarations(FakeBench(fixture="invalid"))
        self.assertEqual(result["verdict"], "FAIL")
        self.assertIn("invalid", result["problems"][0])

    def test_older_image_without_board_fails(self):
        result = records.check_declarations(FakeBench(older_image=True))
        self.assertEqual(result["verdict"], "FAIL")
        self.assertIn("older", result["problems"][0])

    def test_board_differing_from_fixture_fails(self):
        result = records.check_declarations(FakeBench(board={"inputs": [2, 2, 2, 1], "outputs": [1, 1]}))
        self.assertEqual(result["verdict"], "FAIL")


class TruthTableTest(unittest.TestCase):
    def test_shared_cases_cover_li01_to_li15(self):
        self.assertEqual([c["id"] for c in CASES["cases"]], ["LI%02d" % n for n in range(1, 16)])
        self.assertEqual([c["id"] for c in CASES["cases"] if not c["harness_only"]],
                         ["LI%02d" % n for n in range(1, 10)] + ["LI14"])

    def test_each_case_reaches_its_decision(self):
        for case in CASES["cases"]:
            with self.subTest(case=case["id"]):
                result = records.isolation_decision(CASES["output"], records.case_participants(case),
                                                    max_age_ms=CASES["max_age_ms"], named=case["named"],
                                                    pending=case["pending"])
                expect = case["expect"]
                self.assertEqual((result["decision"], result["row"], result["address"]),
                                 (expect["decision"], expect["row"], expect["address"]), result["reason"])
                if "member_row" in expect:
                    member = next(p for p in result["participants"] if p["address"] == case["named"]["address"])
                    self.assertEqual(member["row"], expect["member_row"])

    def test_polarity_relaxes_no_row(self):
        case = copy.deepcopy(next(c for c in CASES["cases"] if c["id"] == "LI05"))
        case["drives"]["2"]["io"][0] = 8  # X3 inverted
        result = records.isolation_decision("mosfet2", records.case_participants(case))
        self.assertEqual((result["decision"], result["row"]), ("refuse", "L5"))

    def test_named_member_needs_its_line_effect_and_polarity(self):
        base = next(c for c in CASES["cases"] if c["id"] == "LI10")
        for change in ({"polarity": 1}, {"effect": "limit"}, {"output": "mosfet1"}):
            with self.subTest(change=change):
                named = dict(base["named"], **change)
                result = records.isolation_decision("mosfet2", records.case_participants(base),
                                                    named=named, pending=base["pending"])
                self.assertEqual((result["decision"], result["row"]), ("refuse", "L4"))


class InterlockConsoleTest(unittest.TestCase):
    def bench_for(self, case):
        bench = FakeBench()
        for key, drive in case["drives"].items():
            a = int(key)
            if drive.get("silent"):
                bench.state[a]["silent"] = True
                continue
            bench.settings[a]["io"][:] = drive["io"]
            bench.state[a].update(running=drive.get("running", False), raw_alarm=drive.get("raw_alarm", 0),
                                  alarm_flag=drive.get("alarm_flag", False), age_ms=drive.get("age_ms", 0))
        for p in case["pending"]:
            bench.pending[len(bench.pending) + 1] = Pending(p["operation_id"], p["address"])
        return bench

    def test_li_cases_through_bench_on(self):
        for case in CASES["cases"]:
            with self.subTest(case=case["id"]):
                bench = self.bench_for(case)
                record = {"max_age_ms": CASES["max_age_ms"],
                          "running_members": [case["named"]] if case["named"] else []}
                if case["expect"]["decision"] == "refuse":
                    with self.assertRaises(records.InterlockRefused) as caught:
                        records.bench_on(bench, "mosfet2", 200, record)
                    self.assertEqual(caught.exception.entry["row"], case["expect"]["row"], caught.exception.entry["reason"])
                    self.assertFalse(any(name == "bench" and (kw["host_args"] or ())[:1] == ("on",)
                                         for name, kw in bench.calls))
                    self.assertEqual(bench.outputs["mosfet2"]["ons"], 0)
                else:
                    records.bench_on(bench, "mosfet2", 200, record)
                    self.assertEqual(bench.outputs["mosfet2"]["ons"], 1)
                    ons = [kw["host_args"] for name, kw in bench.calls if name == "bench" and (kw["host_args"] or ())[:1] == ("on",)]
                    self.assertEqual(ons, [("on", "mosfet2", "200")])
                    entry = record["interlock"][-1]
                    self.assertEqual(sorted(entry["reads"]), [1, 2, 4])
                    self.assertTrue(all(age <= 300 for reads in entry["age_at_on_ms"].values() for age in reads.values()))

    def test_permitted_stop_pulse_during_named_move(self):
        case = next(c for c in CASES["cases"] if c["id"] == "LI10")
        bench = self.bench_for(case)
        record = {"running_members": [case["named"]]}
        records.bench_on(bench, "mosfet2", 50, record)
        member = next(p for p in record["interlock"][-1]["participants"] if p["address"] == 4)
        self.assertEqual(member["row"], "L3")

    def test_unexpected_running_drive_refuses(self):
        for cid in ("LI09", "LI13"):
            case = next(c for c in CASES["cases"] if c["id"] == cid)
            bench = self.bench_for(case)
            with self.subTest(case=cid), self.assertRaises(records.InterlockRefused):
                records.bench_on(bench, "mosfet2", 50, {"running_members": [case["named"]] if case["named"] else []})

    def test_output_already_on_refuses(self):
        bench = FakeBench()
        bench.outputs["relay1"].update(active=True, off_reason="none")
        with self.assertRaises(records.InterlockRefused) as caught:
            records.bench_on(bench, "mosfet1", 100, {})
        self.assertIn("already on", caught.exception.entry["reason"])

    def test_default_age_backstop_refuses_slow_reads(self):
        self.assertEqual(records.MAX_AGE_MS, 1000)
        bench = FakeBench()
        bench.state[1]["age_ms"] = 1200
        with self.assertRaises(records.InterlockRefused) as caught:
            records.bench_on(bench, "mosfet1", 100, {})
        self.assertEqual(caught.exception.entry["row"], "L1")
        self.assertIn("older than 1000", caught.exception.entry["reason"])

    def test_reads_must_be_the_last_bus_traffic(self):
        for damage in ("foreign operation", "owner pending", "gap"):
            bench = FakeBench()
            if damage == "foreign operation":
                bench.foreign_operations = 1
            elif damage == "owner pending":
                bench.owner_pending = 1
            else:
                original = bench._next
                calls = {"n": 0}

                def skipping():
                    calls["n"] += 1
                    if calls["n"] == 3:
                        bench.operation += 1  # another operation took an ID between two interlock reads
                    return original()
                bench._next = skipping
            with self.subTest(damage=damage):
                with self.assertRaises(records.InterlockRefused) as caught:
                    records.bench_on(bench, "mosfet1", 100, {"input_agreement": {"verdict": "PASS", "problems": []}})
                self.assertEqual(caught.exception.entry["row"], "L1")
                self.assertIn("last bus traffic", caught.exception.entry["reason"])
                self.assertFalse(any(n == "bench" and (kw["host_args"] or ())[:1] == ("on",) for n, kw in bench.calls))

    def test_sends_inside_the_window_are_the_interlock_s_own(self):
        bench = FakeBench()
        record = {}
        records.bench_on(bench, "mosfet1", 100, record)
        ordering = record["interlock"][-1]["ordering"]
        self.assertEqual(ordering["sent"], ["io", "read-state"] * 3 + ["drv", "bench"])
        self.assertEqual(ordering["problems"], [])
        self.assertEqual(ordering["drv_operation_id"], ordering["operation_ids"][-1])

    def test_per_on_reads_are_io_then_state_and_agreement_runs_once(self):
        bench = FakeBench()
        record = {}
        records.bench_on(bench, "mosfet1", 100, record)
        bench.command("bench", host_args=("off",))
        first = len(bench.calls)
        records.bench_on(bench, "relay2", 100, record)
        second = [(name, kw["address"]) for name, kw in bench.calls[first:] if name in ("io", "read-config", "read-state")]
        self.assertEqual(second, [("io", 1), ("read-state", 1), ("io", 2), ("read-state", 2), ("io", 4), ("read-state", 4)])
        self.assertEqual(record["input_agreement"]["verdict"], "PASS")
        self.assertEqual(sorted(record["interlock"][-1]["reads"][1]), ["io", "read-state"])

    def test_io_and_config_disagreement_refuses_every_output(self):
        bench = FakeBench()
        bench.config_functions[2] = [1, 2, 3, 7]
        with self.assertRaises(records.InterlockRefused) as caught:
            records.bench_on(bench, "relay1", 100, {})
        self.assertEqual(caught.exception.entry["row"], "L1")
        self.assertIn("disagree", caught.exception.entry["reason"])
        self.assertFalse(any(n == "bench" and (kw["host_args"] or ())[:1] == ("on",) for n, kw in bench.calls))

    def test_failed_declarations_refuse_every_output(self):
        bench = FakeBench(fixture="invalid")
        with self.assertRaises(records.InterlockRefused):
            records.bench_on(bench, "mosfet1", 100, {})
        self.assertFalse(any(name.startswith("read") for name, _ in bench.calls))


class BenchCliTest(unittest.TestCase):
    """``bench_probe.py bench on`` is a harness path: it runs the interlock first."""

    def run_cli(self, bench, tokens):
        from unittest.mock import patch

        class Port:
            def close(self):
                pass
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(probe, "open_port", lambda *a: Port()), \
                patch.object(probe, "Console", lambda port, on_event: bench), \
                redirect_stdout(io.StringIO()), patch("sys.stderr", io.StringIO()):
            return probe.main(["--port", "COM13", "--log", str(Path(tmp) / "cli.jsonl"), "bench", *tokens])

    def ons(self, bench):
        return [kw["host_args"] for name, kw in bench.calls if name == "bench" and (kw["host_args"] or ())[:1] == ("on",)]

    def test_admitted_on_sends_one_bench_on_after_the_reads(self):
        bench = FakeBench()
        self.assertEqual(self.run_cli(bench, ["on", "mosfet1", "100"]), 0)
        self.assertEqual(self.ons(bench), [("on", "mosfet1", "100")])
        names = [name for name, _ in bench.calls]
        self.assertLess(names.index("read-state"), names.index("bench"))

    def test_refused_on_sends_nothing(self):
        bench = FakeBench()
        bench.settings[2]["io"][1] = 7
        self.assertEqual(self.run_cli(bench, ["on", "mosfet1", "100"]), 1)
        self.assertEqual(self.ons(bench), [])
        self.assertEqual(self.run_cli(FakeBench(), ["off"]), 0)


class BeforeFlashTest(unittest.TestCase):
    def test_power_up_words_and_stopped_drives_pass(self):
        self.assertEqual(records.before_flash_check(FakeBench())["verdict"], "PASS")

    def test_trigger_function_running_drive_or_disagreement_fails(self):
        for change in ("function", "running", "disagreement"):
            bench = FakeBench()
            if change == "function":
                bench.settings[2]["io"][4] = 16
            elif change == "running":
                bench.state[4]["running"] = True
            else:
                bench.config_functions[1] = [1, 2, 3, 5]
            with self.subTest(change=change):
                result = records.before_flash_check(bench)
                self.assertEqual(result["verdict"], "FAIL")
                self.assertFalse(any(name == "bench" and kw["host_args"] for name, kw in bench.calls))


class RunFolderTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def test_name_and_exclusive_creation(self):
        folder = records.RunFolder("2026-11-19", "04", "input_map", Path(self.tmp.name)).create()
        self.assertEqual(folder.name, "2026-11-19_wb04_input_map")
        with self.assertRaises(FileExistsError):
            records.RunFolder.from_path(folder.path).create()
        for bad in ("2026-11-19_wb4_x", "2026-13-01_wb04_x", "2026-11-19_wb04_Bad", "x"):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                records.RunFolder.from_path(Path(self.tmp.name) / bad)

    def test_scenario_and_result_writers_validate(self):
        folder = records.RunFolder("2026-11-19", "04", "bad", Path(self.tmp.name)).create()
        with self.assertRaises(records.RecordError):
            records.write_scenario(folder, image={"path": "x", "sha256": "0" * 64})
        with self.assertRaises(records.RecordError):
            records.write_result(folder, commit="c", image={"path": "x", "sha256": "0" * 64},
                                 cases=[{"id": "A", "required": True, "assertion": "a", "required_levels": ["local"],
                                         "verdicts": {"local": "SKIPPED"}, "reason": None, "evidence": []}],
                                 restoration={"outputs_off": True, "fields": [], "drives": [], "port_closed": True},
                                 deviations=[])


class CheckRunTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.count = 0

    def folder(self, **overrides):
        self.count += 1
        return run_folder(self.tmp.name, "2026-11-19_wb04_f%d" % self.count, **overrides)

    def edit(self, folder, name, change):
        data = json.loads((folder / name).read_text(encoding="utf-8"))
        change(data)
        (folder / name).write_text(json.dumps(data), encoding="utf-8")

    def test_complete_folder_passes_both(self):
        folder = self.folder()
        report = records.check_run(folder.path)
        self.assertEqual((report["integrity"]["verdict"], report["session"]["verdict"]), ("PASS", "PASS"), report)
        self.assertEqual(records.check_exit(report), 0)

    def assert_integrity_fails(self, folder, text):
        before = snapshot(folder.path)
        report = records.check_run(folder.path)
        self.assertEqual(report["integrity"]["verdict"], "FAIL")
        self.assertTrue(any(text in p for p in report["integrity"]["problems"]), report["integrity"]["problems"])
        self.assertEqual(records.check_exit(report), 1)
        self.assertEqual(snapshot(folder.path), before)

    def test_integrity_rules(self):
        rules = {
            "missing": (lambda f: (f / "console.log").unlink(), "missing console.log"),
            "scenario schema": (lambda f: self.edit(f, "scenario.json", lambda d: d.update(schema="x")), "scenario schema"),
            "result schema": (lambda f: self.edit(f, "result.json", lambda d: d.pop("deviations")), "result lacks deviations"),
            "image": (lambda f: self.edit(f, "result.json", lambda d: d["image"].update(sha256="0" * 64)), "image SHA-256 differs"),
            "commit": (lambda f: self.edit(f, "result.json", lambda d: d.update(commit="other")), "commit differs"),
            "image file": (lambda f: Path(json.loads((f / "scenario.json").read_text())["image"]["path"]).write_bytes(b"other"),
                           "image file"),
            "no case": (lambda f: (self.edit(f, "result.json", lambda d: d.update(cases=[])),
                                   self.edit(f, "scenario.json", lambda d: d.update(cases=[]))), "no case"),
            "verdict": (lambda f: self.edit(f, "result.json", lambda d: d["cases"][0]["verdicts"].update({"drive-reported": "SKIP"})),
                        "verdict 'SKIP'"),
            "evidence": (lambda f: self.edit(f, "result.json", lambda d: d["cases"][0].update(evidence=["gone.json"])),
                         "evidence gone.json"),
        }
        for verdict in records.NEEDS_REASON:
            rules["reason " + verdict] = (lambda f, v=verdict: self.edit(f, "result.json", lambda d: d["cases"][0]["verdicts"].update(
                {"drive-reported": v})), "needs a reason")
        for name, (damage, text) in rules.items():
            with self.subTest(rule=name):
                folder = self.folder()
                damage(folder.path)
                self.assert_integrity_fails(folder, text)

    def assert_session_fails(self, folder, text):
        before = snapshot(folder.path)
        report = records.check_run(folder.path)
        self.assertEqual(report["integrity"]["verdict"], "PASS", report["integrity"]["problems"])
        self.assertEqual(report["session"]["verdict"], "FAIL")
        self.assertTrue(any(text in p for p in report["session"]["problems"]), report["session"]["problems"])
        self.assertEqual(records.check_exit(report), 2)
        self.assertEqual(snapshot(folder.path), before)

    def test_failing_sessions(self):
        field = {"drive": 1, "key": "io.x3", "index": None, "baseline": 0, "ending": 7, "restored": False,
                 "uncertain": False, "storage": "RAM", "reason": "readback refused"}
        stopped = dict(ok=True, running=False, raw_speed=0, raw_alarm=0, alarm_flag=False, enabled=True)
        drive = {"address": 1, "stopped": True, "alarm": False, "enabled": True, "required_enabled": True}
        fixtures = {
            "failed restoration": ({"restoration": {"fields": [field]}}, "not restored"),
            "missing drive": ({"ending": {"drives": {}}}, "missing from the ending reads"),
            "running": ({"ending": {"drives": {"1": {"state": dict(stopped, running=True)}}},
                         "restoration": {"drives": [dict(drive, stopped=False)]}}, "not reported stopped"),
            "uncertain": ({"restoration": {"fields": [dict(field, ending=0, restored=True, uncertain=True)]}}, "uncertain write"),
            "unplanned": ({"restoration": {"fields": [dict(field, ending=0, restored=True),
                                                      dict(field, key="io.x2", baseline=3, ending=1)]}}, "unplanned left-changed"),
            "outputs": ({"restoration": {"outputs_off": False}}, "outputs not confirmed off"),
            "port": ({"restoration": {"port_closed": False}}, "port not confirmed closed"),
        }
        for verdict in ("FAIL", "NOT RUN", "BLOCKED", "indeterminate"):
            case = {"id": "H01", "required": True, "assertion": "the map equals FIXTURE",
                    "required_levels": ["drive-reported"], "verdicts": {"drive-reported": verdict},
                    "reason": "stated reason", "evidence": []}
            optional = {"id": "H01.extra", "required": False, "assertion": "optional", "required_levels": ["local"],
                        "verdicts": {"local": "PASS"}, "reason": None, "evidence": []}
            fixtures["required " + verdict] = ({"cases": [case, optional]}, "is %s" % verdict)
        for name, (overrides, text) in fixtures.items():
            with self.subTest(fixture=name):
                self.assert_session_fails(self.folder(**copy.deepcopy(overrides)), text)

    def test_planned_left_changed_field_passes(self):
        field = {"drive": 1, "key": "io.x3", "index": None, "baseline": 0, "ending": 7, "restored": False,
                 "uncertain": False, "storage": "RAM", "reason": "planned"}
        folder = self.folder(scenario={"left_changed": [{"drive": 1, "key": "io.x3", "index": None, "storage": "RAM",
                                                         "reason": "kept for prompt 05"}]},
                             restoration={"fields": [field]})
        self.assertEqual(records.check_run(folder.path)["session"]["verdict"], "PASS")

    def test_partial_folder_names_missing_items_and_cli_exit_codes(self):
        folder = records.RunFolder("2026-11-19", "04", "partial", Path(self.tmp.name)).create()
        (folder / "scenario.json").write_text("{}", encoding="utf-8")
        report = records.check_run(folder.path)
        self.assertIn("missing session.jsonl", report["integrity"]["problems"][0])
        with redirect_stdout(io.StringIO()):
            self.assertEqual(records.main(["check", str(folder.path)]), 1)
            self.assertEqual(records.main(["check", str(self.folder().path)]), 0)
            self.assertEqual(records.main(["check", str(self.folder(restoration={"outputs_off": False}).path)]), 2)


class BaselineTest(unittest.TestCase):
    def test_generator_reproduces_the_committed_fields(self):
        for address, drive in BASELINE["drives"].items():
            raws = {k: v["raw"] for k, v in {**drive["live"], **drive["reads"]}.items()}
            self.assertEqual(records.baseline_fields(int(address), raws), drive["fields"])

    def test_load_refuses_fields_without_route_or_reason_and_unexplained_history(self):
        with tempfile.TemporaryDirectory() as tmp:
            for damage in ("route", "history"):
                data = copy.deepcopy(BASELINE)
                if damage == "route":
                    field = next(f for f in data["drives"]["1"]["fields"] if f["restore"] is None)
                    field.pop("none_reason")
                else:
                    data["history"] = [{"date": "2026-11-19", "prompt": "04", "reason": ""}]
                path = Path(tmp) / (damage + ".json")
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.subTest(damage=damage), self.assertRaises(records.RecordError):
                    records.load_baseline(path)

    def test_failed_read_is_unknown_never_equal(self):
        raws = {k: v["raw"] for k, v in BASELINE["drives"]["1"]["reads"].items()}
        raws["io"] = None
        raws["tuning.filters"] = list(raws["tuning.filters"])
        raws["tuning.filters"][0] = 3
        diff = {(d["key"], d["index"]): d for d in records.diff_fields(BASELINE, {1: raws})}
        self.assertEqual(diff[("io.x3", None)]["status"], "unknown")
        self.assertEqual(diff[("filters.input-filter", None)]["status"], "differs")
        self.assertEqual(diff[("control.lock-delay", None)]["status"], "equal")
        self.assertEqual({d["drive"] for d in diff.values()}, {1})

    def test_rebaseline_keeps_dispositions_and_records_history(self):
        new = copy.deepcopy(BASELINE)
        new.update(date="2026-11-20", commit="def", run="build/bench/2026-11-20_wb17_save")
        sub = next(f for f in new["drives"]["2"]["fields"] if f["key"] == "drive.subdivision")
        sub["value"], sub["restore"] = 1600, dict(sub["restore"], command="driver set subdivision 1600 2", facts="other")
        pair = next(f for f in new["drives"]["2"]["fields"] if f["key"] == "drive.positive-limit")
        pair.pop("none_reason")
        pair["restore"] = {"command": "driver set positive-limit 0 2", "facts": "typed pair route", "readback": "driver",
                           "order": None, "requires": "10"}
        doc = records.rebaseline(BASELINE, new, "save in 17", "17")
        fields = {f["key"]: f for f in doc["drives"]["2"]["fields"] if f["index"] is None}
        self.assertEqual(fields["drive.subdivision"]["value"], 1600)
        self.assertEqual(fields["drive.subdivision"]["restore"]["facts"],
                         next(f for f in BASELINE["drives"]["2"]["fields"] if f["key"] == "drive.subdivision")["restore"]["facts"])
        self.assertEqual(fields["drive.subdivision"]["restore"]["command"], "driver set subdivision 1600 2")
        self.assertEqual(fields["drive.positive-limit"]["restore"]["requires"], "10")
        entry = doc["history"][-1]
        self.assertEqual((entry["reason"], entry["prompt"], entry["previous"]["commit"]), ("save in 17", "17", BASELINE["commit"]))
        self.assertEqual({c["change"] for c in entry["changes"]}, {"value", "disposition"})
        with self.assertRaises(records.RecordError):
            records.rebaseline(BASELINE, new, " ", "17")
        self.assertEqual(records.compare_baselines(BASELINE, copy.deepcopy(BASELINE)), [])


def experiment(**changes):
    record = {"image_sha256": "AB" * 32, "prompt": "10", "case": "E1", "drive": 1,
              "field": {"key": "drive.positive-limit", "index": None}, "window": {"start": 0x37, "count": 4},
              "write": {"command": "driver", "kwargs": {"driver_args": ["set", "positive-limit", "0"], "address": 1},
                        "text": "experimental FC10 0x0037/4"},
              "readback": {"command": "driver", "kwargs": {"driver_args": ["read"], "address": 1},
                           "words": "observation.pair_words"},
              "source": {"operation_id": 900, "words": [0, 0, 0, 0]}}
    record.update(changes)
    return record


CONTEXT = {"image_sha256": "AB" * 32, "prompt": "10",
           "source_reads": {900: {"address": 1, "window": {"start": 0x37, "count": 4}, "words": [0, 0, 0, 0],
                                  "before_first_write": True}}}
PAIR = [{"drive": 1, "key": "drive.positive-limit", "index": None, "candidate": [0, 100], "case": "E1"}]


class PlanTest(unittest.TestCase):
    def test_planning_sends_nothing(self):
        bench = FakeBench()
        plan = records.restoration_plan(BASELINE, PAIR, "03", {}, experiments=[experiment()], context=CONTEXT)
        self.assertTrue(plan["admissible"], plan["refused"])
        self.assertEqual(plan["steps"][0]["route"], "experiment")
        self.assertEqual(bench.calls, [])

    def test_each_experimental_mismatch_refuses(self):
        mismatches = {
            "image": experiment(image_sha256="CD" * 32),
            "case": experiment(case="E2"),
            "drive": experiment(drive=2),
            "window": experiment(window={"start": 0x38, "count": 4}),
            "source operation ID": experiment(source={"operation_id": 901, "words": [0, 0, 0, 0]}),
            "source words": experiment(source={"operation_id": 900, "words": [0, 1, 0, 0]}),
            "field": experiment(field={"key": "drive.negative-limit", "index": None}),
        }
        for name, record in mismatches.items():
            with self.subTest(mismatch=name):
                plan = records.restoration_plan(BASELINE, PAIR, "03", {}, experiments=[record], context=CONTEXT)
                self.assertFalse(plan["admissible"])
                self.assertIn(name.split()[0] if name != "source operation ID" else "source operation ID",
                              plan["refused"][0]["reason"])

    def test_null_route_without_record_and_unstated_facts_refuse(self):
        plan = records.restoration_plan(BASELINE, PAIR, "03", {})
        self.assertIn("no admissible route", plan["refused"][0]["reason"])
        for entry, facts, text in (
                ({"drive": 1, "key": "filters.input-filter", "index": None, "candidate": 3}, {}, "configure facts"),
                ({"drive": 2, "key": "io.x3", "index": None, "candidate": 7}, {"2": {"assign": {}}}, "triggers-inhibited"),
                ({"drive": 4, "key": "io.y0", "index": None, "candidate": 9}, {"4": {"assign": {}}}, "load and levels"),
                ({"drive": 1, "key": "io.input-polarity", "index": None, "candidate": 8}, {"1": {"assign": {}}}, "requires prompt 05"),
                ({"drive": 1, "key": "control.algorithm", "index": None, "candidate": 1}, {}, "no admissible route")):
            with self.subTest(key=entry["key"]):
                plan = records.restoration_plan(BASELINE, [entry], "03", facts)
                self.assertFalse(plan["admissible"])
                self.assertIn(text, plan["refused"][0]["reason"])

    def test_ordering_and_named_facts(self):
        write_list = [{"drive": 4, "key": "io.y0", "index": None, "candidate": 9},
                      {"drive": 4, "key": "io.custom", "index": None, "candidate": 1},
                      {"drive": 4, "key": "io.x1", "index": None, "candidate": 16},
                      {"drive": 4, "key": "io.x3", "index": None, "candidate": 7}]
        facts = {"4": {"assign": {"y0": ["indicator", "either"], "x1": ["triggers-inhibited", "protection-removal"],
                                  "x3": ["triggers-inhibited"]}}}
        plan = records.restoration_plan(BASELINE, write_list, "03", facts)
        self.assertTrue(plan["admissible"], plan["refused"])
        self.assertEqual([s["key"] for s in plan["steps"]], ["io.x3", "io.x1", "io.custom", "io.y0"])
        self.assertEqual(plan["steps"][0]["facts"], {"x3": ["triggers-inhibited"]})
        self.assertEqual(plan["steps"][2]["facts"], {"y0": ["indicator", "either"]})
        reverse = records.restoration_plan(BASELINE, [{"drive": 4, "key": "io.x1", "index": None, "candidate": 16}],
                                           "03", {"4": {"assign": {"x1": ["triggers-inhibited"]}}})
        self.assertTrue(reverse["admissible"], reverse["refused"])
        _, refusal = records.plan_entry(BASELINE, {"drive": 4, "key": "io.x1", "index": None, "candidate": 16}, "03",
                                        {"4": {"assign": {"x1": ["triggers-inhibited"]}}}, restore=False)
        self.assertIn("protection-removal", refusal["reason"])
        start = records.restoration_plan(BASELINE, [{"drive": 1, "key": "profile.start-speed", "index": None, "candidate": 31},
                                                    {"drive": 1, "key": "profile.target", "index": None, "candidate": [0, 100]}],
                                         "03", {"1": {"configure": {}, "start_speed_min_rpm": 30}})
        self.assertTrue(start["admissible"], start["refused"])
        self.assertEqual([s["key"] for s in start["steps"]],
                         ["profile.target", "profile.start-speed", "profile.start-speed-relation"])
        self.assertEqual(start["steps"][0]["write"]["text"], "motion-profile stage 100 100 60 0 5000 0 1")


class RestoreTest(unittest.TestCase):
    def plan(self, key="filters.input-filter", candidate=3, facts=None):
        return records.restoration_plan(BASELINE, [{"drive": 1, "key": key, "index": None, "candidate": candidate}],
                                        "03", facts or {"1": {"configure": {}}})

    def test_equal_or_unknown_fields_are_never_written(self):
        bench = FakeBench()
        results = records.restore(bench, json.loads(json.dumps(self.plan())))
        self.assertEqual(results[0]["reason"], "already equal to the captured value; no write")
        self.assertFalse(any(kw["driver_args"] and "set" in kw["driver_args"] for _, kw in bench.calls))
        bench.state[1]["silent"] = True
        results = records.restore(bench, self.plan())
        self.assertIn("unknown", results[0]["reason"])
        self.assertFalse(any(kw["driver_args"] and "set" in kw["driver_args"] for _, kw in bench.calls))

    def test_differing_field_restored_through_its_route_and_read_back(self):
        bench = FakeBench()
        bench.settings[1]["tuning.filters"][0] = 3
        results = records.restore(bench, json.loads(json.dumps(self.plan())))
        self.assertTrue(results[0]["restored"], results[0])
        self.assertEqual(bench.settings[1]["tuning.filters"][0], 2)
        names = [name for name, _ in bench.calls]
        self.assertEqual(names.count("io-qualification-install"), 4)
        self.assertLess(names.index("io-qualification-install"), names.index("tuning", names.index("io-qualification-install")))

    def test_uncertain_restoration_is_not_replayed(self):
        bench = FakeBench()
        bench.settings[1]["tuning.filters"][0] = 3
        bench.fail_write = ("tuning.filters", 0, 2)
        results = records.restore(bench, self.plan())
        self.assertTrue(results[0]["uncertain"])
        writes = [kw for name, kw in bench.calls if name == "tuning" and "set" in kw["driver_args"]]
        self.assertEqual(len(writes), 1)

    def test_experimental_route_restores_and_readback_mismatch_fails_session(self):
        class PairBench(FakeBench):
            pair = [0, 100]
            discard = False

            def _command(self, name, address, host, driver, kwargs):
                if name == "driver" and driver[:2] == ("set", "positive-limit"):
                    if not self.discard:
                        self.pair = [0, 0]
                    return dict(ok=True, operation_id=self._next())
                if name == "driver" and driver == ("read",):
                    reply = super()._command(name, address, host, driver, kwargs)
                    reply["observation"]["pair_words"] = list(self.pair) + [0, 0]
                    reply["observation"]["positive_words"] = list(self.pair)
                    return reply
                return super()._command(name, address, host, driver, kwargs)
        plan = json.loads(json.dumps(records.restoration_plan(BASELINE, PAIR, "03", {}, experiments=[experiment()],
                                                              context=CONTEXT)))
        bench = PairBench()
        results = records.restore(bench, plan)
        self.assertTrue(results[0]["restored"], results[0])
        self.assertEqual(bench.pair, [0, 0])
        bench = PairBench()
        bench.discard = True
        results = records.restore(bench, plan)
        self.assertTrue(results[0]["uncertain"])
        self.assertIn("readback differs", results[0]["reason"])
        with tempfile.TemporaryDirectory() as tmp:
            field = {k: results[0][k] for k in records.FIELD_KEYS}
            folder = run_folder(tmp, scenario={"write_list": PAIR}, restoration={"fields": [dict(field, index=None)]})
            report = records.check_run(folder.path)
            self.assertEqual(report["integrity"]["verdict"], "PASS", report)
            self.assertEqual(report["session"]["verdict"], "FAIL")


def traffic(seq, transaction, kind, at, raw="", start=0, end=0, unc=0):
    return {"sequence": seq, "transaction": transaction, "kind": kind, "at_us": at, "start_us": start, "end_us": end,
            "uncertainty_us": unc, "raw_hex": raw}


FC06_ON = (bytes([4, 6, 0, 0x4F, 0, 1]) + probe.wire_crc(bytes([4, 6, 0, 0x4F, 0, 1])).to_bytes(2, "little")).hex()
READ = (bytes([4, 3, 0, 8, 0, 2]) + probe.wire_crc(bytes([4, 3, 0, 8, 0, 2])).to_bytes(2, "little")).hex()


def view(now, edges, recent, overwritten=0):
    return {"now_us": now, "input": {"active": True, "edges": edges, "overwritten": overwritten, "recent": recent}}


class CorrelationTest(unittest.TestCase):
    def stream(self):
        return [traffic(1, 1, "TX", 900_000, READ), traffic(2, 1, "RX", 905_000, "00", 902_000, 904_000, 40),
                traffic(3, 2, "TX", 1_000_000, FC06_ON), traffic(4, 2, "TX_END", 1_000_700, end=1_000_700),
                traffic(5, 2, "RX", 1_004_100, FC06_ON, 1_003_300, 1_004_000, 50),
                traffic(6, 3, "TX", 1_100_000, READ), traffic(7, 3, "RX", 1_104_000, "00", 1_102_000, 1_103_000, 40)]

    def test_normal_pairing(self):
        result = records.correlate(self.stream(), [view(990_000, 0, []), view(1_050_000, 1, [[1_006_000, False]])])
        edge = result["aux1"]["edges"][0]
        self.assertEqual(edge["status"], "measured")
        self.assertEqual(edge["after_request_start_us"], 6000)
        self.assertEqual(edge["after_reply_end_us"], [1950, 2050])
        self.assertEqual(edge["relationship"], "after reply end")
        self.assertEqual(result["aux1"]["writes"][0]["edges"], 1)

    def test_overwritten_debug_ring_is_indeterminate(self):
        stream = [r for r in self.stream() if r["sequence"] != 4]
        stream = [r for r in stream if r["sequence"] not in (5,)]
        result = records.correlate(stream, [view(990_000, 0, []), view(1_050_000, 1, [[1_006_000, False]])])
        self.assertEqual(result["aux1"]["edges"][0]["status"], "indeterminate")
        self.assertTrue(result["aux1"]["traffic_gaps"])

    def test_aux1_overwrite_is_indeterminate(self):
        result = records.correlate(self.stream(), [view(990_000, 0, []),
                                                   view(1_050_000, 20, [[1_006_000, False]], overwritten=4)])
        self.assertEqual(result["aux1"]["edges"][0]["status"], "indeterminate")
        self.assertTrue(result["aux1"]["lost_edge_windows"])

    def test_edge_without_a_write_is_unpaired(self):
        stream = [traffic(n, n, "TX", 800_000 + 20_000 * n, READ) for n in range(1, 30)]
        result = records.correlate(stream, [view(900_000, 0, []), view(1_300_000, 1, [[1_100_000, True]])])
        self.assertEqual(result["aux1"]["edges"][0]["status"], "unpaired")

    def test_edge_before_the_reply_end_is_a_measured_relationship(self):
        result = records.correlate(self.stream(), [view(990_000, 0, []), view(1_050_000, 1, [[1_002_000, False]])])
        edge = result["aux1"]["edges"][0]
        self.assertEqual((edge["status"], edge["relationship"]), ("measured", "before reply end"))

    def test_output_interval_brackets_the_transition(self):
        old = {"address": 1, "operation_id": 1, "request_start_us": 100, "reply_end_us": 200, "uncertainty_us": 0, "raw_inputs": 0}
        new = {"address": 1, "operation_id": 2, "request_start_us": 900, "reply_end_us": 1000, "uncertainty_us": 5, "raw_inputs": 8}
        result = records.output_interval(500, [old, new], 8, True)
        self.assertEqual((result["status"], result["relative_us"]), ("measured", [-400, 505]))
        self.assertEqual(records.output_interval(500, [new], 8, True)["status"], "indeterminate")
        self.assertEqual(records.output_interval(500, [old], 8, True)["status"], "not observed")


class SnapshotBench(FakeBench):
    """Drive 4's homing words behind `home restore` (wired bench 06) and the
    profile stage route. ``restore_words`` overrides what a restore stores."""
    restore_words = None
    stage_words = None

    def _command(self, name, address, host, driver, kwargs):
        if name == "home-restore":
            home = self.settings[self.selected]["home"]
            if host == ("inspect",):
                return dict(ok=True, result="accepted", pending=False, session_ok=True, execution="acknowledged",
                            unsettled=False, error="none")
            words = self.restore_words or self.snapshot_words
            home.update(raw_method=words[0], raw_search_speed=words[1], raw_return_speed=words[2], raw_ramp_time=words[3],
                        raw_offset_words=list(words[4:6]))
            return dict(ok=True, result="accepted", pending=True, operation_id=self._next())
        if name == "motion-profile-stage":
            self.stage_words = [int(t) for t in host[:5]]
            current = self.settings[self.selected]["motion-profile"]
            current[1:6] = self.stage_words
            return dict(ok=True, operation_id=self._next(), state="succeeded", outcome="success")
        return super()._command(name, address, host, driver, kwargs)


HOME_WRITES = [{"drive": 4, "key": "homing.method", "index": None, "candidate": 35},
               {"drive": 4, "key": "homing.search-speed", "index": None, "candidate": 60}]


class SnapshotRouteTest(unittest.TestCase):
    """Wired bench 06: home/velocity words restored from the run's snapshot read."""

    def plan(self, level="06", context=None):
        return json.loads(json.dumps(records.restoration_plan(BASELINE, HOME_WRITES, level, {}, context=context)))

    def test_plan_needs_a_stated_snapshot_and_the_06_image(self):
        plan = self.plan(context={"snapshots": {"home": {}}})
        self.assertTrue(plan["admissible"], plan["refused"])
        self.assertEqual([(s["route"], s["snapshot"], s["words"]) for s in plan["steps"]],
                         [("snapshot", "home", [24, 120, 60, 100, 0, 0])])  # One restore for the whole window.
        self.assertFalse(self.plan()["admissible"])  # No snapshot stated: no route.
        refused = self.plan("05", {"snapshots": {"home": {}}})
        self.assertIn("requires prompt 06", refused["refused"][0]["reason"])
        missing = self.plan(context={"snapshots": {"velocity": {}}})
        self.assertIn("no home snapshot", missing["refused"][0]["reason"])

    def bench(self, words):
        bench = SnapshotBench()
        bench.selected = 4
        home = bench.settings[4]["home"]
        home.update(raw_method=words[0], raw_search_speed=words[1], raw_return_speed=words[2], raw_ramp_time=words[3],
                    raw_offset_words=list(words[4:6]))
        return bench

    def test_restore_writes_the_snapshot_and_reads_it_back(self):
        bench = self.bench([35, 60, 30, 100, 0, 0])
        bench.snapshot_words = [24, 120, 60, 100, 0, 0]
        snapshots = {"home": {"operation_id": 77, "address": 4, "words": [24, 120, 60, 100, 0, 0]}}
        results = records.restore(bench, self.plan(context={"snapshots": {"home": {}}}), snapshots=snapshots)
        self.assertTrue(results[0]["restored"], results[0])
        self.assertEqual(results[0]["ending"], [24, 120, 60, 100, 0, 0])
        restores = [kw["host_args"] for name, kw in bench.calls if name == "home-restore"]
        self.assertEqual(restores[0], ("77",))
        # Equal words: no write at all.
        bench = self.bench([24, 120, 60, 100, 0, 0])
        results = records.restore(bench, self.plan(context={"snapshots": {"home": {}}}), snapshots=snapshots)
        self.assertIn("no write", results[0]["reason"])
        self.assertFalse([n for n, _ in bench.calls if n == "home-restore"])

    def test_readback_mismatch_is_uncertain_and_ends_the_restoration(self):
        bench = self.bench([35, 60, 30, 100, 0, 0])
        bench.snapshot_words = [24, 120, 60, 100, 0, 0]
        bench.restore_words = [24, 120, 60, 100, 0, 1]
        snapshots = {"home": {"operation_id": 77, "address": 4, "words": [24, 120, 60, 100, 0, 0]}}
        results = records.restore(bench, self.plan(context={"snapshots": {"home": {}}}), snapshots=snapshots)
        self.assertTrue(results[0]["uncertain"])
        self.assertIn("readback differs", results[0]["reason"])
        self.assertEqual(len([n for n, _ in bench.calls if n == "home-restore" and _["host_args"] != ("inspect",)]), 1)
        with self.assertRaises(probe.BenchError):  # No snapshot of this drive: never a guessed source.
            records.snapshot_restore(bench, dict(self.plan(context={"snapshots": {"home": {}}})["steps"][0]), {})

    def test_pre_run_words_away_from_the_baseline_are_reported(self):
        bench = self.bench([33, 60, 30, 100, 0, 0])
        bench.snapshot_words = [35, 60, 30, 100, 0, 0]
        snapshots = {"home": {"operation_id": 77, "address": 4, "words": [35, 60, 30, 100, 0, 0]}}
        results = records.restore(bench, self.plan(context={"snapshots": {"home": {}}}), snapshots=snapshots)
        self.assertFalse(results[0]["restored"] or results[0]["uncertain"], results[0])
        self.assertIn("pre-run snapshot words", results[0]["reason"])

    def test_stage_step_compares_the_whole_profile_window(self):
        writes = [{"drive": 4, "key": k, "index": None, "candidate": None}
                  for k in ("profile.acceleration", "profile.target")]
        plan = json.loads(json.dumps(records.restoration_plan(BASELINE, writes, "06", {"4": {"configure": {}}})))
        self.assertEqual(len(plan["steps"]), 1)
        bench = SnapshotBench()
        bench.selected = 4
        bench.settings[4]["motion-profile"][4:6] = [0, 2000]  # Only the target pair differs.
        results = records.restore(bench, plan)
        self.assertTrue(results[0]["restored"], results[0])
        self.assertEqual(bench.stage_words, [100, 100, 60, 0, 5000])
        self.assertEqual(results[0]["window"], [100, 100, 60, 0, 5000])

if __name__ == "__main__":
    unittest.main()
