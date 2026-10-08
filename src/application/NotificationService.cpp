#include "application/NotificationService.h"

#include <ctype.h>
#include <string.h>

namespace {
bool fits(const char* text, size_t limit) {
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i <= limit; ++i) {
        if (text[i] == '\0') {
            return true;
        }
    }
    return false;
}

bool sameRecipient(const char* left, const char* right) {
    if (left[0] != '@' || right[0] != '@') {
        return strcmp(left, right) == 0;
    }
    for (size_t index = 0;; ++index) {
        if (tolower(static_cast<unsigned char>(left[index])) !=
            tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
        if (left[index] == '\0') {
            return true;
        }
    }
}

bool validRecipient(const char* text) {
    if (!fits(text, NotificationService::MAX_RECIPIENT_BYTES)) {
        return false;
    }
    if (text[0] == '@') {
        const size_t length = strlen(text);
        if (length < 6) {
            return false;
        }
        for (size_t i = 1; i < length; ++i) {
            const char value = text[i];
            if (!((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
                  (value >= '0' && value <= '9') || value == '_')) {
                return false;
            }
        }
        return true;
    }
    size_t index = text[0] == '-' ? 1 : 0;
    if (text[index] < '1' || text[index] > '9') {
        return false;
    }
    for (; text[index] != '\0'; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    return true;
}
} // namespace

bool NotificationService::validate(const char* text, const char* const* ids, size_t size) {
    if (!fits(text, MAX_MESSAGE_BYTES) || ids == nullptr || size == 0 || size > MAX_RECIPIENTS) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        if (!validRecipient(ids[i])) {
            return false;
        }
        for (size_t j = 0; j < i; ++j) {
            if (sameRecipient(ids[i], ids[j])) {
                return false;
            }
        }
    }
    return true;
}

bool NotificationService::enqueue(const char* text, const char* const* ids, size_t size) {
    if (initialized) {
        return false;
    }
    initialized = true;
    if (!validate(text, ids, size)) {
        events.publish(Event(EventType::NOTIFICATION_INVALID));
        return false;
    }
    strcpy(message, text);
    count = size;
    for (size_t i = 0; i < size; ++i) {
        strcpy(recipients[i].id, ids[i]);
    }
    valid = true;
    return true;
}

void NotificationService::update(uint32_t now, bool connected, bool timeReady) {
    if (!valid || !connected || !timeReady ||
        (attempted && uint32_t(now - lastSend) < SEND_GAP_MS)) {
        return;
    }
    for (size_t offset = 0; offset < count; ++offset) {
        const size_t index = (nextRecipient + offset) % count;
        Recipient& recipient = recipients[index];
        if (recipient.delivered || recipient.attempts == MAX_ATTEMPTS ||
            (recipient.attempts != 0 && uint32_t(now - recipient.lastAttempt) < RETRY_MS)) {
            continue;
        }
        ++recipient.attempts;
        attempted = true;
        nextRecipient = (index + 1) % count;
        recipient.delivered = sender.send(recipient.id, message);
        recipient.lastAttempt = clock.now();
        lastSend = recipient.lastAttempt;
        events.publish(Event(recipient.delivered ? EventType::MESSAGE_SEND
                                                 : (recipient.attempts == MAX_ATTEMPTS
                                                        ? EventType::MESSAGE_EXHAUSTED
                                                        : EventType::MESSAGE_SEND_SKIP),
                             "", index, recipient.attempts));
        return;
    }
}

NotificationService::Outcome NotificationService::recipientOutcome(size_t index) const {
    if (index >= count) {
        return Outcome::INVALID;
    }
    if (recipients[index].delivered) {
        return Outcome::DELIVERED;
    }
    if (recipients[index].attempts == MAX_ATTEMPTS) {
        return Outcome::EXHAUSTED;
    }
    return Outcome::WAITING;
}

uint8_t NotificationService::attempts(size_t index) const {
    return index < count ? recipients[index].attempts : 0;
}

NotificationService::Outcome NotificationService::outcome() const {
    if (!initialized) {
        return Outcome::EMPTY;
    }
    if (!valid) {
        return Outcome::INVALID;
    }
    bool exhausted = false;
    for (size_t i = 0; i < count; ++i) {
        const Outcome result = recipientOutcome(i);
        if (result == Outcome::WAITING) {
            return result;
        }
        exhausted = exhausted || result == Outcome::EXHAUSTED;
    }
    return exhausted ? Outcome::EXHAUSTED : Outcome::DELIVERED;
}
