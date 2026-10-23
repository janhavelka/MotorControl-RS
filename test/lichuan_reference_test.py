"""Offline regressions for source restoration and research-vector validation."""

import copy
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

import fitz

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import _lichuan_reference_tools as checker


class SourceChecks(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.docs = Path(directory.name)
        docs_patch = patch.object(checker, "DOCS", self.docs)
        docs_patch.start()
        self.addCleanup(docs_patch.stop)
        with fitz.open() as pdf:
            for text in ("First source page", "Second source page"):
                page = pdf.new_page()
                page.insert_text((40, 40), text)
            self.data = pdf.tobytes()
        self.record = {
            "id": "MANUAL", "path": "vendor/manual.pdf", "format": "pdf", "pages": 2,
            "bytes": len(self.data), "sha256": hashlib.sha256(self.data).hexdigest(),
            "url": "https://origin.invalid/manual.pdf",
            "resolved_url": "https://cdn.invalid/manual.pdf?expired_signature=1",
            "referer": "https://origin.invalid/",
        }
        self.path = self.docs / self.record["path"]

    def write_snapshot(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_bytes(self.data)

    def test_restore_uses_stable_origin_and_preserves_existing_snapshot(self):
        response = Mock(content=self.data)
        with patch.object(checker.requests, "get", return_value=response) as get:
            checker.check_sources([self.record], 1, download_missing=True)
            self.assertEqual(get.call_args.args[0], self.record["url"])
            self.assertEqual(self.path.read_bytes(), self.data)
            get.reset_mock()
            checker.check_sources([self.record], 1, download_missing=True)
            get.assert_not_called()

    def test_changed_download_is_never_saved(self):
        with patch.object(checker.requests, "get", return_value=Mock(content=b"changed")):
            with self.assertRaisesRegex(ValueError, "snapshot differs"):
                checker.check_sources([self.record], 1, download_missing=True)
        self.assertFalse(self.path.exists())

    def test_changed_existing_file_is_not_replaced(self):
        self.write_snapshot()
        self.path.write_bytes(b"changed existing snapshot")
        with patch.object(checker.requests, "get") as get:
            with self.assertRaisesRegex(ValueError, "snapshot differs"):
                checker.check_sources([self.record], 1, download_missing=True)
            get.assert_not_called()
        self.assertEqual(self.path.read_bytes(), b"changed existing snapshot")

    def test_incorrect_pdf_page_count_rejected(self):
        self.write_snapshot()
        self.record["pages"] = 1
        with self.assertRaisesRegex(ValueError, "page count differs"):
            checker.check_sources([self.record], 1)

    def test_extract_rejects_missing_pages_labels_and_changed_text(self):
        self.write_snapshot()
        self.record["text_extract"] = "extract/manual.md"
        checker.check_sources([self.record], 1, extract=True)
        destination = self.docs / self.record["text_extract"]
        expected = destination.read_text(encoding="utf-8")
        for broken in ("", expected.split("## PDF page 2")[0],
                       expected.replace("## PDF page 2", "## PDF page 3"),
                       expected.replace("First source page", "Invented source page")):
            with self.subTest(broken=broken):
                destination.write_text(broken, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "extract pages/text differ"):
                    checker.check_sources([self.record], 1)
                self.assertEqual(self.path.read_bytes(), self.data)
        # Platform newline conversion is not evidence corruption.
        destination.write_bytes(expected.replace("\n", "\r\n").encode("utf-8"))
        checker.check_sources([self.record], 1)

    def test_extract_cannot_overwrite_an_original(self):
        self.write_snapshot()
        self.record["text_extract"] = self.record["path"]
        with self.assertRaisesRegex(ValueError, "overlap"):
            checker.check_sources([self.record], 1, extract=True)
        self.assertEqual(self.path.read_bytes(), self.data)

    def test_paths_cannot_escape_docs(self):
        self.record["path"] = "../outside.pdf"
        with self.assertRaisesRegex(ValueError, "escapes docs"):
            checker.check_sources([self.record], 1, download_missing=True)

    def test_duplicate_source_ids_and_path_aliases_are_rejected(self):
        for field, value in (("id", "MANUAL"), ("path", "vendor/../vendor/manual.pdf")):
            second = dict(self.record, id="SECOND", path="vendor/second.pdf")
            second[field] = value
            with self.subTest(field=field):
                with self.assertRaisesRegex(ValueError, "distinct original"):
                    checker.check_sources([self.record, second], 2)


class VectorChecks(unittest.TestCase):
    by_id = {"MANUAL": {"format": "pdf", "pages": 20}}

    def vector(self, payload="01 06 03 F0 00 01", shape="fc06"):
        data = bytes.fromhex(payload)
        crc = checker.crc16(data)
        return {"id": "VECTOR", "published_hex": (data + crc).hex(" ").upper(),
                "calculated_crc_hex": crc.hex(" ").upper(), "crc_matches": True,
                "shape": shape, "bytes": len(data) + 2, "disposition": "VALID",
                "source": "MANUAL", "pdf_page": 15}

    def check(self, vectors):
        checker.check_vectors(vectors, self.by_id, len(vectors), "VALID", "REJECT")

    def test_known_crc_and_valid_shapes(self):
        self.assertEqual(checker.crc16(b"123456789"), bytes.fromhex("37 4B"))
        for payload, shape in (("01 03 00 04 00 01", "fc03_request"),
                               ("01 03 02 00 01", "fc03_reply"),
                               ("01 06 03 F0 00 01", "fc06"),
                               ("01 10 26 01 00 02 04 75 30 00 00", "fc10_request"),
                               ("01 10 26 01 00 02", "fc10_reply"),
                               ("01 83 02", "exception")):
            with self.subTest(shape=shape):
                self.check([self.vector(payload, shape)])

    def test_crc_valid_malformed_shapes_cannot_be_accepted(self):
        cases = (("01 03 00 04 00 00", "fc03_request"),
                 ("01 03 00", "fc03_reply"), ("01 03 01 00", "fc03_reply"),
                 ("01 06 03 F0 00", "fc06"),
                 ("01 03 03 F0 00 01", "fc06"),
                 ("01 10 26 01 00 00 00", "fc10_request"),
                 ("01 10 26 01 00 02 02 00 00", "fc10_request"),
                 ("01 10 26 01 00 00", "fc10_reply"),
                 ("01 06 04", "invalid_exception"))
        for payload, shape in cases:
            with self.subTest(payload=payload, shape=shape):
                vector = self.vector(payload, shape)
                with self.assertRaisesRegex(ValueError, "disposition"):
                    self.check([vector])
                vector["disposition"] = "REJECT"
                self.check([vector])

    def test_published_bad_crc_retained_only_with_explicit_rejection(self):
        vector = self.vector("01 06 04", "invalid_exception")
        vector.update(published_hex="01 06 04 10 00", crc_matches=False, disposition="REJECT")
        self.check([vector])
        vector["disposition"] = "VALID"
        with self.assertRaisesRegex(ValueError, "disposition"):
            self.check([vector])

    def test_byte_count_unknown_shape_and_invented_disposition_rejected(self):
        for key, value in (("bytes", 9), ("shape", "unknown"), ("disposition", "HARDWARE_PROVEN"),
                           ("shape_valid", False), ("crc_matches", False)):
            with self.subTest(key=key):
                vector = self.vector()
                vector[key] = value
                with self.assertRaises(ValueError):
                    self.check([vector])

    def test_duplicate_vector_ids_and_byte_equivalent_frames_rejected(self):
        original = self.vector()
        changed = self.vector("01 06 03 F1 00 01")
        with self.assertRaisesRegex(ValueError, "distinct published"):
            self.check([original, changed])
        changed = copy.deepcopy(original)
        changed.update(id="SECOND", published_hex=original["published_hex"].lower().replace(" ", ""))
        with self.assertRaisesRegex(ValueError, "distinct published"):
            self.check([original, changed])

    def test_missing_or_invalid_provenance_rejected_in_both_schemas(self):
        for edits in ({"source": "UNKNOWN"}, {"pdf_page": 21}, {"sources": []},
                      {"sources": [{"source_id": "MANUAL", "pdf_page": 0}]}):
            with self.subTest(edits=edits):
                vector = self.vector()
                vector.update(edits)
                with self.assertRaises(ValueError):
                    self.check([vector])


if __name__ == "__main__":
    unittest.main()
