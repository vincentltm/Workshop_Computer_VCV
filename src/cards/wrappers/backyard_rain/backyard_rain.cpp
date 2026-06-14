#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#ifndef _WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif
#include <algorithm>

const int ima_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

const int ima_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

inline int16_t decode_nibble(uint8_t nibble, int32_t& predictor, int32_t& step_index) {
    int32_t step = ima_step_table[step_index];
    int32_t diff = step >> 3;
    if (nibble & 4) diff += step;
    if (nibble & 2) diff += (step >> 1);
    if (nibble & 1) diff += (step >> 2);
    
    if (nibble & 8) predictor -= diff;
    else            predictor += diff;
    
    if (predictor > 32767) predictor = 32767;
    else if (predictor < -32768) predictor = -32768;
    
    step_index += ima_index_table[nibble & 15];
    if (step_index < 0) step_index = 0;
    else if (step_index > 88) step_index = 88;
    
    return (int16_t)predictor;
}

bool decode_adpcm_wav(const std::string& path, std::vector<int16_t>& out_left, std::vector<int16_t>& out_right, int& num_channels) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    char header[12];
    file.read(header, 12);
    if (file.gcount() < 12) return false;
    if (strncmp(header, "RIFF", 4) != 0 || strncmp(header + 8, "WAVE", 4) != 0) return false;
    
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint32_t block_align = 0;
    std::vector<uint8_t> raw_data;
    
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
            channels = fmt_buf[2] | (fmt_buf[3] << 8);
            sample_rate = fmt_buf[4] | (fmt_buf[5] << 8) | (fmt_buf[6] << 16) | (fmt_buf[7] << 24);
            block_align = fmt_buf[12] | (fmt_buf[13] << 8);
        } else if (strncmp(chunk_id, "data", 4) == 0) {
            raw_data.resize(chunk_size);
            file.read((char*)raw_data.data(), chunk_size);
            break;
        } else {
            file.seekg(chunk_size, std::ios::cur);
        }
    }
    
    if (raw_data.empty() || channels == 0 || block_align == 0) return false;
    num_channels = channels;
    
    size_t offset = 0;
    while (offset + block_align <= raw_data.size()) {
        const uint8_t* block = &raw_data[offset];
        if (channels == 2) {
            int32_t pred_l = (int16_t)(block[0] | (block[1] << 8));
            int32_t step_l = block[2];
            if (step_l > 88) step_l = 88;
            
            int32_t pred_r = (int16_t)(block[4] | (block[5] << 8));
            int32_t step_r = block[6];
            if (step_r > 88) step_r = 88;
            
            out_left.push_back(pred_l);
            out_right.push_back(pred_r);
            
            size_t block_offset = 8;
            while (block_offset + 8 <= block_align) {
                // 4 bytes Left
                for (int i = 0; i < 4; ++i) {
                    uint8_t b = block[block_offset + i];
                    out_left.push_back(decode_nibble(b & 0x0F, pred_l, step_l));
                    out_left.push_back(decode_nibble((b >> 4) & 0x0F, pred_l, step_l));
                }
                // 4 bytes Right
                for (int i = 0; i < 4; ++i) {
                    uint8_t b = block[block_offset + 4 + i];
                    out_right.push_back(decode_nibble(b & 0x0F, pred_r, step_r));
                    out_right.push_back(decode_nibble((b >> 4) & 0x0F, pred_r, step_r));
                }
                block_offset += 8;
            }
        } else {
            int32_t pred = (int16_t)(block[0] | (block[1] << 8));
            int32_t step = block[2];
            if (step > 88) step = 88;
            
            out_left.push_back(pred);
            
            size_t block_offset = 4;
            while (block_offset < block_align) {
                uint8_t b = block[block_offset];
                out_left.push_back(decode_nibble(b & 0x0F, pred, step));
                out_left.push_back(decode_nibble((b >> 4) & 0x0F, pred, step));
                block_offset++;
            }
        }
        offset += block_align;
    }
    return true;
}

class BackyardRainCard : public ComputerCard {
public:
    std::vector<int16_t> light_l, light_r;
    std::vector<int16_t> medium_l, medium_r;
    std::vector<int16_t> heavy_l, heavy_r;
    std::vector<int16_t> thunder_a;
    std::vector<int16_t> thunder_b;
    std::vector<int16_t> thunder_c;

    size_t index_light = 0;
    size_t index_medium = 277;
    size_t index_heavy = 691;
    
    size_t index_thunder = 0;
    bool thunder_is_playing = false;
    const std::vector<int16_t>* active_thunder = nullptr;

    int32_t intensity_accum = 0;
    int32_t lfo_value = 1024;
    
    int sample_counter = 0;
    int lfo_tick_counter = 0;
    
    bool last_pulse1 = false;
    bool last_sw_momentary = false;

    BackyardRainCard() {
        std::string res_dir = get_resource_dir();
        std::cout << "[BackyardRainCard] res_dir: " << res_dir << std::endl;
        
        int channels = 0;
        decode_adpcm_wav(res_dir + "backyard_rain/stereo/backyard_rain_light_loop_stereo.wav", light_l, light_r, channels);
        decode_adpcm_wav(res_dir + "backyard_rain/stereo/backyard_rain_medium_loop_stereo.wav", medium_l, medium_r, channels);
        decode_adpcm_wav(res_dir + "backyard_rain/stereo/backyard_rain_heavy_loop_stereo.wav", heavy_l, heavy_r, channels);
        decode_adpcm_wav(res_dir + "backyard_rain/backyard_thunder_a.wav", thunder_a, thunder_b, channels); // mono, goes to thunder_a
        decode_adpcm_wav(res_dir + "backyard_rain/backyard_thunder_b.wav", thunder_b, thunder_a, channels); // mono, goes to thunder_b
        decode_adpcm_wav(res_dir + "backyard_rain/backyard_thunder_c.wav", thunder_c, thunder_a, channels); // mono, goes to thunder_c

        std::cout << "[BackyardRainCard] Loaded files sizes: "
                  << "light=" << light_l.size() << ", "
                  << "medium=" << medium_l.size() << ", "
                  << "heavy=" << heavy_l.size() << ", "
                  << "thunder_a=" << thunder_a.size() << ", "
                  << "thunder_b=" << thunder_b.size() << ", "
                  << "thunder_c=" << thunder_c.size() << std::endl;
    }

    virtual void ProcessSample() override {
        update_inputs();

        // 120Hz control tick (every 400 samples at 48kHz)
        sample_counter++;
        if (sample_counter >= 400) {
            sample_counter = 0;
            lfo_tick_counter++;
            
            // LFO tick (every 16 logic ticks = 7.5Hz)
            if (lfo_tick_counter >= 16) {
                lfo_tick_counter = 0;
                lfo_value -= 1;
                if (lfo_value < -2048) {
                    lfo_value = 2047;
                }
            }

            int32_t current_lfo = (std::abs(lfo_value) - 1024) / 2;

            // Read inputs
            int32_t main_knob = KnobVal(Knob::Main) - 2048; // -2048 to 2047
            int32_t target_intensity = main_knob;
            
            if (Connected(Input::CV1)) {
                target_intensity = CVIn1() + main_knob;
            } else {
                target_intensity = current_lfo + main_knob;
            }
            if (target_intensity < -2048) target_intensity = -2048;
            if (target_intensity > 2047) target_intensity = 2047;

            // smooth_intensity filter (ACCUM_BITS = 3)
            intensity_accum = intensity_accum - (intensity_accum >> 3) + target_intensity;
            int32_t intensity = intensity_accum >> 3;

            // CV outputs
            CVOut1(intensity);
            CVOut2(current_lfo);

            // LEDs
            if (intensity > 0) {
                LedBrightness(0, (intensity / 2047.f) * 2.f * 4095.f);
            } else {
                LedBrightness(0, 0);
            }
            LedBrightness(2, (1.f - std::abs(intensity) / 2047.f) * 4095.f);
            if (intensity < 0) {
                LedBrightness(4, (std::abs(intensity) / 2047.f) * 2.f * 4095.f);
            } else {
                LedBrightness(4, 0);
            }

            // Thunder triggers
            bool current_pulse1 = PulseIn1();
            bool current_sw_momentary = (SwitchVal() == Switch::Down);
            bool trigger_thunder = false;

            if (current_pulse1 && !last_pulse1) {
                trigger_thunder = true;
            }
            if (current_sw_momentary && !last_sw_momentary) {
                trigger_thunder = true;
            }

            last_pulse1 = current_pulse1;
            last_sw_momentary = current_sw_momentary;

            if (trigger_thunder && !thunder_is_playing) {
                thunder_is_playing = true;
                index_thunder = 0;
                int r = rand() % 3;
                if (r == 0 && !thunder_a.empty()) active_thunder = &thunder_a;
                else if (r == 1 && !thunder_b.empty()) active_thunder = &thunder_b;
                else if (!thunder_c.empty()) active_thunder = &thunder_c;
                else active_thunder = nullptr;
            }
        }

        // Stereo mixing
        int32_t intensity = intensity_accum >> 3;
        
        int32_t mix_l = 0;
        int32_t mix_r = 0;

        if (!medium_l.empty() && !light_l.empty() && !heavy_l.empty()) {
            int32_t med_l = medium_l[index_medium] / 16;
            int32_t med_r = medium_r[index_medium] / 16;
            
            if (intensity >= 0) {
                int32_t hvy_l = heavy_l[index_heavy] / 16;
                int32_t hvy_r = heavy_r[index_heavy] / 16;
                mix_l = (med_l * (2047 - intensity) + hvy_l * intensity) / 2047;
                mix_r = (med_r * (2047 - intensity) + hvy_r * intensity) / 2047;
            } else {
                int32_t abs_intensity = -intensity;
                if (abs_intensity > 2047) abs_intensity = 2047;
                int32_t lgt_l = light_l[index_light] / 16;
                int32_t lgt_r = light_r[index_light] / 16;
                mix_l = (med_l * (2047 - abs_intensity) + lgt_l * abs_intensity) / 2047;
                mix_r = (med_r * (2047 - abs_intensity) + lgt_r * abs_intensity) / 2047;
            }
            
            index_light = (index_light + 1) % light_l.size();
            index_medium = (index_medium + 1) % medium_l.size();
            index_heavy = (index_heavy + 1) % heavy_l.size();
        }

        if (thunder_is_playing && active_thunder && !active_thunder->empty()) {
            int32_t thn = (*active_thunder)[index_thunder] / 16;
            mix_l += thn;
            mix_r += thn;
            index_thunder++;
            if (index_thunder >= active_thunder->size()) {
                thunder_is_playing = false;
                active_thunder = nullptr;
            }
        }

        AudioOut1(mix_l);
        AudioOut2(mix_r);
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
};

int main() {
    BackyardRainCard brc;
    brc.Run();
    return 0;
}
