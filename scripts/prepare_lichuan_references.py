"""Verify the preserved LC57BK-R research bundle; optionally restore/extract PDFs.

Offline by default. --download-missing retrieves only missing manifest entries,
checks their pinned bytes before writing, and never replaces an existing file.
--extract regenerates page-indexed search aids; original PDFs remain authoritative.
This is a documentation tool, not a drive implementation or serial test program.
"""

import argparse
import json

from _lichuan_reference_tools import DOCS, check_page, check_sources, check_vectors


PREFIX = "lichuan_lc57bk_rs485"
MANIFEST = DOCS / "reference" / (PREFIX + "_sources.json")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Offline verification (also the default)")
    parser.add_argument("--download-missing", action="store_true")
    parser.add_argument("--extract", action="store_true")
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    records = manifest["references"]
    by_id = check_sources(records, 3, args.download_missing, args.extract)
    ledger = json.loads((DOCS / "reference" / (PREFIX + "_registers.json")).read_text(encoding="utf-8"))
    rows = ledger["registers"]
    if len(rows) != 127 or len({r["address"] for r in rows}) != 127 or ledger["register_count"] != 127:
        raise ValueError("expected the 127 distinct parameter-table addresses")
    for row in rows:
        if (type(row["address"]) is not int or not 0 <= row["address"] <= 0xFFFF
                or row["address_hex"] != f"0x{row['address']:04X}" or row["access"] not in ("R", "RW")):
            raise ValueError(f"invalid extracted row: {row['parameter']}")
        if not row["pdf_pages"] or any(page not in range(6, 11) for page in row["pdf_pages"]):
            raise ValueError("missing parameter-table provenance")
        for page in row["pdf_pages"]:
            check_page(row["source"], page, by_id)
    vectors = json.loads((DOCS / "reference" / (PREFIX + "_vectors.json")).read_text(encoding="utf-8"))["vectors"]
    check_vectors(vectors, by_id, 21, "CRC_VALID_DOCUMENT_EXAMPLE_NOT_HARDWARE_EVIDENCE", "REJECT_PUBLISHED_BYTES")
    models = json.loads((DOCS / "reference" / (PREFIX + "_models.json")).read_text(encoding="utf-8"))["models"]
    if len(models) != 8 or len({m["model"] for m in models}) != 8:
        raise ValueError("expected eight RS485 model candidates")
    print(f"PASS: {len(records)} preserved sources/extracts, 127 parameter rows, {len(vectors)} frame checks, 8 model candidates; no hardware I/O")


if __name__ == "__main__":
    main()
