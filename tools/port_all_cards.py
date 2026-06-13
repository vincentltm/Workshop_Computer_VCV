#!/usr/bin/env python3
import os
import re
import importlib
import sys

# Source workspace paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
VCV_PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
WORKSPACE_DIR = os.path.join(VCV_PROJECT_DIR, "deps", "Workshop_Computer")
CARDS_SRC_DIR = os.path.join(VCV_PROJECT_DIR, "src", "cards")

# Add tools directory to sys.path so we can import porting modules
sys.path.append(os.path.dirname(__file__))

# Whitelist of cards to port
CARD_WHITELIST = [
    {
        "id": "simple_midi",
        "dir": "releases/00_Simple_MIDI",
        "ns": "Card_SimpleMidi",
        "num": "00",
        "sources": ["simple_midi.cpp"]
    },
    {
        "id": "usb_audio_bridge",
        "dir": "releases/06_usb_audio",
        "ns": "Card_UsbAudioBridge",
        "num": "06",
        "sources": ["Card_usb_audio_bridge.cpp"]
    },
    {
        "id": "crafted_volts",
        "dir": "releases/24_crafted_volts",
        "ns": "Card_CraftedVolts",
        "num": "24",
        "sources": ["crafted_volts.cpp"]
    },
    {
        "id": "eighties_bass",
        "dir": "releases/28_eighties_bass",
        "ns": "Card_EightiesBass",
        "num": "28",
        "sources": ["eighties_bass.cpp"]
    },
    {
        "id": "cirpy_wavetable",
        "dir": "releases/30_cirpy_wavetable",
        "ns": "Card_CirpyWavetable",
        "num": "30",
        "sources": ["cirpy_wavetable.cpp"]
    },
    {
        "id": "od",
        "dir": "releases/38_od",
        "ns": "Card_Od",
        "num": "38",
        "sources": ["od.cpp"]
    },
    {
        "id": "bends",
        "dir": "releases/45_bends",
        "ns": "Card_Bends",
        "num": "45",
        "sources": ["bends.cpp"]
    },
    {
        "id": "backyard_rain",
        "dir": "releases/42_backyard_rain",
        "ns": "Card_BackyardRain",
        "num": "42",
        "sources": ["backyard_rain.cpp"]
    },
    {
        "id": "utility_pair",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "utility_pair_singlecard"),
        "ns": "Card_UtilityPair",
        "num": "25",
        "sources": ["src/main.cpp"]
    },
    {
        "id": "turing_machine",
        "dir": "releases/03_Turing_Machine/Rev_1_5_Code",
        "ns": "Card_TuringMachine",
        "num": "03",
        "sources": ["Clock.cpp", "Config.cpp", "MainApp.cpp", "Turing.cpp", "UI.cpp", "main.cpp"]
    },
    {
        "id": "vink",
        "dir": "releases/32_vink",
        "ns": "Card_Vink",
        "num": "32",
        "sources": ["main.cpp"]
    },
    {
        "id": "siren",
        "dir": "releases/27_Siren",
        "ns": "Card_Siren",
        "num": "27",
        "sources": ["main.cpp"]
    },
    {
        "id": "grains",
        "dir": "releases/51_grains",
        "ns": "Card_Grains",
        "num": "51",
        "sources": ["grains.cpp"]
    },
    {
        "id": "reverb",
        "dir": "releases/20_reverb",
        "ns": "Card_Reverb",
        "num": "20",
        "sources": ["reverb.c", "reverb_dsp.c"]
    },
    {
        "id": "flux",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "multifx", "50_flux"),
        "ns": "Card_Flux",
        "num": "50",
        "sources": [
            "src/main.cpp",
            "src/SynthCore.cpp",
            "src/MathTables.cpp",
            "src/synths/synth_noise.cpp",
            "src/synths/synth_wavetable.cpp",
            "src/synths/synth_vabass.cpp",
            "src/synths/synth_strings.cpp",
            "src/synths/synth_modal.cpp",
            "src/synths/synth_granular.cpp",
            "src/synths/synth_piano.cpp",
            "src/synths/synth_sampler_oneshot.cpp",
            "src/synths/synth_sampler_loop.cpp",
            "src/synths/synth_sampler_player.cpp",
            "src/synths/synth_drums.cpp",
            "src/synths/synth_fm.cpp",
            "src/synths/synth_drumsynth.cpp"
        ]
    },
    {
        "id": "chord_organ",
        "dir": "releases/18_chord_organ",
        "ns": "Card_ChordOrgan",
        "num": "18",
        "sources": ["main.cpp"]
    },
    {
        "id": "resonator",
        "dir": "releases/21_resonator",
        "ns": "Card_Resonator",
        "num": "21",
        "sources": ["main.cpp"]
    },
    {
        "id": "fifths",
        "dir": "releases/55_fifths",
        "ns": "Card_Fifths",
        "num": "55",
        "sources": ["main.cpp"]
    },
    {
        "id": "computer_grids",
        "dir": "releases/82_Computer_Grids",
        "ns": "Card_ComputerGrids",
        "num": "82",
        "sources": ["main.cpp", "GridsCard.cpp", "ConfigStore.cpp", "GridsEngine.cpp", "GridsResources.cpp"]
    },
    {
        "id": "byo_benjolin",
        "dir": "releases/04_BYO_Benjolin",
        "ns": "Card_BYOBenjolin",
        "num": "04",
        "sources": ["main.cpp"]
    },
    {
        "id": "goldfish",
        "dir": "releases/11_goldfish",
        "ns": "Card_Goldfish",
        "num": "11",
        "sources": ["main.cpp"]
    },
    {
        "id": "bumpers",
        "dir": "releases/07_bumpers/src",
        "ns": "Card_Bumpers",
        "num": "07",
        "sources": ["bumpers.cpp"]
    },
    {
        "id": "sheep",
        "dir": "releases/22_sheep",
        "ns": "Card_Sheep",
        "num": "22",
        "sources": ["main.cpp"]
    },
    {
        "id": "knots",
        "dir": "releases/39_knots/src/knots/src",
        "ns": "Card_Knots",
        "num": "39",
        "sources": [
            "main.cpp",
            "core/control_router.cpp",
            "core/midi_worker.cpp",
            "engines/bender_engine.cpp",
            "engines/cumulus_engine.cpp",
            "engines/din_sum_engine.cpp",
            "engines/engine_registry.cpp",
            "engines/floatable_engine.cpp",
            "engines/losenge_engine.cpp",
            "engines/sawsome_engine.cpp"
        ]
    },
    {
        "id": "chord_blimey",
        "dir": "releases/05_chord_blimey/src",
        "ns": "Card_ChordBlimey",
        "num": "05",
        "sources": ["main.cpp", "computer.cpp", "ui.cpp"]
    },
    {
        "id": "noisebox",
        "dir": "releases/13_noisebox",
        "ns": "Card_Noisebox",
        "num": "13",
        "sources": ["main.cpp"]
    },
    {
        "id": "cvmod",
        "dir": "releases/14_cvmod",
        "ns": "Card_CVMod",
        "num": "14",
        "sources": ["cvmod.cpp"]
    },
    {
        "id": "esp",
        "dir": "releases/31_esp",
        "ns": "Card_ESP",
        "num": "31",
        "sources": ["main.cpp"]
    },
    {
        "id": "modes",
        "dir": "releases/49_modes",
        "ns": "Card_Modes",
        "num": "49",
        "sources": ["modes.cpp", "resonator_q15.cpp", "resources_q15.cpp", "samples_flash.cpp", "string_q15.cpp"]
    },
    {
        "id": "rompler",
        "dir": "releases/46_rompler",
        "ns": "Card_Rompler",
        "num": "46",
        "sources": ["rompler.cpp"]
    },
    {
        "id": "trace",
        "dir": "releases/69_trace",
        "ns": "Card_Trace",
        "num": "69",
        "sources": ["src/main.cpp"]
    },
    {
        "id": "talker",
        "dir": "releases/78_Talker",
        "ns": "Card_Talker",
        "num": "78",
        "sources": ["src/main.cpp"]
    },
    {
        "id": "bytebeat",
        "dir": "releases/08_bytebeat/Arduino Code/08_bytebeat",
        "ns": "Card_Bytebeat",
        "num": "08",
        "sources": ["formulas.cpp", "bytebeat_card.cpp", "tinyexpr_bitw.c"]
    },
    {
        "id": "twists",
        "dir": "releases/10_twists/src",
        "ns": "Card_Twists",
        "num": "10",
        "sources": [
            "braids/twists.cc",
            "braids/usb_worker.cc",
            "braids/analog_oscillator.cc",
            "braids/digital_oscillator.cc",
            "braids/macro_oscillator.cc",
            "braids/quantizer.cc",
            "braids/resources.cc",
            "braids/settings.cc",
            "braids/ui.cc",
            "stmlib/utils/random.cc",
            "braids/drivers/dac.cc",
            "braids/drivers/display.cc",
            "braids/drivers/gate_input.cc",
            "braids/drivers/switch.cc"
        ]
    },
    {
        "id": "dual_quant",
        "dir": "releases/34_dual_quant",
        "ns": "Card_DualQuant",
        "num": "34",
        "sources": ["dual_quant_pitch.cpp"]
    },
    {
        "id": "birds",
        "dir": "releases/44_Birds",
        "ns": "Card_Birds",
        "num": "44",
        "sources": ["main.cpp"]
    },
    {
        "id": "tapegrade",
        "dir": "releases/54_Tapegrade",
        "ns": "Card_Tapegrade",
        "num": "54",
        "sources": ["Tapegrade.cpp"]
    },
    {
        "id": "glitch",
        "dir": "releases/57_glitch",
        "ns": "Card_Glitch",
        "num": "57",
        "sources": ["main.cpp"]
    },
    {
        "id": "lochovibes",
        "dir": "releases/58_LoChoVibes",
        "ns": "Card_LoChoVibes",
        "num": "58",
        "sources": ["LoChoVibes.cpp"]
    },
    {
        "id": "markov",
        "dir": "releases/60_markov",
        "ns": "Card_Markov",
        "num": "60",
        "sources": ["main.cpp"]
    },
    {
        "id": "stretchcore",
        "dir": "releases/66_stretchcore",
        "ns": "Card_Stretchcore",
        "num": "66",
        "sources": ["src/main.cpp", "src/BreakyAudioBank.cpp", "src/BreakySampleManager.cpp"]
    },
    {
        "id": "wild_pebble",
        "dir": "releases/74_Wild_Pebble",
        "ns": "Card_WildPebble",
        "num": "74",
        "sources": ["WildPebble.cpp"]
    },
    {
        "id": "divcom",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "divcom"),
        "ns": "Card_DivCom",
        "num": "09",
        "sources": ["divcom.cpp"]
    },
    {
        "id": "am_coupler",
        "dir": "releases/12_am_coupler",
        "ns": "Card_AMCoupler",
        "num": "12",
        "sources": ["main.cpp"]
    },
    {
        "id": "slowmod",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "slowmod"),
        "ns": "Card_SlowMod",
        "num": "23",
        "sources": ["main.cpp"]
    },
    {
        "id": "nzt",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "ws"),
        "ns": "Card_NZT",
        "num": "47",
        "sources": ["nzt/main.cpp"]
    },
    {
        "id": "glitter",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "mtws", "53_glitter"),
        "ns": "Card_Glitter",
        "num": "53",
        "sources": ["main.cpp", "src/Glitter.cpp", "src/Utils.cpp"]
    },
    {
        "id": "degenerator",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "Degenerator"),
        "ns": "Card_Degenerator",
        "num": "71",
        "sources": ["main.cpp"]
    },
    {
        "id": "motorik",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "motorik"),
        "ns": "Card_Motorik",
        "num": "72",
        "sources": ["main.cpp"]
    },
    {
        "id": "tesserae",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "Tesserae"),
        "ns": "Card_Tesserae",
        "num": "86",
        "sources": ["main.cpp"]
    },
    {
        "id": "toolbox",
        "dir": os.path.join(VCV_PROJECT_DIR, "deps", "external", "toolbox"),
        "ns": "Card_Toolbox",
        "num": "99",
        "sources": ["toolbox.cpp"]
    },
    {
        "id": "mlrws",
        "dir": "releases/15_MLRws",
        "ns": "Card_MLRws",
        "num": "15",
        "sources": ["main.cpp", "monome_ws.c", "mlr.c", "device_mode.c"]
    },
    {
        "id": "drumdrum",
        "dir": "releases/33_drumdrum",
        "ns": "Card_DrumDrum",
        "num": "33",
        "sources": ["main.cpp", "usb_core1.cpp", "midi_sysex.cpp", "monome_mext.c", "grid_ui.cpp", "midi_host.cpp"]
    },
    {
        "id": "blackbird",
        "dir": "releases/41_blackbird",
        "ns": "Card_Blackbird",
        "num": "41",
        "sources": [
            "main.cpp",
            "lib/casl.c",
            "lib/slopes.c",
            "lib/ashapes.c",
            "lib/detect.c",
            "lib/caw.c",
            "lib/ii.c",
            "lib/metro.c",
            "lib/clock.c",
            "lib/clock_ll.c",
            "lib/events_lockfree.c",
            "lib/usb_lockfree.c",
            "lib/l_bootstrap.c",
            "lib/l_crowlib.c",
            "lib/l_ii_mod.c",
            "lib/ll_timers.c",
            "lib/random.c",
            "lib/wrblocks.c",
            "lib/mailbox.c",
            "lib/fastmath.c",
            "lib/fastmath_lut.c",
            "lib/flash_storage.cpp",
            "lua/src/lapi.c",
            "lua/src/lauxlib.c",
            "lua/src/lbaselib.c",
            "lua/src/lcode.c",
            "lua/src/lcorolib.c",
            "lua/src/lctype.c",
            "lua/src/ldebug.c",
            "lua/src/ldo.c",
            "lua/src/ldump.c",
            "lua/src/lfunc.c",
            "lua/src/lgc.c",
            "lua/src/linit_crow.c",
            "lua/src/llex.c",
            "lua/src/lmathlib.c",
            "lua/src/lmem.c",
            "lua/src/loadlib.c",
            "lua/src/lobject.c",
            "lua/src/lopcodes.c",
            "lua/src/lparser.c",
            "lua/src/lstate.c",
            "lua/src/lstring.c",
            "lua/src/lstrlib.c",
            "lua/src/ltable.c",
            "lua/src/ltablib.c",
            "lua/src/ltm.c",
            "lua/src/lundump.c",
            "lua/src/lvm.c",
            "lua/src/lzio.c"
        ]
    }
]

# Add krell and duo_midi dynamically using blackbird's sources
blackbird_entry = next(c for c in CARD_WHITELIST if c["id"] == "blackbird")
CARD_WHITELIST.append({
    "id": "krell",
    "dir": "releases/41_blackbird",
    "info_dir": "releases/56_Krell",
    "ns": "Card_Krell",
    "num": "56",
    "sources": list(blackbird_entry["sources"])
})
CARD_WHITELIST.append({
    "id": "duo_midi",
    "dir": "releases/41_blackbird",
    "info_dir": "releases/98_duo_midi",
    "ns": "Card_DuoMidi",
    "num": "98",
    "sources": list(blackbird_entry["sources"])
})





def fix_main_return(content):
    match = re.search(r'\bint\s+main\s*\([^)]*\)', content)
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
    
    # Sort whitelist by card number
    CARD_WHITELIST.sort(key=lambda x: int(x["num"]))
    
    registry_entries = []
    card_data = {}
    
    for card in CARD_WHITELIST:
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
                "creator": "Music Thing Modular"
            })
            continue

        if card["dir"].startswith("/"):
            card_dir_abs = card["dir"]
        else:
            card_dir_abs = os.path.join(WORKSPACE_DIR, card["dir"])
        if not os.path.exists(card_dir_abs):
            print(f"Warning: Card directory {card_dir_abs} does not exist. Skipping.")
            continue
            
        # Copy web folder or root HTML files if they exist
        web_src = None
        curr_dir = card_dir_abs
        while curr_dir and os.path.basename(curr_dir) != "releases" and curr_dir != "/":
            potential_web = os.path.join(curr_dir, "web")
            if os.path.isdir(potential_web):
                web_src = potential_web
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
        if info_dir.startswith("/"):
            info_dir_abs = info_dir
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
                with open(info_path, 'r') as f:
                    for line in f:
                        if ":" in line:
                            key, val = line.split(":", 1)
                            key = key.strip().lower()
                            val = val.strip().strip('"').strip("'")
                            if key == "name":
                                metadata["name"] = val
                            elif key == "description":
                                metadata["desc"] = val
                            elif key == "creator":
                                metadata["creator"] = val
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
            vcv_wrapper_path = os.path.join(VCV_PROJECT_DIR, "src", "cards", "wrappers", card["id"], s)
            if os.path.exists(vcv_wrapper_path):
                return vcv_wrapper_path
            if s.startswith("lua/"):
                lua_path = os.path.join(VCV_PROJECT_DIR, "deps", "external", "blackbird_lua", s)
                if os.path.exists(lua_path):
                    return lua_path
            return os.path.join(card_dir_abs, s)

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
                    
        # Add original card source directories second
        card_include_flags.append(f"-I{card_dir_abs}")
        src_sub = os.path.join(card_dir_abs, "src")
        if os.path.exists(src_sub):
            card_include_flags.append(f"-I{src_sub}")
            for root, dirs, files in os.walk(src_sub):
                for d in dirs:
                    card_include_flags.append(f"-I{os.path.join(root, d)}")
        inc_sub = os.path.join(card_dir_abs, "include")
        if os.path.exists(inc_sub):
            card_include_flags.append(f"-I{inc_sub}")
            for root, dirs, files in os.walk(inc_sub):
                for d in dirs:
                    card_include_flags.append(f"-I{os.path.join(root, d)}")
        inc_sub2 = os.path.join(card_dir_abs, "inc")
        if os.path.exists(inc_sub2):
            card_include_flags.append(f"-I{inc_sub2}")
            for root, dirs, files in os.walk(inc_sub2):
                for d in dirs:
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
            
        lib_dir = os.path.join(card_dir_abs, "lib")
        if os.path.exists(lib_dir):
            for root, dirs, files in os.walk(lib_dir):
                for d in dirs:
                    if d in ("inc", "include"):
                        card_include_flags.append(f"-I{os.path.join(root, d)}")
                        
        # Convert include paths to relative paths
        def to_rel(path):
            if os.path.isabs(path):
                if path.startswith(VCV_PROJECT_DIR):
                    return os.path.relpath(path, VCV_PROJECT_DIR)
            return path
        card_include_flags = [f"-I{to_rel(flag[2:])}" if flag.startswith("-I") else flag for flag in card_include_flags]

        # Track sources for Makefile compilation
        sources_to_compile = [os.path.join("src", "cards", wrapper_filename)]
            
        # Write the wrapped unity file
        with open(wrapper_path, 'w') as out_f:
            out_f.write("// Unified compilation file automatically generated by port_all_cards.py\n")
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
            out_f.write("#include \"pico_mocks.h\"\n")
            out_f.write("#include \"tusb.h\"\n")
            out_f.write("#define while(...) while((__VA_ARGS__) && !g_cancellation_requested.load(std::memory_order_relaxed))\n\n")
            out_f.write("#include \"ComputerCard.h\"\n\n")
            
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
            for src_rel in unity_sources:
                src_path_abs = get_source_path(src_rel)
                
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
                    # Replace hardware dmb assembly blocks with portable memory barrier macros
                    src_content = src_content.replace('__asm volatile("dmb" ::: "memory");', 'asm volatile("" ::: "memory");')
                    src_content = src_content.replace('__asm volatile("dmb");', 'asm volatile("" ::: "memory");')
                    
                    # Fix missing return 0 in main() to avoid UB / EXC_BREAKPOINT under -O3
                    src_content = fix_main_return(src_content)
                    
                    # Delegate card-specific post-processing
                    src_content = run_card_post_process(card["id"], src_content, src_rel)
                    
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
                src_content = src_content.replace('__asm volatile("dmb" ::: "memory");', 'asm volatile("" ::: "memory");')
                src_content = src_content.replace('__asm volatile("dmb");', 'asm volatile("" ::: "memory");')
                
                # Strip duplicate includes
                src_content = re.sub(r'#include\s+<[^>]+>', '/* stripped system include */', src_content)
                src_content = re.sub(r'#include\s+"pico/[^"]+"', '/* stripped pico include */', src_content)
                src_content = re.sub(r'#include\s+"hardware/[^"]+"', '/* stripped hardware include */', src_content)
                src_content = re.sub(r'#include\s+"bsp/[^"]+"', '/* stripped bsp include */', src_content)
                src_content = re.sub(r'#include\s+"usb_midi_host\.h"', '/* stripped usb_midi_host include */', src_content)
                src_content = re.sub(r'#include\s+"ComputerCard.h"', '/* stripped ComputerCard include */', src_content)
                src_content = re.sub(r'#include\s+"ComputerCard/ComputerCard.h"', '/* stripped ComputerCard include */', src_content)
                src_content = re.sub(r'#include\s+"tusb\.h"', '/* stripped tusb include */', src_content)
                src_content = re.sub(r'#include\s+<tusb\.h>', '/* stripped tusb include */', src_content)
                src_content = re.sub(r'#include\s+"tusb_config\.h"', '/* stripped tusb_config include */', src_content)
                
                # Delegate card-specific post-processing
                src_content = run_card_post_process(card["id"], src_content, sep_src)
                
                with open(sep_path, 'w') as sep_f:
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
            "creator": metadata["creator"]
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
            f.write(f"    g_card_registry.push_back({{\n")
            f.write(f"        \"{card['id']}\",\n")
            f.write(f"        \"{card['name']}\",\n")
            f.write(f"        \"{card['num']}\",\n")
            f.write(f"        \"{card['desc']}\",\n")
            f.write(f"        \"{card['creator']}\"\n")
            f.write("    });\n")
        f.write("}\n")
        
    # Write Makefile.cards configuration file
    makefile_cards_path = os.path.join(VCV_PROJECT_DIR, "Makefile.cards")
    with open(makefile_cards_path, 'w') as f:
        f.write("# Auto-generated makefile configuration for card sources\n\n")
        f.write("# Card libraries target definitions\n")
        f.write("ifeq ($(OS), Windows_NT)\n")
        f.write("\tCARD_LIB_EXT := dll\n")
        f.write("\tCARD_LDFLAGS_SHARED := -shared\n")
        f.write("else\n")
        f.write("\tUNAME_S := $(shell uname -s)\n")
        f.write("\tifeq ($(UNAME_S), Darwin)\n")
        f.write("\t\tCARD_LIB_EXT := dylib\n")
        f.write("\t\tCARD_LDFLAGS_SHARED := -dynamiclib -undefined dynamic_lookup\n")
        f.write("\telse\n")
        f.write("\t\tCARD_LIB_EXT := so\n")
        f.write("\t\tCARD_LDFLAGS_SHARED := -shared -fPIC\n")
        f.write("\tendif\n")
        f.write("endif\n\n")
        
        # Write list of libs
        lib_list = " ".join([f"res/cards/libcard_{cid}.$(CARD_LIB_EXT)" for cid in card_data.keys()])
        f.write(f"CARD_LIBS := {lib_list}\n\n")
        
        # Compile targets
        for cid, data in card_data.items():
            srcs = " ".join(data["sources"])
            flags_str = " ".join([f'"{flag}"' for flag in data["flags"]])
            f.write(f"res/cards/libcard_{cid}.$(CARD_LIB_EXT): {srcs}\n")
            f.write(f"\t@mkdir -p res/cards\n")
            f.write(f"\t$(CXX) $(CXXFLAGS) $(FLAGS) {flags_str} $(CARD_LDFLAGS_SHARED) -o $@ {srcs}\n\n")
            
        f.write("SOURCES += src/cards/CardRegistry.cpp\n")
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
            
        print("Done! Ported all whitelisted cards successfully.")

if __name__ == "__main__":
    main()
