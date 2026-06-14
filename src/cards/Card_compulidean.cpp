#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

#ifndef _WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include "pico_mocks.h"
#include "ComputerCard.h"

// Sample names and expected filenames
// BD2550.WAV, SD0010.WAV, CH.WAV, OH25.WAV, CP.WAV, CB.WAV

struct WavData {
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint32_t block_align = 0;
    std::vector<int16_t> samples;
};

struct SamplerVoice {
    const WavData* wav = nullptr;
    double pos = 0.0;
    bool playing = false;
    float volume = 1.0f;
    
    void trigger(float vol = 1.0f) {
        pos = 0.0;
        playing = true;
        volume = vol;
    }
    
    int16_t next_sample() {
        if (!playing || !wav || wav->samples.empty()) return 0;
        
        size_t idx = (size_t)pos;
        if (idx >= wav->samples.size() - 1) {
            playing = false;
            return 0;
        }
        
        // Linear interpolation
        double frac = pos - idx;
        int16_t s0 = wav->samples[idx];
        int16_t s1 = wav->samples[idx + 1];
        int16_t out = (int16_t)((1.0 - frac) * s0 + frac * s1);
        
        pos += 1.0; // Play at original rate (48000 Hz)
        return (int16_t)(out * volume);
    }
};

class CompulideanCard : public ComputerCard {
public:
    static constexpr int NUM_VOICES = 6;
    
    WavData drum_wavs[NUM_VOICES];
    SamplerVoice voices[NUM_VOICES];
    
    // Densities for 6 channels (Kick, Snare, CHH, OHH, Clap, Cowbell)
    int densities[NUM_VOICES] = {4, 0, 8, 0, 0, 0};
    bool patterns[NUM_VOICES][16];
    
    // Clock variables
    float bpm = 120.f;
    float swing = 0.0f; // 0.0 to 0.75
    float master_volume = 0.8f;
    
    double clock_accumulator = 0.0;
    double step_duration_samples = 0.0;
    int current_step = 0;
    
    bool last_pulse1 = false;
    bool last_pulse2 = false;
    
    float led_decays[6] = {0.f};
    Switch last_sw = Switch::Middle;
    
    CompulideanCard() {
        std::string res_dir = get_resource_dir();
        std::cout << "[CompulideanCard] Loading WAV assets from: " << res_dir << "compulidean/" << std::endl;
        
        std::string files[NUM_VOICES] = {
            "BD2550.WAV",
            "SD0010.WAV",
            "CH.WAV",
            "OH25.WAV",
            "CP.WAV",
            "CB.WAV"
        };
        
        for (int i = 0; i < NUM_VOICES; ++i) {
            std::string full_path = res_dir + "compulidean/" + files[i];
            if (load_wav_file(full_path, drum_wavs[i])) {
                std::cout << "[CompulideanCard] Loaded " << files[i] << " successfully: " 
                          << drum_wavs[i].samples.size() << " samples" << std::endl;
                voices[i].wav = &drum_wavs[i];
            } else {
                std::cout << "[CompulideanCard] FAILED to load " << files[i] << std::endl;
            }
        }
        
        recalculate_patterns();
        update_clock_params();
    }
    
    void generate_euclidean(bool* pattern, int k, int n) {
        std::fill(pattern, pattern + n, false);
        if (n <= 0) return;
        if (k > n) k = n;
        if (k <= 0) return;
        
        int accumulator = 0;
        for (int i = 0; i < n; i++) {
            accumulator += k;
            if (accumulator >= n) {
                pattern[i] = true;
                accumulator -= n;
            }
        }
    }
    
    void recalculate_patterns() {
        for (int i = 0; i < NUM_VOICES; ++i) {
            generate_euclidean(patterns[i], densities[i], 16);
        }
    }
    
    void update_clock_params() {
        // Step duration (16th note) in samples = (60 / BPM) / 4 * 48000
        step_duration_samples = (60.0 / bpm) / 4.0 * 48000.0;
    }
    
    void trigger_step(int step) {
        for (int i = 0; i < NUM_VOICES; ++i) {
            if (patterns[i][step]) {
                voices[i].trigger(1.0f);
                led_decays[i] = 1.f;
            }
        }
    }

    virtual void ProcessSample() override {
        // Read controls and inputs
        Switch sw = SwitchVal();
        
        // Bipolar CV inputs offset densities
        int cv1_offset = Connected(Input::CV1) ? (int)(g_cv_in[0] * 8.f / 5.f) : 0;
        int cv2_offset = Connected(Input::CV2) ? (int)(g_cv_in[1] * 8.f / 5.f) : 0;
        
        // Handle Knob Editing depending on Switch mode
        if (sw == Switch::Up) {
            // Edit Kick, Snare, CHH densities
            int k_dens = (KnobVal(Knob::Main) * 17) >> 12; // 0 to 16
            int s_dens = (KnobVal(Knob::X) * 17) >> 12;    // 0 to 16
            int c_dens = (KnobVal(Knob::Y) * 17) >> 12;    // 0 to 16
            
            k_dens = std::clamp(k_dens + cv1_offset, 0, 16);
            s_dens = std::clamp(s_dens + cv2_offset, 0, 16);
            
            if (k_dens != densities[0] || s_dens != densities[1] || c_dens != densities[2]) {
                densities[0] = k_dens;
                densities[1] = s_dens;
                densities[2] = c_dens;
                recalculate_patterns();
            }
        } else if (sw == Switch::Middle) {
            // Edit OHH, Clap, Cowbell densities
            int o_dens = (KnobVal(Knob::Main) * 17) >> 12;
            int cl_dens = (KnobVal(Knob::X) * 17) >> 12;
            int co_dens = (KnobVal(Knob::Y) * 17) >> 12;
            
            o_dens = std::clamp(o_dens + cv1_offset, 0, 16);
            cl_dens = std::clamp(cl_dens + cv2_offset, 0, 16);
            
            if (o_dens != densities[3] || cl_dens != densities[4] || co_dens != densities[5]) {
                densities[3] = o_dens;
                densities[4] = cl_dens;
                densities[5] = co_dens;
                recalculate_patterns();
            }
        } else if (sw == Switch::Down) {
            // Global controls: BPM, Swing, Volume
            float new_bpm = 60.f + ((float)KnobVal(Knob::Main) / 4095.f) * 180.f; // 60 to 240 BPM
            float new_swing = ((float)KnobVal(Knob::X) / 4095.f) * 0.75f;        // 0 to 75% Swing
            float new_vol = ((float)KnobVal(Knob::Y) / 4095.f);
            
            if (fabs(new_bpm - bpm) > 0.5f || fabs(new_swing - swing) > 0.01f || fabs(new_vol - master_volume) > 0.01f) {
                bpm = new_bpm;
                swing = new_swing;
                master_volume = new_vol;
                update_clock_params();
            }
        }
        
        // Handle Trigger/Clock inputs
        bool p1 = PulseIn1(); // External clock trigger
        bool p2 = PulseIn2(); // Reset trigger
        
        bool advance_step = false;
        
        if (p2 && !last_pulse2) {
            current_step = 0;
            clock_accumulator = 0.0;
            trigger_step(current_step);
        }
        last_pulse2 = p2;
        
        if (Connected(Input::Pulse1)) {
            // External clock mode
            if (p1 && !last_pulse1) {
                current_step = (current_step + 1) % 16;
                trigger_step(current_step);
            }
            last_pulse1 = p1;
        } else {
            // Internal clock mode
            double current_step_dur = step_duration_samples;
            if (current_step % 2 == 0) {
                current_step_dur *= (1.0 + swing);
            } else {
                current_step_dur *= (1.0 - swing);
            }
            
            clock_accumulator += 1.0;
            if (clock_accumulator >= current_step_dur) {
                clock_accumulator -= current_step_dur;
                current_step = (current_step + 1) % 16;
                trigger_step(current_step);
            }
        }
        
        // Generate stereo mix from active sampler voices
        int32_t mix_l = 0;
        int32_t mix_r = 0;
        
        for (int i = 0; i < NUM_VOICES; ++i) {
            int16_t s = voices[i].next_sample();
            mix_l += s;
            mix_r += s;
        }
        
        mix_l = (int32_t)(mix_l * master_volume);
        mix_r = (int32_t)(mix_r * master_volume);
        
        // Output audio scaled to 12-bit signed (-2048..2047)
        int16_t out_l = std::clamp(mix_l / 16, -2048, 2047);
        int16_t out_r = std::clamp(mix_r / 16, -2048, 2047);
        
        AudioOut1(out_l);
        AudioOut2(out_r);
        
        // Echo current step status to CV out
        CVOut1((int16_t)((current_step / 15.f) * 4095.f - 2048.f));
        CVOut2(Connected(Input::Pulse1) ? (p1 ? 2047 : -2048) : (int16_t)(sinf(2.f * M_PI * clock_accumulator / step_duration_samples) * 2047.f));
        
        // Set Pulse Out triggers
        PulseOut1(patterns[0][current_step] && (clock_accumulator < 1000.0)); // Kick trigger out
        PulseOut2(patterns[1][current_step] && (clock_accumulator < 1000.0)); // Snare trigger out
        
        // LEDs decay/light up on trigger
        for (int i = 0; i < 6; ++i) {
            LedBrightness(i, (int)(led_decays[i] * 4095.f));
            led_decays[i] = std::max(0.f, led_decays[i] - 0.0002f); // slow decay
        }
    }

private:
    static std::string get_resource_dir() {
#ifdef _WIN32
        char path[MAX_PATH];
        HMODULE hm = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&get_resource_dir, &hm)) {
            GetModuleFileNameA(hm, path, sizeof(path));
            std::string path_str(path);
            for (char &c : path_str) {
                if (c == '\\') c = '/';
            }
            size_t pos = path_str.find_last_of('/');
            if (pos != std::string::npos) {
                std::string dir = path_str.substr(0, pos);
                pos = dir.find_last_of('/');
                if (pos != std::string::npos) {
                    std::string res = dir.substr(0, pos + 1);
                    if (res.length() >= 4 && res.substr(res.length() - 4) == "res/") {
                        return res;
                    }
                    return res + "res/";
                }
            }
        }
#else
        Dl_info info;
        if (dladdr((void*)&get_resource_dir, &info)) {
            std::string path(info.dli_fname);
            size_t pos = path.find_last_of('/');
            if (pos != std::string::npos) {
                std::string dir = path.substr(0, pos);
                pos = dir.find_last_of('/');
                if (pos != std::string::npos) {
                    std::string res = dir.substr(0, pos + 1);
                    if (res.length() >= 4 && res.substr(res.length() - 4) == "res/") {
                        return res;
                    }
                    return res + "res/";
                }
            }
        }
#endif
        return "./res/";
    }

    bool load_wav_file(const std::string& path, WavData& wav) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        char header[12];
        file.read(header, 12);
        if (file.gcount() < 12) return false;
        if (strncmp(header, "RIFF", 4) != 0 || strncmp(header + 8, "WAVE", 4) != 0) return false;

        while (file.good()) {
            char chunk_id[4];
            uint32_t chunk_size = 0;
            file.read(chunk_id, 4);
            if (file.gcount() < 4) break;
            file.read((char*)&chunk_size, 4);
            if (file.gcount() < 4) break;

            if (strncmp(chunk_id, "fmt ", 4) == 0) {
                std::vector<uint8_t> fmt_buf(chunk_size);
                file.read((char*)fmt_buf.data(), chunk_size);
                wav.channels = fmt_buf[2] | (fmt_buf[3] << 8);
                wav.sample_rate = fmt_buf[4] | (fmt_buf[5] << 8) | (fmt_buf[6] << 16) | (fmt_buf[7] << 24);
                wav.block_align = fmt_buf[12] | (fmt_buf[13] << 8);
            } else if (strncmp(chunk_id, "data", 4) == 0) {
                size_t num_bytes = chunk_size;
                std::vector<uint8_t> raw_data(num_bytes);
                file.read((char*)raw_data.data(), num_bytes);
                
                size_t num_samples = num_bytes / 2;
                wav.samples.resize(num_samples);
                for (size_t i = 0; i < num_samples; ++i) {
                    wav.samples[i] = (int16_t)(raw_data[i*2] | (raw_data[i*2+1] << 8));
                }
                return true;
            } else {
                file.seekg(chunk_size, std::ios::cur);
            }
        }
        return false;
    }
};

extern "C" {
    void set_thread_globals(CardGlobals* inst) {
        t_instance = inst;
        if (inst) {
            if (!inst->card_ptr && ComputerCard::thisptr) {
                inst->card_ptr = ComputerCard::thisptr;
            }
            ComputerCard::thisptr = inst->card_ptr;
        }
    }
    void set_core1_thread(bool is_core1) {
        is_core1_thread = is_core1;
    }
    void run_card() {
        is_core1_thread = false;
        try {
            CompulideanCard card;
            ComputerCard::thisptr = &card;
            if (t_instance) t_instance->card_ptr = &card;
            
            while (!g_cancellation_requested.load(std::memory_order_relaxed)) {
                card.ProcessSample();
                // Simulating sample tick clock
                // In our VCV plugin, processSample is called by the DSP engine, but in standalone test runner, 
                // we loop and sleep, or run_card is a loop calling process sample.
                // However, the test runner just calls run_card or expects it to yield.
                // Let's add a small yield to prevent 100% CPU lock in standalone runner if cancellation is checked.
                std::this_thread::yield();
            }
        } catch (const ThreadExitException& e) {
            // Thread terminated safely
        }
    }
}
