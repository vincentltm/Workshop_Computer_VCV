#include <dlfcn.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "pico_mocks.h"
#include "ComputerCard.h"

// Define the thread_local symbols that the host defines
thread_local CardGlobals* t_instance = nullptr;
thread_local CardGlobals* dummy_instance_ptr = nullptr;
thread_local ComputerCard* ComputerCard::thisptr = nullptr;
thread_local bool is_core1_thread = false;

void test_multicore_launch_core1(void (*entry)()) {
    CardGlobals* inst = t_instance;
    if (!inst) return;
    ComputerCard* card = inst->card_ptr;

    if (inst->g_core1_thread_val.joinable())
        inst->g_core1_thread_val.join();

    inst->g_core1_thread_val = std::thread([entry, inst, card]() {
        t_instance = inst;
        is_core1_thread = true;
        ComputerCard::thisptr = card;
        if (inst->set_thread_globals_fn) {
            inst->set_thread_globals_fn(inst);
        }
        if (inst->set_core1_thread_fn) {
            inst->set_core1_thread_fn(true);
        }
        try { entry(); } catch (const ThreadExitException&) {}
    });
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_card_library.dylib>" << std::endl;
        return 1;
    }

    const char* dylib_path = argv[1];
    std::cout << "Testing card library: " << dylib_path << std::endl;

    CardGlobals globals;
    globals.multicore_launch_core1_fn = test_multicore_launch_core1;
    t_instance = &globals;

    void* handle = dlopen(dylib_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return 1;
    }

    auto set_thread_globals_fn = (void(*)(CardGlobals*)) dlsym(handle, "set_thread_globals");
    auto set_core1_thread_fn = (void(*)(bool)) dlsym(handle, "set_core1_thread");
    auto run_card_fn = (void(*)()) dlsym(handle, "run_card");

    if (!set_thread_globals_fn || !run_card_fn) {
        std::cerr << "Error: Essential symbols (set_thread_globals or run_card) not found in dylib." << std::endl;
        dlclose(handle);
        return 1;
    }

    // Configure the dylib to use our globals block
    globals.set_thread_globals_fn = set_thread_globals_fn;
    globals.set_core1_thread_fn = set_core1_thread_fn;
    set_thread_globals_fn(&globals);
    if (set_core1_thread_fn) {
        set_core1_thread_fn(false);
    }

    std::cout << "Starting background card thread..." << std::endl;
    std::thread t([run_card_fn, set_thread_globals_fn, &globals]() {
        t_instance = &globals;
        set_thread_globals_fn(&globals);
        try {
            run_card_fn();
        } catch (const ThreadExitException&) {
            std::cout << "Background thread exited safely." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Background thread caught std::exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Background thread caught unknown exception!" << std::endl;
        }
    });

    // Wait for the card constructor to run and set card_ptr
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (globals.card_ptr) {
        std::cout << "Card instantiated successfully. card_ptr = " << globals.card_ptr << std::endl;
        std::cout << "Executing 100000 ProcessSample calls..." << std::endl;
        
        try {
            int success_count = 0;
            for (int i = 0; i < 100000; ++i) {
                globals.card_ptr->update_inputs();
                if (!g_fifo_1_to_0.empty()) {
                    success_count++;
                }
                globals.card_ptr->ProcessSample();
            }
            std::cout << "FIFO 1->0 empty: " << g_fifo_1_to_0.empty() << std::endl;
            std::cout << "FIFO 0->1 empty: " << g_fifo_0_to_1.empty() << std::endl;
            std::cout << "Successful pop ticks: " << success_count << std::endl;
            std::cout << "Successfully executed 100000 ProcessSample calls." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "ProcessSample caught std::exception: " << e.what() << std::endl;
            globals.g_cancellation_requested_val = true;
            t.join();
            if (globals.g_core1_thread_val.joinable()) {
                globals.g_core1_thread_val.join();
            }
            dlclose(handle);
            return 1;
        } catch (...) {
            std::cerr << "ProcessSample caught unknown exception!" << std::endl;
            globals.g_cancellation_requested_val = true;
            t.join();
            if (globals.g_core1_thread_val.joinable()) {
                globals.g_core1_thread_val.join();
            }
            dlclose(handle);
            return 1;
        }
    } else {
        std::cerr << "Warning: card_ptr is still null after startup. The card might not use the standard ComputerCard base class, or lacks static/dynamic instantiation." << std::endl;
    }

    std::cout << "Stopping background thread..." << std::endl;
    globals.g_cancellation_requested_val = true;
    
    std::cout << "Joining Core 0 thread..." << std::endl;
    t.join();
    std::cout << "Core 0 thread joined." << std::endl;
    if (globals.g_core1_thread_val.joinable()) {
        std::cout << "Joining Core 1 thread..." << std::endl;
        globals.g_core1_thread_val.join();
        std::cout << "Core 1 thread joined." << std::endl;
    }
    dlclose(handle);
    std::cout << "Finished testing: " << dylib_path << " - SUCCESS!" << std::endl;
    return 0;
}
