def post_process(src_content, src_rel):
    src_content = src_content.replace(
        'db->buffer = malloc(bufferSize * sizeof(int32_t));',
        'db->buffer = (int32_t*)malloc(bufferSize * sizeof(int32_t));'
    )
    src_content = src_content.replace(
        'reverb *v = malloc(sizeof(reverb));',
        'reverb *v = (reverb*)malloc(sizeof(reverb));'
    )
    src_content = src_content.replace(
        'dv = reverb_create();',
        'dv = reverb_create();\n\tstatic ReverbCard reverb_card;'
    )
    src_content = src_content.replace('static int mux_state = 0;', '')
    src_content = src_content.replace(
        'int rv2 = tud_midi_stream_write(0, packet, bytes_this_packet + SYSEX_READ_CONFIG_HEADER_LEN + 1);',
        'tud_midi_stream_write(0, packet, bytes_this_packet + SYSEX_READ_CONFIG_HEADER_LEN + 1);'
    )
    src_content = src_content.replace(
        'int rv3 = tud_midi_stream_write(0, packet, 9);',
        'tud_midi_stream_write(0, packet, 9);'
    )
    return src_content

def get_header_definitions():
    return """    int mux_state = 0;

    class ReverbCard : public ::ComputerCard {
    public:
        ReverbCard();
        ~ReverbCard();
        void ProcessSample() override;
    };
"""

def get_extra_definitions():
    return """
    ReverbCard::ReverbCard() {
        thisptr = this;
        if (t_instance) {
            t_instance->card_ptr = this;
            t_instance->g_dsp_ready = true;
        }
    }
    ReverbCard::~ReverbCard() {
        if (thisptr == this) thisptr = nullptr;
        if (t_instance && t_instance->card_ptr == this) {
            t_instance->card_ptr = nullptr;
        }
    }
    
    void ReverbCard::ProcessSample() {
        uint16_t main_val = (uint16_t)(g_knobs[0] * 4095.f);
        uint16_t x_val = (uint16_t)(g_knobs[1] * 4095.f);
        uint16_t y_val = (uint16_t)(g_knobs[2] * 4095.f);
        uint16_t z_val = 2048;
        if (g_switch == 0) z_val = 0;
        else if (g_switch == 2) z_val = 4095;

        if (mux_state == 0) ADC_Buffer[dmaPhase][6] = main_val;
        else if (mux_state == 1) ADC_Buffer[dmaPhase][6] = x_val;
        else if (mux_state == 2) ADC_Buffer[dmaPhase][6] = y_val;
        else if (mux_state == 3) ADC_Buffer[dmaPhase][6] = z_val;

        float cv1_volts = g_cv_in[0];
        float cv2_volts = g_cv_in[1];
        int32_t cv1_val = (int32_t)((cv1_volts + 5.f) * (4095.f / 10.f));
        int32_t cv2_val = (int32_t)((cv2_volts + 5.f) * (4095.f / 10.f));
        if (cv1_val < 0) cv1_val = 0; if (cv1_val > 4095) cv1_val = 4095;
        if (cv2_val < 0) cv2_val = 0; if (cv2_val > 4095) cv2_val = 4095;

        int cvi = mux_state % 2;
        if (cvi == 0) ADC_Buffer[dmaPhase][7] = cv1_val;
        else ADC_Buffer[dmaPhase][7] = cv2_val;

        float inL = g_audio_in[0];
        float inR = g_audio_in[1];
        int32_t valL = (int32_t)(inL * 2048.f / 5.f);
        int32_t valR = (int32_t)(inR * 2048.f / 5.f);
        if (valL < -2048) valL = -2048; if (valL > 2047) valL = 2047;
        if (valR < -2048) valR = -2048; if (valR > 2047) valR = 2047;

        ADC_Buffer[dmaPhase][0] = valR + 2048;
        ADC_Buffer[dmaPhase][4] = valR + 2048;
        ADC_Buffer[dmaPhase][1] = valL + 2048;
        ADC_Buffer[dmaPhase][5] = valL + 2048;

        buffer_full();

        g_audio_out[0] = (float)dacOutL * 5.f / 2048.f;
        g_audio_out[1] = (float)dacOutR * 5.f / 2048.f;
    }
"""
