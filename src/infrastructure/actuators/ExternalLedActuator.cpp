#include "infrastructure/actuators/ExternalLedActuator.h"

#include <Arduino.h>

namespace {
constexpr uint32_t PWM_RANGE = 1023;
}

void ExternalLedActuator::begin() {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
    analogWriteRange(PWM_RANGE);
    brightness = 0;
}
void ExternalLedActuator::setState(bool state) {
    setBrightness(state ? 100 : 0);
}

void ExternalLedActuator::setBrightness(uint8_t percent) {
    const uint8_t bounded = percent > 100 ? 100 : percent;
    if (bounded == brightness) {
        return;
    }
    brightness = bounded;
    const uint32_t duty = (static_cast<uint32_t>(brightness) * PWM_RANGE + 50) / 100;
    analogWrite(pin, static_cast<int>(duty));
}
