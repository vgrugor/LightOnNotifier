#include "application/StaticNetworkConfig.h"

#include <stddef.h>

namespace {
bool parseAddress(const char* text, uint8_t (&result)[4]) {
    if (text == nullptr) {
        return false;
    }
    for (size_t index = 0; index < 4; ++index) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        unsigned value = 0;
        size_t digits = 0;
        while (*text >= '0' && *text <= '9') {
            value = value * 10 + static_cast<unsigned>(*text++ - '0');
            if (++digits > 3 || value > 255) {
                return false;
            }
        }
        result[index] = static_cast<uint8_t>(value);
        if (index == 3) {
            return *text == '\0';
        }
        if (*text++ != '.') {
            return false;
        }
    }
    return false;
}
uint32_t bits(const uint8_t (&address)[4]) {
    return (uint32_t(address[0]) << 24) | (uint32_t(address[1]) << 16) |
           (uint32_t(address[2]) << 8) | address[3];
}
} // namespace

bool parseStaticNetwork(const char* address, const char* gateway, const char* subnet,
                        StaticNetworkConfig& result) {
    StaticNetworkConfig parsed;
    if (!parseAddress(address, parsed.address) || !parseAddress(gateway, parsed.gateway) ||
        !parseAddress(subnet, parsed.subnet)) {
        return false;
    }
    const uint32_t ip = bits(parsed.address);
    const uint32_t route = bits(parsed.gateway);
    const uint32_t mask = bits(parsed.subnet);
    const uint32_t inverted = ~mask;
    if (mask == 0 || inverted == 0 || (inverted & (inverted + 1)) != 0 ||
        (ip & mask) != (route & mask) || ip == route || (ip & inverted) == 0 ||
        (ip & inverted) == inverted || (route & inverted) == 0 || (route & inverted) == inverted ||
        parsed.address[0] == 0 || parsed.address[0] >= 224 || parsed.gateway[0] == 0 ||
        parsed.gateway[0] >= 224) {
        return false;
    }
    result = parsed;
    return true;
}
