#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_ACTUATORS_EXTERNALLEDACTUATOR_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_ACTUATORS_EXTERNALLEDACTUATOR_H

#include "domain/Actuator.h"

class ExternalLedActuator : public Actuator {
public:
    explicit ExternalLedActuator(int pin) : pin(pin) {}
    void begin();
    void setState(bool state) override;

private:
    int pin;
};
#endif
