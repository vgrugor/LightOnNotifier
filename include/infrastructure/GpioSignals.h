#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_GPIOSIGNALS_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_GPIOSIGNALS_H

#include <Arduino.h>

#include "application/Ports.h"
#include "infrastructure/actuators/BuzzerActuator.h"
#include "infrastructure/actuators/ExternalLedActuator.h"

class GpioSignals : public SignalOutput {
public:
    GpioSignals(ExternalLedActuator& led, BuzzerActuator& buzzer) : led(led), buzzer(buzzer) {}
    void begin() {
        led.begin();
        buzzer.begin();
    }
    void setLed(bool active) override {
        led.setState(active);
        ledActive = active;
    }
    void setBuzzer(bool active) override {
        buzzer.setState(active);
        buzzerActive = active;
    }
    bool isLedActive() const {
        return ledActive;
    }
    bool isBuzzerActive() const {
        return buzzerActive;
    }

private:
    ExternalLedActuator& led;
    BuzzerActuator& buzzer;
    bool ledActive = false;
    bool buzzerActive = false;
};

class GpioButton : public ButtonPort {
public:
    explicit GpioButton(int pin) : pin(pin) {}
    void begin() {
        pinMode(pin, INPUT_PULLUP);
    }
    bool isPressed() const override {
        return digitalRead(pin) == LOW;
    }

private:
    int pin;
};

class ArduinoClock : public MonotonicClock {
public:
    uint32_t now() const override {
        return millis();
    }
};
#endif
