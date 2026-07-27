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
        # 3. Periodically call process_usb_midi_device and tick_ui_once inside ProcessSample (on host builds)
        content = content.replace(
            'bool tuh_midi_packet_read(uint8_t dev_addr, uint8_t packet[4]);',
            '// bool tuh_midi_packet_read forward decl removed for pico_mocks inline'
        )
        content = content.replace(
            'void __not_in_flash_func(ClockworksCard::ProcessSample)() {',
            'void __not_in_flash_func(ClockworksCard::ProcessSample)() {\n#if !defined(__arm__) && !defined(VCV_PORT)\n    static uint32_t ui_counter = 0;\n    ui_counter++;\n    if (ui_counter >= 48) {\n        ui_counter = 0;\n        process_usb_midi_device();\n        tick_ui_once();\n    }\n#endif'
        )
        
    return content
