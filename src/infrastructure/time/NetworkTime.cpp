#include "infrastructure/time/NetworkTime.h"

#include <time.h>

#include <Arduino.h>

namespace {
constexpr time_t MIN_VALID_UNIX_TIME = 1704067200; // 2024-01-01 UTC; never use boot epoch for TLS.
}

void NetworkTime::startSynchronization() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}
bool NetworkTime::isValid() const {
    return time(nullptr) >= MIN_VALID_UNIX_TIME;
}
