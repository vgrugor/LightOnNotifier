#include "application/ConnectionService.h"

ConnectionService::ConnectionService(ConnectionPort& connection, EventSink& events)
    : connection(connection), events(events) {}

void ConnectionService::begin(uint32_t now) {
    if (currentState != State::IDLE) {
        return;
    }
    if (!connection.configure()) {
        currentState = State::INVALID;
        events.publish(Event(EventType::WIFI_CONFIGURATION_INVALID));
        return;
    }
    start(now);
}

void ConnectionService::reconfigure(uint32_t now) {
    connection.stopAttempt();
    currentState = State::IDLE;
    begin(now);
}

void ConnectionService::start(uint32_t now) {
    stateStarted = now;
    progressStarted = now;
    currentState = State::CONNECTING;
    connection.startAttempt();
    events.publish(Event(EventType::WIFI_START_CONNECT));
}

bool ConnectionService::isConnected() const {
    return currentState == State::CONNECTED && connection.isConnected();
}

void ConnectionService::update(uint32_t now) {
    if (currentState == State::IDLE || currentState == State::INVALID) {
        return;
    }
    if (connection.isConnected()) {
        if (currentState != State::CONNECTED) {
            currentState = State::CONNECTED;
            events.publish(Event(EventType::WIFI_CONNECTED));
        }
        return;
    }
    if (currentState == State::CONNECTED) {
        events.publish(Event(EventType::WIFI_RECONNECT));
        start(now);
    } else if (currentState == State::CONNECTING) {
        if (uint32_t(now - stateStarted) >= ATTEMPT_MS) {
            connection.stopAttempt();
            currentState = State::RETRY_WAIT;
            stateStarted = now;
        } else if (uint32_t(now - progressStarted) >= PROGRESS_MS) {
            progressStarted = now;
            events.publish(Event(EventType::WIFI_TRY_CONNECT));
        }
    } else if (uint32_t(now - stateStarted) >= RETRY_MS) {
        events.publish(Event(EventType::WIFI_RECONNECT));
        start(now);
    }
}
