import re

def post_process(src_content, src_rel):
    if not src_rel.endswith("modes.cpp"):
        return src_content

    # Fix LED update throttle: loopCount < 1000 with sleep_ms(2) in Run()
    # means LEDs update every ~2 seconds. Reduce to 5 iterations (10ms).
    src_content = src_content.replace(
        'if (++loopCount < 1000)\n      return;\n    loopCount = 0;',
        'if (++loopCount < 5)\n      return;\n    loopCount = 0;'
    )

    # Convert loop variables to static so they persist across tick activations
    src_content = src_content.replace("uint32_t prng_seed = 0x12345678;", "static uint32_t prng_seed = 0x12345678;")
    src_content = src_content.replace("int32_t random_pitch = 0;", "static int32_t random_pitch = 0;")
    src_content = src_content.replace("int32_t random_timbre = 0;", "static int32_t random_timbre = 0;")

    # Find the core1_dsp_loop while(1) loop and wrap/inline it
    idx = src_content.find("void __not_in_flash_func(core1_dsp_loop)()")
    if idx != -1:
        while_idx = src_content.find("while (1)", idx)
        if while_idx != -1:
            brace_pos = src_content.find("{", while_idx)
            if brace_pos != -1:
                brace_count = 1
                curr_pos = brace_pos + 1
                while brace_count > 0 and curr_pos < len(src_content):
                    if src_content[curr_pos] == '{':
                        brace_count += 1
                    elif src_content[curr_pos] == '}':
                        brace_count -= 1
                    curr_pos += 1
                closing_brace_idx = curr_pos - 1
                loop_body = src_content[brace_pos+1 : closing_brace_idx]
                
                # Replace local thread-blocking and continue statements with cooperative inline returns
                loop_body = loop_body.replace(
                    "if (pause_core1) {\n      core1_is_paused = true;\n      while (pause_core1) {\n        tight_loop_contents();\n      }\n      core1_is_paused = false;\n    }",
                    "if (pause_core1) { core1_is_paused = true; return; }\n    core1_is_paused = false;"
                )
                loop_body = loop_body.replace(
                    "if (!multicore_fifo_rvalid()) {\n      tight_loop_contents();\n      continue;\n    }",
                    "if (!multicore_fifo_rvalid()) return;"
                )
                loop_body = loop_body.replace("      continue;\n    }", "      return;\n    }")
                loop_body = loop_body.replace("      continue;\n  }", "      return;\n  }")
                loop_body = loop_body.replace("    continue;\n  }", "    return;\n  }")
                loop_body = loop_body.replace("    continue;\n}", "    return;\n}")
                
                replacement = f"""#if defined(__EMSCRIPTEN__)
    g_wasm_core1_tick = []() {{
        {loop_body}
    }};
#else
    while (1) {{
        {loop_body}
    }}
#endif"""
                src_content = src_content[:while_idx] + replacement + src_content[closing_brace_idx+1:]

    return src_content
