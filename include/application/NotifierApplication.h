#ifndef LIGHTONNOTIFIER_APPLICATION_NOTIFIERAPPLICATION_H
#define LIGHTONNOTIFIER_APPLICATION_NOTIFIERAPPLICATION_H

#include "application/ConnectionService.h"
#include "application/NotificationService.h"
#include "application/SignalController.h"
#include "application/TimeService.h"

class NotifierApplication {
public:
    NotifierApplication(ConnectionService& connection, TimeService& time,
                        NotificationService& notifications, SignalController& signals, OtaPort& ota,
                        EventSink& events)
        : connection(connection), time(time), notifications(notifications), signals(signals),
          ota(ota), events(events) {}
    void begin(uint32_t now, const char* message, const char* const* recipients, size_t count);
    void update(uint32_t now);

private:
    ConnectionService& connection;
    TimeService& time;
    NotificationService& notifications;
    SignalController& signals;
    OtaPort& ota;
    EventSink& events;
    bool started = false;
    bool otaStarted = false;
};
#endif
