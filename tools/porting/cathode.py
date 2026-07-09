def post_process(content, src_rel):
    if src_rel == "main.cpp":
        # Define mock type and functions for PIO
        mock_pio = """
typedef void* PIO;
#define pio0 ((PIO)0)
#define pio1 ((PIO)1)
#define GPIO_FUNC_PIO0 0

struct pio_program {
    const uint16_t* instructions;
    uint8_t length;
    int origin;
};
static const uint16_t video_out_instructions[] = {0};
static const pio_program video_out_program = { video_out_instructions, 0, -1 };
struct pio_sm_config {
    uint32_t clkdiv;
    uint32_t execctrl;
    uint32_t shiftctrl;
    uint32_t pinctrl;
};
inline pio_sm_config video_out_program_get_default_config(uint) {
    pio_sm_config c = {0};
    return c;
}
inline unsigned int pio_add_program(PIO, const pio_program*) { return 0; }
inline void sm_config_set_out_pins(pio_sm_config*, unsigned int, unsigned int) {}
inline void sm_config_set_out_shift(pio_sm_config*, bool, bool, unsigned int) {}
inline void sm_config_set_clkdiv(pio_sm_config*, float) {}
inline void pio_sm_set_consecutive_pindirs(PIO, unsigned int, unsigned int, unsigned int, bool) {}
inline void pio_sm_init(PIO, unsigned int, unsigned int, const pio_sm_config*) {}
inline int pio_get_dreq(PIO, unsigned int, bool) { return 0; }
inline void pio_sm_set_enabled(PIO, unsigned int, bool) {}
"""
        # Replace the inclusion of composite.pio.h with our mock definitions
        content = content.replace('#include "composite.pio.h"', mock_pio)
        
    return content
