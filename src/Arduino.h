#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <string>

using String = std::string;

template <typename T>
inline T map(T x, T in_min, T in_max, T out_min, T out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

#define SDA 4
#define SCL 5

typedef bool boolean;
typedef uint8_t byte;

// Mocks for Arduino timing
inline uint64_t micros() {
    extern uint64_t time_us_64();
    return time_us_64();
}
inline uint32_t millis() {
    extern uint32_t to_ms_since_boot(uint64_t t);
    extern uint64_t time_us_64();
    return to_ms_since_boot(time_us_64());
}
inline void delay(uint32_t ms) {
    extern void sleep_ms(uint32_t ms);
    sleep_ms(ms);
}
inline void delayMicroseconds(uint32_t us) {
    extern void sleep_us(uint64_t us);
    sleep_us(us);
}

// F() macro and PROGMEM mock
#ifndef F
#define F(x) x
#endif
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef FLASHMEM
#define FLASHMEM
#endif

// Serial Mock
class SerialMock {
public:
    operator bool() { return true; }
    void begin(uint32_t baud) {}
    void setTimeout(uint32_t ms) {}
    void print(const char* s) {
        printf("%s", s);
    }
    void print(int val) {
        printf("%d", val);
    }
    void print(float val) {
        printf("%f", val);
    }
    void println(const char* s = "") {
        printf("%s\n", s);
    }
    void println(int val) {
        printf("%d\n", val);
    }
    void println(float val) {
        printf("%f\n", val);
    }
    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    void flush() {}
    int read() { return -1; }
    int available() { return 0; }
};

static SerialMock Serial;

// TinyUSB / MIDI stubs
inline void setup_usb() {}
inline void setup_midi() {}

#endif

