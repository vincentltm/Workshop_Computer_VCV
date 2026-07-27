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
        # Allow Core 1 thread to exit when card stops or changes
        src_content = src_content.replace(
            'while (true) {',
            'while (!g_cancellation_requested.load(std::memory_order_relaxed)) {'
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

    return src_content
