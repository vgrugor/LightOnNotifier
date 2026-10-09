#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_TELEGRAMTRANSPORT_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_TELEGRAMTRANSPORT_H

#include <WiFiClientSecureBearSSL.h>

#include "application/Ports.h"

// Uses the pinned framework's protected TLS entry point to preserve hostname validation
// after explicitly bounded DNS/TCP stages. Review this boundary when updating the framework.
class BoundedSecureClient : public BearSSL::WiFiClientSecureCtx {
public:
    enum class Failure { NONE, DNS, TCP, TLS, TLS_FRAGMENT };
    bool connectHost(const char* host);
    Failure failure() const {
        return lastFailure;
    }
    int tlsError() const {
        return lastTlsError;
    }

private:
    Failure lastFailure = Failure::NONE;
    int lastTlsError = 0;
};

class TelegramTransport : public MessageSender {
public:
    explicit TelegramTransport(const char* token);
    void begin();
    void setToken(const char* value) {
        token = value;
    }
    bool send(const char* recipient, const char* message) override;

private:
    bool tokenIsValid() const;
    const char* token;
    // Each send borrows these anchors with a fresh local RAII TLS client.
    BearSSL::X509List certificate;
    bool initialized = false;
};
#endif
