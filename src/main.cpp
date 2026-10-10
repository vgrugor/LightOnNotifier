#include <Arduino.h>

#include "application/NotifierApplication.h"
#include "infrastructure/GpioSignals.h"
#include "infrastructure/env.h"
#include "infrastructure/loaders/OTALoader.h"
#include "infrastructure/settings/SettingsStore.h"
#include "infrastructure/telegram/TelegramTransport.h"
#include "infrastructure/time/NetworkTime.h"
#include "infrastructure/web/WebPortal.h"
#include "infrastructure/wifi/WiFiManager.h"
#include "presentation/EventNotifier.h"
#include "presentation/observers/SerialObserver.h"

namespace {
DeviceSettings settings;
SettingsStore settingsStore;
char startupToken[sizeof(settings.botToken)] = {};
const char* startupRecipients[NotificationService::MAX_RECIPIENTS] = {};
// All callback targets and borrowed configuration have static lifetime.
EventNotifier events;
ExternalLedActuator led(EXTERNAL_LED_PIN);
BuzzerActuator buzzer(BUZZER_PIN);
GpioSignals outputs(led, buzzer);
GpioButton button(BUTTON_PIN);
ArduinoClock monotonicClock;
NetworkTime networkTime;
SignalController signals(outputs, button, networkTime);
SerialObserver serial;
WiFiManager wifi(settings.ssid, settings.wifiPassword, settings.ip, settings.gateway,
                 settings.subnet);
ConnectionService connection(wifi, events);
TimeService timeService(networkTime, events);
TelegramTransport telegram(startupToken);
NotificationService notifications(telegram, events, monotonicClock);
OTALoader ota(OTA_HOSTNAME, OTA_PASSWORD);
NotifierApplication application(connection, timeService, notifications, signals, ota, events);
WebPortal web(settings, settingsStore, connection, timeService, notifications, signals, telegram,
              outputs, SETUP_PASSWORD);
bool buttonWasPressed = false;
bool recoveryTriggered = false;
uint32_t buttonPressedAt = 0;

template <size_t N> void copySetting(char (&destination)[N], const char* source) {
    if (source == nullptr) {
        return;
    }
    const size_t length = strnlen(source, N);
    if (length < N) {
        memcpy(destination, source, length + 1);
    }
}

void loadSettings() {
    if (!settingsStore.begin()) {
        return;
    }
    if (settingsStore.load(settings)) {
        return;
    }
    if (settingsStore.hasRecords() || settingsStore.wasProvisioned()) {
        // Unknown or damaged stored data must never be replaced by compiled credentials.
        return;
    }
    copySetting(settings.ssid, WIFI_SSID);
    copySetting(settings.wifiPassword, WIFI_PASSWORD);
    copySetting(settings.ip, WIFI_IP);
    copySetting(settings.gateway, WIFI_GATEWAY);
    copySetting(settings.subnet, WIFI_SUBNET);
    copySetting(settings.botToken, BOT_TOKEN);
    copySetting(settings.message, LIGHT_ON_MESSAGE);
    if (CHAT_IDS_COUNT <= NotificationService::MAX_RECIPIENTS) {
        settings.recipientCount = CHAT_IDS_COUNT;
        for (size_t index = 0; index < CHAT_IDS_COUNT; ++index) {
            copySetting(settings.recipients[index], CHAT_IDS[index]);
        }
    }
    settings.configured = validNetworkSettings(settings) && validTelegramSettings(settings);
    if (settings.configured && !settingsStore.save(settings)) {
        settings.configured = false;
    }
}
} // namespace

void setup() {
    Serial.begin(115200);
    outputs.begin();
    button.begin();
    loadSettings();
    signals.setSettings(settings);
    events.addObserver(&signals);
    events.addObserver(&serial);
    telegram.begin();
    copySetting(startupToken, settings.botToken);
    for (size_t index = 0; index < settings.recipientCount; ++index) {
        startupRecipients[index] = settings.recipients[index];
    }
    application.begin(monotonicClock.now(), settings.configured ? settings.message : "",
                      startupRecipients, settings.configured ? settings.recipientCount : 0);
    web.begin();
}

void loop() {
    const uint32_t now = monotonicClock.now();
    application.update(now);
    const bool pressed = button.isPressed();
    if (pressed && !buttonWasPressed) {
        buttonPressedAt = now;
        recoveryTriggered = false;
    }
    if (pressed && !recoveryTriggered && uint32_t(now - buttonPressedAt) >= 5000) {
        web.enterRecovery(now);
        recoveryTriggered = true;
    }
    buttonWasPressed = pressed;
    web.update(now);
    yield();
}
