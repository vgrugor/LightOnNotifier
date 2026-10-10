#ifndef LIGHTONNOTIFIER_APPLICATION_CONNECTIONSERVICE_H
#define LIGHTONNOTIFIER_APPLICATION_CONNECTIONSERVICE_H

#include <stdint.h>

#include "application/Ports.h"
#include "domain/Event.h"

class ConnectionService {
public:
    enum class State { IDLE, CONNECTING, RETRY_WAIT, CONNECTED, INVALID };
    static constexpr uint32_t ATTEMPT_MS = 15000;
    static constexpr uint32_t RETRY_MS = 5000;
    static constexpr uint32_t PROGRESS_MS = 1000;
    ConnectionService(ConnectionPort& connection, EventSink& events);
    void begin(uint32_t now);
    void reconfigure(uint32_t now);
    void update(uint32_t now);
    bool isConnected() const;
    State state() const {
        return currentState;
    }

private:
    void start(uint32_t now);
    ConnectionPort& connection;
    EventSink& events;
    State currentState = State::IDLE;
    uint32_t stateStarted = 0;
    uint32_t progressStarted = 0;
};
#endif
