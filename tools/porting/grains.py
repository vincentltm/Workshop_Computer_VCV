import re

def post_process(src_content, src_rel):
    src_content = src_content.replace('if (++lt > 5000) {', 'if (++lt > 5) {')
    
    # Remove 'static' from 'static Grains grains;' so it re-initializes on every card launch
    src_content = src_content.replace('static Grains grains;', 'Grains grains;')

    # Replace Core 0 Non-Tape Mode FIFO: use non-blocking 3-item pop to avoid deadlock on audio thread.
    old_non_tape_pop = """      int32_t sL = (int32_t)multicore_fifo_pop_blocking(),
              sR = (int32_t)multicore_fifo_pop_blocking(),
              eS = (int32_t)multicore_fifo_pop_blocking();"""

    new_non_tape_pop = """      int32_t sL = 0, sR = 0, eS = 0;
      if (g_fifo_1_to_0.size() >= 3) {
        sL = (int32_t)g_fifo_1_to_0.pop();
        sR = (int32_t)g_fifo_1_to_0.pop();
        eS = (int32_t)g_fifo_1_to_0.pop();
      }"""

    src_content = src_content.replace('multicore_fifo_push_blocking(trig);', 'g_fifo_0_to_1.push(trig);')
    src_content = src_content.replace('multicore_fifo_push_blocking(0x80000000);', 'g_fifo_0_to_1.push(0x80000000);')
    src_content = src_content.replace(old_non_tape_pop, new_non_tape_pop)

    old_tape_pop = """        int32_t sL = (int32_t)multicore_fifo_pop_blocking();
        int32_t sR = (int32_t)multicore_fifo_pop_blocking();
        int32_t speedCV = ((int32_t)multicore_fifo_pop_blocking()) / 2;"""

    new_tape_pop = """        int32_t sL = 0, sR = 0, speedCV = 0;
        if (g_fifo_1_to_0.size() >= 3) {
          sL = (int32_t)g_fifo_1_to_0.pop();
          sR = (int32_t)g_fifo_1_to_0.pop();
          speedCV = ((int32_t)g_fifo_1_to_0.pop()) / 2;
        }"""

    src_content = src_content.replace(old_tape_pop, new_tape_pop)

    # Inject CVOut1(2047) debug probe to verify if ProcessSample is running
    src_content = src_content.replace(
        'void __not_in_flash_func(ProcessSample)() override {',
        'void __not_in_flash_func(ProcessSample)() override {\n    CVOut1(2047);'
    )

    # Find core1_worker's while(1) loop and wrap/inline it
    idx = src_content.find("void __not_in_flash_func(core1_worker)()")
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
                loop_body = loop_body.replace("while (core1_paused) {\n      }", "if (core1_paused) { core1_is_paused = true; return; }\n      core1_is_paused = false;")
                loop_body = loop_body.replace("continue;", "return;")
                
                replacement = f"""#if defined(__EMSCRIPTEN__)
    g_wasm_core1_tick = []() {{
        {loop_body}
    }};
#else
    while (!g_cancellation_requested.load(std::memory_order_relaxed)) {{
        {loop_body}
    }}
#endif"""
                src_content = src_content[:while_idx] + replacement + src_content[closing_brace_idx+1:]

    return src_content
