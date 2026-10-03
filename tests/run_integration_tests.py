#!/usr/bin/env python3
"""
System 7 Integration Test Runner

Automated test execution and result parsing for Phase 1 integration tests.
- Builds kernel with INTEGRATION_TESTS=1
- Runs QEMU and captures serial output
- Parses test results from log output
- Reports pass/fail status
- Can be integrated into CI/CD pipelines

Usage:
    python3 tests/run_integration_tests.py                  # Run all tests
    python3 tests/run_integration_tests.py --timeout 60      # Custom timeout
    python3 tests/run_integration_tests.py --output junit.xml  # JUnit XML output
"""

import sys
import subprocess
import re
import argparse
from pathlib import Path
from typing import List
from datetime import datetime

class TestResult:
    def __init__(self, name: str, passed: bool, reason: str = ""):
        self.name = name
        self.passed = passed
        self.reason = reason

    def __str__(self):
        status = "✓ PASS" if self.passed else "✗ FAIL"
        return f"{status}: {self.name}" + (f" ({self.reason})" if self.reason else "")

class TestRunner:
    def __init__(self, project_root: str, timeout: int = 120, verbose: bool = False):
        self.project_root = Path(project_root)
        self.timeout = timeout
        self.verbose = verbose
        self.results: List[TestResult] = []
        self.qemu_output = ""

    def log(self, msg: str, level: str = "INFO"):
        """Log message with timestamp"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        prefix = {
            "INFO": "[*]",
            "PASS": "[+]",
            "FAIL": "[!]",
            "WARN": "[~]",
            "DEBUG": "[D]"
        }.get(level, "[?]")
        print(f"{prefix} [{timestamp}] {msg}")

    def build_kernel(self) -> bool:
        """Build kernel and ISO with integration tests enabled"""
        self.log("Building kernel with INTEGRATION_TESTS=1...", "INFO")

        build_cmd = [
            "make", "-C", str(self.project_root),
            "INTEGRATION_TESTS=1", "clean", "iso"
        ]

        try:
            result = subprocess.run(
                build_cmd,
                capture_output=True,
                text=True,
                timeout=600,
                cwd=str(self.project_root)
            )

            if result.returncode != 0:
                self.log("Build failed!", "FAIL")
                if self.verbose:
                    self.log("Build stderr:", "DEBUG")
                    print(result.stderr)
                return False

            self.log("Build successful", "PASS")
            return True

        except subprocess.TimeoutExpired:
            self.log("Build timeout after 600 seconds", "FAIL")
            return False
        except Exception as e:
            self.log(f"Build error: {e}", "FAIL")
            return False

    def run_tests(self) -> bool:
        """Run QEMU with serial output capture"""
        self.log("Starting QEMU for integration testing...", "INFO")

        iso_path = self.project_root / "system71.iso"
        if not iso_path.exists():
            self.log(f"ISO not found: {iso_path}", "FAIL")
            return False

        qemu_cmd = [
            "qemu-system-i386",
            "-cdrom", str(iso_path),
            "-m", "512",
            "-serial", "stdio",
            "-nographic",
            "-monitor", "none"
        ]

        try:
            result = subprocess.run(
                qemu_cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout,
                cwd=str(self.project_root)
            )

            self.qemu_output = result.stdout + result.stderr
            self.log(f"QEMU execution completed (timeout: {self.timeout}s)", "INFO")

            if result.returncode != 0:
                self.log(f"QEMU exited with status {result.returncode}", "FAIL")
                return False

            if self.verbose:
                self.log("QEMU output (first 2000 chars):", "DEBUG")
                print(self.qemu_output[:2000])

            return True

        except subprocess.TimeoutExpired as error:
            stdout = error.stdout or ""
            stderr = error.stderr or ""
            if isinstance(stdout, bytes):
                stdout = stdout.decode(errors="replace")
            if isinstance(stderr, bytes):
                stderr = stderr.decode(errors="replace")
            self.qemu_output = stdout + stderr
            self.log(f"QEMU timeout after {self.timeout} seconds", "WARN")
            return True  # Still parse whatever output we got
        except Exception as e:
            self.log(f"QEMU execution error: {e}", "FAIL")
            return False

    def parse_test_results(self) -> bool:
        """Parse test results from QEMU output"""
        self.log("Parsing test results...", "INFO")

        pass_matches = re.findall(r"✓ PASS: ([^\r\n]+)", self.qemu_output)
        pass_names = []
        seen_passes = set()
        for name in pass_matches:
            name = name.strip()
            if name != "ALL TESTS PASSED!" and name not in seen_passes:
                pass_names.append(name)
                seen_passes.add(name)

        fail_matches = re.findall(r"✗ FAIL: ([^\r\n]+)", self.qemu_output)
        failed_tests = {}
        for failure in fail_matches:
            if failure.startswith("[") and "]" in failure:
                name, reason = failure[1:].split("]", 1)
            else:
                name, separator, reason = failure.partition(":")
                if not separator:
                    reason = ""
            failed_tests.setdefault(name.strip(), reason.strip())

        self.results = [TestResult(name, True) for name in pass_names]
        self.results.extend(
            TestResult(name, False, reason)
            for name, reason in failed_tests.items()
        )

        summary_pattern = r"Total tests: (\d+)\s+Passed:\s+(\d+)\s+Failed:\s+(\d+)"
        summary_match = re.search(summary_pattern, self.qemu_output)
        if not summary_match:
            self.log(
                "Integration test summary is missing; the suite may not have completed",
                "FAIL",
            )
            return False

        total, passed, failed = map(int, summary_match.groups())
        if (total != len(self.results)
                or passed != len(pass_names)
                or failed != len(failed_tests)):
            self.log(
                "Integration test summary does not match parsed results",
                "FAIL",
            )
            return False

        # If no tests found, check if QEMU ran
        if not self.results:
            self.log("Integration tests produced no test results", "FAIL")
            return False

        self.log(f"Parsed {len(self.results)} test results", "INFO")
        return True

    def print_results(self):
        """Print human-readable test results"""
        print("\n" + "="*60)
        print("INTEGRATION TEST RESULTS")
        print("="*60)

        passed = sum(1 for r in self.results if r.passed)
        failed = sum(1 for r in self.results if r.passed is False)
        total = len(self.results)

        for result in self.results:
            status = "✓" if result.passed else "✗"
            print(f"  {status} {result.name}")
            if result.reason and not result.passed:
                print(f"      Reason: {result.reason}")

        print("\n" + "-"*60)
        print(f"Total:  {total}")
        print(f"Passed: {passed}")
        print(f"Failed: {failed}")
        print("="*60 + "\n")

        if failed == 0 and total > 0:
            self.log("ALL TESTS PASSED!", "PASS")
            return True
        elif failed > 0:
            self.log(f"{failed} test(s) failed", "FAIL")
            return False
        else:
            self.log("No tests were run", "WARN")
            return False

    def generate_junit_report(self, output_file: str):
        """Generate JUnit XML report for CI/CD integration"""
        self.log(f"Generating JUnit report: {output_file}", "INFO")

        xml = '<?xml version="1.0" encoding="UTF-8"?>\n'
        xml += f'<testsuites name="System7-IntegrationTests" tests="{len(self.results)}">\n'
        xml += f'  <testsuite name="Phase1-IntegrationTests" tests="{len(self.results)}">\n'

        for result in self.results:
            xml += f'    <testcase name="{result.name}" time="0">\n'
            if not result.passed:
                xml += f'      <failure message="{result.reason}"/>\n'
            xml += '    </testcase>\n'

        xml += '  </testsuite>\n'
        xml += '</testsuites>\n'

        try:
            with open(output_file, 'w') as f:
                f.write(xml)
            self.log(f"JUnit report written: {output_file}", "PASS")
        except Exception as e:
            self.log(f"Failed to write JUnit report: {e}", "FAIL")

def main():
    parser = argparse.ArgumentParser(
        description="System 7 Integration Test Runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                              # Run tests with defaults
  %(prog)s --timeout 60                 # Use 60 second timeout
  %(prog)s --output junit.xml           # Generate JUnit report
  %(prog)s --verbose --no-build         # Verbose output, reuse existing ISO
        """
    )

    parser.add_argument("--timeout", type=int, default=120,
                      help="QEMU execution timeout in seconds (default: 120)")
    parser.add_argument("--output", type=str, metavar="FILE",
                      help="Generate JUnit XML report to FILE")
    parser.add_argument("--verbose", action="store_true",
                      help="Enable verbose output")
    parser.add_argument("--no-build", action="store_true",
                      help="Skip build and use the existing ISO")
    default_project_root = Path(__file__).resolve().parent.parent
    parser.add_argument("--project-root", type=str, default=str(default_project_root),
                      help="Project root directory (defaults to this checkout)")

    args = parser.parse_args()

    project_root = args.project_root
    if not Path(project_root).exists():
        print(f"Error: Project root not found: {project_root}", file=sys.stderr)
        return 1

    # Create test runner
    runner = TestRunner(
        project_root=project_root,
        timeout=args.timeout,
        verbose=args.verbose
    )

    if not args.no_build and not runner.build_kernel():
        return 1

    # Run tests
    if not runner.run_tests():
        return 1

    # Parse results
    if not runner.parse_test_results():
        return 1

    # Print results
    success = runner.print_results()

    # Generate report if requested
    if args.output:
        runner.generate_junit_report(args.output)

    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
