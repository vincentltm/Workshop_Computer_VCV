def post_process(src_content, src_rel):
    if src_rel == "main.cpp":
        # Replace the tight busy-loop in usb_core0_loop with a yielding loop
        target_loop = """  for (;;) {
    const absolute_time_t slice_end = make_timeout_time_ms(1);
    while (!time_reached(slice_end)) {
      if (is_host) {
        tuh_task();
      } else {
        tud_task();
      }
    }
    if (!is_host) {
      card->Housekeeping();
    }
  }"""
        replacement_loop = """  while (1) {
    if (!is_host) {
      card->Housekeeping();
    }
    sleep_ms(1);
  }"""
        src_content = src_content.replace(target_loop, replacement_loop)
    if src_rel == "ConfigStore.cpp":
        src_content = src_content.replace(
            "static const uint8_t* kFlashPtr = reinterpret_cast<const uint8_t*>(XIP_BASE + ConfigStore::kOffset);",
            "#define kFlashPtr (reinterpret_cast<const uint8_t*>(XIP_BASE + ConfigStore::kOffset))"
        )
    return src_content



