#!/usr/bin/env python3
import os
import re
import importlib
import sys
import json

# Source workspace paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
VCV_PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
WORKSPACE_DIR = os.path.join(VCV_PROJECT_DIR, "deps", "Workshop_Computer")
CARDS_SRC_DIR = os.path.join(VCV_PROJECT_DIR, "src", "cards")

# Add tools directory to sys.path so we can import porting modules
sys.path.append(os.path.dirname(__file__))

# Whitelist & Allowlist loaded dynamically from cards_registry.json
# Load master card registry from cards_registry.json
REGISTRY_FILE = os.path.join(VCV_PROJECT_DIR, "cards_registry.json")

def load_cards_registry():
    whitelist = []
    allowlist = []
    if os.path.exists(REGISTRY_FILE):
        with open(REGISTRY_FILE, "r") as f:
            registry = json.load(f)
        for card in registry:
            cid = card["id"]
            ns = card.get("ns")
            if not ns:
                # Special cases or standard capitalization
                if cid == "usb_audio_bridge": ns = "Card_USBAudio"
                elif cid == "ca_sequencer": ns = "Card_CASequencer"
                elif cid == "cosmik_c1zzl3": ns = "Card_CosmikC1zzl3"
                elif cid == "fr330hfr33": ns = "Card_Fr330hFr33"
                elif cid == "duo_midi": ns = "Card_DuoMidi"
                elif cid == "simple_midi": ns = "Card_SimpleMidi"
                elif cid == "bitphase": ns = "Card_BitPhase"
                elif cid == "eighties_bass": ns = "Card_EightiesBass"
                elif cid == "cirpy_wavetable": ns = "Card_CirpyWavetable"
                elif cid == "backyard_rain": ns = "Card_BackyardRain"
                elif cid == "utility_pair": ns = "Card_UtilityPair"
                elif cid == "turing_machine": ns = "Card_TuringMachine"
                elif cid == "resonator": ns = "Card_Resonator"
                elif cid == "byo_benjolin": ns = "Card_ByoBenjolin"
                elif cid == "computer_grids": ns = "Card_ComputerGrids"
                elif cid == "dual_quant": ns = "Card_DualQuant"
                elif cid == "tapegrade": ns = "Card_Tapegrade"
                elif cid == "wild_pebble": ns = "Card_WildPebble"
                elif cid == "castle_process": ns = "Card_CastleProcess"
                elif cid == "west_coast_lpg": ns = "Card_WestCoastLPG"
                elif cid == "turing_clouds": ns = "Card_TuringClouds"
                elif cid == "sense_of_space": ns = "Card_SenseOfSpace"
                else:
                    ns = "Card_" + "".join(x.capitalize() for x in cid.split("_"))
            
            item = dict(card)
            item["ns"] = ns
            if item.get("enabled", False):
                whitelist.append(item)
            else:
                allowlist.append(item)
    return whitelist, allowlist

CARD_WHITELIST, CARD_ALLOWLIST = load_cards_registry()





def fix_main_return(content):
    match = re.search(r'(?m)^\s*(?!//|/\*)\bint\s+main\s*\([^)]*\)', content)
    if not match:
        return content
    start_pos = match.end()
    open_brace_pos = content.find('{', start_pos)
    if open_brace_pos == -1:
        return content
    balance = 1
    pos = open_brace_pos + 1
    while pos < len(content) and balance > 0:
        if content[pos] == '{':
            balance += 1
        elif content[pos] == '}':
            balance -= 1
            if balance == 0:
                sub_block = content[open_brace_pos+1:pos]
                if not re.search(r'\breturn\b', sub_block):
                    return content[:pos] + "\n    return 0;\n" + content[pos:]
        pos += 1
    return content

def run_card_post_process(card_id, src_content, src_rel):
    if card_id == "sense_of_space":
        src_content = src_content.replace("db->buffer = malloc(", "db->buffer = (int32_t*)malloc(")
        src_content = src_content.replace("reverb *v = malloc(", "reverb *v = (reverb*)malloc(")
        src_content = src_content.replace("extern const int8_t seat_creak_data[];", "extern const int8_t* const seat_creak_data;")
        src_content = src_content.replace("extern const int8_t seat_creak_data_end[];", "extern const int8_t* const seat_creak_data_end;")
        src_content = src_content.replace("extern const int8_t sample_data[];", "extern const uint8_t* const sample_data;")
        src_content = src_content.replace("extern const int8_t sample_data_end[];", "extern const uint8_t* const sample_data_end;")
        src_content = src_content.replace("extern const uint8_t sample_data[];", "extern const uint8_t* const sample_data;")
        src_content = src_content.replace("extern const uint8_t sample_data_end[];", "extern const uint8_t* const sample_data_end;")
        src_content = "#ifndef FOUR33_SAMPLE_RATE\n#define FOUR33_SAMPLE_RATE 10000\n#endif\n" + src_content
    if card_id == "lens" and "snapshot_apply" in src_rel:
        src_content = src_content.replace("pack12_write", "pack12_write_snapshot")
    try:
        mod = importlib.import_module(f"porting.{card_id}")
        if hasattr(mod, "post_process"):
            return mod.post_process(src_content, src_rel)
    except ImportError:
        pass
    return src_content

def run_card_header_definitions(card_id):
    try:
        mod = importlib.import_module(f"porting.{card_id}")
        if hasattr(mod, "get_header_definitions"):
            return mod.get_header_definitions()
    except ImportError:
        pass
    return ""

def run_card_extra_definitions(card_id):
    try:
        mod = importlib.import_module(f"porting.{card_id}")
        if hasattr(mod, "get_extra_definitions"):
            return mod.get_extra_definitions()
    except ImportError:
        pass
    return ""

def main():
    os.makedirs(CARDS_SRC_DIR, exist_ok=True)
    
    # Load visible_cards.json configuration if present
    visible_config_path = os.path.join(SCRIPT_DIR, "visible_cards.json")
    visible_config = {}
    if os.path.exists(visible_config_path):
        try:
            with open(visible_config_path, 'r') as vf:
                visible_config = json.load(vf)
            print(f"Loaded card visibility configuration from {visible_config_path}")
        except Exception as e:
            print(f"Warning: could not parse {visible_config_path}: {e}")

    def is_card_visible(card):
        if not visible_config.get("enabled", True):
            return card.get("visible", True)
        cid = card["id"]
        cnum = str(card["num"]).zfill(2)
        folder = os.path.basename(card.get("dir", ""))
        excluded = set(str(x) for x in visible_config.get("excluded_cards", []))
        allowed = set(str(x) for x in visible_config.get("allowed_cards", []))
        if cid in excluded or cnum in excluded or card["num"] in excluded or folder in excluded:
            return False
        if allowed and (cid not in allowed and cnum not in allowed and card["num"] not in allowed and folder not in allowed):
            return False
        return card.get("visible", True)

    # Sort whitelist by card number
    CARD_WHITELIST.sort(key=lambda x: int(x["num"]))
    
    registry_entries = []
    card_data = {}
    
    for card in CARD_WHITELIST:
        if not is_card_visible(card):
            continue
            
        if card["id"] == "usb_audio_bridge":
            card_data[card["id"]] = {
                "sources": [os.path.join("src", "cards", "Card_usb_audio_bridge.cpp")],
                "flags": []
            }
            registry_entries.append({
                "id": card["id"],
                "name": "USB Audio Bridge",
                "num": card["num"],
                "desc": "Direct hardware bridge for the Workshop System Computer USB Audio card",
                "creator": "Music Thing Modular",
                "visible": is_card_visible(card)
            })
            continue

        if card["id"] == "compulidean":
            card_data[card["id"]] = {
                "sources": [os.path.join("src", "cards", "Card_compulidean.cpp")],
                "flags": []
            }
            registry_entries.append({
                "id": card["id"],
                "name": "Compulidean",
                "num": card["num"],
                "desc": "Drum machine / Euclidean generated drum patterns + drum machine.",
                "creator": "Tristan Rowley (semi-rewrite by Antigravity)",
                "visible": is_card_visible(card)
            })
            continue

        if card["dir"].startswith("/") or card["dir"].startswith("deps/"):
            card_dir_abs = os.path.join(VCV_PROJECT_DIR, card["dir"]) if not card["dir"].startswith("/") else card["dir"]
        else:
            card_dir_abs = os.path.join(WORKSPACE_DIR, card["dir"])
        if not os.path.exists(card_dir_abs):
            print(f"Warning: Card directory {card_dir_abs} does not exist. Skipping.")
            continue
            
        # Copy web folder or root HTML files if they exist
        web_src = None
        curr_dir = card_dir_abs
        while curr_dir and os.path.basename(curr_dir) != "releases" and curr_dir != "/":
            for folder_name in ["web", "web_config", "editor", "web_ui"]:
                potential_web = os.path.join(curr_dir, folder_name)
                if os.path.isdir(potential_web):
                    web_src = potential_web
                    break
            if web_src:
                break
            curr_dir = os.path.dirname(curr_dir)
            
        import shutil
        web_dest = os.path.join(VCV_PROJECT_DIR, "res", "web", card["id"])
        
        if web_src:
            if os.path.exists(web_dest):
                shutil.rmtree(web_dest)
            shutil.copytree(web_src, web_dest)
            print(f"Copied web UI files for {card['id']}")
        else:
            # Check for any .html files in card_dir_abs and parents up to releases
            curr_dir = card_dir_abs
            found_htmls = []
            while curr_dir and os.path.basename(curr_dir) != "releases" and curr_dir != "/":
                try:
                    html_files = [f for f in os.listdir(curr_dir) if f.endswith(".html") or f.endswith(".htm")]
                    if html_files:
                        for hf in html_files:
                            found_htmls.append(os.path.join(curr_dir, hf))
                        break
                except OSError:
                    pass
                curr_dir = os.path.dirname(curr_dir)
                
            if found_htmls:
                if os.path.exists(web_dest):
                    shutil.rmtree(web_dest)
                os.makedirs(web_dest, exist_ok=True)
                for fh in found_htmls:
                    shutil.copy(fh, os.path.join(web_dest, os.path.basename(fh)))
                print(f"Copied root HTML manager files for {card['id']}")
            
        if os.path.exists(web_dest):
            bridge_src = os.path.join(VCV_PROJECT_DIR, "res", "web", "vcv_web_bridge.js")
            if os.path.exists(bridge_src):
                shutil.copy(bridge_src, os.path.join(web_dest, "vcv_web_bridge.js"))
                for root, _, files in os.walk(web_dest):
                    for f_name in files:
                        if f_name.endswith(".html") or f_name.endswith(".htm"):
                            h_path = os.path.join(root, f_name)
                            try:
                                with open(h_path, "r", encoding="utf-8") as hf:
                                    h_content = hf.read()
                                if "vcv_web_bridge.js" not in h_content:
                                    if "<head>" in h_content:
                                        h_content = h_content.replace("<head>", "<head>\n    <script src=\"vcv_web_bridge.js\"></script>")
                                    elif "<html>" in h_content:
                                        h_content = h_content.replace("<html>", "<html>\n<head><script src=\"vcv_web_bridge.js\"></script></head>")
                                    else:
                                        h_content = "<script src=\"vcv_web_bridge.js\"></script>\n" + h_content
                                    with open(h_path, "w", encoding="utf-8") as hf:
                                        hf.write(h_content)
                            except Exception as e:
                                print(f"Error injecting web bridge into {h_path}: {e}")
        # Dynamically scan and append pipicofx sources for flux (only those compiled in CMakeLists.txt)
        if card["id"] == "flux":
            cm_path = os.path.join(card_dir_abs, "CMakeLists.txt")
            cm_content = ""
            if os.path.exists(cm_path):
                with open(cm_path, 'r') as cm_f:
                    cm_content = cm_f.read()
            pipico_src = os.path.join(card_dir_abs, "lib", "pipicofx", "src")
            if os.path.exists(pipico_src):
                for f_name in sorted(os.listdir(pipico_src)):
                    if f_name.endswith(".c"):
                        if f_name.lower() in cm_content.lower():
                            if f_name == "romfunc.c":
                                continue
                            rel_path = os.path.join("lib", "pipicofx", "src", f_name)
                            if rel_path not in card["sources"]:
                                card["sources"].append(rel_path)

        print(f"Porting card: {card['id']} in {card['dir']}")
        
        # Read info.yaml for metadata
        info_dir = card.get("info_dir", card["dir"])
        if info_dir.startswith("/") or info_dir.startswith("deps/"):
            info_dir_abs = os.path.join(VCV_PROJECT_DIR, info_dir) if not info_dir.startswith("/") else info_dir
        else:
            info_dir_abs = os.path.join(WORKSPACE_DIR, info_dir)
        info_path = os.path.join(info_dir_abs, "info.yaml")
        if not os.path.exists(info_path):
            parent_info = os.path.join(os.path.dirname(info_dir_abs), "info.yaml")
            if os.path.exists(parent_info):
                info_path = parent_info
                
        metadata = {
            "name": card["id"].replace("_", " ").title(),
            "desc": "Workshop Computer Card",
            "creator": "Music Thing Modular"
        }
        
        if os.path.exists(info_path):
            try:
                with open(info_path, 'r', encoding='utf-8', errors='ignore') as f:
                    lines = f.readlines()
                i = 0
                while i < len(lines):
                    line_rstrip = lines[i].rstrip()
                    stripped = line_rstrip.strip()
                    if not stripped or stripped.startswith('#'):
                        i += 1
                        continue
                    indent = len(line_rstrip) - len(line_rstrip.lstrip())
                    if ":" in line_rstrip:
                        key, val = line_rstrip.split(":", 1)
                        key = key.strip().lower()
                        val = val.strip().strip('"').strip("'")

                        if key in ["title", "name"] and val and not metadata.get("found_title"):
                            metadata["name"] = val
                            metadata["found_title"] = True
                        elif key in ["short-description", "summary", "description"] and not metadata.get("found_desc"):
                            if val in ["|", ">", "|-", ">-"] or not val:
                                multiline_val = []
                                i += 1
                                while i < len(lines):
                                    sub_line = lines[i].rstrip()
                                    sub_stripped = sub_line.strip()
                                    if not sub_stripped:
                                        multiline_val.append("")
                                        i += 1
                                        continue
                                    sub_indent = len(sub_line) - len(sub_line.lstrip())
                                    if sub_indent > indent:
                                        multiline_val.append(sub_stripped)
                                        i += 1
                                    else:
                                        i -= 1
                                        break
                                res_desc = " ".join(" ".join(multiline_val).split())
                                if res_desc:
                                    metadata["desc"] = res_desc
                                    metadata["found_desc"] = True
                            else:
                                metadata["desc"] = val
                                metadata["found_desc"] = True
                        elif key == "creator" and val and not metadata.get("found_creator"):
                            metadata["creator"] = val
                            metadata["found_creator"] = True
                    i += 1
            except Exception as e:
                print(f"Error parsing info.yaml for {card['id']}: {e}")
                
        # Detect sample rate divisor
        sample_rate_div = 1
        cm_path = os.path.join(card_dir_abs, "CMakeLists.txt")
        if os.path.exists(cm_path):
            try:
                with open(cm_path, 'r') as f:
                    cm_content = f.read()
                    m = re.search(r'COMPUTERCARD_SAMPLE_RATE_DIV=(\d+)', cm_content)
                    if m:
                        sample_rate_div = int(m.group(1))
            except Exception:
                pass
        # Helper to resolve source path, prioritizing local VCV wrappers
        def get_source_path(s):
            card_src_path = os.path.join(card_dir_abs, s)
            if os.path.exists(card_src_path):
                return card_src_path
            if s.startswith("src/"):
                return os.path.join(VCV_PROJECT_DIR, s)
            vcv_wrapper_path = os.path.join(VCV_PROJECT_DIR, "src", "cards", "wrappers", card["id"], s)
            if os.path.exists(vcv_wrapper_path):
                return vcv_wrapper_path
            if s.startswith("lua/"):
                lua_path = os.path.join(VCV_PROJECT_DIR, "deps", "external", "blackbird_lua", s)
                if os.path.exists(lua_path):
                    return lua_path
            return card_src_path

        for s in card["sources"]:
            src_path_abs = get_source_path(s)
            if os.path.exists(src_path_abs):
                try:
                    with open(src_path_abs, 'r') as f:
                        content = f.read()
                        m = re.search(r'#define\s+COMPUTERCARD_SAMPLE_RATE_DIV\s+(\d+)', content)
                        if m:
                            sample_rate_div = int(m.group(1))
                except Exception:
                    pass

        # Parse all sources to find system includes & replace flash literals
        system_includes = set()
        system_includes.add('<algorithm>')
        system_includes.add('<cmath>')
        system_includes.add('<stdint.h>')
        
        # Split into unity_sources and separate_sources
        unity_sources = []
        separate_sources = []
        for s in card["sources"]:
            if (card["id"] == "flux" and s.startswith("lib/pipicofx/src/")) or \
               (card["id"] == "twists" and s != "braids/twists.cc") or \
               (card["id"] == "voices_of_sid" and s.startswith("reSID/")) or \
               (card["id"] in ("blackbird", "krell", "duo_midi") and s != "main.cpp"):
                separate_sources.append(s)
            else:
                unity_sources.append(s)
                
        wrapper_filename = f"Card_{card['id']}.cpp"
        wrapper_path = os.path.join(CARDS_SRC_DIR, wrapper_filename)
        
        # Gather card-specific include directories
        card_include_flags = []
        
        # Add local VCV wrappers directory and all subdirectories first so they take priority
        wrapper_dir_abs = os.path.join(VCV_PROJECT_DIR, "src", "cards", "wrappers", card["id"])
        if os.path.exists(wrapper_dir_abs):
            card_include_flags.append(f"-I{wrapper_dir_abs}")
            for root, dirs, files in os.walk(wrapper_dir_abs):
                if "build" in dirs:
                    dirs.remove("build")
                for d in dirs:
                    card_include_flags.append(f"-I{os.path.join(root, d)}")
                    
        # Add original card source directory and all valid subdirectories second
        card_include_flags.append(f"-I{card_dir_abs}")
        if os.path.exists(card_dir_abs):
            for root, dirs, files in os.walk(card_dir_abs):
                for d in list(dirs):
                    if " " in d or d in ("build", ".git", "test", "web", "docs", "tools", "__pycache__", "UF2", "assets_tmp", "Documentation", "Docs"):
                        dirs.remove(d)
                    else:
                        card_include_flags.append(f"-I{os.path.join(root, d)}")

        # Allow porting modules to inject extra include directories
        try:
            mod = importlib.import_module(f"porting.{card['id']}")
            if hasattr(mod, "get_extra_include_dirs"):
                for extra_dir in mod.get_extra_include_dirs(card_dir_abs):
                    card_include_flags.append(f"-I{extra_dir}")
            if hasattr(mod, "get_extra_compiler_flags"):
                card_include_flags.extend(mod.get_extra_compiler_flags())
        except ImportError:
            pass
                        
        # Convert include paths to relative paths
        def to_rel(path):
            if os.path.isabs(path):
                if path.startswith(VCV_PROJECT_DIR):
                    return os.path.relpath(path, VCV_PROJECT_DIR)
            return path
        card_include_flags = [f"-I{to_rel(flag[2:])}" if flag.startswith("-I") else flag for flag in card_include_flags]

        # Track sources for Makefile compilation
        sources_to_compile = [os.path.join("src", "cards", wrapper_filename)]
            
        # Check if the card source code defines the host callbacks
        defines_midi_mount = False
        defines_midi_rx = False
        defines_midi_umount = False
        for src_rel in unity_sources:
            src_path_abs = get_source_path(src_rel)
            if os.path.exists(src_path_abs):
                with open(src_path_abs, 'r') as src_f:
                    src_content = src_f.read()
                    if "tuh_midi_mount_cb" in src_content:
                        defines_midi_mount = True
                    if "tuh_midi_rx_cb" in src_content:
                        defines_midi_rx = True
                    if "tuh_midi_umount_cb" in src_content:
                        defines_midi_umount = True

        # Write the wrapped unity file
        # Skip regeneration for cards with hand-crafted wrappers (e.g. grains)
        if card.get("manual_rewrite"):
            print(f"Skipping auto-generation for {card['id']} (manual_rewrite=true, preserving existing wrapper)")
        else:
         with open(wrapper_path, 'w') as out_f:
          if True:  # indent shim
            out_f.write(f"#define COMPUTERCARD_SAMPLE_RATE_DIV {sample_rate_div}\n")
            out_f.write("#include <stdint.h>\n")
            out_f.write("#include <stddef.h>\n")
            out_f.write("#include <stdlib.h>\n")
            out_f.write("#include <math.h>\n")
            out_f.write("#include <algorithm>\n")
            out_f.write("#include <vector>\n")
            out_f.write("#include <string>\n")
            out_f.write("#include <atomic>\n")
            out_f.write("#include <thread>\n")
            out_f.write("#ifndef _WIN32\n#include <dlfcn.h>\n#endif\n")
            out_f.write("#include <fstream>\n")
            out_f.write("#include <iostream>\n")
            out_f.write("#include <sstream>\n")
            out_f.write("#include <stdio.h>\n")
            out_f.write("#include <string.h>\n")
            out_f.write("#include <cstring>\n")
            out_f.write("#include <stdarg.h>\n")
            out_f.write("#include <limits.h>\n")
            out_f.write("#include <float.h>\n")
            out_f.write("#include <setjmp.h>\n")
            out_f.write("#include <time.h>\n")
            out_f.write("#include <errno.h>\n")
            out_f.write("#include <locale.h>\n")
            out_f.write("#include <inttypes.h>\n")
            out_f.write("#include <cinttypes>\n")
            out_f.write("#include <array>\n")
            out_f.write("#include <cmath>\n")
            out_f.write("#include <complex>\n")
            out_f.write("#include <random>\n")
            out_f.write("#include <functional>\n")
            out_f.write("#include <queue>\n")
            out_f.write("#include <deque>\n")
            out_f.write("#include <map>\n")
            out_f.write("#include <set>\n")
            out_f.write("#include <unordered_map>\n")
            out_f.write("#include <unordered_set>\n")
            out_f.write("#include <utility>\n")
            out_f.write("#include <numeric>\n")
            out_f.write("#include <initializer_list>\n")
            out_f.write("#include \"pico_mocks.h\"\n")
            out_f.write("#include \"tusb.h\"\n")
            out_f.write("#define while(...) while((__VA_ARGS__) && !g_cancellation_requested.load(std::memory_order_relaxed))\n\n")
            out_f.write("#include \"ComputerCard.h\"\n\n")
            out_f.write("#define _Static_assert static_assert\n")
            out_f.write("namespace std { using ::sinf; using ::cosf; using ::tanf; using ::asinf; using ::acosf; using ::atanf; using ::atan2f; using ::sqrtf; using ::expf; using ::logf; using ::log10f; using ::powf; }\n\n")
            
            out_f.write("// DSO-local thread-local variables definition\n")
            out_f.write("thread_local CardGlobals* t_instance = nullptr;\n")
            out_f.write("thread_local bool is_core1_thread = false;\n")
            out_f.write("thread_local ComputerCard* ComputerCard::thisptr = nullptr;\n\n")
            
            # Open namespace
            out_f.write(f"namespace {card['ns']} {{\n\n")
            
            # Declare main to prevent symbol resolution compiler errors
            out_f.write("    int main();\n\n")
            
            # Write dynamic header declarations
            headers_code = run_card_header_definitions(card["id"])
            if headers_code:
                out_f.write(headers_code + "\n")
            
            # Now dump and process each unity source file
            has_sources = True
            for src_rel in unity_sources:
                src_path_abs = get_source_path(src_rel)
                if not os.path.exists(src_path_abs):
                    print(f"Notice: Source file not found locally for {card['id']}: {src_path_abs}, skipping wrapper generation.")
                    has_sources = False
                    break
                
                out_f.write(f"// ──────────────────────────────────────────────────────────────────────────────\n")
                out_f.write(f"// Source: {src_rel}\n")
                out_f.write(f"// ──────────────────────────────────────────────────────────────────────────────\n\n")
                
                with open(src_path_abs, 'r') as src_f:
                    src_content = src_f.read()
                    
                    # Extract system includes
                    includes = re.findall(r'#include\s+<([^>]+)>', src_content)
                    for inc in includes:
                        system_includes.add(f"<{inc}>")
                        
                    # Extract local pico SDK includes: e.g. #include "pico/stdlib.h"
                    pico_includes = re.findall(r'#include\s+"(pico/[^"]+)"', src_content)
                    for pinc in pico_includes:
                        system_includes.add(f'"{pinc}"')
                    pico_hardware = re.findall(r'#include\s+"(hardware/[^"]+)"', src_content)
                    for hinc in pico_hardware:
                        system_includes.add(f'"{hinc}"')
                    
                    # Replace flash literals: any 0x10xxxxxx -> XIP_BASE + 0x00xxxxxx
                    src_content = re.sub(r'\b0x10([0-9a-fA-F]{6})\b', r'(XIP_BASE + 0x00\1)', src_content)
                    src_content = re.sub(r'\b0x11([0-9a-fA-F]{6})\b', r'(XIP_BASE + 0x01\1)', src_content)
                    # Replace pointer casts to uint32_t with uintptr_t for 64-bit portability
                    src_content = src_content.replace('(uint32_t)header', '(uintptr_t)header')
                    # Replace flash macros for Flux: FLASH_SAMPLES_BASE -> (XIP_BASE + 0x00180000), FLASH_SETTINGS_BASE -> (XIP_BASE + 0x0017f000)
                    src_content = src_content.replace('FLASH_SAMPLES_BASE', '(XIP_BASE + 0x00180000)')
                    src_content = src_content.replace('FLASH_SETTINGS_BASE', '(XIP_BASE + 0x0017f000)')
                    # Rename embedded Lua clock bytecode array symbol to avoid colliding with C library clock(void) function
                    src_content = src_content.replace('const unsigned char clock[', 'extern "C" const unsigned char crow_lua_clock_data[')
                    src_content = src_content.replace('extern const unsigned char clock[', 'extern "C" const unsigned char crow_lua_clock_data[')
                    src_content = src_content.replace('extern const unsigned int clock_len;', 'extern "C" const unsigned int crow_lua_clock_data_len;')
                    src_content = src_content.replace('"lua_clock"     , clock', '"lua_clock"     , crow_lua_clock_data')
                    src_content = src_content.replace('"lua_clock"   , clock', '"lua_clock"   , crow_lua_clock_data')
                    src_content = src_content.replace('"lua_clock", clock', '"lua_clock", crow_lua_clock_data')
                    
                    # Replace hardware ARM assembly instructions with portable memory barrier macros
                    src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"dmb"\s*:::\s*"memory"\s*\);?', r'asm volatile("" ::: "memory");', src_content)
                    src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"dsb"\s*:::\s*"memory"\s*\);?', r'asm volatile("" ::: "memory");', src_content)
                    src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"dmb"\s*\);?', r'asm volatile("" ::: "memory");', src_content)
                    src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"dsb"\s*\);?', r'asm volatile("" ::: "memory");', src_content)
                    src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"bkpt\s+[^"]+"\s*\);?', r'/* bkpt */', src_content)
                    src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"mov\s+%0,\s*lr"\s*:\s*"=r"\([^)]+\)\s*\);?', r'/* mov lr */', src_content)
                    src_content = src_content.replace('__asm volatile("dmb" ::: "memory");', 'asm volatile("" ::: "memory");')
                    src_content = src_content.replace('__asm volatile("dmb");', 'asm volatile("" ::: "memory");')
                    
                    # Fix sleep_us / busy_wait_us no-op spin loops in VCV Rack desktop builds
                    src_content = re.sub(r'\bbusy_wait_us\s*\(\s*([0-9]+)\s*\)', r'busy_wait_us_32(\1)', src_content)
                    src_content = re.sub(r'\bsleep_us\s*\(\s*([0-9]+)\s*\)', r'busy_wait_us_32(\1)', src_content)
                    
                    # Fix missing return 0 in main() to avoid UB / EXC_BREAKPOINT under -O3
                    src_content = fix_main_return(src_content)
                    
                    # Delegate card-specific post-processing
                    src_content = run_card_post_process(card["id"], src_content, src_rel)
                    
                    # Strip linker section attributes that break Clang Mach-O compilation on macOS
                    src_content = re.sub(r'__attribute__\s*\(\s*\(\s*section\s*\([^)]+\)\s*\)\s*\)', '', src_content)
                    
                    # Strip standard includes to avoid double compilation within namespace (since they are global)
                    src_content = re.sub(r'#include\s+<[^>]+>', '/* stripped system include */', src_content)
                    src_content = re.sub(r'#include\s+"pico/[^"]+"', '/* stripped pico include */', src_content)
                    src_content = re.sub(r'#include\s+"hardware/[^"]+"', '/* stripped hardware include */', src_content)
                    src_content = re.sub(r'#include\s+"bsp/[^"]+"', '/* stripped bsp include */', src_content)
                    src_content = re.sub(r'#include\s+"usb_midi_host\.h"', '/* stripped usb_midi_host include */', src_content)
                    
                    # Strip duplicate ComputerCard.h inclusions
                    src_content = re.sub(r'#include\s+"ComputerCard.h"', '/* stripped ComputerCard include */', src_content)
                    src_content = re.sub(r'#include\s+"ComputerCard/ComputerCard.h"', '/* stripped ComputerCard include */', src_content)
                    src_content = re.sub(r'#include\s+"tusb\.h"', '/* stripped tusb include */', src_content)
                    src_content = re.sub(r'#include\s+<tusb\.h>', '/* stripped tusb include */', src_content)
                    src_content = re.sub(r'#include\s+"tusb_config\.h"', '/* stripped tusb_config include */', src_content)
                    
                    out_f.write(src_content)
                    out_f.write("\n\n")
            
            # Write dynamic extra definitions
            extra_code = run_card_extra_definitions(card["id"])
            if extra_code:
                out_f.write(extra_code + "\n")
                    
            # Close namespace
            out_f.write(f"}} // namespace {card['ns']}\n\n")
            
            # Write global DSO exports
            out_f.write('extern "C" {\n')
            if not defines_midi_mount:
                out_f.write("    __attribute__((weak)) void tuh_midi_mount_cb(uint8_t, uint8_t, uint8_t, uint8_t, uint16_t) {}\n")
            if not defines_midi_rx:
                out_f.write("    __attribute__((weak)) void tuh_midi_rx_cb(uint8_t, uint32_t) {}\n")
            if not defines_midi_umount:
                out_f.write("    __attribute__((weak)) void tuh_midi_umount_cb(uint8_t, uint8_t) {}\n")
            out_f.write("    void set_thread_globals(CardGlobals* inst) {\n")
            out_f.write("        t_instance = inst;\n")
            out_f.write("        if (inst) {\n")
            out_f.write("            if (!inst->card_ptr && ComputerCard::thisptr) {\n")
            out_f.write("                inst->card_ptr = ComputerCard::thisptr;\n")
            out_f.write("            }\n")
            out_f.write("            ComputerCard::thisptr = inst->card_ptr;\n")
            out_f.write("        }\n")
            out_f.write("    }\n")
            out_f.write("    void set_core1_thread(bool is_core1) {\n")
            out_f.write("        is_core1_thread = is_core1;\n")
            out_f.write("    }\n")
            out_f.write("    void run_card() {\n")
            out_f.write("        is_core1_thread = false;\n")
            out_f.write("        try {\n")
            out_f.write(f"            {card['ns']}::main();\n")
            out_f.write("        } catch (const ThreadExitException& e) {\n")
            out_f.write("            // Thread terminated safely\n")
            out_f.write("        }\n")
            out_f.write("    }\n")
            out_f.write("}\n")
            
        # Now process separate_sources (if any)
        for sep_src in separate_sources:
            sep_filename = f"Card_{card['id']}_" + sep_src.replace("/", "_").replace(".", "_") + ".cpp"
            sep_path = os.path.join(CARDS_SRC_DIR, sep_filename)
            sep_path_abs = get_source_path(sep_src)
            
            if not os.path.exists(sep_path_abs):
                print(f"Error: Separate source file {sep_path_abs} not found!")
                continue
                
            with open(sep_path_abs, 'r') as src_f:
                src_content = src_f.read()
                
                # Apply standard replacements
                src_content = re.sub(r'\b0x10([0-9a-fA-F]{6})\b', r'(XIP_BASE + 0x00\1)', src_content)
                src_content = re.sub(r'\b0x11([0-9a-fA-F]{6})\b', r'(XIP_BASE + 0x01\1)', src_content)
                src_content = src_content.replace('(uint32_t)header', '(uintptr_t)header')
                src_content = src_content.replace('FLASH_SAMPLES_BASE', '(XIP_BASE + 0x00180000)')
                src_content = src_content.replace('FLASH_SETTINGS_BASE', '(XIP_BASE + 0x0017f000)')
                # Replace hardware dmb assembly blocks with portable memory barrier macros
                src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"dmb"\s*:::\s*"memory"\s*\);?', r'asm volatile("" ::: "memory");', src_content)
                src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"dsb"\s*:::\s*"memory"\s*\);?', r'asm volatile("" ::: "memory");', src_content)
                src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"dmb"\s*\);?', r'asm volatile("" ::: "memory");', src_content)
                src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"dsb"\s*\);?', r'asm volatile("" ::: "memory");', src_content)
                src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"bkpt\s+[^"]+"\s*\);?', r'/* bkpt */', src_content)
                src_content = re.sub(r'__asm__?\s+volatile\s*\(\s*"mov\s+%0,\s*lr"\s*:\s*"=r"\([^)]+\)\s*\);?', r'/* mov lr */', src_content)
                # Delegate card-specific post-processing
                src_content = run_card_post_process(card["id"], src_content, sep_src)
                
                # Strip linker section attributes that break Clang Mach-O compilation on macOS
                src_content = re.sub(r'__attribute__\s*\(\s*\(\s*section\s*\([^)]+\)\s*\)\s*\)', '', src_content)
                
                with open(sep_path, 'w') as sep_f:
                    # C++ wrapper: full C++ headers wrapped in card namespace
                    sep_f.write("// Automatically generated separate compilation wrapper\n")
                    sep_f.write("#include <stdint.h>\n")
                    sep_f.write("#include <stddef.h>\n")
                    sep_f.write("#include <stdlib.h>\n")
                    sep_f.write("#include <math.h>\n")
                    sep_f.write("#include <algorithm>\n")
                    sep_f.write("#include <vector>\n")
                    sep_f.write("#include <string>\n")
                    sep_f.write("#include <atomic>\n")
                    sep_f.write("#include <thread>\n")
                    sep_f.write("#include <stdio.h>\n")
                    sep_f.write("#include <string.h>\n")
                    sep_f.write("#include <cstring>\n")
                    sep_f.write("#include <stdarg.h>\n")
                    sep_f.write("#include <limits.h>\n")
                    sep_f.write("#include <float.h>\n")
                    sep_f.write("#include <setjmp.h>\n")
                    sep_f.write("#include <time.h>\n")
                    sep_f.write("#include <errno.h>\n")
                    sep_f.write("#include <locale.h>\n")
                    sep_f.write("#include <inttypes.h>\n")
                    sep_f.write("#include <cinttypes>\n")
                    sep_f.write("#include \"pico_mocks.h\"\n")
                    sep_f.write("#include \"tusb.h\"\n")
                    sep_f.write("#define while(...) while((__VA_ARGS__) && !g_cancellation_requested.load(std::memory_order_relaxed))\n\n")
                    sep_f.write("#include \"ComputerCard.h\"\n\n")
                    sep_f.write(f"namespace {card['ns']} {{\n")
                    if card["id"] == "flux":
                        sep_f.write("    extern const int16_t exptable_impl[];\n")
                        sep_f.write("    extern const int16_t logtable_impl[];\n")
                    sep_f.write(src_content)
                    sep_f.write(f"\n}} // namespace {card['ns']}\n")
                
            sources_to_compile.append(os.path.join("src", "cards", sep_filename))
                
        # Store for Makefile.cards generation
        card_data[card["id"]] = {
            "sources": sources_to_compile,
            "flags": card_include_flags
        }
        
        # Registry metadata entry
        registry_entries.append({
            "id": card["id"],
            "name": metadata["name"],
            "num": card["num"],
            "desc": metadata["desc"].replace("\n", "\\n").replace('"', '\\"'),
            "creator": metadata["creator"],
            "visible": is_card_visible(card)
        })
        
    # Write CardRegistry.hpp
    registry_hpp_path = os.path.join(CARDS_SRC_DIR, "CardRegistry.hpp")
    with open(registry_hpp_path, 'w') as f:
        f.write("#pragma once\n")
        f.write("#include <string>\n")
        f.write("#include <vector>\n\n")
        f.write("struct CardMetadata {\n")
        f.write("    std::string id;\n")
        f.write("    std::string name;\n")
        f.write("    std::string number;\n")
        f.write("    std::string description;\n")
        f.write("    std::string creator;\n")
        f.write("    bool visible = true;\n")
        f.write("};\n\n")
        f.write("extern std::vector<CardMetadata> g_card_registry;\n")
        f.write("void register_all_cards();\n")
        
    # Write CardRegistry.cpp
    registry_cpp_path = os.path.join(CARDS_SRC_DIR, "CardRegistry.cpp")
    with open(registry_cpp_path, 'w') as f:
        f.write("#include \"CardRegistry.hpp\"\n\n")
        f.write("std::vector<CardMetadata> g_card_registry;\n\n")
        f.write("void register_all_cards() {\n")
        f.write("    g_card_registry.clear();\n")
        for card in registry_entries:
            vis_str = "true" if card.get("visible", True) else "false"
            def escape_cpp_string(s):
                return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
            f.write(f"    g_card_registry.push_back({{\n")
            f.write(f"        \"{escape_cpp_string(card['id'])}\",\n")
            f.write(f"        \"{escape_cpp_string(card['name'])}\",\n")
            f.write(f"        \"{escape_cpp_string(card['num'])}\",\n")
            f.write(f"        \"{escape_cpp_string(card['desc'])}\",\n")
            f.write(f"        \"{escape_cpp_string(card['creator'])}\",\n")
            f.write(f"        {vis_str}\n")
            f.write("    });\n")
        f.write("}\n")
        
    # Write Makefile.cards configuration file
    makefile_cards_path = os.path.join(VCV_PROJECT_DIR, "Makefile.cards")
    with open(makefile_cards_path, 'w') as f:
        f.write("# Auto-generated makefile configuration for card sources\n\n")
        f.write("# Card libraries target definitions\n")
        f.write("ifdef ARCH_WIN\n")
        f.write("\tCARD_LIB_EXT := dll\n")
        f.write("\tCARD_LDFLAGS_SHARED := -shared -static-libgcc -static-libstdc++\n")
        f.write("else ifdef ARCH_MAC\n")
        f.write("\tCARD_LIB_EXT := dylib\n")
        f.write("\tCARD_LDFLAGS_SHARED := -dynamiclib -undefined dynamic_lookup\n")
        f.write("else\n")
        f.write("\tCARD_LIB_EXT := so\n")
        f.write("\tCARD_LDFLAGS_SHARED := -shared -fPIC\n")
        f.write("endif\n\n")
        
        f.write("res/cards:\n")
        f.write("\t@mkdir -p res/cards\n\n")
        
        # Write list of libs
        lib_list = " ".join([f"res/cards/libcard_{cid}.$(CARD_LIB_EXT)" for cid in card_data.keys()])
        f.write(f"CARD_LIBS := {lib_list}\n\n")

        # Compile targets
        for cid, data in card_data.items():
            all_srcs = data["sources"]
            c_srcs = [s for s in all_srcs if s.endswith(".c")]
            cxx_srcs = [s for s in all_srcs if not s.endswith(".c")]
            flags_str = " ".join([f'"{flag}"' for flag in data["flags"]])
            
            if c_srcs:
                # Mixed C/C++ card: compile .c files as C (gnu99) to avoid GCC C++ designated-initializer errors
                # on the old Ubuntu toolchain used by CI
                c_objs = []
                all_objs = []
                f.write(f"res/cards/libcard_{cid}.$(CARD_LIB_EXT): {' '.join(all_srcs)} | res/cards\n")
                for src in c_srcs:
                    obj = src.replace("/", "_").replace(".", "_") + f"_{cid}.o"
                    obj_path = f"res/cards/{obj}"
                    f.write(f"\t$(CC) -std=gnu99 -fPIC $(FLAGS) {flags_str} -c -o {obj_path} {src}\n")
                    c_objs.append(obj_path)
                    all_objs.append(obj_path)
                for src in cxx_srcs:
                    obj = src.replace("/", "_").replace(".", "_") + f"_{cid}.o"
                    obj_path = f"res/cards/{obj}"
                    f.write(f"\t$(CXX) $(CXXFLAGS) $(FLAGS) -DVCV_PORT=1 {flags_str} -c -o {obj_path} {src}\n")
                    all_objs.append(obj_path)
                objs_str = " ".join(all_objs)
                f.write(f"\t$(CXX) $(CXXFLAGS) $(FLAGS) -DVCV_PORT=1 {flags_str} $(CARD_LDFLAGS_SHARED) -o $@ {objs_str}\n")
                f.write(f"\t@rm -f {objs_str}\n\n")
            else:
                # Pure C++ card: compile all at once with CXX
                srcs = " ".join(all_srcs)
                f.write(f"res/cards/libcard_{cid}.$(CARD_LIB_EXT): {srcs} | res/cards\n")
                f.write(f"\t$(CXX) $(CXXFLAGS) $(FLAGS) {flags_str} $(CARD_LDFLAGS_SHARED) -o $@ {srcs}\n\n")
            
        f.write("SOURCES += src/cards/CardRegistry.cpp\n")
        # Copy wav files for compulidean
        comp_src_dir = os.path.join("/Users/vmaurer/Music/WorkshopComputerExternal/compulidian/include/audio/808samples")
        comp_dst_dir = os.path.join(VCV_PROJECT_DIR, "res", "compulidean")
        if os.path.exists(comp_src_dir):
            os.makedirs(comp_dst_dir, exist_ok=True)
            for f_name in os.listdir(comp_src_dir):
                if f_name.upper().endswith(".WAV"):
                    shutil.copy(os.path.join(comp_src_dir, f_name), os.path.join(comp_dst_dir, f_name))
            print("Copied sample files for compulidean")

        # Copy wav files for cirpy_wavetable
        wav_src_dir = os.path.join(WORKSPACE_DIR, "releases/30_cirpy_wavetable/wav")
        wav_dst_dir = os.path.join(VCV_PROJECT_DIR, "res", "wav")
        if os.path.exists(wav_src_dir):
            os.makedirs(wav_dst_dir, exist_ok=True)
            for f_name in os.listdir(wav_src_dir):
                if f_name.upper().endswith(".WAV"):
                    shutil.copy(os.path.join(wav_src_dir, f_name), os.path.join(wav_dst_dir, f_name))
            print("Copied wavetable files for cirpy_wavetable")
            
        # Copy wav files for backyard_rain
        rain_src_dir = os.path.join("/Users/vmaurer/Music/WorkshopComputerExternal/mtmws_cards/backyard_rain/data")
        rain_dst_dir = os.path.join(VCV_PROJECT_DIR, "res", "backyard_rain")
        if os.path.exists(rain_src_dir):
            os.makedirs(rain_dst_dir, exist_ok=True)
            for f_name in os.listdir(rain_src_dir):
                if f_name.upper().endswith(".WAV"):
                    shutil.copy(os.path.join(rain_src_dir, f_name), os.path.join(rain_dst_dir, f_name))
            # Also copy the stereo folder files
            stereo_src_dir = os.path.join(rain_src_dir, "stereo")
            if os.path.exists(stereo_src_dir):
                stereo_dst_dir = os.path.join(rain_dst_dir, "stereo")
                os.makedirs(stereo_dst_dir, exist_ok=True)
                for f_name in os.listdir(stereo_src_dir):
                    if f_name.upper().endswith(".WAV"):
                        shutil.copy(os.path.join(stereo_src_dir, f_name), os.path.join(stereo_dst_dir, f_name))
            print("Copied wavetable and sample files for backyard_rain")
            
        # Sanitize all generated clock.h headers with extern "C" block
        for root, dirs, files in os.walk(os.path.join(VCV_PROJECT_DIR, "src", "cards", "wrappers")):
            for file in files:
                if file == "clock.h":
                    fp = os.path.join(root, file)
                    with open(fp, "r") as hf:
                        hc = hf.read()
                    hc = hc.replace('extern "C" const unsigned char crow_lua_clock_data[', 'const unsigned char crow_lua_clock_data[')
                    hc = hc.replace('extern "C" const unsigned int crow_lua_clock_data_len', 'const unsigned int crow_lua_clock_data_len')
                    if '#ifdef __cplusplus' not in hc:
                        hc = "#ifdef __cplusplus\nextern \"C\" {\n#endif\n" + hc + "\n#ifdef __cplusplus\n}\n#endif\n"
        # Regenerate CARDS.md from cards_registry.json
        try:
            from generate_cards_markdown import generate_markdown
            generate_markdown()
        except Exception as e:
            print("Notice: Could not auto-generate CARDS.md:", e)

        print("Done! Ported all whitelisted cards successfully.")

if __name__ == "__main__":
    main()
