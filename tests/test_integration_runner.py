import importlib.util
import shutil
import struct
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parent.parent
RUNNER_PATH = ROOT / "tests" / "run_integration_tests.py"
SPEC = importlib.util.spec_from_file_location("integration_runner", RUNNER_PATH)
integration_runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(integration_runner)


class IntegrationRunnerTests(unittest.TestCase):
    @unittest.skipUnless(
        shutil.which("as") and shutil.which("objcopy"),
        "GNU assembler and objcopy are required",
    )
    def test_x86_framebuffer_request_matches_build_mode(self):
        version = subprocess.run(
            ["as", "--version"], capture_output=True, text=True, check=True
        )
        if "GNU assembler" not in version.stdout:
            self.skipTest("GNU assembler is required")

        with tempfile.TemporaryDirectory() as directory:
            obj = str(Path(directory) / "boot.o")
            header = Path(directory) / "header.bin"
            for flags, expected in (
                ([], (800, 600, 32)),
                (["--defsym", "INTEGRATION_TESTS=1"], (1024, 768, 32)),
            ):
                with self.subTest(expected=expected):
                    subprocess.run(
                        [
                            "as",
                            "--32",
                            *flags,
                            str(ROOT / "src/Platform/x86/platform_boot.S"),
                            "-o",
                            obj,
                        ],
                        check=True,
                    )
                    subprocess.run(
                        [
                            "objcopy",
                            "-O",
                            "binary",
                            "-j",
                            ".multiboot2",
                            obj,
                            str(header),
                        ],
                        check=True,
                    )
                    data = header.read_bytes()
                    self.assertEqual(sum(struct.unpack_from("<4I", data)) % 2**32, 0)
                    self.assertEqual(struct.unpack_from("<3I", data, 24), expected)

    def test_build_cleans_and_builds_in_separate_make_processes(self):
        runner = integration_runner.TestRunner(str(ROOT))
        results = [
            subprocess.CompletedProcess(["make", "clean"], 0, "", ""),
            subprocess.CompletedProcess(["make", "iso"], 0, "", ""),
        ]

        with patch("subprocess.run", side_effect=results) as run:
            self.assertTrue(runner.build_kernel())

        self.assertEqual(run.call_count, 2)
        self.assertEqual(run.call_args_list[0].args[0][-1], "clean")
        self.assertEqual(
            run.call_args_list[1].args[0][-2:], ["INTEGRATION_TESTS=1", "iso"]
        )

    def test_build_failure_includes_captured_diagnostics(self):
        runner = integration_runner.TestRunner(str(ROOT))
        results = [
            subprocess.CompletedProcess(["make", "clean"], 0, "", ""),
            subprocess.CompletedProcess(
                ["make", "iso"], 2, "compile output", "compiler error"
            ),
        ]

        with (
            patch("subprocess.run", side_effect=results),
            patch("builtins.print") as output,
        ):
            self.assertFalse(runner.build_kernel())

        printed = [call.args[0] for call in output.call_args_list]
        self.assertTrue(any("compile output" in message for message in printed))
        self.assertTrue(any("compiler error" in message for message in printed))

    def test_duplicate_failure_records_match_summary_once(self):
        runner = integration_runner.TestRunner(str(ROOT))
        runner.qemu_output = """✓ PASS: first
✗ FAIL: second: failed assertion
Total tests: 2
Passed:      1
Failed:      1
✗ FAIL: [second] failed assertion"""

        self.assertTrue(runner.parse_test_results())
        self.assertEqual(len(runner.results), 2)
        self.assertEqual(sum(not result.passed for result in runner.results), 1)

    def test_success_summary_marker_is_not_counted_as_a_test(self):
        runner = integration_runner.TestRunner(str(ROOT))
        runner.qemu_output = """✓ PASS: first
✓ PASS: ALL TESTS PASSED!
Total tests: 1
Passed:      1
Failed:      0"""

        self.assertTrue(runner.parse_test_results())
        self.assertEqual(len(runner.results), 1)
        self.assertTrue(runner.results[0].passed)

    def test_missing_or_inconsistent_summary_fails(self):
        runner = integration_runner.TestRunner(str(ROOT))
        runner.qemu_output = "✓ PASS: incomplete"
        self.assertFalse(runner.parse_test_results())

        runner.qemu_output = """✓ PASS: first
Total tests: 2
Passed:      2
Failed:      0"""
        self.assertFalse(runner.parse_test_results())

    def test_missing_summary_includes_bounded_boot_diagnostics(self):
        runner = integration_runner.TestRunner(str(ROOT))
        runner.qemu_output = "x" * 13000 + "framebuffer too small"

        with patch("builtins.print") as output:
            self.assertFalse(runner.parse_test_results())

        printed = [call.args[0] for call in output.call_args_list]
        self.assertIn(runner.qemu_output[-12000:], printed)
        self.assertNotIn(runner.qemu_output, printed)

    def test_timeout_preserves_and_parses_partial_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "system71.iso").touch()
            runner = integration_runner.TestRunner(str(root), timeout=1)
            output = b"""INTEGRATION TEST SUITE
\xe2\x9c\x93 PASS: timeout case
Total tests: 1
Passed:      1
Failed:      0"""

            with patch(
                "subprocess.run",
                side_effect=subprocess.TimeoutExpired(["qemu"], 1, output=output),
            ):
                self.assertTrue(runner.run_tests())

            self.assertTrue(runner.parse_test_results())
            self.assertEqual(runner.results[0].name, "timeout case")

    def test_qemu_nonzero_exit_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "system71.iso").touch()
            runner = integration_runner.TestRunner(str(root), timeout=1)
            result = subprocess.CompletedProcess(
                ["qemu"], returncode=1, stdout="", stderr="QEMU failed"
            )

            with patch("subprocess.run", return_value=result):
                self.assertFalse(runner.run_tests())

    def test_junit_report_escapes_values_and_reports_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "results.xml"
            runner = integration_runner.TestRunner(str(ROOT))
            runner.results = [
                integration_runner.TestResult("test <one>", True),
                integration_runner.TestResult(
                    "test two", False, "expected <x> & got <y>"
                ),
            ]

            self.assertTrue(runner.generate_junit_report(str(output)))
            root = ET.parse(output).getroot()
            self.assertEqual(root.attrib["tests"], "2")
            self.assertEqual(root.attrib["failures"], "1")
            failure = root.find("./testsuite/testcase/failure")
            self.assertEqual(failure.attrib["message"], "expected <x> & got <y>")

    def test_junit_report_failure_is_returned_to_caller(self):
        runner = integration_runner.TestRunner(str(ROOT))
        with patch(
            "xml.etree.ElementTree.ElementTree.write",
            side_effect=OSError("write failed"),
        ):
            self.assertFalse(runner.generate_junit_report("unwritable.xml"))


if __name__ == "__main__":
    unittest.main()
