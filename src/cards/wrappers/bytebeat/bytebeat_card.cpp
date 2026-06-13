#include "ComputerCard.h"
#include "formulas.h"
#include "tinyexpr_bitw.h"
#include <string>
#include <string.h>

typedef uint32_t (*FormulaFunction)(uint32_t, uint8_t, uint32_t, uint32_t, uint32_t);

static FormulaFunction formulas[36] = {
    formula1, formula2, formula3, formula4, formula5,
    formula6, formula7, formula8, formula9, formula10,
    formula11, formula12, formula13, formula14, formula15,
    formula16, formula17, formula18, formula19, formula20,
    formula21, formula22, formula23, formula24, formula25,
    formula26, formula27, formula28, formula29, formula30,
    formula31, formula32, formula33, formula34, formula35,
    formula36
};

class BytebeatCard : public ComputerCard {
private:
    uint32_t t = 0;
    uint32_t w = 0;
    uint32_t w2 = 0;
    uint32_t w3 = 0;
    uint32_t w4 = 0;
    bool reversePlayback = false;
    bool last_pulse2 = false;
    bool last_momentary = false;

    // tinyexpr variables
    te_expr* expr = nullptr;
    te_expr* exprs[6] = { nullptr };
    std::string bytebeatFormula = "";
    int formulaUpdate = 0;
    int userslot = 0;
    int prev_userslot = 0;

    int t_val = 0;
    int w_val = 0;
    int x_val = 0;
    int y_val = 0;
    int z_val = 0;

    std::string serialInput = "";

    void serial_print(const std::string& str) {
        for (char c : str) {
            putchar_raw(c);
        }
    }

    void serial_println(const std::string& str) {
        serial_print(str + "\n");
    }

    void eeSave(int address, const std::string& formula) {
        int len = formula.length() + 1;
        if (len > 95) len = 95;
        
        memcpy(&g_flash_memory[address], &len, sizeof(int));
        
        char exprString[95];
        memset(exprString, 0, sizeof(exprString));
        strncpy(exprString, formula.c_str(), len - 1);
        memcpy(&g_flash_memory[address + sizeof(int)], exprString, 95);
        
        if (get_safe_instance()->save_flash_to_disk_fn) {
            get_safe_instance()->save_flash_to_disk_fn();
        }
        
        eeRead(address);
    }

    void eeRead(int address) {
        int len = 0;
        memcpy(&len, &g_flash_memory[address], sizeof(int));
        
        if (len > 0 && len <= 95) {
            char exprString[95];
            memcpy(exprString, &g_flash_memory[address + sizeof(int)], 95);
            exprString[len - 1] = '\0';
            
            serial_print(std::to_string(address) + "Read from EEPROM: " + exprString + "\n");
            
            char tchar[] = "t";
            char xchar[] = "p1";
            char ychar[] = "p2";
            char zchar[] = "p3";
            char wchar[] = "w";
            te_variable vars[] = { { tchar, &t_val }, { xchar, &x_val }, { ychar, &y_val }, { zchar, &z_val }, { wchar, &w_val } };
            
            int err = 0;
            int slot = address / 100;
            if (slot >= 0 && slot < 6) {
                if (exprs[slot]) {
                    te_free(exprs[slot]);
                }
                exprs[slot] = te_compile(exprString, vars, 5, &err);
                if (!exprs[slot]) {
                    serial_print("te_compile error at address " + std::to_string(address) + ": " + std::to_string(err) + "\n");
                }
            }
        } else {
            serial_print(std::to_string(address) + "Error: Invalid string length read from EEPROM.\n");
        }
    }

    void clearEEPROM() {
        memset(g_flash_memory, 0, 2048);
        if (get_safe_instance()->save_flash_to_disk_fn) {
            get_safe_instance()->save_flash_to_disk_fn();
        }
        for (int i = 0; i < 6; i++) {
            if (exprs[i]) {
                te_free(exprs[i]);
                exprs[i] = nullptr;
            }
        }
    }

    void process_serial_line(const std::string& line) {
        if (line.rfind("_", 0) != 0) { // does not start with "_"
            bytebeatFormula = line;
            serial_println("Formula Recieved!");
            
            if (expr) {
                te_free(expr);
                expr = nullptr;
            }
            formulaUpdate = 1;

            char tchar[] = "t";
            char xchar[] = "p1";
            char ychar[] = "p2";
            char zchar[] = "p3";
            char wchar[] = "w";
            te_variable vars[] = { { tchar, &t_val }, { xchar, &x_val }, { ychar, &y_val }, { zchar, &z_val }, { wchar, &w_val } };

            int err = 0;
            expr = te_compile(bytebeatFormula.c_str(), vars, 5, &err);
            if (!expr) {
                serial_print("error compiling: " + std::to_string(err) + "\n");
            }
        } else if (line == "_SAVE1") {
            eeSave(0, bytebeatFormula);
            serial_println("User Slot 1 Saved");
        } else if (line == "_SAVE2") {
            eeSave(100, bytebeatFormula);
            serial_println("User Slot 2 Saved");
        } else if (line == "_SAVE3") {
            eeSave(200, bytebeatFormula);
            serial_println("User Slot 3 Saved");
        } else if (line == "_SAVE4") {
            eeSave(300, bytebeatFormula);
            serial_println("User Slot 4 Saved");
        } else if (line == "_SAVE5") {
            eeSave(400, bytebeatFormula);
            serial_println("User Slot 5 Saved");
        } else if (line == "_SAVE6") {
            eeSave(500, bytebeatFormula);
            serial_println("User Slot 6 Saved");
        } else if (line == "_CLEAR") {
            clearEEPROM();
            serial_println("EEPROM Cleared");
        }
    }

public:
    BytebeatCard() : ComputerCard() {
        t = 0; w = 0; w2 = 0; w3 = 0; w4 = 0;
        reversePlayback = false; last_pulse2 = false; last_momentary = false;
        expr = nullptr;
        for (int i = 0; i < 6; i++) exprs[i] = nullptr;
    }

    ~BytebeatCard() {
        if (expr) te_free(expr);
        for (int i = 0; i < 6; i++) {
            if (exprs[i]) te_free(exprs[i]);
        }
    }

    void ProcessSample() override {
        if (g_pulse_in[1] && !last_pulse2) { reversePlayback = !reversePlayback; }
        last_pulse2 = g_pulse_in[1];
        if (g_pulse_in[0]) { t = 0; }
        t = reversePlayback ? t - 1 : t + 1;

        int knobMRead = KnobVal(Main);
        int knobXRead = KnobVal(X);
        int knobYRead = KnobVal(Y);

        int formulaIndex = (knobXRead * 35) / 4000;
        if (formulaIndex < 0) formulaIndex = 0;
        if (formulaIndex > 35) formulaIndex = 35;

        uint16_t p1_orig = (knobYRead * 255) / 4095;
        uint16_t p2_orig = 126;
        uint16_t p3_orig = 0;

        int cv1 = g_cv_in[0];
        formulaIndex = (formulaIndex + (cv1 * 35 / 4095)) % 36;
        if (formulaIndex < 0) formulaIndex += 36;

        int sr = (knobMRead * 7980 / 4095) + 20;
        int cv2 = g_cv_in[1];
        int modulated_sr = (sr * (cv2 + 2048)) / 4096;
        if (modulated_sr < 20) modulated_sr = 20;
        if (modulated_sr > 32000) modulated_sr = 32000;

        if (t_instance) { t_instance->expected_sample_rate = (double)modulated_sr; }

        // Switch Z: middle (1) or down (0) = built-in formulas mode
        //           up (2) = user/custom formula mode; also resets t on rising edge
        int switchState = 0; // default: built-in formulas
        if (g_switch == 2) {
            switchState = 1; // top/up = user mode
            if (!last_momentary) {
                t = 0;
                serial_println("Reset");
            }
        }
        last_momentary = (g_switch == 2);

        // Param modulation matching hardware
        int AUDIO1_IN = (int)(g_audio_in[0] * (2048.f / 5.0f));
        int AUDIO2_IN = (int)(g_audio_in[1] * (2048.f / 5.0f));
        uint16_t p1 = ((p1_orig * (AUDIO1_IN + 2048)) / 4096) % 255;
        uint16_t p2 = ((p2_orig * (AUDIO2_IN + 2048)) / 4096) % 255;
        uint16_t p3 = p3_orig;

        t_val = (int)t;
        x_val = p1;
        y_val = p2;
        z_val = p3;
        w_val = (int)w;

        if (switchState == 0) {
            // Built-in Formulas
            w = formulas[formulaIndex](t, w, p1, p2, p3);
            w2 = formulas[(formulaIndex + 1) % 36](t, w, p1, p2, p3);
            w3 = formulas[formulaIndex](t >> 6, w, p1, p2, p3);
            w4 = formulas[formulaIndex](t << 3, w, p1, p2, p3);

            for (int i = 0; i < 6; i++) { g_led_brightness[i] = 0.f; }
            g_led_brightness[formulaIndex % 6] = 0.2f;
            g_led_brightness[formulaIndex / 6] = 1.0f;
        } else {
            // User Formulas
            userslot = formulaIndex / 6;
            int cv1_val = g_cv_in[0];
            userslot = (((userslot * (cv1_val + 2048)) / 4096) % 6);
            if (userslot < 0) userslot += 6;

            for (int i = 0; i < 6; i++) { g_led_brightness[i] = 0.f; }
            if (userslot >= 0 && userslot < 6) {
                g_led_brightness[userslot] = 1.0f;
            }

            if (userslot != prev_userslot) {
                formulaUpdate = 0;
            }
            prev_userslot = userslot;

            if (formulaUpdate && expr) {
                w = te_eval(expr);
            } else if (userslot < 6 && exprs[userslot]) {
                w = te_eval(exprs[userslot]);
                int next_slot = (userslot + 1) % 6;
                w2 = exprs[next_slot] ? te_eval(exprs[next_slot]) : 0;
            } else {
                w = 0;
                w2 = 0;
            }
            w3 = w;
            w4 = w2;
        }

        AudioOut1(((int)w - 128) << 4);
        AudioOut2(((int)w2 - 128) << 4);

        g_cv_out[0] = ((float)((int)w3 - 128) * 16.f) * (5.f / 2048.f);
        g_cv_out[1] = ((float)((int)w4 - 128) * 16.f) * (5.f / 2048.f);

        g_pulse_out[0] = (w & 1) != 0;
        g_pulse_out[1] = ((t >> 4) & 1) != 0;
    }

    void BackgroundLoop() override {
        // Read stored formulas on startup
        eeRead(0);
        eeRead(100);
        eeRead(200);
        eeRead(300);
        eeRead(400);
        eeRead(500);

        while (!g_cancellation_requested.load(std::memory_order_relaxed)) {
            int c = getchar_timeout_us(1000);
            if (c != PICO_ERROR_TIMEOUT) {
                if (c == '\n' || c == '\r') {
                    if (!serialInput.empty()) {
                        process_serial_line(serialInput);
                        serialInput = "";
                    }
                } else if (c >= 32 && c < 127) {
                    serialInput += (char)c;
                }
            }
        }
    }
};

static BytebeatCard bytebeat_card;

int main() {
    bytebeat_card.Run();
    return 0;
}
