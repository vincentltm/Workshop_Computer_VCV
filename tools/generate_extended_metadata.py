import os
import sys
import re
import subprocess

# Add current tools directory to path to import port_all_cards
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from port_all_cards import CARD_WHITELIST, WORKSPACE_DIR, VCV_PROJECT_DIR

# Get all info.yaml files on main branch
git_files = subprocess.run(["git", "ls-tree", "-r", "--name-only", "main"], capture_output=True, text=True, cwd=WORKSPACE_DIR).stdout.splitlines()
main_info_yamls = [f for f in git_files if f.endswith("info.yaml")]

def clean_name(name):
    c = name.lower().replace("_", "")
    return re.sub(r'^\d+', '', c)

def find_info_yaml_content(card_id, card_dir):
    clean_id = clean_name(card_id)

    # 1. Exact match filesystem (direct paths)
    info_path = os.path.join(card_dir, "info.yaml")
    if os.path.exists(info_path):
        with open(info_path, 'r', encoding='utf-8') as f:
            return f.read()
            
    parent_info = os.path.join(os.path.dirname(card_dir), "info.yaml")
    if os.path.exists(parent_info):
        with open(parent_info, 'r', encoding='utf-8') as f:
            return f.read()

    # 2. Exact match local filesystem search in releases/
    releases_dir = os.path.join(WORKSPACE_DIR, "releases")
    if os.path.exists(releases_dir):
        for root, dirs, files in os.walk(releases_dir):
            if "info.yaml" in files:
                folder_name = os.path.basename(root)
                if clean_id == clean_name(folder_name):
                    with open(os.path.join(root, "info.yaml"), 'r', encoding='utf-8') as f:
                        return f.read()

    # 3. Exact match main branch git paths
    for git_path in main_info_yamls:
        folder_name = git_path.split('/')[-2]
        if clean_id == clean_name(folder_name):
            res = subprocess.run(["git", f"show", f"main:{git_path}"], capture_output=True, text=True, cwd=WORKSPACE_DIR)
            if res.returncode == 0:
                return res.stdout

    # 4. Substring match local filesystem search in releases/
    if os.path.exists(releases_dir):
        for root, dirs, files in os.walk(releases_dir):
            if "info.yaml" in files:
                folder_name = os.path.basename(root)
                clean_folder = clean_name(folder_name)
                if clean_id in clean_folder or clean_folder in clean_id:
                    with open(os.path.join(root, "info.yaml"), 'r', encoding='utf-8') as f:
                        return f.read()

    # 5. Substring match main branch git paths
    for git_path in main_info_yamls:
        folder_name = git_path.split('/')[-2]
        clean_folder = clean_name(folder_name)
        if clean_id in clean_folder or clean_folder in clean_id:
            res = subprocess.run(["git", f"show", f"main:{git_path}"], capture_output=True, text=True, cwd=WORKSPACE_DIR)
            if res.returncode == 0:
                return res.stdout

    return None

def parse_inline_dict(s):
    s = s.strip().strip('{}')
    d = {}
    for part in s.split('Edge,' if 'Edge,' in s else ','):
        if ':' in part:
            k, v = part.split(':', 1)
            d[k.strip().lower()] = v.strip().strip('"\'')
    return d

def parse_info_yaml(content):
    if not content:
        return None

    lines = content.splitlines()
    result = {
        "name": "",
        "description": "",
        "creator": "",
        "editor": "",
        "inputs": [],
        "outputs": [],
        "knobs": [],
        "switches": {}
    }

    current_section = None
    current_subsection = None
    current_item = None
    current_knob_control = None
    current_switch_pos = None

    for line in lines:
        line_rstrip = line.rstrip()
        stripped = line_rstrip.strip()
        
        if not stripped or stripped.startswith('#'):
            continue

        indent = len(line_rstrip) - len(line_rstrip.lstrip())

        # Check sections at indent 0
        if indent == 0:
            if ':' in stripped:
                k, v = stripped.split(':', 1)
                k = k.strip().lower()
                v = v.strip().strip('"\'')
                if k == "name":
                    result["name"] = v
                elif k == "description":
                    result["description"] = v
                elif k == "creator":
                    result["creator"] = v
                elif k == "editor":
                    result["editor"] = v
                elif k in ["panel", "controls", "host"]:
                    current_section = k
                    current_subsection = None
            continue

        # Subsections at indent 2
        if indent == 2:
            if ':' in stripped:
                k, v = stripped.split(':', 1)
                k = k.strip().lower()
                if current_section == "panel" and k in ["inputs", "outputs"]:
                    current_subsection = k
                elif current_section == "controls" and k in ["knobs", "switches"]:
                    current_subsection = k
                else:
                    current_subsection = None
            continue

        # List items or keys at indent 4
        if indent == 4:
            if stripped.startswith('-'):
                item_content = stripped[1:].strip()
                if current_subsection in ["inputs", "outputs"]:
                    current_item = {}
                    result[current_subsection].append(current_item)
                    if ':' in item_content:
                        k, v = item_content.split(':', 1)
                        current_item[k.strip().lower()] = v.strip().strip('"\'')
                elif current_subsection == "knobs":
                    current_item = {"when": {}, "main": {}, "x": {}, "y": {}}
                    result["knobs"].append(current_item)
                    if item_content.startswith("when:"):
                        when_val = item_content.split(':', 1)[1].strip()
                        current_item["when"] = parse_inline_dict(when_val)
                elif current_subsection == "switches":
                    current_item = {"id": "", "up": {}, "middle": {}, "down": {}}
                    if ':' in item_content:
                        k, v = item_content.split(':', 1)
                        if k.strip().lower() == "id":
                            current_item["id"] = v.strip().strip('"\'')
                            result["switches"] = current_item
            continue

        # Nested keys inside list items at indent 6 or 8
        if indent >= 6:
            if ':' in stripped:
                k, v = stripped.split(':', 1)
                k = k.strip().lower()
                v = v.strip().strip('"\'')
                
                if current_subsection in ["inputs", "outputs"] and current_item is not None:
                    current_item[k] = v
                elif current_subsection == "knobs" and current_item is not None:
                    if indent == 6:
                        current_knob_control = k
                    elif indent >= 8 and current_knob_control in ["main", "x", "y"]:
                        current_item[current_knob_control][k] = v
                elif current_subsection == "switches" and current_item is not None:
                    if indent == 6:
                        current_switch_pos = k
                    elif indent >= 8 and current_switch_pos in ["up", "middle", "down"]:
                        current_item[current_switch_pos][k] = v
            continue

    return result

def escape_cpp(s):
    if not s:
        return '""'
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'

# Mappings for Port IDs
in_map = {
    "audioin1": 0, "audioinput1": 0, "audioinleft": 0, "audioinputleft": 0, "audio1": 0, "audioin": 0,
    "audioin2": 1, "audioinput2": 1, "audioinright": 1, "audioinputright": 1, "audio2": 1,
    "cvin1": 2, "cvinput1": 2, "cv1": 2, "cvmodx": 2,
    "cvin2": 3, "cvinput2": 3, "cv2": 3, "cvmody": 3,
    "pulsein1": 4, "pulseinput1": 4, "trigin1": 4, "triginput1": 4, "gatein1": 4, "gateinput1": 4, "pulse1": 4,
    "pulsein2": 5, "pulseinput2": 5, "trigin2": 5, "triginput2": 5, "gatein2": 5, "gateinput2": 5, "pulse2": 5
}
out_map = {
    "audioout1": 0, "audiooutput1": 0, "audiooutleft": 0, "audiooutputleft": 0, "audio1": 0, "audioout": 0,
    "audioout2": 1, "audiooutput2": 1, "audiooutright": 1, "audiooutputright": 1, "audio2": 1,
    "cvout1": 2, "cvoutput1": 2, "cv1": 2, "cvout": 2,
    "cvout2": 3, "cvoutput2": 3, "cv2": 3,
    "pulseout1": 4, "pulseoutput1": 4, "gateout1": 4, "gateoutput1": 4, "pulse1": 4,
    "pulseout2": 5, "pulseoutput2": 5, "gateout2": 5, "gateoutput2": 5, "pulse2": 5
}

DEFAULT_INPUTS = [
    {"name": "Audio 1 Input", "description": "Audio input 1"},
    {"name": "Audio 2 Input", "description": "Audio input 2"},
    {"name": "CV 1 Input", "description": "CV input 1"},
    {"name": "CV 2 Input", "description": "CV input 2"},
    {"name": "Pulse 1 Input", "description": "Pulse input 1"},
    {"name": "Pulse 2 Input", "description": "Pulse input 2"}
]
DEFAULT_OUTPUTS = [
    {"name": "Audio 1 Output", "description": "Audio output 1"},
    {"name": "Audio 2 Output", "description": "Audio output 2"},
    {"name": "CV 1 Output", "description": "CV output 1"},
    {"name": "CV 2 Output", "description": "CV output 2"},
    {"name": "Pulse 1 Output", "description": "Pulse output 1"},
    {"name": "Pulse 2 Output", "description": "Pulse output 2"}
]

# Start generating code
header = """// ExtendedMetadata.hpp
// This file is dynamically generated. Do not edit manually.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace ExtendedMetadata {

struct PortMeta {
    std::string name;
    std::string description;
};

struct KnobContext {
    std::string z;        // "up", "middle", "down", "any"
    std::string gesture;  // e.g. "hold", "double-hold", or ""
    std::string mode;     // e.g. "bank-indicator" or ""
    std::string name;
    std::string description;
};

struct SwitchContext {
    std::string name;
    std::string description;
};

struct SwitchMeta {
    std::string id;
    SwitchContext up;
    SwitchContext middle;
    SwitchContext down;
};

struct CardMeta {
    std::string id;
    std::string name;
    std::string description;
    std::string creator;
    std::string editor;

    PortMeta inputs[6];
    PortMeta outputs[6];

    std::vector<KnobContext> knobs[3]; // 0=Main, 1=X, 2=Y
    SwitchMeta z_switch;
    bool has_switch_metadata = false;
};

inline const CardMeta* get_card_metadata(const std::string& card_id) {
    static const std::unordered_map<std::string, CardMeta> metadata_map = {
"""

for card in CARD_WHITELIST:
    card_id = card["id"]
    info_dir = card.get("info_dir", card["dir"])
    if info_dir.startswith("/"):
        info_dir_abs = info_dir
    else:
        info_dir_abs = os.path.join(WORKSPACE_DIR, info_dir)
        
    yaml_content = find_info_yaml_content(card_id, info_dir_abs)
    data = parse_info_yaml(yaml_content)
    
    if not data:
        data = {
            "name": card_id.replace("_", " ").title(),
            "description": "Workshop Computer Card",
            "creator": "Music Thing Modular",
            "editor": "",
            "inputs": [],
            "outputs": [],
            "knobs": [],
            "switches": {}
        }
    
    # Process Inputs
    inputs_meta = [{"name": d["name"], "description": d["description"]} for d in DEFAULT_INPUTS]
    for inp in data.get("inputs", []):
        iid = inp.get("id", "").lower().replace("_", "").replace(" ", "")
        idx = in_map.get(iid, -1)
        if idx != -1:
            inputs_meta[idx] = {
                "name": inp.get("name", DEFAULT_INPUTS[idx]["name"]),
                "description": inp.get("description", DEFAULT_INPUTS[idx]["description"])
            }
            
    # Process Outputs
    outputs_meta = [{"name": d["name"], "description": d["description"]} for d in DEFAULT_OUTPUTS]
    for outp in data.get("outputs", []):
        oid = outp.get("id", "").lower().replace("_", "").replace(" ", "")
        idx = out_map.get(oid, -1)
        if idx != -1:
            outputs_meta[idx] = {
                "name": outp.get("name", DEFAULT_OUTPUTS[idx]["name"]),
                "description": outp.get("description", DEFAULT_OUTPUTS[idx]["description"])
            }

    # Generate Switch Z Meta
    sw = data.get("switches", {})
    has_sw = "id" in sw and sw["id"].upper() == "Z"
    sw_up_name = sw.get("up", {}).get("name", "Up")
    sw_up_desc = sw.get("up", {}).get("description", "Switch position Up")
    sw_mid_name = sw.get("middle", {}).get("name", "Middle")
    sw_mid_desc = sw.get("middle", {}).get("description", "Switch position Middle")
    sw_down_name = sw.get("down", {}).get("name", "Down")
    sw_down_desc = sw.get("down", {}).get("description", "Switch position Down")

    header += f"        {{\n            {escape_cpp(card_id)},\n            {{\n"
    header += f"                {escape_cpp(card_id)},\n"
    header += f"                {escape_cpp(data.get('name') or card_id.replace('_', ' ').title())},\n"
    header += f"                {escape_cpp(data.get('description') or 'Workshop Computer Card')},\n"
    header += f"                {escape_cpp(data.get('creator') or 'Music Thing Modular')},\n"
    header += f"                {escape_cpp(data.get('editor') or '')},\n"
    
    # Write inputs array
    header += "                {\n"
    for i in range(6):
        header += f"                    {{ {escape_cpp(inputs_meta[i]['name'])}, {escape_cpp(inputs_meta[i]['description'])} }},\n"
    header += "                },\n"

    # Write outputs array
    header += "                {\n"
    for i in range(6):
        header += f"                    {{ {escape_cpp(outputs_meta[i]['name'])}, {escape_cpp(outputs_meta[i]['description'])} }},\n"
    header += "                },\n"

    # Write Knobs contexts (Main=0, X=1, Y=2)
    knobs_contexts = [[], [], []]
    for kb in data.get("knobs", []):
        when = kb.get("when", {})
        z = when.get("z", "any")
        gesture = when.get("gesture", "")
        mode = when.get("mode", "")
        
        for idx, key in enumerate(["main", "x", "y"]):
            ctrl = kb.get(key, {})
            name = ctrl.get("name", "")
            desc = ctrl.get("description", "")
            if name or desc:
                knobs_contexts[idx].append({
                    "z": z, "gesture": gesture, "mode": mode,
                    "name": name, "description": desc
                })

    header += "                {\n"
    for idx in range(3):
        header += "                    {\n"
        for ctx in knobs_contexts[idx]:
            header += f"                        {{ {escape_cpp(ctx['z'])}, {escape_cpp(ctx['gesture'])}, {escape_cpp(ctx['mode'])}, {escape_cpp(ctx['name'])}, {escape_cpp(ctx['description'])} }},\n"
        header += "                    },\n"
    header += "                },\n"

    # Write SwitchMeta
    header += f"                {{\n"
    header += f"                    {escape_cpp(sw.get('id', 'Z'))},\n"
    header += f"                    {{ {escape_cpp(sw_up_name)}, {escape_cpp(sw_up_desc)} }},\n"
    header += f"                    {{ {escape_cpp(sw_mid_name)}, {escape_cpp(sw_mid_desc)} }},\n"
    header += f"                    {{ {escape_cpp(sw_down_name)}, {escape_cpp(sw_down_desc)} }}\n"
    header += f"                }},\n"
    header += f"                { 'true' if has_sw else 'false' }\n"
    header += f"            }}\n        }},\n"

header += """    };

    auto it = metadata_map.find(card_id);
    if (it != metadata_map.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace ExtendedMetadata
"""

output_path = os.path.join(VCV_PROJECT_DIR, "src", "shared", "ExtendedMetadata.hpp")
with open(output_path, 'w', encoding='utf-8') as f:
    f.write(header)

print(f"Generated extended metadata file at {output_path}")
