#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_TELEGRAMTRANSPORT_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_TELEGRAMTRANSPORT_H

#include <WiFiClientSecureBearSSL.h>

#include "application/Ports.h"
#include "infrastructure/telegram/TelegramConnection.h"

// Uses the pinned framework's protected TLS entry point to preserve hostname validation
// after explicitly bounded DNS/TCP stages. Review this boundary when updating the framework.
class BoundedSecureClient : public BearSSL::WiFiClientSecureCtx, public TelegramConnectPort {
public:
    bool connectHost(const char* host);
    bool resolve(const char* host, uint32_t timeoutMs) override;
    void setIoTimeout(uint32_t timeoutMs) override;
    bool connectTcp() override;
    bool handshake(const char* host) override;
    bool mflnAccepted() override;
    int handshakeError() override;
    void close() override;
    TelegramConnectFailure failure() const {
        return lastFailure;
    }
    int tlsError() const {
        return lastTlsError;
    }

private:
    IPAddress resolvedAddress;
    TelegramConnectFailure lastFailure = TelegramConnectFailure::NONE;
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
