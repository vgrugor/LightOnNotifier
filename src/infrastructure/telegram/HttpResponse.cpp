#include "infrastructure/telegram/HttpResponse.h"

#include <ctype.h>
#include <new>
#include <string.h>

namespace {
bool matches(const char* text, const char* expected) {
    for (size_t i = 0; expected[i] != '\0'; ++i) {
        if (tolower(static_cast<unsigned char>(text[i])) != expected[i]) {
            return false;
        }
    }
    return true;
}
} // namespace

HttpResponse::HttpResponse() : buffer(new(std::nothrow) char[MAX_BYTES + 1]()) {
    invalid = !buffer;
}

bool HttpResponse::append(char value) {
    if (complete || invalid || size == MAX_BYTES || value == '\0') {
        invalid = true;
        return false;
    }
    buffer[size++] = value;
    buffer[size] = '\0';
    if (!headersReady && size >= 4 && memcmp(buffer.get() + size - 4, "\r\n\r\n", 4) == 0) {
        bodyOffset = size;
        headersReady = parseHeaders();
        if (!headersReady) {
            invalid = true;
            return false;
        }
    }
    if (headersReady && hasLength && bodySize() == contentLength) {
        complete = true;
    }
    return true;
}

bool HttpResponse::parseHeaders() {
    char* lineEnd = strstr(buffer.get(), "\r\n");
    if (lineEnd == nullptr || lineEnd - buffer.get() < 12 ||
        (strncmp(buffer.get(), "HTTP/1.0 ", 9) != 0 &&
         strncmp(buffer.get(), "HTTP/1.1 ", 9) != 0) ||
        buffer[9] < '1' || buffer[9] > '5' || buffer[10] < '0' || buffer[10] > '9' ||
        buffer[11] < '0' || buffer[11] > '9' || (buffer[12] != ' ' && buffer[12] != '\r')) {
        return false;
    }
    successStatus = buffer[9] == '2' && buffer[10] == '0' && buffer[11] == '0';
    char* line = lineEnd + 2;
    while (*line != '\r') {
        lineEnd = strstr(line, "\r\n");
        if (lineEnd == nullptr) {
            return false;
        }
        *lineEnd = '\0';
        const char* colon = strchr(line, ':');
        if (colon == nullptr || colon == line) {
            return false;
        }
        if (matches(line, "transfer-encoding:")) {
            return false;
        }
        if (matches(line, "content-length:")) {
            if (hasLength) {
                return false;
            }
            const char* value = colon + 1;
            while (*value == ' ' || *value == '\t') {
                ++value;
            }
            if (*value < '0' || *value > '9') {
                return false;
            }
            size_t length = 0;
            while (*value >= '0' && *value <= '9') {
                length = length * 10 + static_cast<size_t>(*value++ - '0');
                if (length > MAX_BYTES - bodyOffset) {
                    return false;
                }
            }
            while (*value == ' ' || *value == '\t') {
                ++value;
            }
            if (*value != '\0') {
                return false;
            }
            contentLength = length;
            hasLength = true;
        }
        line = lineEnd + 2;
    }
    return true;
}

void HttpResponse::finish() {
    if (!headersReady || (hasLength && bodySize() != contentLength)) {
        invalid = true;
    }
    complete = true;
}
