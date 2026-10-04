#include "application/NotifierApplication.h"

void NotifierApplication::begin(uint32_t now, const char* message, const char* const* recipients,
                                size_t count) {
    if (started) {
        return;
    }
    started = true;
    signals.begin();
    events.publish(Event(EventType::LIGHT_ON));
    signals.update(now);
    notifications.enqueue(message, recipients, count);
    connection.begin(now);
}

void NotifierApplication::update(uint32_t now) {
    if (!started) {
        return;
    }
    connection.update(now);
    const bool connected = connection.isConnected();
    time.update(now, connected);
    signals.update(now);
    if (connected) {
        if (!otaStarted) {
            ota.begin();
            otaStarted = true;
        }
        ota.handle();
    }
    notifications.update(now, connected, time.isReady());
}
