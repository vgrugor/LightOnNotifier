#include "infrastructure/actuators/ExternalLedActuator.h"

#include <Arduino.h>

void ExternalLedActuator::begin() {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
}
void ExternalLedActuator::setState(bool state) {
    digitalWrite(pin, state ? HIGH : LOW);
}
