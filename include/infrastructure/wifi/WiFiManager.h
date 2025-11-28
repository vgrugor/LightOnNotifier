#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include "presentation/EventNotifier.h"

class WiFiManager {
    private:
        const char* ssid;
        const char* password;
        const char* ip;
        const char* gateway;
        const char* subnet;

    public:
        WiFiManager(const char* ssid, const char* password, const char* ip, const char* gateway, const char* subnet);
        void connect();
        void reconnect();
        bool isConnected();
        String getIPAddress();
};

#endif // WIFI_MANAGER_H
