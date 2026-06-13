import re

def post_process(src_content, src_rel):
    src_content = src_content.replace(
        'bool SynthCore_GetSample(int16_t *outL, int16_t *outR) {',
        '''bool SynthCore_GetSample(int16_t *outL, int16_t *outR) {
  int count = (rbHead - rbTail + SYNTH_RING_BUF_SIZE) % SYNTH_RING_BUF_SIZE;
  if (count < 64) {
      g_synth_need_render.store(true);
      g_synth_cv.notify_one();
  }'''
    )
    src_content = src_content.replace(
        'void __attribute__((used)) GetLiveState(uint8_t *outBlob) {',
        'public:\n  void __attribute__((used)) GetLiveState(uint8_t *outBlob) {'
    )
    src_content = re.sub(
        r'class\s+MultiFx_Computer\s*\{[^}]*GetLiveState[^}]*\};',
        '/* stripped dummy class MultiFx_Computer */',
        src_content
    )
    if "exptable.c" in src_rel:
        src_content = src_content.replace('exptable', 'exptable_impl')
    if "logtable.c" in src_rel:
        src_content = src_content.replace('logtable', 'logtable_impl')
    # In VCV, SaveSettingsToFlash() calls SynthCore_Start() after flash erase/program
    # to restart core1 (which RP2040 pauses during flash writes).
    # In VCV, core1 never stops during a save, so SynthCore_Start() would try to
    # join() a live infinite-loop thread → deadlock → crash.
    src_content = src_content.replace(
        'restore_interrupts(interrupts);\n  SynthCore_Start();\n}',
        'restore_interrupts(interrupts);\n  // SynthCore_Start() suppressed in VCV: core1 never stops during flash save\n}'
    )
    # Fix SynthCore_ScanFlashSamples: cap the bounds check to our 2MB flash buffer
    # The original code allows up to 16MB past the flash base → segfault on host
    src_content = src_content.replace(
        '    if ((uintptr_t)ptr > ((XIP_BASE + 0x00180000) + 16 * 1024 * 1024))\n      break;',
        '    if ((uintptr_t)ptr >= (XIP_BASE + PICO_FLASH_SIZE_BYTES))\n      break;'
    )
    # Also fix the per-sample safety check that also uses 16MB
    src_content = src_content.replace(
        'numSamples > (16 * 1024 * 1024)',
        'numSamples > PICO_FLASH_SIZE_BYTES'
    )
    return src_content

def get_extra_definitions():
    return """    extern const int16_t exptable_impl[];
    extern const int16_t logtable_impl[];
    const int16_t * exptable = exptable_impl;
    const int16_t * logtable = logtable_impl;

    struct BootRomInfoType;
    float fsqrt(float a) { return ::sqrtf(a); }
    float fcos(float a) { return ::cosf(a); }
    float fsin(float a) { return ::sinf(a); }
    float ftan(float a) { return ::tanf(a); }
    float fexp(float a) { return ::expf(a); }
    float fln(float a) { return ::logf(a); }
    int32_t float2int(float a) { return (int32_t)a; }
    float int2float(int32_t a) { return (float)a; }
    void initFloatFunctions() {}
    void * getRomData(char c1,char c2) { return nullptr; }
    void getBootRomInfo(BootRomInfoType** bootRomInfo) { if (bootRomInfo) *bootRomInfo = nullptr; }
    char * getCopyright() { return (char*)"Mock"; }
    void reset_usb_boot(uint32_t gpio_activity_mask, uint32_t disable_interface_mask) {}
    void flash_range_erase(uint32_t addr, uint32_t count, uint32_t block_size, uint8_t block_cmd) {}
    void flash_range_program(uint32_t addr, const uint8_t *data, uint32_t count) {}
    float __aeabi_fadd(float a,float b) { return a + b; }
    float __aeabi_fsub(float a,float b) { return a - b; }
    float __aeabi_frsub(float a,float b) { return b - a; }
    float __aeabi_fmul(float a,float b) { return a * b; }
    float __aeabi_fdiv(float a,float b) { return a / b; }
"""
