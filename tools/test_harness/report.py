#!/usr/bin/env python3
"""
report.py — Output formatting and report generators (terminal, JSON, markdown).
"""
import os
import json
from datetime import datetime
from typing import List, Dict, Any, Tuple, Optional, Iterable

from .conftest import (
    CardTestReport,
    GREEN, RED, YELLOW, BLUE, CYAN, BOLD, DIM, RESET
)


def _build_card_map(reports: List[CardTestReport]) -> Tuple[Dict[str, Dict[str, CardTestReport]], Dict[str, str], Dict[str, str]]:
    from .conftest import load_card_registry
    all_cards = load_card_registry()
    by_card: Dict[str, Dict[str, CardTestReport]] = {c.id: {} for c in all_cards}
    card_names: Dict[str, str] = {c.id: c.name for c in all_cards}
    card_nums: Dict[str, str] = {c.id: c.num for c in all_cards}

    for r in reports:
        if r.card_id.startswith("__"):
            continue
        if r.card_id not in by_card:
            by_card[r.card_id] = {}
            card_names[r.card_id] = r.card_name
            card_nums[r.card_id] = r.card_num
        by_card[r.card_id][r.platform] = r

    return by_card, card_names, card_nums


def _sort_cids(cids: Iterable[str], card_nums: Dict[str, str]) -> List[str]:
    def sort_key(cid: str) -> Tuple[int, str]:
        num_str = card_nums.get(cid, "9999")
        try:
            return (int(num_str), cid)
        except ValueError:
            return (9999, cid)
    return sorted(cids, key=sort_key)


def print_terminal_report(reports: List[CardTestReport]):
    """Print rich ANSI-colored summary table to terminal."""
    print(f"\n{BOLD}═════════════════════════════════════════════════════════════════════════════════{RESET}")
    print(f"{BOLD}                        CARD TEST SUITE OVERALL REPORT                           {RESET}")
    print(f"{BOLD}═════════════════════════════════════════════════════════════════════════════════{RESET}\n")

    by_card, card_names, card_nums = _build_card_map(reports)

    # Table Header
    print(f"{BOLD}{'NUM':<4} {'CARD ID':<22} {'NAME':<20} {'BUILD':<8} {'META':<8} {'VCV':<8} {'WASM':<8}{RESET}")
    print(f"{DIM}─────────────────────────────────────────────────────────────────────────────────{RESET}")

    def icon(rep: Optional[CardTestReport]) -> str:
        if not rep:
            return f"{DIM}—{RESET}"
        st = rep.status
        if st == "pass":
            return f"{GREEN}✅ PASS{RESET}"
        elif st == "degraded":
            return f"{YELLOW}⚠️ DEG{RESET}"
        elif st == "silent":
            return f"{YELLOW}🔇 SIL{RESET}"
        elif st == "fail":
            return f"{RED}❌ FAIL{RESET}"
        elif st == "skip":
            return f"{DIM}⏭ SKIP{RESET}"
        return st

    for cid in _sort_cids(by_card.keys(), card_nums):
        num = card_nums[cid]
        name = card_names[cid][:19]
        plat_map = by_card[cid]

        b_ic = icon(plat_map.get("build"))
        m_ic = icon(plat_map.get("metadata"))
        v_ic = icon(plat_map.get("vcv"))
        w_ic = icon(plat_map.get("patchnotes"))

        print(f"{num:<4} {cid:<22} {name:<20} {b_ic:<17} {m_ic:<17} {v_ic:<17} {w_ic:<17}")

    print(f"{DIM}─────────────────────────────────────────────────────────────────────────────────{RESET}")

    # Summarize failed details
    failures = [r for r in reports if r.status == "fail" and not r.card_id.startswith("__")]
    if failures:
        print(f"\n{RED}{BOLD}Failed Test Details ({len(failures)} item(s)):{RESET}")
        for r in failures:
            print(f" • {BOLD}[{r.platform.upper()}] {r.card_id}{RESET} (#{r.card_num}): {r.error or 'Failed checks'}")
            for t in r.tests:
                if not t.passed:
                    print(f"    └─ {t.name}: {DIM}{t.message}{RESET}")

    print(f"\n{BOLD}Report generation complete.{RESET}\n")


def generate_json_report(reports: List[CardTestReport], output_path: str):
    """Generate machine-readable JSON report."""
    by_card: Dict[str, Dict[str, Any]] = {}
    summary_counts = {
        "build": {"pass": 0, "degraded": 0, "fail": 0, "skip": 0},
        "metadata": {"pass": 0, "degraded": 0, "fail": 0, "skip": 0},
        "vcv": {"pass": 0, "degraded": 0, "fail": 0, "skip": 0},
        "patchnotes": {"pass": 0, "degraded": 0, "fail": 0, "skip": 0},
    }

    for r in reports:
        if r.platform in summary_counts:
            st = r.status if r.status in summary_counts[r.platform] else "fail"
            summary_counts[r.platform][st] += 1

        if r.card_id not in by_card:
            by_card[r.card_id] = {
                "id": r.card_id,
                "num": r.card_num,
                "name": r.card_name,
                "results": {}
            }
        by_card[r.card_id]["results"][r.platform] = r.to_dict()

    data = {
        "timestamp": datetime.now().isoformat(),
        "summary": summary_counts,
        "cards": list(by_card.values())
    }

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    print(f"JSON report saved to {output_path}")


def generate_markdown_report(reports: List[CardTestReport], output_path: str):
    """Generate structured markdown report for documentation and auto-fixing."""
    by_card, card_names, card_nums = _build_card_map(reports)

    lines = []
    lines.append("# Unified Card Test Suite Report\n")
    lines.append(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")

    lines.append("## Overview\n")
    lines.append("| Card Num | ID | Name | Build | Metadata | VCV Rack | Patchnotes (WASM) |")
    lines.append("|---|---|---|---|---|---|---|")

    def md_badge(rep: Optional[CardTestReport]) -> str:
        if not rep:
            return "—"
        st = rep.status
        if st == "pass":
            return "✅ PASS"
        elif st == "degraded":
            return "⚠️ DEGRADED"
        elif st == "silent":
            return "🔇 SILENT"
        elif st == "fail":
            return "❌ FAIL"
        elif st == "skip":
            return "⏭ SKIP"
        return st

    for cid in _sort_cids(by_card.keys(), card_nums):
        num = card_nums[cid]
        name = card_names[cid]
        pm = by_card[cid]
        lines.append(f"| {num} | `{cid}` | {name} | {md_badge(pm.get('build'))} | {md_badge(pm.get('metadata'))} | {md_badge(pm.get('vcv'))} | {md_badge(pm.get('patchnotes'))} |")

    lines.append("\n## Issues & Action Items\n")
    failures = [r for r in reports if r.status in ("fail", "degraded") and not r.card_id.startswith("__")]
    if not failures:
        lines.append("🎉 All tested cards passed successfully with no errors or degraded behavior!\n")
    else:
        for r in failures:
            lines.append(f"### Card `{r.card_id}` (#{r.card_num}) — [{r.platform.upper()}] {r.status.upper()}")
            lines.append(f"- **Error:** {r.error or 'Failed assertions'}")
            lines.append("- **Failed Checks:**")
            for t in r.tests:
                if not t.passed:
                    lines.append(f"  - `{t.name}`: {t.message}")
            lines.append("")

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"Markdown report ({len(lines)} lines, {len(by_card)} cards) saved to {output_path}")

