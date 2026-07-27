import re

def post_process(src_content, src_rel):
    src_content = src_content.replace('if (++lt > 5000) {', 'if (++lt > 5) {')
    
    # Remove 'static' from 'static Grains grains;' so it re-initializes on every card launch
    src_content = src_content.replace('static Grains grains;', 'Grains grains;')

    # Replace Core 0 Non-Tape Mode FIFO: use non-blocking 3-item pop to avoid deadlock.
    # Must check all 3 items available before consuming any (avoids blocking mid-burst).
    old_non_tape_fifo = """    if (++gPh >= 2) {
      gPh = 0;
      if (multicore_fifo_rvalid()) {
        uint32_t trig = 1;
        if (trigger1Buffered) {
          trig |= 2;
          trigger1Buffered = false;
        }
        int32_t sL = (int32_t)multicore_fifo_pop_blocking(),
                sR = (int32_t)multicore_fifo_pop_blocking(),
                __attribute__((unused)) eS =
                    (int32_t)multicore_fifo_pop_blocking();
        int32_t fA = params[2].pX, smL = fDL.process(sL), smR = fDR.process(sR);
        if (freezeMode || synthMode) {
          // Freeze: Knob X is ONLY Diffusion (Smear). NO buffer writing.
          gL48 =
              (int32_t)(((int64_t)sL * (32767 - fA) + (int64_t)smL * fA) >> 15);
          gR48 =
              (int32_t)(((int64_t)sR * (32767 - fA) + (int64_t)smR * fA) >> 15);
        } else {
          // Live mode: Knob X is ONLY Feedback.
          gL48 = sL;
          gR48 = sR;
        }
        multicore_fifo_push_blocking(trig);
      }"""

    new_non_tape_fifo = """    if (++gPh >= 2) {
      gPh = 0;
      int32_t sL = (int32_t)g_fifo_1_to_0.pop();
      int32_t sR = (int32_t)g_fifo_1_to_0.pop();
      int32_t eS = (int32_t)g_fifo_1_to_0.pop();
      int32_t fA = params[2].pX, smL = fDL.process(sL), smR = fDR.process(sR);
      if (freezeMode || synthMode) {
        // Freeze: Knob X is ONLY Diffusion (Smear). NO buffer writing.
        gL48 =
            (int32_t)(((int64_t)sL * (32767 - fA) + (int64_t)smL * fA) >> 15);
        gR48 =
            (int32_t)(((int64_t)sR * (32767 - fA) + (int64_t)smR * fA) >> 15);
      } else {
        // Live mode: Knob X is ONLY Feedback.
        gL48 = sL;
        gR48 = sR;
      }
      // Push trigger AFTER successfully popping all 3 outputs
      uint32_t trig = 1;
      if (trigger1Buffered) {
        trig |= 2;
        trigger1Buffered = false;
      }
      g_fifo_0_to_1.push(trig);"""

    if old_non_tape_fifo in src_content:
        src_content = src_content.replace(old_non_tape_fifo, new_non_tape_fifo)
    else:
        print("WARNING: grains.py non-tape FIFO pattern not matched — check indentation!")

    # Replace Core 0 Tape Mode FIFO check & push/pop sequence to be non-blocking and self-correcting (deadlock-free)
    old_tape_fifo = """      if (++gPh >= 2) {
        gPh = 0;
        if (multicore_fifo_rvalid()) {
          int32_t sL = (int32_t)multicore_fifo_pop_blocking();
          int32_t sR = (int32_t)multicore_fifo_pop_blocking();
          int32_t speedCV = ((int32_t)multicore_fifo_pop_blocking()) / 2;
          multicore_fifo_push_blocking(0x80000000); // Special bit for Tape Mode

          // Tape Saturation (Refined Soft Clipping)
          auto saturate = [](int32_t x) {
            if (x > 12000)
              x = 12000 + ((x - 12000) >> 2);
            if (x < -12000)
              x = -12000 + ((x + 12000) >> 2);
            return x;
          };
          gL48 = saturate(sL);
          gR48 = saturate(sR);

          // CV2 Out: Movement/Speed CV (-2048 to 2047)
          if (speedCV > 2047)
            speedCV = 2047;
          if (speedCV < -2048)
            speedCV = -2048;
          CVOut2((int16_t)speedCV);
        }
      }"""

    new_tape_fifo = """      if (++gPh >= 2) {
        gPh = 0;
        int32_t sL = (int32_t)g_fifo_1_to_0.pop();
        int32_t sR = (int32_t)g_fifo_1_to_0.pop();
        int32_t speedCV = ((int32_t)g_fifo_1_to_0.pop()) / 2;

        auto saturate = [](int32_t x) {
          if (x > 12000)
            x = 12000 + ((x - 12000) >> 2);
          if (x < -12000)
            x = -12000 + ((x + 12000) >> 2);
          return x;
        };
        gL48 = saturate(sL);
        gR48 = saturate(sR);

        if (speedCV > 2047)
          speedCV = 2047;
        if (speedCV < -2048)
          speedCV = -2048;
        CVOut2((int16_t)speedCV);
        g_fifo_0_to_1.push(0x80000000);
      }"""

    if old_tape_fifo in src_content:
        src_content = src_content.replace(old_tape_fifo, new_tape_fifo)
    else:
        print("WARNING: grains.py tape FIFO pattern not matched — check indentation!")

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
