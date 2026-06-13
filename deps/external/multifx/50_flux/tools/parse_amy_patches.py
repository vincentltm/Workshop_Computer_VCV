import re
import sys

# Paths
AMY_PATCHES_PATH = "/Users/vmaurer/Music/Git/amy-main/src/patches.h"
OUTPUT_PATH = "src/synths/fm_presets.h"

def parse_wire_string(wire_str):
    """
    Parses an AMY wire string into a dictionary of voice events.
    Returns: { voice_id: { 'param': value, ... } }
    """
    events = {}
    current_voice = -1 # Context
    
    # Split by 'Z' (End of message/Event separator)
    segments = wire_str.split('Z')
    
    for seg in segments:
        if not seg: continue
        
        # Simple parser for single-char commands
        # v1 a... A...
        # We need to scan char by char
        i = 0
        
        # Temp store for this segment (which applies to a specific voice)
        # Note: 'v' sets the voice for the current segment parameters.
        segment_params = {}
        
        while i < len(seg):
            cmd = seg[i]
            i += 1
            
            # Find next alpha char or end
            j = i
            while j < len(seg) and not seg[j].isalpha() and seg[j] not in [',', '.']: # Wait, separator?
                # Actually, args are numbers, commas, dots, minuses
               if seg[j].isalpha() and seg[j] not in ['e', 'E']: # scientific notation check? AMY doesn't support sci notation in atoff
                   break
               j += 1
            
            # Arg is seg[i:j]. BUT AMY args can be lists: "1,2,3".
            # If next char is alpha, it's a new cmd.
            # But "e" might comprise a float? `parse.c` atoff says no 'e'.
            # So simple isalpha check is fine.
            
            # Wait, `_next_alpha` logic in parse.c scans until it hits an alpha.
            # "0.000188" -> alpha 'f' is next?
            # Yes.
            
            # EXCEPT! Some params take string-like things? No, mostly lists of numbers.
            # "A...,...,..."
            
            # Scan forward until we hit a command char.
            # Problem: "1,2,3" contains no alphas.
            # "1,2,3f" -> 'f' is next cmd.
            
            arg_str = ""
            while i < len(seg):
                char = seg[i]
                if char.isalpha():
                    # Check if it's really a command or part of something else?
                    # AMY wire protocol is strictly [CMD][ARG]...
                    break
                arg_str += char
                i += 1
                
            val = arg_str
            
            if cmd == 'v':
                current_voice = int(val)
                # Ensure voice entry exists
                if current_voice not in events:
                    events[current_voice] = {}
            else:
                # Store param in current_voice context (if set, otherwise use -1 or ignore?)
                # Usually 'v' comes first in valid wire strings for patches.
                # If current_voice is not set, we might be in global or previous context?
                # Patches usually start with vN.
                if current_voice >= 0:
                    events[current_voice][cmd] = val
                    
    return events

def parse_patches():
    with open(AMY_PATCHES_PATH, "r") as f:
        content = f.read()

    # Regex to find patch entries: /* ID: Name */ "CMDSTRING"
    # Note: Name is in comment `/* ... : NAME */`
    # Regex: /\* \d+: (.*?) \*/ "(.*?)"
    patch_regex = re.compile(r'/\*\s*\d+:\s*(.*?)\s*\*/\s*"(.*?)"')
    matches = patch_regex.findall(content)

    presets = []

    for name, wire_str in matches:
        if "DX7" not in name and "dx7" not in name.lower() and "ALGO" not in wire_str and "w8" not in wire_str:
            continue
            
        print(f"Parsing: {name}")
        
        # Parse the wire string
        voice_events = parse_wire_string(wire_str)
        
        # 1. Find the ALGO setup voice (contains 'o' or 'w8')
        algo_voice = None
        algo_id = 0
        feedback = 0.0
        algo_sources = []
        
        for v_id, params in voice_events.items():
            if 'w' in params and params['w'] == '8':
                algo_voice = v_id
            if 'o' in params: # Algorithm ID implies this is the algo voice
                algo_voice = v_id
                
        if algo_voice is None:
            # Maybe implicit? Look for 'O' (Sources)
            for v_id, params in voice_events.items():
                if 'O' in params:
                    algo_voice = v_id
                    break
        
        if algo_voice is None:
            print(f"  Skipping {name}: No algo voice found")
            continue
            
        # Get Algo Params
        params = voice_events[algo_voice]
        if 'o' in params:
            algo_id = int(params['o'])
        if 'b' in params:
            feedback = float(params['b'])
        if 'O' in params:
            algo_sources = [int(x) for x in params['O'].split(',')]
            
        # Sanity check
        if len(algo_sources) != 6:
            # Some patches might not use all ops? Or parse error.
            print(f"  Warning: {name} has {len(algo_sources)} sources: {algo_sources}. Expecting 6.")
            # If < 6, pad? If > 6, truncate?
            # DX7 always has 6 ops.
            # If not defined, we can't build a 6-op preset.
            if len(algo_sources) == 0:
                continue
        
        # Construct Operators
        ops_data = [] # 6 operators
        
        for i in range(6):
            if i < len(algo_sources):
                src_voice_id = algo_sources[i]
            else:
                src_voice_id = -1
                
            op = {
                "ratio": 1.0,
                "amp": 0.0, # Default to silent if not found
                "env_count": 0,
                "env_times": [0]*8, # Fixed point generation expects int array
                "env_levels": [0.0]*8
            }
            
            if src_voice_id in voice_events:
                v_params = voice_events[src_voice_id]
                
                # Ratio 'I'
                if 'I' in v_params:
                    op["ratio"] = float(v_params['I'])
                
                # Amp 'a' (Coefs: Const, Vel, Note, Mod...)
                # We usually want the Constant level.
                if 'a' in v_params:
                    # 'a' can be "0.2" or "0.2,0,0..."
                    # We take the first value.
                    vals = v_params['a'].split(',')
                    try:
                        op["amp"] = float(vals[0])
                    except:
                        pass
                
                # Envelope 'A' (BP0)
                # Format: "time,level,time,level..."
                if 'A' in v_params:
                    bp_str = v_params['A']
                    vals = [float(x) for x in bp_str.split(',')]
                    # vals[0], vals[1] are first point?
                    # parse.c: (time, value), (time, value)...
                    # time is ms.
                    
                    count = len(vals) // 2
                    op["env_count"] = count
                    for k in range(min(count, 8)):
                        t = int(vals[k*2])
                        l = vals[k*2 + 1]
                        op["env_times"][k] = t
                        op["env_levels"][k] = l
            
            ops_data.append(op)
            
        presets.append({
            "name": name.strip(),
            "algorithm": algo_id,
            "feedback": feedback,
            "ops": ops_data
        })
        
        if len(presets) >= 32:
            break

    # Generate Header
    with open(OUTPUT_PATH, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n")
        f.write("#include <math.h>\n\n")
        
        f.write("struct FmOpParams {\n")
        f.write("    uint16_t ratio; // Q8 (256 = 1.0)\n")
        f.write("    uint16_t amp;   // Q12 (4096 = 1.0)\n")
        f.write("    uint8_t envCount;\n")
        f.write("    uint32_t envTimes[8];\n")
        f.write("    int16_t envLevels[8]; // Q15 (32767 = 1.0)\n")
        f.write("};\n\n")
        
        f.write("struct FmPreset {\n")
        f.write("    const char* name;\n")
        f.write("    uint8_t algorithm;\n")
        f.write("    int16_t feedback; // Q15 (32767 = 1.0)\n")
        f.write("    FmOpParams ops[6];\n")
        f.write("};\n\n")
        
        f.write("static const FmPreset fm_presets[32] = {\n")
        
        for patch in presets:
            fb_int = int(patch["feedback"] * 32767.0)
            if fb_int > 32767: fb_int = 32767
            if fb_int < -32767: fb_int = -32767
            
            # The algorithm code in synth_fm uses 0-31?
            # AMY algo IDs are 1-32 usually?
            # Check AMY algorithms.c. `algorithms[33]`. Index 0 is empty?
            # DX7 algos are 1-32.
            # My synth_fm uses `algorithms[preset->algorithm]`.
            # If patch has ID 1, it matches standard.
            # But I should verify.
            # Assuming 1-based from wire string.
            
            f.write(f"    {{ \"{patch['name']}\", {patch['algorithm']}, {fb_int}, {{\n")
            
            for i, op in enumerate(patch["ops"]):
                # Ratio Q8
                r = int(op["ratio"] * 256.0)
                if r > 65535: r = 65535
                
                # Amp Q12
                # Note: 'a' in wire string is linear amplitude.
                # If amp is > 1.0 (sometimes happens in AMY for gain), clamp or allow?
                # Max 15.99 in Q12.
                a = int(op["amp"] * 4096.0)
                if a > 65535: a = 65535
                
                f.write(f"        {{ {r}, {a}, {op['env_count']}, {{")
                
                times = ", ".join(str(x) for x in op["env_times"])
                f.write(f"{times}}}, {{")
                
                levels = []
                for l in op["env_levels"]:
                    l_int = int(l * 32767.0)
                    if l_int > 32767: l_int = 32767
                    levels.append(str(l_int))
                levels_str = ", ".join(levels)
                
                f.write(f"{levels_str}}} }}")
                
                if i < 5: f.write(",\n")
            
            f.write("    } },\n")
            
        f.write("};\n")

if __name__ == "__main__":
    parse_patches()
