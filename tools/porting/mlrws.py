import re

def post_process(content, src_rel):
    if src_rel == "main.cpp":
        # Normalize line endings to \n for matching
        content = content.replace("\r\n", "\n")
        
        # Bypass the 2-second protocol detection delay at boot
        content = content.replace("device_mode_detect_protocol(2000)", "{}")
        
        # Target implementation of run_device_gridless_core1_loop
        target = """	static void run_device_gridless_core1_loop()
	{
		device_mode_init();  /* pre-init so it's ready if needed */
		while (true) {
			tud_task();

			/* ---- Monitor CDC: if a sample-manager byte arrives, transition ---- */
			if (tud_cdc_n_connected(0) && tud_cdc_n_available(0) > 0) {
				uint8_t peek = 0;
				tud_cdc_n_read(0, &peek, 1);

				if (sample_mgr_wake_byte(peek)) {
					run_sample_manager_until_disconnect(peek, true);
				}
				/* else: unknown byte — ignore, stay in gridless mode */
			}

			mlr_io_task();
		}
	}"""
        
        replacement = """	static void run_grid_device_until_disconnect(uint8_t first_byte)
	{
		if (s_card_) {
			s_card_->mode_ = Mode::DeviceMLR;
		}
		s_mode_ = Mode::DeviceMLR;
		__dmb();

		monome_ws_init(MONOME_WS_TRANSPORT_DEVICE, 0);
		monome_ws_connect(0);
		monome_ws_rx_feed(&first_byte, 1);

		while (tud_cdc_n_connected(0)) {
			tud_task();
			if (tud_cdc_n_available(0) > 0) {
				uint8_t peek = 0;
				tud_cdc_n_read(0, &peek, 1);
				if (sample_mgr_wake_byte(peek)) {
					run_sample_manager_until_disconnect(peek, false);
					break;
				} else {
					monome_ws_rx_feed(&peek, 1);
				}
			}
			monome_ws_task();
			mlr_io_task();
			service_panel_vu_leds_core1();
			service_grid_redraw_core1();
		}

		if (s_card_) {
			s_card_->mode_ = Mode::DeviceGridless;
		}
		s_mode_ = Mode::DeviceGridless;
		s_rescan_needed_ = true;
		__dmb();
	}

	static void run_device_gridless_core1_loop()
	{
		device_mode_init();  /* pre-init so it's ready if needed */
		while (true) {
			tud_task();

			/* ---- Monitor CDC: if a sample-manager byte arrives, transition ---- */
			if (tud_cdc_n_connected(0) && tud_cdc_n_available(0) > 0) {
				uint8_t peek = 0;
				tud_cdc_n_read(0, &peek, 1);

				if (sample_mgr_wake_byte(peek)) {
					run_sample_manager_until_disconnect(peek, true);
				} else {
					run_grid_device_until_disconnect(peek);
				}
			}

			mlr_io_task();
		}
	}"""
        
        if target in content:
            content = content.replace(target, replacement)
            print("Successfully patched run_device_gridless_core1_loop and bypassed 2s delay in main.cpp for mlrws.")
        else:
            print("Warning: Target run_device_gridless_core1_loop not found in main.cpp for mlrws.")
            
    return content
