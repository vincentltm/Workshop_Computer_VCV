#!/usr/bin/env python3
"""
run_all_tests.py — Main orchestrator for Workshop Computer unified card test harness.
"""
import sys
import os
import argparse
import time

# Ensure python path includes tools directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

from test_harness.conftest import load_card_registry, CardTestReport, BOLD, RESET, GREEN, RED, YELLOW, CYAN
from test_harness.test_build import run_build_tests
from test_harness.test_metadata import run_metadata_tests
from test_harness.test_vcv_cards import run_vcv_tests
from test_harness.test_patchnotes_cards import run_patchnotes_tests
from test_harness.test_parity import run_parity_tests
from test_harness.report import print_terminal_report, generate_json_report, generate_markdown_report


def main():
    parser = argparse.ArgumentParser(description="Unified Card Test Harness for Workshop Computer (VCV + Patchnotes)")
    parser.add_argument("--vcv-only", action="store_true", help="Run VCV Rack tests only")
    parser.add_argument("--patchnotes-only", action="store_true", help="Run Patchnotes WASM tests only")
    parser.add_argument("--build-only", action="store_true", help="Run build verification only")
    parser.add_argument("--metadata-only", action="store_true", help="Run metadata consistency checks only")
    parser.add_argument("--card", type=str, help="Test a single card by ID")
    parser.add_argument("--json", type=str, default=os.path.join(PROJECT_DIR, "test_report.json"), help="Output path for JSON report")
    parser.add_argument("--markdown", type=str, default=os.path.join(PROJECT_DIR, "TEST_REPORT.md"), help="Output path for Markdown report")
    parser.add_argument("--no-compile", action="store_true", help="Skip harness C++ compilation")
    parser.add_argument("--no-report", action="store_true", help="Skip writing report files to disk")
    parser.add_argument("--stress", action="store_true", help="Run extended stress tests (slower but more thorough)")
    parser.add_argument("--parity", action="store_true", help="Run cross-platform parity tests (VCV vs WASM)")
    parser.add_argument("--parity-only", action="store_true", help="Run parity tests only")

    args = parser.parse_args()

    cards = load_card_registry()
    if not cards:
        print(f"{RED}No enabled cards loaded from registry.{RESET}")
        sys.exit(1)

    if args.card:
        cards = [c for c in cards if c.id == args.card or c.num == args.card]
        if not cards:
            print(f"{RED}Card '{args.card}' not found in enabled registry.{RESET}")
            sys.exit(1)

    print(f"{BOLD}╔══════════════════════════════════════════════════════════════════════════════╗{RESET}")
    stress_str = " [STRESS]" if args.stress else ""
    print(f"{BOLD}║  Workshop Computer — Unified Card Test Harness{stress_str:<31s}║{RESET}")
    print(f"{BOLD}║  Testing {len(cards):2d} card(s) across VCV Rack + Patchnotes                              ║{RESET}")
    print(f"{BOLD}╚══════════════════════════════════════════════════════════════════════════════╝{RESET}")

    all_reports: list[CardTestReport] = []

    run_all = not (args.vcv_only or args.patchnotes_only or args.build_only or args.metadata_only or args.parity_only)

    t_start = time.time()

    # 1. Build Verification
    if run_all or args.build_only:
        reports_build = run_build_tests(cards)
        all_reports.extend(reports_build)

    # 2. Metadata Consistency
    if run_all or args.metadata_only:
        reports_meta = run_metadata_tests(cards)
        all_reports.extend(reports_meta)

    # 3. VCV Rack Functional Tests
    if run_all or args.vcv_only:
        reports_vcv = run_vcv_tests(cards, skip_compile=args.no_compile, stress=args.stress)
        all_reports.extend(reports_vcv)

    # 4. Patchnotes WASM Tests
    if run_all or args.patchnotes_only:
        reports_pn = run_patchnotes_tests(cards, stress=args.stress)
        all_reports.extend(reports_pn)

    # 5. Cross-Platform Parity Tests
    if (run_all and (args.stress or args.parity)) or args.parity_only:
        reports_parity = run_parity_tests(cards)
        all_reports.extend(reports_parity)

    t_elapsed = time.time() - t_start

    # Terminal summary table
    print_terminal_report(all_reports)

    print(f"Total time elapsed: {BOLD}{t_elapsed:.2f}s{RESET}")

    # Generate Reports
    if not args.no_report:
        if args.json:
            generate_json_report(all_reports, args.json)
        if args.markdown:
            generate_markdown_report(all_reports, args.markdown)

    # Check overall pass/fail status (exit code 1 if any failure)
    has_failures = any(r.status == "fail" for r in all_reports)
    if has_failures:
        print(f"\n{RED}{BOLD}Test suite finished with failures.{RESET}")
        sys.exit(1)
    else:
        print(f"\n{GREEN}{BOLD}Test suite finished successfully! All checks passed.{RESET}")
        sys.exit(0)


if __name__ == "__main__":
    main()
