import tempfile
import unittest
from pathlib import Path

from scripts.check_markdown_links import broken_links


class MarkdownLinkTests(unittest.TestCase):
    def test_existing_relative_and_nonlocal_links_are_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs = root / "docs"
            docs.mkdir()
            (root / "README.md").write_text("# Project\n")
            (docs / "guide.md").write_text(
                "[readme](../README.md) [heading](#intro) "
                "[site](https://example.com) [mail](mailto:test@example.com)\n"
            )

            self.assertEqual(list(broken_links(root)), [])

    def test_missing_relative_link_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "guide.md").write_text("[missing](missing.md)\n")

            self.assertEqual(
                list(broken_links(root)), [(root / "guide.md", "missing.md")]
            )

    def test_links_in_fenced_code_are_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "guide.md").write_text("```markdown\n[example](missing.md)\n```\n")

            self.assertEqual(list(broken_links(root)), [])

    def test_url_encoded_path_is_resolved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "image file.png").touch()
            (root / "guide.md").write_text("[image](image%20file.png)\n")

            self.assertEqual(list(broken_links(root)), [])


if __name__ == "__main__":
    unittest.main()
