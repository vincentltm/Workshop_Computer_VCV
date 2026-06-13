#pragma once
#include <audio.hpp>
#include <dsp/ringbuffer.hpp>
#include <atomic>

struct BridgeAudioPort : rack::audio::Port {
    std::atomic<bool> stream_active{false};
    rack::dsp::RingBuffer<float, 8192> dac_buffer;
    rack::dsp::RingBuffer<float, 8192> adc_buffer;
    float last_dac_vals[6] = {0.f};

    // Stored stats to avoid card DLL linking against rack::audio::Port member functions
    std::atomic<int> num_inputs{0};
    std::atomic<int> num_outputs{0};
    std::atomic<float> sample_rate{0.f};

    void onStartStream() override {
        stream_active = true;
        num_inputs = getNumInputs();
        num_outputs = getNumOutputs();
        sample_rate = getSampleRate();
    }

    void onStopStream() override {
        stream_active = false;
        dac_buffer.clear();
        adc_buffer.clear();
    }

    void processBuffer(const float* input, int inputStride, float* output, int outputStride, int frames) override {
        if (!stream_active.load(std::memory_order_relaxed)) return;

        int num_in = getNumInputs();
        int num_out = getNumOutputs();
        num_inputs = num_in;
        num_outputs = num_out;
        sample_rate = getSampleRate();

        for (int f = 0; f < frames; f++) {
            // Read inputs from physical ADC and push to adc_buffer
            for (int c = 0; c < 6; c++) {
                if (c < num_in) {
                    float val = (input && c < num_in) ? input[f * inputStride + c] : 0.f;
                    adc_buffer.push(val);
                }
            }

            // Pop outputs from dac_buffer and write to physical DAC
            for (int c = 0; c < 6; c++) {
                if (c < num_out) {
                    if (!dac_buffer.empty()) {
                        last_dac_vals[c] = dac_buffer.shift();
                    }
                    if (output) {
                        output[f * outputStride + c] = last_dac_vals[c];
                    }
                }
            }
        }
    }
};
