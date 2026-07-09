#include <dlfcn.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <cmath>
#include "pico_mocks.h"
#include "ComputerCard.h"

// Undefine the mock macros so we can access member variables of the globals block directly
#undef g_knobs
#undef g_switch
#undef g_audio_in
#undef g_cv_in
#undef g_pulse_in
#undef g_input_connected
#undef g_audio_out
#undef g_cv_out
#undef g_pulse_out
#undef g_led_brightness
#undef g_cancellation_requested
#undef g_fifo_1_to_0
#undef g_fifo_0_to_1
#undef g_core1_thread
#undef g_synth_mutex
#undef g_synth_cv
#undef g_synth_need_render
#undef g_flash_memory
#undef g_midi_rx_packet_queue
#undef g_midi_tx_byte_queue
#undef g_serial_rx_byte_queue
#undef g_serial_tx_byte_queue

// Stubs for rack namespace to satisfy dynamic linking when loading card dylibs
namespace rack {
struct Context;
namespace audio {
struct Driver;
struct Device;
struct Port {
    int inputOffset = 0;
    int outputOffset = 0;
    int maxInputs = 8;
    int maxOutputs = 8;
    int driverId = -1;
    int deviceId = -1;
    Driver* driver = nullptr;
    Device* device = nullptr;
    Context* context = nullptr;

    Port();
    virtual ~Port();
    int getNumInputs();
    int getNumOutputs();
    float getSampleRate();
    virtual void processBuffer(const float* input, int inputStride, float* output, int outputStride, int frames) {}
    virtual void processInput(const float* input, int inputStride, int frames) {}
    virtual void processOutput(float* output, int outputStride, int frames) {}
    virtual void onStartStream() {}
    virtual void onStopStream() {}
};

volatile int g_port_dummy = 0;
volatile float g_port_dummy_f = 0.f;

__attribute__((visibility("default"))) Port::Port() { g_port_dummy = 1; }
__attribute__((visibility("default"))) Port::~Port() { g_port_dummy = 2; }
__attribute__((visibility("default"))) int Port::getNumInputs() { g_port_dummy = 3; return 2; }
__attribute__((visibility("default"))) int Port::getNumOutputs() { g_port_dummy = 4; return 2; }
__attribute__((visibility("default"))) float Port::getSampleRate() { g_port_dummy_f = 44100.f; return 44100.f; }
}
}

// Define the thread_local symbols that the host defines
thread_local CardGlobals* t_instance = nullptr;
thread_local ComputerCard* ComputerCard::thisptr = nullptr;
thread_local bool is_core1_thread = false;

void test_multicore_launch_core1(void (*entry)()) {
    CardGlobals* inst = t_instance;
    if (!inst) return;
    ComputerCard* card = inst->card_ptr;
    inst->g_core1_thread_val = std::thread([entry, inst, card]() {
        t_instance = inst;
        is_core1_thread = true;
        ComputerCard::thisptr = card;
        if (inst->set_thread_globals_fn) inst->set_thread_globals_fn(inst);
        if (inst->set_core1_thread_fn) inst->set_core1_thread_fn(true);
        try { entry(); } catch (...) {}
    });
}

void* load_card_library(const char* path, CardGlobals& globals, std::thread& out_thread) {
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "Failed to load " << path << ": " << dlerror() << std::endl;
        return nullptr;
    }
    auto set_thread_globals_fn = (void(*)(CardGlobals*)) dlsym(handle, "set_thread_globals");
    auto run_card_fn = (void(*)()) dlsym(handle, "run_card");
    if (!set_thread_globals_fn || !run_card_fn) {
        dlclose(handle);
        return nullptr;
    }
    globals.set_thread_globals_fn = set_thread_globals_fn;
    set_thread_globals_fn(&globals);
    
    out_thread = std::thread([run_card_fn, set_thread_globals_fn, &globals]() {
        t_instance = &globals;
        set_thread_globals_fn(&globals);
        try { run_card_fn(); } catch (const ThreadExitException&) {}
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    return handle;
}

int main(int argc, char* argv[]) {
    // Prevent linker from dead-stripping rack::audio::Port stubs
    if (argc == 9999) {
        rack::audio::Port* volatile p = new rack::audio::Port();
        rack::audio::g_port_dummy = p->getNumInputs() + p->getNumOutputs() + (int)p->getSampleRate();
        delete p;
    }

    std::cout << "==========================================================" << std::endl;
    std::cout << "RUNNING FUNCTIONAL CARD TESTS (SEMANTIC ASSERTIONS)..." << std::endl;
    std::cout << "==========================================================" << std::endl;

    // ─── TEST 1: REVERB+ AUDIO DECAY ASSERTION ───
    {
        std::cout << "Testing Reverb+ (ID: reverb) audio decay..." << std::endl;
        CardGlobals globals;
        globals.multicore_launch_core1_fn = test_multicore_launch_core1;
        t_instance = &globals;

        std::thread t;
        void* handle = load_card_library("res/cards/libcard_reverb.dylib", globals, t);
        assert(handle != nullptr && "Failed to load reverb library");
        assert(globals.card_ptr != nullptr && "Reverb card was not instantiated");

        // Set knobs: Wet/Dry to 0.8 (high wet mix), Decay to 0.8 (long tail), Tone to 0.5 (neutral)
        globals.g_knobs[0] = 0.8f;
        globals.g_knobs[1] = 0.8f;
        globals.g_knobs[2] = 0.5f;

        // 1. Run past the startup sample delay (20000 samples) to unmute the card
        for (int i = 0; i < 22000; ++i) {
            globals.card_ptr->update_inputs();
            globals.card_ptr->ProcessSample();
        }

        // 2. Feed an impulse on left and right inputs
        globals.g_audio_in[0] = 1.0f;
        globals.g_audio_in[1] = -1.0f;
        globals.card_ptr->update_inputs();
        globals.card_ptr->ProcessSample();

        // 3. Clear input back to silence
        globals.g_audio_in[0] = 0.0f;
        globals.g_audio_in[1] = 0.0f;

        // 4. Process samples and check for decaying reverberant energy
        bool has_output_tail = false;
        for (int i = 0; i < 4000; ++i) {
            globals.card_ptr->update_inputs();
            globals.card_ptr->ProcessSample();
            float energy = std::abs(globals.g_audio_out[0]) + std::abs(globals.g_audio_out[1]);
            
            // Reverb algorithm should produce a tail that is non-zero after the impulse
            if (i > 1000 && energy > 0.01f) {
                has_output_tail = true;
            }
        }

        if (has_output_tail) {
            std::cout << " -> Reverb+: PASS (Successfully generated decaying reverb tail)" << std::endl;
        } else {
            std::cerr << " -> Reverb+: FAIL (No reverb tail detected after impulse)" << std::endl;
            return 1;
        }

        // Clean up
        globals.g_cancellation_requested_val = true;
        if (t.joinable()) t.join();
        if (globals.g_core1_thread_val.joinable()) globals.g_core1_thread_val.join();
        dlclose(handle);
    }

    // ─── TEST 2: SIMPLE MIDI PITCH & GATE ROUTING ───
    {
        std::cout << "Testing Simple MIDI (ID: simple_midi) pitch and gate translation..." << std::endl;
        CardGlobals globals;
        globals.multicore_launch_core1_fn = test_multicore_launch_core1;
        t_instance = &globals;

        std::thread t;
        void* handle = load_card_library("res/cards/libcard_simple_midi.dylib", globals, t);
        assert(handle != nullptr && "Failed to load simple_midi library");
        assert(globals.card_ptr != nullptr && "Simple MIDI card was not instantiated");

        // Send MIDI Note On (Note 60 = Middle C, Channel 1, Velocity 100)
        uint8_t note_on[4] = { 0x09, 0x90, 60, 100 };
        globals.g_midi_rx_packet_queue.push(note_on);

        // Process a few ticks so the background thread receives the packet and updates registers
        for (int i = 0; i < 50; ++i) {
            globals.card_ptr->BackgroundLoop();
            globals.card_ptr->update_inputs();
            globals.card_ptr->ProcessSample();
        }

        // Verify Gate went high
        bool gate_on = globals.g_pulse_out[0];
        // Verify Pitch CV (Note 60 is middle C, which maps to 0.0V reference)
        float pitch_cv = globals.g_cv_out[0];

        if (!gate_on) {
            std::cerr << " -> Simple MIDI: FAIL (Gate output did not go high on Note On)" << std::endl;
            return 1;
        }
        if (std::abs(pitch_cv) > 0.01f) {
            std::cerr << " -> Simple MIDI: FAIL (Pitch CV was " << pitch_cv << "V instead of 0.0V for note 60)" << std::endl;
            return 1;
        }

        // Send MIDI Note Off
        uint8_t note_off[4] = { 0x08, 0x80, 60, 0 };
        globals.g_midi_rx_packet_queue.push(note_off);

        for (int i = 0; i < 50; ++i) {
            globals.card_ptr->BackgroundLoop();
            globals.card_ptr->update_inputs();
            globals.card_ptr->ProcessSample();
        }

        // Verify Gate went low
        bool gate_off = !globals.g_pulse_out[0];

        if (!gate_off) {
            std::cerr << " -> Simple MIDI: FAIL (Gate output did not go low on Note Off)" << std::endl;
            return 1;
        }

        std::cout << " -> Simple MIDI: PASS (MIDI Note-to-CV pitch and gate translated correctly)" << std::endl;

        // Clean up
        globals.g_cancellation_requested_val = true;
        if (t.joinable()) t.join();
        if (globals.g_core1_thread_val.joinable()) globals.g_core1_thread_val.join();
        dlclose(handle);
    }

    std::cout << "==========================================================" << std::endl;
    std::cout << "ALL FUNCTIONAL TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
