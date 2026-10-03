import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parent.parent
RUNNER_PATH = ROOT / "tests" / "run_integration_tests.py"
SPEC = importlib.util.spec_from_file_location("integration_runner", RUNNER_PATH)
integration_runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(integration_runner)


class IntegrationRunnerTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
