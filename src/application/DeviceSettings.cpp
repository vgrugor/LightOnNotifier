#include "application/DeviceSettings.h"

#include <string.h>

#include "application/NotificationService.h"
#include "application/StaticNetworkConfig.h"

namespace {
bool terminated(const char* value, size_t capacity) {
    return memchr(value, '\0', capacity) != nullptr;
}

bool validMode(LedMode mode) {
    return mode == LedMode::OFF || mode == LedMode::STEADY || mode == LedMode::BLINK;
}
} // namespace

bool validNetworkSettings(const DeviceSettings& settings) {
    StaticNetworkConfig parsed;
    return terminated(settings.ssid, sizeof(settings.ssid)) && settings.ssid[0] != '\0' &&
           terminated(settings.wifiPassword, sizeof(settings.wifiPassword)) &&
           terminated(settings.ip, sizeof(settings.ip)) &&
           terminated(settings.gateway, sizeof(settings.gateway)) &&
           terminated(settings.subnet, sizeof(settings.subnet)) &&
           parseStaticNetwork(settings.ip, settings.gateway, settings.subnet, parsed);
}

bool validTelegramSettings(const DeviceSettings& settings) {
    if (!terminated(settings.botToken, sizeof(settings.botToken)) || settings.botToken[0] == '\0' ||
        !terminated(settings.message, sizeof(settings.message)) || settings.recipientCount == 0 ||
        settings.recipientCount > 8) {
        return false;
    }
    for (size_t i = 0; i < sizeof(settings.botToken) && settings.botToken[i] != '\0'; ++i) {
        const char value = settings.botToken[i];
        if (!((value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z') || value == ':' || value == '_' || value == '-')) {
            return false;
        }
    }
    const char* ids[8] = {};
    for (size_t i = 0; i < settings.recipientCount; ++i) {
        if (!terminated(settings.recipients[i], sizeof(settings.recipients[i]))) {
            return false;
        }
        ids[i] = settings.recipients[i];
    }
    return NotificationService::validate(settings.message, ids, settings.recipientCount);
}

bool validSignalSettings(const DeviceSettings& settings) {
    return settings.startupSoundSeconds >= 1 && settings.startupSoundSeconds <= 60 &&
           validMode(settings.startupLed) && validMode(settings.connectingLed) &&
           validMode(settings.waitingLed) && validMode(settings.idleLed) &&
           validMode(settings.errorLed);
}
