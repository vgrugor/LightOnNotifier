#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_WEB_BOUNDEDFIELD_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_WEB_BOUNDEDFIELD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

inline bool copyBoundedField(char* target, size_t capacity, const char* source, size_t length) {
    if (target == nullptr || source == nullptr || length >= capacity ||
        memchr(source, '\0', length) != nullptr) {
        return false;
    }
    memcpy(target, source, length);
    target[length] = '\0';
    return true;
}

inline bool parsePercentage(const char* source, size_t length, uint32_t& percent) {
    if (source == nullptr || length == 0 || length > 3) {
        return false;
    }
    uint32_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        if (source[i] < '0' || source[i] > '9') {
            return false;
        }
        value = value * 10 + static_cast<uint32_t>(source[i] - '0');
    }
    if (value > 100) {
        return false;
    }
    percent = value;
    return true;
}

#endif
