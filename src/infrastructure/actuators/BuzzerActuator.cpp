#include "infrastructure/actuators/BuzzerActuator.h"

BuzzerActuator::BuzzerActuator(int pin) : pin(pin) {
    pinMode(pin, OUTPUT);
}

void BuzzerActuator::setState(bool state) {
    digitalWrite(pin, state ? HIGH : LOW);
}

void BuzzerActuator::update() {
    // Обновление состояния актуатора (если требуется)
}
