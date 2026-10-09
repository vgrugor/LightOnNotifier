#ifndef LIGHTONNOTIFIER_APPLICATION_PORTS_H
#define LIGHTONNOTIFIER_APPLICATION_PORTS_H

#include <stdint.h>

class MonotonicClock {
public:
    virtual ~MonotonicClock() = default;
    virtual uint32_t now() const = 0;
};

class ConnectionPort {
public:
    virtual ~ConnectionPort() = default;
    virtual bool configure() = 0;
    virtual void startAttempt() = 0;
    virtual void stopAttempt() = 0;
    virtual bool isConnected() const = 0;
};

class TimePort {
public:
    virtual ~TimePort() = default;
    virtual void startSynchronization() = 0;
    virtual bool isValid() const = 0;
    virtual bool localHour(uint8_t& hour) const = 0;
};

class MessageSender {
public:
    virtual ~MessageSender() = default;
    virtual bool send(const char* recipient, const char* message) = 0;
};

class SignalOutput {
public:
    virtual ~SignalOutput() = default;
    virtual void setLed(bool active) = 0;
    virtual void setBuzzer(bool active) = 0;
};

class ButtonPort {
public:
    virtual ~ButtonPort() = default;
    virtual bool isPressed() const = 0;
};

class OtaPort {
public:
    virtual ~OtaPort() = default;
    virtual void begin() = 0;
    virtual void handle() = 0;
};
#endif
