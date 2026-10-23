"""Verify the preserved LC57BK-R research bundle; optionally restore/extract PDFs.

Offline by default. --download-missing retrieves only missing manifest entries,
checks their pinned bytes before writing, and never replaces an existing file.
--extract regenerates page-indexed search aids; original PDFs remain authoritative.
This is a documentation tool, not a drive implementation or serial test program.
"""

import argparse
import hashlib
import json
from pathlib import Path

import fitz
import requests


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
PREFIX = "lichuan_lc57bk_rs485"
MANIFEST = DOCS / "reference" / (PREFIX + "_sources.json")


def checked_path(relative):
    path = (DOCS / relative).resolve()
    if not path.is_relative_to(DOCS.resolve()):
        raise ValueError(f"reference escapes docs: {relative}")
    return path


def check_bytes(record, data):
    if len(data) != record["bytes"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
        raise ValueError(f"snapshot differs from manifest: {record['path']}")
    if record["format"] == "pdf":
        if not data.startswith(b"%PDF-"):
            raise ValueError(f"not a PDF: {record['path']}")
        with fitz.open(stream=data, filetype="pdf") as pdf:
            if len(pdf) != record["pages"]:
                raise ValueError(f"PDF page count differs: {record['path']}")


def crc16(payload):
    crc = 0xFFFF
    for value in payload:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc.to_bytes(2, "little")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Offline verification (also the default)")
    parser.add_argument("--download-missing", action="store_true")
    parser.add_argument("--extract", action="store_true")
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    records = manifest["references"]
    if len({r["path"] for r in records}) != len(records):
        raise ValueError("duplicate reference paths")
    for record in records:
        path = checked_path(record["path"])
        if not path.exists() and args.download_missing:
            response = requests.get(record["resolved_url"], headers={
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
                content = [f"# {path.name}\n\nPhysical PDF page numbers start at 1.\n"
                           "Search aid only: tables may be reordered; inspect the original PDF.\n"]
                with fitz.open(stream=data, filetype="pdf") as pdf:
                    for number, page in enumerate(pdf, 1):
                        content.append(f"\n## PDF page {number}\n\n{page.get_text(sort=True)}")
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text("\n".join(content), encoding="utf-8", newline="\n")
            if not destination.is_file():
                raise ValueError(f"missing extract: {destination}")
        for page in record.get("visually_reviewed_pdf_pages", []):
            if not 1 <= page <= record["pages"]:
                raise ValueError("invalid reviewed page")
    ledger = json.loads((DOCS / "reference" / (PREFIX + "_registers.json")).read_text(encoding="utf-8"))
    rows = ledger["registers"]
    if len(rows) != 127 or len({r["address"] for r in rows}) != 127:
        raise ValueError("expected the 127 distinct parameter-table addresses")
    for row in rows:
        if row["address_hex"] != f"0x{row['address']:04X}" or row["access"] not in ("R", "RW"):
            raise ValueError(f"invalid extracted row: {row['parameter']}")
        if not row["pdf_pages"] or any(page not in range(6, 11) for page in row["pdf_pages"]):
            raise ValueError("missing parameter-table provenance")
    vectors = json.loads((DOCS / "reference" / (PREFIX + "_vectors.json")).read_text(encoding="utf-8"))["vectors"]
    for vector in vectors:
        frame = bytes.fromhex(vector["published_hex"])
        calculated = crc16(frame[:-2])
        if calculated.hex(" ").upper() != vector["calculated_crc_hex"]:
            raise ValueError(f"CRC calculation differs: {vector['id']}")
        if (frame[-2:] == calculated) != vector["crc_matches"]:
            raise ValueError(f"CRC disposition differs: {vector['id']}")
        if vector["shape"] == "fc10_request":
            count = int.from_bytes(frame[4:6], "big")
            if frame[1] != 16 or frame[6] != 2 * count or len(frame) != 9 + 2 * count:
                raise ValueError(f"FC10 shape differs: {vector['id']}")
        if vector["shape"] == "fc03_reply" and (frame[1] != 3 or len(frame) != 5 + frame[2]):
            raise ValueError(f"FC03 shape differs: {vector['id']}")
    print(f"PASS: {len(records)} preserved sources, 127 parameter rows, {len(vectors)} frame checks; no hardware I/O")


if __name__ == "__main__":
    main()
