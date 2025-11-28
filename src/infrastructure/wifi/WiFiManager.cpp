#include "infrastructure/wifi/WiFiManager.h"
#include <Arduino.h>

WiFiManager::WiFiManager(
    const char* ssid, 
    const char* password, 
    const char* ip, 
    const char* gateway, 
    const char* subnet
) : 
    ssid(ssid), 
    password(password), 
    ip(ip), 
    gateway(gateway), 
    subnet(subnet) 
{
}

void WiFiManager::connect() {
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;

    ip.fromString(this->ip);
    gateway.fromString(this->gateway);
    subnet.fromString(this->subnet);

    WiFi.mode(WIFI_STA);
    WiFi.config(ip, gateway, subnet);
    WiFi.begin(ssid, password);

    EventNotifier& eventNotifier = EventNotifier::getInstance();

    eventNotifier.notifyObservers(EventType::WIFI_START_CONNECT);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        eventNotifier.notifyObservers(EventType::WIFI_TRY_CONNECT);
    }

    eventNotifier.notifyObservers(EventType::WIFI_CONNECTED);
}

void WiFiManager::reconnect() {
    if (!isConnected()) {
        EventNotifier::getInstance().notifyObservers(EventType::WIFI_RECONNECT);
        connect();
    }
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIPAddress() {
    return WiFi.localIP().toString();
}
