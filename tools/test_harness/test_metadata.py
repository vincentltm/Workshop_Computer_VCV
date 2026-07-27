#!/usr/bin/env python3
"""
test_metadata.py — Cross-platform metadata consistency tests.
Validates info.yaml, CardDefinitions.js, WASM_CARD_MAP, and ExtendedMetadata.hpp.
"""
import os
from typing import List, Dict, Optional

from .conftest import (
    CardInfo, TestResult, CardTestReport,
    resolve_card_source_dir, find_info_yaml,
    parse_card_definitions_js, parse_wasm_card_map, parse_simple_yaml,
    EXTENDED_METADATA_HPP,
    GREEN, RED, YELLOW, BOLD, DIM, RESET
)


def _check_info_yaml_parseable(card: CardInfo) -> TestResult:
    """Check that info.yaml exists and is parseable."""
    yaml_path = find_info_yaml(card)
    if not yaml_path:
        return TestResult("info_yaml_parseable", False, "info.yaml not found")
    try:
        data = parse_simple_yaml(yaml_path)
        has_name = bool(data.get("Name") or data.get("name") or
                        data.get("short-description") or data.get("title"))
        if has_name:
            return TestResult("info_yaml_parseable", True, "Parsed OK",
                              {"keys": list(data.keys())})
        return TestResult("info_yaml_parseable", False,
                          "No Name/title/short-description field found",
                          {"keys": list(data.keys())})
    except Exception as e:
        return TestResult("info_yaml_parseable", False, f"Parse error: {e}")


def _check_in_card_definitions(card: CardInfo,
                                pn_cards: Dict[str, Dict]) -> TestResult:
    """Check that the card appears in CardDefinitions.js (if patchnotes=true)."""
    if not card.patchnotes:
        return TestResult("in_card_definitions_js", True,
                          "Skipped (patchnotes=false)")
    if card.id in pn_cards:
        return TestResult("in_card_definitions_js", True, "Found")
    return TestResult("in_card_definitions_js", False,
                      f"Card '{card.id}' not found in CardDefinitions.js")


def _check_in_wasm_card_map(card: CardInfo,
                             wasm_map: Dict[str, int]) -> TestResult:
    """Check that the card has an entry in WASM_CARD_MAP."""
    if not card.patchnotes:
        return TestResult("in_wasm_card_map", True,
                          "Skipped (patchnotes=false)")
    from .conftest import WASM_ALIASES
    target_id = WASM_ALIASES.get(card.id, card.id)
    if target_id in wasm_map:
        return TestResult("in_wasm_card_map", True,
                          f"Index={wasm_map[target_id]} (mapped from '{card.id}')" if target_id != card.id else f"Index={wasm_map[card.id]}")
    return TestResult("in_wasm_card_map", False,
                      f"Card '{card.id}' (mapped: '{target_id}') not in WASM_CARD_MAP")


def _check_in_extended_metadata(card: CardInfo,
                                 metadata_content: str) -> TestResult:
    """Check that the card id appears in ExtendedMetadata.hpp."""
    search_str = f'"{card.id}"'
    if search_str in metadata_content:
        return TestResult("in_extended_metadata", True, "Found")
    return TestResult("in_extended_metadata", False,
                      f'"{card.id}" not found in ExtendedMetadata.hpp')


def _check_creator_consistent(card: CardInfo,
                                pn_cards: Dict[str, Dict]) -> TestResult:
    """Check creator field consistency across sources."""
    yaml_path = find_info_yaml(card)
    yaml_creator = ""
    if yaml_path:
        data = parse_simple_yaml(yaml_path)
        yaml_creator = data.get("Creator", data.get("creator", ""))

    pn_creator = ""
    if card.id in pn_cards:
        pn_creator = pn_cards[card.id].get("creator", "")

    registry_creator = card.creator

    # Check consistency (case-insensitive, ignoring empty)
    sources = {}
    if registry_creator:
        sources["registry"] = registry_creator
    if yaml_creator:
        sources["yaml"] = yaml_creator
    if pn_creator:
        sources["patchnotes"] = pn_creator

    if len(sources) <= 1:
        return TestResult("creator_consistent", True,
                          f"Only one source: {list(sources.values())}")

    unique = set(v.lower().strip() for v in sources.values())
    if len(unique) == 1:
        return TestResult("creator_consistent", True, "Consistent")

    return TestResult("creator_consistent", False,
                      f"Mismatch: {sources}",
                      {"sources": sources})


def _check_license_present(card: CardInfo) -> TestResult:
    """Check that a license field exists and is non-empty."""
    if card.license and card.license.lower() not in ("", "no license specified",
                                                       "none", "unknown"):
        return TestResult("license_present", True, card.license)
    # Check info.yaml
    yaml_path = find_info_yaml(card)
    if yaml_path:
        data = parse_simple_yaml(yaml_path)
        lic = data.get("License", data.get("license", ""))
        if lic and lic.lower() not in ("", "none", "unknown"):
            return TestResult("license_present", True, f"From YAML: {lic}")
    return TestResult("license_present", False, "No license specified")


def _check_patchnotes_name(card: CardInfo,
                            pn_cards: Dict[str, Dict]) -> TestResult:
    """Check that the name in CardDefinitions.js matches the registry."""
    if not card.patchnotes or card.id not in pn_cards:
        return TestResult("patchnotes_name_matches", True, "Skipped")

    pn_name = pn_cards[card.id].get("name", "")
    reg_name = card.name
    import re
    clean_reg = re.sub(r'^\d+\s*', '', reg_name).strip()
    clean_pn = re.sub(r'^\d+\s*', '', pn_name).strip()

    if clean_reg.lower() == clean_pn.lower():
        return TestResult("patchnotes_name_matches", True, f"Matches ('{card.name}' ~ '{pn_name}')")
    return TestResult("patchnotes_name_matches", False,
                      f"Registry='{card.name}' vs PN='{pn_name}'",
                      {"registry": card.name, "patchnotes": pn_name})


def run_metadata_tests(cards: List[CardInfo]) -> List[CardTestReport]:
    """Run metadata consistency tests for all cards.

    Returns a CardTestReport for each card (platform='metadata'),
    plus reports for global cross-platform checks.
    """
    reports = []
    total = len(cards)

    print(f"\n{BOLD}Metadata Consistency{RESET}")
    print("━" * 56)

    # Load metadata sources once
    pn_cards = parse_card_definitions_js()
    wasm_map = parse_wasm_card_map()
    metadata_content = ""
    if os.path.exists(EXTENDED_METADATA_HPP):
        with open(EXTENDED_METADATA_HPP, "r", encoding="utf-8") as f:
            metadata_content = f.read()

    pass_count = 0
    degraded_count = 0
    fail_count = 0

    for i, card in enumerate(cards):
        tests = []
        tests.append(_check_info_yaml_parseable(card))
        tests.append(_check_in_card_definitions(card, pn_cards))
        tests.append(_check_in_wasm_card_map(card, wasm_map))
        tests.append(_check_in_extended_metadata(card, metadata_content))
        tests.append(_check_creator_consistent(card, pn_cards))
        tests.append(_check_license_present(card))
        tests.append(_check_patchnotes_name(card, pn_cards))

        # Classify status
        # Critical: card_definitions, wasm_map, extended_metadata
        # Non-critical: info_yaml, creator, license, name_match
        critical_names = {"in_card_definitions_js", "in_wasm_card_map",
                          "in_extended_metadata"}
        critical_fail = any(not t.passed for t in tests
                           if t.name in critical_names)
        any_fail = any(not t.passed for t in tests)

        if critical_fail:
            status = "fail"
            fail_count += 1
        elif any_fail:
            status = "degraded"
            degraded_count += 1
        else:
            status = "pass"
            pass_count += 1

        # Print progress
        checks = ""
        short_names = {
            "info_yaml_parseable": "yaml",
            "in_card_definitions_js": "pn",
            "in_wasm_card_map": "wasm",
            "in_extended_metadata": "meta",
            "creator_consistent": "cre",
            "license_present": "lic",
            "patchnotes_name_matches": "name",
        }
        for t in tests:
            label = short_names.get(t.name, t.name[:4])
            if t.message.startswith("Skipped"):
                mark = f"{DIM}·{RESET}"
            elif t.passed:
                mark = f"{GREEN}✓{RESET}"
            else:
                mark = f"{RED}✗{RESET}"
            checks += f" {label} {mark}"

        status_icon = {"pass": f"{GREEN}✅{RESET}", "degraded": f"{YELLOW}⚠️{RESET}",
                       "fail": f"{RED}❌{RESET}"}[status]
        print(f"  [{i+1:02d}/{total}] {card.id:<22s} (#{card.num})"
              f"  {checks}  {status_icon}")

        if status != "pass":
            for t in tests:
                if not t.passed and not t.message.startswith("Skipped"):
                    print(f"         └─ {t.name}: {DIM}{t.message}{RESET}")

        reports.append(CardTestReport(
            card_id=card.id,
            card_num=card.num,
            card_name=card.name,
            platform="metadata",
            status=status,
            tests=tests,
        ))

    # Global checks
    enabled_ids = {c.id for c in cards}
    pn_ids = set(pn_cards.keys())

    # Orphaned: in Patchnotes but not in enabled registry
    orphaned = pn_ids - enabled_ids
    orphaned_tests = []
    if orphaned:
        orphaned_tests.append(TestResult(
            "orphaned_patchnotes_cards", False,
            f"{len(orphaned)} cards in CardDefinitions.js not in registry",
            {"card_ids": sorted(orphaned)}))
    else:
        orphaned_tests.append(TestResult(
            "orphaned_patchnotes_cards", True, "None found"))

    reports.append(CardTestReport(
        card_id="__orphaned__",
        card_num="--",
        card_name="Orphaned Patchnotes Cards",
        platform="metadata",
        status="fail" if orphaned else "pass",
        tests=orphaned_tests,
    ))

    # Missing: patchnotes=true in registry but not in CardDefinitions.js
    pn_enabled = {c.id for c in cards if c.patchnotes}
    missing = pn_enabled - pn_ids
    missing_tests = []
    if missing:
        missing_tests.append(TestResult(
            "missing_patchnotes_cards", False,
            f"{len(missing)} cards marked patchnotes=true but missing from JS",
            {"card_ids": sorted(missing)}))
    else:
        missing_tests.append(TestResult(
            "missing_patchnotes_cards", True, "None found"))

    reports.append(CardTestReport(
        card_id="__missing_pn__",
        card_num="--",
        card_name="Missing Patchnotes Cards",
        platform="metadata",
        status="fail" if missing else "pass",
        tests=missing_tests,
    ))

    orphan_mark = f"{RED}{len(orphaned)}{RESET}" if orphaned else f"{GREEN}0{RESET}"
    missing_mark = f"{RED}{len(missing)}{RESET}" if missing else f"{GREEN}0{RESET}"
    print(f"\n  Global: {orphan_mark} orphaned, {missing_mark} missing patchnotes")
    print(f"  Results: {GREEN}{pass_count} pass{RESET}, "
          f"{YELLOW}{degraded_count} degraded{RESET}, "
          f"{RED}{fail_count} fail{RESET}")

    return reports
