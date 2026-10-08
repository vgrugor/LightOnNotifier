#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_ACTUATORS_BUZZERACTUATOR_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_ACTUATORS_BUZZERACTUATOR_H

#include "domain/Actuator.h"

class BuzzerActuator : public Actuator {
public:
    explicit BuzzerActuator(int pin) : pin(pin) {}
    void begin();
    void setState(bool state) override;

private:
    int pin;
};
#endif
