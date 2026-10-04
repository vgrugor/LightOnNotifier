#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_TIME_NETWORKTIME_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_TIME_NETWORKTIME_H

#include "application/Ports.h"

class NetworkTime : public TimePort {
public:
    void startSynchronization() override;
    bool isValid() const override;
};
#endif
