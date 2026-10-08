#ifndef LIGHTONNOTIFIER_DOMAIN_EVENT_H
#define LIGHTONNOTIFIER_DOMAIN_EVENT_H

#include <stddef.h>
#include <stdint.h>

enum class EventType {
    LIGHT_ON,
    WIFI_START_CONNECT,
    WIFI_TRY_CONNECT,
    WIFI_CONNECTED,
    WIFI_RECONNECT,
    WIFI_CONFIGURATION_INVALID,
    TIME_WAITING,
    TIME_READY,
    MESSAGE_SEND,
    MESSAGE_SEND_SKIP,
    MESSAGE_EXHAUSTED,
    NOTIFICATION_INVALID
};

// Payload is borrowed only for synchronous dispatch. Receivers retaining it must copy it.
struct Event {
    EventType type;
    const char* message = "";
    size_t recipientIndex = 0;
    uint8_t attempt = 0;
    explicit Event(EventType type, const char* message = "", size_t recipientIndex = 0,
                   uint8_t attempt = 0)
        : type(type), message(message), recipientIndex(recipientIndex), attempt(attempt) {}
};

class EventSink {
public:
    virtual ~EventSink() = default;
    virtual void publish(const Event& event) = 0;
};

class Observer {
public:
    virtual ~Observer() = default;
    virtual void onEvent(const Event& event) = 0;
};
#endif
