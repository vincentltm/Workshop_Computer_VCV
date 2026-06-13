#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <audio.hpp>
#include <midi.hpp>
#include <dsp/ringbuffer.hpp>
#include "pico_mocks.h"
#include "ComputerCard.h"
#include "BridgeAudioPort.hpp"

// DSO-local thread-local variables definition
thread_local CardGlobals* t_instance = nullptr;
thread_local bool is_core1_thread = false;
thread_local ComputerCard* ComputerCard::thisptr = nullptr;

namespace Card_UsbAudioBridge {

// Card implementation
class UsbAudioBridge : public ComputerCard {
public:
    BridgeAudioPort* audio_port = nullptr;

    // Output tracking state
    bool lastGate = false;
    int sentNote = -1;
    int lastCCVal = -1;
    long long lastCCSendTime = 0;
    bool lastClock = false;

    // Input tracking state
    int activeNote = 255;
    int pitchBend = 8192;
    int clockPulseTimer = 0;
    float last_adc_vals[6] = {0.f};

    UsbAudioBridge() {
        if (t_instance) {
            audio_port = static_cast<BridgeAudioPort*>(t_instance->bridge_audio_port);
        }
    }

    ~UsbAudioBridge() override {}

    void ProcessSample() override {
        if (!audio_port || !audio_port->stream_active.load(std::memory_order_relaxed)) {
            // Output silence when audio stream is stopped
            for (int i = 0; i < 2; i++) {
                g_audio_out[i] = 0.f;
                g_cv_out[i] = 0.f;
                g_pulse_out[i] = false;
            }
            return;
        }

        int num_in = audio_port->num_inputs.load(std::memory_order_relaxed);
        int num_out = audio_port->num_outputs.load(std::memory_order_relaxed);

        // 1. VCV to Physical DAC: push to dac_buffer
        float dac_inputs[6];
        dac_inputs[0] = g_audio_in[0];
        dac_inputs[1] = g_audio_in[1];
        dac_inputs[2] = g_cv_in[0];
        dac_inputs[3] = g_cv_in[1];
        dac_inputs[4] = g_pulse_in[0] ? 5.f : 0.f;
        dac_inputs[5] = g_pulse_in[1] ? 5.f : 0.f;

        for (int c = 0; c < 6; c++) {
            if (c < num_out) {
                float val = dac_inputs[c] / 6.0f; // Scale -6V..+6V to -1.0..1.0
                if (val < -1.f) val = -1.f;
                if (val > 1.f) val = 1.f;
                audio_port->dac_buffer.push(val);
            }
        }

        // 2. Physical ADC to VCV: pop from adc_buffer
        for (int c = 0; c < 6; c++) {
            if (c < num_in) {
                if (!audio_port->adc_buffer.empty()) {
                    last_adc_vals[c] = audio_port->adc_buffer.shift() * 6.0f; // Scale -1.0..1.0 to -6V..+6V
                }
                float val = last_adc_vals[c];
                if (c == 0) g_audio_out[0] = val;
                else if (c == 1) g_audio_out[1] = val;
                else if (c == 2) g_cv_out[0] = val;
                else if (c == 3) g_cv_out[1] = val;
                else if (c == 4) g_pulse_out[0] = (val > 2.5f);
                else if (c == 5) g_pulse_out[1] = (val > 2.5f);
            }
        }
    }

    long long get_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    void processFallbackMidiInput() {
        if (!audio_port) return;
        int num_in = audio_port->num_inputs.load(std::memory_order_relaxed);
        
        uint8_t packet[4];
        while (g_midi_rx_packet_queue.pop(packet)) {
            uint8_t status = packet[1];
            uint8_t cmd = status & 0xF0;
            uint8_t ch = status & 0x0F;
            
            // Only process Channel 1 (ch == 0) or system messages
            if (ch != 0 && status < 0xF0) continue;

            if (status == 0xF8) { // MIDI Clock
                if (5 >= num_in) {
                    g_pulse_out[1] = true;
                    clockPulseTimer = 10; // 10 background ticks (approx 20ms) high
                }
            }
            else if (cmd == 0x90 && packet[3] > 0) { // Note On
                uint8_t note = packet[2];
                activeNote = note;
                if (2 >= num_in) {
                    float bend = ((float)pitchBend - 8192.f) / 8192.f * (2.f / 12.f);
                    g_cv_out[0] = (float)(note - 60) / 12.f + bend;
                }
                if (4 >= num_in) {
                    g_pulse_out[0] = true;
                }
            }
            else if (cmd == 0x80 || (cmd == 0x90 && packet[3] == 0)) { // Note Off
                uint8_t note = packet[2];
                if (note == activeNote) {
                    if (4 >= num_in) {
                        g_pulse_out[0] = false;
                    }
                }
            }
            else if (cmd == 0xE0) { // Pitch Bend
                uint16_t pb = packet[2] | (packet[3] << 7);
                pitchBend = pb;
                if (2 >= num_in && activeNote != 255) {
                    float bend = ((float)pitchBend - 8192.f) / 8192.f * (2.f / 12.f);
                    g_cv_out[0] = (float)(activeNote - 60) / 12.f + bend;
                }
            }
            else if (cmd == 0xB0) { // CC
                uint8_t ccNum = packet[2];
                uint8_t ccVal = packet[3];
                if (ccNum == 4) { // CC 4
                    if (3 >= num_in) {
                        g_cv_out[1] = ((float)ccVal / 127.f) * 12.f - 6.f;
                    }
                }
            }
        }
    }

    void processFallbackMidiOutput() {
        if (!audio_port) return;
        int num_out = audio_port->num_outputs.load(std::memory_order_relaxed);

        // 1. CV 1 & Pulse 1 (channel 2 & 4) -> MIDI Note
        if (4 >= num_out) {
            bool currentGate = g_pulse_in[0];
            if (currentGate != lastGate) {
                lastGate = currentGate;
                if (currentGate) {
                    float pitchVoltage = g_cv_in[0];
                    int note = std::clamp(60 + (int)std::round(pitchVoltage * 12.f), 0, 127);
                    sentNote = note;
                    
                    uint8_t msg[3] = {0x90, (uint8_t)note, 127};
                    g_midi_tx_byte_queue.push(msg, 3);
                } else {
                    if (sentNote != -1) {
                        uint8_t msg[3] = {0x80, (uint8_t)sentNote, 0};
                        g_midi_tx_byte_queue.push(msg, 3);
                        sentNote = -1;
                    }
                }
            }
        }

        // 2. CV 2 (channel 3) -> MIDI CC 4
        if (3 >= num_out) {
            float currentCV2 = g_cv_in[1];
            int ccVal = std::clamp((int)std::round(((currentCV2 + 6.f) / 12.f) * 127.f), 0, 127);
            if (ccVal != lastCCVal) {
                long long now = get_time_ms();
                if (now - lastCCSendTime >= 10) {
                    lastCCVal = ccVal;
                    lastCCSendTime = now;
                    
                    uint8_t msg[3] = {0xB0, 4, (uint8_t)ccVal};
                    g_midi_tx_byte_queue.push(msg, 3);
                }
            }
        }

        // 3. Pulse 2 (channel 5) -> MIDI Clock (0xF8)
        if (5 >= num_out) {
            bool currentClock = g_pulse_in[1];
            if (currentClock && !lastClock) {
                uint8_t msg[1] = {0xF8};
                g_midi_tx_byte_queue.push(msg, 1);
            }
            lastClock = currentClock;
        }
    }

    void BackgroundLoop() override {
        // Keep actual hardware sample rate synced to expected VCV rate
        if (audio_port) {
            float sr = audio_port->sample_rate.load(std::memory_order_relaxed);
            if (sr > 0.f && t_instance && t_instance->expected_sample_rate != (double)sr) {
                t_instance->expected_sample_rate = (double)sr;
            }
        }

        // Process MIDI fallback I/O
        processFallbackMidiInput();
        processFallbackMidiOutput();

        // Decrement Clock Pulse Timer
        if (clockPulseTimer > 0) {
            clockPulseTimer--;
            if (clockPulseTimer == 0) {
                g_pulse_out[1] = false;
            }
        }
    }
};

int main() {
    UsbAudioBridge bridge;
    bridge.Run();
    return 0;
}

} // namespace Card_UsbAudioBridge

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
            Card_UsbAudioBridge::main();
        } catch (const ThreadExitException& e) {}
    }
}
