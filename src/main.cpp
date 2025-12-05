#include <Arduino.h>
#include "infrastructure/env.h"
#include "infrastructure/wifi/WiFiManager.h"
#include "infrastructure/actuators/ExternalLedActuator.h"
#include "infrastructure/actuators/BuzzerActuator.h"
#include "presentation/observers/LedObserver.h"
#include "presentation/observers/SerialObserver.h"
#include "presentation/observers/BuzzerObserver.h"
#include "infrastructure/loaders/OTALoader.h"
#include "presentation/TelegramNotifier.h"

WiFiManager wifiManager(WIFI_SSID, WIFI_PASSWORD, WIFI_IP, WIFI_GATEWAY, WIFI_SUBNET);

ExternalLedActuator externalLedActuator(EXTERNAL_LED_PIN);
BuzzerActuator buzzerActuator(BUZZER_PIN);

LedObserver ledObserver(externalLedActuator);
SerialObserver serialObserver;
BuzzerObserver buzzerObserver(buzzerActuator);

EventNotifier& eventNotifier = EventNotifier::getInstance();

TelegramNotifier telegramNotifier(BOT_TOKEN, CHAT_IDS, CHAT_IDS_COUNT);

OTALoader OTA(OTA_HOSTNAME, OTA_PASSWORD);

void setup() {
    Serial.begin(115200);

    eventNotifier.addObserver(&ledObserver);
    eventNotifier.addObserver(&serialObserver);

    EventNotifier::getInstance().notifyObservers(EventType::LIGHT_ON);

    wifiManager.connect();

    telegramNotifier.init();
	telegramNotifier.sendMessage(LIGHT_ON_MESSAGE);

    OTA.begin();
}

void loop() {
	wifiManager.reconnect();

    OTA.handle();
}
