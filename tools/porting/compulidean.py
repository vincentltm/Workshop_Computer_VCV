import os
import sys

def get_extra_include_dirs(card_dir_abs):
    return [
        os.path.join(card_dir_abs, "include"),
        os.path.join(card_dir_abs, "lib", "SimplyAtomic"),
        os.path.join(card_dir_abs, "lib", "LinkedList"),
        os.path.join(card_dir_abs, "lib", "Functional-Vlpp", "src"),
        os.path.join(card_dir_abs, "lib", "ArxTypeTraits"),
        os.path.join(card_dir_abs, "lib", "ArduinoHashtable"),
        os.path.join(card_dir_abs, "lib", "SimpleVector"),
        os.path.join(card_dir_abs, "lib", "uClock", "src"),
        os.path.join(card_dir_abs, "lib", "seqlib", "src"),
        os.path.join(card_dir_abs, "lib", "parameters", "src"),
        os.path.join(card_dir_abs, "lib", "midihelpers", "src"),
        os.path.join(card_dir_abs, "lib", "MIDI", "src")
    ]

def get_extra_compiler_flags():
    return [
        "-DUSE_UCLOCK",
        "-DENABLE_EUCLIDIAN",
        "-DENABLE_PARAMETERS",
        "-DENABLE_CV_INPUT",
        "-DFAST_VOLTAGE_READS",
        "-DRP2040OutputWrapperClass=WorkshopOutputWrapper",
        "-DNUM_GLOBAL_DENSITY_GROUPS=2",
        "-DSAMPLESET=R808",
        "-DCALCULATE_SAMPLES_MODE=IN_MAIN_LOOP",
        "-DDEFAULT_INTERPOLATION_ENABLED=1",
        "-DENABLE_SHUFFLE",
        "-DSEQLIB_DISABLE_PATTERN_EUCLIDIAN_PARAMETERS",
        "-DCOMPENSATE_ADC_DNL",
        "-DCOMPENSATE_ADC_LPF_CV",
        "-DCOMPENSATE_ADC_LPF_KNOB",
        "-DDEFAULT_CV_PPQN=umodular::clock::uClockClass::PPQNResolution::PPQN_4",
        "-D__FLASH__="
    ]

def post_process(content, src_rel):
    # In main.cpp, inject uClock.run() in loop to drive software timer
    if src_rel == "src/main.cpp":
        content = content.replace("void __not_in_flash_func(loop)() {", "void __not_in_flash_func(loop)() {\n  uClock.run();")
        
        # Stub out tud_task() and TinyUSB callbacks
        content = content.replace("tud_task();", "")
        content = content.replace("while(USBMIDI.read()) {}", "")
        
        # Stub out RP2040 specific repeating timer setup
        content = content.replace("add_repeating_timer_ms(5, parameter_repeating_callback, nullptr, &parameter_timer);", "")
        content = content.replace("add_repeating_timer_us(250, usb_repeating_callback, nullptr, &usb_timer);", "")
        
        # Stub out set_sys_clock_khz
        content = content.replace("set_sys_clock_khz(150000, true);", "")

    # Handle FLASHMEM macros which are teensy specific
    content = content.replace("FLASHMEM ", "")
    
    # Fix ArxTypeTraits/Functional-Vlpp std namespace references if needed
    return content
