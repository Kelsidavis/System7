import tempfile
import unittest
from pathlib import Path

from scripts.check_markdown_links import broken_links, broken_repo_paths


class MarkdownReferenceTests(unittest.TestCase):
    def test_existing_relative_and_nonlocal_links_are_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs = root / "docs"
            docs.mkdir()
            (root / "README.md").write_text("# Project\n")
            (docs / "guide.md").write_text(
                "# Intro\n\n[readme](../README.md) [heading](#intro) "
                "[site](https://example.com) [mail](mailto:test@example.com)\n"
            )

            self.assertEqual(list(broken_links(root)), [])

    def test_heading_fragments_match_github_style_slugs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = root / "guide.md"
            document.write_text(
                "# Heading with `code` & punctuation!\n"
                "## Repeat\n## Repeat\n"
                "Setext heading\n-------------\n"
                "[valid](#heading-with-code-punctuation) "
                "[duplicate](#repeat-1) [setext](#setext-heading)\n"
            )

            self.assertEqual(list(broken_links(root)), [])

    def test_missing_heading_fragments_are_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = root / "guide.md"
            document.write_text("# Present\n[missing](#absent)\n")

            self.assertEqual(list(broken_links(root)), [(document, "#absent")])

    def test_headings_in_fenced_code_are_not_fragments(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = root / "guide.md"
            document.write_text("```md\n# Example\n```\n[missing](#example)\n")

            self.assertEqual(list(broken_links(root)), [(document, "#example")])

    def test_missing_relative_link_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "guide.md").write_text("[missing](missing.md)\n")

            self.assertEqual(
                list(broken_links(root)), [(root / "guide.md", "missing.md")]
            )

    def test_reference_style_links_are_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "README.md").touch()
            document = root / "guide.md"
            document.write_text(
                "[readme][project]\n\n"
                "[project]: README.md\n"
                "[website]: https://example.com\n"
            )

            self.assertEqual(list(broken_links(root)), [])

    def test_missing_reference_style_link_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = root / "guide.md"
            document.write_text("[missing]: absent.md\n")

            self.assertEqual(list(broken_links(root)), [(document, "absent.md")])

    def test_links_in_fenced_code_are_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "guide.md").write_text("```markdown\n[example](missing.md)\n```\n")

            self.assertEqual(list(broken_links(root)), [])

    def test_links_in_inline_code_are_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "guide.md").write_text("Use `[example](missing.md)` literally.\n")

            self.assertEqual(list(broken_links(root)), [])

    def test_url_encoded_path_is_resolved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "image file.png").touch()
            (root / "guide.md").write_text("[image](image%20file.png)\n")

            self.assertEqual(list(broken_links(root)), [])

    def test_existing_inline_repository_path_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "src").mkdir()
            (root / "src" / "module.c").touch()
            (root / "guide.md").write_text("See `src/module.c` for details.\n")

            self.assertEqual(list(broken_repo_paths(root)), [])

    def test_missing_inline_repository_path_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = root / "guide.md"
            document.write_text("The implementation is in `src/missing.c`.\n")

            self.assertEqual(
                list(broken_repo_paths(root)), [(document, "src/missing.c")]
            )

    def test_repository_paths_in_fenced_code_are_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "guide.md").write_text("```text\n`src/example.c`\n```\n")

            self.assertEqual(list(broken_repo_paths(root)), [])


if __name__ == "__main__":
    unittest.main()
