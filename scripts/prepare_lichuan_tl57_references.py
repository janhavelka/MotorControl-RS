"""Check the TL57-R research bundle offline; optionally restore or extract sources.

This documentation tool performs no serial I/O. Downloads restore missing files
only, verify pinned bytes before writing, and never replace existing snapshots.
"""

import argparse
import json

import fitz
import requests

from prepare_lichuan_references import DOCS, checked_path, check_bytes, crc16


PREFIX = "lichuan_tl57_rs485"


def read_data(suffix):
    return json.loads((DOCS / "reference" / f"{PREFIX}_{suffix}.json").read_text(encoding="utf-8"))


def valid_shape(frame, shape):
    if len(frame) < 5:
        return False
    if shape == "exception":
        return len(frame) == 5 and bool(frame[1] & 0x80)
    if shape == "fc03_reply":
        return frame[1] == 3 and frame[2] % 2 == 0 and len(frame) == 5 + frame[2]
    if shape == "fc10_request":
        return (len(frame) >= 9 and frame[1] == 16
                and frame[6] == 2 * int.from_bytes(frame[4:6], "big")
                and len(frame) == 9 + frame[6])
    functions = {"fc03_request": 3, "fc06_request_or_echo": 6, "fc10_reply": 16}
    return len(frame) == 8 and frame[1] == functions.get(shape)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Offline check (also the default)")
    parser.add_argument("--download-missing", action="store_true")
    parser.add_argument("--extract", action="store_true")
    args = parser.parse_args()
    records = read_data("sources")["references"]
    if len(records) != 4 or len({r["path"] for r in records}) != 4:
        raise ValueError("expected four distinct original sources")
    by_id = {r["id"]: r for r in records}
    for record in records:
        path = checked_path(record["path"])
        if not path.exists() and args.download_missing:
            # The original URL obtains a fresh CDN redirect; archived signed URLs expire.
            response = requests.get(record["url"], headers={
                "User-Agent": "Mozilla/5.0", "Referer": record["referer"]}, timeout=(15, 60))
            response.raise_for_status()
            check_bytes(record, response.content)
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("xb") as output:
                output.write(response.content)
        data = path.read_bytes()
        check_bytes(record, data)
        if record.get("text_extract"):
            destination = checked_path(record["text_extract"])
            if args.extract:
                content = [f"# {path.name}\n\nPhysical PDF pages start at 1. Search aid only; original tables and figures are authoritative.\n"]
                with fitz.open(stream=data, filetype="pdf") as pdf:
                    content += [f"\n## PDF page {i+1}\n\n" + p.get_text(sort=True) for i, p in enumerate(pdf)]
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text("\n".join(content), encoding="utf-8", newline="\n")
            if not destination.is_file():
                raise ValueError(f"missing extract: {destination}")
        if any(not 1 <= p <= record["pages"] for p in record.get("visually_reviewed_pdf_pages", [])):
            raise ValueError("invalid reviewed page")
    ledger = read_data("registers")
    rows = ledger["registers"]
    addresses = {r["address"] for r in rows}
    if len(rows) != 203 or len(addresses) != 203:
        raise ValueError("expected 203 distinct documented register words")
    if ledger["missing_addresses_through_0x0119"] != [a for a in range(282) if a not in addresses]:
        raise ValueError("gap ledger differs")
    for row in rows:
        if row["address_hex"] != f"0x{row['address']:04X}" or not row["name_en"]:
            raise ValueError("invalid register transcription")
        if not row["pdf_pages"] or any(p not in range(15, 24) for p in row["pdf_pages"]):
            raise ValueError("invalid register-table page")
        if "pair_start" in row and not {row["pair_start"], row["pair_start"] + 1} <= addresses:
            raise ValueError("incomplete documented pair")
    vectors = read_data("vectors")["vectors"]
    if len(vectors) != 52 or len({v["published_hex"] for v in vectors}) != 52:
        raise ValueError("expected 52 distinct published frames")
    for vector in vectors:
        frame = bytes.fromhex(vector["published_hex"])
        crc = crc16(frame[:-2])
        if crc.hex(" ").upper() != vector["calculated_crc_hex"] or (crc == frame[-2:]) != vector["crc_matches"]:
            raise ValueError(f"CRC disposition differs: {vector['id']}")
        if valid_shape(frame, vector["shape"]) != vector["shape_valid"]:
            raise ValueError(f"shape disposition differs: {vector['id']}")
        rejected = not vector["crc_matches"] or not vector["shape_valid"]
        if rejected != (vector["disposition"] == "REJECT_PUBLISHED_FRAME"):
            raise ValueError("invalid published-frame disposition")
        for source in vector["sources"]:
            if not 1 <= source["pdf_page"] <= by_id[source["source_id"]]["pages"]:
                raise ValueError("invalid frame provenance")
    models = read_data("models")["models"]
    if len(models) != 6 or len({m["model"] for m in models}) != 6:
        raise ValueError("expected six RS485 model candidates")
    print("PASS: 4 preserved sources, 203 register words, 52 frame checks, 6 model candidates; no hardware I/O")


if __name__ == "__main__":
    main()
