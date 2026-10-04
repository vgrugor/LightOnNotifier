#ifndef LIGHTONNOTIFIER_APPLICATION_TIMESERVICE_H
#define LIGHTONNOTIFIER_APPLICATION_TIMESERVICE_H

#include <stdint.h>

#include "application/Ports.h"
#include "domain/Event.h"

class TimeService {
public:
    static constexpr uint32_t RETRY_MS = 60000;
    TimeService(TimePort& time, EventSink& events) : time(time), events(events) {}
    void update(uint32_t now, bool connected);
    bool isReady() const {
        return ready;
    }

private:
    TimePort& time;
    EventSink& events;
    bool requested = false;
    bool ready = false;
    bool wasConnected = false;
    uint32_t lastRequest = 0;
};
#endif
