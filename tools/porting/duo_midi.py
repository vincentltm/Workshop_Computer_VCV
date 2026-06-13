import os
import sys
import shutil
import tempfile
from porting.blackbird import post_process as blackbird_post_process, get_extra_compiler_flags

def get_extra_include_dirs(card_dir_abs):
    vcv_project_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    workspace_dir = os.path.dirname(card_dir_abs)
    blackbird_dir = os.path.join(workspace_dir, "41_blackbird")
    duo_midi_dir = os.path.join(workspace_dir, "98_duo_midi")
    
    wrapper_dir = os.path.join(vcv_project_dir, "src", "cards", "wrappers", "duo_midi")
    
    convert_lua_files(blackbird_dir, duo_midi_dir, wrapper_dir)
    
    return [
        os.path.join(blackbird_dir, "lua", "src"),
        os.path.join(blackbird_dir, "lib"),
        wrapper_dir
    ]

def convert_lua_files(blackbird_dir, duo_midi_dir, wrapper_dir):
    build_dir = os.path.join(wrapper_dir, "build")
    os.makedirs(build_dir, exist_ok=True)
    
    lua_files = [
        "lib/lib-lua/asl.lua",
        "lib/lib-lua/asllib.lua",
        "lib/lib-lua/crowlib.lua",
        "lib/lib-lua/clock.lua",
        "lib/lib-lua/metro.lua",
        "lib/lib-lua/public.lua",
        "lib/lib-lua/input.lua",
        "lib/lib-lua/output.lua",
        "lib/lib-lua/ii.lua",
        "lib/lib-lua/calibrate.lua",
        "lib/lib-lua/sequins.lua",
        "lib/lib-lua/quote.lua",
        "lib/lib-lua/timeline.lua",
        "lib/lib-lua/hotswap.lua"
    ]
    
    lua2header_path = os.path.join(blackbird_dir, "util", "lua2header.py")
    
    for rel_path in lua_files:
        in_file = os.path.join(blackbird_dir, rel_path)
        filename_we = os.path.splitext(os.path.basename(rel_path))[0]
        out_file = os.path.join(build_dir, f"{filename_we}.h")
        
        import subprocess
        cmd = [sys.executable, lua2header_path, in_file, out_file]
        subprocess.run(cmd, check=True, capture_output=True)
        
    duo_midi_lua = os.path.join(duo_midi_dir, "duo_midi.lua")
    temp_dir = tempfile.mkdtemp()
    temp_first_lua = os.path.join(temp_dir, "First.lua")
    shutil.copy2(duo_midi_lua, temp_first_lua)
    
    out_file = os.path.join(build_dir, "First.h")
    cmd = [sys.executable, lua2header_path, temp_first_lua, out_file]
    try:
        subprocess.run(cmd, check=True, capture_output=True)
    finally:
        shutil.rmtree(temp_dir)

def post_process(content, src_rel):
    return blackbird_post_process(content, src_rel)
