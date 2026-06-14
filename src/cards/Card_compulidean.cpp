#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <thread>
#include <cmath>

#ifndef _WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include "pico_mocks.h"
#include "ComputerCard.h"

thread_local CardGlobals* t_instance = nullptr;

// Sample names and expected filenames for 16 tracks:
// BD2550.WAV, RS.WAV, CP.WAV, SD0010.WAV, CY1000.WAV, MA.WAV, HT25.WAV, LT25.WAV,
// OH00.WAV, OH25.WAV, CH.WAV, CY7525.WAV, CY1025.WAV, CY0025.WAV, CB.WAV, CY0000.WAV

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
    
    int16_t next_sample(float pitch_mod) {
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
        
        pos += pitch_mod; // play speed modulated by pitch_mod
        return (int16_t)(out * volume);
    }
};

class LCG {
private:
    uint32_t state = 1;
public:
    void seed(uint32_t s) {
        state = s;
    }
    // Return a random number between min and max (exclusive of max)
    int random(int min, int max) {
        if (min >= max) return min;
        state = state * 1103515245 + 12345;
        return min + (int)(state % (max - min));
    }
    int random(int max) {
        return random(0, max);
    }
};

struct Track {
    int steps = 16;
    int pulses = 0;
    int rotation = 0;
    int duration = 2;
    
    int default_steps = 16;
    int default_pulses = 0;
    int default_rotation = 0;
    int default_duration = 2;
    
    int group = 0; // 0 or 1
    bool pattern[32] = {false};
};

class CompulideanCard : public ComputerCard {
public:
    static constexpr int NUM_VOICES = 16;
    
    WavData drum_wavs[NUM_VOICES];
    SamplerVoice voices[NUM_VOICES];
    Track tracks[NUM_VOICES];
    
    // Seed and phrase state
    uint32_t base_seed = 12345;
    uint32_t phrase_count = 0;
    
    // Clock variables
    double clock_accumulator = 0.0;
    int current_step = 0;
    uint32_t samples_since_clock = 0;
    
    bool last_pulse1 = false;
    bool last_pulse2 = false;
    
    // Gate triggers
    bool kick_triggered = false;
    bool snare_triggered = false;
    bool closed_hat_triggered = false;
    bool open_hat_triggered = false;
    
    float led_brightness[6] = {0.f};
    
    float last_density_0 = -1.0f;
    float last_density_1 = -1.0f;
    
    CompulideanCard() {
        std::string res_dir = get_resource_dir();
        std::cout << "[CompulideanCard] Loading WAV assets from: " << res_dir << "compulidean/" << std::endl;
        
        std::string files[NUM_VOICES] = {
            "BD2550.WAV",   // Track 0: Kick
            "RS.WAV",       // Track 1: Stick
            "CP.WAV",       // Track 2: Clap
            "SD0010.WAV",   // Track 3: Snare
            "CY1000.WAV",   // Track 4: Cymbal 1
            "MA.WAV",       // Track 5: Tamb
            "HT25.WAV",     // Track 6: HiTom
            "LT25.WAV",     // Track 7: LoTom
            "OH00.WAV",     // Track 8: PHH
            "OH25.WAV",     // Track 9: OHH
            "CH.WAV",       // Track 10: CHH
            "CY7525.WAV",   // Track 11: Cymbal 2
            "CY1025.WAV",   // Track 12: Splash
            "CY0025.WAV",   // Track 13: Vibra
            "CB.WAV",       // Track 14: Ride Bell
            "CY0000.WAV"    // Track 15: Ride Cymbal
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
        
        init_tracks();
        recalculate_patterns();
    }
    
    void init_tracks() {
        // Track settings: { default_steps, default_pulses, default_rotation, default_duration, group }
        // First 8 tracks are in group 0. Last 8 tracks are in group 1.
        
        // Group 0:
        tracks[0] = { 16, 4, 1, 2, 16, 4, 1, 2, 0 }; // Kick
        tracks[1] = { 16, 5, 1, 2, 16, 5, 1, 2, 0 }; // Stick
        tracks[2] = { 16, 2, 5, 2, 16, 2, 5, 2, 0 }; // Clap
        tracks[3] = { 16, 3, 5, 2, 16, 3, 5, 2, 0 }; // Snare
        tracks[4] = { 32, 1, 1, 2, 32, 1, 1, 2, 0 }; // Cymbal 1
        tracks[5] = { 16, 7, 1, 2, 16, 7, 1, 2, 0 }; // Tamb
        tracks[6] = { 16, 9, 1, 2, 16, 9, 1, 2, 0 }; // HiTom
        tracks[7] = { 4,  2, 3, 2, 4,  2, 3, 2, 0 }; // LoTom
        
        // Group 1:
        tracks[8]  = { 8,  2, 3, 2, 8,  2, 3, 2, 1 }; // PHH
        tracks[9]  = { 16, 4, 3, 2, 16, 4, 3, 2, 1 }; // OHH
        tracks[10] = { 16, 16,1, 2, 16, 16,1, 2, 1 }; // CHH
        tracks[11] = { 32, 1, 1, 8, 32, 1, 1, 8, 1 }; // Cymbal 2
        tracks[12] = { 32, 1, 5, 8, 32, 1, 5, 8, 1 }; // Splash
        tracks[13] = { 32, 1, 9, 8, 32, 1, 9, 8, 1 }; // Vibra
        tracks[14] = { 32, 1, 13,8, 32, 1, 13,8, 1 }; // Ride Bell
        tracks[15] = { 32, 5, 13,8, 32, 5, 13,8, 1 }; // Ride Cymbal
    }
    
    void reset_patterns() {
        for (int i = 0; i < NUM_VOICES; ++i) {
            tracks[i].steps = tracks[i].default_steps;
            tracks[i].pulses = tracks[i].default_pulses;
            tracks[i].rotation = tracks[i].default_rotation;
        }
    }
    
    void generate_euclidean(Track& track, float density_val) {
        std::fill(track.pattern, track.pattern + 32, false);
        
        if (track.steps <= 0) return;
        int steps = std::clamp(track.steps, 1, 32);
        
        float multiplier = 1.5f * density_val;
        int temp_pulses = (int)(0.5f + ((float)track.pulses) * multiplier);
        temp_pulses = std::clamp(temp_pulses, 0, steps);
        
        int bucket = 0;
        for (int i = 0; i < steps; i++) {
            int new_i = (track.rotation + i) % steps;
            bucket += temp_pulses;
            if (bucket >= steps) {
                bucket -= steps;
                track.pattern[new_i] = true;
            } else {
                track.pattern[new_i] = false;
            }
        }
    }
    
    void recalculate_patterns() {
        float density_0 = KnobVal(Knob::Main) + (Connected(Input::CV1) ? (g_cv_in[0] / 5.0f) : 0.0f);
        float density_1 = KnobVal(Knob::X) + (Connected(Input::CV2) ? (g_cv_in[1] / 5.0f) : 0.0f);
        
        density_0 = std::clamp(density_0, 0.0f, 1.5f);
        density_1 = std::clamp(density_1, 0.0f, 1.5f);
        
        for (int i = 0; i < NUM_VOICES; ++i) {
            float d = (tracks[i].group == 0) ? density_0 : density_1;
            generate_euclidean(tracks[i], d);
        }
    }
    
    void check_density_changes() {
        float density_0 = KnobVal(Knob::Main) + (Connected(Input::CV1) ? (g_cv_in[0] / 5.0f) : 0.0f);
        float density_1 = KnobVal(Knob::X) + (Connected(Input::CV2) ? (g_cv_in[1] / 5.0f) : 0.0f);
        
        density_0 = std::clamp(density_0, 0.0f, 1.5f);
        density_1 = std::clamp(density_1, 0.0f, 1.5f);
        
        if (std::abs(density_0 - last_density_0) > 0.01f || std::abs(density_1 - last_density_1) > 0.01f) {
            last_density_0 = density_0;
            last_density_1 = density_1;
            recalculate_patterns();
        }
    }
    
    virtual void ProcessSample() override {
        Switch sw = SwitchVal();
        
        check_density_changes();
        
        bool p1 = PulseIn1();
        bool p2 = PulseIn2();
        
        bool advance_step = false;
        
        if (p2 && !last_pulse2) {
            current_step = 0;
            clock_accumulator = 0.0;
            samples_since_clock = 0;
            advance_step = true;
        }
        last_pulse2 = p2;
        
        if (Connected(Input::Pulse1)) {
            if (p1 && !last_pulse1) {
                current_step = (current_step + 1) % 64;
                samples_since_clock = 0;
                advance_step = true;
            }
            last_pulse1 = p1;
        } else {
            float knob_y = KnobVal(Knob::Y);
            float bpm = 40.0f + knob_y * 200.0f; // 40 to 240 BPM
            double step_duration_samples = (60.0 / bpm) / 4.0 * 48000.0;
            
            clock_accumulator += 1.0;
            if (clock_accumulator >= step_duration_samples) {
                clock_accumulator -= step_duration_samples;
                current_step = (current_step + 1) % 64;
                samples_since_clock = 0;
                advance_step = true;
            }
        }
        
        if (advance_step) {
            kick_triggered = false;
            snare_triggered = false;
            closed_hat_triggered = false;
            open_hat_triggered = false;
            
            // Phrase mutations
            if (current_step == 0) {
                if (sw != Switch::Down) {
                    phrase_count++;
                    uint32_t current_seed = base_seed + phrase_count;
                    
                    reset_patterns();
                    
                    LCG lcg;
                    lcg.seed(current_seed);
                    int num_mutations = lcg.random(1, 5);
                    for (int m = 0; m < num_mutations; ++m) {
                        int ran = lcg.random(0, 16);
                        lcg.seed(current_seed + ran);
                        
                        int r = lcg.random(0, 100);
                        if (r > 50) {
                            if (r > 75) tracks[ran].pulses += 1;
                            else        tracks[ran].pulses -= 1;
                        } else if (r < 25) {
                            tracks[ran].rotation += 1;
                        } else if (r > 25) {
                            tracks[ran].pulses *= 2;
                        } else {
                            tracks[ran].pulses /= 2;
                        }
                        if (tracks[ran].pulses >= tracks[ran].steps || tracks[ran].pulses <= 0) {
                            tracks[ran].pulses = 1;
                        }
                    }
                    
                    recalculate_patterns();
                }
            }
            
            // Bar fills
            if (current_step == 48) {
                if (sw != Switch::Down) {
                    uint32_t current_seed = base_seed + phrase_count;
                    LCG lcg_fill;
                    lcg_fill.seed(current_seed + 999);
                    
                    for (int i = 0; i < 3; ++i) {
                        int ran = lcg_fill.random(0, 16);
                        tracks[ran].rotation += 2;
                    }
                    for (int i = 0; i < 3; ++i) {
                        int ran = lcg_fill.random(0, 16);
                        tracks[ran].rotation *= 2;
                        if (tracks[ran].pulses > tracks[ran].steps) {
                            tracks[ran].pulses /= 8;
                        }
                        if (tracks[ran].pulses <= 0) tracks[ran].pulses = 1;
                    }
                    
                    recalculate_patterns();
                }
            }
            
            // Trigger active tracks
            for (int i = 0; i < NUM_VOICES; ++i) {
                int track_step = current_step % tracks[i].steps;
                if (tracks[i].pattern[track_step]) {
                    voices[i].trigger(1.0f);
                    led_brightness[i % 6] = 1.0f;
                    
                    if (i == 0) kick_triggered = true;
                    if (i == 3) snare_triggered = true;
                    if (i == 10) closed_hat_triggered = true;
                    if (i == 9) open_hat_triggered = true;
                }
            }
        }
        
        float pitch_mod = 1.0f;
        if (Connected(Input::Audio2)) {
            pitch_mod = 1.0f + (g_audio_in[1] / 5.0f);
            pitch_mod = std::clamp(pitch_mod, 0.1f, 4.0f);
        }
        
        int32_t mix_l = 0;
        int32_t mix_r = 0;
        
        for (int i = 0; i < NUM_VOICES; ++i) {
            int16_t s = voices[i].next_sample(pitch_mod);
            mix_l += s;
            mix_r += s;
        }
        
        float vol_scale = (sw == Switch::Up) ? 0.0f : 0.8f;
        mix_l = (int32_t)(mix_l * vol_scale);
        mix_r = (int32_t)(mix_r * vol_scale);
        
        int16_t out_l = std::clamp(mix_l / 16, -2048, 2047);
        int16_t out_r = std::clamp(mix_r / 16, -2048, 2047);
        
        AudioOut1(out_l);
        AudioOut2(out_r);
        
        samples_since_clock++;
        bool trigger_active = (samples_since_clock < 1000);
        
        if (!trigger_active) {
            kick_triggered = false;
            snare_triggered = false;
            closed_hat_triggered = false;
            open_hat_triggered = false;
        }
        
        bool mute_active = (sw == Switch::Up);
        PulseOut1(kick_triggered && trigger_active && !mute_active);
        PulseOut2(snare_triggered && trigger_active && !mute_active);
        CVOut1((closed_hat_triggered && trigger_active && !mute_active) ? 2047 : -2048);
        CVOut2((open_hat_triggered && trigger_active && !mute_active) ? 2047 : -2048);
        
        for (int i = 0; i < 6; ++i) {
            LedBrightness(i, (int)(led_brightness[i] * 4095.f));
            led_brightness[i] = std::max(0.f, led_brightness[i] - 0.0001f);
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
                std::this_thread::yield();
            }
        } catch (const ThreadExitException& e) {
            // Thread terminated safely
        }
    }
}
