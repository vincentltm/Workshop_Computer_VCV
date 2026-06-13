import os
import sys
import re
import subprocess

def get_extra_include_dirs(card_dir_abs):
    # VCV project dir is parent of tools/porting/
    vcv_project_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    wrapper_dir = os.path.join(vcv_project_dir, "src", "cards", "wrappers", "blackbird")
    
    # Run lua2header.py conversion on import / when include dirs are queried
    convert_lua_files(card_dir_abs, wrapper_dir)
    
    return [
        os.path.join(card_dir_abs, "lua", "src"),
        os.path.join(card_dir_abs, "lib"),
        wrapper_dir
    ]

def get_extra_compiler_flags():
    return [
        "-DLUA_32BITS=1",
        "-DLUA_USE_C89=1",
        "-DLUA_COMPAT_5_3=1"
    ]

def convert_lua_files(card_dir_abs, wrapper_dir):
    # Output to wrapper_dir/build/ to isolate Lua headers from C headers (e.g. clock.h)
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
        "lib/lib-lua/hotswap.lua",
        "lib/lib-lua/First.lua"
    ]
    
    lua2header_path = os.path.join(card_dir_abs, "util", "lua2header.py")
    
    for rel_path in lua_files:
        in_file = os.path.join(card_dir_abs, rel_path)
        filename_we = os.path.splitext(os.path.basename(rel_path))[0]
        out_file = os.path.join(build_dir, f"{filename_we}.h")
        
        # Run conversion script using current python interpreter
        cmd = [sys.executable, lua2header_path, in_file, out_file]
        print(f"Running conversion: {' '.join(cmd)}")
        try:
            subprocess.run(cmd, check=True, capture_output=True)
        except subprocess.CalledProcessError as e:
            print(f"Error converting {rel_path} to header: {e.stderr.decode()}", file=sys.stderr)
            raise e

def post_process(content, src_rel):
    if src_rel == "main.cpp":
        # Remove TinyUSB CDC header inclusion (covered by mock/tusb.h)
        content = content.replace('#include "class/cdc/cdc_device.h"', '// #include "class/cdc/cdc_device.h"')
        
        # Redirect Lua compiled headers to build/ subdirectory to avoid conflicts (like clock.h)
        lua_headers = [
            "asl.h", "asllib.h", "output.h", "input.h", "metro.h", "First.h",
            "calibrate.h", "sequins.h", "public.h", "quote.h", "timeline.h", "hotswap.h"
        ]
        for header in lua_headers:
            content = content.replace(f'#include "{header}"', f'#include "build/{header}"')
            
        # Remove extern "C" blocks and keywords to use C++ linkage consistently
        content = content.replace(
            'extern "C" {\n#include "lua/src/lua.h"\n#include "lua/src/lualib.h"\n#include "lua/src/lauxlib.h"\n}',
            '#include "lua/src/lua.h"\n#include "lua/src/lualib.h"\n#include "lua/src/lauxlib.h"'
        )
        content = content.replace('extern "C" {\n#include "lib/ashapes.h"', '#include "lib/ashapes.h"')
        content = content.replace('#include "lib/sample_rate.h"\n}', '#include "lib/sample_rate.h"')
        content = content.replace('extern "C" {\nextern const unsigned char clock[];\nextern const unsigned int clock_len;\n}', 'extern const unsigned char clock[];\nextern const unsigned int clock_len;')
        content = content.replace('extern "C" {\nstatic volatile uint32_t g_hardfault_stack[8];', 'static volatile uint32_t g_hardfault_stack[8];')
        content = content.replace('static volatile uint32_t g_hardfault_icsr = 0;\n}', 'static volatile uint32_t g_hardfault_icsr = 0;')
        content = content.replace('extern "C" {\n    int l_crowlib_crow_reset(lua_State* L);\n}', '    int l_crowlib_crow_reset(lua_State* L);')
        
        # Remove extern "C" wrapper for time-critical hardware set voltage functions
        content = content.replace(
            'extern "C" {\n__attribute__((section(".time_critical.hardware_output_set_voltage")))',
            '__attribute__((section(".time_critical.hardware_output_set_voltage")))'
        )
        content = content.replace(
            '        ((BlackbirdCrow*)g_blackbird_instance)->hardware_set_output_q16(channel, voltage_q16);\n    }\n}\n}',
            '        ((BlackbirdCrow*)g_blackbird_instance)->hardware_set_output_q16(channel, voltage_q16);\n    }\n}'
        )

        # Remove extern "C" wrapper for stdout/puts redirection
        content = content.replace(
            'extern "C" {\n    // Override putchar for printf support',
            '    // Override putchar for printf support'
        )
        content = content.replace(
            '            sleep_ms(10);\n        }\n    }\n}\n\nint main()',
            '            sleep_ms(10);\n        }\n    }\n\nint main()'
        )

        # Remove single-line extern "C" functions
        extern_c_keywords = [
            'extern "C" void hardfault_capture_c',
            'extern "C" __attribute__((naked)) void HardFault_Handler',
            'extern "C" void queue_slope_action_callback',
            'extern "C" float get_input_state_simple',
            'extern "C" float get_audioin_volts',
            'extern "C" void hardware_output_set_voltage',
            'extern "C" void hardware_output_set_voltage_q16',
            'extern "C" void hardware_pulse_output_set',
            'extern "C" void L_handle_input_lockfree',
            'extern "C" void L_handle_metro_lockfree',
            'extern "C" void L_handle_clock_resume_lockfree',
            'extern "C" void output_batch_begin',
            'extern "C" void output_batch_flush',
            'extern "C" int LuaManager_lua_c_tell',
            'extern "C" void L_queue_asl_done',
            'extern "C" void trigger_soutput_handler',
            'extern "C" lua_State* get_lua_state',
            'extern "C" uint64_t get_card_unique_id'
        ]
        for kw in extern_c_keywords:
            content = content.replace(kw, kw.replace('extern "C" ', ''))

        # Stub out RP2040 HardFault capture and naked handlers which contain non-portable ARM assembly
        content = content.replace('g_hardfault_sp = (uint32_t)sp;', 'g_hardfault_sp = (uint32_t)(uintptr_t)sp;')
        content = content.replace('__asm volatile("mov %0, lr" : "=r"(g_hardfault_lr));', '(void)g_hardfault_lr;')
        content = content.replace('__asm volatile("bkpt #0");', '')
        content = content.replace('__asm volatile("nop");', '')
        content = content.replace('__attribute__((naked))', '')
        content = content.replace('__asm volatile(\n        "mov r0, sp\\n"\n        "b hardfault_capture_c\\n"\n    );', '')

        # Add BackgroundLoop override to BlackbirdCrow class to execute the Core 1 background service
        content = content.replace(
            '    BlackbirdCrow()\n    {',
            '    void BackgroundLoop() override {\n        blackbird_core1_background_service();\n    }\n\n    BlackbirdCrow()\n    {'
        )

        # Force USB polling and transmission to run unconditionally in the emulated main loop
        content = content.replace(
            'static inline void usb_service_poll_from_main(void) {\n    // Coalesce ticks to bounded work per loop iteration\n    if (g_usb_service_tick) {\n        g_usb_service_tick = 0;',
            'static inline void usb_service_poll_from_main(void) {\n    // Coalesce ticks to bounded work per loop iteration\n    {\n        g_usb_service_tick = 0;'
        )
        content = content.replace(
            '    if (g_usb_tx_tick) {\n        g_usb_tx_tick = 0;\n        usb_process_tx_bounded();\n    }',
            '''    {
        g_usb_tx_tick = 0;
        usb_process_tx_bounded();
    }
    lua_State* get_lua_state(void);
    lua_State* L = get_lua_state();
    if (L) {
        uint8_t packet[4];
        while (g_midi_rx_packet_queue.pop(packet)) {
            uint8_t status = packet[1];
            uint8_t high_nibble = status & 0xF0;
            lua_getglobal(L, "bb");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "midi");
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, "rx");
                    if (lua_istable(L, -1)) {
                        if (high_nibble == 0x90 && packet[3] > 0) { // Note On
                            lua_getfield(L, -1, "note");
                            if (lua_isfunction(L, -1)) {
                                lua_pushstring(L, "on");
                                lua_pushinteger(L, packet[2]); // Note number
                                lua_pushinteger(L, packet[3]); // Velocity
                                if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
                                    const char* err = lua_tostring(L, -1);
                                    if (err) printf("bb.midi.rx.note error: %s\\n", err);
                                    lua_pop(L, 1);
                                }
                            } else {
                                lua_pop(L, 1);
                            }
                        } else if (high_nibble == 0x80 || (high_nibble == 0x90 && packet[3] == 0)) { // Note Off
                            lua_getfield(L, -1, "note");
                            if (lua_isfunction(L, -1)) {
                                lua_pushstring(L, "off");
                                lua_pushinteger(L, packet[2]); // Note number
                                lua_pushinteger(L, packet[3]); // Velocity
                                if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
                                    const char* err = lua_tostring(L, -1);
                                    if (err) printf("bb.midi.rx.note error: %s\\n", err);
                                    lua_pop(L, 1);
                                }
                            } else {
                                lua_pop(L, 1);
                            }
                        } else if (high_nibble == 0xE0) { // Pitch Bend
                            lua_getfield(L, -1, "bend");
                            if (lua_isfunction(L, -1)) {
                                int raw_bend = (packet[3] << 7) | packet[2];
                                double norm_bend = ((double)raw_bend - 8192.0) / 8192.0;
                                lua_pushnumber(L, norm_bend);
                                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                                    const char* err = lua_tostring(L, -1);
                                    if (err) printf("bb.midi.rx.bend error: %s\\n", err);
                                    lua_pop(L, 1);
                                }
                            } else {
                                lua_pop(L, 1);
                            }
                        } else if (high_nibble == 0xB0) { // CC
                            lua_getfield(L, -1, "cc");
                            if (lua_isfunction(L, -1)) {
                                lua_pushinteger(L, packet[2]); // CC number
                                lua_pushinteger(L, packet[3]); // CC value
                                if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                                    const char* err = lua_tostring(L, -1);
                                    if (err) printf("bb.midi.rx.cc error: %s\\n", err);
                                    lua_pop(L, 1);
                                }
                            } else {
                                lua_pop(L, 1);
                            }
                        }
                    }
                    lua_pop(L, 1); // pop rx
                }
                lua_pop(L, 1); // pop midi
            }
            lua_pop(L, 1); // pop bb
        }
    }'''
        )

        # Add sleep in MainControlLoop to avoid 100% CPU thread starvation in VCV host
        content = content.replace(
            '            g_loop_iteration_count++;',
            '            g_loop_iteration_count++;\n            sleep_us(100);'
        )

    elif src_rel == "lib/l_crowlib.c":
        content = content.replace(
            '    lua_getglobal(L, "bb"); // @1\n    if (lua_isnil(L, -1)) {\n        lua_pop(L, 1);\n        lua_newtable(L);\n        lua_setglobal(L, "bb");\n    } else {\n        lua_pop(L, 1);\n    }',
            '    lua_getglobal(L, "bb"); // @1\n    if (lua_isnil(L, -1)) {\n        lua_pop(L, 1);\n        lua_newtable(L);\n        lua_setglobal(L, "bb");\n    } else {\n        lua_pop(L, 1);\n    }\n    lua_getglobal(L, "bb");\n    lua_newtable(L);\n    lua_newtable(L);\n    lua_setfield(L, -2, "rx");\n    lua_setfield(L, -2, "midi");\n    lua_pop(L, 1);'
        )
            
    elif src_rel == "lib/l_bootstrap.c":
        # Rename 'public' to 'public_lua' because 'public' is a C++ keyword
        content = content.replace('#include "build/public.h"', '#define public public_lua\n#include "build/public.h"\n#undef public')
        content = content.replace('"lua_public"    , public    ,', '"lua_public"    , public_lua,')
        # Give 'clock' external linkage by declaring it 'extern' before definition in build/clock.h
        content = content.replace(
            '#include "build/clock.h"',
            'extern const unsigned char clock[];\nextern const unsigned int clock_len;\n#include "build/clock.h"'
        )
        
    elif src_rel == "lib/casl.c":
        # C++ explicit casts for malloc
        content = content.replace("Casl* self = malloc(sizeof(Casl));", "Casl* self = (Casl*)malloc(sizeof(Casl));")
        # Fix C++ switch case variable initialization bypass error by wrapping case LUA_TTABLE in braces
        content = content.replace("case LUA_TTABLE: // handle behavioural-types", "case LUA_TTABLE: { // handle behavioural-types")
        content = content.replace(
            '            lua_pop(L, 1);\n            break;\n\n        default:\n            printf("ERROR unknown To type\\n");',
            '            lua_pop(L, 1);\n            break;\n        }\n\n        default:\n            printf("ERROR unknown To type\\n");'
        )
        
    elif src_rel == "lib/slopes.c":
        # C++ explicit casts for malloc
        content = content.replace("slopes = malloc( sizeof ( Slope_t ) * channels );", "slopes = (Slope_t*)malloc( sizeof ( Slope_t ) * channels );")
        # Fix volatile struct copies
        content = content.replace("slope_cmd_queue[found_pos] = *cmd;", "*(slope_cmd_t*)&slope_cmd_queue[found_pos] = *cmd;")
        content = content.replace("slope_cmd_queue[slope_cmd_write_idx] = *cmd;", "*(slope_cmd_t*)&slope_cmd_queue[slope_cmd_write_idx] = *cmd;")
        content = content.replace("*out = slope_cmd_queue[read];", "*out = *(slope_cmd_t*)&slope_cmd_queue[read];")
        
    elif src_rel == "lib/ashapes.c":
        # C++ explicit casts for malloc
        content = content.replace("ashapers = malloc( sizeof( AShape_t ) * channels );", "ashapers = (AShape_t*)malloc( sizeof( AShape_t ) * channels );")
        
    elif src_rel == "lib/metro.c":
        # C++ explicit casts for malloc
        content = content.replace("metros = malloc( sizeof(Metro_t) * max_num_metros );", "metros = (Metro_t*)malloc( sizeof(Metro_t) * max_num_metros );")
        
    elif src_rel == "lib/clock_ll.c":
        # C++ explicit casts for malloc
        content = content.replace("clock_pool = malloc( sizeof(clock_node_t) * max_clocks );", "clock_pool = (clock_node_t*)malloc( sizeof(clock_node_t) * max_clocks );")
        
    elif src_rel == "lib/ll_timers.c":
        # C++ explicit casts for malloc
        content = content.replace("timers = malloc(sizeof(bb_timer_t) * max_timers);", "timers = (bb_timer_t*)malloc(sizeof(bb_timer_t) * max_timers);")
        
    elif src_rel == "lib/detect.c":
        # Replace Cortex-M0+ memory barriers with portable barriers
        content = content.replace('#define DMB() __asm volatile ("dmb" ::: "memory")', '#define DMB() asm volatile("" ::: "memory")')
        content = content.replace('#define DSB() __asm volatile ("dsb" ::: "memory")', '#define DSB() asm volatile("" ::: "memory")')
        
    elif src_rel == "lib/events_lockfree.c":
        # Replace Cortex-M0+ memory barriers with portable barriers
        content = content.replace('#define DMB() __asm volatile ("dmb" ::: "memory")', '#define DMB() asm volatile("" ::: "memory")')
        content = content.replace('#define DSB() __asm volatile ("dsb" ::: "memory")', '#define DSB() asm volatile("" ::: "memory")')
        
        # Replace volatile structure copies with cast-away-volatile references to satisfy C++ assignment
        content = re.sub(
            r"(bool clock_lockfree_get\(clock_event_lockfree_t\* event\)\s*\{[\s\S]*?)\*event = queue->events\[current_read\];",
            r"\g<1>*event = *(clock_event_lockfree_t*)&queue->events[current_read];",
            content
        )
        content = re.sub(
            r"(bool clock_lockfree_peek\(clock_event_lockfree_t\* event\)\s*\{[\s\S]*?)\*event = queue->events\[current_read\];",
            r"\g<1>*event = *(clock_event_lockfree_t*)&queue->events[current_read];",
            content
        )
        content = re.sub(
            r"(bool metro_lockfree_get\(metro_event_lockfree_t\* event\)\s*\{[\s\S]*?)\*event = queue->events\[current_read\];",
            r"\g<1>*event = *(metro_event_lockfree_t*)&queue->events[current_read];",
            content
        )
        content = re.sub(
            r"(bool metro_lockfree_peek\(metro_event_lockfree_t\* event\)\s*\{[\s\S]*?)\*event = queue->events\[current_read\];",
            r"\g<1>*event = *(metro_event_lockfree_t*)&queue->events[current_read];",
            content
        )
        content = re.sub(
            r"(bool input_lockfree_get\(input_event_lockfree_t\* event\)\s*\{[\s\S]*?)\*event = queue->events\[current_read\];",
            r"\g<1>*event = *(input_event_lockfree_t*)&queue->events[current_read];",
            content
        )
        content = re.sub(
            r"(bool asl_done_lockfree_get\(asl_done_event_lockfree_t\* event\)\s*\{[\s\S]*?)\*event = queue->events\[current_read\];",
            r"\g<1>*event = *(asl_done_event_lockfree_t*)&queue->events[current_read];",
            content
        )
        content = re.sub(
            r"(bool input_lockfree_post_extended\(const input_event_lockfree_t\* event\)\s*\{[\s\S]*?)queue->events\[current_write\] = \*event;",
            r"\g<1>*(input_event_lockfree_t*)&queue->events[current_write] = *event;",
            content
        )
        
    return content
