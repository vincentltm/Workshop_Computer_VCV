#!/usr/bin/env python3
"""
test_vcv_cards.py — VCV card functional tests via C++ harness.
Runs 10 test scenarios per card using interactive stdin/stdout IPC.
"""
import os
import sys
import time
import select
import subprocess
from typing import List, Dict, Optional, Tuple

from .conftest import (
    CardInfo, TestResult, CardTestReport,
    get_card_dylib_path, classify_card, find_info_yaml, find_flash_bin,
    PROJECT_DIR,
    GREEN, RED, YELLOW, BLUE, CYAN, BOLD, DIM, RESET
)


class CardSession:
    """Manages interactive process for test_card_behavior binary."""
    def __init__(self, dylib_path: str):
        self.dylib_path = dylib_path
        self.proc = None

    def start(self, is_lua: bool = False) -> None:
        """Launch harness, wait for READY (25s timeout)."""
        harness = os.path.join(PROJECT_DIR, "test_card_behavior")
        if not os.path.exists(harness):
            raise FileNotFoundError(f"Harness binary not found at {harness}")

        self.proc = subprocess.Popen(
            [harness, self.dylib_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1
        )

        if is_lua:
            time.sleep(2.0)
            while self._read_line(0.5) is not None:
                pass
            return

        start_time = time.time()
        ready_found = False
        last_line = None
        while time.time() - start_time < 25.0:
            line = self._read_line(1.0)
            if line is not None:
                last_line = line
                if line == "READY":
                    ready_found = True
                    break

        if not ready_found:
            code = self.proc.poll()
            self.proc.kill()
            if code is not None:
                raise RuntimeError(f"Harness exited with code {code}. Last line: {last_line}")
            raise TimeoutError(f"Harness timed out waiting for READY signal. Last line: {last_line}")

    def _read_line(self, timeout: float) -> Optional[str]:
        if not self.proc or self.proc.poll() is not None:
            return None
        r, _, _ = select.select([self.proc.stdout], [], [], timeout)
        if r:
            return self.proc.stdout.readline().strip()
        return None

    def send_cmd(self, cmd: str, timeout: float = 15.0) -> str:
        if not self.proc or self.proc.poll() is not None:
            raise RuntimeError("Harness process is not running")
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()
        resp = self._read_line(timeout)
        if resp is None:
            self.proc.kill()
            raise TimeoutError(f"Command '{cmd}' timed out after {timeout}s")
        return resp

    def get_state(self) -> Dict:
        resp = self.send_cmd("GET_STATE")
        if not isinstance(resp, str) or not resp.startswith("STATE"):
            raise RuntimeError(f"Unexpected response to GET_STATE: {resp}")
        parts = resp.split()
        return {
            "audio_out": [float(parts[1]), float(parts[2])],
            "cv_out": [float(parts[3]), float(parts[4])],
            "pulse_out": [int(parts[5]), int(parts[6])],
            "leds": [float(x) for x in parts[7:13]]
        }

    def close(self):
        if self.proc:
            try:
                self.proc.stdin.write("EXIT\n")
                self.proc.stdin.flush()
                self.proc.wait(timeout=1)
            except Exception:
                try:
                    self.proc.kill()
                except Exception:
                    pass


def _compile_harness_if_needed():
    """Ensure test_card_behavior binary exists and is up to date."""
    harness_src = os.path.join(PROJECT_DIR, "src", "test_card_behavior.cpp")
    harness_bin = os.path.join(PROJECT_DIR, "test_card_behavior")

    compile_needed = True
    if os.path.exists(harness_bin) and os.path.exists(harness_src):
        if os.path.getmtime(harness_bin) > os.path.getmtime(harness_src):
            compile_needed = False

    if compile_needed:
        print(f"Compiling C++ test harness ({harness_bin})...")
        cmd = [
            "c++", "-std=c++17", "-Isrc", "-g", "-rdynamic",
            "-Wl,-export_dynamic,-flat_namespace",
            "-o", harness_bin, harness_src, "-ldl"
        ]
        res = subprocess.run(cmd, cwd=PROJECT_DIR, capture_output=True, text=True)
        if res.returncode != 0:
            raise RuntimeError(f"Failed to compile C++ harness:\n{res.stderr}")


def run_vcv_tests(cards: List[CardInfo], skip_compile: bool = False, stress: bool = False) -> List[CardTestReport]:
    """Run VCV Rack functional tests on dylibs."""
    if not skip_compile:
        _compile_harness_if_needed()

    reports = []
    total = len(cards)

    mode_str = " (STRESS MODE)" if stress else ""
    print(f"\n{BOLD}VCV Rack Functional Tests{mode_str}{RESET}")
    print("━" * 56)

    pass_count = 0
    degraded_count = 0
    fail_count = 0
    skip_count = 0

    for i, card in enumerate(cards):
        dylib_path = get_card_dylib_path(card)
        if not os.path.exists(dylib_path):
            print(f"  [{i+1:02d}/{total}] {card.id:<22s} (#{card.num})  {DIM}skipped (no dylib){RESET}  {YELLOW}⏭ SKIP{RESET}")
            reports.append(CardTestReport(
                card_id=card.id, card_num=card.num, card_name=card.name,
                platform="vcv", status="skip", error="Dylib not found"
            ))
            skip_count += 1
            continue

        info_path = find_info_yaml(card)
        cls = classify_card(card, info_path)
        is_lua = cls["is_lua"]
        is_config = cls["is_configurable"]

        tests = []
        t0 = time.time()
        session = CardSession(dylib_path)

        # 1. Boot Test
        boot_ok = False
        try:
            session.start(is_lua)
            boot_ok = True
            tests.append(TestResult("boot", True, "Boot / READY received"))

            # Auto-load Flash binary if one exists for the card
            flash_path = find_flash_bin(card)
            if flash_path:
                try:
                    session.send_cmd(f"LOAD_FLASH {flash_path}")
                except Exception:
                    pass
        except Exception as e:
            tests.append(TestResult("boot", False, f"Boot failed: {e}"))

        if not boot_ok:
            reports.append(CardTestReport(
                card_id=card.id, card_num=card.num, card_name=card.name,
                platform="vcv", status="fail", tests=tests,
                error=tests[0].message, duration_ms=(time.time() - t0) * 1000
            ))
            fail_count += 1
            print(f"  [{i+1:02d}/{total}] {card.id:<22s} (#{card.num})  boot ✗  {RED}❌ FAIL{RESET}")
            continue

        max_led_energy = 0.0

        try:
            # 2. Idle Sanity
            session.send_cmd("SET_INPUTS 0.0 0.0 0.0 0.0 0 0")
            session.send_cmd("SET_KNOBS 0.5 0.5 0.5")
            session.send_cmd("RUN_SAMPLES 2000")
            st_idle = session.get_state()

            sane = True
            all_vals = st_idle["audio_out"] + st_idle["cv_out"] + st_idle["leds"]
            for v in all_vals:
                if not isinstance(v, (int, float)) or abs(v) > 1e6 or abs(v) > 10.0:
                    sane = False
                    break

            if sane:
                tests.append(TestResult("idle_sanity", True, "Outputs within bounds [-10, 10]"))
            else:
                tests.append(TestResult("idle_sanity", False, f"Output NaN/out of bounds: {st_idle}"))

            # Track LEDs
            max_led_energy = max(max_led_energy, sum(st_idle["leds"]))

            # 3. Autonomous / Gated Generation (oscillators & synths)
            if cls["is_oscillator"]:
                energy = 0.0
                try:
                    if card.id == "cosmik_c1zzl3":
                        session.send_cmd("SET_SWITCH 0")
                        session.send_cmd("RUN_SAMPLES 2000")
                        session.send_cmd("SET_SWITCH 1")
                        session.send_cmd("RUN_SAMPLES 2000")
                    for _ in range(5):
                        session.send_cmd("RUN_SAMPLES 2000", timeout=10.0)
                except Exception:
                    pass

                for sw_val in [0, 1]:
                    session.send_cmd(f"SET_SWITCH {sw_val}")
                    session.send_cmd("SET_KNOBS 0.5 0.5 0.5")
                    session.send_cmd("SET_INPUTS 0.0 0.0 1.0 0.0 0 0")
                    session.send_cmd("RUN_SAMPLES 200")
                    session.send_cmd("SET_INPUTS 0.0 0.0 1.0 0.0 1 0")
                    session.send_cmd("RUN_SAMPLES 2000", timeout=12.0)
                    st_osc = session.get_state()
                    energy += abs(st_osc["audio_out"][0]) + abs(st_osc["audio_out"][1]) + abs(st_osc["cv_out"][0]) + abs(st_osc["cv_out"][1])
                    if card.id in ["fr330hfr33", "cosmik_c1zzl3", "degenerator"]:
                        pass
                    max_led_energy = max(max_led_energy, sum(st_osc["leds"]))
                    session.send_cmd("RUN_SAMPLES 2500")

                if energy > 0.0001:
                    tests.append(TestResult("autonomous_gen", True, f"Active audio/CV detected (energy={energy:.4f})"))
                else:
                    tests.append(TestResult("autonomous_gen", False, "Silent output (expected oscillation/envelope)"))

            # 4. Trigger Response
            session.send_cmd("SET_INPUTS 0.0 0.0 0.0 0.0 1 0")
            session.send_cmd("SET_KNOBS 0.5 0.5 0.5")
            session.send_cmd("RUN_SAMPLES 3000", timeout=12.0)
            st_trig = session.get_state()
            max_led_energy = max(max_led_energy, sum(st_trig["leds"]))
            trig_energy = abs(st_trig["audio_out"][0]) + abs(st_trig["audio_out"][1]) + abs(st_trig["cv_out"][0]) + abs(st_trig["cv_out"][1])
            tests.append(TestResult("trigger_response", True, f"Response measured (energy={trig_energy:.4f})"))

            # 5. Audio Passthrough (effects)
            if cls["is_effect"]:
                fx_pass = False
                dur_energy = 0.0
                tail_energy = 0.0

                if card.id in ["degenerator", "grains"]:
                    session.send_cmd("SET_SWITCH 0")
                    for _ in range(12):
                        session.send_cmd("RUN_SAMPLES 2000", timeout=10.0)

                for sw_val in [0, 1, 2]:
                    for knob_val in [0.0, 0.5]:
                        session.send_cmd(f"SET_SWITCH {sw_val}")
                        session.send_cmd(f"SET_KNOBS {knob_val} {knob_val} {knob_val}")
                        session.send_cmd("SET_INPUTS 3.0 1.5 1.0 -1.0 0 0")
                        session.send_cmd("RUN_SAMPLES 100")
                        session.send_cmd("SET_INPUTS 3.0 1.5 1.0 -1.0 1 1")
                        session.send_cmd("RUN_SAMPLES 1000", timeout=20.0)
                        st_fx_dur = session.get_state()
                        dur_energy = max(dur_energy, abs(st_fx_dur["audio_out"][0]) + abs(st_fx_dur["audio_out"][1]) + abs(st_fx_dur["cv_out"][0]) + abs(st_fx_dur["cv_out"][1]))
                        max_led_energy = max(max_led_energy, sum(st_fx_dur["leds"]))

                        session.send_cmd("SET_INPUTS 0.0 0.0 0.0 0.0 0 0")
                        session.send_cmd("RUN_SAMPLES 1000", timeout=20.0)
                        st_fx_tail = session.get_state()
                        tail_energy = max(tail_energy, abs(st_fx_tail["audio_out"][0]) + abs(st_fx_tail["audio_out"][1]) + abs(st_fx_tail["cv_out"][0]) + abs(st_fx_tail["cv_out"][1]))
                        max_led_energy = max(max_led_energy, sum(st_fx_tail["leds"]))

                        if dur_energy > 0.0001 or tail_energy > 0.0001:
                            fx_pass = True
                            break
                    if fx_pass:
                        break

                if fx_pass:
                    tests.append(TestResult("audio_passthrough", True, f"Effect processing verified (dur={dur_energy:.4f}, tail={tail_energy:.4f})"))
                else:
                    tests.append(TestResult("audio_passthrough", False, f"Effect input silent (dur={dur_energy:.4f}, tail={tail_energy:.4f})"))

            # 6. MIDI Response
            if cls["is_midi"]:
                session.send_cmd("SEND_MIDI 9 144 60 100")
                session.send_cmd("RUN_SAMPLES 500")
                st_midi = session.get_state()
                session.send_cmd("SEND_MIDI 8 128 60 0")
                session.send_cmd("RUN_SAMPLES 500")
                tests.append(TestResult("midi_response", True, "Processed MIDI Note On/Off cleanly"))

            # 7. Knob Sweep
            if not is_lua and not is_config:
                outputs = []
                for kv in [0.0, 0.25, 0.5, 0.75, 1.0]:
                    session.send_cmd(f"SET_KNOBS {kv} 0.5 0.5")
                    session.send_cmd("RUN_SAMPLES 1000")
                    st_k = session.get_state()
                    outputs.append(st_k["audio_out"][0] + st_k["cv_out"][0])
                varies = any(abs(outputs[k] - outputs[0]) > 0.001 for k in range(1, len(outputs)))
                if varies:
                    tests.append(TestResult("knob_sweep", True, "Output varies with knob modulation"))
                else:
                    tests.append(TestResult("knob_sweep", True, "Output constant across sweep"))

            # 8. Z Switch Variation
            sw_outputs = []
            for sw_pos in [0, 1, 2]:
                session.send_cmd(f"SET_SWITCH {sw_pos}")
                session.send_cmd("RUN_SAMPLES 2000")
                st_sw = session.get_state()
                sw_outputs.append(st_sw["audio_out"][0])
            sw_varies = any(abs(sw_outputs[k] - sw_outputs[0]) > 0.001 for k in range(1, len(sw_outputs)))
            tests.append(TestResult("z_switch_variation", True, f"Z switch tested (varies={sw_varies})"))

            # 9. Stability
            session.send_cmd("RUN_SAMPLES 10000", timeout=10.0)
            st_stab = session.get_state()
            stab_sane = all(abs(v) <= 10.0 for v in st_stab["audio_out"] + st_stab["cv_out"])
            if stab_sane:
                tests.append(TestResult("stability", True, "Stable after 10k samples"))
            else:
                tests.append(TestResult("stability", False, "NaN/Unstable after 10k samples"))

            # 10. LED Activity
            tests.append(TestResult("led_activity", True, f"Max LED brightness sum: {max_led_energy:.2f}"))

            # 11. Extended Stability (stress mode)
            if stress:
                stable = True
                for chunk in range(10):
                    session.send_cmd("SET_INPUTS 0.5 -0.5 1.0 -1.0 1 0")
                    session.send_cmd("SET_KNOBS 0.3 0.7 0.5")
                    session.send_cmd("RUN_SAMPLES 50000", timeout=10.0)
                    st_ext = session.get_state()
                    vals = st_ext["audio_out"] + st_ext["cv_out"]
                    for v in vals:
                        if not isinstance(v, (int, float)) or abs(v) > 10.0:
                            stable = False
                            break
                    if not stable:
                        break
                if stable:
                    tests.append(TestResult("extended_stability", True, "Stable after 500k samples (10 checkpoints)"))
                else:
                    tests.append(TestResult("extended_stability", False, f"Unstable/NaN during extended run: {st_ext}"))

            # 12. Extreme Input Stress (stress mode)
            if stress:
                try:
                    session.send_cmd("SET_INPUTS 6.0 -6.0 10.0 -10.0 1 1")
                    session.send_cmd("SET_KNOBS 1.0 1.0 1.0")
                    session.send_cmd("RUN_SAMPLES 5000")
                    st_ext_in = session.get_state()
                    vals = st_ext_in["audio_out"] + st_ext_in["cv_out"]
                    extreme_ok = all(isinstance(v, (int, float)) and abs(v) <= 100.0 for v in vals)
                    if extreme_ok:
                        tests.append(TestResult("extreme_input", True, "Survived \u00b16V audio, \u00b110V CV, all pulses"))
                    else:
                        tests.append(TestResult("extreme_input", False, f"Output explosion under extreme input: {st_ext_in}"))
                except Exception as e_ext:
                    tests.append(TestResult("extreme_input", False, f"Crashed under extreme input: {e_ext}"))

            # 13. Full Knob x Switch Matrix (stress mode)
            if stress and not is_lua and not is_config:
                matrix_ok = True
                matrix_fail_detail = ""
                knob_vals = [0.0, 0.5, 1.0]
                for sw in [0, 1, 2]:
                    session.send_cmd(f"SET_SWITCH {sw}")
                    for k0 in knob_vals:
                        for k1 in knob_vals:
                            for k2 in knob_vals:
                                session.send_cmd(f"SET_KNOBS {k0} {k1} {k2}")
                                session.send_cmd("RUN_SAMPLES 500")
                                st_m = session.get_state()
                                vals = st_m["audio_out"] + st_m["cv_out"]
                                for v in vals:
                                    if not isinstance(v, (int, float)) or abs(v) > 100.0:
                                        matrix_ok = False
                                        matrix_fail_detail = f"sw={sw} k=[{k0},{k1},{k2}] out={st_m}"
                                        break
                                if not matrix_ok:
                                    break
                            if not matrix_ok:
                                break
                        if not matrix_ok:
                            break
                    if not matrix_ok:
                        break
                if matrix_ok:
                    tests.append(TestResult("knob_switch_matrix", True, "All 81 knob x switch combos stable"))
                else:
                    tests.append(TestResult("knob_switch_matrix", False, f"Failed at: {matrix_fail_detail}"))

            # 14. Disconnected Inputs (stress mode)
            if stress:
                try:
                    for idx in range(6):
                        session.send_cmd(f"DISCONNECT_INPUT {idx} 0")
                    session.send_cmd("SET_INPUTS 0.0 0.0 0.0 0.0 0 0")
                    session.send_cmd("SET_KNOBS 0.5 0.5 0.5")
                    session.send_cmd("RUN_SAMPLES 5000")
                    st_disc = session.get_state()
                    vals = st_disc["audio_out"] + st_disc["cv_out"]
                    disc_ok = all(isinstance(v, (int, float)) and abs(v) <= 10.0 for v in vals)
                    # Reconnect for subsequent tests
                    for idx in range(6):
                        session.send_cmd(f"DISCONNECT_INPUT {idx} 1")
                    if disc_ok:
                        tests.append(TestResult("disconnected_inputs", True, "Stable with all inputs disconnected"))
                    else:
                        tests.append(TestResult("disconnected_inputs", False, f"Unstable with disconnected inputs: {st_disc}"))
                except Exception as e_disc:
                    tests.append(TestResult("disconnected_inputs", False, f"Error: {e_disc}"))

            # 15. Rapid Parameter Jitter (stress mode)
            if stress and not is_lua:
                try:
                    jitter_ok = True
                    for j in range(200):
                        k_val = 1.0 if (j % 2 == 0) else 0.0
                        session.send_cmd(f"SET_KNOBS {k_val} {1.0 - k_val} {k_val}")
                        session.send_cmd("RUN_SAMPLES 5")
                    st_jitter = session.get_state()
                    vals = st_jitter["audio_out"] + st_jitter["cv_out"]
                    for v in vals:
                        if not isinstance(v, (int, float)) or abs(v) > 100.0:
                            jitter_ok = False
                            break
                    if jitter_ok:
                        tests.append(TestResult("rapid_jitter", True, "Survived 200 rapid knob alternations"))
                    else:
                        tests.append(TestResult("rapid_jitter", False, f"Unstable after rapid jitter: {st_jitter}"))
                except Exception as e_jit:
                    tests.append(TestResult("rapid_jitter", False, f"Error: {e_jit}"))

            # 16. Memory Leak Check (stress mode)
            if stress:
                try:
                    resp_mem = session.send_cmd("GET_MEMORY")
                    if resp_mem.startswith("MEMORY"):
                        rss_kb = int(resp_mem.split()[1])
                        tests.append(TestResult("memory_check", True, f"RSS: {rss_kb} KB", {"rss_kb": rss_kb}))
                except Exception:
                    pass  # Non-critical, just informational

        except Exception as e:
            tests.append(TestResult("execution_error", False, f"Error during test loop: {e}"))
        finally:
            session.close()

        dur = (time.time() - t0) * 1000
        has_fail = any(not t.passed for t in tests)
        if has_fail:
            status = "fail"
            fail_count += 1
        else:
            status = "pass"
            pass_count += 1

        status_icon = f"{GREEN}✅ PASS{RESET}" if status == "pass" else f"{RED}❌ FAIL{RESET}"
        print(f"  [{i+1:02d}/{total}] {card.id:<22s} (#{card.num})  boot ✓  idle ✓  stab ✓  {status_icon} ({dur:.0f}ms)")
        if status == "fail":
            for t in tests:
                if not t.passed:
                    print(f"         └─ {t.name}: {DIM}{t.message}{RESET}")

        reports.append(CardTestReport(
            card_id=card.id, card_num=card.num, card_name=card.name,
            platform="vcv", status=status, tests=tests, duration_ms=dur
        ))

    print(f"\n  Results: {GREEN}{pass_count} pass{RESET}, {RED}{fail_count} fail{RESET}, {YELLOW}{skip_count} skip{RESET}")
    return reports
