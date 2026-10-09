#include <algorithm>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

#include <EEPROM.h>
#include <LittleFS.h>
#include <unity.h>

#include "application/ConnectionService.h"
#include "application/NotificationService.h"
#include "application/NotifierApplication.h"
#include "application/SignalController.h"
#include "application/StaticNetworkConfig.h"
#include "application/TimeService.h"
#include "infrastructure/settings/SettingsStore.h"
#include "infrastructure/telegram/HttpResponse.h"
#include "infrastructure/telegram/TelegramAcknowledgement.h"
#include "infrastructure/telegram/TelegramConnection.h"
#include "infrastructure/web/BoundedField.h"
#include "presentation/EventNotifier.h"

namespace {

void test_web_field_accepts_valid_wifi_values_and_rejects_oversize_or_embedded_null() {
    char ssid[33] = {};
    char password[65] = {};
    char address[16] = {};
    TEST_ASSERT_TRUE(copyBoundedField(ssid, sizeof(ssid), "ENERGYzal", 9));
    TEST_ASSERT_EQUAL_STRING("ENERGYzal", ssid);
    TEST_ASSERT_TRUE(copyBoundedField(password, sizeof(password), "1234567890", 10));
    TEST_ASSERT_EQUAL_STRING("1234567890", password);
    TEST_ASSERT_TRUE(copyBoundedField(address, sizeof(address), "192.168.1.204", 13));
    TEST_ASSERT_EQUAL_STRING("192.168.1.204", address);
    TEST_ASSERT_TRUE(copyBoundedField(address, sizeof(address), "255.255.255.0", 13));
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", address);

    const char maxLength[] = "abcdefghijklmnopqrstuvwxyz123456";
    TEST_ASSERT_TRUE(copyBoundedField(ssid, sizeof(ssid), maxLength, 32));
    TEST_ASSERT_EQUAL_STRING(maxLength, ssid);
    const char tooLong[] = "abcdefghijklmnopqrstuvwxyz1234567";
    TEST_ASSERT_FALSE(copyBoundedField(ssid, sizeof(ssid), tooLong, 33));
    TEST_ASSERT_EQUAL_STRING(maxLength, ssid);
    const char embeddedNull[] = {'E', 'N', '\0', 'X'};
    TEST_ASSERT_FALSE(copyBoundedField(ssid, sizeof(ssid), embeddedNull, sizeof(embeddedNull)));
    TEST_ASSERT_EQUAL_STRING(maxLength, ssid);
}

struct RecordedEvent {
    EventType type;
    std::string message;
    size_t recipientIndex;
    uint8_t attempt;
};

class FakeEvents : public EventSink {
public:
    std::vector<RecordedEvent> records;

    void publish(const Event& event) override {
        records.push_back({event.type, event.message, event.recipientIndex, event.attempt});
    }

    size_t count(EventType type) const {
        size_t matches = 0;
        for (const auto& event : records) {
            if (event.type == type) {
                ++matches;
            }
        }
        return matches;
    }
};

class FakeConnection : public ConnectionPort {
public:
    bool configured = true;
    bool connected = false;
    unsigned configurations = 0;
    unsigned starts = 0;
    unsigned stops = 0;

    bool configure() override {
        ++configurations;
        return configured;
    }
    void startAttempt() override {
        ++starts;
    }
    void stopAttempt() override {
        ++stops;
    }
    bool isConnected() const override {
        return connected;
    }
};

class FakeTime : public TimePort {
public:
    bool valid = false;
    uint8_t hour = 0;
    unsigned starts = 0;
    void startSynchronization() override {
        ++starts;
    }
    bool isValid() const override {
        return valid;
    }
    bool localHour(uint8_t& result) const override {
        if (!valid) {
            return false;
        }
        result = hour;
        return true;
    }
};

struct SentMessage {
    std::string recipient;
    std::string text;
};

class FakeClock : public MonotonicClock {
public:
    uint32_t current = 0;
    uint32_t now() const override {
        return current;
    }
};

class FakeSender : public MessageSender {
public:
    bool defaultResult = true;
    FakeClock* clock = nullptr;
    uint32_t blockingMs = 0;
    std::vector<bool> results;
    std::vector<SentMessage> calls;

    bool send(const char* recipient, const char* message) override {
        const size_t index = calls.size();
        calls.push_back({recipient, message});
        if (clock != nullptr) {
            clock->current += blockingMs;
        }
        return index < results.size() ? results[index] : defaultResult;
    }
};

class FakeOutput : public SignalOutput {
public:
    bool led = false;
    bool buzzer = false;
    void setLed(bool active) override {
        led = active;
    }
    void setBuzzer(bool active) override {
        buzzer = active;
    }
};

class FakeButton : public ButtonPort {
public:
    bool pressed = false;
    bool isPressed() const override {
        return pressed;
    }
};

class FakeOta : public OtaPort {
public:
    unsigned starts = 0;
    unsigned handles = 0;
    void begin() override {
        ++starts;
    }
    void handle() override {
        ++handles;
    }
};

class RecordingObserver : public Observer {
public:
    unsigned calls = 0;
    unsigned id = 0;
    std::vector<unsigned>* order = nullptr;
    std::string copiedMessage;
    const char* observedPayload = nullptr;
    void onEvent(const Event& event) override {
        ++calls;
        observedPayload = event.message;
        copiedMessage = event.message;
        if (order != nullptr) {
            order->push_back(id);
        }
    }
};

class MutatingObserver : public RecordingObserver {
public:
    EventNotifier* notifier = nullptr;
    Observer* extra = nullptr;
    bool addedDuringDispatch = true;
    bool removedDuringDispatch = true;

    void onEvent(const Event& event) override {
        RecordingObserver::onEvent(event);
        addedDuringDispatch = notifier->addObserver(extra);
        removedDuringDispatch = notifier->removeObserver(this);
        notifier->publish(Event(EventType::MESSAGE_SEND, "nested"));
    }
};

void assertState(ConnectionService::State expected, const ConnectionService& service) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(service.state()));
}

void assertOutcome(NotificationService::Outcome expected, const NotificationService& service) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(service.outcome()));
}

void assertRecipientOutcome(NotificationService::Outcome expected,
                            const NotificationService& service, size_t index) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected),
                          static_cast<int>(service.recipientOutcome(index)));
}

void tick(NotificationService& service, FakeClock& clock, uint32_t now, bool connected = true,
          bool timeReady = true) {
    clock.current = now;
    service.update(now, connected, timeReady);
}

void tickApp(NotifierApplication& app, FakeClock& clock, uint32_t now) {
    clock.current = now;
    app.update(now);
}

void test_dispatch_rejects_null_duplicate_and_capacity_then_removes_in_order() {
    EventNotifier notifier;
    RecordingObserver observers[EventNotifier::MAX_OBSERVERS + 1];
    std::vector<unsigned> order;
    TEST_ASSERT_FALSE(notifier.addObserver(nullptr));
    TEST_ASSERT_FALSE(notifier.removeObserver(nullptr));
    for (size_t i = 0; i < EventNotifier::MAX_OBSERVERS; ++i) {
        observers[i].id = static_cast<unsigned>(i);
        observers[i].order = &order;
        TEST_ASSERT_TRUE(notifier.addObserver(&observers[i]));
    }
    TEST_ASSERT_FALSE(notifier.addObserver(&observers[0]));
    TEST_ASSERT_FALSE(notifier.addObserver(&observers[EventNotifier::MAX_OBSERVERS]));
    notifier.publish(Event(EventType::LIGHT_ON));
    TEST_ASSERT_EQUAL_UINT(EventNotifier::MAX_OBSERVERS, order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        TEST_ASSERT_EQUAL_UINT(i, order[i]);
    }
    TEST_ASSERT_TRUE(notifier.removeObserver(&observers[1]));
    TEST_ASSERT_FALSE(notifier.removeObserver(&observers[1]));
    TEST_ASSERT_TRUE(notifier.addObserver(&observers[EventNotifier::MAX_OBSERVERS]));
    order.clear();
    notifier.publish(Event(EventType::WIFI_CONNECTED));
    TEST_ASSERT_EQUAL_UINT(1, observers[1].calls);
    TEST_ASSERT_EQUAL_UINT(0, order[0]);
    TEST_ASSERT_EQUAL_UINT(2, order[1]);
    TEST_ASSERT_EQUAL_UINT(3, order[2]);
    TEST_ASSERT_EQUAL_UINT(1, observers[EventNotifier::MAX_OBSERVERS].calls);
}

void test_dispatch_rejects_mutation_and_nesting_and_borrows_payload_synchronously() {
    EventNotifier notifier;
    MutatingObserver mutator;
    RecordingObserver receiver;
    RecordingObserver extra;
    mutator.notifier = &notifier;
    mutator.extra = &extra;
    TEST_ASSERT_TRUE(notifier.addObserver(&mutator));
    TEST_ASSERT_TRUE(notifier.addObserver(&receiver));
    char payload[] = "original";
    notifier.publish(Event(EventType::LIGHT_ON, payload));
    TEST_ASSERT_FALSE(mutator.addedDuringDispatch);
    TEST_ASSERT_FALSE(mutator.removedDuringDispatch);
    TEST_ASSERT_EQUAL_UINT(1, mutator.calls);
    TEST_ASSERT_EQUAL_UINT(1, receiver.calls);
    TEST_ASSERT_EQUAL_UINT(0, extra.calls);
    TEST_ASSERT_EQUAL_PTR(payload, receiver.observedPayload);
    payload[0] = 'X';
    TEST_ASSERT_EQUAL_STRING("original", receiver.copiedMessage.c_str());
    TEST_ASSERT_TRUE(notifier.removeObserver(&mutator));
    TEST_ASSERT_TRUE(notifier.addObserver(&extra));
    notifier.publish(Event(EventType::WIFI_CONNECTED));
    TEST_ASSERT_EQUAL_UINT(2, receiver.calls);
    TEST_ASSERT_EQUAL_UINT(1, extra.calls);
}

void test_connection_invalid_configuration_never_starts_or_retries() {
    FakeConnection port;
    FakeEvents events;
    port.configured = false;
    ConnectionService service(port, events);
    service.update(0);
    service.begin(10);
    service.begin(20);
    service.update(UINT32_MAX);
    assertState(ConnectionService::State::INVALID, service);
    TEST_ASSERT_FALSE(service.isConnected());
    TEST_ASSERT_EQUAL_UINT(1, port.configurations);
    TEST_ASSERT_EQUAL_UINT(0, port.starts);
    TEST_ASSERT_EQUAL_UINT(0, port.stops);
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::WIFI_CONFIGURATION_INVALID));
}

void test_connection_reconfigure_recovers_invalid_settings_without_reboot() {
    FakeConnection port;
    FakeEvents events;
    port.configured = false;
    ConnectionService service(port, events);
    service.begin(0);
    assertState(ConnectionService::State::INVALID, service);
    port.configured = true;
    service.reconfigure(100);
    assertState(ConnectionService::State::CONNECTING, service);
    TEST_ASSERT_EQUAL_UINT(2, port.configurations);
    TEST_ASSERT_EQUAL_UINT(1, port.starts);
    port.connected = true;
    service.update(101);
    TEST_ASSERT_TRUE(service.isConnected());
    port.connected = false;
    service.reconfigure(200);
    TEST_ASSERT_EQUAL_UINT(2, port.starts);
    TEST_ASSERT_EQUAL_UINT(2, port.stops);
    assertState(ConnectionService::State::CONNECTING, service);
}

void test_connection_unavailable_expires_and_retries_at_interval_boundaries() {
    FakeConnection port;
    FakeEvents events;
    ConnectionService service(port, events);
    const uint32_t start = 27;
    service.begin(start);
    service.begin(start + 1);
    TEST_ASSERT_EQUAL_UINT(1, port.starts);
    service.update(start + ConnectionService::PROGRESS_MS - 1);
    TEST_ASSERT_EQUAL_UINT(0, events.count(EventType::WIFI_TRY_CONNECT));
    service.update(start + ConnectionService::PROGRESS_MS);
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::WIFI_TRY_CONNECT));
    service.update(start + ConnectionService::ATTEMPT_MS - 1);
    assertState(ConnectionService::State::CONNECTING, service);
    service.update(start + ConnectionService::ATTEMPT_MS);
    assertState(ConnectionService::State::RETRY_WAIT, service);
    TEST_ASSERT_EQUAL_UINT(1, port.stops);
    service.update(start + ConnectionService::ATTEMPT_MS + ConnectionService::RETRY_MS - 1);
    TEST_ASSERT_EQUAL_UINT(1, port.starts);
    service.update(start + ConnectionService::ATTEMPT_MS + ConnectionService::RETRY_MS);
    assertState(ConnectionService::State::CONNECTING, service);
    TEST_ASSERT_EQUAL_UINT(2, port.starts);
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::WIFI_RECONNECT));
}

void test_connection_loss_retries_immediately_and_recovery_publishes_once() {
    FakeConnection port;
    FakeEvents events;
    ConnectionService service(port, events);
    service.begin(0);
    port.connected = true;
    service.update(1);
    service.update(2);
    TEST_ASSERT_TRUE(service.isConnected());
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::WIFI_CONNECTED));
    port.connected = false;
    TEST_ASSERT_FALSE(service.isConnected());
    service.update(3);
    assertState(ConnectionService::State::CONNECTING, service);
    TEST_ASSERT_EQUAL_UINT(2, port.starts);
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::WIFI_RECONNECT));
    port.connected = true;
    service.update(4);
    TEST_ASSERT_TRUE(service.isConnected());
    TEST_ASSERT_EQUAL_UINT(2, events.count(EventType::WIFI_CONNECTED));
}

void test_connection_retry_timing_survives_millis_rollover() {
    FakeConnection port;
    FakeEvents events;
    ConnectionService service(port, events);
    const uint32_t start = UINT32_MAX - ConnectionService::ATTEMPT_MS / 2;
    service.begin(start);
    service.update(start + ConnectionService::ATTEMPT_MS - 1);
    TEST_ASSERT_EQUAL_UINT(0, port.stops);
    service.update(start + ConnectionService::ATTEMPT_MS);
    TEST_ASSERT_EQUAL_UINT(1, port.stops);
    service.update(start + ConnectionService::ATTEMPT_MS + ConnectionService::RETRY_MS - 1);
    TEST_ASSERT_EQUAL_UINT(1, port.starts);
    service.update(start + ConnectionService::ATTEMPT_MS + ConnectionService::RETRY_MS);
    TEST_ASSERT_EQUAL_UINT(2, port.starts);
}

void test_time_waits_offline_and_retries_without_waiting_then_accepts_late_time() {
    FakeTime port;
    FakeEvents events;
    TimeService service(port, events);
    service.update(0, false);
    TEST_ASSERT_EQUAL_UINT(0, port.starts);
    TEST_ASSERT_FALSE(service.isReady());
    service.update(10, true);
    TEST_ASSERT_EQUAL_UINT(1, port.starts);
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::TIME_WAITING));
    service.update(10 + TimeService::RETRY_MS - 1, true);
    TEST_ASSERT_EQUAL_UINT(1, port.starts);
    service.update(10 + TimeService::RETRY_MS, true);
    TEST_ASSERT_EQUAL_UINT(2, port.starts);
    port.valid = true;
    service.update(11 + TimeService::RETRY_MS, true);
    service.update(12 + TimeService::RETRY_MS, true);
    TEST_ASSERT_TRUE(service.isReady());
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::TIME_READY));
    TEST_ASSERT_EQUAL_UINT(2, port.starts);
}

void test_time_regression_revokes_readiness_and_reconnect_requests_sync() {
    FakeTime port;
    FakeEvents events;
    TimeService service(port, events);
    port.valid = true;
    service.update(0, true);
    TEST_ASSERT_TRUE(service.isReady());
    port.valid = false;
    service.update(1, true);
    TEST_ASSERT_FALSE(service.isReady());
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::TIME_WAITING));
    service.update(2, false);
    service.update(3, true);
    TEST_ASSERT_EQUAL_UINT(2, port.starts);
    port.valid = true;
    service.update(4, true);
    TEST_ASSERT_TRUE(service.isReady());
    TEST_ASSERT_EQUAL_UINT(2, events.count(EventType::TIME_READY));
}

void test_time_retry_interval_survives_millis_rollover() {
    FakeTime port;
    FakeEvents events;
    TimeService service(port, events);
    const uint32_t start = UINT32_MAX - TimeService::RETRY_MS / 2;
    service.update(start, true);
    service.update(start + TimeService::RETRY_MS - 1, true);
    TEST_ASSERT_EQUAL_UINT(1, port.starts);
    service.update(start + TimeService::RETRY_MS, true);
    TEST_ASSERT_EQUAL_UINT(2, port.starts);
}

void test_notification_keeps_pending_until_network_and_time_are_ready_and_copies_inputs() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    NotificationService service(sender, events, clock);
    assertOutcome(NotificationService::Outcome::EMPTY, service);
    char message[] = "startup";
    char id[] = "-123";
    const char* recipients[] = {id};
    TEST_ASSERT_TRUE(service.enqueue(message, recipients, 1));
    message[0] = 'X';
    id[1] = '9';
    tick(service, clock, 0, false, false);
    tick(service, clock, 1000, true, false);
    tick(service, clock, 2000, false, true);
    TEST_ASSERT_EQUAL_UINT(0, sender.calls.size());
    TEST_ASSERT_EQUAL_UINT(0, service.attempts(0));
    assertOutcome(NotificationService::Outcome::WAITING, service);
    tick(service, clock, 3000, true, true);
    TEST_ASSERT_EQUAL_UINT(1, sender.calls.size());
    TEST_ASSERT_EQUAL_STRING("startup", sender.calls[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("-123", sender.calls[0].recipient.c_str());
    assertOutcome(NotificationService::Outcome::DELIVERED, service);
    tick(service, clock, 100000, true, true);
    TEST_ASSERT_EQUAL_UINT(1, sender.calls.size());
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::MESSAGE_SEND));
    TEST_ASSERT_EQUAL_UINT(0, events.records.back().recipientIndex);
    TEST_ASSERT_EQUAL_UINT(1, events.records.back().attempt);
}

void test_partial_delivery_never_resends_acknowledged_recipient() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    NotificationService service(sender, events, clock);
    sender.results = {false, true, true};
    const char* recipients[] = {"101", "102"};
    TEST_ASSERT_TRUE(service.enqueue("startup", recipients, 2));
    tick(service, clock, 0, true, true);
    tick(service, clock, NotificationService::SEND_GAP_MS - 1, true, true);
    TEST_ASSERT_EQUAL_UINT(1, sender.calls.size());
    tick(service, clock, NotificationService::SEND_GAP_MS, true, true);
    assertRecipientOutcome(NotificationService::Outcome::DELIVERED, service, 1);
    assertOutcome(NotificationService::Outcome::WAITING, service);
    tick(service, clock, NotificationService::RETRY_MS - 1, true, true);
    TEST_ASSERT_EQUAL_UINT(2, sender.calls.size());
    tick(service, clock, NotificationService::RETRY_MS, true, true);
    assertOutcome(NotificationService::Outcome::DELIVERED, service);
    TEST_ASSERT_EQUAL_UINT(3, sender.calls.size());
    TEST_ASSERT_EQUAL_STRING("101", sender.calls[2].recipient.c_str());
    TEST_ASSERT_EQUAL_UINT(2, service.attempts(0));
    TEST_ASSERT_EQUAL_UINT(1, service.attempts(1));
    tick(service, clock, 100000, true, true);
    TEST_ASSERT_EQUAL_UINT(3, sender.calls.size());
}

void test_notification_exhausts_exactly_five_attempts_and_keeps_other_success() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    NotificationService service(sender, events, clock);
    sender.defaultResult = false;
    sender.results = {false, true};
    const char* recipients[] = {"101", "102"};
    TEST_ASSERT_TRUE(service.enqueue("startup", recipients, 2));
    tick(service, clock, 0, true, true);
    tick(service, clock, NotificationService::SEND_GAP_MS, true, true);
    for (uint8_t attempt = 1; attempt < NotificationService::MAX_ATTEMPTS; ++attempt) {
        const uint32_t due = attempt * NotificationService::RETRY_MS;
        tick(service, clock, due - 1, true, true);
        TEST_ASSERT_EQUAL_UINT(attempt, service.attempts(0));
        tick(service, clock, due, true, true);
        TEST_ASSERT_EQUAL_UINT(attempt + 1, service.attempts(0));
    }
    TEST_ASSERT_EQUAL_UINT(5, service.attempts(0));
    TEST_ASSERT_EQUAL_UINT(1, service.attempts(1));
    assertRecipientOutcome(NotificationService::Outcome::EXHAUSTED, service, 0);
    assertRecipientOutcome(NotificationService::Outcome::DELIVERED, service, 1);
    assertOutcome(NotificationService::Outcome::EXHAUSTED, service);
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::MESSAGE_EXHAUSTED));
    TEST_ASSERT_EQUAL_UINT(0, events.records.back().recipientIndex);
    TEST_ASSERT_EQUAL_UINT(5, events.records.back().attempt);
    tick(service, clock, 100000, true, true);
    TEST_ASSERT_EQUAL_UINT(6, sender.calls.size());
}

void test_notification_disconnect_and_invalid_time_do_not_consume_retries() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    NotificationService service(sender, events, clock);
    sender.defaultResult = false;
    const char* recipients[] = {"101"};
    TEST_ASSERT_TRUE(service.enqueue("startup", recipients, 1));
    tick(service, clock, 0, true, true);
    for (uint32_t now = 1000; now <= 10000; now += 1000) {
        tick(service, clock, now, false, true);
        tick(service, clock, now, true, false);
    }
    TEST_ASSERT_EQUAL_UINT(1, service.attempts(0));
    sender.defaultResult = true;
    tick(service, clock, 10001, true, true);
    TEST_ASSERT_EQUAL_UINT(2, service.attempts(0));
    assertOutcome(NotificationService::Outcome::DELIVERED, service);
}

void assertInvalidBatch(const char* message, const char* const* recipients, size_t count) {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    NotificationService service(sender, events, clock);
    TEST_ASSERT_FALSE(service.enqueue(message, recipients, count));
    assertOutcome(NotificationService::Outcome::INVALID, service);
    tick(service, clock, 0, true, true);
    TEST_ASSERT_EQUAL_UINT(0, sender.calls.size());
    TEST_ASSERT_EQUAL_UINT(1, events.count(EventType::NOTIFICATION_INVALID));
}

void test_notification_rejects_missing_malformed_duplicate_and_oversized_inputs() {
    const char* valid[] = {"101"};
    assertInvalidBatch(nullptr, valid, 1);
    assertInvalidBatch("", valid, 1);
    assertInvalidBatch("startup", nullptr, 1);
    assertInvalidBatch("startup", valid, 0);
    const char* malformed[] = {nullptr, "", "-", "+1", "12x", "1 2"};
    for (const auto* id : malformed) {
        const char* recipient[] = {id};
        assertInvalidBatch("startup", recipient, 1);
    }
    const char* duplicate[] = {"101", "101"};
    assertInvalidBatch("startup", duplicate, 2);
    char longId[NotificationService::MAX_RECIPIENT_BYTES + 2];
    memset(longId, '1', sizeof(longId) - 1);
    longId[sizeof(longId) - 1] = '\0';
    const char* oversized[] = {longId};
    assertInvalidBatch("startup", oversized, 1);
    char longMessage[NotificationService::MAX_MESSAGE_BYTES + 2];
    memset(longMessage, 'm', sizeof(longMessage) - 1);
    longMessage[sizeof(longMessage) - 1] = '\0';
    assertInvalidBatch(longMessage, valid, 1);
    const char* tooMany[NotificationService::MAX_RECIPIENTS + 1] = {};
    assertInvalidBatch("startup", tooMany, NotificationService::MAX_RECIPIENTS + 1);
}

void test_notification_accepts_storage_boundaries_and_sends_all_eight_once() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    NotificationService service(sender, events, clock);
    char message[NotificationService::MAX_MESSAGE_BYTES + 1];
    memset(message, 'm', sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
    char id[NotificationService::MAX_RECIPIENT_BYTES + 1];
    memset(id, '1', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    const char* recipients[] = {id, "2", "3", "4", "5", "6", "7", "8"};
    TEST_ASSERT_EQUAL_UINT(NotificationService::MAX_RECIPIENTS,
                           sizeof(recipients) / sizeof(recipients[0]));
    TEST_ASSERT_TRUE(service.enqueue(message, recipients, NotificationService::MAX_RECIPIENTS));
    for (size_t index = 0; index < NotificationService::MAX_RECIPIENTS; ++index) {
        tick(service, clock, static_cast<uint32_t>(index) * NotificationService::SEND_GAP_MS, true,
             true);
        TEST_ASSERT_EQUAL_UINT(index + 1, sender.calls.size());
        TEST_ASSERT_EQUAL_STRING(recipients[index], sender.calls[index].recipient.c_str());
        TEST_ASSERT_EQUAL_STRING(message, sender.calls[index].text.c_str());
        TEST_ASSERT_EQUAL_UINT(1, service.attempts(index));
    }
    assertOutcome(NotificationService::Outcome::DELIVERED, service);
    assertRecipientOutcome(NotificationService::Outcome::INVALID, service,
                           NotificationService::MAX_RECIPIENTS);
    TEST_ASSERT_EQUAL_UINT(0, service.attempts(NotificationService::MAX_RECIPIENTS));
    tick(service, clock, 100000, true, true);
    TEST_ASSERT_EQUAL_UINT(NotificationService::MAX_RECIPIENTS, sender.calls.size());
}

void test_notification_rejects_reenqueue_and_restart_creates_a_new_startup_batch() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    const char* recipients[] = {"101"};
    {
        NotificationService service(sender, events, clock);
        TEST_ASSERT_TRUE(service.enqueue("first boot", recipients, 1));
        TEST_ASSERT_FALSE(service.enqueue("replacement", recipients, 1));
        tick(service, clock, 0, true, true);
        TEST_ASSERT_FALSE(service.enqueue("replacement", recipients, 1));
        TEST_ASSERT_EQUAL_STRING("first boot", sender.calls[0].text.c_str());
    }
    NotificationService restarted(sender, events, clock);
    assertOutcome(NotificationService::Outcome::EMPTY, restarted);
    TEST_ASSERT_TRUE(restarted.enqueue("second boot", recipients, 1));
    tick(restarted, clock, 0, true, true);
    TEST_ASSERT_EQUAL_UINT(2, sender.calls.size());
    TEST_ASSERT_EQUAL_STRING("second boot", sender.calls[1].text.c_str());
    TEST_ASSERT_EQUAL_UINT(1, restarted.attempts(0));
}

void test_notification_retry_and_send_gap_survive_millis_rollover() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    NotificationService service(sender, events, clock);
    sender.results = {false, true, true};
    const char* recipients[] = {"101", "102"};
    TEST_ASSERT_TRUE(service.enqueue("startup", recipients, 2));
    const uint32_t start = UINT32_MAX - NotificationService::SEND_GAP_MS / 2;
    tick(service, clock, start, true, true);
    tick(service, clock, start + NotificationService::SEND_GAP_MS - 1, true, true);
    TEST_ASSERT_EQUAL_UINT(1, sender.calls.size());
    tick(service, clock, start + NotificationService::SEND_GAP_MS, true, true);
    TEST_ASSERT_EQUAL_UINT(2, sender.calls.size());
    tick(service, clock, start + NotificationService::RETRY_MS - 1, true, true);
    TEST_ASSERT_EQUAL_UINT(2, sender.calls.size());
    tick(service, clock, start + NotificationService::RETRY_MS, true, true);
    TEST_ASSERT_EQUAL_UINT(3, sender.calls.size());
    assertOutcome(NotificationService::Outcome::DELIVERED, service);
}

void test_notification_retry_and_send_gap_start_after_blocking_send_completes() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    sender.clock = &clock;
    sender.blockingMs = 3 * NotificationService::RETRY_MS;
    sender.results = {false, true};
    NotificationService service(sender, events, clock);
    const char* recipient[] = {"101"};
    TEST_ASSERT_TRUE(service.enqueue("startup", recipient, 1));
    const uint32_t start = UINT32_MAX - NotificationService::RETRY_MS;
    tick(service, clock, start);
    const uint32_t completedAt = start + sender.blockingMs;
    TEST_ASSERT_EQUAL_UINT32(completedAt, clock.current);
    tick(service, clock, completedAt);
    tick(service, clock, completedAt + NotificationService::RETRY_MS - 1);
    TEST_ASSERT_EQUAL_UINT(1, sender.calls.size());
    tick(service, clock, completedAt + NotificationService::RETRY_MS);
    TEST_ASSERT_EQUAL_UINT(2, sender.calls.size());
    assertOutcome(NotificationService::Outcome::DELIVERED, service);

    FakeSender multiSender;
    FakeClock multiClock;
    multiSender.clock = &multiClock;
    multiSender.blockingMs = 3 * NotificationService::RETRY_MS;
    NotificationService multiService(multiSender, events, multiClock);
    const char* recipients[] = {"101", "102"};
    TEST_ASSERT_TRUE(multiService.enqueue("startup", recipients, 2));
    tick(multiService, multiClock, start);
    tick(multiService, multiClock, completedAt + NotificationService::SEND_GAP_MS - 1);
    TEST_ASSERT_EQUAL_UINT(1, multiSender.calls.size());
    tick(multiService, multiClock, completedAt + NotificationService::SEND_GAP_MS);
    TEST_ASSERT_EQUAL_UINT(2, multiSender.calls.size());
    assertOutcome(NotificationService::Outcome::DELIVERED, multiService);
}

void test_signal_startup_is_requested_without_io_and_finishes_at_boundary() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(10);
    TEST_ASSERT_TRUE(output.buzzer);
    service.update(10 + SignalController::STARTUP_MS - 1);
    TEST_ASSERT_TRUE(output.buzzer);
    service.update(10 + SignalController::STARTUP_MS);
    TEST_ASSERT_FALSE(output.buzzer);
    TEST_ASSERT_FALSE(output.led);
}

void test_signal_uses_saved_startup_duration_across_tick_wrap_without_shortening_led_window() {
    FakeOutput output;
    FakeButton button;
    DeviceSettings settings;
    settings.startupSoundSeconds = 3;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    const uint32_t start = UINT32_MAX - 1500;
    service.update(start);
    TEST_ASSERT_TRUE(output.buzzer);
    TEST_ASSERT_TRUE(output.led);
    service.update(start + 2999);
    TEST_ASSERT_TRUE(output.buzzer);
    service.update(start + 3000);
    TEST_ASSERT_FALSE(output.buzzer);
    TEST_ASSERT_FALSE(service.startupFinished());
    service.update(start + SignalController::STARTUP_MS);
    TEST_ASSERT_TRUE(service.startupFinished());
}

void test_signal_button_preheld_and_pressed_during_pattern_cancel_without_restarting() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.begin();
    button.pressed = true;
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(0);
    TEST_ASSERT_FALSE(output.buzzer);
    button.pressed = false;
    service.update(1);
    TEST_ASSERT_FALSE(output.buzzer);
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(2);
    TEST_ASSERT_TRUE(output.buzzer);
    service.onEvent(Event(EventType::WIFI_CONNECTED));
    button.pressed = true;
    service.update(3);
    TEST_ASSERT_FALSE(output.buzzer);
    button.pressed = false;
    service.update(4 + SignalController::STARTUP_MS);
    TEST_ASSERT_FALSE(output.buzzer);
}

void test_signal_connecting_pulse_and_connected_two_pulses_match_boundaries() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.begin();
    service.onEvent(Event(EventType::WIFI_START_CONNECT));
    service.onEvent(Event(EventType::WIFI_TRY_CONNECT));
    service.update(0);
    TEST_ASSERT_TRUE(output.led);
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(SignalController::CONNECT_PULSE_MS - 1);
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(SignalController::CONNECT_PULSE_MS);
    TEST_ASSERT_TRUE(output.buzzer);
    service.update(2 * SignalController::CONNECT_PULSE_MS - 1);
    TEST_ASSERT_TRUE(output.buzzer);
    service.update(2 * SignalController::CONNECT_PULSE_MS);
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(3 * SignalController::CONNECT_PULSE_MS);
    TEST_ASSERT_FALSE(output.buzzer);
    const uint32_t connectedAt = 1000;
    service.onEvent(Event(EventType::WIFI_CONNECTED));
    service.update(connectedAt);
    TEST_ASSERT_FALSE(output.led);
    TEST_ASSERT_FALSE(output.buzzer);
    for (uint32_t phase = 1; phase <= 6; ++phase) {
        service.update(connectedAt + phase * SignalController::CONNECTED_PULSE_MS - 1);
        TEST_ASSERT_EQUAL(phase == 2 || phase == 5, output.buzzer);
        service.update(connectedAt + phase * SignalController::CONNECTED_PULSE_MS);
        TEST_ASSERT_EQUAL(phase == 1 || phase == 4, output.buzzer);
    }
}

void test_signal_startup_has_priority_and_pending_connected_supersedes_connecting() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(0);
    service.onEvent(Event(EventType::WIFI_TRY_CONNECT));
    service.onEvent(Event(EventType::WIFI_CONNECTED));
    service.onEvent(Event(EventType::WIFI_TRY_CONNECT));
    service.update(SignalController::STARTUP_MS - 1);
    TEST_ASSERT_TRUE(output.buzzer);
    service.update(SignalController::STARTUP_MS);
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(SignalController::STARTUP_MS + 1);
    service.update(SignalController::STARTUP_MS + 1 + SignalController::CONNECT_PULSE_MS);
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(SignalController::STARTUP_MS + 1 + SignalController::CONNECTED_PULSE_MS);
    TEST_ASSERT_TRUE(output.buzzer);
    TEST_ASSERT_FALSE(output.led);
}

void test_signal_delivery_led_blinks_then_keeps_final_on_state() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.begin();
    service.onEvent(Event(EventType::WIFI_CONNECTED));
    service.update(0);
    TEST_ASSERT_FALSE(output.led);
    service.onEvent(Event(EventType::MESSAGE_SEND));
    service.update(10);
    TEST_ASSERT_FALSE(output.led);
    service.update(10 + SignalController::LED_PHASE_MS - 1);
    TEST_ASSERT_FALSE(output.led);
    service.update(10 + SignalController::LED_PHASE_MS);
    TEST_ASSERT_FALSE(output.led);
    service.update(10 + 2 * SignalController::LED_PHASE_MS - 1);
    TEST_ASSERT_FALSE(output.led);
    service.update(10 + 2 * SignalController::LED_PHASE_MS);
    TEST_ASSERT_TRUE(output.led);
    service.update(10 + 3 * SignalController::LED_PHASE_MS);
    TEST_ASSERT_TRUE(output.led);
    service.update(100000);
    TEST_ASSERT_TRUE(output.led);
    TEST_ASSERT_FALSE(output.buzzer);
}

void test_signal_startup_and_delivery_blink_survive_millis_rollover() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.begin();
    const uint32_t start = UINT32_MAX - SignalController::LED_PHASE_MS / 2;
    service.onEvent(Event(EventType::LIGHT_ON));
    service.onEvent(Event(EventType::MESSAGE_SEND));
    service.update(start);
    TEST_ASSERT_TRUE(output.buzzer);
    TEST_ASSERT_FALSE(output.led);
    service.update(start + SignalController::LED_PHASE_MS);
    TEST_ASSERT_FALSE(output.led);
    TEST_ASSERT_TRUE(output.buzzer);
    service.update(start + 3 * SignalController::LED_PHASE_MS);
    TEST_ASSERT_TRUE(output.led);
    service.update(start + SignalController::STARTUP_MS - 1);
    TEST_ASSERT_TRUE(output.buzzer);
    service.update(start + SignalController::STARTUP_MS);
    TEST_ASSERT_FALSE(output.buzzer);
}

void test_configured_signal_modes_follow_startup_waiting_and_idle_boundaries() {
    FakeOutput output;
    FakeButton button;
    DeviceSettings settings;
    settings.startupSound = false;
    settings.wifiProgressSound = false;
    settings.wifiConnectedSound = false;
    settings.startupLed = LedMode::BLINK;
    settings.waitingLed = LedMode::OFF;
    settings.idleLed = LedMode::OFF;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);
    service.begin();
    const uint32_t start = UINT32_MAX - 100;
    service.onEvent(Event(EventType::LIGHT_ON));
    service.onEvent(Event(EventType::WIFI_START_CONNECT));
    service.update(start);
    TEST_ASSERT_TRUE(output.led);
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(start + 499);
    TEST_ASSERT_TRUE(output.led);
    service.update(start + 500);
    TEST_ASSERT_FALSE(output.led);
    service.setOperatingState(SignalController::OperatingState::WAITING);
    service.update(start + SignalController::STARTUP_MS - 1);
    TEST_ASSERT_FALSE(service.startupFinished());
    service.update(start + SignalController::STARTUP_MS);
    TEST_ASSERT_TRUE(service.startupFinished());
    TEST_ASSERT_FALSE(output.led);
    service.setOperatingState(SignalController::OperatingState::IDLE);
    service.onEvent(Event(EventType::MESSAGE_SEND));
    service.update(start + SignalController::STARTUP_MS + 1);
    TEST_ASSERT_FALSE(output.led);
    service.update(start + SignalController::STARTUP_MS + 1001);
    TEST_ASSERT_TRUE(output.led);
    service.update(start + SignalController::STARTUP_MS + 1501);
    TEST_ASSERT_FALSE(output.led);
}

void test_configured_signal_switches_cancel_active_sound_and_global_led() {
    FakeOutput output;
    FakeButton button;
    DeviceSettings settings;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(0);
    TEST_ASSERT_TRUE(output.buzzer);
    TEST_ASSERT_TRUE(output.led);
    settings.startupSound = false;
    settings.ledEnabled = false;
    service.update(1);
    TEST_ASSERT_FALSE(output.buzzer);
    TEST_ASSERT_FALSE(output.led);
    service.onEvent(Event(EventType::WIFI_CONNECTED));
    service.onEvent(Event(EventType::MESSAGE_SEND));
    service.update(1000);
    TEST_ASSERT_FALSE(output.led);
    settings.wifiConnectedSound = false;
    service.update(1001);
    TEST_ASSERT_FALSE(output.buzzer);
    settings.startupSound = true;
    service.update(SignalController::STARTUP_MS);
    TEST_ASSERT_FALSE(output.buzzer);
}

void test_disabling_startup_sound_preserves_enabled_pending_wifi_success() {
    FakeOutput output;
    FakeButton button;
    DeviceSettings settings;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(0);
    service.onEvent(Event(EventType::WIFI_CONNECTED));
    settings.startupSound = false;
    service.update(1);
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(1 + SignalController::CONNECTED_PULSE_MS);
    TEST_ASSERT_TRUE(output.buzzer);
}

void test_configured_preview_respects_led_off_and_button_and_expires() {
    FakeOutput output;
    FakeButton button;
    DeviceSettings settings;
    settings.startupSound = false;
    settings.ledEnabled = false;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(0);
    service.update(SignalController::STARTUP_MS);
    TEST_ASSERT_FALSE(service.startPreview(SignalController::Preview::LED, LedMode::BLINK,
                                           SignalController::STARTUP_MS));
    TEST_ASSERT_TRUE(service.startPreview(SignalController::Preview::STARTUP, LedMode::OFF,
                                          SignalController::STARTUP_MS));
    service.update(SignalController::STARTUP_MS + 1);
    TEST_ASSERT_TRUE(output.buzzer);
    TEST_ASSERT_FALSE(output.led);
    button.pressed = true;
    service.update(SignalController::STARTUP_MS + 2);
    TEST_ASSERT_FALSE(output.buzzer);
    button.pressed = false;
    settings.ledEnabled = true;
    TEST_ASSERT_TRUE(
        service.startPreview(SignalController::Preview::LED, LedMode::BLINK, UINT32_MAX - 250));
    service.update(UINT32_MAX - 250);
    TEST_ASSERT_TRUE(output.led);
    service.update(250);
    TEST_ASSERT_FALSE(output.led);
    service.update(2750);
    TEST_ASSERT_FALSE(output.buzzer);
}

void test_wifi_transition_cancels_active_sound_preview() {
    FakeOutput output;
    FakeButton button;
    DeviceSettings settings;
    settings.startupSound = false;
    FakeTime wallTime;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(0);
    service.update(SignalController::STARTUP_MS);
    TEST_ASSERT_TRUE(service.startPreview(SignalController::Preview::STARTUP, LedMode::OFF,
                                          SignalController::STARTUP_MS));
    service.update(SignalController::STARTUP_MS + 1);
    TEST_ASSERT_TRUE(output.buzzer);
    service.onEvent(Event(EventType::WIFI_RECONNECT));
    service.update(SignalController::STARTUP_MS + 2);
    TEST_ASSERT_FALSE(output.buzzer);
}

void test_quiet_hours_cover_daytime_overnight_and_unknown_time() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    DeviceSettings settings;
    settings.quietHoursEnabled = true;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);

    TEST_ASSERT_TRUE(service.quietHoursActive());
    wallTime.valid = true;
    for (const uint8_t hour : {21, 7, 12}) {
        wallTime.hour = hour;
        TEST_ASSERT_FALSE(service.quietHoursActive());
    }
    for (const uint8_t hour : {22, 23, 0, 6}) {
        wallTime.hour = hour;
        TEST_ASSERT_TRUE(service.quietHoursActive());
    }
    settings.quietStartHour = 9;
    settings.quietEndHour = 17;
    for (const uint8_t hour : {8, 17, 23}) {
        wallTime.hour = hour;
        TEST_ASSERT_FALSE(service.quietHoursActive());
    }
    for (const uint8_t hour : {9, 12, 16}) {
        wallTime.hour = hour;
        TEST_ASSERT_TRUE(service.quietHoursActive());
    }
    settings.quietHoursEnabled = false;
    wallTime.valid = false;
    TEST_ASSERT_FALSE(service.quietHoursActive());
}

void test_quiet_hours_stop_active_sound_without_replay_and_block_sound_preview() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    wallTime.valid = true;
    wallTime.hour = 21;
    DeviceSettings settings;
    settings.quietHoursEnabled = true;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(0);
    TEST_ASSERT_TRUE(output.buzzer);
    TEST_ASSERT_TRUE(output.led);

    wallTime.hour = 22;
    service.update(500);
    TEST_ASSERT_FALSE(output.buzzer);
    service.update(SignalController::STARTUP_MS);
    TEST_ASSERT_FALSE(service.startPreview(SignalController::Preview::STARTUP, LedMode::OFF,
                                           SignalController::STARTUP_MS));
    TEST_ASSERT_TRUE(service.startPreview(SignalController::Preview::LED, LedMode::BLINK,
                                          SignalController::STARTUP_MS));

    wallTime.hour = 7;
    service.update(SignalController::STARTUP_MS + 1);
    TEST_ASSERT_FALSE(output.buzzer);
    service.onEvent(Event(EventType::WIFI_CONNECTED));
    service.update(SignalController::STARTUP_MS + 2);
    service.update(SignalController::STARTUP_MS + 2 + SignalController::CONNECTED_PULSE_MS);
    TEST_ASSERT_TRUE(output.buzzer);
}

void test_quiet_hours_mute_startup_until_clock_is_ready_without_affecting_led() {
    FakeOutput output;
    FakeButton button;
    FakeTime wallTime;
    DeviceSettings settings;
    settings.quietHoursEnabled = true;
    SignalController service(output, button, wallTime);
    service.setSettings(settings);
    service.begin();
    service.onEvent(Event(EventType::LIGHT_ON));
    service.update(0);
    TEST_ASSERT_FALSE(output.buzzer);
    TEST_ASSERT_TRUE(output.led);
    wallTime.valid = true;
    wallTime.hour = 12;
    service.update(1);
    TEST_ASSERT_FALSE(output.buzzer);
}

void test_device_settings_validate_network_telegram_and_signal_modes() {
    DeviceSettings settings;
    strcpy(settings.ssid, "network");
    strcpy(settings.ip, "192.168.1.20");
    strcpy(settings.gateway, "192.168.1.1");
    strcpy(settings.subnet, "255.255.255.0");
    strcpy(settings.botToken, "123:abc");
    strcpy(settings.message, "Light on");
    strcpy(settings.recipients[0], "123");
    settings.recipientCount = 1;
    TEST_ASSERT_TRUE(validNetworkSettings(settings));
    TEST_ASSERT_TRUE(validTelegramSettings(settings));
    TEST_ASSERT_TRUE(validSignalSettings(settings));
    settings.startupSoundSeconds = 0;
    TEST_ASSERT_FALSE(validSignalSettings(settings));
    settings.startupSoundSeconds = 60;
    TEST_ASSERT_TRUE(validSignalSettings(settings));
    settings.startupSoundSeconds = 61;
    TEST_ASSERT_FALSE(validSignalSettings(settings));
    settings.startupSoundSeconds = 10;
    settings.quietHoursEnabled = true;
    settings.quietStartHour = 24;
    TEST_ASSERT_FALSE(validSignalSettings(settings));
    settings.quietStartHour = 22;
    settings.quietEndHour = 22;
    TEST_ASSERT_FALSE(validSignalSettings(settings));
    settings.quietEndHour = 7;
    TEST_ASSERT_TRUE(validSignalSettings(settings));
    settings.startupLed = static_cast<LedMode>(3);
    TEST_ASSERT_FALSE(validSignalSettings(settings));
    settings.startupLed = LedMode::STEADY;
    strcpy(settings.recipients[0], "000123");
    TEST_ASSERT_FALSE(validTelegramSettings(settings));
    strcpy(settings.recipients[0], "123");
    strcpy(settings.gateway, "192.168.2.1");
    TEST_ASSERT_FALSE(validNetworkSettings(settings));
}

void test_application_services_ota_during_startup_and_missing_ntp_then_delivers() {
    EventNotifier events;
    FakeConnection network;
    FakeTime clock;
    FakeSender sender;
    FakeOutput output;
    FakeButton button;
    FakeOta ota;
    ConnectionService connection(network, events);
    TimeService time(clock, events);
    FakeClock monotonicClock;
    NotificationService notifications(sender, events, monotonicClock);
    SignalController signals(output, button, clock);
    TEST_ASSERT_TRUE(events.addObserver(&signals));
    NotifierApplication app(connection, time, notifications, signals, ota, events);
    const char* recipients[] = {"101"};
    tickApp(app, monotonicClock, 0);
    TEST_ASSERT_EQUAL_UINT(0, network.starts);
    app.begin(0, "startup", recipients, 1);
    app.begin(1, "replacement", recipients, 1);
    TEST_ASSERT_EQUAL_UINT(1, network.starts);
    TEST_ASSERT_TRUE(output.buzzer);
    tickApp(app, monotonicClock, 1);
    TEST_ASSERT_EQUAL_UINT(0, ota.handles);
    network.connected = true;
    tickApp(app, monotonicClock, 2);
    tickApp(app, monotonicClock, 3);
    TEST_ASSERT_EQUAL_UINT(1, ota.starts);
    TEST_ASSERT_EQUAL_UINT(2, ota.handles);
    TEST_ASSERT_TRUE(output.buzzer);
    TEST_ASSERT_EQUAL_UINT(0, sender.calls.size());
    TEST_ASSERT_EQUAL_UINT(0, notifications.attempts(0));
    button.pressed = true;
    tickApp(app, monotonicClock, 4);
    TEST_ASSERT_FALSE(output.buzzer);
    TEST_ASSERT_EQUAL_UINT(3, ota.handles);
    clock.valid = true;
    tickApp(app, monotonicClock, 5);
    TEST_ASSERT_EQUAL_UINT(1, sender.calls.size());
    TEST_ASSERT_EQUAL_STRING("startup", sender.calls[0].text.c_str());
    network.connected = false;
    tickApp(app, monotonicClock, 6);
    TEST_ASSERT_EQUAL_UINT(4, ota.handles);
    TEST_ASSERT_EQUAL_UINT(2, network.starts);
    network.connected = true;
    tickApp(app, monotonicClock, 7);
    TEST_ASSERT_EQUAL_UINT(1, ota.starts);
    TEST_ASSERT_EQUAL_UINT(5, ota.handles);
    TEST_ASSERT_EQUAL_UINT(1, sender.calls.size());
}

void test_application_missing_wifi_still_finishes_signals_and_retries_without_sending() {
    EventNotifier events;
    FakeConnection network;
    FakeTime clock;
    FakeSender sender;
    FakeOutput output;
    FakeButton button;
    FakeOta ota;
    ConnectionService connection(network, events);
    TimeService time(clock, events);
    FakeClock monotonicClock;
    NotificationService notifications(sender, events, monotonicClock);
    SignalController signals(output, button, clock);
    TEST_ASSERT_TRUE(events.addObserver(&signals));
    NotifierApplication app(connection, time, notifications, signals, ota, events);
    const char* recipients[] = {"101"};
    app.begin(0, "startup", recipients, 1);
    tickApp(app, monotonicClock, SignalController::STARTUP_MS);
    TEST_ASSERT_FALSE(output.buzzer);
    tickApp(app, monotonicClock, ConnectionService::ATTEMPT_MS);
    tickApp(app, monotonicClock, ConnectionService::ATTEMPT_MS + ConnectionService::RETRY_MS);
    TEST_ASSERT_EQUAL_UINT(2, network.starts);
    TEST_ASSERT_EQUAL_UINT(0, clock.starts);
    TEST_ASSERT_EQUAL_UINT(0, ota.starts);
    TEST_ASSERT_EQUAL_UINT(0, sender.calls.size());
    assertOutcome(NotificationService::Outcome::WAITING, notifications);
}

bool appendResponse(HttpResponse& response, const std::string& text) {
    for (const char value : text) {
        if (!response.append(value)) {
            return false;
        }
    }
    return true;
}

std::string framedResponse(const std::string& body, unsigned status = 200) {
    return "HTTP/1.1 " + std::to_string(status) +
           " Response\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

void test_http_content_length_completes_without_waiting_for_connection_close() {
    HttpResponse response;
    TEST_ASSERT_TRUE(response.isAvailable());
    const std::string body = "{\"ok\":true}";
    const std::string wire = framedResponse(body);
    TEST_ASSERT_TRUE(appendResponse(response, wire.substr(0, wire.size() - 1)));
    TEST_ASSERT_FALSE(response.isComplete());
    TEST_ASSERT_TRUE(response.append(wire.back()));
    TEST_ASSERT_TRUE(response.isComplete());
    TEST_ASSERT_TRUE(response.isValid());
    TEST_ASSERT_TRUE(response.isSuccessStatus());
    TEST_ASSERT_EQUAL_INT(200, response.statusCode());
    TEST_ASSERT_EQUAL_UINT(body.size(), response.bodySize());
    TEST_ASSERT_EQUAL_STRING(body.c_str(), response.body());
    TEST_ASSERT_TRUE(isAcknowledged(response));
}

void test_http_connection_closed_body_and_non_success_status_are_distinguished() {
    HttpResponse response;
    TEST_ASSERT_TRUE(appendResponse(response, "HTTP/1.0 200 OK\r\n\r\n{\"ok\":true}"));
    TEST_ASSERT_FALSE(response.isComplete());
    TEST_ASSERT_FALSE(isAcknowledged(response));
    response.finish();
    TEST_ASSERT_TRUE(response.isValid());
    TEST_ASSERT_TRUE(isAcknowledged(response));
    const unsigned errors[] = {201, 301, 400, 429, 500};
    for (const auto status : errors) {
        HttpResponse failure;
        TEST_ASSERT_TRUE(appendResponse(failure, framedResponse("{\"ok\":true}", status)));
        TEST_ASSERT_TRUE(failure.isValid());
        TEST_ASSERT_FALSE(failure.isSuccessStatus());
        TEST_ASSERT_EQUAL_INT(status, failure.statusCode());
        TEST_ASSERT_FALSE(isAcknowledged(failure));
    }
}

void test_http_rejects_malformed_truncated_duplicate_lengths_and_transfer_encoding() {
    const char* invalid[] = {"HTTP/2 200 OK\r\n\r\n",
                             "HTTP/1.1 200X OK\r\n\r\n",
                             "HTTP/1.1 200 OK\r\nMalformed\r\n\r\n",
                             "HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n",
                             "HTTP/1.1 200 OK\r\nContent-Length: 1x\r\n\r\n",
                             "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx",
                             "HTTP/1.1 200 OK\r\ntransfer-encoding: chunked\r\n\r\n",
                             "HTTP/1.1 200 OK\r\nTransfer-Encoding: identity\r\n\r\n",
                             "HTTP/1.1 200 OK\r\nContent-Length: 999999999999999999\r\n\r\n",
                             "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort",
                             "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"};
    for (const auto* wire : invalid) {
        HttpResponse response;
        appendResponse(response, wire);
        response.finish();
        TEST_ASSERT_FALSE(response.isValid());
        TEST_ASSERT_FALSE(isAcknowledged(response));
    }
    HttpResponse response;
    TEST_ASSERT_TRUE(appendResponse(response, "HTTP/1.1 200 OK\r\n\r\n"));
    TEST_ASSERT_FALSE(response.append('\0'));
    response.finish();
    TEST_ASSERT_FALSE(response.isValid());
}

void test_http_total_buffer_boundary_accepts_exact_capacity_and_rejects_overflow() {
    const std::string headers = "HTTP/1.0 200 OK\r\n\r\n";
    HttpResponse exact;
    const std::string wire = headers + std::string(HttpResponse::MAX_BYTES - headers.size(), 'x');
    TEST_ASSERT_TRUE(appendResponse(exact, wire));
    exact.finish();
    TEST_ASSERT_TRUE(exact.isValid());
    TEST_ASSERT_EQUAL_UINT(HttpResponse::MAX_BYTES - headers.size(), exact.bodySize());
    HttpResponse oversized;
    TEST_ASSERT_TRUE(appendResponse(oversized, wire));
    TEST_ASSERT_FALSE(oversized.append('x'));
    oversized.finish();
    TEST_ASSERT_FALSE(oversized.isValid());
    HttpResponse declaredOversized;
    appendResponse(declaredOversized, "HTTP/1.1 200 OK\r\nContent-Length: " +
                                          std::to_string(HttpResponse::MAX_BYTES) + "\r\n\r\n");
    declaredOversized.finish();
    TEST_ASSERT_FALSE(declaredOversized.isValid());
}

void test_telegram_acknowledgement_requires_boolean_true_and_rejects_invalid_json() {
    const char* invalidBodies[] = {"{\"ok\":false}",
                                   "{\"ok\":1}",
                                   "{\"ok\":\"true\"}",
                                   "{\"ok\":null}",
                                   "{\"ok\":{}}",
                                   "{}",
                                   "true",
                                   "[]",
                                   "{\"ok\":true",
                                   "{\"ok\":true}garbage",
                                   "{\"ok\":true}{\"ok\":false}",
                                   "{\"ok\":true,\"result\":bad}",
                                   ""};
    for (const auto* body : invalidBodies) {
        HttpResponse response;
        TEST_ASSERT_TRUE(appendResponse(response, framedResponse(body)));
        TEST_ASSERT_TRUE(response.isValid());
        TEST_ASSERT_FALSE_MESSAGE(isAcknowledged(response), body);
    }
    HttpResponse whitespace;
    TEST_ASSERT_TRUE(appendResponse(whitespace, framedResponse(" {\"ok\":true}\r\n\t ")));
    TEST_ASSERT_TRUE(isAcknowledged(whitespace));
}

void test_telegram_acknowledgement_filters_maximum_escaped_message_and_chat_metadata() {
    std::string escapedMessage;
    for (size_t index = 0; index < NotificationService::MAX_MESSAGE_BYTES; ++index) {
        escapedMessage += "\\u0001";
    }
    const std::string body = "{\"ok\":true,\"result\":{\"message_id\":100,\"chat\":{\"id\":-123,"
                             "\"type\":\"supergroup\",\"title\":\"Example\"},\"text\":\"" +
                             escapedMessage + "\",\"date\":1700000000}}";
    HttpResponse response;
    TEST_ASSERT_TRUE(appendResponse(response, framedResponse(body)));
    TEST_ASSERT_TRUE(response.isValid());
    TEST_ASSERT_TRUE(isAcknowledged(response));
    TEST_ASSERT_EQUAL_UINT(body.size(), response.bodySize());
}

class FakeTelegramConnection : public TelegramConnectPort {
public:
    bool dnsOk = true;
    bool tcpOk = true;
    bool handshakeOk = true;
    bool mflnOk = true;
    int error = 0;
    std::vector<std::string> calls;

    bool resolve(const char* host, uint32_t timeoutMs) override {
        calls.push_back(std::string("dns:") + host + ":" + std::to_string(timeoutMs));
        return dnsOk;
    }
    void setIoTimeout(uint32_t timeoutMs) override {
        calls.push_back("timeout:" + std::to_string(timeoutMs));
    }
    bool connectTcp() override {
        calls.push_back("tcp");
        return tcpOk;
    }
    bool handshake(const char* host) override {
        calls.push_back(std::string("tls:") + host);
        return handshakeOk;
    }
    int handshakeError() override {
        calls.push_back("tls-error");
        return error;
    }
    bool mflnAccepted() override {
        calls.push_back("mfln");
        return mflnOk;
    }
    void close() override {
        calls.push_back("close");
    }
};

void test_telegram_tls_requires_mfln_and_closes_rejected_connection() {
    FakeTelegramConnection client;
    const auto result = connectTelegram(client, "api.telegram.org", 1000, 1000, 1000);
    TEST_ASSERT_TRUE(result.connected());
    TEST_ASSERT_EQUAL_INT(0, result.tlsError);
    const std::vector<std::string> expected = {
        "dns:api.telegram.org:1000", "timeout:1000", "tcp",
        "tls:api.telegram.org",      "mfln",         "timeout:1000"};
    TEST_ASSERT_TRUE(client.calls == expected);

    client.calls.clear();
    client.mflnOk = false;
    const auto rejected = connectTelegram(client, "api.telegram.org", 1000, 1000, 1000);
    TEST_ASSERT_FALSE(rejected.connected());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelegramConnectFailure::TLS_FRAGMENT),
                          static_cast<int>(rejected.failure));
    TEST_ASSERT_EQUAL_STRING("close", client.calls.back().c_str());
}

void test_telegram_tls_allocation_failure_closes_socket_and_preserves_error() {
    FakeTelegramConnection client;
    client.handshakeOk = false;
    client.error = -1000;
    const auto result = connectTelegram(client, "api.telegram.org", 1000, 1000, 1000);
    TEST_ASSERT_FALSE(result.connected());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelegramConnectFailure::TLS),
                          static_cast<int>(result.failure));
    TEST_ASSERT_EQUAL_INT(-1000, result.tlsError);
    TEST_ASSERT_EQUAL_STRING("close", client.calls.back().c_str());
    TEST_ASSERT_TRUE(std::find(client.calls.begin(), client.calls.end(), "mfln") ==
                     client.calls.end());
}

void test_telegram_dns_and_tcp_fail_before_tls() {
    FakeTelegramConnection client;
    client.dnsOk = false;
    const auto dns = connectTelegram(client, "api.telegram.org", 1000, 1000, 1000);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelegramConnectFailure::DNS),
                          static_cast<int>(dns.failure));
    TEST_ASSERT_EQUAL_UINT(1, client.calls.size());

    client.calls.clear();
    client.dnsOk = true;
    client.tcpOk = false;
    const auto tcp = connectTelegram(client, "api.telegram.org", 1000, 1000, 1000);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelegramConnectFailure::TCP),
                          static_cast<int>(tcp.failure));
    TEST_ASSERT_EQUAL_STRING("tcp", client.calls.back().c_str());
}

void test_static_network_parses_valid_addresses_and_contiguous_subnet() {
    StaticNetworkConfig result = {};
    TEST_ASSERT_TRUE(parseStaticNetwork("192.168.1.20", "192.168.1.1", "255.255.255.0", result));
    const uint8_t address[] = {192, 168, 1, 20};
    const uint8_t gateway[] = {192, 168, 1, 1};
    const uint8_t subnet[] = {255, 255, 255, 0};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(address, result.address, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(gateway, result.gateway, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(subnet, result.subnet, 4);
    TEST_ASSERT_TRUE(parseStaticNetwork("10.1.2.1", "10.1.2.2", "255.255.255.252", result));
    TEST_ASSERT_TRUE(parseStaticNetwork("172.16.8.20", "172.16.0.1", "255.255.0.0", result));
}

void test_static_network_rejects_invalid_settings_atomically_without_dhcp_fallback() {
    struct Settings {
        const char* address;
        const char* gateway;
        const char* subnet;
    };
    const Settings invalid[] = {{nullptr, "192.168.1.1", "255.255.255.0"},
                                {"", "192.168.1.1", "255.255.255.0"},
                                {"192.168.1.256", "192.168.1.1", "255.255.255.0"},
                                {"192.168.1", "192.168.1.1", "255.255.255.0"},
                                {"192.168.1.20.1", "192.168.1.1", "255.255.255.0"},
                                {"192..1.20", "192.168.1.1", "255.255.255.0"},
                                {"192.168.1.-1", "192.168.1.1", "255.255.255.0"},
                                {"192.168.1.20 ", "192.168.1.1", "255.255.255.0"},
                                {"192.168.1.20", nullptr, "255.255.255.0"},
                                {"192.168.1.20", "192.168.1.1", nullptr},
                                {"192.168.1.20", "192.168.1.1", "255.0.255.0"},
                                {"192.168.1.20", "192.168.1.1", "0.0.0.0"},
                                {"192.168.1.20", "192.168.1.1", "255.255.255.255"},
                                {"192.168.1.0", "192.168.1.1", "255.255.255.0"},
                                {"192.168.1.255", "192.168.1.1", "255.255.255.0"},
                                {"192.168.1.20", "192.168.1.0", "255.255.255.0"},
                                {"192.168.1.20", "192.168.1.255", "255.255.255.0"},
                                {"192.168.1.20", "192.168.2.1", "255.255.255.0"},
                                {"192.168.1.20", "192.168.1.20", "255.255.255.0"},
                                {"224.0.0.20", "224.0.0.1", "255.255.255.0"},
                                {"0.0.0.20", "0.0.0.1", "255.255.255.0"}};
    for (const auto& input : invalid) {
        StaticNetworkConfig result;
        memset(&result, 77, sizeof(result));
        const StaticNetworkConfig before = result;
        TEST_ASSERT_FALSE(parseStaticNetwork(input.address, input.gateway, input.subnet, result));
        TEST_ASSERT_EQUAL_MEMORY(&before, &result, sizeof(result));
    }
}

void test_notification_rejects_noncanonical_numeric_ids_and_duplicate_channel_names() {
    const char* invalid[] = {"0", "-0", "01", "-01", "@abcd", "@bad!name", "@name space"};
    for (const auto* id : invalid) {
        const char* recipient[] = {id};
        assertInvalidBatch("startup", recipient, 1);
    }
    const char* duplicates[] = {"@Example_Name", "@example_name"};
    assertInvalidBatch("startup", duplicates, 2);
    std::string oversized = "@" + std::string(NotificationService::MAX_RECIPIENT_BYTES, 'a');
    const char* recipient[] = {oversized.c_str()};
    assertInvalidBatch("startup", recipient, 1);
}

void test_notification_accepts_full_32_character_channel_name_and_preserves_case() {
    FakeSender sender;
    FakeEvents events;
    FakeClock clock;
    NotificationService service(sender, events, clock);
    const std::string fullName = "@" + std::string(32, 'A');
    const char* recipients[] = {fullName.c_str(), "@abc_1", "-100123"};
    TEST_ASSERT_TRUE(service.enqueue("startup", recipients, 3));
    tick(service, clock, 0);
    tick(service, clock, NotificationService::SEND_GAP_MS);
    tick(service, clock, 2 * NotificationService::SEND_GAP_MS);
    TEST_ASSERT_EQUAL_UINT(3, sender.calls.size());
    TEST_ASSERT_EQUAL_STRING(fullName.c_str(), sender.calls[0].recipient.c_str());
    TEST_ASSERT_EQUAL_STRING("@abc_1", sender.calls[1].recipient.c_str());
    TEST_ASSERT_EQUAL_STRING("-100123", sender.calls[2].recipient.c_str());
    assertOutcome(NotificationService::Outcome::DELIVERED, service);
}

DeviceSettings validStoredSettings() {
    DeviceSettings settings = {};
    strcpy(settings.ssid, "network");
    strcpy(settings.ip, "192.168.1.20");
    strcpy(settings.gateway, "192.168.1.1");
    strcpy(settings.subnet, "255.255.255.0");
    strcpy(settings.botToken, "123:abc");
    strcpy(settings.message, "Light on");
    strcpy(settings.recipients[0], "123");
    settings.recipientCount = 1;
    settings.configured = true;
    return settings;
}

void resetFakeStorage() {
    LittleFS.files.clear();
    LittleFS.failWrite = false;
    LittleFS.failRename = false;
    memset(EEPROM.bytes, 0, sizeof(EEPROM.bytes));
    EEPROM.failCommit = false;
}

void test_settings_store_preserves_previous_record_after_interrupted_replacement() {
    resetFakeStorage();
    SettingsStore firstBoot;
    TEST_ASSERT_TRUE(firstBoot.begin());
    DeviceSettings original = validStoredSettings();
    TEST_ASSERT_TRUE(firstBoot.save(original));
    TEST_ASSERT_TRUE(firstBoot.wasProvisioned());
    DeviceSettings replacement = original;
    replacement.revision = 2;
    strcpy(replacement.message, "Revised");
    LittleFS.failRename = true;
    TEST_ASSERT_FALSE(firstBoot.save(replacement));
    LittleFS.failRename = false;
    SettingsStore reboot;
    TEST_ASSERT_TRUE(reboot.begin());
    DeviceSettings loaded = {};
    TEST_ASSERT_TRUE(reboot.load(loaded));
    TEST_ASSERT_EQUAL_STRING("Light on", loaded.message);
    TEST_ASSERT_TRUE(reboot.wasProvisioned());
    TEST_ASSERT_TRUE(reboot.save(replacement));
    SettingsStore nextBoot;
    TEST_ASSERT_TRUE(nextBoot.begin());
    TEST_ASSERT_TRUE(nextBoot.load(loaded));
    TEST_ASSERT_EQUAL_STRING("Revised", loaded.message);
}

void test_settings_store_rejects_future_schema_until_explicit_reset() {
    resetFakeStorage();
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    DeviceSettings original = validStoredSettings();
    TEST_ASSERT_TRUE(store.save(original));
    auto& record = LittleFS.files["/settings-a.bin"];
    const uint32_t future = DeviceSettings::SCHEMA + 1;
    memcpy(record.data() + 12, &future, sizeof(future));
    SettingsStore reboot;
    TEST_ASSERT_TRUE(reboot.begin());
    DeviceSettings loaded = {};
    TEST_ASSERT_FALSE(reboot.load(loaded));
    TEST_ASSERT_TRUE(reboot.hasUnsupportedSchema());
    TEST_ASSERT_FALSE(reboot.save(original));
    DeviceSettings empty = {};
    empty.revision = 2;
    TEST_ASSERT_TRUE(reboot.reset(empty));
    SettingsStore afterReset;
    TEST_ASSERT_TRUE(afterReset.begin());
    TEST_ASSERT_TRUE(afterReset.load(loaded));
    TEST_ASSERT_FALSE(loaded.configured);
    TEST_ASSERT_TRUE(afterReset.wasProvisioned());
}

void test_settings_store_requires_durable_provisioning_marker_before_first_record() {
    resetFakeStorage();
    EEPROM.failCommit = true;
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    DeviceSettings settings = validStoredSettings();
    TEST_ASSERT_FALSE(store.save(settings));
    TEST_ASSERT_FALSE(store.hasRecords());
    EEPROM.failCommit = false;
    TEST_ASSERT_TRUE(store.save(settings));
}

void test_settings_store_migrates_version_one_without_losing_saved_configuration() {
    resetFakeStorage();
    DeviceSettings oldSettings = validStoredSettings();
    oldSettings.schema = 1;
    constexpr size_t oldSize = 1132;
    const auto* bytes = reinterpret_cast<const uint8_t*>(&oldSettings);
    uint32_t checksum = 2166136261UL;
    for (size_t i = 0; i < oldSize; ++i) {
        checksum = (checksum ^ bytes[i]) * 16777619UL;
    }
    uint32_t header[] = {0x4c4f4e31, oldSize, checksum};
    auto& file = LittleFS.files["/settings-a.bin"];
    file.resize(sizeof(header) + oldSize);
    memcpy(file.data(), header, sizeof(header));
    memcpy(file.data() + sizeof(header), bytes, oldSize);

    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    DeviceSettings loaded;
    TEST_ASSERT_TRUE(store.load(loaded));
    TEST_ASSERT_EQUAL_UINT32(DeviceSettings::SCHEMA, loaded.schema);
    TEST_ASSERT_EQUAL_UINT32(10, loaded.startupSoundSeconds);
    TEST_ASSERT_FALSE(loaded.quietHoursEnabled);
    TEST_ASSERT_EQUAL_UINT8(22, loaded.quietStartHour);
    TEST_ASSERT_EQUAL_UINT8(7, loaded.quietEndHour);
    TEST_ASSERT_EQUAL_STRING(oldSettings.ssid, loaded.ssid);
    TEST_ASSERT_EQUAL_STRING(oldSettings.message, loaded.message);
    loaded.revision = 2;
    loaded.startupSoundSeconds = 25;
    TEST_ASSERT_TRUE(store.save(loaded));

    SettingsStore reboot;
    TEST_ASSERT_TRUE(reboot.begin());
    DeviceSettings reloaded;
    TEST_ASSERT_TRUE(reboot.load(reloaded));
    TEST_ASSERT_EQUAL_UINT32(25, reloaded.startupSoundSeconds);
    TEST_ASSERT_EQUAL_STRING(oldSettings.ssid, reloaded.ssid);
}

void test_settings_store_migrates_version_two_with_quiet_hours_disabled() {
    resetFakeStorage();
    DeviceSettings oldSettings = validStoredSettings();
    oldSettings.schema = 2;
    oldSettings.startupSoundSeconds = 25;
    constexpr size_t oldSize = 1136;
    const auto* bytes = reinterpret_cast<const uint8_t*>(&oldSettings);
    uint32_t checksum = 2166136261UL;
    for (size_t i = 0; i < oldSize; ++i) {
        checksum = (checksum ^ bytes[i]) * 16777619UL;
    }
    const uint32_t header[] = {0x4c4f4e31, oldSize, checksum};
    auto& file = LittleFS.files["/settings-a.bin"];
    file.resize(sizeof(header) + oldSize);
    memcpy(file.data(), header, sizeof(header));
    memcpy(file.data() + sizeof(header), bytes, oldSize);

    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    DeviceSettings loaded;
    TEST_ASSERT_TRUE(store.load(loaded));
    TEST_ASSERT_EQUAL_UINT32(DeviceSettings::SCHEMA, loaded.schema);
    TEST_ASSERT_EQUAL_UINT32(25, loaded.startupSoundSeconds);
    TEST_ASSERT_FALSE(loaded.quietHoursEnabled);
    TEST_ASSERT_EQUAL_UINT8(22, loaded.quietStartHour);
    TEST_ASSERT_EQUAL_UINT8(7, loaded.quietEndHour);
    TEST_ASSERT_EQUAL_UINT(sizeof(header) + oldSize, file.size());

    loaded.revision = 2;
    loaded.quietHoursEnabled = true;
    TEST_ASSERT_TRUE(store.save(loaded));
    SettingsStore reboot;
    TEST_ASSERT_TRUE(reboot.begin());
    DeviceSettings reloaded;
    TEST_ASSERT_TRUE(reboot.load(reloaded));
    TEST_ASSERT_TRUE(reloaded.quietHoursEnabled);
    TEST_ASSERT_EQUAL_UINT8(22, reloaded.quietStartHour);
    TEST_ASSERT_EQUAL_UINT8(7, reloaded.quietEndHour);
}

} // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_web_field_accepts_valid_wifi_values_and_rejects_oversize_or_embedded_null);
    RUN_TEST(test_dispatch_rejects_null_duplicate_and_capacity_then_removes_in_order);
    RUN_TEST(test_dispatch_rejects_mutation_and_nesting_and_borrows_payload_synchronously);
    RUN_TEST(test_connection_invalid_configuration_never_starts_or_retries);
    RUN_TEST(test_connection_reconfigure_recovers_invalid_settings_without_reboot);
    RUN_TEST(test_connection_unavailable_expires_and_retries_at_interval_boundaries);
    RUN_TEST(test_connection_loss_retries_immediately_and_recovery_publishes_once);
    RUN_TEST(test_connection_retry_timing_survives_millis_rollover);
    RUN_TEST(test_time_waits_offline_and_retries_without_waiting_then_accepts_late_time);
    RUN_TEST(test_time_regression_revokes_readiness_and_reconnect_requests_sync);
    RUN_TEST(test_time_retry_interval_survives_millis_rollover);
    RUN_TEST(test_notification_keeps_pending_until_network_and_time_are_ready_and_copies_inputs);
    RUN_TEST(test_partial_delivery_never_resends_acknowledged_recipient);
    RUN_TEST(test_notification_exhausts_exactly_five_attempts_and_keeps_other_success);
    RUN_TEST(test_notification_disconnect_and_invalid_time_do_not_consume_retries);
    RUN_TEST(test_notification_rejects_missing_malformed_duplicate_and_oversized_inputs);
    RUN_TEST(test_notification_accepts_storage_boundaries_and_sends_all_eight_once);
    RUN_TEST(test_notification_rejects_reenqueue_and_restart_creates_a_new_startup_batch);
    RUN_TEST(test_notification_retry_and_send_gap_survive_millis_rollover);
    RUN_TEST(test_notification_retry_and_send_gap_start_after_blocking_send_completes);
    RUN_TEST(test_signal_startup_is_requested_without_io_and_finishes_at_boundary);
    RUN_TEST(
        test_signal_uses_saved_startup_duration_across_tick_wrap_without_shortening_led_window);
    RUN_TEST(test_signal_button_preheld_and_pressed_during_pattern_cancel_without_restarting);
    RUN_TEST(test_signal_connecting_pulse_and_connected_two_pulses_match_boundaries);
    RUN_TEST(test_signal_startup_has_priority_and_pending_connected_supersedes_connecting);
    RUN_TEST(test_signal_delivery_led_blinks_then_keeps_final_on_state);
    RUN_TEST(test_signal_startup_and_delivery_blink_survive_millis_rollover);
    RUN_TEST(test_configured_signal_modes_follow_startup_waiting_and_idle_boundaries);
    RUN_TEST(test_configured_signal_switches_cancel_active_sound_and_global_led);
    RUN_TEST(test_disabling_startup_sound_preserves_enabled_pending_wifi_success);
    RUN_TEST(test_configured_preview_respects_led_off_and_button_and_expires);
    RUN_TEST(test_wifi_transition_cancels_active_sound_preview);
    RUN_TEST(test_quiet_hours_cover_daytime_overnight_and_unknown_time);
    RUN_TEST(test_quiet_hours_stop_active_sound_without_replay_and_block_sound_preview);
    RUN_TEST(test_quiet_hours_mute_startup_until_clock_is_ready_without_affecting_led);
    RUN_TEST(test_device_settings_validate_network_telegram_and_signal_modes);
    RUN_TEST(test_application_services_ota_during_startup_and_missing_ntp_then_delivers);
    RUN_TEST(test_application_missing_wifi_still_finishes_signals_and_retries_without_sending);
    RUN_TEST(test_http_content_length_completes_without_waiting_for_connection_close);
    RUN_TEST(test_http_connection_closed_body_and_non_success_status_are_distinguished);
    RUN_TEST(test_http_rejects_malformed_truncated_duplicate_lengths_and_transfer_encoding);
    RUN_TEST(test_http_total_buffer_boundary_accepts_exact_capacity_and_rejects_overflow);
    RUN_TEST(test_telegram_acknowledgement_requires_boolean_true_and_rejects_invalid_json);
    RUN_TEST(test_telegram_acknowledgement_filters_maximum_escaped_message_and_chat_metadata);
    RUN_TEST(test_telegram_tls_requires_mfln_and_closes_rejected_connection);
    RUN_TEST(test_telegram_tls_allocation_failure_closes_socket_and_preserves_error);
    RUN_TEST(test_telegram_dns_and_tcp_fail_before_tls);
    RUN_TEST(test_static_network_parses_valid_addresses_and_contiguous_subnet);
    RUN_TEST(test_static_network_rejects_invalid_settings_atomically_without_dhcp_fallback);
    RUN_TEST(test_notification_rejects_noncanonical_numeric_ids_and_duplicate_channel_names);
    RUN_TEST(test_notification_accepts_full_32_character_channel_name_and_preserves_case);
    RUN_TEST(test_settings_store_preserves_previous_record_after_interrupted_replacement);
    RUN_TEST(test_settings_store_rejects_future_schema_until_explicit_reset);
    RUN_TEST(test_settings_store_requires_durable_provisioning_marker_before_first_record);
    RUN_TEST(test_settings_store_migrates_version_one_without_losing_saved_configuration);
    RUN_TEST(test_settings_store_migrates_version_two_with_quiet_hours_disabled);
    return UNITY_END();
}
