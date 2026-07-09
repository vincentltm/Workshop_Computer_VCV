def post_process(content, src_rel):
    if src_rel == "main.cpp":
        # 1. Division / multiplication hardware/asm bypass
        content = content.replace(
            'inline uint32_t __not_in_flash_func(safe_div_u32)(uint32_t dividend, uint32_t divisor) {',
            '#ifdef __arm__\ninline uint32_t __not_in_flash_func(safe_div_u32)(uint32_t dividend, uint32_t divisor) {'
        )
        content = content.replace(
            '    return hw_divider_u32_quotient_wait();\n}',
            '    return hw_divider_u32_quotient_wait();\n}\n#else\ninline uint32_t safe_div_u32(uint32_t dividend, uint32_t divisor) {\n    if (divisor == 0) return 0;\n    return dividend / divisor;\n}\n#endif'
        )
        
        content = content.replace(
            'inline uint32_t __not_in_flash_func(mul16_16)(uint32_t a, uint32_t b) {',
            '#ifdef __arm__\ninline uint32_t __not_in_flash_func(mul16_16)(uint32_t a, uint32_t b) {'
        )
        content = content.replace(
            '    asm ("mul %0, %1" : "=l" (res) : "l" (b), "0" (a) : "cc");\n    return res;\n}',
            '    asm ("mul %0, %1" : "=l" (res) : "l" (b), "0" (a) : "cc");\n    return res;\n}\n#else\ninline uint32_t mul16_16(uint32_t a, uint32_t b) {\n    return (uint32_t)((a & 0xffff) * (b & 0xffff));\n}\n#endif'
        )
        
        # 2. Comment out audio_callback_ptr and audio_callback_inst declarations/assignments
        content = content.replace(
            'void (*ComputerCard::audio_callback_ptr)(void*) = nullptr;',
            '// void (*ComputerCard::audio_callback_ptr)(void*) = nullptr;'
        )
        content = content.replace(
            'void *ComputerCard::audio_callback_inst = nullptr;',
            '// void *ComputerCard::audio_callback_inst = nullptr;'
        )
        content = content.replace(
            'ComputerCard::audio_callback_inst = this;',
            '// ComputerCard::audio_callback_inst = this;'
        )
        content = content.replace(
            'ComputerCard::audio_callback_ptr = audio_callback_ram_wrapper;',
            '// ComputerCard::audio_callback_ptr = audio_callback_ram_wrapper;'
        )
        
    return content
