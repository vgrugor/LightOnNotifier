#ifndef LIGHTONNOTIFIER_DOMAIN_ACTUATOR_H
#define LIGHTONNOTIFIER_DOMAIN_ACTUATOR_H

class Actuator {
public:
    virtual ~Actuator() = default;
    virtual void setState(bool state) = 0;
};
#endif
