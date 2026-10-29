# SPDX-License-Identifier: MIT
"""Private pure validation of supplied motor-console evidence.

These checks consume supplied records and timestamps only. They own no serial
port, clock, session cache, correlation, release policy or command execution.
bench_probe re-exports the shared helpers and Console aliases the validators.
"""
from __future__ import annotations

import math
import re
from fractions import Fraction

ACTION_KINDS = {"enable": "enable", "motor-release": "release", "alarm-clear": "clear_alarm", "stop": "stop", "position-clear": "clear_position"}

HOME_EVIDENCE_COLUMNS = ["step", "event", "raw_hex", "received_length", "tx_accepted", "tx_complete", "response_confirmed", "qualified", "execution_unknown", "earliest_us", "latest_us", "delivered_us", "transport_detail", "status", "detail", "frame_error"]

HOST_BAUDS = (9600, 19200, 38400, 115200)

TYPED_WINDOWS = {"identity": ((0x0000, 4),),
                 "config": ((0x0010, 2), (0x0013, 3), (0x0017, 3), (0x0040, 5), (0x0100, 2)),
                 "state": ((0x0006, 2), (0x0008, 2), (0x000A, 3)),
                 "velocity_parameters": ((0x001D, 3),),
                 "homing_parameters": ((0x0030, 7),)}


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
    if kind in ("velocity_parameters", "homing_parameters"):
        require(operation_id(item.get("configuration_generation")), "configuration generation is invalid")
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
    if kind == "homing_parameters":
        for key, value in zip(("raw_auxiliary", "raw_method", "raw_search_speed", "raw_return_speed", "raw_ramp_time"), decoded_words):
            require(type(decoded.get(key)) is int and decoded[key] == value,
                    "homing parameters differ from retained RX")
        offset = decoded.get("raw_offset_words")
        require(isinstance(offset, list) and len(offset) == 2 and
                all(type(word) is int for word in offset) and offset == decoded_words[5:7],
                "homing offset words differ from retained RX")
        method = decoded_words[1]
        known_method = method in (*range(1, 15), *range(17, 31), 33, 34, 35)
        support = ("unknown" if not known_method else "implemented" if method in (33, 34, 35) else
                   "unresolved" if method == 18 else "unimplemented")
        for key, value in (("auxiliary_known", decoded_words[0] <= 11), ("method_known", known_method),
                           ("search_speed_in_range", 5 <= decoded_words[2] <= 3000),
                           ("return_speed_in_range", 5 <= decoded_words[3] <= 300),
                           ("ramp_time_in_range", 30 <= decoded_words[4] <= 2000)):
            require(type(decoded.get(key)) is bool and decoded[key] == value,
                    "homing native interpretation is inconsistent")
        require(type(decoded.get("method_support")) is str and decoded["method_support"] == support,
                "homing method support is inconsistent")
        for key, value in (("speed_scale_resolution", 5), ("ramp_scale_resolution", 5),
                           ("offset_word_order_resolution", 9), ("offset_signed_resolution", 7),
                           ("offset_scale_resolution", 5)):
            require(type(decoded.get(key)) is int and decoded[key] == value,
                    "homing parameters claim unsupported interpretation")
        return
    if kind == "velocity_parameters":
        for key, value in zip(("raw_speed", "raw_acceleration_time", "raw_deceleration_time"), decoded_words):
            require(type(decoded.get(key)) is int and decoded[key] == value,
                    "velocity parameters differ from retained RX")
        for key, value in (("acceleration_time_in_range", decoded_words[1] <= 2000),
                           ("deceleration_time_in_range", decoded_words[2] <= 2000)):
            require(type(decoded.get(key)) is bool and decoded[key] == value,
                    "velocity native ramp range is inconsistent")
        for key, value in (("speed_signed_resolution", 7), ("speed_scale_resolution", 5), ("ramp_scale_resolution", 5)):
            require(type(decoded.get(key)) is int and decoded[key] == value,
                    "velocity parameters claim unsupported interpretation")
        return
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


def home_arguments(arguments: tuple[str, ...]) -> tuple[int, int, int, int]:
    """Explicit native words and zero offset; unavailable methods reach API gates."""
    if (not isinstance(arguments, tuple) or len(arguments) != 5 or arguments[4] != "zero" or
            any(type(token) is not str for token in arguments) or
            not re.fullmatch(r"-?[0-9]+", arguments[0]) or
            any(not re.fullmatch(r"[0-9]+", token) for token in arguments[1:4])):
        raise ValueError("home requires method search_native return_native ramp_native zero")
    values = tuple(int(token) for token in arguments[:4])
    if not (-32768 <= values[0] <= 32767 and 5 <= values[1] <= 3000 and
            5 <= values[2] <= 300 and 30 <= values[3] <= 2000):
        raise ValueError("home native words exceed the reviewed ranges")
    return values


def move_arguments(kind: str, arguments: tuple[str, ...]) -> dict:
    """Validate the finite console grammar; conversions stay in the public API."""
    if (kind not in ("relative", "absolute", "angle") or not isinstance(arguments, tuple) or
            not 5 <= len(arguments) <= 16 or any(type(token) is not str or not token or
            any(ord(char) < 33 or ord(char) > 126 for char in token) for token in arguments)):
        raise ValueError("move requires bounded ASCII coordinate and ramp tokens")
    value, unit, frame = arguments[:3]
    if unit not in ("steps", "fullsteps", "counts", "turn", "deg", "rad", "mm") or frame not in ("native", "motor", "load"):
        raise ValueError("move unit or frame is invalid")
    result = dict(value=value, unit=unit, frame=frame, path=2, tie=0, rounding=0,
                  error="0", approximate=False, approximation_error="0", basis=0, setup="write")
    index = 3
    if kind == "angle":
        if unit not in ("turn", "deg", "rad") or frame == "native":
            raise ValueError("wrapped orientation requires a motor or load angular unit")
        if len(arguments) < 7 or arguments[index] not in ("positive", "negative", "shortest") or arguments[index + 1] not in ("reject", "positive", "negative"):
            raise ValueError("angle requires explicit path and half-turn tie")
        result["path"] = ("positive", "negative", "shortest").index(arguments[index])
        result["tie"] = ("reject", "positive", "negative").index(arguments[index + 1])
        index += 2
    result.update(speed=arguments[index], ramp=arguments[index + 1])
    if result["ramp"] != "configured":
        raise ValueError("move requires configured ramp")
    index += 2
    if index < len(arguments) and arguments[index] == "setup":
        if index + 1 >= len(arguments) or arguments[index + 1] not in ("write", "verify", "stored"):
            raise ValueError("move setup requires write, verify or stored")
        result["setup"] = arguments[index + 1]
        index += 2
    if index < len(arguments) and arguments[index] == "basis":
        if kind != "relative" or index + 1 >= len(arguments) or arguments[index + 1] not in ("actual", "commanded", "queued"):
            raise ValueError("relative basis is invalid")
        result["basis"] = ("actual", "commanded", "queued").index(arguments[index + 1])
        index += 2
    if index < len(arguments) and arguments[index] == "round":
        if index + 2 >= len(arguments) or arguments[index + 1] not in ("exact", "nearest", "zero", "floor", "ceil"):
            raise ValueError("move rounding policy is invalid")
        result["rounding"] = ("exact", "nearest", "zero", "floor", "ceil").index(arguments[index + 1])
        result["error"] = arguments[index + 2]
        index += 3
        if index < len(arguments) and arguments[index] == "approx":
            if unit != "rad" or index + 1 >= len(arguments):
                raise ValueError("approximation allowance requires radians")
            result.update(approximate=True, approximation_error=arguments[index + 1])
            index += 2
    if index != len(arguments):
        raise ValueError("move arguments exceed the explicit grammar")
    for key in ("value", "speed", "error", "approximation_error"):
        token = result[key]
        if re.fullmatch(r"[+-]?(?:[0-9]+(?:\.[0-9]+)?|[0-9]+/[0-9]+)", token) is None:
            raise ValueError("move numbers require exact integer, decimal or fraction syntax")
        try:
            number = Fraction(token)
        except (ValueError, ZeroDivisionError) as exc:
            raise ValueError("move exact number is invalid") from exc
        if not -2**63 <= number.numerator < 2**63 or not 1 <= number.denominator < 2**64:
            raise ValueError("move exact number exceeds installed API storage")
        if key == "speed" and (number.denominator != 1 or not 1 <= number <= 65535):
            raise ValueError("native rpm requires a positive integer")
        if key in ("error", "approximation_error") and number < 0:
            raise ValueError("numerical allowances must be nonnegative")
    if result["approximate"] and Fraction(result["approximation_error"]) <= 0:
        raise ValueError("radians require a positive approximation allowance")
    return result


def velocity_arguments(arguments: tuple[str, ...]) -> dict:
    """Exact finite signed-velocity grammar; conversion belongs to the core API."""
    if (not isinstance(arguments, tuple) or not 6 <= len(arguments) <= 11 or
            any(type(token) is not str or not token or any(ord(c) < 33 or ord(c) > 126 for c in token) for token in arguments)):
        raise ValueError("velocity requires bounded ASCII tokens")
    value, unit, frame, duration, ramp, stop = arguments[:6]
    units = {"rpm": (3, 1), "steps/s": (0, 0), "fullsteps/s": (1, 0), "counts/s": (2, 0),
             "turn/s": (3, 0), "turns/s": (3, 0), "deg/s": (4, 0), "rad/s": (5, 0), "mm/s": (6, 0)}
    if unit not in units or frame not in ("native", "motor", "load") or ramp != "configured" or stop not in ("normal", "fast"):
        raise ValueError("velocity unit, frame, ramp or stop is invalid")
    if re.fullmatch(r"[0-9]+", duration) is None or not 1 <= int(duration) <= 1000:
        raise ValueError("velocity duration must be within 1..1000 ms")
    result = dict(value=value, unit=units[unit], frame=("native", "motor", "load").index(frame), duration_us=int(duration) * 1000,
                  stop=stop, rounding=0, error="0", approximate=False, approximation_error="0")
    index = 6
    if index < len(arguments) and arguments[index] == "round":
        if index + 2 >= len(arguments) or arguments[index + 1] not in ("exact", "nearest", "zero", "floor", "ceil"):
            raise ValueError("velocity rounding policy is invalid")
        result.update(rounding=("exact", "nearest", "zero", "floor", "ceil").index(arguments[index + 1]), error=arguments[index + 2]); index += 3
        if index < len(arguments) and arguments[index] == "approx":
            if unit != "rad/s" or index + 1 >= len(arguments): raise ValueError("velocity approximation requires radians")
            result.update(approximate=True, approximation_error=arguments[index + 1]); index += 2
    if index != len(arguments): raise ValueError("velocity arguments exceed explicit grammar")
    for key in ("value", "error", "approximation_error"):
        token = result[key]
        if re.fullmatch(r"[+-]?(?:[0-9]+(?:\.[0-9]+)?|[0-9]+/[0-9]+)", token) is None:
            raise ValueError("velocity numbers require exact syntax")
        try: number = Fraction(token)
        except (ValueError, ZeroDivisionError) as exc: raise ValueError("velocity number is invalid") from exc
        if not -2**63 <= number.numerator < 2**63 or not 1 <= number.denominator < 2**64:
            raise ValueError("velocity number exceeds installed API storage")
        if key != "value" and number < 0: raise ValueError("velocity error allowance must be nonnegative")
    if result["approximate"] and Fraction(result["approximation_error"]) <= 0:
        raise ValueError("velocity radians require positive approximation allowance")
    return result


def check_action(item: dict, command: str, address: int | None, policy: str | None) -> None:
    """Separate a confirmed write acknowledgement from a later drive report."""
    def require(condition, message):
        if not condition:
            raise BenchError("action " + message)

    def integer(value, maximum=0xFFFFFFFFFFFFFFFF):
        return type(value) is int and 0 <= value <= maximum


    empty_evidence = dict(step=0, event=0, raw_hex="", received_length=0, tx_accepted=0,
        tx_complete=False, response_confirmed=False, qualified=False, execution_unknown=False,
        earliest_us=0, latest_us=0, delivered_us=0, transport_detail=0, status="OK", detail=0, frame_error=0)

    def empty(entry):
        return all(entry.get(key) == value for key, value in empty_evidence.items())

    require(item.get("action_kind") == ACTION_KINDS.get(command)
            and item.get("stop_policy") == policy and (command != "stop" or policy in ("normal", "fast"))
            and item.get("read_kind") is None and item.get("capture_read", False) is False
            and item.get("recovery", False) is False, "kind or policy does not match request")
    require(integer(item.get("address"), 247) and item["address"] >= 1
            and (address is None or item["address"] == address), "address does not match acceptance")
    require(operation_id(item.get("target")) and operation_id(item.get("generation")),
            "target or generation is invalid")
    check_counts(item, ("started_us", "deadline_us", "serviced_us", "polls"), "action")
    require(item["started_us"] < item["deadline_us"] and item["serviced_us"] >= item["started_us"]
            and item["polls"] <= 64, "time budget or observation count is inconsistent")
    require(item.get("state") == ("succeeded" if item["ok"] else "failed"), "state is inconsistent")
    require(item.get("outcome") in ({"observed"} if item["ok"] else
            {"reply_error", "transport_error", "cancelled", "deadline", "timing_unqualified",
             "unconfirmed_response", "observation_limit"}), "outcome is inconsistent")
    require(item.get("execution") in ("not_transmitted", "acknowledged", "rejected", "unknown"),
            "execution evidence is invalid")
    require(item.get("completion") == ("observed" if item["ok"] else "not_observed"),
            "reported completion is inconsistent")
    require(type(item.get("observation_known")) is bool, "observation validity is missing")
    require(type(item.get("interrupted_by_stop")) is bool, "stop interruption evidence is missing")
    if command == "position-clear":
        require(type(item.get("device_position")) is int and item["device_position"] == 0 and item.get("position_clear_qualified") is True,
                "device position clear was not zero-only and qualified")
        require(item["observation_known"] or item.get("raw_position") is None, "absent clear observation publishes a counter")
    evidence = {}
    for name in ("write_evidence", "last_observation", "failure_evidence"):
        entry = item.get(name)
        require(isinstance(entry, dict), "missing " + name)
        for key, limit in (("received_length", 0xFFFFFFFF), ("tx_accepted", 8), ("event", 3),
                           ("step", 64), ("frame_error", 255), ("earliest_us", 0xFFFFFFFFFFFFFFFF),
                           ("latest_us", 0xFFFFFFFFFFFFFFFF), ("delivered_us", 0xFFFFFFFFFFFFFFFF)):
            require(integer(entry.get(key), limit), name + " has invalid " + key)
        for key in ("tx_complete", "response_confirmed", "qualified", "execution_unknown"):
            require(type(entry.get(key)) is bool, name + " has invalid " + key)
        require(all(type(entry.get(key)) is int and -0x80000000 <= entry[key] <= 0x7FFFFFFF
                    for key in ("detail", "transport_detail"))
                and entry.get("status") in {"OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED",
                    "CRC_ERROR", "FRAME_ERROR", "EXCEPTION"}, name + " has invalid status")
        raw = entry.get("raw_hex")
        require(isinstance(raw, str) and re.fullmatch(r"(?:[0-9a-fA-F]{2}){0,19}", raw) is not None,
                name + " raw frame is malformed")
        raw = bytes.fromhex(raw)
        require(len(raw) in (min(entry["received_length"], 9), min(entry["received_length"], 19)), name + " length is inconsistent")
        if entry["status"] == "OK":
            require(entry["detail"] == entry["frame_error"] == 0, name + " successful codec retains an error")
        elif entry["status"] == "EXCEPTION":
            require(entry["event"] == 0 and len(raw) == 5
                    and raw[:2] == bytes((item["address"], 0x86 if entry["step"] == 0 else 0x83))
                    and wire_crc(raw) == 0 and entry["detail"] == raw[2] and entry["frame_error"] == 10,
                    name + " exception differs from checked frame")
        require(not entry["tx_complete"] or entry["tx_accepted"] == 8, name + " physical TX lacks full acceptance")
        if not empty(entry):
            require(item["started_us"] <= entry["delivered_us"] <= item["serviced_us"],
                    name + " delivery is outside the action lifetime")
            require(entry["event"] != 0 or entry["tx_complete"], name + " frame precedes physical TX completion")
        require(entry["event"] == 0 or not entry["response_confirmed"],
                name + " local event claims a confirmed response")
        if entry["qualified"]:
            require(entry["event"] == 0 and item["started_us"] <= entry["earliest_us"] <= entry["latest_us"]
                    <= entry["delivered_us"] <= item["serviced_us"], name + " timing is inconsistent")
        else:
            require(entry["earliest_us"] == entry["latest_us"] == 0, name + " unqualified bounds are published")
        evidence[name] = (entry, raw)
    write, raw = evidence["write_evidence"]
    require(write["step"] == 0 and not empty(write), "write token or evidence is inconsistent")
    reg, value = {"enable": (0x2D, 0x12), "motor-release": (0x2D, 0x11),
                  "alarm-clear": (0x2D, 0x21), "position-clear": (0x2D, 0x31),
                  "stop": (0x27, 0x100 if policy == "normal" else 0x200)}[command]
    unconfirmed_write = (item["execution"] == "unknown" and
        write["event"] == 0 and write["qualified"] and not write["response_confirmed"] and
        write["tx_complete"] and write["tx_accepted"] == 8 and write["status"] == "OK" and
        len(raw) == 8 and raw[:6] == bytes((item["address"], 6, reg >> 8, reg & 255, value >> 8, value & 255)) and
        wire_crc(raw) == 0 and write["latest_us"] <= item["deadline_us"])
    observable_write = item["execution"] == "acknowledged" or unconfirmed_write
    if item["execution"] in ("acknowledged", "rejected"):
        require(write["qualified"] and write["response_confirmed"] and write["tx_complete"]
                and write["event"] == 0 and wire_crc(raw) == 0, "acknowledgement lacks confirmed drive response")
        if item["execution"] == "acknowledged":
            reg, value = {"enable": (0x2D, 0x12), "motor-release": (0x2D, 0x11),
                          "alarm-clear": (0x2D, 0x21), "position-clear": (0x2D, 0x31), "stop": (0x27, 0x100 if policy == "normal" else 0x200)}[command]
            require(raw[:6] == bytes((item["address"], 6, reg >> 8, reg & 255, value >> 8, value & 255))
                    and len(raw) == 8 and write["status"] == "OK" and write["frame_error"] == 0,
                    "write echo differs from requested action")
        else:
            require(len(raw) == 5 and raw[:2] == bytes((item["address"], 0x86))
                    and 1 <= raw[2] <= 7 and write["detail"] == raw[2]
                    and write["status"] == "EXCEPTION" and write["frame_error"] == 10,
                    "device rejection lacks checked documented exception")
    elif item["execution"] == "not_transmitted":
        require(write["tx_accepted"] == 0 and not write["execution_unknown"], "non-transmission contradicts TX evidence")
    else:
        require(write["tx_accepted"] > 0 or write["execution_unknown"], "unknown execution lacks transmission uncertainty")
        require(not (write["qualified"] and write["response_confirmed"]
                     and (write["status"] == "OK" or (write["status"] == "EXCEPTION" and 1 <= write["detail"] <= 7))),
                "unknown execution contradicts a checked acknowledgement or documented rejection")
    if item["observation_known"]:
        obs, raw = evidence["last_observation"]
        require(observable_write and item["polls"] >= 1
                and obs["step"] == item["polls"] and obs["event"] == 0 and obs["qualified"]
                and obs["response_confirmed"] and obs["tx_complete"] and obs["status"] == "OK"
                and obs["frame_error"] == 0 and obs["received_length"] == 9 and wire_crc(raw) == 0
                and raw[:3] == bytes((item["address"], 3, 4)), "observation lacks checked FC03 evidence")
        require(write["delivered_us"] < obs["earliest_us"] <= obs["latest_us"] <= item["deadline_us"],
                "observation precedes acknowledgement or exceeds the deadline")
        alarm, motion = int.from_bytes(raw[3:5], "big"), int.from_bytes(raw[5:7], "big")
        if command == "position-clear":
            require(item.get("raw_alarm") is None and item.get("raw_motion") is None and
                    type(item.get("raw_position")) is int and item["raw_position"] == (alarm << 16 | motion),
                    "clear-position observation differs from retained RX")
            observed = item["raw_position"] == 0
        else:
            require(type(item.get("raw_alarm")) is int and item["raw_alarm"] == alarm
                and type(item.get("raw_motion")) is int and item["raw_motion"] == motion,
                "decoded report differs from retained RX")
            observed = {"enable": not bool(motion & 16), "motor-release": bool(motion & 16),
                    "alarm-clear": alarm == 0 and not bool(motion & 8), "stop": not bool(motion & 4)}[command]
        require(observed == item["ok"], "status differs from requested reported completion")
    else:
        require(item.get("raw_alarm") is None and item.get("raw_motion") is None and not item["ok"]
                and item["polls"] == 0 and empty(evidence["last_observation"][0]),
                "unknown observation publishes completion or decoded values")
    failure, _ = evidence["failure_evidence"]
    if item["ok"]:
        require(empty(failure), "successful action retains failure evidence")
        require(evidence["last_observation"][0]["delivered_us"] == item["serviced_us"],
                "successful action service differs from final observation")
    else:
        require(not empty(failure) and failure["delivered_us"] == item["serviced_us"],
                "failure lacks terminal event evidence")
        require(failure["step"] in (item["polls"], item["polls"] + 1),
                "failure token skips an observation")
        if failure["step"] == 0:
            require(failure == write, "failed write differs from retained write evidence")
        else:
            require(observable_write, "observation failure precedes a checked write")
            previous = evidence["last_observation"][0] if item["observation_known"] else write
            if failure["step"] == item["polls"]:
                require(failure == previous, "failed observation differs from retained observation")
            else:
                require(failure["delivered_us"] >= previous["delivered_us"]
                        and (not failure["qualified"] or failure["earliest_us"] > previous["delivered_us"]),
                        "failure precedes the previous transaction")
        if failure["event"] != 0:
            expected = {1: "transport_error", 2: "cancelled", 3: "deadline"}[failure["event"]]
        elif not failure["qualified"]:
            expected = "timing_unqualified"
        elif failure["latest_us"] > item["deadline_us"]:
            expected = "deadline"
        elif failure["status"] != "OK":
            expected = "reply_error"
        elif not failure["response_confirmed"] and not (failure["step"] == 0 and unconfirmed_write):
            expected = "unconfirmed_response"
        else:
            require(item["outcome"] in ("deadline", "observation_limit")
                    and (failure["step"] > 0 or item["outcome"] == "deadline"),
                    "checked response does not establish the failure")
            expected = item["outcome"]
        require(item["outcome"] == expected, "failure outcome contradicts terminal evidence")
        if expected == "deadline":
            require(item["serviced_us"] >= item["deadline_us"], "deadline failure precedes deadline")


def check_move_relation(item: dict, original_id: int | None = None, *, replacement: bool = False) -> None:
    """Correlate retained original/replacement results without inventing completion."""
    relation = item.get("relation")
    if (not isinstance(relation, dict) or
            not operation_id(relation.get("original_operation_id")) or
            not operation_id(relation.get("replacement_operation_id")) or
            relation["original_operation_id"] == relation["replacement_operation_id"] or
            type(relation.get("observation_superseded")) is not bool or
            not operation_id(item.get("operation_id")) or
            item["operation_id"] != relation["replacement_operation_id" if replacement else "original_operation_id"] or
            (original_id is not None and relation["original_operation_id"] != original_id)):
        raise BenchError("position interruption relation does not match the retained operation")


def check_position_interrupt(item: dict, address: int | None, original_id: int | None = None) -> None:
    """Check the single stored-profile FC06 retrigger; no observation proves arrival."""
    def require(condition, message):
        if not condition:
            raise BenchError("position interruption " + message)

    def integer(value, low=0, high=0xFFFFFFFFFFFFFFFF):
        return type(value) is int and low <= value <= high

    check_move_relation(item, original_id, replacement=True)
    require(item.get("position_interrupt") is True and item.get("move_kind") in ("relative", "absolute") and
            item.get("action_kind") is None and item.get("read_kind") is None and
            item.get("capture_read") is False and item.get("recovery") is False, "kind differs")
    require(integer(item.get("address"), 1, 247) and (address is None or item["address"] == address) and
            all(operation_id(item.get(key)) for key in ("target", "generation", "configuration_generation")),
            "target or configuration identity differs")
    check_counts(item, ("started_us", "deadline_us", "serviced_us"), "position interruption")
    require(item["started_us"] < item["deadline_us"] and item["serviced_us"] >= item["started_us"],
            "lifetime is inconsistent")
    require(item.get("ok") is False and item.get("completion") == "not_observed" and
            item.get("setup") == "stored" and item.get("setup_execution") == "not_transmitted" and
            all(item.get(key) is False for key in ("staging_applied", "observation_known", "running_observed",
                                                    "endpoint_known", "displacement_known")),
            "stored retrigger claims setup or observed completion")
    require(not any(key in item for key in ("polls", "last_observation", "activity_evidence", "staging_evidence",
                "verification_evidence", "requested", "reference", "prerequisites", "raw_alarm", "raw_motion",
                "endpoint_native", "displacement_native", "effective_native", "observed_position", "observed_speed",
                "position_confirmed", "position_feedback_matches_command", "position_match_evidence", "raw_position")),
            "native retrigger includes observed-move evidence")
    require(type(item.get("interrupted_by_stop")) is bool and type(item.get("uncertain")) is bool and
            integer(item.get("trigger_step"), 1, 1), "state evidence is missing")
    stored = item.get("stored")
    require(isinstance(stored, dict), "stored qualification is missing")
    require(all(type(stored.get(key)) is int and stored[key] == item[key]
                for key in ("target", "address", "generation", "configuration_generation")) and
            stored.get("original_operation_id") == item["relation"]["original_operation_id"] and
            operation_id(stored.get("original_operation_id")) and
            type(stored.get("relative")) is bool and stored["relative"] == (item["move_kind"] == "relative") and
            stored.get("qualified") is True and integer(stored.get("subdivision"), 400, 51200) and
            integer(stored.get("word_order"), 0, 1), "stored identity or qualification differs")
    require(integer(stored.get("qualified_us")) and integer(stored.get("maximum_age_us")) and
            evidence_age_valid(stored["qualified_us"], item["started_us"], stored["maximum_age_us"]),
            "qualification was not fresh at admission")
    require(integer(stored.get("start_speed"), 0, 3000) and
            all(integer(stored.get(key), 0, 2000) for key in ("acceleration_time", "deceleration_time")) and
            integer(stored.get("speed"), 1, 3000) and integer(stored.get("target_bits"), 0, 0xFFFFFFFF) and
            type(item.get("native_rpm")) is int and item["native_rpm"] == stored["speed"],
            "stored profile is invalid")
    pair = [stored["target_bits"] >> 16, stored["target_bits"] & 0xFFFF]
    if stored["word_order"] == 1:
        pair.reverse()
    expected_words = [stored["acceleration_time"], stored["deceleration_time"], stored["speed"], *pair]
    require(isinstance(item.get("staging_words"), list) and len(item["staging_words"]) == 5 and
            all(integer(word, 0, 0xFFFF) for word in item["staging_words"]) and item["staging_words"] == expected_words,
            "candidate differs from qualified stored parameters")
    cap = min(item["deadline_us"], evidence_age_deadline(stored["qualified_us"], stored["maximum_age_us"]))
    empty = dict(step=0, event=0, raw_hex="", received_length=0, tx_accepted=0, tx_complete=False,
        response_confirmed=False, qualified=False, execution_unknown=False, earliest_us=0, latest_us=0,
        delivered_us=0, transport_detail=0, status="OK", detail=0, frame_error=0)
    evidence = {}
    echo = (0x27, 9 if stored["relative"] else 13)
    for name in ("trigger_evidence", "failure_evidence"):
        entry = item.get(name)
        require(isinstance(entry, dict), "missing " + name)
        for key, limit in (("step", 1), ("event", 3), ("received_length", 0xFFFFFFFF), ("tx_accepted", 8),
                           ("frame_error", 255), ("earliest_us", 0xFFFFFFFFFFFFFFFF),
                           ("latest_us", 0xFFFFFFFFFFFFFFFF), ("delivered_us", 0xFFFFFFFFFFFFFFFF)):
            require(integer(entry.get(key), 0, limit), name + " has invalid " + key)
        require(all(type(entry.get(key)) is bool for key in
                    ("tx_complete", "response_confirmed", "qualified", "execution_unknown")) and
                all(integer(entry.get(key), -0x80000000, 0x7FFFFFFF) for key in ("detail", "transport_detail")) and
                entry.get("status") in ("OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED", "CRC_ERROR", "FRAME_ERROR", "EXCEPTION"),
                name + " has invalid transport or codec evidence")
        require(isinstance(entry.get("raw_hex"), str) and
                re.fullmatch(r"(?:[0-9a-fA-F]{2}){0,19}", entry["raw_hex"]) is not None,
                name + " raw frame is malformed")
        raw = bytes.fromhex(entry["raw_hex"])
        require(len(raw) == min(entry["received_length"], 19), name + " copied frame length differs")
        is_empty = all(entry.get(key) == value for key, value in empty.items())
        if is_empty:
            evidence[name] = (entry, True)
            continue
        require(entry["step"] == 1 and item["started_us"] <= entry["delivered_us"] <= item["serviced_us"] and
                (not entry["tx_complete"] or entry["tx_accepted"] == 8), name + " lifetime or trigger token differs")
        if entry["event"] == 0:
            require(entry["tx_complete"] and entry["tx_accepted"] == 8, name + " frame precedes full physical TX")
            require((entry["status"], entry["detail"], entry["frame_error"]) ==
                    _reply_status(raw, entry["received_length"], item["address"], 6, 8, echo),
                    name + " codec result differs from retained FC06 reply")
        else:
            require(not entry["qualified"] and not entry["response_confirmed"] and entry["frame_error"] == 0 and
                    entry["status"] == "ILLEGAL_VALUE" and
                    entry["detail"] == {1: 17, 2: 18, 3: 6 if cap < item["deadline_us"] else 16}[entry["event"]],
                    name + " local event claims incorrect response or outcome")
        if entry["qualified"]:
            require(item["started_us"] <= entry["earliest_us"] <= entry["latest_us"] <= entry["delivered_us"],
                    name + " qualified bounds differ")
        else:
            require(entry["earliest_us"] == entry["latest_us"] == 0, name + " unqualified bounds are published")
        if entry["event"] == 3:
            require(entry["delivered_us"] >= cap, name + " deadline precedes its immutable budget")
        evidence[name] = (entry, False)
    trigger, trigger_empty = evidence["trigger_evidence"]
    failure, failure_empty = evidence["failure_evidence"]
    require(not trigger_empty and trigger["delivered_us"] == item["serviced_us"], "trigger lacks final event evidence")
    if trigger["tx_accepted"] == 0 and not trigger["execution_unknown"]:
        execution = "not_transmitted"
    elif trigger["event"] == 0 and trigger["qualified"] and trigger["response_confirmed"] and trigger["status"] == "OK":
        execution = "acknowledged"
    elif trigger["event"] == 0 and trigger["qualified"] and trigger["response_confirmed"] and trigger["status"] == "EXCEPTION" and 1 <= trigger["detail"] <= 7:
        execution = "rejected"
    else:
        execution = "unknown"
    require(item.get("execution") == execution, "execution differs from transmission and checked response")
    if trigger["event"] != 0:
        outcome = {1: "transport_error", 2: "cancelled", 3: "deadline"}[trigger["event"]]
        status, detail = trigger["status"], trigger["detail"]
    elif not trigger["qualified"]:
        outcome, status, detail = "timing_unqualified", "ILLEGAL_VALUE", 19
    elif trigger["latest_us"] > cap:
        outcome, status, detail = "deadline", "ILLEGAL_VALUE", 6 if cap < item["deadline_us"] else 16
    elif trigger["status"] != "OK":
        outcome, status, detail = "reply_error", trigger["status"], trigger["detail"]
    elif not trigger["response_confirmed"]:
        outcome, status, detail = "unconfirmed_response", "ILLEGAL_VALUE", 20
    else:
        outcome, status, detail = "acknowledged", "OK", 0
    succeeded = outcome == "acknowledged"
    require(item.get("outcome") == outcome and item.get("state") == ("succeeded" if succeeded else "failed") and
            item.get("status") == status and type(item.get("detail")) is int and item["detail"] == detail,
            "outcome differs from terminal trigger evidence")
    require((failure_empty if succeeded else failure == trigger), "failure evidence differs from terminal trigger")
    require(item["uncertain"] == (not succeeded and (trigger["tx_accepted"] > 0 or trigger["execution_unknown"])),
            "possible effects were discarded or invented")
    require(item["relation"]["observation_superseded"] or
            (outcome in ("cancelled", "deadline") and execution == "not_transmitted"),
            "unsuperseded original permits only no-transmission cancellation or expiry")


def check_move(item: dict, address: int | None, arguments: tuple[str, ...] | None, kind: str = "relative") -> None:
    """Check staged/triggered execution and fresh running-to-arrival evidence."""
    def require(condition, text):
        if not condition:
            raise BenchError("move " + text)
    def integer(value, low=0, high=0xFFFFFFFFFFFFFFFF):
        return type(value) is int and low <= value <= high
    require(item.get("move_kind") == kind and item.get("action_kind") is None and
            item.get("read_kind") is None and item.get("capture_read") is False and item.get("recovery") is False,
            "kind is inconsistent")
    require(integer(item.get("address"), 1, 247) and (address is None or item["address"] == address) and
            all(operation_id(item.get(key)) for key in ("target", "generation", "configuration_generation")),
            "target binding is inconsistent")
    require(item.get("state") == ("succeeded" if item["ok"] else "failed") and
            item.get("completion") == ("observed" if item["ok"] else "not_observed") and
            item.get("outcome") in ({"observed"} if item["ok"] else {"reply_error", "transport_error", "cancelled", "deadline",
                "timing_unqualified", "unconfirmed_response", "observation_limit"}), "terminal state is inconsistent")
    require(all(integer(item.get(key)) for key in ("started_us", "deadline_us", "serviced_us", "polls")) and
            item["started_us"] < item["deadline_us"] and item["serviced_us"] >= item["started_us"] and item["polls"] <= 64,
            "time or observation bounds are inconsistent")
    require(item.get("status") in {"OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED", "CRC_ERROR", "FRAME_ERROR", "EXCEPTION"} and
            integer(item.get("detail"), -2**31, 2**31 - 1) and (not item["ok"] or (item["status"] == "OK" and item["detail"] == 0)),
            "status is inconsistent")
    for key in ("staging_applied", "uncertain", "running_observed", "observation_known", "interrupted_by_stop",
                "endpoint_known", "zero_displacement", "exact_arithmetic"):
        require(type(item.get(key)) is bool, "missing " + key)
    prerequisites = item.get("prerequisites")
    require(isinstance(prerequisites, dict) and all(prerequisites.get(key) == item[key] for key in
            ("target", "generation", "configuration_generation")), "qualification binding differs")
    displacement_known = item.get("displacement_known", True)
    require(type(displacement_known) is bool, "invalid displacement validity")
    require(integer(item.get("effective_native"), -2**31, 2**31 - 1) and
            integer(item.get("displacement_native"), -2**63, 2**63 - 1) and
            (kind != "relative" or item["displacement_native"] == item["effective_native"]) and item["zero_displacement"] is False and
            integer(item.get("endpoint_native"), -2**63, 2**63 - 1), "effective target is invalid")
    if not displacement_known:
        require(kind == "absolute" and item.get("displacement_known") is False and item["displacement_native"] == 0 and
                item["effective_native"] == item["endpoint_native"] and item["endpoint_known"],
                "absolute target publishes an inferred displacement")
    else:
        require(displacement_known and item["displacement_native"] != 0, "displacement is absent or zero")
    require(integer(item.get("native_rpm"), 1, 3000) and item.get("ramp") == "configured", "speed or ramp is invalid")
    setup = item.get("setup", "write")
    trigger_step = item.get("trigger_step", 1)
    setup_offset, setup_count = item.get("setup_offset", 0), item.get("setup_count", 5)
    require(setup in ("write", "verify", "stored") and type(trigger_step) is int and
            trigger_step == (2 if setup == "verify" else 1) and
            integer(setup_offset, 0, 3) and integer(setup_count, 0, 5) and
            (setup_offset, setup_count) in ((0, 0), (0, 1), (1, 1), (2, 1), (3, 2), (0, 5)), "setup policy is invalid")
    require(setup != "write" or (setup_offset, setup_count) == (0, 5), "full write omitted settings")
    require(setup != "stored" or (setup_offset, setup_count) == (0, 0), "stored policy writes settings")
    verification_known = item.get("verification_known", False)
    require(type(verification_known) is bool and (setup == "verify" or not verification_known), "verification validity differs")
    setup_size = 8 if setup_count == 1 else 13 if setup_count == 2 else 19
    words = item.get("staging_words")
    require(isinstance(words, list) and len(words) == 5 and all(integer(word, 0, 65535) for word in words) and
            words[0] <= 2000 and words[1] <= 2000 and words[2] == item["native_rpm"], "staged parameter words are inconsistent")
    encoded = item["effective_native"] & 0xFFFFFFFF
    require(words[3:] in ([encoded >> 16, encoded & 65535], [encoded & 65535, encoded >> 16]), "staged target differs from effective target")
    require(all(prerequisites.get(key) is True for key in ("command_units_verified", "configured_ramp_verified",
            "serial_inputs_permit", "readiness_qualified", "word_order_known", "start_speed_known")) and
            type(prerequisites.get("negative_encoding_verified")) is bool and
            (item["effective_native"] >= 0 or prerequisites["negative_encoding_verified"]) and
            (kind != "relative" or prerequisites.get("relative_basis_verified") is True), "qualification is missing")
    require(integer(prerequisites.get("observed_us")) and integer(prerequisites.get("maximum_age_us")) and
            evidence_age_valid(prerequisites["observed_us"], item["started_us"], prerequisites["maximum_age_us"]) and
            prerequisites.get("raw_alarm") == 0 and integer(prerequisites.get("raw_motion"), 0, 65535) and
            not prerequisites["raw_motion"] & 0x7C and integer(prerequisites.get("word_order"), 0, 1) and
            integer(prerequisites.get("start_speed"), 0, item["native_rpm"]), "readiness or configured speed evidence is inconsistent")
    write_deadline = min(item["deadline_us"],
                         evidence_age_deadline(prerequisites["observed_us"], prerequisites["maximum_age_us"]))
    reference = item.get("reference")
    require(isinstance(reference, dict) and type(reference.get("native_known")) is bool and
            all(integer(reference.get(key), 0, 0xFFFFFFFF) for key in ("target", "generation", "configuration_generation")) and
            integer(reference.get("native_position"), -2**63, 2**63 - 1) and integer(reference.get("basis"), 0, 2) and
            integer(reference.get("source"), 0, 4) and integer(reference.get("observed_us")) and integer(reference.get("maximum_age_us")),
            "reference provenance is invalid")
    if not displacement_known:
        require(reference["native_known"] is False and all(reference[key] == 0 for key in
                ("target", "generation", "configuration_generation", "native_position", "basis", "source", "observed_us", "maximum_age_us")),
                "unreferenced absolute target invents a coordinate reference")
    elif kind != "relative" or reference["native_known"]:
        require(reference["native_known"] and all(reference[key] == item[key] for key in ("target", "generation", "configuration_generation")) and
                reference["basis"] == 0 and reference["source"] != 0 and
                evidence_age_valid(reference["observed_us"], item["started_us"], reference["maximum_age_us"]),
                "reference is stale or mismatched")
        require(item["endpoint_known"] and item["endpoint_native"] - reference["native_position"] == item["displacement_native"] and
                (kind == "relative" or item["effective_native"] == item["endpoint_native"]), "reference endpoint differs")
        write_deadline = min(write_deadline, evidence_age_deadline(reference["observed_us"], reference["maximum_age_us"]))
    expected_words = [encoded >> 16, encoded & 65535]
    if prerequisites["word_order"] == 1: expected_words.reverse()
    require(words[3:] == expected_words, "staged target word order differs from qualification")
    requested = item.get("requested")
    require(isinstance(requested, dict) and integer(requested.get("numerator"), -2**63, 2**63 - 1) and
            integer(requested.get("denominator"), 1, 2**64 - 1) and requested.get("unit") in
            ("steps", "fullsteps", "counts", "turn", "deg", "rad", "mm") and integer(requested.get("frame"), 0, 2) and
            requested.get("relative") is (kind == "relative") and requested.get("wrapped") is (kind == "angle") and
            integer(requested.get("angle_path"), 0, 2) and integer(requested.get("half_turn_tie"), 0, 2) and requested.get("basis") == 0 and integer(requested.get("rounding"), 0, 4),
            "request provenance is invalid")
    require(kind != "angle" or (requested["unit"] in ("turn", "deg", "rad") and requested["frame"] != 0), "wrapped request is not an angular frame")
    if not displacement_known:
        require(requested["relative"] is False and requested["wrapped"] is False and requested["basis"] == 0,
                "unreferenced target must be unwrapped absolute")
    def finite(value):
        return type(value) in (int, float) and math.isfinite(value)
    require(type(requested.get("approximate")) is bool and type(requested.get("rational_radians")) is bool and
            finite(requested.get("maximum_quantization_error")) and requested["maximum_quantization_error"] >= 0 and
            finite(item.get("rounding_error")) and finite(item.get("approximation_error_bound")) and
            item["approximation_error_bound"] >= 0 and
            abs(item["rounding_error"]) <= requested["maximum_quantization_error"], "numerical error provenance is inconsistent")
    require((requested.get("radians") is None) if requested["unit"] != "rad" or requested["rational_radians"] else
            finite(requested.get("radians")), "selected radian input is missing")
    if item["exact_arithmetic"]:
        require(item["approximation_error_bound"] == 0 and item.get("requested_native_approximate") is None and
                (requested.get("maximum_approximation_error") is None or
                 (requested["unit"] == "rad" and requested["approximate"] and
                  (requested["numerator"] == 0 if requested["rational_radians"] else requested["radians"] == 0) and
                  finite(requested.get("maximum_approximation_error")) and requested["maximum_approximation_error"] > 0)),
                "exact preparation contains approximate provenance")
    else:
        require(requested["unit"] == "rad" and requested["approximate"] is True and requested["rounding"] != 0 and
                finite(requested.get("maximum_approximation_error")) and requested["maximum_approximation_error"] > 0 and
                item["approximation_error_bound"] <= requested["maximum_approximation_error"] and
                finite(item.get("requested_native_approximate")),
                "approximate preparation lost numerical provenance")
    if requested["rounding"] == 0:
        require(item["exact_arithmetic"] is True and item["rounding_error"] == 0, "EXACT preparation rounded the target")
    if arguments is not None:
        parsed = move_arguments(kind, arguments)
        def retained_allowance(text, value):
            # The public parser narrows allowances toward zero. Check its
            # retained binary64 interval without repeating target math.
            exact = Fraction(text)
            return (finite(value) and value >= 0 and Fraction(value) <= exact and
                    (exact == Fraction(value) or exact < Fraction(math.nextafter(value, math.inf))))
        require(Fraction(parsed["value"]) == Fraction(requested["numerator"], requested["denominator"]) and
                parsed["unit"] == requested["unit"] and ("native", "motor", "load")[requested["frame"]] == parsed["frame"] and
                Fraction(parsed["speed"]) == item["native_rpm"] and parsed["ramp"] == item["ramp"] and
                parsed["basis"] == requested["basis"] and parsed["setup"] == setup and
                parsed["rounding"] == requested["rounding"] and
                retained_allowance(parsed["error"], requested["maximum_quantization_error"]) and
                parsed["approximate"] == requested["approximate"], "request differs from admitted input")
        if kind == "angle":
            require(parsed["path"] == requested["angle_path"] and parsed["tie"] == requested["half_turn_tie"], "angle policy differs")
        if parsed["approximate"]:
            require(retained_allowance(parsed["approximation_error"], requested["maximum_approximation_error"]), "radian error differs")
    position_feedback = item.get("position_feedback_matches_command", False)
    position_confirmed = item.get("position_confirmed", False)
    require(type(position_feedback) is bool and type(position_confirmed) is bool and
            (not position_confirmed or position_feedback), "invalid position completion policy")
    if position_feedback:
        require(reference["native_known"] and item["endpoint_known"] and
                item["endpoint_native"] != reference["native_position"] and
                -2**31 <= item["endpoint_native"] < 2**31,
                "position completion lacks a changed qualified endpoint")
    observation_size, observation_bytes = (19, 14) if position_feedback else (9, 4)
    evidence = {}
    specs = [("staging_evidence", trigger_step - 1, setup_size), ("trigger_evidence", trigger_step, 8),
             ("activity_evidence", None, 8), ("last_observation", None, 8), ("failure_evidence", None, None)]
    if "setup" in item or setup == "verify": specs.append(("verification_evidence", 0, 8))
    if "position_match_evidence" in item: specs.append(("position_match_evidence", None, 8))
    for name, token, tx_size in specs:
        entry = item.get(name)
        require(isinstance(entry, dict), "missing " + name)
        require(all(integer(entry.get(key), 0, limit) for key, limit in (("event", 3), ("step", 66),
                ("received_length", 0xFFFFFFFF), ("tx_accepted", 19), ("frame_error", 255))), name + " bounds are invalid")
        require(all(integer(entry.get(key)) for key in ("earliest_us", "latest_us", "delivered_us")) and
                all(type(entry.get(key)) is bool for key in ("tx_complete", "response_confirmed", "qualified", "execution_unknown")) and
                all(integer(entry.get(key), -2**31, 2**31 - 1) for key in ("detail", "transport_detail")) and
                entry.get("status") in {"OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED", "CRC_ERROR", "FRAME_ERROR", "EXCEPTION"},
                name + " evidence is malformed")
        raw = entry.get("raw_hex")
        require(isinstance(raw, str) and re.fullmatch(r"(?:[0-9a-fA-F]{2}){0,19}", raw) is not None, name + " raw bytes are invalid")
        raw = bytes.fromhex(raw)
        require(len(raw) in (min(entry["received_length"], 9), min(entry["received_length"], 19)), name + " retained length differs")
        empty = all(entry.get(key) == value for key, value in dict(step=0, event=0, raw_hex="", received_length=0,
            tx_accepted=0, tx_complete=False, response_confirmed=False, qualified=False, execution_unknown=False,
            earliest_us=0, latest_us=0, delivered_us=0, transport_detail=0, status="OK", detail=0, frame_error=0).items())
        if not empty:
            require(token is None or entry["step"] == token, name + " token differs")
            require(item["started_us"] <= entry["delivered_us"] <= item["serviced_us"], name + " is outside operation lifetime")
            size = tx_size or (setup_size if entry["step"] == trigger_step - 1 else 8)
            require(entry["tx_accepted"] <= size and (not entry["tx_complete"] or entry["tx_accepted"] == size), name + " TX evidence differs")
            require(entry["event"] == 0 or not entry["response_confirmed"], name + " local event claims a response")
            if entry["qualified"]:
                require(entry["event"] == 0 and entry["tx_complete"] and item["started_us"] <= entry["earliest_us"] <= entry["latest_us"] <= entry["delivered_us"],
                        name + " timing evidence differs")
            else:
                require(entry["earliest_us"] == entry["latest_us"] == 0, name + " unqualified times are published")
        evidence[name] = (entry, raw, empty)
    def confirmed(name, prefix, size):
        entry, raw, empty = evidence[name]
        require(not empty and entry["event"] == 0 and entry["qualified"] and entry["response_confirmed"] and entry["tx_complete"] and
                entry["status"] == "OK" and entry["detail"] == entry["frame_error"] == 0 and len(raw) == size and
                raw[:len(prefix)] == bytes(prefix) and wire_crc(raw) == 0 and
                (name in ("staging_evidence", "trigger_evidence") or entry["latest_us"] <= item["deadline_us"]),
                name + " lacks a checked response")
        return entry, raw
    if "verification_evidence" in evidence:
        verification, prefix, empty = evidence["verification_evidence"]
        full_hex = item.get("verification_raw_hex")
        require(isinstance(full_hex, str) and re.fullmatch(r"(?:[0-9a-fA-F]{2}){0,15}", full_hex) is not None,
                "verification raw bytes are invalid")
        full = bytes.fromhex(full_hex)
        require(len(full) == min(verification["received_length"], 15) and full[:len(prefix)] == prefix,
                "verification raw retention differs")
        verified_words = item.get("verification_words")
        require(isinstance(verified_words, list) and len(verified_words) == 5 and all(integer(word, 0, 65535) for word in verified_words),
                "verification words are invalid")
        if setup != "verify":
            require(empty and not full and verified_words == [0] * 5, "non-verifying setup claims a read")
        if verification_known:
            require(not empty and verification["event"] == 0 and verification["qualified"] and verification["response_confirmed"] and
                    verification["tx_complete"] and verification["status"] == "OK" and
                    verification["detail"] == verification["frame_error"] == 0 and verification["received_length"] == 15 and
                    full[:3] == bytes((item["address"], 3, 10)) and wire_crc(full) == 0 and
                    verified_words == [int.from_bytes(full[i:i + 2], "big") for i in range(3, 13, 2)],
                    "verification lacks a checked parameter read")
            changed = [i for i in range(5) if words[i] != verified_words[i]]
            selected = ((0, 0) if not changed else (changed[0], 1) if len(changed) == 1 and changed[0] < 3 else
                        (3, 2) if all(i >= 3 for i in changed) else (0, 5))
            require((setup_offset, setup_count) == selected, "write selection differs from checked parameters")
        else:
            require(verified_words == [0] * 5, "unknown verification publishes parameter values")
    if setup_count == 1:
        stage_prefix = (item["address"], 6, 0, 0x21 + setup_offset, words[setup_offset] >> 8, words[setup_offset] & 255)
        stage_function = 6
    else:
        stage_prefix = (item["address"], 16, 0, 0x21 + setup_offset, 0, setup_count)
        stage_function = 16
    for execution, name in (("setup_execution", "staging_evidence"), ("execution", "trigger_evidence")):
        require(item.get(execution) in ("not_transmitted", "acknowledged", "rejected", "unknown"), execution + " is invalid")
        entry, raw, empty = evidence[name]
        if item[execution] == "acknowledged":
            confirmed(name, stage_prefix if execution == "setup_execution" else (item["address"], 6, 0, 0x27, 0, 1 if kind == "relative" else 5), 8)
        elif item[execution] == "rejected":
            require(not empty and entry["qualified"] and entry["response_confirmed"] and entry["tx_complete"] and
                    entry["status"] == "EXCEPTION" and entry["frame_error"] == 10 and len(raw) == 5 and
                    raw[:2] == bytes((item["address"], (stage_function | 0x80) if execution == "setup_execution" else 0x86)) and
                    1 <= raw[2] <= 7 and raw[2] == entry["detail"] and wire_crc(raw) == 0, name + " rejection is unproven")
        elif item[execution] == "not_transmitted":
            require(entry["tx_accepted"] == 0 and not entry["execution_unknown"], name + " contradicts non-transmission")
        else:
            require(not empty and (entry["tx_accepted"] > 0 or entry["execution_unknown"]), name + " uncertainty lacks TX")
            require(not (entry["event"] == 0 and entry["qualified"] and entry["response_confirmed"] and
                (entry["status"] == "OK" or (entry["status"] == "EXCEPTION" and 1 <= entry["detail"] <= 7))),
                name + " unknown execution contradicts checked acknowledgement/rejection")
    require(item["staging_applied"] == (item["setup_execution"] == "acknowledged"), "staging validity differs")
    stage, trigger = evidence["staging_evidence"][0], evidence["trigger_evidence"][0]
    if setup_count == 0:
        require(evidence["staging_evidence"][2] and item["setup_execution"] == "not_transmitted", "skipped staging contains a write")
    if setup == "verify" and not evidence["staging_evidence"][2]:
        require(verification_known and verification["delivered_us"] <= stage["delivered_us"] and
                (not stage["qualified"] or verification["delivered_us"] <= stage["earliest_us"]), "staging precedes verification")
    trigger_raw = evidence["trigger_evidence"][1]
    unconfirmed_trigger = (item["execution"] == "unknown" and
        trigger["event"] == 0 and trigger["qualified"] and not trigger["response_confirmed"] and
        trigger["tx_complete"] and trigger["tx_accepted"] == 8 and trigger["status"] == "OK" and
        trigger["detail"] == trigger["frame_error"] == 0 and len(trigger_raw) == 8 and
        trigger_raw[:6] == bytes((item["address"], 6, 0, 0x27, 0, 1 if kind == "relative" else 5)) and
        wire_crc(trigger_raw) == 0 and trigger["latest_us"] <= write_deadline)
    observable_trigger = item["execution"] == "acknowledged" or unconfirmed_trigger
    if not evidence["trigger_evidence"][2]:
        if setup == "verify": require(verification_known, "trigger precedes verification")
        if setup_count:
            require(item["staging_applied"], "trigger precedes checked staging")
        prior = stage if setup_count else verification if setup == "verify" else None
        if prior is not None:
            require(prior["delivered_us"] <= trigger["delivered_us"] and
                    (not trigger["qualified"] or prior["delivered_us"] <= trigger["earliest_us"]), "trigger precedes setup")
            require(prior["latest_us"] <= write_deadline and prior["delivered_us"] < write_deadline,
                    "trigger follows expired setup readiness")
    if item["running_observed"]:
        activity, raw = confirmed("activity_evidence", (item["address"], 3, observation_bytes), observation_size)
        motion = int.from_bytes(raw[5:7], "big")
        require(item["observation_known"] and trigger_step + 1 <= activity["step"] <= item["polls"] + trigger_step and
                trigger["delivered_us"] < activity["earliest_us"] and int.from_bytes(raw[3:5], "big") == 0 and
                bool(motion & 4) and not motion & 0x78, "activity is stale, faulted or lacks RUNNING")
    else:
        require(evidence["activity_evidence"][2], "unobserved activity retains a report")
    if item["observation_known"]:
        observed, raw = confirmed("last_observation", (item["address"], 3, observation_bytes), observation_size)
        require(observable_trigger and item["polls"] >= 1 and observed["step"] == item["polls"] + trigger_step and
                trigger["delivered_us"] < observed["earliest_us"] and
                item.get("raw_alarm") == int.from_bytes(raw[3:5], "big") and item.get("raw_motion") == int.from_bytes(raw[5:7], "big"),
                "last report differs from retained RX")
        require(trigger["latest_us"] <= write_deadline, "observations follow a trigger after readiness expiry")
        if item["running_observed"] and activity["step"] == observed["step"]:
            require(activity == observed, "same activity and observation token changed retained evidence")
    else:
        require(item.get("raw_alarm") is None and item.get("raw_motion") is None and evidence["last_observation"][2], "unknown report publishes values")
    if position_feedback and item["observation_known"]:
        pair = [int.from_bytes(raw[i:i+2], "big") for i in (11, 13)]
        if prerequisites["word_order"]: pair.reverse()
        position = (pair[0] << 16) | pair[1]
        if position & 0x80000000: position -= 2**32
        require(item.get("observed_position") == position and item.get("observed_speed") == int.from_bytes(raw[15:17], "big"),
                "position fields differ from checked feedback")
    if position_confirmed:
        match, match_raw = confirmed("position_match_evidence", (item["address"], 3, 14), 19)
        require(item["ok"] and item["execution"] == "acknowledged" and item["observation_known"] and
                match["step"] + 1 == observed["step"] and trigger["delivered_us"] < match["earliest_us"] and
                match["delivered_us"] < observed["earliest_us"] and
                int.from_bytes(match_raw[3:5], "big") == 0 and int.from_bytes(match_raw[5:7], "big") & 0x7D == 1 and
                match_raw[11:17] == raw[11:17] and item["observed_speed"] == 0 and
                item["observed_position"] == item["endpoint_native"], "completion lacks two exact stopped endpoint reports")
    if item["ok"]:
        require(not position_feedback or item["observed_speed"] == 0, "completion reports nonzero speed")
        require((position_confirmed or (item["running_observed"] and activity["delivered_us"] < observed["earliest_us"])) and item["observation_known"] and item["uncertain"] is False and
                observed["delivered_us"] == item["serviced_us"] and
                item["raw_alarm"] == 0 and item["raw_motion"] & 0x7D == 1 and evidence["failure_evidence"][2],
                "completion lacks correlated activity or exact endpoint evidence")
    else:
        require(not evidence["failure_evidence"][2] and evidence["failure_evidence"][0]["delivered_us"] == item["serviced_us"],
                "failure lacks terminal evidence")
        failure, raw, _ = evidence["failure_evidence"]
        if failure["step"] <= trigger_step:
            require(failure == evidence["verification_evidence" if setup == "verify" and failure["step"] == 0 else "staging_evidence" if failure["step"] < trigger_step else "trigger_evidence"][0],
                    "failed write differs from retained evidence")
        else:
            require(observable_trigger and failure["step"] in (item["polls"] + trigger_step, item["polls"] + trigger_step + 1),
                    "failure token skips an observation")
            previous = evidence["last_observation"][0] if item["observation_known"] else trigger
            if failure["step"] == item["polls"] + trigger_step:
                require(failure == previous, "terminal observation differs from retained report")
            else:
                require(previous["delivered_us"] <= failure["delivered_us"] and
                        (not failure["qualified"] or previous["delivered_us"] < failure["earliest_us"]), "failure precedes prior transaction")
        if failure["event"] != 0:
            expected = {1: "transport_error", 2: "cancelled", 3: "deadline"}[failure["event"]]
            if failure["event"] == 3:
                require(item["serviced_us"] >= (write_deadline if failure["step"] <= trigger_step else item["deadline_us"]),
                        "local deadline precedes its transaction budget")
        elif not failure["qualified"]:
            expected = "timing_unqualified"
        elif failure["latest_us"] > (write_deadline if failure["step"] <= trigger_step else item["deadline_us"]):
            expected = "deadline"
        elif failure["status"] != "OK":
            expected = "reply_error"
        elif not failure["response_confirmed"] and not (failure["step"] == trigger_step and unconfirmed_trigger):
            expected = "unconfirmed_response"
        elif failure["step"] > trigger_step and len(raw) == observation_size and (int.from_bytes(raw[3:5], "big") or int.from_bytes(raw[5:7], "big") & 0x78):
            expected = "reply_error"
        else:
            require(item["outcome"] in ("deadline", "observation_limit") and
                    (item["outcome"] != "deadline" or item["serviced_us"] >=
                        (write_deadline if failure["step"] <= trigger_step else item["deadline_us"])) and
                    (item["outcome"] != "observation_limit" or failure["step"] > trigger_step), "checked reply does not establish failure")
            expected = item["outcome"]
        require(item["outcome"] == expected, "failure outcome contradicts terminal evidence")
    require(item["uncertain"] == (not item["ok"] and bool(stage["tx_accepted"] or stage["execution_unknown"] or
                trigger["tx_accepted"] or trigger["execution_unknown"])), "uncertainty differs from staging/trigger evidence")


def check_velocity(item: dict, address: int | None, arguments: tuple[str, ...] | None) -> None:
    """Retain start uncertainty separately from a checked, finite stop report."""
    def require(condition, message):
        if not condition: raise BenchError("velocity " + message)
    def integer(value, low=0, high=2**64 - 1): return type(value) is int and low <= value <= high
    require(item.get("velocity") is True and item.get("move_kind") is None and item.get("action_kind") is None and
            item.get("read_kind") is None and not item.get("recovery", False) and not item.get("capture_read", False), "kind differs")
    require(integer(item.get("address"), 1, 247) and (address is None or item["address"] == address) and
            all(operation_id(item.get(key)) for key in ("target", "generation", "configuration_generation")), "binding differs")
    require(item.get("state") == ("succeeded" if item["ok"] else "failed") and
            item.get("completion") in ("observed", "not_observed") and item.get("outcome") in
            ("observed", "reply_error", "transport_error", "cancelled", "deadline", "timing_unqualified", "unconfirmed_response", "observation_limit"), "state differs")
    require(item.get("status") in {"OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED", "CRC_ERROR", "FRAME_ERROR", "EXCEPTION"} and
            integer(item.get("detail"), -2**31, 2**31 - 1) and
            (item["ok"] or (item["outcome"] not in ("none", "observed") and item["status"] != "OK")), "status differs")
    require(all(integer(item.get(k)) for k in ("started_us", "deadline_us", "stop_due_us", "serviced_us", "polls", "phase")) and
            item["started_us"] < item["stop_due_us"] < item["deadline_us"] and item["serviced_us"] >= item["started_us"] and
            item["polls"] <= 64 and item["phase"] <= 3, "time bounds differ")
    for key in ("staging_applied", "uncertain", "needs_stop", "running_observed", "service_missed", "observation_known", "interrupted_by_stop", "exact_arithmetic"):
        require(type(item.get(key)) is bool, "missing " + key)
    require(integer(item.get("native_rpm"), -3000, 3000) and item["native_rpm"] != 0 and item.get("ramp") == "configured" and
            item.get("stop_policy") in ("normal", "fast"), "speed or stop policy differs")
    words = item.get("staging_words")
    require(isinstance(words, list) and len(words) == 3 and words[0] == (item["native_rpm"] & 65535) and
            all(integer(word, 0, 2000) for word in words[1:]), "staged words differ")
    requested = item.get("requested")
    require(isinstance(requested, dict) and integer(requested.get("numerator"), -2**63, 2**63 - 1) and
            integer(requested.get("denominator"), 1) and integer(requested.get("position_unit"), 0, 6) and
            integer(requested.get("time_unit"), 0, 2) and integer(requested.get("frame"), 0, 2) and integer(requested.get("rounding"), 0, 4) and
            integer(requested.get("duration_us"), 1000, 1000000) and item["stop_due_us"] - item["started_us"] == requested["duration_us"] and
            type(requested.get("approximate")) is bool, "retained request differs")
    for key in ("rounding_error", "approximation_error_bound", "requested_rpm_approximate"):
        require(type(item.get(key)) in (int, float) and math.isfinite(item[key]), "invalid " + key)
    for key in ("maximum_quantization_error_rpm", "maximum_approximation_error_rpm"):
        require(type(requested.get(key)) in (int, float) and math.isfinite(requested[key]) and requested[key] >= 0, "invalid allowance")
    require(abs(item["rounding_error"]) + item["approximation_error_bound"] <= requested["maximum_quantization_error_rpm"] and
            item["approximation_error_bound"] >= 0 and item["approximation_error_bound"] <= requested["maximum_approximation_error_rpm"], "numerical allowance exceeded")
    if item["exact_arithmetic"]:
        require(requested["position_unit"] != 5 and item["approximation_error_bound"] == 0 and
                item["requested_rpm_approximate"] == 0, "exact preparation contains approximate provenance")
    else:
        require(requested["position_unit"] == 5 and requested["approximate"] and requested["rounding"] != 0 and
                requested["maximum_approximation_error_rpm"] > 0, "approximate preparation lost radian provenance")
    if requested["rounding"] == 0:
        require(item["exact_arithmetic"] and item["rounding_error"] == 0, "EXACT preparation rounded velocity")
    if arguments is not None:
        parsed = velocity_arguments(arguments)
        def retained_allowance(text, value):
            exact = Fraction(text)
            return Fraction(value) <= exact and (Fraction(value) == exact or exact < Fraction(math.nextafter(value, math.inf)))
        require(Fraction(parsed["value"]) == Fraction(requested["numerator"], requested["denominator"]) and
                parsed["unit"] == (requested["position_unit"], requested["time_unit"]) and parsed["frame"] == requested["frame"] and
                parsed["duration_us"] == requested["duration_us"] and parsed["stop"] == item["stop_policy"] and parsed["rounding"] == requested["rounding"] and
                parsed["approximate"] == requested["approximate"] and retained_allowance(parsed["error"], requested["maximum_quantization_error_rpm"]) and
                retained_allowance(parsed["approximation_error"], requested["maximum_approximation_error_rpm"]), "request correlation differs")
    evidence = {}
    for name, token, size in (("staging_evidence", 0, 15), ("trigger_evidence", 1, 8), ("activity_evidence", None, 8),
            ("failure_evidence", None, None), ("stop_write_evidence", 0, 8), ("stop_observation", None, 8), ("stop_failure_evidence", None, 8)):
        entry = item.get(name)
        require(isinstance(entry, dict) and all(integer(entry.get(k), 0, n) for k, n in (("step", 255), ("event", 3),
                ("received_length", 2**32 - 1), ("tx_accepted", 15), ("frame_error", 255))) and
                all(type(entry.get(k)) is bool for k in ("tx_complete", "response_confirmed", "qualified", "execution_unknown")) and
                all(integer(entry.get(k)) for k in ("earliest_us", "latest_us", "delivered_us")) and
                all(integer(entry.get(k), -2**31, 2**31 - 1) for k in ("detail", "transport_detail")) and
                entry.get("status") in {"OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED", "CRC_ERROR", "FRAME_ERROR", "EXCEPTION"}, name + " invalid")
        raw = entry.get("raw_hex")
        require(isinstance(raw, str) and re.fullmatch(r"(?:[0-9a-fA-F]{2}){0,19}", raw) is not None, name + " raw invalid")
        raw = bytes.fromhex(raw); require(len(raw) in (min(entry["received_length"], 9), min(entry["received_length"], 19)), name + " length differs")
        empty = all(entry.get(key) == value for key, value in dict(step=0, event=0, raw_hex="", received_length=0,
            tx_accepted=0, tx_complete=False, response_confirmed=False, qualified=False, execution_unknown=False,
            earliest_us=0, latest_us=0, delivered_us=0, transport_detail=0, status="OK", detail=0, frame_error=0).items())
        if not empty:
            require(token is None or entry["step"] == token, name + " step differs")
            require(item["started_us"] <= entry["delivered_us"] <= item["serviced_us"] and entry["tx_accepted"] <= (size or (15 if entry["step"] == 0 else 8)) and
                    (not entry["tx_complete"] or entry["tx_accepted"] == (size or (15 if entry["step"] == 0 else 8))), name + " TX differs")
            require(entry["event"] == 0 or not entry["response_confirmed"], name + " local response")
        if entry["qualified"]:
            require(not empty and entry["event"] == 0 and entry["tx_complete"] and
                    item["started_us"] <= entry["earliest_us"] <= entry["latest_us"] <= entry["delivered_us"], name + " closure differs")
        else: require(entry["earliest_us"] == entry["latest_us"] == 0, name + " unqualified timing")
        evidence[name] = (entry, raw, empty)
    def confirmed(name, prefix, size):
        e, raw, empty = evidence[name]
        require(not empty and e["qualified"] and e["response_confirmed"] and e["tx_complete"] and e["status"] == "OK" and
                e["detail"] == e["frame_error"] == 0 and len(raw) == size and raw.startswith(bytes(prefix)) and wire_crc(raw) == 0,
                name + " checked response absent")
        return e, raw
    for field, name, prefix in (("setup_execution", "staging_evidence", (item["address"], 16, 0, 0x1D, 0, 3)),
            ("execution", "trigger_evidence", (item["address"], 6, 0, 0x27, 0, 2)),
            ("stop_execution", "stop_write_evidence", (item["address"], 6, 0, 0x27, 1 if item["stop_policy"] == "normal" else 2, 0))):
        execution = item.get(field); e, raw, empty = evidence[name]
        require(execution in ("not_transmitted", "acknowledged", "rejected", "unknown"), field + " invalid")
        if execution == "acknowledged": confirmed(name, prefix, 8)
        elif execution == "not_transmitted": require(e["tx_accepted"] == 0 and not e["execution_unknown"], name + " transmission contradiction")
        elif execution == "unknown":
            require(not empty and (e["tx_accepted"] > 0 or e["execution_unknown"]) and not
                    (e["event"] == 0 and e["qualified"] and e["response_confirmed"] and
                     (e["status"] == "OK" or (e["status"] == "EXCEPTION" and 1 <= e["detail"] <= 7))), name + " uncertainty absent or contradicts checked reply")
        else:
            require(not empty and e["qualified"] and e["response_confirmed"] and e["status"] == "EXCEPTION" and e["frame_error"] == 10 and
                    len(raw) == 5 and raw[:2] == bytes((item["address"], prefix[1] | 128)) and 1 <= raw[2] <= 7 and e["detail"] == raw[2] and wire_crc(raw) == 0,
                    name + " rejection absent")
    require(item["staging_applied"] == (item["setup_execution"] == "acknowledged"), "staging validity differs")
    if item["execution"] != "not_transmitted": require(item["staging_applied"], "start without applied setup")
    if not evidence["trigger_evidence"][2]:
        require(evidence["trigger_evidence"][0]["delivered_us"] >= evidence["staging_evidence"][0]["delivered_us"], "trigger precedes setup")
    if not evidence["stop_write_evidence"][2]:
        require(item["execution"] in ("acknowledged", "unknown") and
                evidence["stop_write_evidence"][0]["delivered_us"] >= evidence["trigger_evidence"][0]["delivered_us"] and
                (not evidence["stop_write_evidence"][0]["qualified"] or
                 evidence["stop_write_evidence"][0]["earliest_us"] >= evidence["trigger_evidence"][0]["delivered_us"]),
                "stop precedes possibly executed trigger")
    for name in ("staging_evidence", "trigger_evidence"):
        if item["ok"]:
            require(evidence[name][0]["latest_us"] <= item["stop_due_us"], name + " exceeds finite run deadline")
    if item["running_observed"]:
        activity, raw = confirmed("activity_evidence", (item["address"], 3, 4), 9)
        require(item["execution"] == "acknowledged" and raw[3:5] == b"\0\0" and int.from_bytes(raw[5:7], "big") & 4 and
                not int.from_bytes(raw[5:7], "big") & 0x78 and 2 <= activity["step"] <= item["polls"] + 1 and
                activity["earliest_us"] > evidence["trigger_evidence"][0]["delivered_us"] and activity["latest_us"] <= item["stop_due_us"], "running report differs")
    else:
        require(evidence["activity_evidence"][2], "unobserved activity retains a report")
    latest = item.get("last_observation")
    require(isinstance(latest, dict) and integer(latest.get("step"), 0, 255) and
            all(integer(latest.get(key)) for key in ("earliest_us", "latest_us", "delivered_us")) and
            isinstance(latest.get("raw_hex"), str) and re.fullmatch(r"(?:[0-9a-fA-F]{2}){0,19}", latest["raw_hex"]) is not None, "latest observation invalid")
    if item["observation_known"]:
        raw = bytes.fromhex(latest["raw_hex"])
        require(len(raw) == 9 and raw[:3] == bytes((item["address"], 3, 4)) and wire_crc(raw) == 0 and
                item["execution"] == "acknowledged" and item["polls"] >= 1 and latest["step"] == item["polls"] + 1 and
                latest["earliest_us"] > evidence["trigger_evidence"][0]["delivered_us"] and
                item["started_us"] <= latest["earliest_us"] <= latest["latest_us"] <= min(latest["delivered_us"], item["stop_due_us"]) and
                latest["delivered_us"] <= item["serviced_us"] and item.get("raw_alarm") == int.from_bytes(raw[3:5], "big") and
                item.get("raw_motion") == int.from_bytes(raw[5:7], "big"), "observation differs")
        if item["running_observed"]:
            activity = evidence["activity_evidence"][0]
            require(latest["step"] >= activity["step"] and latest["latest_us"] >= activity["latest_us"] and
                    latest["delivered_us"] >= activity["delivered_us"], "latest observation precedes activity")
            if latest["step"] == activity["step"]:
                require(all(latest[key] == activity[key] for key in latest), "same observation token changed retained evidence")
    else:
        require(item.get("raw_alarm") is None and item.get("raw_motion") is None and latest ==
                dict(step=0, raw_hex="", earliest_us=0, latest_us=0, delivered_us=0), "unknown observation published")
    require(item.get("stop_completion") in ("observed", "not_observed") and item.get("stop_outcome") in
            ("none", "observed", "reply_error", "transport_error", "cancelled", "deadline", "timing_unqualified", "unconfirmed_response", "observation_limit") and
            (item["stop_completion"] == "observed") == (item["stop_outcome"] == "observed") and
            (item["completion"] == "observed") == (item["stop_completion"] == "observed"), "stop completion invalid")
    require(item["uncertain"] == (not item["ok"] and any(
                evidence[name][0]["tx_accepted"] or evidence[name][0]["execution_unknown"]
                for name in ("staging_evidence", "trigger_evidence"))), "uncertainty differs from setup/start evidence")
    require(item["needs_stop"] == (item["execution"] in ("acknowledged", "unknown") and
                item["stop_completion"] != "observed"), "stop obligation differs from possible start execution")
    if item["stop_completion"] == "observed":
        observation, raw = confirmed("stop_observation", (item["address"], 3, 4), 9)
        require(not int.from_bytes(raw[5:7], "big") & 4 and not item["needs_stop"] and item["stop_execution"] == "acknowledged" and
                1 <= observation["step"] <= 64 and
                observation["earliest_us"] > evidence["stop_write_evidence"][0]["delivered_us"] and observation["latest_us"] <= item["deadline_us"] and
                evidence["stop_write_evidence"][0]["latest_us"] <= item["deadline_us"], "stop observation differs")
    if item["ok"]:
        require(item["outcome"] == "observed" and item["status"] == "OK" and item["detail"] == 0 and item["running_observed"] and
                item["execution"] == "acknowledged" and item["stop_completion"] == "observed" and item["completion"] == "observed" and not item["uncertain"] and
                not item["service_missed"] and not item["interrupted_by_stop"], "success lacks complete evidence")
        require(evidence["failure_evidence"][2] and evidence["stop_failure_evidence"][2], "success retains failure evidence")
    elif evidence["failure_evidence"][2]:
        # Time service and an absent activity report have no transaction to
        # copy. A stop failure has its own independently retained evidence.
        stop_failed = (item["outcome"] == item["stop_outcome"] and
                       not evidence["stop_failure_evidence"][2])
        local_deadline = item["outcome"] == "deadline" and item["status"] == "ILLEGAL_VALUE" and (
            item["serviced_us"] >= item["deadline_us"] or item["service_missed"] or
            (item["detail"] == 17 and item["phase"] < 2 and item["serviced_us"] >= item["stop_due_us"]) or
            (item["detail"] == 10 and item["phase"] < 2))
        absent_activity = (item["outcome"] == "observation_limit" and item["detail"] == 25 and
                           not item["running_observed"] and item["stop_completion"] == "observed")
        require(stop_failed or local_deadline or absent_activity, "failure lacks terminal evidence")


def check_home(item: dict, address: int | None, arguments: tuple[str, ...] | None) -> None:
    """Validate retained homing traffic without treating old HOMED as completion."""
    def require(condition, message):
        if not condition: raise BenchError("home " + message)
    def uint(value, limit=0xFFFFFFFFFFFFFFFF):
        return type(value) is int and 0 <= value <= limit
    require(item.get("home") is True and all(item.get(k) is None for k in ("read_kind", "action_kind", "move_kind")) and
            all(item.get(k, False) is False for k in ("velocity", "driver", "capture_read", "recovery")), "kind is inconsistent")
    require(uint(item.get("address"), 247) and item["address"] > 0 and (address is None or item["address"] == address), "address differs")
    require(all(operation_id(item.get(k)) for k in ("target", "generation", "configuration_generation")), "binding is invalid")
    check_counts(item, ("started_us", "deadline_us", "serviced_us", "polls", "completion_observed_us"), "home")
    require(item["started_us"] < item["deadline_us"] and item["started_us"] <= item["serviced_us"] and item["polls"] <= 64, "time/count bounds are invalid")
    require(item.get("method") in (33, 34, 35), "unimplemented method yielded traffic")
    require(uint(item.get("phase"), 3), "phase is invalid")
    words = item.get("staging_words")
    require(isinstance(words, list) and len(words) == 6 and all(uint(w, 65535) for w in words) and
            words[0] == item["method"] and 5 <= words[1] <= 3000 and 5 <= words[2] <= 300 and
            30 <= words[3] <= 2000 and words[4:] == [0, 0], "staging differs from reviewed zero-offset window")
    if arguments is not None: require(tuple(words[:4]) == home_arguments(arguments), "request differs from command")
    require(type(item.get("ok")) is bool and item.get("state") == ("succeeded" if item["ok"] else "failed") and
            item.get("completion") == ("observed" if item["ok"] else "not_observed"), "state/completion is inconsistent")
    require(item.get("outcome") in ({"observed"} if item["ok"] else
            {"reply_error", "transport_error", "cancelled", "deadline", "timing_unqualified", "unconfirmed_response", "observation_limit"}), "outcome is inconsistent")
    for flag in ("staging_applied", "uncertain", "running_observed", "homed_low_observed", "observation_known", "interrupted_by_stop"):
        require(type(item.get(flag)) is bool, flag + " is invalid")
    p = item.get("prerequisites")
    require(isinstance(p, dict) and uint(p.get("observed_us")) and uint(p.get("maximum_age_us")) and
            evidence_age_valid(p["observed_us"], item["started_us"], p["maximum_age_us"]) and
            uint(p.get("raw_motion"), 65535) and not p["raw_motion"] & 0x7C and
            p.get("auxiliary") == 7 and type(p.get("reference_semantics_qualified")) is bool,
            "immutable prerequisites are invalid")
    qualified = p.get("qualified_parameters")
    require(isinstance(qualified, list) and len(qualified) == 4 and all(type(v) is int for v in qualified) and
            qualified == words[:4], "method/rate/ramp differs from exact qualified configuration")
    # Older protocol-3 records used only the two-word observation window.
    # New records retain fixture age independently of refreshed readback.
    q = item.get("qualification")
    observe_inputs = False
    revoked = item.get("qualification_revoked", False)
    write_cap = min(item["deadline_us"], evidence_age_deadline(p["observed_us"], p["maximum_age_us"]))
    if q is not None:
        require(isinstance(q, dict) and type(q.get("observe_inputs")) is bool and
                type(revoked) is bool and type(item.get("inputs_observed")) is bool and
                uint(item.get("raw_inputs"), 65535), "qualification schema is invalid")
        observe_inputs = q["observe_inputs"]
        require(all(uint(q.get(k)) for k in ("qualified_us", "maximum_age_us", "io_observed_us", "readback_observed_us")) and
                evidence_age_valid(q["qualified_us"], item["started_us"], q["maximum_age_us"]), "fixture age is invalid")
        write_cap = min(write_cap, evidence_age_deadline(q["qualified_us"], q["maximum_age_us"]))
        if observe_inputs:
            require(all(evidence_age_valid(q[k], item["started_us"], p["maximum_age_us"])
                        for k in ("io_observed_us", "readback_observed_us")), "live input/readback age is invalid")
            write_cap = min(write_cap, *(evidence_age_deadline(q[k], p["maximum_age_us"])
                                        for k in ("io_observed_us", "readback_observed_us")))
            require(uint(q.get("input_polarity"), 15) and uint(q.get("raw_inputs"), 15) and
                    uint(q.get("available_inputs"), 7), "input binding is invalid")
            functions, wiring = q.get("input_functions"), q.get("input_wiring")
            require(isinstance(functions, list) and len(functions) == 4 and all(uint(v, 3) for v in functions) and
                    isinstance(wiring, list) and len(wiring) == 4 and all(uint(v, 2) for v in wiring), "input configuration is invalid")
            available = 0
            for function, wire in zip(functions, wiring):
                if function and wire == 2: available |= 1 << (function - 1)
            require(available == q["available_inputs"] and
                    not any(function and q["raw_inputs"] & (1 << i) for i, function in enumerate(functions)),
                    "input qualification contradicts implemented no-switch method")
        require(not revoked or (not item["ok"] and item["outcome"] == "cancelled" and item.get("detail") == 33),
                "revocation did not prevent success")
    else:
        require(not revoked, "revocation lacks copied qualification")
    status_length, status_bytes = (11, 6) if observe_inputs else (9, 4)
    require(item.get("evidence_columns") == HOME_EVIDENCE_COLUMNS, "evidence schema is invalid")
    evidence = {}
    for name in ("staging_evidence", "trigger_evidence", "activity_evidence", "low_evidence", "last_observation", "completion_evidence", "zero_evidence", "failure_evidence"):
        values = item.get(name)
        require(isinstance(values, list) and len(values) == len(HOME_EVIDENCE_COLUMNS), "missing " + name)
        e = dict(zip(HOME_EVIDENCE_COLUMNS, values))
        for key, limit in (("step", 67), ("event", 3), ("received_length", 0xFFFFFFFF), ("tx_accepted", 21 if name == "staging_evidence" or e.get("step") == 0 else 8),
                           ("frame_error", 255), ("earliest_us", 0xFFFFFFFFFFFFFFFF), ("latest_us", 0xFFFFFFFFFFFFFFFF), ("delivered_us", 0xFFFFFFFFFFFFFFFF)):
            require(uint(e.get(key), limit), name + " invalid " + key)
        for key in ("tx_complete", "response_confirmed", "qualified", "execution_unknown"):
            require(type(e.get(key)) is bool, name + " invalid " + key)
        require(all(type(e.get(k)) is int and -0x80000000 <= e[k] <= 0x7FFFFFFF for k in ("detail", "transport_detail")) and
                e.get("status") in ("OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED", "CRC_ERROR", "FRAME_ERROR", "EXCEPTION"), name + " invalid status")
        raw = e.get("raw_hex")
        require(isinstance(raw, str) and re.fullmatch(r"(?:[0-9A-Fa-f]{2}){0,19}", raw) is not None, name + " malformed raw frame")
        raw = bytes.fromhex(raw)
        require(len(raw) in (min(e["received_length"], 9), min(e["received_length"], 19)), name + " inconsistent raw length")
        expected = 21 if e["step"] == 0 else 8
        require(not e["tx_complete"] or e["tx_accepted"] == expected, name + " incomplete accepted TX")
        if e["delivered_us"]:
            require(item["started_us"] <= e["delivered_us"] <= item["serviced_us"], name + " delivery outside lifetime")
            require(e["event"] != 0 or e["tx_complete"], name + " frame lacks physical TX completion")
        require(e["event"] == 0 or not e["response_confirmed"], name + " local event claims response")
        if e["qualified"]:
            require(e["event"] == 0 and item["started_us"] <= e["earliest_us"] <= e["latest_us"] <= e["delivered_us"], name + " invalid timing")
        else: require(e["earliest_us"] == e["latest_us"] == 0, name + " unqualified bounds")
        if e["status"] == "OK": require(e["detail"] == e["frame_error"] == 0, name + " OK retains errors")
        if e["delivered_us"] and e["status"] == "OK" and e["event"] == 0 and e["step"] >= 2:
            is_zero = name == "zero_evidence" or (name == "failure_evidence" and item["phase"] == 3)
            require(len(raw) == (9 if is_zero else status_length) and
                    raw[:3] == bytes((item["address"], 3, 4 if is_zero else status_bytes)) and wire_crc(raw) == 0,
                    name + " differs from admitted observation window")
        evidence[name] = (e, raw)
    def checked(name, prefix):
        e, raw = evidence[name]
        return (e["event"] == 0 and e["tx_complete"] and e["response_confirmed"] and e["qualified"] and
                e["status"] == "OK" and e["frame_error"] == e["detail"] == 0 and len(raw) == len(prefix) + 2 and
                raw[:-2] == prefix and wire_crc(raw) == 0)
    for name, field, prefix in (("staging_evidence", "setup_execution", bytes((item["address"], 16, 0, 0x31, 0, 6))),
                                ("trigger_evidence", "execution", bytes((item["address"], 6, 0, 0x27, 0, 16)))):
        e, raw = evidence[name]
        require(e["step"] == (0 if name == "staging_evidence" else 1) or not e["delivered_us"], field + " token differs")
        execution = item.get(field)
        require(execution in ("not_transmitted", "acknowledged", "rejected", "unknown"), field + " invalid")
        if execution == "acknowledged": require(checked(name, prefix), field + " lacks checked echo")
        elif execution == "not_transmitted": require(not e["tx_accepted"] and not e["execution_unknown"], field + " contradicts TX")
        elif execution == "rejected":
            require(e["qualified"] and e["response_confirmed"] and e["tx_complete"] and e["event"] == 0 and
                    e["status"] == "EXCEPTION" and e["frame_error"] == 10 and len(raw) == 5 and raw[:2] == bytes((item["address"], prefix[1] | 128)) and
                    1 <= raw[2] <= 7 and raw[2] == e["detail"] and wire_crc(raw) == 0, field + " lacks documented exception")
        else: require(e["tx_accepted"] > 0 or e["execution_unknown"], field + " lacks uncertainty")
        if e["qualified"] and e["status"] == "OK":
            late = e["latest_us"] > write_cap
            require(not late or (not item["ok"] and (item["outcome"] == "deadline" or revoked) and item["failure_evidence"] == item[name]), "write exceeds readiness without retained deadline")
    require(item["staging_applied"] == (item["setup_execution"] == "acknowledged"), "staging application differs")
    stage, _ = evidence["staging_evidence"]
    trigger, _ = evidence["trigger_evidence"]
    if trigger["tx_accepted"] or trigger["execution_unknown"]:
        cap = write_cap
        require(item["setup_execution"] == "acknowledged" and stage["latest_us"] <= cap and stage["delivered_us"] < cap and
                stage["delivered_us"] <= (trigger["earliest_us"] if trigger["qualified"] else trigger["delivered_us"]),
                "trigger precedes checked in-budget staging")
    require(item["uncertain"] == (not item["ok"] and any(evidence[n][0]["tx_accepted"] or evidence[n][0]["execution_unknown"] for n in ("staging_evidence", "trigger_evidence"))), "uncertainty differs")
    require(uint(item.get("raw_alarm"), 65535) and uint(item.get("raw_motion"), 65535) and
            isinstance(item.get("raw_position_words"), list) and len(item["raw_position_words"]) == 2 and all(uint(w, 65535) for w in item["raw_position_words"]), "raw outputs invalid")
    if item["ok"]:
        require(item["setup_execution"] == item["execution"] == "acknowledged" and item["homed_low_observed"] and item["observation_known"], "completion lacks fresh transition")
        comp, raw = evidence["completion_evidence"]
        require(len(raw) == status_length and checked("completion_evidence", raw[:-2]) and raw[:3] == bytes((item["address"], 3, status_bytes)) and
                raw[3:5] == b"\0\0" and int.from_bytes(raw[5:7], "big") & 0x7F == 3 and
                evidence["trigger_evidence"][0]["delivered_us"] < comp["earliest_us"] and comp["latest_us"] <= item["deadline_us"], "completion lacks stopped homed FC03")
        require(comp["step"] == item["polls"] + 1 and comp["step"] >= 2 and item["phase"] == 3 and
                item["raw_alarm"] == 0 and item["raw_motion"] == int.from_bytes(raw[5:7], "big") and
                item["last_observation"] == item["completion_evidence"] and
                evidence["zero_evidence"][0]["step"] == comp["step"] + 1, "completion/zero tokens or decoded outputs differ")
        if observe_inputs:
            require(item["inputs_observed"] and item["raw_inputs"] == int.from_bytes(raw[7:9], "big") and item["raw_inputs"] <= 15 and
                    not any(function and item["raw_inputs"] & (1 << i) for i, function in enumerate(q["input_functions"])),
                    "completion logical inputs contradict qualification")
        require(checked("zero_evidence", bytes((item["address"], 3, 4, 0, 0, 0, 0))) and item["raw_position_words"] == [0, 0] and
                comp["delivered_us"] < evidence["zero_evidence"][0]["earliest_us"] and
                item["started_us"] <= item["completion_observed_us"] <= comp["earliest_us"] and
                evidence["zero_evidence"][0]["delivered_us"] == item["serviced_us"] and evidence["zero_evidence"][0]["latest_us"] <= item["deadline_us"], "zero/reference evidence is inconsistent")
        if item["method"] != 35 or p["raw_motion"] & 2:
            low, raw = evidence["low_evidence"]
            require(len(raw) == status_length and checked("low_evidence", raw[:-2]) and raw[:3] == bytes((item["address"], 3, status_bytes)) and
                    raw[3:5] == b"\0\0" and not int.from_bytes(raw[5:7], "big") & (0x7E if item["method"] == 35 else 0x7A) and
                    2 <= low["step"] < comp["step"] and evidence["trigger_evidence"][0]["delivered_us"] < low["earliest_us"] and
                    low["delivered_us"] < comp["earliest_us"], "completion lacks new homed-low evidence")
        if item["method"] != 35:
            activity, raw = evidence["activity_evidence"]
            require(item["running_observed"] and len(raw) == status_length and checked("activity_evidence", raw[:-2]) and raw[:3] == bytes((item["address"], 3, status_bytes)) and
                    raw[3:5] == b"\0\0" and not int.from_bytes(raw[5:7], "big") & 0x78 and int.from_bytes(raw[5:7], "big") & 4 and
                    2 <= activity["step"] < comp["step"] and evidence["trigger_evidence"][0]["delivered_us"] < activity["earliest_us"] and
                    activity["delivered_us"] < comp["earliest_us"], "index search lacks new running evidence")
        else:
            require(not item["running_observed"] and not evidence["activity_evidence"][0]["delivered_us"], "current-position origin claims motion")
        require(not evidence["failure_evidence"][0]["delivered_us"] and item["status"] == "OK" and item["detail"] == 0, "success retains failure")
    else:
        failure, raw = evidence["failure_evidence"]
        require(failure["delivered_us"] == item["serviced_us"] and failure["delivered_us"] > 0, "failure lacks terminal evidence")
        require(item.get("status") in ("ILLEGAL_VALUE", "UNSUPPORTED", "CRC_ERROR", "FRAME_ERROR", "EXCEPTION") and
                type(item.get("detail")) is int, "failure has invalid terminal status")
        outcome = item["outcome"]
        if revoked:
            require(item["status"] == "ILLEGAL_VALUE" and item["detail"] == 33 and outcome == "cancelled", "revoked terminal differs")
            if failure["event"] == 0 and failure["status"] == "OK":
                require(wire_crc(raw) == 0 and len(raw) in (8, 9, 11), "revocation lost checked settled frame")
                if item["phase"] == 3: require(item["zero_evidence"] == item["failure_evidence"], "revocation lost final zero evidence")
        elif outcome in ("cancelled", "transport_error"):
            event, detail = (2, 22) if outcome == "cancelled" else (1, 21)
            require(failure["event"] == event and failure["status"] == item["status"] == "ILLEGAL_VALUE" and
                    failure["detail"] == item["detail"] == detail, "local failure differs from terminal evidence")
        elif outcome == "deadline":
            cap = write_cap if failure["step"] < 2 else item["deadline_us"]
            require(item["status"] == "ILLEGAL_VALUE" and item["detail"] in (15, 20) and
                    ((failure["event"] == 3 and failure["delivered_us"] >= cap and
                      failure["status"] == item["status"] and failure["detail"] == item["detail"]) or
                     (failure["event"] == 0 and failure["qualified"] and
                      (failure["latest_us"] > cap or
                       (failure["status"] == "OK" and failure["response_confirmed"] and
                        item["failure_evidence"] != item["zero_evidence"] and failure["delivered_us"] >= cap)))),
                    "deadline lacks expired transaction or next-step budget")
        elif outcome in ("timing_unqualified", "unconfirmed_response"):
            unqualified = outcome == "timing_unqualified"
            require(item["status"] == "ILLEGAL_VALUE" and item["detail"] == (23 if unqualified else 24) and
                    failure["event"] == 0 and failure["qualified"] == (not unqualified) and
                    (unqualified or (failure["status"] == "OK" and not failure["response_confirmed"])),
                    "response qualification failure differs from evidence")
        else:
            require(failure["event"] == 0 and failure["qualified"], "reply failure lacks qualified frame")
            if outcome == "observation_limit":
                require(item["status"] == "ILLEGAL_VALUE" and item["detail"] == 26 and item["polls"] > 0 and
                        failure["status"] == "OK" and failure["response_confirmed"] and
                        item["failure_evidence"] == item["last_observation"],
                        "observation limit lacks final observation")
            elif failure["status"] != "OK":
                require(item["status"] == failure["status"] and item["detail"] == failure["detail"],
                        "parser failure differs from terminal evidence")
            else:
                zero_failure = item["detail"] == 27
                require(failure["response_confirmed"] and item["status"] == "ILLEGAL_VALUE" and
                        len(raw) == (9 if zero_failure else status_length) and raw[:3] == bytes((item["address"], 3, 4 if zero_failure else status_bytes)) and
                        wire_crc(raw) == 0 and item["detail"] in (13, 25, 27, 31), "decoded failure lacks checked read")
                first, second = int.from_bytes(raw[3:5], "big"), int.from_bytes(raw[5:7], "big")
                inputs = int.from_bytes(raw[7:9], "big") if observe_inputs and not zero_failure else 0
                require((item["detail"] == 13 and observe_inputs and (inputs > 15 or
                            any(function and inputs & (1 << i) for i, function in enumerate(q["input_functions"])))) or
                        (item["detail"] == 25 and (first or second & 0x78)) or
                        (item["detail"] == 27 and (first or second) and item["failure_evidence"] == item["zero_evidence"]) or
                        (item["detail"] == 31 and item["method"] == 35 and not first and not second & 0x78 and second & 4),
                        "decoded failure reason differs from frame")
