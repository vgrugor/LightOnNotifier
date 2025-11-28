#include "infrastructure/actuators/InternalLedActuator.h"

InternalLedActuator::InternalLedActuator(int pin) : pin(pin) {
    pinMode(pin, OUTPUT);
}

void InternalLedActuator::setState(bool state) {
    digitalWrite(pin, state ? HIGH : LOW);
}

void InternalLedActuator::update() {
    // Обновление состояния актуатора (если требуется)
}
