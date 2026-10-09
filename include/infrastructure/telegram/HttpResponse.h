#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_HTTPRESPONSE_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_HTTPRESPONSE_H

#include <memory>
#include <stddef.h>

// Incremental bounded HTTP/1.x response framing. HTTP/1.0 requests avoid chunked bodies;
// unexpected transfer encoding, truncated bodies, and oversized replies fail closed.
class HttpResponse {
public:
    static constexpr size_t MAX_BYTES = 8192;
    HttpResponse();
    bool append(char value);
    void finish();
    bool isAvailable() const {
        return buffer != nullptr;
    }
    bool isComplete() const {
        return complete;
    }
    bool isValid() const {
        return complete && !invalid;
    }
    bool isSuccessStatus() const {
        return successStatus;
    }
    int statusCode() const {
        return httpStatus;
    }
    const char* body() const {
        return buffer ? buffer.get() + bodyOffset : "";
    }
    size_t bodySize() const {
        return size - bodyOffset;
    }

private:
    bool parseHeaders();
    std::unique_ptr<char[]> buffer;
    size_t size = 0;
    size_t bodyOffset = 0;
    size_t contentLength = 0;
    bool hasLength = false;
    bool headersReady = false;
    bool complete = false;
    bool invalid = false;
    bool successStatus = false;
    int httpStatus = 0;
};
#endif
