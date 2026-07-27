#!/usr/bin/env python3
"""
test_build.py — Build verification tests.
Checks source files, info.yaml, VCV wrappers, dylibs, and WASM binaries exist.
"""
import os
import ctypes
from typing import List

from .conftest import (
    CardInfo, TestResult, CardTestReport,
    resolve_card_source_dir, find_info_yaml,
    get_card_dylib_path, get_dylib_extension,
    PROJECT_DIR, CARDS_SRC_DIR, WASM_BINARY, WASM_JS,
    GREEN, RED, YELLOW, BOLD, DIM, RESET
)


def _check_source_exists(card: CardInfo) -> TestResult:
    """Verify source files exist for the card (in release directory or ported wrapper)."""
    src_dir = resolve_card_source_dir(card)
    wrapper_path = os.path.join(CARDS_SRC_DIR, f"Card_{card.id}.cpp")

    # Collect all existing filenames in src_dir recursively
    existing_in_sdir = set()
    if os.path.exists(src_dir):
        for root, _, files in os.walk(src_dir):
            for f in files:
                existing_in_sdir.add(f.lower())

    missing = []
    for src in card.sources:
        base_src = os.path.basename(src).lower()
        paths_to_check = [
            os.path.join(src_dir, src),
            os.path.join(src_dir, os.path.basename(src)),
            os.path.join(PROJECT_DIR, src),
        ]
        found = any(os.path.exists(p) for p in paths_to_check) or (base_src in existing_in_sdir)
        if not found and os.path.exists(wrapper_path):
            found = True  # Auto-ported wrapper exists

        if not found:
            missing.append(src)

    if missing:
        return TestResult("source_exists", False,
                          f"Missing: {', '.join(missing)}",
                          {"missing": missing, "src_dir": src_dir})
    return TestResult("source_exists", True,
                      f"{len(card.sources)} source file(s) / wrapper verified")


def _check_info_yaml(card: CardInfo) -> TestResult:
    """Verify info.yaml exists for the card."""
    path = find_info_yaml(card)
    if path:
        return TestResult("info_yaml_exists", True, "Found", {"path": path})
    return TestResult("info_yaml_exists", False,
                      f"Not found in {resolve_card_source_dir(card)}")


def _check_vcv_wrapper(card: CardInfo) -> TestResult:
    """Verify the generated Card_<id>.cpp wrapper exists."""
    wrapper = os.path.join(CARDS_SRC_DIR, f"Card_{card.id}.cpp")
    if os.path.exists(wrapper):
        return TestResult("vcv_wrapper_exists", True, "Found",
                          {"path": wrapper})
    return TestResult("vcv_wrapper_exists", False,
                      f"Not found: Card_{card.id}.cpp")


def _check_vcv_dylib(card: CardInfo) -> TestResult:
    """Verify the compiled dylib exists."""
    dylib_path = get_card_dylib_path(card)
    if os.path.exists(dylib_path):
        size_kb = os.path.getsize(dylib_path) / 1024
        return TestResult("vcv_dylib_exists", True,
                          f"Found ({size_kb:.0f} KB)",
                          {"path": dylib_path, "size_kb": size_kb})
    return TestResult("vcv_dylib_exists", False,
                      f"Not found: {os.path.basename(dylib_path)}")


def _check_vcv_dylib_loadable(card: CardInfo) -> TestResult:
    """Try to load the dylib and verify essential symbols exist."""
    dylib_path = get_card_dylib_path(card)
    if not os.path.exists(dylib_path):
        return TestResult("vcv_dylib_loadable", False,
                          "Skipped (dylib missing)")

    try:
        lib = ctypes.CDLL(dylib_path, mode=ctypes.RTLD_LOCAL)
        # Check essential symbols
        has_set_globals = hasattr(lib, "set_thread_globals")
        has_run_card = hasattr(lib, "run_card")
        del lib  # Unload

        if has_set_globals and has_run_card:
            return TestResult("vcv_dylib_loadable", True,
                              "Loaded OK, symbols verified")
        missing_syms = []
        if not has_set_globals:
            missing_syms.append("set_thread_globals")
        if not has_run_card:
            missing_syms.append("run_card")
        return TestResult("vcv_dylib_loadable", False,
                          f"Missing symbols: {', '.join(missing_syms)}")
    except OSError as e:
        return TestResult("vcv_dylib_loadable", False,
                          f"Load error: {e}")


def run_build_tests(cards: List[CardInfo]) -> List[CardTestReport]:
    """Run build verification tests for all cards.

    Returns a CardTestReport for each card (platform='build'),
    plus one report for global WASM checks.
    """
    reports = []
    total = len(cards)

    print(f"\n{BOLD}Build Verification{RESET}")
    print("━" * 56)

    pass_count = 0
    degraded_count = 0
    fail_count = 0

    for i, card in enumerate(cards):
        tests = []
        tests.append(_check_source_exists(card))
        tests.append(_check_info_yaml(card))
        tests.append(_check_vcv_wrapper(card))
        tests.append(_check_vcv_dylib(card))
        tests.append(_check_vcv_dylib_loadable(card))

        # Determine status
        # info_yaml is non-critical
        critical = [t for t in tests if t.name != "info_yaml_exists"]
        critical_fail = any(not t.passed for t in critical)
        info_fail = not tests[1].passed

        if critical_fail:
            status = "fail"
            fail_count += 1
        elif info_fail:
            status = "degraded"
            degraded_count += 1
        else:
            status = "pass"
            pass_count += 1

        # Print progress line
        checks = ""
        for t in tests:
            short = {"source_exists": "src", "info_yaml_exists": "yaml",
                     "vcv_wrapper_exists": "wrap", "vcv_dylib_exists": "dylib",
                     "vcv_dylib_loadable": "load"}
            label = short.get(t.name, t.name[:4])
            mark = f"{GREEN}✓{RESET}" if t.passed else f"{RED}✗{RESET}"
            checks += f" {label} {mark}"

        status_icon = {"pass": f"{GREEN}✅{RESET}", "degraded": f"{YELLOW}⚠️{RESET}",
                       "fail": f"{RED}❌{RESET}"}[status]
        print(f"  [{i+1:02d}/{total}] {card.id:<22s} (#{card.num})"
              f"  {checks}  {status_icon}")

        # Print failure details
        if status != "pass":
            for t in tests:
                if not t.passed:
                    print(f"         └─ {t.name}: {DIM}{t.message}{RESET}")

        reports.append(CardTestReport(
            card_id=card.id,
            card_num=card.num,
            card_name=card.name,
            platform="build",
            status=status,
            tests=tests,
        ))

    # Global WASM checks
    wasm_tests = []
    wasm_binary_ok = os.path.exists(WASM_BINARY)
    wasm_js_ok = os.path.exists(WASM_JS)

    if wasm_binary_ok:
        size_mb = os.path.getsize(WASM_BINARY) / (1024 * 1024)
        wasm_tests.append(TestResult("wasm_binary_exists", True,
                                     f"Found ({size_mb:.1f} MB)"))
    else:
        wasm_tests.append(TestResult("wasm_binary_exists", False,
                                     f"Not found: {WASM_BINARY}"))

    if wasm_js_ok:
        wasm_tests.append(TestResult("wasm_js_exists", True, "Found"))
    else:
        wasm_tests.append(TestResult("wasm_js_exists", False,
                                     f"Not found: {WASM_JS}"))

    wasm_status = "pass" if all(t.passed for t in wasm_tests) else "fail"
    wasm_mark_b = f"{GREEN}✓{RESET}" if wasm_binary_ok else f"{RED}✗{RESET}"
    wasm_mark_j = f"{GREEN}✓{RESET}" if wasm_js_ok else f"{RED}✗{RESET}"
    print(f"\n  WASM:  binary {wasm_mark_b}  js {wasm_mark_j}")

    reports.append(CardTestReport(
        card_id="__wasm_global__",
        card_num="--",
        card_name="WASM Binary",
        platform="build",
        status=wasm_status,
        tests=wasm_tests,
    ))

    print(f"\n  Results: {GREEN}{pass_count} pass{RESET}, "
          f"{YELLOW}{degraded_count} degraded{RESET}, "
          f"{RED}{fail_count} fail{RESET}")

    return reports
