"""Shared offline mechanics for the two preserved Lichuan research bundles.

These checks establish source integrity and frame syntax, never drive semantics
or permission to transmit. Profile-specific evidence remains in each checker.
"""

import hashlib
from pathlib import Path
import re

import fitz
import requests


DOCS = Path(__file__).resolve().parents[1] / "docs"


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


def check_extract(record, data, regenerate=False):
    destination = checked_path(record["text_extract"])
    with fitz.open(stream=data, filetype="pdf") as pdf:
        pages = [page.get_text(sort=True).strip() for page in pdf]
    if regenerate:
        content = [f"# {Path(record['path']).name}\n\nPhysical PDF page numbers start at 1.\n"
                   "Search aid only: tables may be reordered; inspect the original PDF.\n"]
        content += [f"\n## PDF page {number}\n\n{text}\n" for number, text in enumerate(pages, 1)]
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text("\n".join(content), encoding="utf-8", newline="\n")
    if not destination.is_file():
        raise ValueError(f"missing extract: {destination}")
    parts = re.split(r"^## PDF page (\d+)\n\n", destination.read_text(encoding="utf-8"), flags=re.MULTILINE)
    if (parts[1::2] != [str(n) for n in range(1, len(pages) + 1)]
            or [part.strip() for part in parts[2::2]] != pages):
        raise ValueError(f"extract pages/text differ from PDF: {destination}")


def check_sources(records, expected_count, download_missing=False, extract=False):
    paths = [checked_path(record["path"]) for record in records]
    ids = [record["id"] for record in records]
    if len(records) != expected_count or len(set(paths)) != expected_count or len(set(ids)) != expected_count:
        raise ValueError(f"expected {expected_count} distinct original sources and IDs")
    extracts = [checked_path(r["text_extract"]) for r in records if r.get("text_extract")]
    # Validate all destinations before any --extract write can damage an original.
    if len(set(extracts)) != len(extracts) or set(paths) & set(extracts):
        raise ValueError("extract paths overlap each other or preserved sources")
    for record, path in zip(records, paths):
        if record.get("text_extract") and record["format"] != "pdf":
            raise ValueError("only PDF sources can have text extracts")
        if not path.exists() and download_missing:
            # Stable origin URLs obtain fresh CDN redirects; recorded signatures expire.
            response = requests.get(record["url"], headers={
                "User-Agent": "Mozilla/5.0", "Referer": record["referer"]}, timeout=(15, 60))
            response.raise_for_status()
            data = response.content
            check_bytes(record, data)
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("xb") as output:
                output.write(data)
        data = path.read_bytes()
        check_bytes(record, data)
        if record.get("text_extract"):
            check_extract(record, data, extract)
        for page in record.get("visually_reviewed_pdf_pages", []):
            check_page(record["id"], page, {record["id"]: record})
    return dict(zip(ids, records))


def check_page(source_id, page, by_id):
    source = by_id.get(source_id)
    if source is None or source.get("format") != "pdf" or type(page) is not int or not 1 <= page <= source["pages"]:
        raise ValueError(f"invalid PDF provenance: {source_id} page {page}")


def crc16(payload):
    crc = 0xFFFF
    for value in payload:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc.to_bytes(2, "little")


def valid_shape(frame, shape):
    # The LC bundle's original labels are aliases, not alternate wire formats.
    shape = {"fc06": "fc06_request_or_echo", "invalid_exception": "exception"}.get(shape, shape)
    if shape not in {"exception", "fc03_reply", "fc10_request", "fc03_request", "fc06_request_or_echo", "fc10_reply"}:
        raise ValueError(f"unknown frame shape: {shape}")
    if not 5 <= len(frame) <= 256:
        return False
    if shape == "exception":
        return len(frame) == 5 and frame[1] in (0x83, 0x86, 0x90)
    if shape == "fc03_reply":
        return frame[1] == 3 and frame[2] > 0 and frame[2] % 2 == 0 and len(frame) == 5 + frame[2]
    if shape == "fc10_request":
        return (len(frame) >= 9 and frame[1] == 16 and frame[6] > 0
                and frame[6] == 2 * int.from_bytes(frame[4:6], "big")
                and len(frame) == 9 + frame[6])
    functions = {"fc03_request": 3, "fc06_request_or_echo": 6, "fc10_reply": 16}
    return (len(frame) == 8 and frame[1] == functions[shape]
            and (shape == "fc06_request_or_echo" or int.from_bytes(frame[4:6], "big") > 0))


def check_vectors(vectors, by_id, expected_count, valid_disposition, rejected_disposition):
    frames = [bytes.fromhex(vector["published_hex"]) for vector in vectors]
    if (len(vectors) != expected_count or len(set(frames)) != expected_count
            or len({vector["id"] for vector in vectors}) != expected_count):
        raise ValueError(f"expected {expected_count} distinct published frames and IDs")
    for vector, frame in zip(vectors, frames):
        if len(frame) < 5 or ("bytes" in vector and vector["bytes"] != len(frame)):
            raise ValueError(f"frame byte count differs: {vector['id']}")
        crc = crc16(frame[:-2])
        matches = crc == frame[-2:]
        if crc.hex(" ").upper() != vector["calculated_crc_hex"] or matches != vector["crc_matches"]:
            raise ValueError(f"CRC disposition differs: {vector['id']}")
        shape_valid = valid_shape(frame, vector["shape"])
        if "shape_valid" in vector and shape_valid != vector["shape_valid"]:
            raise ValueError(f"shape disposition differs: {vector['id']}")
        disposition = valid_disposition if matches and shape_valid else rejected_disposition
        if vector["disposition"] != disposition:
            raise ValueError(f"invalid published-frame disposition: {vector['id']}")
        if "sources" in vector:
            sources = vector["sources"]
        else:
            sources = [{"source_id": vector["source"], "pdf_page": vector["pdf_page"]}]
        if not sources:
            raise ValueError(f"missing frame provenance: {vector['id']}")
        for source in sources:
            check_page(source["source_id"], source["pdf_page"], by_id)
