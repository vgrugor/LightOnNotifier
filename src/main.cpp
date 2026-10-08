#include <Arduino.h>

#include "application/NotifierApplication.h"
#include "infrastructure/GpioSignals.h"
#include "infrastructure/env.h"
#include "infrastructure/loaders/OTALoader.h"
#include "infrastructure/telegram/TelegramTransport.h"
#include "infrastructure/time/NetworkTime.h"
#include "infrastructure/wifi/WiFiManager.h"
#include "presentation/EventNotifier.h"
#include "presentation/observers/SerialObserver.h"

namespace {
// All callback targets and borrowed configuration have static lifetime.
EventNotifier events;
ExternalLedActuator led(EXTERNAL_LED_PIN);
BuzzerActuator buzzer(BUZZER_PIN);
GpioSignals outputs(led, buzzer);
GpioButton button(BUTTON_PIN);
ArduinoClock monotonicClock;
SignalController signals(outputs, button);
SerialObserver serial;
WiFiManager wifi(WIFI_SSID, WIFI_PASSWORD, WIFI_IP, WIFI_GATEWAY, WIFI_SUBNET);
ConnectionService connection(wifi, events);
NetworkTime networkTime;
TimeService timeService(networkTime, events);
TelegramTransport telegram(BOT_TOKEN);
NotificationService notifications(telegram, events, monotonicClock);
OTALoader ota(OTA_HOSTNAME, OTA_PASSWORD);
NotifierApplication application(connection, timeService, notifications, signals, ota, events);
} // namespace

void setup() {
    Serial.begin(115200);
    outputs.begin();
    button.begin();
    events.addObserver(&signals);
    events.addObserver(&serial);
    telegram.begin();
    application.begin(monotonicClock.now(), LIGHT_ON_MESSAGE, CHAT_IDS, CHAT_IDS_COUNT);
}

void loop() {
    application.update(monotonicClock.now());
    yield();
}
