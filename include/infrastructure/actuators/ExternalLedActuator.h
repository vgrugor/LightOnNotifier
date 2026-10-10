#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_ACTUATORS_EXTERNALLEDACTUATOR_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_ACTUATORS_EXTERNALLEDACTUATOR_H

#include <stdint.h>

#include "domain/Actuator.h"

class ExternalLedActuator : public Actuator {
public:
    explicit ExternalLedActuator(int pin) : pin(pin) {}
    void begin();
    void setState(bool state) override;
    void setBrightness(uint8_t percent);

private:
    int pin;
    uint8_t brightness = 0;
};
#endif
