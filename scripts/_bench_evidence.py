# SPDX-License-Identifier: MIT
"""Private pure validation of supplied motor-console evidence.

These checks consume supplied records and timestamps only. They own no serial
port, clock, session cache, correlation, release policy or command execution.
bench_probe re-exports the shared helpers and Console aliases the validators.
"""
from __future__ import annotations

import re

HOST_BAUDS = (9600, 19200, 38400, 115200)

TYPED_WINDOWS = {"identity": ((0x0000, 4),),
                 "config": ((0x0010, 2), (0x0013, 3), (0x0017, 3), (0x0040, 5), (0x0100, 2)),
                 "state": ((0x0006, 2), (0x0008, 2), (0x000A, 3))}


class BenchError(RuntimeError):
    """Evidence is incomplete, the console failed, or a probe was rejected."""


def check_counts(response: dict, names: tuple[str, ...], command: str) -> None:
    """Require actual nonnegative integer counters, never missing values or bools."""
    for name in names:
        value = response.get(name)
        if type(value) is not int or not 0 <= value <= 0xFFFFFFFFFFFFFFFF:
            raise BenchError(f"{command} reply lacks a valid {name}")


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def invalid_constant(value: str) -> None:
    raise ValueError(f"invalid JSON number: {value}")


def evidence_age_valid(observed_us: int, now_us: int, maximum_age_us: int) -> bool:
    """Zero disables age expiry only; callers still check provenance/binding."""
    return observed_us <= now_us and (maximum_age_us == 0 or now_us - observed_us < maximum_age_us)


def evidence_age_deadline(observed_us: int, maximum_age_us: int) -> int:
    return min(2**64 - 1, observed_us + maximum_age_us) if maximum_age_us else 2**64 - 1


def wire_crc(data: bytes) -> int:
    """Check retained read-only fixture bytes independently of firmware parsing."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def _reply_status(raw: bytes, received: int, address: int, function: int,
                  normal_length: int, echo: tuple[int, int] | None = None) -> tuple[str, int, int]:
    """Check retained reply bytes in the public codec's bounded validation order.

    Callers must validate raw storage/hex shape and captured prefix length
    before this check. Length/address/function/byte-count precede CRC; a valid
    CRC precedes exception and write-echo interpretation.
    """
    if received < 5 or received > normal_length:
        error = 2
    elif raw[0] != address:
        error = 3
    elif raw[1] not in (function, function | 0x80):
        error = 4
    elif received != (5 if raw[1] == (function | 0x80) else normal_length):
        error = 2
    elif function == 3 and raw[1] == function and raw[2] != normal_length - 5:
        error = 5
    elif wire_crc(raw) != 0:
        error = 6
    elif raw[1] == (function | 0x80):
        return "EXCEPTION", raw[2], 10
    elif echo is not None and (int.from_bytes(raw[2:4], "big"), int.from_bytes(raw[4:6], "big")) != echo:
        error = 7
    else:
        return "OK", 0, 0
    return ("CRC_ERROR" if error == 6 else "FRAME_ERROR"), error, error


def operation_id(value) -> bool:
    return type(value) is int and 1 <= value <= 0xFFFFFFFF


def check_probe(item: dict, address: int | None) -> None:
    if (item.get("capture_read", False) is not False or item.get("recovery", False) is not False
            or item.get("read_kind") is not None):
        raise BenchError("probe result kind is inconsistent")
    model = item.get("raw_model")
    if item["ok"] and (type(model) is not int or not 0 <= model <= 65535):
        raise BenchError("successful probe lacks consistent result evidence")
    if item["ok"] or "confidence" in item:
        confidence = ("responder_model_unresolved" if item["ok"] else "responder_only"
                      if item.get("transport") == "FRAME" and item.get("codec") == "EXCEPTION" else "none")
        if (item.get("confidence") != confidence or any(item.get(key) is not False for key in
                ("manufacturer_confirmed", "exact_model_confirmed", "collision_excluded"))):
            raise BenchError("probe confidence overstates checked identity evidence")
    check_read_evidence(item, address, 7)
    if not item["ok"]:
        return
    try:
        frames = []
        for name, size in (("tx_hex", 8), ("rx_hex", 7)):
            value = item.get(name)
            if (type(value) is not str or
                    re.fullmatch(r"[0-9A-Fa-f]{%d}" % (2 * size), value) is None):
                raise ValueError("invalid raw frame")
            frames.append(bytes.fromhex(value))
        tx, rx = frames
        if (tx[:6] != bytes((item["address"], 3, 0, 0, 0, 1)) or wire_crc(tx) != 0 or
                _reply_status(rx, len(rx), item["address"], 3, 7) != ("OK", 0, 0) or
                int.from_bytes(rx[3:5], "big") != model or
                type(item.get("register_start")) is not int or item["register_start"] != 0 or
                type(item.get("register_count")) is not int or item["register_count"] != 1 or
                item.get("identity") != "responder_only"):
            raise ValueError("wrong probe request, reply or decoded model")
    except ValueError as exc:
        raise BenchError("probe raw frame evidence is inconsistent") from exc


def check_read_evidence(item: dict, address: int | None, reply_size: int) -> None:
    actual = item.get("address")
    if (type(actual) is not int or not 1 <= actual <= 247
            or (address is not None and actual != address)):
        raise BenchError("probe terminal address does not match acceptance")
    if not item["ok"]:
        return
    if (item.get("transport") != "FRAME" or item.get("codec") != "OK"
            or type(item.get("detail")) is not int or item["detail"] != 0
            or type(item.get("frame_error")) is not int or item["frame_error"] != 0
            or item.get("outcome") != "success" or item.get("execution_unknown") is not False
            or type(item.get("tx_bytes")) is not int or item["tx_bytes"] != 8
            or type(item.get("rx_bytes")) is not int or item["rx_bytes"] != reply_size
            or type(item.get("duration_us")) is not int
            or not 0 <= item["duration_us"] <= 0xFFFFFFFFFFFFFFFF
            or item.get("timing_valid") is not True
            or item.get("raw_truncated") is not False):
        raise BenchError("successful probe lacks consistent result evidence")
    check_counts(item, ("observed_earliest_us", "observed_latest_us", "delivered_us"), "probe")
    if not item["observed_earliest_us"] <= item["observed_latest_us"] <= item["delivered_us"]:
        raise BenchError("probe observation and delivery bounds are inconsistent")


def check_capture_read(item: dict, address: int | None) -> None:
    if (item.get("capture_read") is not True
            or item.get("recovery", False) is not False
            or item.get("read_kind") is not None
            or type(item.get("register_start")) is not int or item["register_start"] != 0x0130
            or type(item.get("register_count")) is not int or item["register_count"] != 16
            or "raw_model" not in item or item["raw_model"] is not None
            or item.get("identity") != "not_requested"):
        raise BenchError("capture-read result kind/window is inconsistent")
    check_read_evidence(item, address, 37)
    if not item["ok"]:
        return
    try:
        raw = []
        for name, size in (("tx_hex", 8), ("rx_hex", 37)):
            value = item.get(name)
            if not isinstance(value, str) or re.fullmatch(r"[0-9A-Fa-f]{%d}" % (2 * size), value) is None:
                raise ValueError("invalid raw frame")
            frame = bytes.fromhex(value)
            if wire_crc(frame) != 0:
                raise ValueError("invalid CRC")
            raw.append(frame)
        tx, rx = raw
        if tx[:6] != bytes((item["address"], 3, 1, 0x30, 0, 16)) or rx[:3] != bytes((item["address"], 3, 32)):
            raise ValueError("wrong request/reply shape")
    except ValueError as exc:
        raise BenchError("capture-read raw frame evidence is inconsistent") from exc


def check_typed_read(item: dict, kind: str, address: int | None) -> None:
    """Check retained FC03 provenance and decoded fields independently."""
    def require(condition, message):
        if not condition:
            raise BenchError("typed-read " + message)

    def integer(value, maximum=0xFFFFFFFFFFFFFFFF):
        return type(value) is int and 0 <= value <= maximum

    def frame(value, maximum):
        require(isinstance(value, str) and len(value) <= 2 * maximum
                and re.fullmatch(r"(?:[0-9A-Fa-f]{2})*", value) is not None,
                "raw frame is malformed")
        return bytes.fromhex(value)

    require(item.get("read_kind") == kind and item.get("recovery", False) is False
            and item.get("capture_read", False) is False, "result kind is inconsistent")
    require(integer(item.get("address"), 247) and item["address"] >= 1
            and (address is None or item["address"] == address), "address does not match acceptance")
    require(operation_id(item.get("target")) and operation_id(item.get("generation")),
            "target or generation is invalid")
    check_counts(item, ("started_us", "deadline_us", "serviced_us", "completed_steps"), "typed-read")
    require(item["started_us"] < item["deadline_us"] and item["serviced_us"] >= item["started_us"],
            "absolute time budget is inconsistent")
    statuses = {"OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED", "CRC_ERROR", "FRAME_ERROR", "EXCEPTION"}
    require(type(item.get("status")) is str and item["status"] in statuses
            and type(item.get("detail")) is int and -0x80000000 <= item["detail"] <= 0x7FFFFFFF,
            "status evidence is invalid")
    require(item.get("state") == ("succeeded" if item["ok"] else "failed"), "terminal state is inconsistent")
    outcome = item.get("outcome")
    require(type(outcome) is str and (outcome == "success" if item["ok"] else outcome in
            {"reply_error", "transport_error", "cancelled", "deadline", "timing_unqualified"}),
            "terminal outcome is inconsistent")
    serial = item.get("active_serial")
    require(isinstance(serial, dict) and type(serial.get("known")) is bool, "active tuple is unavailable")
    for key, maximum in (("baud", 0xFFFFFFFF), ("data_bits", 255), ("parity", 3), ("stop_bits", 255)):
        require(integer(serial.get(key), maximum), "active tuple is invalid")
    if serial["known"]:
        require(serial["baud"] in HOST_BAUDS and serial["data_bits"] == 8
                and (serial["parity"], serial["stop_bits"]) in ((1, 1), (1, 2), (2, 1), (3, 1)),
                "active tuple is outside reviewed ESS host formats")
    windows = TYPED_WINDOWS[kind]
    steps = item.get("steps")
    complete = item["completed_steps"]
    require(isinstance(steps, list) and complete <= len(windows)
            and complete <= len(steps) <= min(len(windows), complete + 1), "step counts are inconsistent")
    if item["ok"]:
        require(complete == len(windows) and len(steps) == complete and item["status"] == "OK"
                and item["detail"] == 0, "successful operation is incomplete")
    decoded_words = []
    previous_delivery = item["started_us"]
    for index, step in enumerate(steps):
        require(isinstance(step, dict), "step is not an object")
        first, count = windows[index]
        require(type(step.get("step")) is int and step["step"] == index
                and type(step.get("first")) is int and step["first"] == first
                and type(step.get("count")) is int and step["count"] == count, "step/window order is inconsistent")
        require(integer(step.get("event"), 3) and type(step.get("status")) is str
                and step["status"] in statuses and integer(step.get("frame_error"), 10)
                and type(step.get("qualified")) is bool and type(step.get("execution_unknown")) is bool
                and integer(step.get("tx_accepted"), 8)
                and type(step.get("transport_detail")) is int
                and -0x80000000 <= step["transport_detail"] <= 0x7FFFFFFF,
                "step evidence is invalid")
        check_counts(step, ("attempted_us", "earliest_us", "latest_us", "delivered_us", "received_length"), "typed-read step")
        require(type(step.get("detail")) is int and -0x80000000 <= step["detail"] <= 0x7FFFFFFF,
                "step status detail is invalid")
        require(previous_delivery <= step["delivered_us"] <= item["serviced_us"], "step delivery order is inconsistent")
        require(previous_delivery <= step["attempted_us"] <= step["delivered_us"], "step eligibility order is inconsistent")
        if step["qualified"]:
            require(step["event"] == 0 and previous_delivery <= step["earliest_us"]
                    <= step["latest_us"] <= step["delivered_us"], "closure bounds are inconsistent")
        else:
            require(step["earliest_us"] == step["latest_us"] == 0, "unqualified step invents closure bounds")
        tx, rx = frame(step.get("tx"), 8), frame(step.get("rx"), 37)
        require(len(tx) == 8 and wire_crc(tx) == 0
                and tx[:6] == bytes((item["address"], 3, first >> 8, first & 255, 0, count)),
                "retained request does not match reviewed FC03 window")
        require(len(rx) == min(step["received_length"], 37), "captured prefix length is inconsistent")
        if step["event"] == 0:
            expected = _reply_status(rx, step["received_length"], item["address"], 3, 5 + 2 * count)
            require((step["status"], step["detail"], step["frame_error"]) == expected,
                    "codec evidence differs from retained RX")
        else:
            require(not step["qualified"] and step["status"] == "ILLEGAL_VALUE", "control event evidence is inconsistent")
        if index < complete:
            require(step["event"] == 0 and step["status"] == "OK" and step["qualified"]
                    and step["tx_accepted"] == 8 and step["execution_unknown"] is False
                    and step["latest_us"] <= item["deadline_us"]
                    and (index == len(windows) - 1 or not item["ok"] or step["delivered_us"] < item["deadline_us"]),
                    "completed step lacks on-time checked closure")
            decoded_words.extend(int.from_bytes(rx[3 + 2 * word:5 + 2 * word], "big") for word in range(count))
        previous_delivery = step["delivered_us"]
    decoded = item.get(kind)
    if kind == "state":
        check_state_values(item, decoded_words)
    if not item["ok"]:
        require(kind == "state" or (kind in item and decoded is None), "failed operation published a decoded observation")
        require(bool(steps), "failed operation lacks retained terminal evidence")
        last = steps[-1]
        if outcome == "reply_error":
            require(last["event"] == 0 and last["qualified"] and last["latest_us"] <= item["deadline_us"]
                    and last["status"] != "OK" and item["status"] == last["status"]
                    and item["detail"] == last["detail"], "failure outcome differs from last step")
        elif outcome in ("transport_error", "cancelled"):
            require(last["event"] == (1 if outcome == "transport_error" else 2)
                    and item["status"] == last["status"] == "ILLEGAL_VALUE"
                    and item["detail"] == last["detail"], "failure outcome differs from last step")
        elif outcome == "timing_unqualified":
            require(last["event"] == 0 and not last["qualified"] and item["status"] == "ILLEGAL_VALUE",
                    "failure outcome differs from last step")
        else:
            expired_frame = (last["event"] == 0 and last["qualified"]
                             and last["latest_us"] > item["deadline_us"])
            partial_budget = (complete == len(steps) < len(windows)
                              and item["serviced_us"] >= item["deadline_us"])
            expired_control = last["event"] == 3 and last["delivered_us"] >= item["deadline_us"]
            require(item["status"] == "ILLEGAL_VALUE" and (expired_frame or partial_budget or expired_control),
                    "deadline outcome lacks absolute expiry evidence")
        return
    if kind == "state":
        return
    require(isinstance(decoded, dict), "successful operation lacks decoded observation")
    if kind == "identity":
        keys = ("raw_model", "raw_version", "raw_active_node", "raw_dip")
        for key, value in zip(keys, decoded_words):
            require(type(decoded.get(key)) is int and decoded[key] == value, "identity differs from retained RX")
        known = 1 <= decoded_words[2] <= 247
        require(type(decoded.get("active_node_known")) is bool and decoded["active_node_known"] == known
                and type(decoded.get("active_node")) is int and decoded["active_node"] == (decoded_words[2] if known else 0),
                "active node interpretation is inconsistent")
        for key, value in (("model_resolution", 2), ("version_resolution", 3), ("dip_resolution", 4), ("dip_issues", 320)):
            require(type(decoded.get(key)) is int and decoded[key] == value, "identity mapping claims unsupported certainty")
        require(decoded.get("model") == decoded.get("firmware") == "mapping_unresolved"
                and decoded.get("dip") == "mapping_conflict_unresolved", "identity mapping claims unsupported certainty")
        return
    keys = ("direction", "subdivision", "custom_node", "baud", "format", "over_limit_stop", "soft_limit_enable",
            "word_order", "input_polarity", "algorithm", "encoder_resolution")
    values = decoded_words[:9] + decoded_words[13:15]
    raw, known = decoded.get("raw"), decoded.get("known")
    require(isinstance(raw, dict) and isinstance(known, dict), "configuration raw/known evidence is unavailable")
    for key, value in zip(keys, values):
        require(type(raw.get(key)) is int and raw[key] == value, "configuration differs from retained RX")
    require(decoded.get("stored_serial") == {"baud_code": raw["baud"], "format_code": raw["format"],
                                               "activation": "power_cycle_required"}
            and decoded.get("units") == "command_scale_unresolved_encoder_readback_only",
            "stored settings were promoted to active transport or physical units")
    for key, valid in (("direction", raw["direction"] <= 1), ("baud", raw["baud"] <= 3),
                       ("format", raw["format"] <= 3), ("over_limit_stop", raw["over_limit_stop"] <= 1),
                       ("soft_limit_enable", raw["soft_limit_enable"] <= 1), ("word_order", raw["word_order"] <= 1),
                       ("algorithm", raw["algorithm"] in (1, 2))):
        require(type(known.get(key)) is bool and known[key] == valid, "unknown configuration code was normalized")
    for key, value in (("unknown_polarity_bits", raw["input_polarity"] & 0xFFF0),
                       ("subdivision_resolution", 5), ("encoder_resolution", 0 if raw["encoder_resolution"] else 6),
                       ("subdivision_issues", 4), ("custom_node_issues", 64), ("over_limit_stop_issues", 1024),
                       ("soft_limit_issues", 256), ("encoder_scale_source", 3 if raw["encoder_resolution"] else 0)):
        require(type(decoded.get(key)) is int and decoded[key] == value, "configuration provenance is inconsistent")
    inputs = decoded.get("inputs")
    require(isinstance(inputs, list) and len(inputs) == 4, "configuration input evidence is incomplete")
    for index, entry in enumerate(inputs):
        function = decoded_words[9 + index]
        require(isinstance(entry, dict) and type(entry.get("function")) is int and entry["function"] == function
                and type(entry.get("known")) is bool and entry["known"] == (function <= 17)
                and type(entry.get("inverted")) is bool and entry["inverted"] == bool(raw["input_polarity"] & (1 << index))
                and integer(entry.get("wiring"), 2) and entry.get("level_known") is False
                and "level" in entry and entry["level"] is None, "input assignment/wiring/level evidence is inconsistent")


def check_state_values(item: dict, words: list[int]) -> None:
    def require(condition, message):
        if not condition:
            raise BenchError("typed-read state " + message)

    blocks, config = item.get("state_blocks"), item.get("decode_config")
    require(isinstance(blocks, list) and len(blocks) == item["completed_steps"], "partial blocks are inconsistent")
    require(isinstance(config, dict) and type(config.get("operation_id")) is int
            and 0 <= config["operation_id"] <= 0xFFFFFFFF, "decode configuration is invalid")
    for flag, code, choices in (("word_order_known", "word_order", (0, 1)),
                                ("algorithm_known", "algorithm", (1, 2))):
        require(type(config.get(flag)) is bool and type(config.get(code)) is int
                and config[code] in choices and (not config[flag] or config["operation_id"] != 0),
                "decode configuration is invalid")
    for index, block in enumerate(blocks):
        check_state_block(block, index, words, config)


def check_state_block(block: dict, index: int, words: list[int] | None = None,
                       config: dict | None = None) -> None:
    """Check one bounded raw/decoded block for both live and cached reports."""
    def require(condition, message):
        if not condition:
            raise BenchError("typed-read state " + message)

    def raw(key, maximum=0xFFFF):
        value = block.get(key)
        require(type(value) is int and 0 <= value <= maximum, "raw field is invalid: " + key)
        return value

    require(isinstance(block, dict) and type(block.get("block")) is int and block["block"] == index,
            "block identity is inconsistent")
    config_id = raw("config_operation_id", 0xFFFFFFFF)
    if config is not None:
        require(config_id == config["operation_id"], "block configuration is inconsistent")
    if index == 0:
        alarm, motion = (words[:2] if words is not None else (raw("raw_alarm"), raw("raw_motion")))
        expected = dict(raw_alarm=alarm, alarm_known=alarm in (0, 1, 2, 3, 5), raw_motion=motion,
                        unknown_motion_bits=motion & 0xFF80, in_position=bool(motion & 1),
                        homing_complete=bool(motion & 2), running=bool(motion & 4), alarm_flag=bool(motion & 8),
                        released=bool(motion & 16), enabled=not bool(motion & 16),
                        positive_soft_limit=bool(motion & 32), negative_soft_limit=bool(motion & 64))
    elif index == 1:
        inputs, outputs = (words[2:4] if words is not None else (raw("raw_inputs"), raw("raw_outputs")))
        expected = dict(raw_inputs=inputs, raw_outputs=outputs, unknown_input_bits=inputs & 0xFFF0,
                        unknown_output_bits=outputs & 0xFFFC,
                        inputs=[bool(inputs & (1 << bit)) for bit in range(4)],
                        outputs=[bool(outputs & (1 << bit)) for bit in range(2)], levels="logical_valid_not_voltage")
    else:
        position = block.get("position_words")
        require(isinstance(position, list) and len(position) == 2
                and all(type(value) is int and 0 <= value <= 0xFFFF for value in position),
                "position words are invalid")
        first, second, speed = words[4:7] if words is not None else (*position, raw("raw_speed"))
        if config is not None:
            pair_known = config["word_order_known"]
            raw_position = ((first << 16 | second) if config["word_order"] == 0 else (second << 16 | first)) if pair_known else 0
            source = config["algorithm"] if config["algorithm_known"] else 0
        else:
            pair_known = block.get("pair_known")
            source = block.get("position_source")
            require(type(pair_known) is bool and type(source) is int and source in (0, 1, 2),
                    "position interpretation is invalid")
            require(config_id != 0 or (not pair_known and source == 0), "position lacks configuration provenance")
            raw_position = raw("raw_position", 0xFFFFFFFF)
            require(raw_position in ((first << 16 | second), (second << 16 | first)) if pair_known else raw_position == 0,
                    "paired position differs from raw words")
        expected = dict(position_words=[first, second], raw_speed=speed, pair_known=pair_known,
                        raw_position=raw_position, position_source=source,
                        word_order_resolution=0 if pair_known else 9,
                        position_source_resolution=0 if source else 10,
                        position_signed_resolution=7, position_scale_resolution=5,
                        speed_signed_resolution=7, speed_unit_resolution=8,
                        physical_units="unresolved", raw_encoder_counts=None)
    for key, value in expected.items():
        require(key in block and type(block[key]) is type(value) and block[key] == value,
                "decoded field differs from retained RX: " + key)
    if index == 1:
        require(all(type(value) is bool for value in block["inputs"] + block["outputs"]), "logical levels are not booleans")


def check_cached_state(item: dict) -> None:
    def require(condition, message):
        if not condition:
            raise BenchError("cached state " + message)
    check_counts(item, ("now_us", "stale_after_ms", "selected_target", "selected_address", "selected_generation"), "cached state")
    require(item.get("monitoring") in ("enabled", "disabled") and item.get("atomic_snapshot") is False
            and item.get("sample_time") == "drive_internal_age_undocumented", "snapshot claims unsupported certainty")
    blocks = item.get("state_blocks")
    require(isinstance(blocks, list) and len(blocks) == 3, "observation blocks are incomplete")
    for index, block in enumerate(blocks):
        require(isinstance(block, dict) and type(block.get("block")) is int and block["block"] == index, "block identity is invalid")
        for key in ("valid", "current", "fresh", "attempt_known", "last_attempt_ok"):
            require(type(block.get(key)) is bool, "block flags are invalid")
        check_counts(block, ("target", "address", "generation", "operation_id", "last_attempt_us", "last_attempt_target",
                           "last_attempt_address", "last_attempt_generation", "last_attempt_operation_id", "last_success_us",
                           "observed_earliest_us", "observed_latest_us", "delivered_us", "invalidated_us"), "cached state block")
        require(block["last_attempt_us"] <= item["now_us"], "last attempt is in the future")
        require(type(block.get("last_attempt_status")) is str and type(block.get("last_attempt_detail")) is int, "attempt error is unavailable")
        if index == 2 and not block["valid"]:
            require(type(block.get("current_config_operation_id")) is int
                    and 0 <= block["current_config_operation_id"] <= 0xFFFFFFFF
                    and block.get("interpretation_current") is False,
                    "absent feedback interpretation is invalid")
        if not block["valid"]:
            require(not block["current"] and not block["fresh"] and block.get("value") is None
                    and block.get("age_us") is None and block.get("source") == "absent", "absent feedback was fabricated")
            continue
        require(isinstance(block.get("value"), dict) and block["value"].get("block") == index
                and block.get("source") == "checked_rtu_register", "valid observation source is invalid")
        check_state_block(block["value"], index)
        if index == 2:
            config_id = block.get("current_config_operation_id")
            require(type(config_id) is int and 0 <= config_id <= 0xFFFFFFFF
                    and type(block.get("interpretation_current")) is bool
                    and block["interpretation_current"] == (config_id != 0
                        and config_id == block["value"]["config_operation_id"]),
                    "feedback interpretation generation is inconsistent")
        require(0 <= block["observed_earliest_us"] <= block["observed_latest_us"] <= block["delivered_us"] <= item["now_us"]
                and block["last_success_us"] == block["observed_latest_us"], "observation timing bounds are invalid")
        invalidated = block.get("invalidated_us")
        require(type(invalidated) is int and 0 <= invalidated <= item["now_us"], "action invalidation watermark is invalid")
        expected_current = all(block[key] == item["selected_" + key] for key in ("target", "address", "generation"))
        expected_current = expected_current and (invalidated == 0 or block["observed_earliest_us"] > invalidated)
        age = item["now_us"] - block["observed_earliest_us"]
        require(type(block.get("age_us")) is int and block["age_us"] == age and block["current"] == expected_current
                and block["fresh"] == (expected_current and (item["stale_after_ms"] == 0 or age <= item["stale_after_ms"] * 1000)), "observation age or generation is inconsistent")
    if item.get("command") == "health":
        require(item.get("readiness") == "unknown", "observation claims unsupported readiness")
        motion = blocks[0]
        value = motion.get("value")
        expected_alarms = "unknown" if not motion["fresh"] else "present" if value["alarm_flag"] or 1 <= value["raw_alarm"] <= 5 else "clear" if value["raw_alarm"] == 0 else "unknown"
        require(item.get("alarms") == expected_alarms, "alarm presence differs from checked motion evidence")
        require(item.get("state") == ("observed" if motion["fresh"] else "unknown"), "state freshness differs from block evidence")
    require(type(item.get("communication_known")) is bool and item.get("age_source") == "model_probe", "communication evidence is unavailable")
    check_counts(item, ("communication_target", "communication_address", "communication_generation", "communication_earliest_us", "communication_latest_us"), "cached communication")
    if not item["communication_known"]:
        require(item.get("communication_age_us") is None, "absent communication age was fabricated")
    if item["communication_known"]:
        require(type(item.get("communication_age_us")) is int and item["communication_age_us"] == item["now_us"] - item["communication_earliest_us"], "communication age differs from evidence")
        require(0 <= item["communication_earliest_us"] <= item["communication_latest_us"] <= item["now_us"], "communication timing bounds are invalid")
        if item.get("command") == "health" and item.get("communication") not in ("unavailable", "failed"):
            same = all(item["communication_" + key] == item["selected_" + key] for key in ("target", "address", "generation"))
            expired = item["stale_after_ms"] != 0 and item["now_us"] - item["communication_earliest_us"] > item["stale_after_ms"] * 1000
            expected = "unknown" if not same else "stale" if expired else "current"
            require(item.get("communication") == expected, "communication freshness differs from evidence")
