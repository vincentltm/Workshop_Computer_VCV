def post_process(src_content, src_rel):
    if src_rel == "main.cpp":
        # Fix the race condition in the Core 1 handshake wait loop
        src_content = src_content.replace(
            "while (multicore_fifo_rvalid())",
            "while (!multicore_fifo_rvalid()) { sleep_ms(1); }"
        )
    elif src_rel == "MainApp.cpp":
        # Initialize settings in MainApp constructor to prevent null dereference in ProcessSample()
        src_content = src_content.replace(
            "ui.init(this, &clk);",
            "ui.init(this, &clk);\n    LoadSettings(false);"
        )
    elif src_rel == "Config.cpp":
        src_content = src_content.replace(
            "static const uint8_t *CONFIG_FLASH_PTR = reinterpret_cast<const uint8_t *>(XIP_BASE + Config::OFFSET);",
            "#define CONFIG_FLASH_PTR (reinterpret_cast<const uint8_t *>(XIP_BASE + Config::OFFSET))"
        )
    return src_content


