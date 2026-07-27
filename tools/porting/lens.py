import os

def get_extra_include_dirs(card_dir_abs):
    return [
        os.path.join(card_dir_abs, "runtime"),
        os.path.join(card_dir_abs, "runtime", "thirdparty"),
        os.path.join(card_dir_abs, "runtime", "thirdparty", "braids"),
        os.path.join(card_dir_abs, "runtime", "thirdparty", "stmlib"),
        os.path.join(card_dir_abs, "runtime", "thirdparty", "stmlib", "utils")
    ]

def post_process(src_content, src_rel):

    if src_rel.endswith("snapshot_apply.c"):
        src_content = src_content.replace(
            "rt->state_pool = alloc_nodestate(pool ? pool : 1);",
            "rt->state_pool = (uint8_t*)alloc_nodestate(pool ? pool : 1);"
        )
        src_content = src_content.replace(
            "bytes = alloc_audio(nbytes);",
            "bytes = (uint8_t*)alloc_audio(nbytes);"
        )
        src_content = src_content.replace(
            "bytes = alloc_control(nbytes);",
            "bytes = (uint8_t*)alloc_control(nbytes);"
        )
        src_content = src_content.replace(
            "uint8_t  lens_audio_pool[LENS_AUDIO_BUFFER_BYTES]",
            'extern "C" {\nuint8_t  lens_audio_pool[LENS_AUDIO_BUFFER_BYTES]'
        )
        src_content = src_content.replace(
            "int32_t  lens_shadow_pool[LENS_MAX_SLOTS];",
            'int32_t  lens_shadow_pool[LENS_MAX_SLOTS];\n}'
        )
        src_content = src_content.replace(
            "size_t lens_nodestate_used(void)",
            'extern "C" size_t lens_nodestate_used(void)'
        )
        src_content = src_content.replace(
            "size_t lens_control_used(void)",
            'extern "C" size_t lens_control_used(void)'
        )
        src_content = src_content.replace(
            "int snapshot_apply(struct LensRuntime** out_rt,",
            'extern "C" int snapshot_apply(struct LensRuntime** out_rt,'
        )

    if src_rel.endswith("main.cpp"):
        # Mock SysTick registers to avoid out-of-bounds memory dereferences in WASM/VCV
        src_content = src_content.replace(
            '#define M0P_SYSTICK_CSR (*((volatile uint32_t *)0xE000E010))',
            'static uint32_t mock_systick_csr = 0;\n#define M0P_SYSTICK_CSR mock_systick_csr'
        )
        src_content = src_content.replace(
            '#define M0P_SYSTICK_RVR (*((volatile uint32_t *)0xE000E014))',
            'static uint32_t mock_systick_rvr = 0;\n#define M0P_SYSTICK_RVR mock_systick_rvr'
        )
        src_content = src_content.replace(
            '#define M0P_SYSTICK_CVR (*((volatile uint32_t *)0xE000E018))',
            'static uint32_t mock_systick_cvr = 0;\n#define M0P_SYSTICK_CVR mock_systick_cvr'
        )
        # Inline Core 1 walk on Core 0 for VCV Rack compilation to avoid scheduling latency and dropouts
        src_content = src_content.replace(
            '            if (multicore_fifo_wready()) {\n                sio_hw->fifo_wr = seq;              /* triggers SIO_IRQ_PROC1 on Core 1 */\n            }',
            '#ifdef VCV_PORT\n            // Run Core 1 walk inline on Core 0 for 100% synchronous, crackle-free execution\n            runtime_walk_core1(rt, seq);\n            recordhead_sweep_core1(rt);\n            runtime_publish_shadows_core1(rt);\n#else\n            if (multicore_fifo_wready()) {\n                sio_hw->fifo_wr = seq;              /* triggers SIO_IRQ_PROC1 on Core 1 */\n            }\n#endif'
        )

    if src_rel.endswith("runtime.c"):
        import re
        src_content = src_content.replace(
            "static const uint16_t KSTATE_BYTES[KID_COUNT] = {",
            "static uint16_t KSTATE_BYTES[KID_COUNT];\nstatic void init_kstate_bytes() {"
        )
        src_content = re.sub(
            r'\[(KID_[A-Z0-9_]+)\]\s*=\s*([^,\n;]+)[,\n;]',
            r'KSTATE_BYTES[\1] = \2;',
            src_content
        )
        src_content = src_content.replace(
            "uint32_t runtime_kernel_state_bytes(uint8_t kid) {",
            "uint32_t runtime_kernel_state_bytes(uint8_t kid) {\n    init_kstate_bytes();"
        )
        src_content = src_content.replace(
            "void __not_in_flash_func(recordhead_sweep_core0)(struct LensRuntime* rt)",
            'extern "C" void __not_in_flash_func(recordhead_sweep_core0)(struct LensRuntime* rt)'
        )
        src_content = src_content.replace(
            "void __not_in_flash_func(recordhead_sweep_core1)(struct LensRuntime* rt)",
            'extern "C" void __not_in_flash_func(recordhead_sweep_core1)(struct LensRuntime* rt)'
        )
        src_content = src_content.replace(
            "void __not_in_flash_func(runtime_update_hw_scratch)(const struct HardwareInputs* hw)",
            'extern "C" void __not_in_flash_func(runtime_update_hw_scratch)(const struct HardwareInputs* hw)'
        )
        src_content = src_content.replace(
            "void __not_in_flash_func(runtime_walk_core0)(struct LensRuntime* rt, uint32_t seq)",
            'extern "C" void __not_in_flash_func(runtime_walk_core0)(struct LensRuntime* rt, uint32_t seq)'
        )
        src_content = src_content.replace(
            "void __not_in_flash_func(runtime_walk_core1)(struct LensRuntime* rt, uint32_t seq)",
            'extern "C" void __not_in_flash_func(runtime_walk_core1)(struct LensRuntime* rt, uint32_t seq)'
        )
        src_content = src_content.replace(
            "void __not_in_flash_func(runtime_drive_terminals)(struct LensRuntime* rt,",
            'extern "C" void __not_in_flash_func(runtime_drive_terminals)(struct LensRuntime* rt,'
        )
        src_content = src_content.replace(
            "void __not_in_flash_func(runtime_publish_shadows_core0)(struct LensRuntime* rt)",
            'extern "C" void __not_in_flash_func(runtime_publish_shadows_core0)(struct LensRuntime* rt)'
        )
        src_content = src_content.replace(
            "void __not_in_flash_func(runtime_publish_shadows_core1)(struct LensRuntime* rt)",
            'extern "C" void __not_in_flash_func(runtime_publish_shadows_core1)(struct LensRuntime* rt)'
        )
        src_content = src_content.replace(
            "void runtime_destroy(struct LensRuntime* rt)",
            'extern "C" void runtime_destroy(struct LensRuntime* rt)'
        )

    return src_content
