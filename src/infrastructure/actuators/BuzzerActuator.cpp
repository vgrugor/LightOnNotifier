#include "infrastructure/actuators/BuzzerActuator.h"

#include <Arduino.h>

void BuzzerActuator::begin() {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
}
void BuzzerActuator::setState(bool state) {
    digitalWrite(pin, state ? HIGH : LOW);
}
