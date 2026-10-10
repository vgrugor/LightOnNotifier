#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_WEB_WEBPORTAL_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_WEB_WEBPORTAL_H

#include <ESP8266WebServer.h>

#include "application/ConnectionService.h"
#include "application/DeviceSettings.h"
#include "application/NotificationService.h"
#include "application/SignalController.h"
#include "application/TimeService.h"
#include "infrastructure/GpioSignals.h"
#include "infrastructure/settings/SettingsStore.h"
#include "infrastructure/telegram/TelegramTransport.h"

class WebPortal {
public:
    WebPortal(DeviceSettings& settings, SettingsStore& store, ConnectionService& connection,
              TimeService& time, NotificationService& notifications, SignalController& signals,
              TelegramTransport& telegram, GpioSignals& outputs, const char* setupPassword);
    void begin();
    void update(uint32_t now);
    void enterRecovery(uint32_t now);

private:
    void routes();
    void login();
    void session();
    void status();
    void getSettings();
    void saveSignals();
    void saveTelegram();
    void applyWifi();
    void confirmWifi();
    void changePassword();
    void preview();
    void startTest();
    void reset();
    void restart();
    void rollbackWifi(uint32_t now);
    bool authorized(bool mutation = false);
    bool hostAllowed() const;
    bool validRequest(bool mutation);
    bool checkRevision();
    bool saveCandidate(DeviceSettings& candidate);
    bool copyField(char* target, size_t capacity, const String& value);
    void fail(int code, const char* message);
    void sendJson(int code, const String& body);
    void startAccessPoint();
    void stopAccessPoint();
    bool checkPassword(const String& password) const;
    void hashPassword(const String& password, const uint8_t* salt, uint8_t* result) const;
    String randomHex(size_t bytes);

    DeviceSettings& settings;
    SettingsStore& store;
    ConnectionService& connection;
    TimeService& time;
    NotificationService& notifications;
    SignalController& signals;
    TelegramTransport& telegram;
    GpioSignals& outputs;
    const char* setupPassword;
    ESP8266WebServer server{80};
    struct NetworkSnapshot {
        char ssid[33] = {};
        char password[65] = {};
        char ip[16] = {};
        char gateway[16] = {};
        char subnet[16] = {};
    } previousWifi;
    bool wifiPending = false;
    uint32_t wifiStarted = 0;
    bool wifiReconfigureScheduled = false;
    uint32_t wifiReconfigureRequestedAt = 0;
    bool accessPointActive = false;
    bool stopAccessPointPending = false;
    bool recovery = false;
    uint32_t recoveryStarted = 0;
    String sessionToken;
    String csrfToken;
    bool sessionBootstrap = false;
    uint32_t sessionLastUse = 0;
    uint8_t failedLogins = 0;
    uint32_t loginBlockedAt = 0;
    bool loginBlocked = false;
    uint8_t testState[8] = {};
    uint8_t testMask = 0;
    bool testActive = false;
    uint32_t testLastSend = 0;
    uint8_t testNext = 0;
    bool restartRequested = false;
    uint32_t restartAt = 0;
    uint64_t uptimeMs = 0;
    uint32_t lastUptimeTick = 0;
    bool uptimeStarted = false;
};

#endif
