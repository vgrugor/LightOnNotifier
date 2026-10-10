#ifndef LIGHTONNOTIFIER_APPLICATION_DEVICESETTINGS_H
#define LIGHTONNOTIFIER_APPLICATION_DEVICESETTINGS_H

#include <stddef.h>
#include <stdint.h>

enum class LedMode : uint8_t { OFF, STEADY, BLINK };

struct DeviceSettings {
    static constexpr uint32_t SCHEMA = 4;
    uint32_t schema = SCHEMA;
    uint32_t revision = 1;
    bool configured = false;
    char ssid[33] = {};
    char wifiPassword[65] = {};
    char ip[16] = {};
    char gateway[16] = {};
    char subnet[16] = {};
    char botToken[129] = {};
    char recipients[8][34] = {};
    uint8_t recipientCount = 0;
    char message[513] = {};
    bool startupSound = true;
    bool wifiProgressSound = true;
    bool wifiConnectedSound = true;
    bool ledEnabled = true;
    bool deliveryBlink = true;
    LedMode startupLed = LedMode::STEADY;
    LedMode connectingLed = LedMode::STEADY;
    LedMode waitingLed = LedMode::OFF;
    LedMode idleLed = LedMode::STEADY;
    LedMode errorLed = LedMode::STEADY;
    uint8_t adminSalt[16] = {};
    uint8_t adminHash[32] = {};
    bool adminConfigured = false;
    uint32_t startupSoundSeconds = 10;
    bool quietHoursEnabled = false;
    uint8_t quietStartHour = 22;
    uint8_t quietEndHour = 7;
    uint32_t quietLedBrightnessPercent = 10;
};

bool validNetworkSettings(const DeviceSettings& settings);
bool validTelegramSettings(const DeviceSettings& settings);
bool validSignalSettings(const DeviceSettings& settings);

#endif
