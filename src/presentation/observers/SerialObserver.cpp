#include "presentation/observers/SerialObserver.h"

#include <Arduino.h>

void SerialObserver::onEvent(const Event& event) {
    const char* text = "";
    switch (event.type) {
    case EventType::LIGHT_ON:
        text = "Light on";
        break;
    case EventType::WIFI_START_CONNECT:
        text = "Start connecting to WiFi";
        break;
    case EventType::WIFI_TRY_CONNECT:
        text = ".";
        break;
    case EventType::WIFI_CONNECTED:
        text = "WiFi connected";
        break;
    case EventType::WIFI_RECONNECT:
        text = "WiFi reconnect";
        break;
    case EventType::WIFI_CONFIGURATION_INVALID:
        text = "Invalid WiFi configuration";
        break;
    case EventType::TIME_WAITING:
        text = "Waiting for network time";
        break;
    case EventType::TIME_READY:
        text = "Network time ready";
        break;
    case EventType::NOTIFICATION_INVALID:
        text = "Invalid notification configuration";
        break;
    case EventType::MESSAGE_SEND:
        text = "Message sent";
        break;
    case EventType::MESSAGE_SEND_SKIP:
        text = "Message attempt failed";
        break;
    case EventType::MESSAGE_EXHAUSTED:
        text = "Message attempts exhausted";
        break;
    }
    Serial.print(text);
    if (event.type == EventType::MESSAGE_SEND || event.type == EventType::MESSAGE_SEND_SKIP ||
        event.type == EventType::MESSAGE_EXHAUSTED) {
        Serial.printf("; recipient index=%u; attempt=%u", unsigned(event.recipientIndex),
                      unsigned(event.attempt));
    }
    Serial.println();
}
