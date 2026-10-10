#ifndef LIGHTONNOTIFIER_TEST_FAKES_ARDUINO_H
#define LIGHTONNOTIFIER_TEST_FAKES_ARDUINO_H

#include <stdint.h>
#include <vector>

constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int OUTPUT = 1;

namespace FakeGpio {
enum class Kind { DIGITAL, MODE, RANGE, PWM };
struct Call {
    Kind kind;
    int pin;
    uint32_t value;
};
inline std::vector<Call> calls;
} // namespace FakeGpio

inline void digitalWrite(int pin, int value) {
    FakeGpio::calls.push_back({FakeGpio::Kind::DIGITAL, pin, static_cast<uint32_t>(value)});
}
inline void pinMode(int pin, int mode) {
    FakeGpio::calls.push_back({FakeGpio::Kind::MODE, pin, static_cast<uint32_t>(mode)});
}
inline void analogWriteRange(uint32_t range) {
    FakeGpio::calls.push_back({FakeGpio::Kind::RANGE, -1, range});
}
inline void analogWrite(int pin, int duty) {
    FakeGpio::calls.push_back({FakeGpio::Kind::PWM, pin, static_cast<uint32_t>(duty)});
}

#endif
