#include "application/TimeService.h"

void TimeService::update(uint32_t now, bool connected) {
    const bool valid = time.isValid();
    if (valid != ready) {
        ready = valid;
        events.publish(Event(valid ? EventType::TIME_READY : EventType::TIME_WAITING));
    }
    if (connected &&
        (!requested || !wasConnected || (!valid && uint32_t(now - lastRequest) >= RETRY_MS))) {
        time.startSynchronization();
        lastRequest = now;
        requested = true;
        if (!valid) {
            events.publish(Event(EventType::TIME_WAITING));
        }
    }
    wasConnected = connected;
}
