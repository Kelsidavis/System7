import contextlib
import io
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from scripts.check_public_headers import check_headers, compiler_command

ROOT = Path(__file__).resolve().parent.parent


class PublicHeaderTests(unittest.TestCase):
    def test_compiler_command_preserves_arguments_and_rejects_empty_commands(self):
        self.assertEqual(compiler_command("cc -Werror", "--cc"), ["cc", "-Werror"])
        with self.assertRaises(ValueError):
            compiler_command("  ", "--cc")

    def check_fixture(self, contents, language, standard):
        variable = "HOST_CXX" if language == "c++" else "HOST_CC"
        fallback = "c++" if language == "c++" else "cc"
        compiler = compiler_command(os.environ.get(variable, fallback), variable)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            include = root / "include"
            include.mkdir()
            header = include / "fixture.h"
            header.write_text(contents)
            with contextlib.redirect_stderr(io.StringIO()):
                return check_headers(root, compiler, language, standard, [header])

    def test_macro_only_headers_are_valid_translation_units(self):
        self.assertEqual(self.check_fixture("#define VALUE 1\n", "c", "gnu11"), 0)

    def test_header_warnings_are_errors(self):
        self.assertEqual(
            self.check_fixture(
                "static inline int fixture(void) { int unused; return 0; }\n",
                "c",
                "gnu11",
            ),
            1,
        )

    def test_cpp_extensions_are_rejected(self):
        self.assertEqual(
            self.check_fixture(
                "struct Fixture { struct { int value; }; };\n", "c++", "c++17"
            ),
            1,
        )

    def test_static_assertions_enforce_conditions_in_each_supported_language(self):
        for language, standard, variable, fallback in (
            ("c", "c99", "HOST_CC", "cc"),
            ("c", "c11", "HOST_CC", "cc"),
            ("c++", "c++17", "HOST_CXX", "c++"),
        ):
            compiler = compiler_command(os.environ.get(variable, fallback), variable)
            for condition, expected in (("sizeof(int) > 0", 0), ("0", 1)):
                with self.subTest(language=language, condition=condition):
                    source = (
                        '#include "StaticAssert.h"\n'
                        '#include "StaticAssert.h"\n'
                        f"SYSTEM7_STATIC_ASSERT({condition}, fixture_condition);\n"
                    )
                    result = subprocess.run(
                        [
                            *compiler,
                            f"-std={standard}",
                            "-pedantic-errors",
                            "-fsyntax-only",
                            f"-I{ROOT / 'include'}",
                            "-x",
                            language,
                            "-",
                        ],
                        input=source,
                        text=True,
                        capture_output=True,
                        check=False,
                    )
                    self.assertEqual(
                        bool(result.returncode), bool(expected), result.stderr
                    )


if __name__ == "__main__":
    unittest.main()
