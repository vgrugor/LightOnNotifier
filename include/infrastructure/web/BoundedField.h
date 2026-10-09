#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_WEB_BOUNDEDFIELD_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_WEB_BOUNDEDFIELD_H

#include <stddef.h>
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

#endif
