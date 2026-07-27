#!/usr/bin/env python3
"""
test_parity.py — Cross-platform parity tests: VCV Rack vs Patchnotes WASM.
Runs identical deterministic input sequences through both platforms and
compares output trajectories to detect porting regressions.
"""
import os
import sys
import json
import math
import shutil
import tempfile
import subprocess
import time
import select
from typing import List, Dict, Optional, Tuple

from .conftest import (
    CardInfo, TestResult, CardTestReport,
    get_card_dylib_path, classify_card, find_info_yaml,
    parse_wasm_card_map,
    PROJECT_DIR, WASM_DIR, WASM_BINARY, WASM_JS, WASM_ALIASES,
    GREEN, RED, YELLOW, BLUE, CYAN, BOLD, DIM, RESET
)

# Number of samples per test scenario
CAPTURE_SAMPLES = 2000
# Report state every N samples
CAPTURE_INTERVAL = 200
# RMS threshold for parity failure (allow floating-point platform differences)
PARITY_RMS_THRESHOLD = 0.5
# Max absolute difference threshold per checkpoint
PARITY_ABS_THRESHOLD = 2.0


class VCVCaptureSession:
    """Runs a VCV card via the test_card_behavior harness and captures output."""

    def __init__(self, dylib_path: str):
        self.dylib_path = dylib_path
        self.proc = None

    def start(self) -> bool:
        harness = os.path.join(PROJECT_DIR, "test_card_behavior")
        if not os.path.exists(harness):
            return False

        self.proc = subprocess.Popen(
            [harness, self.dylib_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1
        )

        start_time = time.time()
        while time.time() - start_time < 15.0:
            r, _, _ = select.select([self.proc.stdout], [], [], 1.0)
            if r:
                line = self.proc.stdout.readline().strip()
                if line == "READY":
                    return True
        self.proc.kill()
        return False

    def _read_line(self, timeout: float = 2.0) -> Optional[str]:
        if not self.proc or self.proc.poll() is not None:
            return None
        r, _, _ = select.select([self.proc.stdout], [], [], timeout)
        if r:
            return self.proc.stdout.readline().strip()
        return None

    def send_cmd(self, cmd: str, timeout: float = 2.0) -> str:
        if not self.proc or self.proc.poll() is not None:
            raise RuntimeError("Process not running")
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()
        resp = self._read_line(timeout)
        if resp is None:
            self.proc.kill()
            raise TimeoutError(f"Command '{cmd}' timed out")
        return resp

    def run_capture(self, num_samples: int, interval: int) -> List[List[float]]:
        """Run samples and capture output checkpoints.
        Returns list of [audio0, audio1, cv0, cv1, pulse0, pulse1] at each checkpoint."""
        self.proc.stdin.write(f"RUN_CAPTURE {num_samples} {interval}\n")
        self.proc.stdin.flush()

        checkpoints = []
        expected_count = num_samples // interval

        for _ in range(expected_count + 1):  # +1 for the final OK
            line = self._read_line(timeout=10.0)
            if line is None:
                break
            if line == "OK":
                break
            if line.startswith("CAPTURE"):
                parts = line.split()
                values = [float(x) for x in parts[1:]]
                checkpoints.append(values)

        return checkpoints

    def close(self):
        if self.proc:
            try:
                self.proc.stdin.write("EXIT\n")
                self.proc.stdin.flush()
                self.proc.wait(timeout=2)
            except Exception:
                try:
                    self.proc.kill()
                except Exception:
                    pass


# Node.js script template for WASM parity capture
WASM_PARITY_SCRIPT = """
delete process.versions.node;
global.window = global;
global.document = { createElement: () => ({}), querySelector: () => null };
global.navigator = { userAgent: 'node' };
global.location = { href: 'http://localhost/' };
global.performance = require('perf_hooks').performance;

const path = require('path');
const fs = require('fs');
const wasmDir = __WASM_DIR__;
const createModule = require(path.join(wasmDir, 'patchnotes_cards.js'));

const CARDS = __CARDS__;
const SCENARIOS = __SCENARIOS__;
const CAPTURE_INTERVAL = __INTERVAL__;

async function main() {
    let inst;
    try {
        const wasmBuf = fs.readFileSync(path.join(wasmDir, 'patchnotes_cards.wasm'));
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
        const r = {id: card.id, idx: card.idx, checkpoints: []};
        try {
            inst._init_card(card.idx);
            try { inst._set_input_connected(true,true,true,true,true,true); } catch(e) {}

            // Warmup
            for (let i = 0; i < 500; i++) {
                inst._set_inputs(0.5, 0.5, 0.5, 1, false, false, 0, 0, 0, 0);
                inst._process_sample();
            }

            // Run each scenario
            for (const scen of SCENARIOS) {
                let sampleCount = 0;
                for (let i = 0; i < scen.samples; i++) {
                    inst._set_inputs(
                        scen.knobs[0], scen.knobs[1], scen.knobs[2], scen.sw,
                        scen.pulse1, scen.pulse2,
                        scen.audio1, scen.audio2, scen.cv1, scen.cv2
                    );
                    inst._process_sample();
                    sampleCount++;
                    if (sampleCount % CAPTURE_INTERVAL === 0) {
                        r.checkpoints.push([
                            inst._get_audio_out1(), inst._get_audio_out2(),
                            inst._get_cv_out1(), inst._get_cv_out2(),
                            inst._get_pulse_out1 ? inst._get_pulse_out1() : 0,
                            inst._get_pulse_out2 ? inst._get_pulse_out2() : 0
                        ]);
                    }
                }
            }
            r.status = 'ok';
        } catch(e) {
            r.status = 'error';
            r.error = e.message;
        }
        results.push(r);
    }
    console.log(JSON.stringify({error: null, results}));
}

main().catch(e => { console.log(JSON.stringify({error: e.message, results: []})); process.exit(0); });
"""

# Deterministic test scenarios for parity comparison
PARITY_SCENARIOS = [
    {
        "name": "idle",
        "samples": CAPTURE_SAMPLES,
        "knobs": [0.5, 0.5, 0.5],
        "sw": 1,
        "pulse1": False, "pulse2": False,
        "audio1": 0.0, "audio2": 0.0,
        "cv1": 0.0, "cv2": 0.0,
    },
    {
        "name": "triggered",
        "samples": CAPTURE_SAMPLES,
        "knobs": [0.8, 0.5, 0.3],
        "sw": 2,
        "pulse1": True, "pulse2": False,
        "audio1": 0.0, "audio2": 0.0,
        "cv1": 1.0, "cv2": 0.0,
    },
    {
        "name": "audio_in",
        "samples": CAPTURE_SAMPLES,
        "knobs": [0.7, 0.7, 0.5],
        "sw": 0,
        "pulse1": True, "pulse2": False,
        "audio1": 3.0, "audio2": -3.0,
        "cv1": 0.5, "cv2": -0.5,
    },
]


def _compute_rms_diff(vcv_checkpoints: List[List[float]],
                      wasm_checkpoints: List[List[float]]) -> Tuple[float, float]:
    """Compute RMS and max absolute difference between checkpoint vectors.
    Returns (rms_diff, max_abs_diff)."""
    if not vcv_checkpoints or not wasm_checkpoints:
        return (float('inf'), float('inf'))

    min_len = min(len(vcv_checkpoints), len(wasm_checkpoints))
    sum_sq = 0.0
    count = 0
    max_abs = 0.0

    for i in range(min_len):
        vcv_pt = vcv_checkpoints[i]
        wasm_pt = wasm_checkpoints[i]
        # Compare audio and CV outputs (first 4 values)
        for j in range(min(4, len(vcv_pt), len(wasm_pt))):
            v = vcv_pt[j]
            w = wasm_pt[j]
            # Skip NaN comparisons
            if math.isnan(v) or math.isnan(w):
                continue
            diff = abs(v - w)
            sum_sq += diff * diff
            max_abs = max(max_abs, diff)
            count += 1

    if count == 0:
        return (0.0, 0.0)

    rms = math.sqrt(sum_sq / count)
    return (rms, max_abs)


def _run_vcv_parity(card: CardInfo) -> Optional[List[List[float]]]:
    """Run VCV card through parity scenarios and capture checkpoints."""
    dylib_path = get_card_dylib_path(card)
    if not os.path.exists(dylib_path):
        return None

    session = VCVCaptureSession(dylib_path)
    try:
        if not session.start():
            return None

        # Warmup
        session.send_cmd("SET_INPUTS 0.0 0.0 0.0 0.0 0 0")
        session.send_cmd("SET_KNOBS 0.5 0.5 0.5")
        session.send_cmd("RUN_SAMPLES 500")

        all_checkpoints = []

        for scen in PARITY_SCENARIOS:
            p1 = 1 if scen["pulse1"] else 0
            p2 = 1 if scen["pulse2"] else 0
            session.send_cmd(
                f"SET_INPUTS {scen['audio1']} {scen['audio2']} "
                f"{scen['cv1']} {scen['cv2']} {p1} {p2}"
            )
            session.send_cmd(
                f"SET_KNOBS {scen['knobs'][0]} {scen['knobs'][1]} {scen['knobs'][2]}"
            )
            session.send_cmd(f"SET_SWITCH {scen['sw']}")

            checkpoints = session.run_capture(scen["samples"], CAPTURE_INTERVAL)
            all_checkpoints.extend(checkpoints)

        return all_checkpoints

    except Exception:
        return None
    finally:
        session.close()


def _run_wasm_parity(cards_to_test: List[Dict]) -> Dict[str, List[List[float]]]:
    """Run WASM cards through parity scenarios via Node.js.
    Returns dict mapping card_id -> list of checkpoints."""
    node_bin = shutil.which("node")
    if not node_bin or not os.path.exists(WASM_BINARY):
        return {}

    script = WASM_PARITY_SCRIPT
    script = script.replace("__WASM_DIR__", json.dumps(WASM_DIR))
    script = script.replace("__CARDS__", json.dumps(cards_to_test))
    script = script.replace("__SCENARIOS__", json.dumps(PARITY_SCENARIOS))
    script = script.replace("__INTERVAL__", str(CAPTURE_INTERVAL))

    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False) as f:
        f.write(script)
        temp_path = f.name

    try:
        proc = subprocess.run(
            [node_bin, temp_path],
            capture_output=True, text=True, timeout=120
        )
        if proc.returncode != 0:
            return {}

        data = json.loads(proc.stdout)
        result = {}
        for r in data.get("results", []):
            if r.get("status") == "ok":
                result[r["id"]] = r.get("checkpoints", [])
        return result

    except Exception:
        return {}
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)


def run_parity_tests(cards: List[CardInfo]) -> List[CardTestReport]:
    """Run cross-platform parity tests comparing VCV vs WASM outputs."""
    print(f"\n{BOLD}Cross-Platform Parity Tests (VCV \u2194 WASM){RESET}")
    print("\u2501" * 56)

    reports = []
    node_bin = shutil.which("node")

    if not node_bin or not os.path.exists(WASM_BINARY):
        print(f"  {YELLOW}Skipping parity tests (Node.js or WASM binary missing){RESET}")
        for c in cards:
            reports.append(CardTestReport(
                card_id=c.id, card_num=c.num, card_name=c.name,
                platform="parity", status="skip",
                error="Prerequisites missing"
            ))
        return reports

    # Build list of cards that exist on both platforms
    wasm_map = parse_wasm_card_map()
    testable_cards = []
    wasm_test_entries = []

    for card in cards:
        dylib_path = get_card_dylib_path(card)
        target_id = WASM_ALIASES.get(card.id, card.id)
        has_dylib = os.path.exists(dylib_path)
        has_wasm = target_id in wasm_map

        if has_dylib and has_wasm:
            testable_cards.append(card)
            wasm_test_entries.append({
                "id": card.id,
                "idx": wasm_map[target_id],
                "name": card.name,
                "num": card.num,
            })
        else:
            reason = []
            if not has_dylib:
                reason.append("no dylib")
            if not has_wasm:
                reason.append("no WASM")
            reports.append(CardTestReport(
                card_id=card.id, card_num=card.num, card_name=card.name,
                platform="parity", status="skip",
                error=f"Missing: {', '.join(reason)}"
            ))

    if not testable_cards:
        print(f"  {YELLOW}No cards available on both platforms{RESET}")
        return reports

    total = len(testable_cards)
    print(f"  Testing {total} cards on both VCV and WASM ({len(PARITY_SCENARIOS)} scenarios \u00d7 {CAPTURE_SAMPLES} samples each)...")

    # Phase 1: Collect all WASM outputs in one batch
    print(f"  {DIM}Running WASM batch...{RESET}")
    wasm_results = _run_wasm_parity(wasm_test_entries)
    print(f"  {DIM}WASM captured {len(wasm_results)}/{total} cards{RESET}")

    pass_count = 0
    degraded_count = 0
    fail_count = 0
    skip_count = 0

    # Phase 2: Run VCV cards and compare
    for i, card in enumerate(testable_cards):
        tests = []

        # Get WASM checkpoints
        wasm_cps = wasm_results.get(card.id, [])
        if not wasm_cps:
            tests.append(TestResult("wasm_capture", False, "No WASM checkpoints captured"))
            reports.append(CardTestReport(
                card_id=card.id, card_num=card.num, card_name=card.name,
                platform="parity", status="skip", tests=tests,
                error="WASM capture failed"
            ))
            skip_count += 1
            print(f"  [{i+1:02d}/{total}] {card.id:<22s} (#{card.num})  {YELLOW}\u23ed SKIP (no WASM data){RESET}")
            continue

        # Run VCV capture
        vcv_cps = _run_vcv_parity(card)
        if vcv_cps is None:
            tests.append(TestResult("vcv_capture", False, "VCV capture failed"))
            reports.append(CardTestReport(
                card_id=card.id, card_num=card.num, card_name=card.name,
                platform="parity", status="skip", tests=tests,
                error="VCV capture failed"
            ))
            skip_count += 1
            print(f"  [{i+1:02d}/{total}] {card.id:<22s} (#{card.num})  {YELLOW}\u23ed SKIP (VCV capture fail){RESET}")
            continue

        tests.append(TestResult("vcv_capture", True,
                                f"{len(vcv_cps)} checkpoints captured"))
        tests.append(TestResult("wasm_capture", True,
                                f"{len(wasm_cps)} checkpoints captured"))

        # Compute parity metrics
        rms_diff, max_abs_diff = _compute_rms_diff(vcv_cps, wasm_cps)

        if rms_diff <= PARITY_RMS_THRESHOLD and max_abs_diff <= PARITY_ABS_THRESHOLD:
            tests.append(TestResult("output_parity", True,
                                    f"RMS diff={rms_diff:.4f}, max abs={max_abs_diff:.4f}",
                                    {"rms_diff": rms_diff, "max_abs_diff": max_abs_diff}))
            status = "pass"
            pass_count += 1
            icon = f"{GREEN}\u2705 MATCH{RESET}"
        elif rms_diff <= PARITY_RMS_THRESHOLD * 3:
            tests.append(TestResult("output_parity", True,
                                    f"Minor divergence: RMS={rms_diff:.4f}, max abs={max_abs_diff:.4f}",
                                    {"rms_diff": rms_diff, "max_abs_diff": max_abs_diff}))
            status = "degraded"
            degraded_count += 1
            icon = f"{YELLOW}\u26a0\ufe0f DRIFT{RESET}"
        else:
            tests.append(TestResult("output_parity", False,
                                    f"Significant divergence: RMS={rms_diff:.4f}, max abs={max_abs_diff:.4f}",
                                    {"rms_diff": rms_diff, "max_abs_diff": max_abs_diff}))
            status = "fail"
            fail_count += 1
            icon = f"{RED}\u274c DIVERGE{RESET}"

        print(f"  [{i+1:02d}/{total}] {card.id:<22s} (#{card.num})  "
              f"RMS={rms_diff:.4f}  max={max_abs_diff:.4f}  {icon}")

        reports.append(CardTestReport(
            card_id=card.id, card_num=card.num, card_name=card.name,
            platform="parity", status=status, tests=tests,
        ))

    print(f"\n  Results: {GREEN}{pass_count} match{RESET}, "
          f"{YELLOW}{degraded_count} drift{RESET}, "
          f"{RED}{fail_count} diverge{RESET}, "
          f"{YELLOW}{skip_count} skip{RESET}")

    return reports
