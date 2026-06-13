def post_process(content, src_rel):
    if src_rel == "usb_core1.cpp":
        # Normalize line endings
        content = content.replace("\r\n", "\n")
        
        target = """extern "C" void core1_entry(void)
{
    if (gUsbHostMode) {
        run_host_loop();    // never returns
    } else {
        run_device_loop();  // never returns
    }
}"""
        
        replacement = """void tuh_cdc_mount_cb(uint8_t idx);
void tuh_cdc_umount_cb(uint8_t idx);

static void drumdrum_on_grid_connected(bool connected) {
    if (connected) {
        tuh_cdc_mount_cb(0);
    } else {
        tuh_cdc_umount_cb(0);
    }
}

extern "C" void core1_entry(void)
{
    if (t_instance) {
        t_instance->on_grid_connected_fn = drumdrum_on_grid_connected;
        if (t_instance->grid_connected_flag) {
            tuh_cdc_mount_cb(0);
        }
    }
    board_init();
    mext_init(MEXT_TRANSPORT_HOST, 0);
    midi_host_init();
    tusb_init();
    grid_ui_init();
    while (true) {
        tud_task();
        midi_device_task();
        mext_task();
        grid_ui_process_input();
        grid_ui_render();
    }
}"""
        
        if target in content:
            content = content.replace(target, replacement)
            print("Successfully patched core1_entry in usb_core1.cpp for drumdrum.")
        else:
            print("Warning: Target core1_entry not found in usb_core1.cpp for drumdrum.")
            
    return content
