import os
import re

def get_extra_include_dirs(card_dir_abs):
    return [
        os.path.join(card_dir_abs, "braids"),
        os.path.join(card_dir_abs, "braids", "drivers")
    ]

def post_process(src_content, src_rel):
    # Only post-process twists.cc
    if not src_rel.endswith("twists.cc"):
        return src_content

    # 1. Comment out ADC register / interrupt setup in Init()
    adc_init_block = """  adc_init();
  adc_gpio_init(PIN_MUX_OUT_X);
  adc_gpio_init(PIN_MUX_OUT_Y);

  adc_set_round_robin(0x0F);
\tadc_fifo_setup(true, true, 1, false, false);
\tadc_set_clkdiv((48000000 / 96000) - 1); // 96KHz interrupt
\tirq_set_exclusive_handler(ADC_IRQ_FIFO, timer_callback);
\tadc_irq_set_enabled(true);
\tirq_set_enabled(ADC_IRQ_FIFO, true);
\tadc_run(true);"""

    if adc_init_block in src_content:
        src_content = src_content.replace(adc_init_block, "// ADC Init disabled in VCV")
    else:
        # Fallback simpler replacements if spacing differs
        src_content = src_content.replace("adc_run(true);", "// adc_run(true);")
        src_content = src_content.replace("irq_set_enabled(ADC_IRQ_FIFO, true);", "// irq_set_enabled(ADC_IRQ_FIFO, true);")
        src_content = src_content.replace("adc_irq_set_enabled(true);", "// adc_irq_set_enabled(true);")

    # 2. Replace the original main() with our VCV-aware version.
    #    Use regex to strip everything from "uint32_t last = 0;" through
    #    the closing brace of int main(), then append our new code.
    vcv_replacement = """
// ─── VCV Rack bridge ──────────────────────────────────────────────────────────

class TwistsComputerCard : public ComputerCard {
public:
    TwistsComputerCard() : ComputerCard() {}

    void ProcessSample() override {
        // 1. Map Knobs from VCV (0..1 → 0..4095)
        knobs[0] = (uint16_t)(g_knobs[0] * 4095.f);
        knobs[1] = (uint16_t)(g_knobs[1] * 4095.f);
        knobs[2] = (uint16_t)(g_knobs[2] * 4095.f);
        // Z switch: 0=pressed(bottom), 1=middle, 2=top
        // knobs[3] drives ui.Poll() for shape/mode cycling
        if (g_switch == 0) {
            knobs[3] = 0;    // bottom pressed: advance shape
        } else if (g_switch == 2) {
            knobs[3] = 4095; // top held
        } else {
            knobs[3] = 2048; // middle (rest)
        }

        // 2. Map 1V/Oct CV using calibration slope/intercept
        if (slope != 0.0f) {
            cv[0] = (uint16_t)((g_cv_in[0] - intercept) / slope);
        } else {
            cv[0] = (uint16_t)(g_cv_in[0] * (4095.f / 10.f));
        }
        cv[1] = (uint16_t)(g_cv_in[1] * (4095.f / 10.f));

        // 3. Map Audio Inputs for FM / timbre modulation
        audio_in[0] = (uint16_t)(g_audio_in[0] * (2048.f / 5.0f)) + 2048;
        audio_in[1] = (uint16_t)(g_audio_in[1] * (2048.f / 5.0f)) + 2048;

        // 4. Read sample from background-rendered buffer.
        // Braids renders at 96kHz (hardware ADC rate), VCV runs at 48kHz.
        // Consume 2 rendered samples per VCV tick to maintain correct pitch/speed.
        // Interpolate between the pair for smoother output.
        int16_t s0 = audio_samples[playback_block][current_sample];
        int next_cs = (int)current_sample + 1;
        size_t next_pb = playback_block;
        if (next_cs >= (int)kBlockSize) { next_cs = 0; next_pb = (next_pb + 1) % kNumBlocks; }
        int16_t s1 = audio_samples[next_pb][next_cs];
        int16_t raw_sample = (int16_t)((s0 + s1) / 2);
        // Braids renders: (-sample + 32768) >> 5 -> range 0..2048, center = 1024
        float out_volts = ((float)raw_sample - 1024.f) / 1024.f * 5.0f;
        g_audio_out[0] = out_volts;
        g_audio_out[1] = out_volts;

        // 5. Sync/Trigger pulse output
        g_pulse_out[0] = sync_samples[playback_block][current_sample] != 0;

        // 6. Advance play queue by 2 to match 96kHz render rate at 48kHz playback
        for (int _i = 0; _i < 2; _i++) {
            current_sample = current_sample + 1;
            if (current_sample >= (int)kBlockSize) {
                current_sample = 0;
                playback_block = (playback_block + 1) % kNumBlocks;
            }
        }
    }

    void BackgroundLoop() override {
        // unused: render loop driven by main() below
    }
};

static TwistsComputerCard twists_card;

uint32_t last = 0;
int main() {
    // Register the card so ProcessSample() is wired up per audio tick
    ComputerCard::thisptr = &twists_card;
    if (t_instance) {
        t_instance->card_ptr = &twists_card;
        t_instance->g_dsp_ready = true;
    }

    Init();
    calibration.readCVInCalibration(&slope, &intercept);

    while (!g_cancellation_requested.load(std::memory_order_relaxed)) {
        // Update quantizer if scale changed
        if (current_scale != settings.GetValue(SETTING_QUANTIZER_SCALE)) {
            current_scale = settings.GetValue(SETTING_QUANTIZER_SCALE);
            quantizer.Configure(scales[current_scale]);
        }

        // Render audio blocks whenever render buffer is behind playback
        if (!DoCalibration()) {
            if (render_block != playback_block) {
                RenderBlock();
            }
        }

        // Poll UI every millisecond: drives LED display + switch debounce
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now != last) {
            last = now;
            ui.Poll(now, knobs[3]);
        }
    }
    return 0;
}
"""

    # Strip the original main() block using regex.
    # Pattern: "uint32_t last = 0;" followed by "int main() { ... }" (the closing } at start of line)
    stripped = re.sub(
        r'uint32_t last\s*=\s*0\s*;\s*int\s+main\s*\(\s*\)\s*\{.*?\n\}',
        '',
        src_content,
        flags=re.DOTALL
    )

    if stripped == src_content:
        # Regex didn't match — try simpler fallback: remove from "uint32_t last" to end of file
        idx = src_content.find("uint32_t last = 0;")
        if idx != -1:
            stripped = src_content[:idx]
        else:
            stripped = src_content

    src_content = stripped + vcv_replacement

    return src_content
