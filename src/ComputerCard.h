#ifndef COMPUTERCARD_H
#define COMPUTERCARD_H

#pragma once

#include "pico_mocks.h"
#include <stdint.h>

// Global shared variables are now per-instance, redirected via macros in pico_mocks.h.
// These externs exist only for legacy compatibility — actual storage is in CardGlobals.

class ComputerCard {
public:
    enum Knob { Main = 0, X = 1, Y = 2 };
    enum Switch { Down = 0, Middle = 1, Up = 2 };
    enum Input { Audio1 = 0, Audio2 = 1, CV1 = 2, CV2 = 3, Pulse1 = 4, Pulse2 = 5 };
    enum HardwareVersion_t { Proto1 = 0x2a, Proto2_Rev1 = 0x30, Rev1_1 = 0x0C, Unknown = 0xFF };
    enum USBPowerState_t { DFP, UFP, Unsupported };

    static thread_local ComputerCard* thisptr;
    bool useNormProbe = false;
    bool pulse[2] = {false, false};
    bool last_pulse[2] = {false, false};
    volatile int32_t cv[2] = {0, 0};

    ComputerCard() {
        thisptr = this;
        if (t_instance) {
            t_instance->card_ptr = this;
#ifdef COMPUTERCARD_SAMPLE_RATE_DIV
            adc_set_clkdiv(124 * COMPUTERCARD_SAMPLE_RATE_DIV + COMPUTERCARD_SAMPLE_RATE_DIV - 1);
#else
            adc_set_clkdiv(124);
#endif
        }
    }

    virtual ~ComputerCard() {
        if (thisptr == this) {
            thisptr = nullptr;
        }
        if (t_instance && t_instance->card_ptr == this) {
            t_instance->card_ptr = nullptr;
        }
    }

    void Run() {
        thisptr = this;
        if (t_instance) {
            t_instance->card_ptr = this;
            t_instance->g_dsp_ready = true;
        }
        while (!g_cancellation_requested.load(std::memory_order_relaxed)) {
            BackgroundLoop();
            sleep_ms(2);
        }
    }

    void EnableNormalisationProbe() { useNormProbe = true; }
    
    static void RegisterCore1BackgroundHook(void (*hook)()) { (void)hook; }

    static ComputerCard* ThisPtr() { return thisptr; }

    void Abort() {
        g_cancellation_requested = true;
    }

    uint16_t CRCencode(const uint8_t *data, int length) {
        // Simple CRC mock
        uint16_t crc = 0xFFFF;
        for (int i = 0; i < length; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }

    // Methods called by DSP thread (Core 1)
    virtual void ProcessSample() = 0;
    virtual void BackgroundLoop() {}

    void update_inputs() {
        // Update pulse states for edge detection
        last_pulse[0] = pulse[0];
        last_pulse[1] = pulse[1];
        pulse[0] = g_pulse_in[0];
        pulse[1] = g_pulse_in[1];
        cv[0] = CVIn(0);
        cv[1] = CVIn(1);
    }

public:
    // Knobs (returns 0-4095)
    int32_t KnobVal(Knob ind) {
        return (int32_t)(g_knobs[ind] * 4095.f);
    }

    // Switch state
    Switch SwitchVal() {
        return (Switch)g_switch;
    }

    bool SwitchChanged() {
        static Switch last_sw = Middle;
        Switch current = (Switch)g_switch;
        bool changed = (current != last_sw);
        last_sw = current;
        return changed;
    }

    // Audio Ins & Outs (±6V range remapped to signed 12-bit range -2048 to 2047)
    int16_t AudioIn(int i) {
        if (i == 0 && !g_input_connected[0]) return 0;
        if (i == 1 && !g_input_connected[1]) return 0;
        float v = g_audio_in[i];
        if (v < -6.f) v = -6.f;
        if (v > 6.f) v = 6.f;
        return (int16_t)(v * (2047.f / 6.f));
    }

    int16_t AudioIn1() { return AudioIn(0); }
    int16_t AudioIn2() { return AudioIn(1); }

    void AudioOut(int i, int16_t val) {
        if (val < -2048) val = -2048;
        if (val > 2047) val = 2047;
        g_audio_out[i] = val * (6.f / 2048.f);
    }

    void AudioOut1(int16_t val) { AudioOut(0, val); }
    void AudioOut2(int16_t val) { AudioOut(1, val); }

    // CV Ins (±6V range remapped to signed 12-bit range -2048 to 2047)
    int16_t CVIn(int i) {
        if (i == 0 && !g_input_connected[2]) return 0;
        if (i == 1 && !g_input_connected[3]) return 0;
        float v = g_cv_in[i];
        if (v < -6.f) v = -6.f;
        if (v > 6.f) v = 6.f;
        return (int16_t)(v * (2047.f / 6.f));
    }

    int16_t CVIn1() { return CVIn(0); }
    int16_t CVIn2() { return CVIn(1); }

    // CV Outs (±6V output range)
    void CVOut(int i, int16_t val) {
        if (val < -2048) val = -2048;
        if (val > 2047) val = 2047;
        g_cv_out[i] = val * (6.f / 2048.f);
    }

    void CVOut1(int16_t val) { CVOut(0, val); }
    void CVOut2(int16_t val) { CVOut(1, val); }

    void CVOutPrecise(int i, int32_t val) {
        if (val < -262144) val = -262144;
        if (val > 262143) val = 262143;
        g_cv_out[i] = val * (6.f / 262144.f);
    }

    void CVOut1Precise(int32_t val) { CVOutPrecise(0, val); }
    void CVOut2Precise(int32_t val) { CVOutPrecise(1, val); }

    void CVOutMIDINote(int i, uint8_t noteNum) {
        // Standard 1V/Oct CV output: C4 (MIDI 60) = 0V
        g_cv_out[i] = (float)(noteNum - 60) / 12.f;
    }

    void CVOut1MIDINote(uint8_t noteNum) { CVOutMIDINote(0, noteNum); }
    void CVOut2MIDINote(uint8_t noteNum) { CVOutMIDINote(1, noteNum); }

    bool CVOutMillivolts(int i, int32_t millivolts) {
        g_cv_out[i] = millivolts / 1000.f;
        return (g_cv_out[i] < -6.f || g_cv_out[i] > 6.f);
    }

    bool CVOut1Millivolts(int32_t millivolts) { return CVOutMillivolts(0, millivolts); }
    bool CVOut2Millivolts(int32_t millivolts) { return CVOutMillivolts(1, millivolts); }

    // Pulse Ins
    bool PulseIn(int i) {
        if (i == 0 && !g_input_connected[4]) return false;
        if (i == 1 && !g_input_connected[5]) return false;
        return pulse[i];
    }

    bool PulseIn1() { return PulseIn(0); }
    bool PulseIn2() { return PulseIn(1); }

    bool PulseInRisingEdge(int i) { return PulseIn(i) && !last_pulse[i]; }
    bool PulseInFallingEdge(int i) { return !PulseIn(i) && last_pulse[i]; }

    bool PulseIn1RisingEdge() { return PulseInRisingEdge(0); }
    bool PulseIn1FallingEdge() { return PulseInFallingEdge(0); }
    bool PulseIn2RisingEdge() { return PulseInRisingEdge(1); }
    bool PulseIn2FallingEdge() { return PulseInFallingEdge(1); }

    // Pulse Outs (0 to 5V output)
    void PulseOut(int i, bool val) {
        g_pulse_out[i] = val;
    }

    void PulseOut1(bool val) { PulseOut(0, val); }
    void PulseOut2(bool val) { PulseOut(1, val); }

    // Connected & Disconnected check stubs
    bool Connected(Input i) {
        return g_input_connected[(int)i];
    }

    bool Disconnected(Input i) {
        return !g_input_connected[(int)i];
    }

    // LEDs
    void LedBrightness(uint32_t index, uint16_t value) {
        if (index < 6) {
            g_led_brightness[index] = value / 4095.f;
        }
    }

    void LedOn(uint32_t index, bool value = true) {
        if (index < 6) {
            g_led_brightness[index] = value ? 1.f : 0.f;
        }
    }

    void LedOff(uint32_t index) {
        if (index < 6) {
            g_led_brightness[index] = 0.f;
        }
    }

    USBPowerState_t USBPowerState() {
        return UFP;
    }

    HardwareVersion_t HardwareVersion() const {
        return Rev1_1;
    }

    uint64_t UniqueCardID() const {
        return 0xCAFEBABEDEADbeefULL;
    }

    bool CVOutsCalibrated() const {
        return true;
    }
};

struct HeapCardGuard {
    ComputerCard*& ptr;
    HeapCardGuard(ComputerCard*& p) : ptr(p) {}
    ~HeapCardGuard() {
        if (ptr) {
            delete ptr;
            ptr = nullptr;
        }
    }
};

#endif // COMPUTERCARD_H
