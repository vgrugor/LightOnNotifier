#ifndef LIGHTONNOTIFIER_APPLICATION_NOTIFICATIONSERVICE_H
#define LIGHTONNOTIFIER_APPLICATION_NOTIFICATIONSERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "application/Ports.h"
#include "domain/Event.h"

class NotificationService {
public:
    enum class Outcome { EMPTY, WAITING, DELIVERED, EXHAUSTED, INVALID };
    static constexpr size_t MAX_RECIPIENTS = 8;
    static constexpr size_t MAX_RECIPIENT_BYTES = 33;
    static constexpr size_t MAX_MESSAGE_BYTES = 512;
    static constexpr uint8_t MAX_ATTEMPTS = 5;
    static constexpr uint32_t RETRY_MS = 1000;
    static constexpr uint32_t SEND_GAP_MS = 200;
    NotificationService(MessageSender& sender, EventSink& events, MonotonicClock& clock)
        : sender(sender), events(events), clock(clock) {}
    static bool validate(const char* message, const char* const* recipients, size_t count);
    // One startup batch per instance. Copies text/IDs into bounded storage; empty lists are
    // invalid.
    bool enqueue(const char* message, const char* const* recipients, size_t count);
    void update(uint32_t now, bool connected, bool timeReady);
    Outcome outcome() const;
    Outcome recipientOutcome(size_t index) const;
    uint8_t attempts(size_t index) const;
    size_t recipientCount() const {
        return count;
    }

private:
    struct Recipient {
        char id[MAX_RECIPIENT_BYTES + 1] = {};
        uint8_t attempts = 0;
        bool delivered = false;
        uint32_t lastAttempt = 0;
    };
    MessageSender& sender;
    EventSink& events;
    MonotonicClock& clock;
    Recipient recipients[MAX_RECIPIENTS];
    char message[MAX_MESSAGE_BYTES + 1] = {};
    size_t count = 0;
    size_t nextRecipient = 0;
    bool initialized = false;
    bool valid = false;
    bool attempted = false;
    uint32_t lastSend = 0;
};
#endif
