#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_WIFI_WIFIMANAGER_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_WIFI_WIFIMANAGER_H

#include "application/Ports.h"

class WiFiManager : public ConnectionPort {
public:
    WiFiManager(const char* ssid, const char* password, const char* ip, const char* gateway,
                const char* subnet);
    bool configure() override;
    void startAttempt() override;
    void stopAttempt() override;
    bool isConnected() const override;

private:
    const char* ssid;
    const char* password;
    const char* ip;
    const char* gateway;
    const char* subnet;
};
#endif
