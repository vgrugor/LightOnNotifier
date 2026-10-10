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
    void setLedBrightness(uint8_t percent) override {
        ledBrightness = percent > 100 ? 100 : percent;
        led.setBrightness(ledBrightness);
    }
    void setBuzzer(bool active) override {
        buzzer.setState(active);
        buzzerActive = active;
    }
    bool isLedActive() const {
        return ledBrightness != 0;
    }
    uint8_t ledBrightnessPercent() const {
        return ledBrightness;
    }
    bool isBuzzerActive() const {
        return buzzerActive;
    }

private:
    ExternalLedActuator& led;
    BuzzerActuator& buzzer;
    uint8_t ledBrightness = 0;
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
