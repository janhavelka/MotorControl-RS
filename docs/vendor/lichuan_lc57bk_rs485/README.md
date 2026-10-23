# Lichuan LC57BK-R / RC original references

Collected 2026-10-23 for the [RS485 implementation promptset](../../prompts/lichuan_lc57bk_rs485/README.md).
These files establish documentation evidence, not implemented or tested support.

| ID | Original snapshot | Purpose |
| --- | --- | --- |
| LC-USER | [LC57BK-R user manual](Lichuan_LC57BK-R_User_Manual_V1.1_CN.pdf), 20 pages | V1.1 / 20250517 in the manufacturer download filename; electrical/mechanical data, pinout, parameters, operations and RTU frames |
| LC-OP | [LC57BK-R operation manual](Lichuan_LC57BK-R_Operation_Manual_CN.pdf), 7 pages | Commissioning, wiring and command sequences; no explicit document revision |
| LC-PRODUCT | [Manufacturer product-page snapshot](Lichuan_LC57BK-R_Product_Page_2026-10-23.html) | Records the current model/encoder claims that conflict with the listing and manual |

The [manifest](../../reference/lichuan_lc57bk_rs485_sources.json) retains download
URLs, resolved URLs, retrieval times, sizes, SHA-256 hashes and visually checked
physical PDF pages. The manuals are Chinese originals. An exact-family English
manual was not obtained. Searchable extracts and an English engineering review
are provided; neither replaces the original tables and figures.

The AliExpress URL was not retrievable through the web tool. The user's four
screenshots identify the requested eight R/RC variants; their transcribed values
are explicitly marked as listing claims. The optional English sales selection
poster did not finish downloading within the bounded attempts; no partial PDF
is committed or used as a specification. These limitations are in the manifest.

Use [the extracted engineering review](../../reference/13_lichuan_lc57bk_rs485.md)
before using any register or model value. In particular, the status-word address,
encoder terminology, PA labels and signed position representation need attention.

Offline verification:

```sh
python scripts/prepare_lichuan_references.py --check
```

Regenerate search extracts with `--extract`. `--download-missing` retrieves only
missing snapshots and accepts only the pinned bytes. It never replaces existing
files. Downloads require a normal browser User-Agent and the manufacturer
Referer for the linked file host. A changed upstream document requires a reviewed
new snapshot and manifest entry, not silent replacement.

Original documents and page content retain their owners' rights. They are
commissioning/research references, excluded from the MIT core distribution.
No vendor software was installed or executed.
