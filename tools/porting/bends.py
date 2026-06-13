def post_process(src_content, src_rel):
    # Replace run_core0_ui_loop with a simplified loop that doesn't use the FIFO or reverb process
    def replace_function_body(content, signature, new_body):
        idx = content.find(signature)
        if idx == -1:
            return content
        brace_idx = content.find('{', idx)
        if brace_idx == -1:
            return content
        brace_count = 1
        i = brace_idx + 1
        while i < len(content) and brace_count > 0:
            if content[i] == '{':
                brace_count += 1
            elif content[i] == '}':
                brace_count -= 1
            i += 1
        if brace_count == 0:
            return content[:brace_idx] + new_body + content[i:]
        return content

    new_ui_loop = """{
        while (1) {
            if (g_cancellation_requested.load(std::memory_order_relaxed)) {
                throw ThreadExitException();
            }
            tick_ui_once();
            sleep_ms(1);
        }
    }"""
    src_content = replace_function_body(src_content, "void BendsCard::run_core0_ui_loop()", new_ui_loop)
    return src_content

def get_header_definitions():
    return """    // Local overrides for synchronous reverb forward declarations
    void multicore_fifo_push_blocking(uintptr_t packed_filt);
    uintptr_t multicore_fifo_pop_blocking();
    bool multicore_fifo_rvalid();
"""

def get_extra_definitions():
    return """    // Definitions of local overrides for synchronous reverb
    static uint32_t g_last_reverb_result = 0;

    void multicore_fifo_push_blocking(uintptr_t packed_filt) {
        int16_t filtL = (int16_t)(packed_filt >> 16);
        int16_t filtR = (int16_t)(packed_filt & 0xFFFF);
        int16_t L = filtL;
        int16_t R = filtR;
        int32_t eff_fb_glitch = c0_reverb_fb_glitch;
        int32_t active_macro = g_macro_active ? grittiness_macro : 16384;
        if (active_macro < 16384) {
            if (c0_reverb_fb_glitch > 16384) {
                int32_t diff = c0_reverb_fb_glitch - 16384;
                eff_fb_glitch = 16384 + ((diff * active_macro) >> 14);
            }
        } else {
            int32_t diff = 32767 - c0_reverb_fb_glitch;
            eff_fb_glitch = c0_reverb_fb_glitch + ((diff * (active_macro - 16384)) / 16383);
        }
        reverb.process(L, R, c0_reverb_mix, c0_reverb_size, eff_fb_glitch);
        g_last_reverb_result = ((uintptr_t)(uint16_t)L << 16) | (uint16_t)R;
    }

    uintptr_t multicore_fifo_pop_blocking() {
        return g_last_reverb_result;
    }

    bool multicore_fifo_rvalid() {
        return true;
    }
"""

