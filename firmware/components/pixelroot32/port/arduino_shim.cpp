#include "Arduino.h"

namespace little_arcade::pr32_port {
namespace {
std::uint32_t g_now_ms = 0;
}
std::uint32_t millis() { return g_now_ms; }
std::uint32_t micros() { return g_now_ms * 1000U; }
void set_now_ms(std::uint32_t now_ms) { g_now_ms = now_ms; }
}  // namespace little_arcade::pr32_port
