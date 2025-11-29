#include <Arduino.h>
#include "infrastructure/env.h"
#include "infrastructure/wifi/WiFiManager.h"
#include "infrastructure/actuators/InternalLedActuator.h"
#include "presentation/observers/LedObserver.h"
#include "presentation/observers/SerialObserver.h"
#include "infrastructure/loaders/OTALoader.h"
#include "presentation/TelegramNotifier.h"

WiFiManager wifiManager(WIFI_SSID, WIFI_PASSWORD, WIFI_IP, WIFI_GATEWAY, WIFI_SUBNET);

InternalLedActuator internalLedActuator(BOARD_LED);

LedObserver ledObserver(internalLedActuator);
SerialObserver serialObserver;

EventNotifier& eventNotifier = EventNotifier::getInstance();

TelegramNotifier telegramNotifier(BOT_TOKEN, CHAT_ID);

OTALoader OTA(OTA_HOSTNAME, OTA_PASSWORD);

void setup() {
    Serial.begin(115200);

    eventNotifier.addObserver(&ledObserver);
    eventNotifier.addObserver(&serialObserver);

    wifiManager.connect();

    telegramNotifier.init();
	telegramNotifier.sendMessage(LIGHT_ON_MESSAGE);

    OTA.begin();
}

void loop() {
	wifiManager.reconnect();

    OTA.handle();
}
