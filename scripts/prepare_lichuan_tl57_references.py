"""Check the TL57-R research bundle offline; optionally restore or extract sources.

This documentation tool performs no serial I/O. Downloads restore missing files
only, verify pinned bytes before writing, and never replace existing snapshots.
"""

import argparse
import json

from _lichuan_reference_tools import DOCS, check_page, check_sources, check_vectors


PREFIX = "lichuan_tl57_rs485"


def read_data(suffix):
    return json.loads((DOCS / "reference" / f"{PREFIX}_{suffix}.json").read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Offline check (also the default)")
    parser.add_argument("--download-missing", action="store_true")
    parser.add_argument("--extract", action="store_true")
    args = parser.parse_args()
    records = read_data("sources")["references"]
    by_id = check_sources(records, 4, args.download_missing, args.extract)
    ledger = read_data("registers")
    rows = ledger["registers"]
    by_address = {r["address"]: r for r in rows}
    addresses = set(by_address)
    if len(rows) != 203 or len(addresses) != 203:
        raise ValueError("expected 203 distinct documented register words")
    if ledger["missing_addresses_through_0x0119"] != [a for a in range(282) if a not in addresses]:
        raise ValueError("gap ledger differs")
    for row in rows:
        if (type(row["address"]) is not int or not 0 <= row["address"] <= 0x0119
                or row["address_hex"] != f"0x{row['address']:04X}" or not row["name_en"]):
            raise ValueError("invalid register transcription")
        if not row["pdf_pages"] or any(p not in range(15, 24) for p in row["pdf_pages"]):
            raise ValueError("invalid register-table page")
        for page in row["pdf_pages"]:
            check_page(row["source_id"], page, by_id)
        if "pair_start" in row:
            start = row["pair_start"]
            if (row["address"] not in (start, start + 1)
                    or any(by_address.get(a, {}).get("pair_start") != start for a in (start, start + 1))):
                raise ValueError("incomplete or inconsistent documented pair")
    vectors = read_data("vectors")["vectors"]
    check_vectors(vectors, by_id, 52, "WIRE_VALID_REVIEW_SEMANTICS", "REJECT_PUBLISHED_FRAME")
    models = read_data("models")["models"]
    if len(models) != 6 or len({m["model"] for m in models}) != 6:
        raise ValueError("expected six RS485 model candidates")
    print("PASS: 4 preserved sources/extracts, 203 register words, 52 frame checks, 6 model candidates; no hardware I/O")


if __name__ == "__main__":
    main()
