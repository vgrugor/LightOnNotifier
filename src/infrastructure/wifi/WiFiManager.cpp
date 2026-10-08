#include "infrastructure/wifi/WiFiManager.h"

#include <ESP8266WiFi.h>

#include "application/StaticNetworkConfig.h"

WiFiManager::WiFiManager(const char* ssid, const char* password, const char* ip,
                         const char* gateway, const char* subnet)
    : ssid(ssid), password(password), ip(ip), gateway(gateway), subnet(subnet) {}

bool WiFiManager::configure() {
    StaticNetworkConfig parsed;
    if (ssid == nullptr || ssid[0] == '\0' || password == nullptr ||
        !parseStaticNetwork(ip, gateway, subnet, parsed)) {
        return false;
    }
    const IPAddress address(parsed.address);
    const IPAddress route(parsed.gateway);
    const IPAddress mask(parsed.subnet);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    const WiFiMode_t mode = (WiFi.getMode() & WIFI_AP) != 0 ? WIFI_AP_STA : WIFI_STA;
    return WiFi.mode(mode) && WiFi.config(address, route, mask, route);
}

void WiFiManager::startAttempt() {
    WiFi.begin(ssid, password);
}
void WiFiManager::stopAttempt() {
    WiFi.disconnect();
}
bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}
