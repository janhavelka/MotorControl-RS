# Lichuan TL57-R original references

Collected 23 October 2026 for the requested TLO57R/TLC57R L-type RS485 series.
Research is complete for these retained sources; implementation and physical
qualification have not started. This is a separate family from LC57BK-R/RC.

| ID | Original snapshot | Pages | Use |
| --- | --- | --- | --- |
| TL-USER | [User manual, Chinese](Lichuan_TL57-R_User_Manual_CN.pdf) | 26 | Hardware, wiring, registers, motion and protocol |
| TL-OP | [Operation manual, Chinese](Lichuan_TL57-R_Operation_Manual_CN.pdf) | 18 | Worked positive/negative moves, commissioning and control examples |
| TL-EX | [Serial examples, Chinese](Lichuan_TL-R_Serial_Examples_CN.pdf) | 12 | Exact frames, persistence notes and conflicting I/O examples |
| TL-WEB | [Manufacturer product page](Lichuan_TL57-R_Product_Page_2026-10-23.html) | — | Six model variants, physical specifications and original download links |

All three PDFs were linked by the [manufacturer's TL57-R product page](https://www.xlichuan.com/product-xq/175.html).
The [manifest](../../reference/lichuan_tl57_rs485_sources.json) records stable
source URLs, resolved CDN URLs, retrieval times, byte counts, SHA-256 hashes and
the exact physical PDF pages visually reviewed. Signed CDN redirects may expire;
the restore tool uses the stable original URL and still requires identical bytes.
No revision identifier was established in the PDFs. Do not equate download date
with firmware compatibility. Preserve any later revision as a separate snapshot.

The AliExpress page could not be fetched directly. The six user-supplied images
were reviewed as listing evidence, not saved or represented as downloaded originals.
The indexed English TL-R manual URL returned HTTP 404; it is recorded as unavailable.
These three Chinese documents provide the retained family-specific source bundle.
No commissioning software was executed, and no motor traffic was sent.

Read the [English engineering review](../../reference/14_lichuan_tl57_rs485.md)
and [implementation promptset](../../prompts/lichuan_tl57_rs485/README.md).
Page-indexed extracts under [pdf-extracted-md](../../pdf-extracted-md/README.md)
are search aids; merged tables and diagrams require the original PDF.

```sh
python scripts/prepare_lichuan_tl57_references.py --check
python scripts/prepare_lichuan_tl57_references.py --extract
python scripts/prepare_lichuan_tl57_references.py --download-missing
```

Commands run from the repository root. Offline checking is the default. Existing
snapshots are never overwritten by download; changed bytes fail verification.
Vendor documents retain their owners' rights and are not relicensed under the
library's license or included in its exported core package.
