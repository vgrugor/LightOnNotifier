#include "infrastructure/telegram/TelegramConnection.h"

TelegramConnectResult connectTelegram(TelegramConnectPort& client, const char* host,
                                      uint32_t dnsTimeoutMs, uint32_t connectTimeoutMs,
                                      uint32_t ioTimeoutMs) {
    if (!client.resolve(host, dnsTimeoutMs)) {
        return {TelegramConnectFailure::DNS, 0};
    }
    client.setIoTimeout(connectTimeoutMs);
    if (!client.connectTcp()) {
        return {TelegramConnectFailure::TCP, 0};
    }
    const bool handshake = client.handshake(host);
    const int tlsError = handshake ? 0 : client.handshakeError();
    const bool mfln = handshake && client.mflnAccepted();
    client.setIoTimeout(ioTimeoutMs);
    if (!handshake || !mfln) {
        client.close();
        return {handshake ? TelegramConnectFailure::TLS_FRAGMENT : TelegramConnectFailure::TLS,
                tlsError};
    }
    return {TelegramConnectFailure::NONE, 0};
}
