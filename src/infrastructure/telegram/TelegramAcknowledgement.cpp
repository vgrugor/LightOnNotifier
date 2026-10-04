#include "infrastructure/telegram/TelegramAcknowledgement.h"

#include <ctype.h>

#include <ArduinoJson.h>

namespace {
class BodyReader {
public:
    BodyReader(const char* data, size_t size) : data(data), size(size) {}
    int read() {
        return offset < size ? static_cast<unsigned char>(data[offset++]) : -1;
    }
    size_t readBytes(char* target, size_t length) {
        size_t copied = 0;
        while (copied < length && offset < size) {
            target[copied++] = data[offset++];
        }
        return copied;
    }
    bool hasOnlyWhitespaceRemaining() const {
        for (size_t index = offset; index < size; ++index) {
            if (!isspace(static_cast<unsigned char>(data[index]))) {
                return false;
            }
        }
        return true;
    }

private:
    const char* data;
    size_t size;
    size_t offset = 0;
};
} // namespace

bool isAcknowledged(const HttpResponse& response) {
    if (!response.isValid() || !response.isSuccessStatus()) {
        return false;
    }
    // Ignore Telegram's echoed message/chat metadata to limit JSON heap use.
    JsonDocument filter;
    filter["ok"] = true;
    JsonDocument result;
    BodyReader reader(response.body(), response.bodySize());
    if (deserializeJson(result, reader, DeserializationOption::Filter(filter)) ||
        !reader.hasOnlyWhitespaceRemaining()) {
        return false;
    }
    return result["ok"].is<bool>() && result["ok"].as<bool>();
}
