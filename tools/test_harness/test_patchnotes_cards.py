#!/usr/bin/env python3
"""
test_patchnotes_cards.py — Headless Patchnotes WASM card tests using Node.js.
"""
import os
import sys
import json
import shutil
import tempfile
import subprocess
from typing import List, Dict

from .conftest import (
    CardInfo, TestResult, CardTestReport,
    parse_wasm_card_map,
    WASM_DIR, WASM_BINARY, WASM_JS,
    GREEN, RED, YELLOW, BLUE, BOLD, DIM, RESET
)


NODE_SCRIPT_TEMPLATE = """
delete process.versions.node;
global.window = global;
global.document = { createElement: () => ({}), querySelector: () => null };
global.navigator = { userAgent: 'node' };
global.location = { href: 'http://localhost/' };
global.performance = require('perf_hooks').performance;

const path = require('path');
const wasmDir = __WASM_DIR__;

const createModule = require(path.join(wasmDir, 'patchnotes_cards.js'));
const CARDS = __CARDS__;

const A_THR = 0.01;
const C_THR = 0.01;
const L_THR = 0.01;
const WARMUP = 480;
const N_EACH = 1500; const STRESS_MODE = __STRESS_MODE__;async function main() {
    let inst;
    try {
        const fs = require('fs');
        const wasmPath = path.join(wasmDir, 'patchnotes_cards.wasm');
        const wasmBuf = fs.readFileSync(wasmPath);
        inst = await createModule({
            wasmBinary: wasmBuf,
            print: () => {},
            printErr: () => {},
            locateFile: (p) => path.join(wasmDir, p)
        });
    } catch(e) {
        console.log(JSON.stringify({error: 'Module load failed: ' + e.message, results: []}));
        process.exit(0);
    }
    
    const results = [];
    for (const card of CARDS) {
        results.push(testCard(inst, card));
    }
    console.log(JSON.stringify({error: null, results}));
}

function testCard(inst, card) {
    const r = {id: card.id, idx: card.idx, name: card.name, num: card.num, tests: [], status: 'pass', error: null, duration_ms: 0};
    const t0 = Date.now();
    
    // Test 1: init
    try {
        inst._init_card(card.idx);
        r.tests.push({name: 'init', passed: true, message: 'OK'});
    } catch(e) {
        r.tests.push({name: 'init', passed: false, message: 'Crash: ' + e.message});
        r.status = 'fail'; r.error = e.message;
        r.duration_ms = Date.now() - t0;
        return r;
    }
    
    try { inst._set_input_connected(true,true,true,true,true,true); } catch(e) {}
    
    // Test 2: idle
    try {
        let audioPeak = 0, cvPeak = 0, ledPeak = 0, hasNaN = false;
        for (let i = 0; i < WARMUP; i++) {
            inst._set_inputs(0.5, 0.5, 0.5, 1, false, false, 0, 0, 0, 0);
            inst._process_sample();
        }
        for (let i = 0; i < N_EACH; i++) {
            inst._set_inputs(0.5, 0.5, 0.5, 1, false, false, 0, 0, 0, 0);
            inst._process_sample();
            const a1 = inst._get_audio_out1(), a2 = inst._get_audio_out2();
            const c1 = inst._get_cv_out1(), c2 = inst._get_cv_out2();
            if (isNaN(a1) || isNaN(a2) || isNaN(c1) || isNaN(c2)) hasNaN = true;
            audioPeak = Math.max(audioPeak, Math.abs(a1), Math.abs(a2));
            cvPeak = Math.max(cvPeak, Math.abs(c1), Math.abs(c2));
        }
        for (let j = 0; j < 6; j++) ledPeak = Math.max(ledPeak, inst._get_led_brightness(j));
        r.tests.push({name: 'idle', passed: !hasNaN, message: hasNaN ? 'NaN in outputs' : 'OK', details: {audioPeak, cvPeak, ledPeak}});
        if (hasNaN) r.status = 'fail';
    } catch(e) {
        r.tests.push({name: 'idle', passed: false, message: 'Crash: ' + e.message});
        r.status = 'fail'; r.error = e.message;
        r.duration_ms = Date.now() - t0;
        return r;
    }
    
    // Test 3: trigger
    try {
        let audioPeak = 0, cvPeak = 0, ledPeak = 0;
        for (let i = 0; i < N_EACH; i++) {
            const pulse = (i % 96) < 5;
            inst._set_inputs(0.5, 0.5, 0.5, 1, pulse, false, 0, 0, 1.0, 0);
            inst._process_sample();
            audioPeak = Math.max(audioPeak, Math.abs(inst._get_audio_out1()), Math.abs(inst._get_audio_out2()));
            cvPeak = Math.max(cvPeak, Math.abs(inst._get_cv_out1()), Math.abs(inst._get_cv_out2()));
        }
        for (let j = 0; j < 6; j++) ledPeak = Math.max(ledPeak, inst._get_led_brightness(j));
        r.tests.push({name: 'trigger', passed: true, message: 'OK', details: {audioPeak, cvPeak, ledPeak}});
    } catch(e) {
        r.tests.push({name: 'trigger', passed: false, message: 'Crash: ' + e.message});
        r.status = 'fail';
    }
    
    // Test 4: audio input
    try {
        let audioPeak = 0, cvPeak = 0, ledPeak = 0;
        for (let i = 0; i < N_EACH; i++) {
            inst._set_inputs(0.5, 0.5, 0.5, 1, true, false, 4.0, 4.0, 0, 0);
            inst._process_sample();
            audioPeak = Math.max(audioPeak, Math.abs(inst._get_audio_out1()), Math.abs(inst._get_audio_out2()));
            cvPeak = Math.max(cvPeak, Math.abs(inst._get_cv_out1()), Math.abs(inst._get_cv_out2()));
        }
        for (let j = 0; j < 6; j++) ledPeak = Math.max(ledPeak, inst._get_led_brightness(j));
        r.tests.push({name: 'audio_input', passed: true, message: 'OK', details: {audioPeak, cvPeak, ledPeak}});
    } catch(e) {
        r.tests.push({name: 'audio_input', passed: false, message: 'Crash: ' + e.message});
        r.status = 'fail';
    }
       // Test 5: stability
    try {
        let hasNaN = false;
        for (let i = 0; i < 2000; i++) {
            const pulse = (i % 96) < 5;
            inst._set_inputs(0.5, 0.5, 0.5, 1, pulse, false, 0.5, 0.5, 0.5, 0.5);
            inst._process_sample();
            if (isNaN(inst._get_audio_out1()) || !isFinite(inst._get_audio_out1())) { hasNaN = true; break; }
        }
        r.tests.push({name: 'stability', passed: !hasNaN, message: hasNaN ? 'NaN/Inf after extended run' : 'Stable'});
        if (hasNaN) r.status = 'fail';
    } catch(e) {
        r.tests.push({name: 'stability', passed: false, message: 'Crash: ' + e.message});
        r.status = 'fail';
    }
    
    // Test 6: Extreme Inputs (stress only)
    if (STRESS_MODE) {
        try {
            let hasNaN = false;
            for (let i = 0; i < 3000; i++) {
                inst._set_inputs(1.0, 1.0, 1.0, 2, true, true, 6.0, -6.0, 10.0, -10.0);
                inst._process_sample();
                const a1 = inst._get_audio_out1();
                if (isNaN(a1) || !isFinite(a1)) { hasNaN = true; break; }
            }
            r.tests.push({name: 'extreme_input', passed: !hasNaN, message: hasNaN ? 'NaN/Inf under extreme input' : 'Survived extreme inputs'});
            if (hasNaN) r.status = 'fail';
        } catch(e) {
            r.tests.push({name: 'extreme_input', passed: false, message: 'Crash: ' + e.message});
            r.status = 'fail';
        }
    }

    // Test 7: Full Switch Matrix (stress only)
    if (STRESS_MODE) {
        try {
            let matrixOk = true;
            const kvals = [0.0, 0.5, 1.0];
            for (let sw = 0; sw <= 2 && matrixOk; sw++) {
                for (const k0 of kvals) {
                    for (const k1 of kvals) {
                        for (const k2 of kvals) {
                            inst._set_inputs(k0, k1, k2, sw, true, false, 0.5, 0.5, 0.5, 0.5);
                            for (let s = 0; s < 100; s++) inst._process_sample();
                            const a1 = inst._get_audio_out1();
                            if (isNaN(a1) || !isFinite(a1)) { matrixOk = false; break; }
                        }
                        if (!matrixOk) break;
                    }
                    if (!matrixOk) break;
                }
            }
            r.tests.push({name: 'switch_matrix', passed: matrixOk, message: matrixOk ? 'All 81 combos OK' : 'NaN/Inf in switch matrix'});
            if (!matrixOk) r.status = 'fail';
        } catch(e) {
            r.tests.push({name: 'switch_matrix', passed: false, message: 'Crash: ' + e.message});
            r.status = 'fail';
        }
    }

    // Test 8: Extended Stability (stress only)
    if (STRESS_MODE) {
        try {
            let hasNaN = false;
            for (let i = 0; i < 20000; i++) {
                const pulse = (i % 96) < 5;
                inst._set_inputs(0.5, 0.5, 0.5, 1, pulse, false, 0.5, 0.5, 0.5, 0.5);
                inst._process_sample();
                if (i % 2000 === 0) {
                    const a1 = inst._get_audio_out1();
                    if (isNaN(a1) || !isFinite(a1)) { hasNaN = true; break; }
                }
            }
            r.tests.push({name: 'extended_stability', passed: !hasNaN, message: hasNaN ? 'NaN after extended run' : 'Stable after 20k samples'});
            if (hasNaN) r.status = 'fail';
        } catch(e) {
            r.tests.push({name: 'extended_stability', passed: false, message: 'Crash: ' + e.message});
            r.status = 'fail';
        }
    }

    const hasAudio = r.tests.some(t => t.details && t.details.audioPeak > A_THR);
    const hasCV = r.tests.some(t => t.details && t.details.cvPeak > C_THR);
    const hasLED = r.tests.some(t => t.details && t.details.ledPeak > L_THR);
    const hasFail = r.tests.some(t => !t.passed);
    if (hasFail) r.status = 'fail';
    else if (!hasAudio && !hasCV && !hasLED) r.status = 'silent';
    
    r.duration_ms = Date.now() - t0;
    return r;
}

main().catch(e => { console.log(JSON.stringify({error: e.message, results: []})); process.exit(0); });
"""


def run_patchnotes_tests(cards: List[CardInfo], stress: bool = False) -> List[CardTestReport]:
    """Run Patchnotes WASM tests headlessly via Node.js."""
    mode_str = " (STRESS MODE)" if stress else ""
    print(f"\n{BOLD}Patchnotes WASM Tests (via Node.js){mode_str}{RESET}")
    print("━" * 56)

    reports = []
    node_bin = shutil.which("node")
    if not node_bin:
        print(f"  {YELLOW}Node.js not found in PATH — skipping Patchnotes WASM tests{RESET}")
        for c in cards:
            reports.append(CardTestReport(
                card_id=c.id, card_num=c.num, card_name=c.name,
                platform="patchnotes", status="skip", error="Node.js not available"
            ))
        return reports

    if not os.path.exists(WASM_BINARY) or not os.path.exists(WASM_JS):
        print(f"  {YELLOW}WASM binary/JS missing — skipping Patchnotes WASM tests{RESET}")
        for c in cards:
            reports.append(CardTestReport(
                card_id=c.id, card_num=c.num, card_name=c.name,
                platform="patchnotes", status="skip", error="WASM binary missing"
            ))
        return reports

    from .conftest import WASM_ALIASES
    wasm_map = parse_wasm_card_map()
    cards_to_test = []
    cards_map = {}

    for c in cards:
        target_id = WASM_ALIASES.get(c.id, c.id)
        if target_id in wasm_map:
            idx = wasm_map[target_id]
            cards_to_test.append({"id": c.id, "idx": idx, "name": c.name, "num": c.num})
            cards_map[c.id] = c
        else:
            reports.append(CardTestReport(
                card_id=c.id, card_num=c.num, card_name=c.name,
                platform="patchnotes", status="skip", error=f"Card '{c.id}' not in WASM_CARD_MAP"
            ))

    print(f"  Loading WASM module from {DIM}{WASM_DIR}{RESET}...")
    print(f"  Testing {len(cards_to_test)} WASM cards...")

    # Write Node.js script
    script_content = NODE_SCRIPT_TEMPLATE.replace("__WASM_DIR__", json.dumps(WASM_DIR)).replace("__CARDS__", json.dumps(cards_to_test)).replace("__STRESS_MODE__", "true" if stress else "false")

    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False) as f:
        f.write(script_content)
        temp_script_path = f.name

    try:
        proc = subprocess.run(
            [node_bin, temp_script_path],
            capture_output=True, text=True, timeout=180
        )
        if proc.returncode != 0:
            print(f"  {RED}Node.js process failed with code {proc.returncode}:{RESET}\n{proc.stderr}")
            for c in cards_to_test:
                reports.append(CardTestReport(
                    card_id=c["id"], card_num=c["num"], card_name=c["name"],
                    platform="patchnotes", status="fail", error=f"Node process crashed: {proc.stderr[:200]}"
                ))
            return reports

        data = json.loads(proc.stdout)
        if data.get("error"):
            print(f"  {RED}WASM Test error: {data['error']}{RESET}")

        results = data.get("results", [])
        pass_count = 0
        fail_count = 0
        silent_count = 0

        for r in results:
            cid = r["id"]
            cinfo = cards_map.get(cid)
            cnum = cinfo.num if cinfo else r["num"]
            cname = cinfo.name if cinfo else r["name"]

            tests = [TestResult(t["name"], t["passed"], t["message"], t.get("details")) for t in r["tests"]]
            st = r["status"]
            if st == "pass":
                pass_count += 1
                st_icon = f"{GREEN}✅ PASS{RESET}"
            elif st == "silent":
                silent_count += 1
                st_icon = f"{YELLOW}🔇 SILENT{RESET}"
            else:
                fail_count += 1
                st_icon = f"{RED}❌ FAIL{RESET}"

            dur = r.get("duration_ms", 0)
            print(f"  [{cid:<22s}] (#{cnum})  {st_icon} ({dur:.0f}ms)")
            if st == "fail":
                for t in tests:
                    if not t.passed:
                        print(f"         └─ {t.name}: {DIM}{t.message}{RESET}")

            reports.append(CardTestReport(
                card_id=cid, card_num=cnum, card_name=cname,
                platform="patchnotes", status=st, tests=tests,
                error=r.get("error"), duration_ms=dur
            ))

        print(f"\n  Results: {GREEN}{pass_count} pass{RESET}, {YELLOW}{silent_count} silent{RESET}, {RED}{fail_count} fail{RESET}")

    except Exception as e:
        print(f"  {RED}Error running Node.js WASM test harness: {e}{RESET}")
    finally:
        if os.path.exists(temp_script_path):
            os.remove(temp_script_path)

    return reports
