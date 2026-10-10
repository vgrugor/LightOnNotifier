#include "infrastructure/time/NetworkTime.h"

#include <time.h>

#include <Arduino.h>
#include <TZ.h>

namespace {
constexpr time_t MIN_VALID_UNIX_TIME = 1704067200; // 2024-01-01 UTC; never use boot epoch for TLS.
}

void NetworkTime::startSynchronization() {
    configTime(TZ_Europe_Kiev, "pool.ntp.org", "time.nist.gov");
    timezoneConfigured = true;
}
bool NetworkTime::isValid() const {
    return time(nullptr) >= MIN_VALID_UNIX_TIME;
}
bool NetworkTime::localHour(uint8_t& hour) const {
    if (!timezoneConfigured) {
        return false;
    }
    const time_t current = time(nullptr);
    if (current < MIN_VALID_UNIX_TIME) {
        return false;
    }
    struct tm local = {};
    if (localtime_r(&current, &local) == nullptr) {
        return false;
    }
    hour = static_cast<uint8_t>(local.tm_hour);
    return true;
}
