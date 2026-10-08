#ifndef LIGHTONNOTIFIER_APPLICATION_STATICNETWORKCONFIG_H
#define LIGHTONNOTIFIER_APPLICATION_STATICNETWORKCONFIG_H

#include <stdint.h>

struct StaticNetworkConfig {
    uint8_t address[4];
    uint8_t gateway[4];
    uint8_t subnet[4];
};
// Reject invalid static settings atomically; no DHCP fallback or output mutation on error.
bool parseStaticNetwork(const char* address, const char* gateway, const char* subnet,
                        StaticNetworkConfig& result);
#endif
