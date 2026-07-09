#include <dlfcn.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <vector>
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

// Define the thread_local symbols that the host/test runner needs
thread_local CardGlobals* t_instance = nullptr;
thread_local ComputerCard* ComputerCard::thisptr = nullptr;
thread_local bool is_core1_thread = false;

void test_multicore_launch_core1(void (*entry)()) {
    CardGlobals* inst = t_instance;
    if (!inst) return;
    ComputerCard* card = inst->card_ptr;

    if (inst->g_core1_thread_val.joinable()) {
        inst->g_core1_thread_val.join();
    }

    inst->g_core1_thread_val = std::thread([entry, inst, card]() {
        t_instance = inst;
        is_core1_thread = true;
        ComputerCard::thisptr = card;
        if (inst->set_thread_globals_fn) inst->set_thread_globals_fn(inst);
        if (inst->set_core1_thread_fn) inst->set_core1_thread_fn(true);
        try { entry(); } catch (const ThreadExitException&) {}
    });
}

int main(int argc, char* argv[]) {
    // Disable stdout and stderr buffering completely so piped output is received immediately
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Prevent linker from dead-stripping rack::audio::Port stubs
    if (argc == 9999) {
        rack::audio::Port* volatile p = new rack::audio::Port();
        rack::audio::g_port_dummy = p->getNumInputs() + p->getNumOutputs() + (int)p->getSampleRate();
        delete p;
    }

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_card_library.dylib>" << std::endl;
        return 1;
    }

    const char* dylib_path = argv[1];

    CardGlobals globals;
    globals.multicore_launch_core1_fn = test_multicore_launch_core1;
    t_instance = &globals;

    // Set all inputs as connected by default for testing
    for (int i = 0; i < 6; ++i) {
        globals.g_input_connected[i] = true;
    }

    void* handle = dlopen(dylib_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return 1;
    }

    auto set_thread_globals_fn = (void(*)(CardGlobals*)) dlsym(handle, "set_thread_globals");
    auto set_core1_thread_fn = (void(*)(bool)) dlsym(handle, "set_core1_thread");
    auto run_card_fn = (void(*)()) dlsym(handle, "run_card");

    if (!set_thread_globals_fn || !run_card_fn) {
        std::cerr << "Error: Essential symbols not found in dylib." << std::endl;
        dlclose(handle);
        return 1;
    }

    globals.set_thread_globals_fn = set_thread_globals_fn;
    globals.set_core1_thread_fn = set_core1_thread_fn;
    set_thread_globals_fn(&globals);
    if (set_core1_thread_fn) {
        set_core1_thread_fn(false);
    }

    // Start background thread representing Core 0
    std::thread t([run_card_fn, set_thread_globals_fn, &globals]() {
        t_instance = &globals;
        set_thread_globals_fn(&globals);
        try {
            run_card_fn();
        } catch (const ThreadExitException&) {
            // Safe thread exit
        } catch (...) {
            // Catch all to avoid crashing the runner on exit
        }
    });

    // Wait for the card constructor to run and set card_ptr
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    if (!globals.card_ptr) {
        std::cerr << "Warning: card_ptr is still null after startup. Standard commands might not execute." << std::endl;
    }

    std::cout << "READY" << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "EXIT") {
            break;
        } else if (cmd == "SET_INPUTS") {
            float audio0, audio1, cv0, cv1;
            int pulse0, pulse1;
            if (ss >> audio0 >> audio1 >> cv0 >> cv1 >> pulse0 >> pulse1) {
                globals.g_audio_in[0] = audio0;
                globals.g_audio_in[1] = audio1;
                globals.g_cv_in[0] = cv0;
                globals.g_cv_in[1] = cv1;
                globals.g_pulse_in[0] = (pulse0 != 0);
                globals.g_pulse_in[1] = (pulse1 != 0);
                // Mark them all as connected
                for (int i = 0; i < 6; ++i) {
                    globals.g_input_connected[i] = true;
                }
                std::cout << "OK" << std::endl;
            } else {
                std::cout << "ERR_BAD_ARGS" << std::endl;
            }
        } else if (cmd == "SET_KNOBS") {
            float k0, k1, k2;
            if (ss >> k0 >> k1 >> k2) {
                globals.g_knobs[0] = k0;
                globals.g_knobs[1] = k1;
                globals.g_knobs[2] = k2;
                std::cout << "OK" << std::endl;
            } else {
                std::cout << "ERR_BAD_ARGS" << std::endl;
            }
        } else if (cmd == "SET_SWITCH") {
            int z_val;
            if (ss >> z_val) {
                globals.g_switch = z_val;
                std::cout << "OK" << std::endl;
            } else {
                std::cout << "ERR_BAD_ARGS" << std::endl;
            }
        } else if (cmd == "SEND_MIDI") {
            int b0, b1, b2, b3;
            if (ss >> b0 >> b1 >> b2 >> b3) {
                uint8_t packet[4] = { (uint8_t)b0, (uint8_t)b1, (uint8_t)b2, (uint8_t)b3 };
                globals.g_midi_rx_packet_queue.push(packet);
                std::cout << "OK" << std::endl;
            } else {
                std::cout << "ERR_BAD_ARGS" << std::endl;
            }
        } else if (cmd == "RUN_SAMPLES") {
            int num_samples;
            if (ss >> num_samples) {
                if (globals.card_ptr) {
                    for (int i = 0; i < num_samples; ++i) {
                        globals.card_ptr->update_inputs();
                        globals.card_ptr->ProcessSample();
                    }
                }
                std::cout << "OK" << std::endl;
            } else {
                std::cout << "ERR_BAD_ARGS" << std::endl;
            }
        } else if (cmd == "GET_STATE") {
            std::cout << "STATE "
                      << globals.g_audio_out[0] << " "
                      << globals.g_audio_out[1] << " "
                      << globals.g_cv_out[0] << " "
                      << globals.g_cv_out[1] << " "
                      << (globals.g_pulse_out[0] ? 1 : 0) << " "
                      << (globals.g_pulse_out[1] ? 1 : 0) << " "
                      << globals.g_led_brightness[0] << " "
                      << globals.g_led_brightness[1] << " "
                      << globals.g_led_brightness[2] << " "
                      << globals.g_led_brightness[3] << " "
                      << globals.g_led_brightness[4] << " "
                      << globals.g_led_brightness[5] << std::endl;
        } else {
            std::cout << "ERR_UNKNOWN_CMD" << std::endl;
        }
    }

    // Clean up
    globals.g_cancellation_requested_val = true;
    if (t.joinable()) {
        t.join();
    }
    if (globals.g_core1_thread_val.joinable()) {
        globals.g_core1_thread_val.join();
    }
    dlclose(handle);
    return 0;
}
