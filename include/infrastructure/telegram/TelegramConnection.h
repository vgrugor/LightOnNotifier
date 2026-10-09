#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_TELEGRAMCONNECTION_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_TELEGRAMCONNECTION_H

#include <stdint.h>

enum class TelegramConnectFailure { NONE, DNS, TCP, TLS, TLS_FRAGMENT };

struct TelegramConnectResult {
    TelegramConnectFailure failure;
    int tlsError;

    bool connected() const {
        return failure == TelegramConnectFailure::NONE;
    }
};

class TelegramConnectPort {
public:
    virtual ~TelegramConnectPort() = default;
    virtual bool resolve(const char* host, uint32_t timeoutMs) = 0;
    virtual void setIoTimeout(uint32_t timeoutMs) = 0;
    virtual bool connectTcp() = 0;
    virtual bool handshake(const char* host) = 0;
    virtual int handshakeError() = 0;
    virtual bool mflnAccepted() = 0;
    virtual void close() = 0;
};

TelegramConnectResult connectTelegram(TelegramConnectPort& client, const char* host,
                                      uint32_t dnsTimeoutMs, uint32_t connectTimeoutMs,
                                      uint32_t ioTimeoutMs);

#endif
