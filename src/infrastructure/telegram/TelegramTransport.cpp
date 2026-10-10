#include "infrastructure/telegram/TelegramTransport.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>

#include "infrastructure/telegram/HttpResponse.h"
#include "infrastructure/telegram/TelegramAcknowledgement.h"

namespace {
constexpr uint32_t DNS_TIMEOUT_MS = 1000;
constexpr uint32_t CONNECT_TIMEOUT_MS = 1000;
constexpr uint32_t IO_TIMEOUT_MS = 1000;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 3000;
constexpr size_t MAX_TOKEN_BYTES = 128;
constexpr size_t MAX_BODY_BYTES = 3200;
constexpr size_t REQUEST_HEADROOM_BYTES = 350;
constexpr int TLS_RECEIVE_BYTES = 4096;
constexpr int TLS_TRANSMIT_BYTES = 4096;
constexpr unsigned CLOSE_ACK_MS = 1;
constexpr char HOST[] = "api.telegram.org";

bool failure(const char* stage, int detail = 0) {
    Serial.printf("Telegram failure: %s; code=%d; free heap=%u\n", stage, detail,
                  ESP.getFreeHeap());
    return false;
}
} // namespace

bool BoundedSecureClient::connectHost(const char* host) {
    const auto result =
        connectTelegram(*this, host, DNS_TIMEOUT_MS, CONNECT_TIMEOUT_MS, IO_TIMEOUT_MS);
    lastFailure = result.failure;
    lastTlsError = result.tlsError;
    return result.connected();
}

bool BoundedSecureClient::resolve(const char* host, uint32_t timeoutMs) {
    return WiFi.hostByName(host, resolvedAddress, timeoutMs);
}

void BoundedSecureClient::setIoTimeout(uint32_t timeoutMs) {
    setTimeout(timeoutMs);
}

bool BoundedSecureClient::connectTcp() {
    return WiFiClient::connect(resolvedAddress, 443);
}

bool BoundedSecureClient::handshake(const char* host) {
    // The pinned core resets TLS handshake timeout to 15000 ms internally.
    return _connectSSL(host);
}

bool BoundedSecureClient::mflnAccepted() {
    // A 4 KiB receive buffer is safe only when the peer accepts MFLN.
    return getMFLNStatus();
}

int BoundedSecureClient::handshakeError() {
    return getLastSSLError();
}

void BoundedSecureClient::close() {
    stop(CLOSE_ACK_MS);
}

TelegramTransport::TelegramTransport(const char* token)
    : token(token), certificate(TELEGRAM_CERTIFICATE_ROOT) {}

void TelegramTransport::begin() {
    if (initialized) {
        return;
    }
    initialized = true;
}

bool TelegramTransport::tokenIsValid() const {
    if (token == nullptr || token[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i <= MAX_TOKEN_BYTES; ++i) {
        const char value = token[i];
        if (value == '\0') {
            return true;
        }
        if (!((value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z') || value == ':' || value == '_' || value == '-')) {
            return false;
        }
    }
    return false;
}

bool TelegramTransport::send(const char* recipient, const char* message) {
    if (!initialized || !tokenIsValid() || recipient == nullptr || message == nullptr) {
        return failure("input");
    }
    HttpResponse response;
    if (!response.isAvailable()) {
        return failure("response allocation");
    }
    // UniversalTelegramBot::sendMessage hides retries and unbounded response accumulation.
    // Keep its trust root, but issue one bounded HTTP request per application attempt.
    JsonDocument payload;
    payload["chat_id"] = recipient;
    payload["text"] = message;
    if (payload.overflowed()) {
        return failure("JSON allocation");
    }
    const size_t bodyBytes = measureJson(payload);
    String body;
    if (bodyBytes > MAX_BODY_BYTES || !body.reserve(bodyBytes) ||
        serializeJson(payload, body) != bodyBytes) {
        return failure("request body");
    }
    String request;
    if (!request.reserve(body.length() + REQUEST_HEADROOM_BYTES)) {
        return failure("request allocation");
    }
    request = "POST /bot";
    request += token;
    request += "/sendMessage HTTP/1.0\r\nHost: api.telegram.org\r\n";
    request += "Content-Type: application/json\r\nConnection: close\r\nContent-Length: ";
    request += body.length();
    request += "\r\n\r\n";
    request += body;
    // Fresh client avoids the core resetting TCP timeout while closing a retained connection.
    BoundedSecureClient client;
    client.setTrustAnchors(&certificate);
    // The bounded request fits one plaintext TLS record; avoid per-fragment timeout multiplication.
    client.setBufferSizes(TLS_RECEIVE_BYTES, TLS_TRANSMIT_BYTES);
    if (!client.connectHost(HOST)) {
        switch (client.failure()) {
        case TelegramConnectFailure::DNS:
            return failure("DNS");
        case TelegramConnectFailure::TCP:
            return failure("TCP");
        case TelegramConnectFailure::TLS:
            return failure("TLS", client.tlsError());
        case TelegramConnectFailure::TLS_FRAGMENT:
            return failure("TLS fragment negotiation");
        case TelegramConnectFailure::NONE:
            return failure("connection");
        }
        return failure("connection");
    }
    if (client.write(reinterpret_cast<const uint8_t*>(request.c_str()), request.length()) !=
        request.length()) {
        client.stop(CLOSE_ACK_MS);
        return failure("request write");
    }
    // Bounded heap buffer and absolute response window reject trickle/oversized replies.
    const uint32_t started = millis();
    while (uint32_t(millis() - started) < RESPONSE_TIMEOUT_MS && !response.isComplete()) {
        if (client.available() > 0) {
            const int value = client.read();
            if (value >= 0 && !response.append(static_cast<char>(value))) {
                break;
            }
        } else if (!client.connected()) {
            response.finish();
            break;
        }
        yield();
    }
    client.stop(CLOSE_ACK_MS);
    if (!response.isValid()) {
        return failure("HTTP framing", response.statusCode());
    }
    if (!response.isSuccessStatus()) {
        return failure("HTTP status", response.statusCode());
    }
    if (!isAcknowledged(response)) {
        return failure("Telegram acknowledgement", response.statusCode());
    }
    return true;
}
