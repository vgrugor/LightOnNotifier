#include "infrastructure/actuators/ExternalLedActuator.h"

ExternalLedActuator::ExternalLedActuator(int pin) : pin(pin) {
    pinMode(pin, OUTPUT);
}

void ExternalLedActuator::setState(bool state) {
    digitalWrite(pin, state ? HIGH : LOW);
}

void ExternalLedActuator::update() {
    // Обновление состояния актуатора (если требуется)
}
