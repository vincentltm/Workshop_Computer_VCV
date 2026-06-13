#!/usr/bin/env python3
import os
import subprocess
import glob
import sys

VCV_PROJECT_DIR = "/Users/vmaurer/Music/Workshop_Computer_VCV"

def main():
    os.chdir(VCV_PROJECT_DIR)

    # 1. Compile test_card_runner
    print("Compiling test_card_runner harness...")
    compile_cmd = [
        "c++", "-std=c++17", "-Isrc", "-g",
        "-o", "test_card_runner", "src/test_card_runner.cpp", "-ldl"
    ]
    try:
        subprocess.run(compile_cmd, check=True)
        print("Compiled test_card_runner successfully.\n")
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to compile test_card_runner: {e}", file=sys.stderr)
        sys.exit(1)

    # 2. Find all card dylibs in res/cards/
    dylib_pattern = os.path.join("res", "cards", "libcard_*.dylib")
    card_dylibs = sorted(glob.glob(dylib_pattern))

    if not card_dylibs:
        print("No card libraries found in res/cards/. Please build them first (e.g. with 'make').")
        sys.exit(1)

    print(f"Found {len(card_dylibs)} card libraries to test.")
    print("-" * 60)

    failed_cards = []
    passed_cards = []

    for dylib in card_dylibs:
        card_id = os.path.basename(dylib).replace("libcard_", "").replace(".dylib", "")
        print(f"Testing card: {card_id} ...")
        
        # Run test runner for this card
        run_cmd = ["./test_card_runner", dylib]
        try:
            res = subprocess.run(run_cmd, capture_output=True, text=True, timeout=5)
            if res.returncode == 0:
                print(f" -> {card_id}: PASS")
                passed_cards.append(card_id)
            else:
                print(f" -> {card_id}: FAIL (exit code {res.returncode})")
                print("--- Output ---")
                print(res.stdout)
                print(res.stderr)
                print("--------------")
                failed_cards.append((card_id, f"Exit code {res.returncode}"))
        except subprocess.TimeoutExpired:
            print(f" -> {card_id}: TIMEOUT (hung)")
            failed_cards.append((card_id, "Timeout (hung)"))
        except Exception as e:
            print(f" -> {card_id}: ERROR ({e})")
            failed_cards.append((card_id, f"Exception: {e}"))

    print("\n" + "=" * 60)
    print("Test Summary:")
    print(f"Passed: {len(passed_cards)} / {len(card_dylibs)}")
    print(f"Failed: {len(failed_cards)} / {len(card_dylibs)}")
    
    if failed_cards:
        print("\nFailed Cards:")
        for cid, reason in failed_cards:
            print(f" - {cid} ({reason})")
        sys.exit(1)
    else:
        print("\nAll cards passed testing successfully!")
        sys.exit(0)

if __name__ == "__main__":
    main()
