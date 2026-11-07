// Little Arcade port shim: the vendored PixelRoot32 engine includes <Arduino.h>
// whenever PLATFORM_NATIVE (SDL2) is not defined. We build it on ESP-IDF
// without the Arduino core and on the host without SDL2, so this header
// supplies the handful of Arduino symbols the compiled subset touches.
#pragma once

#include <cstddef>
#include <cstdint>

namespace little_arcade::pr32_port {
// Monotonic milliseconds. The engine derives its frame delta from this; the
// arcade sets it explicitly before each frame so the host and the device step
// the same way (see Pr32Host::frame).
std::uint32_t millis();
std::uint32_t micros();
void set_now_ms(std::uint32_t now_ms);
}  // namespace little_arcade::pr32_port

inline unsigned long millis() { return little_arcade::pr32_port::millis(); }
inline unsigned long micros() { return little_arcade::pr32_port::micros(); }
inline void delay(unsigned long) {}
inline void yield() {}

#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif
#ifndef INPUT
#define INPUT 0x01
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x05
#endif
#ifndef OUTPUT
#define OUTPUT 0x03
#endif

// The arcade never configures engine GPIO buttons (InputConfig count = 0).
inline void pinMode(std::uint8_t, std::uint8_t) {}
inline int digitalRead(std::uint8_t) { return HIGH; }

struct LaSerialStub {
    void begin(unsigned long) {}
    template <typename... A> void printf(const char*, A...) {}
    template <typename T> void print(const T&) {}
    template <typename T> void println(const T&) {}
    void println() {}
};
inline LaSerialStub Serial;

// Engine.cpp's debug overlay reads heap stats; the overlay is compiled out.
struct LaEspStub {
    std::uint32_t getFreeHeap() const { return 0; }
    std::uint32_t getHeapSize() const { return 0; }
};
inline LaEspStub ESP;

// Engine.cpp aliases this namespace even when no engine driver is built.
namespace pixelroot32::drivers {}
