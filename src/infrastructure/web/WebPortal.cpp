#include "infrastructure/web/WebPortal.h"

#include <algorithm>
#include <string.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <bearssl/bearssl.h>
#include <osapi.h>

#include "infrastructure/web/WebPage.h"

namespace {
constexpr uint32_t SESSION_MS = 30UL * 60UL * 1000UL;
constexpr uint32_t WIFI_CONFIRM_MS = 120000;
constexpr uint32_t WIFI_RESPONSE_MS = 1000;
constexpr uint32_t RECOVERY_MS = 600000;
constexpr uint32_t LOGIN_DELAY_MS = 60000;
constexpr size_t MAX_FORM_BYTES = 2048;
constexpr size_t MAX_HEADER_BYTES = 1024;
constexpr char FIRMWARE_VERSION[] = "0.2.0-dev";

bool validSetupSecret(const char* value) {
    if (value == nullptr || strcmp(value, "unique-device-setup-password") == 0) {
        return false;
    }
    const size_t length = strlen(value);
    if (length < 10 || length > 63) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (value[i] < '!' || value[i] > '~') {
            return false;
        }
    }
    return true;
}

bool boundedPostHeaders(WiFiClient& client) {
    char header[MAX_HEADER_BYTES + 1] = {};
    const uint32_t started = millis();
    size_t length = 0;
    while (uint32_t(millis() - started) < 1000) {
        const size_t available =
            std::min(static_cast<size_t>(client.available()), MAX_HEADER_BYTES);
        if (available > length) {
            length = client.peekBytes(header, available);
            header[length] = '\0';
            if (strstr(header, "\r\n\r\n") != nullptr) {
                break;
            }
            if (length == MAX_HEADER_BYTES) {
                return false;
            }
        }
        yield();
    }
    if (strstr(header, "\r\n\r\n") == nullptr) {
        return false;
    }
    bool hasLength = false;
    size_t bodyLength = 0;
    char* line = header;
    while (*line != '\0' && !(line[0] == '\r' && line[1] == '\n')) {
        char* end = strstr(line, "\r\n");
        if (end == nullptr) {
            return false;
        }
        *end = '\0';
        if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
            return false;
        }
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            if (hasLength) {
                return false;
            }
            hasLength = true;
            const char* value = line + 15;
            while (*value == ' ') {
                ++value;
            }
            if (*value == '\0') {
                return false;
            }
            for (; *value != '\0'; ++value) {
                if (*value < '0' || *value > '9') {
                    return false;
                }
                bodyLength = bodyLength * 10 + static_cast<size_t>(*value - '0');
                if (bodyLength > MAX_FORM_BYTES) {
                    return false;
                }
            }
        }
        line = end + 2;
    }
    return hasLength;
}

const char* outcomeName(NotificationService::Outcome outcome) {
    switch (outcome) {
    case NotificationService::Outcome::DELIVERED:
        return "delivered";
    case NotificationService::Outcome::EXHAUSTED:
        return "exhausted";
    case NotificationService::Outcome::INVALID:
        return "invalid";
    case NotificationService::Outcome::EMPTY:
        return "empty";
    case NotificationService::Outcome::WAITING:
        return "waiting";
    }
    return "invalid";
}
} // namespace

WebPortal::WebPortal(DeviceSettings& settings, SettingsStore& store, ConnectionService& connection,
                     TimeService& time, NotificationService& notifications,
                     SignalController& signals, TelegramTransport& telegram, GpioSignals& outputs,
                     const char* setupPassword)
    : settings(settings), store(store), connection(connection), time(time),
      notifications(notifications), signals(signals), telegram(telegram), outputs(outputs),
      setupPassword(setupPassword) {}

void WebPortal::begin() {
    server.addHook([](const String& method, const String& url, WiFiClient* client,
                      ESP8266WebServer::ContentTypeFunction) {
        if (url.length() > 128 ||
            ((method == "POST" || method == "PUT" || method == "PATCH" || method == "DELETE") &&
             !boundedPostHeaders(*client))) {
            client->print("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n"
                          "Content-Length: 0\r\n\r\n");
            client->stop(1);
            return ESP8266WebServer::CLIENT_MUST_STOP;
        }
        return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
    });
    server.collectHeaders("Cookie", "X-CSRF", "Origin", "Content-Type", "Content-Length");
    routes();
    server.begin();
    if (!settings.configured) {
        startAccessPoint();
    }
}

void WebPortal::routes() {
    server.on("/", HTTP_GET, [this]() {
        server.sendHeader("Cache-Control", "no-store");
        server.sendHeader("Content-Security-Policy",
                          "default-src 'self'; script-src 'self'; style-src 'self'; "
                          "object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
        server.send_P(200, PSTR("text/html; charset=utf-8"), WEB_PAGE);
    });
    server.on("/app.js", HTTP_GET, [this]() {
        server.sendHeader("Cache-Control", "no-store");
        server.send_P(200, PSTR("application/javascript; charset=utf-8"), WEB_SCRIPT);
    });
    server.on("/app.css", HTTP_GET, [this]() {
        server.sendHeader("Cache-Control", "no-store");
        server.send_P(200, PSTR("text/css; charset=utf-8"), WEB_STYLE);
    });
    server.on("/api/login", HTTP_POST, [this]() { login(); });
    server.on("/api/session", HTTP_GET, [this]() { session(); });
    server.on("/api/status", HTTP_GET, [this]() { status(); });
    server.on("/api/settings", HTTP_GET, [this]() { getSettings(); });
    server.on("/api/signals", HTTP_POST, [this]() { saveSignals(); });
    server.on("/api/telegram", HTTP_POST, [this]() { saveTelegram(); });
    server.on("/api/wifi", HTTP_POST, [this]() { applyWifi(); });
    server.on("/api/wifi/confirm", HTTP_POST, [this]() { confirmWifi(); });
    server.on("/api/password", HTTP_POST, [this]() { changePassword(); });
    server.on("/api/preview", HTTP_POST, [this]() { preview(); });
    server.on("/api/test", HTTP_POST, [this]() { startTest(); });
    server.on("/api/reset", HTTP_POST, [this]() { reset(); });
    server.on("/api/restart", HTTP_POST, [this]() { restart(); });
    server.onNotFound([this]() { fail(404, "Не знайдено"); });
}

void WebPortal::sendJson(int code, const String& body) {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(code, "application/json; charset=utf-8", body);
}

void WebPortal::fail(int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String body;
    serializeJson(doc, body);
    sendJson(code, body);
}

bool WebPortal::validRequest(bool mutation) {
    if (!mutation) {
        return true;
    }
    if (!server.header("Content-Type").startsWith("application/x-www-form-urlencoded")) {
        fail(415, "Потрібні дані форми");
        return false;
    }
    const String lengthHeader = server.header("Content-Length");
    if (lengthHeader.length() > 5 || lengthHeader.toInt() > static_cast<int>(MAX_FORM_BYTES)) {
        fail(413, "Завеликий запит");
        return false;
    }
    size_t total = 0;
    for (int index = 0; index < server.args(); ++index) {
        total += server.arg(index).length() + server.argName(index).length();
        if (total > MAX_FORM_BYTES) {
            fail(413, "Завеликий запит");
            return false;
        }
    }
    return true;
}

bool WebPortal::hostAllowed() const {
    String host = server.hostHeader();
    if (host.endsWith(":80")) {
        host.remove(host.length() - 3);
    }
    return (connection.isConnected() && host == WiFi.localIP().toString()) ||
           (accessPointActive && host == WiFi.softAPIP().toString());
}

bool WebPortal::authorized(bool mutation) {
    if (!hostAllowed()) {
        fail(403, "Недійсна адреса пристрою");
        return false;
    }
    const String cookie = server.header("Cookie");
    const String marker = "lon_session=" + sessionToken;
    const int offset = cookie.indexOf(marker);
    if (sessionToken.isEmpty() || offset < 0 ||
        (offset > 0 && cookie[offset - 1] != ' ' && cookie[offset - 1] != ';') ||
        (static_cast<size_t>(offset + marker.length()) < cookie.length() &&
         cookie[offset + marker.length()] != ';') ||
        uint32_t(millis() - sessionLastUse) >= SESSION_MS) {
        fail(401, "Потрібен вхід");
        return false;
    }
    if (mutation && (server.header("X-CSRF") != csrfToken ||
                     server.header("Origin") != "http://" + server.hostHeader())) {
        fail(403, "Недійсний запит");
        return false;
    }
    sessionLastUse = millis();
    return validRequest(mutation);
}

String WebPortal::randomHex(size_t bytes) {
    uint8_t randomBytes[16] = {};
    if (bytes > sizeof(randomBytes) || os_get_random(randomBytes, bytes) != 0) {
        return String();
    }
    static constexpr char HEX_DIGITS[] = "0123456789abcdef";
    char output[33] = {};
    for (size_t i = 0; i < bytes; ++i) {
        output[2 * i] = HEX_DIGITS[randomBytes[i] >> 4];
        output[2 * i + 1] = HEX_DIGITS[randomBytes[i] & 15];
    }
    return String(output);
}

void WebPortal::hashPassword(const String& password, const uint8_t* salt, uint8_t* result) const {
    br_sha256_context hash;
    br_sha256_init(&hash);
    br_sha256_update(&hash, salt, 16);
    br_sha256_update(&hash, password.c_str(), password.length());
    br_sha256_out(&hash, result);
    for (unsigned round = 0; round < 4096; ++round) {
        br_sha256_init(&hash);
        br_sha256_update(&hash, result, 32);
        br_sha256_update(&hash, salt, 16);
        br_sha256_out(&hash, result);
    }
}

bool WebPortal::checkPassword(const String& password) const {
    if (password.isEmpty() || password.length() > 64) {
        return false;
    }
    if (settings.adminConfigured) {
        uint8_t candidate[32] = {};
        hashPassword(password, settings.adminSalt, candidate);
        uint8_t difference = 0;
        for (size_t i = 0; i < sizeof(candidate); ++i) {
            difference |= candidate[i] ^ settings.adminHash[i];
        }
        if (difference == 0) {
            return true;
        }
    }
    return (!settings.adminConfigured || recovery) && validSetupSecret(setupPassword) &&
           password == setupPassword;
}

void WebPortal::login() {
    if (!hostAllowed()) {
        fail(403, "Недійсна адреса пристрою");
        return;
    }
    if (!validRequest(true)) {
        return;
    }
    const uint32_t now = millis();
    if (loginBlocked && uint32_t(now - loginBlockedAt) < LOGIN_DELAY_MS) {
        fail(429, "Спробуйте пізніше");
        return;
    }
    loginBlocked = false;
    if (!checkPassword(server.arg("password"))) {
        if (++failedLogins >= 5) {
            loginBlocked = true;
            loginBlockedAt = now;
            failedLogins = 0;
        }
        fail(401, "Неправильний пароль");
        return;
    }
    failedLogins = 0;
    sessionBootstrap = setupPassword != nullptr && server.arg("password") == setupPassword &&
                       (!settings.adminConfigured || recovery);
    sessionToken = randomHex(16);
    csrfToken = randomHex(16);
    if (sessionToken.isEmpty() || csrfToken.isEmpty()) {
        fail(503, "Не вдалося створити сеанс");
        return;
    }
    sessionLastUse = now;
    server.sendHeader("Set-Cookie",
                      "lon_session=" + sessionToken + "; HttpOnly; SameSite=Strict; Path=/");
    JsonDocument doc;
    doc["csrf"] = csrfToken;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WebPortal::session() {
    if (!authorized()) {
        return;
    }
    JsonDocument doc;
    doc["csrf"] = csrfToken;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WebPortal::status() {
    if (!authorized()) {
        return;
    }
    JsonDocument doc;
    doc["wifi"] = connection.isConnected() ? "connected" : "disconnected";
    doc["ip"] = connection.isConnected() ? WiFi.localIP().toString() : "";
    doc["uptimeMs"] = uptimeMs;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["timeReady"] = time.isReady();
    size_t delivered = 0;
    for (size_t i = 0; i < notifications.recipientCount(); ++i) {
        delivered += notifications.recipientOutcome(i) == NotificationService::Outcome::DELIVERED;
    }
    doc["delivery"] =
        delivered != 0 && notifications.outcome() == NotificationService::Outcome::WAITING
            ? "partial"
            : outcomeName(notifications.outcome());
    doc["ledActive"] = outputs.isLedActive();
    doc["buzzerActive"] = outputs.isBuzzerActive();
    doc["wifiPending"] = wifiPending;
    doc["recovery"] = recovery;
    doc["storageReady"] = store.available();
    doc["unsupportedSchema"] = store.hasUnsupportedSchema();
    doc["configured"] = settings.configured;
    JsonArray recipients = doc["recipients"].to<JsonArray>();
    for (size_t i = 0; i < notifications.recipientCount(); ++i) {
        JsonObject item = recipients.add<JsonObject>();
        item["index"] = i;
        item["outcome"] = outcomeName(notifications.recipientOutcome(i));
        item["attempts"] = notifications.attempts(i);
    }
    JsonArray tests = doc["tests"].to<JsonArray>();
    for (size_t i = 0; i < settings.recipientCount; ++i) {
        tests.add(testState[i]);
    }
    doc["testActive"] = testActive;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WebPortal::getSettings() {
    if (!authorized()) {
        return;
    }
    JsonDocument doc;
    doc["revision"] = settings.revision;
    doc["ssid"] = settings.ssid;
    doc["ip"] = settings.ip;
    doc["gateway"] = settings.gateway;
    doc["subnet"] = settings.subnet;
    doc["wifiPasswordSet"] = settings.wifiPassword[0] != '\0';
    doc["tokenSet"] = settings.botToken[0] != '\0';
    doc["adminConfigured"] = settings.adminConfigured;
    doc["message"] = settings.message;
    JsonArray ids = doc["recipients"].to<JsonArray>();
    for (size_t i = 0; i < settings.recipientCount; ++i) {
        ids.add(settings.recipients[i]);
    }
    doc["startupSound"] = settings.startupSound;
    doc["wifiProgressSound"] = settings.wifiProgressSound;
    doc["wifiConnectedSound"] = settings.wifiConnectedSound;
    doc["ledEnabled"] = settings.ledEnabled;
    doc["deliveryBlink"] = settings.deliveryBlink;
    doc["startupLed"] = static_cast<uint8_t>(settings.startupLed);
    doc["connectingLed"] = static_cast<uint8_t>(settings.connectingLed);
    doc["waitingLed"] = static_cast<uint8_t>(settings.waitingLed);
    doc["idleLed"] = static_cast<uint8_t>(settings.idleLed);
    doc["errorLed"] = static_cast<uint8_t>(settings.errorLed);
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

bool WebPortal::checkRevision() {
    const String value = server.arg("revision");
    if (value.isEmpty() || value != String(settings.revision)) {
        fail(409, "Налаштування змінилися в іншому браузері");
        return false;
    }
    if (wifiPending) {
        fail(409, "Підтвердьте Wi-Fi або дочекайтеся повернення старих налаштувань");
        return false;
    }
    if (testActive) {
        fail(409, "Дочекайтеся завершення тесту");
        return false;
    }
    return true;
}

bool WebPortal::copyField(char* target, size_t capacity, const String& value) {
    if (value.length() >= capacity || value.indexOf('\0') >= 0) {
        return false;
    }
    memcpy(target, value.c_str(), value.length() + 1);
    return true;
}

bool WebPortal::saveCandidate(DeviceSettings& candidate) {
    if (memcmp(&candidate, &settings, sizeof(candidate)) == 0) {
        sendJson(200, "{\"saved\":false}");
        return true;
    }
    candidate.revision = settings.revision + 1;
    if (!store.save(candidate)) {
        fail(500, "Не вдалося зберегти налаштування");
        return false;
    }
    settings = candidate;
    if (settings.configured && accessPointActive && !recovery && !wifiPending) {
        stopAccessPointPending = true;
    }
    sendJson(200, "{\"saved\":true}");
    return true;
}

void WebPortal::saveSignals() {
    if (!authorized(true) || !checkRevision()) {
        return;
    }
    DeviceSettings candidate = settings;
    candidate.startupSound = server.arg("startupSound") == "1";
    candidate.wifiProgressSound = server.arg("wifiProgressSound") == "1";
    candidate.wifiConnectedSound = server.arg("wifiConnectedSound") == "1";
    candidate.ledEnabled = server.arg("ledEnabled") == "1";
    candidate.deliveryBlink = server.arg("deliveryBlink") == "1";
    const char* names[] = {"startupLed", "connectingLed", "waitingLed", "idleLed", "errorLed"};
    LedMode* modes[] = {&candidate.startupLed, &candidate.connectingLed, &candidate.waitingLed,
                        &candidate.idleLed, &candidate.errorLed};
    for (size_t i = 0; i < 5; ++i) {
        const String value = server.arg(names[i]);
        if (value != "0" && value != "1" && value != "2") {
            fail(400, "Неправильний режим світлодіода");
            return;
        }
        *modes[i] = static_cast<LedMode>(value.toInt());
    }
    saveCandidate(candidate);
}

void WebPortal::saveTelegram() {
    if (!authorized(true) || !checkRevision()) {
        return;
    }
    DeviceSettings candidate = settings;
    const String token = server.arg("token");
    if (!token.isEmpty() && !copyField(candidate.botToken, sizeof(candidate.botToken), token)) {
        fail(400, "Завеликий токен");
        return;
    }
    if (!copyField(candidate.message, sizeof(candidate.message), server.arg("message"))) {
        fail(400, "Повідомлення перевищує 512 байтів");
        return;
    }
    const String list = server.arg("recipients");
    memset(candidate.recipients, 0, sizeof(candidate.recipients));
    candidate.recipientCount = 0;
    int begin = 0;
    while (begin < static_cast<int>(list.length())) {
        int end = list.indexOf('\n', begin);
        if (end < 0) {
            end = list.length();
        }
        String value = list.substring(begin, end);
        value.trim();
        if (candidate.recipientCount >= 8 || value.isEmpty() ||
            !copyField(candidate.recipients[candidate.recipientCount], 34, value)) {
            fail(400, "Неправильний список одержувачів");
            return;
        }
        ++candidate.recipientCount;
        begin = end + 1;
    }
    if (!validTelegramSettings(candidate)) {
        fail(400, "Неправильні налаштування Telegram");
        return;
    }
    candidate.configured = validNetworkSettings(candidate);
    if (saveCandidate(candidate)) {
        memset(testState, 0, sizeof(testState));
    }
}

void WebPortal::applyWifi() {
    if (!authorized(true) || !checkRevision()) {
        return;
    }
    if (wifiPending) {
        fail(409, "Дочекайтеся підтвердження або відновлення попередньої мережі");
        return;
    }
    DeviceSettings candidate = settings;
    const String password = server.arg("password");
    if (!copyField(candidate.ssid, sizeof(candidate.ssid), server.arg("ssid")) ||
        (!password.isEmpty() &&
         !copyField(candidate.wifiPassword, sizeof(candidate.wifiPassword), password)) ||
        !copyField(candidate.ip, sizeof(candidate.ip), server.arg("ip")) ||
        !copyField(candidate.gateway, sizeof(candidate.gateway), server.arg("gateway")) ||
        !copyField(candidate.subnet, sizeof(candidate.subnet), server.arg("subnet"))) {
        fail(400, "Завелике мережеве поле");
        return;
    }
    if (server.arg("openNetwork") == "1") {
        candidate.wifiPassword[0] = '\0';
    }
    if (!validNetworkSettings(candidate)) {
        fail(400, "Неправильні статичні мережеві параметри");
        return;
    }
    memcpy(previousWifi.ssid, settings.ssid, sizeof(previousWifi.ssid));
    memcpy(previousWifi.password, settings.wifiPassword, sizeof(previousWifi.password));
    memcpy(previousWifi.ip, settings.ip, sizeof(previousWifi.ip));
    memcpy(previousWifi.gateway, settings.gateway, sizeof(previousWifi.gateway));
    memcpy(previousWifi.subnet, settings.subnet, sizeof(previousWifi.subnet));
    settings = candidate;
    wifiPending = true;
    wifiReconfigureScheduled = true;
    wifiReconfigureRequestedAt = millis();
    sendJson(202, "{\"pending\":true}");
}

void WebPortal::confirmWifi() {
    if (!authorized(true) || !wifiPending) {
        return;
    }
    if (wifiReconfigureScheduled || !connection.isConnected() ||
        server.client().localIP() != WiFi.localIP()) {
        fail(409, "Відкрийте нову IP-адресу для підтвердження");
        return;
    }
    DeviceSettings candidate = settings;
    candidate.configured = validTelegramSettings(candidate);
    candidate.revision = settings.revision + 1;
    if (!store.save(candidate)) {
        rollbackWifi(millis());
        fail(500, "Не вдалося зберегти Wi-Fi");
        return;
    }
    settings = candidate;
    wifiPending = false;
    if (settings.configured && !recovery) {
        stopAccessPointPending = true;
    }
    sendJson(200, "{\"saved\":true}");
}

void WebPortal::rollbackWifi(uint32_t now) {
    if (!wifiPending) {
        return;
    }
    memcpy(settings.ssid, previousWifi.ssid, sizeof(previousWifi.ssid));
    memcpy(settings.wifiPassword, previousWifi.password, sizeof(previousWifi.password));
    memcpy(settings.ip, previousWifi.ip, sizeof(previousWifi.ip));
    memcpy(settings.gateway, previousWifi.gateway, sizeof(previousWifi.gateway));
    memcpy(settings.subnet, previousWifi.subnet, sizeof(previousWifi.subnet));
    wifiPending = false;
    wifiReconfigureScheduled = false;
    connection.reconfigure(now);
}

void WebPortal::changePassword() {
    if (!authorized(true) || !checkRevision()) {
        return;
    }
    const String password = server.arg("password");
    if (password.length() < 10 || password.length() > 64) {
        fail(400, "Пароль має містити 10–64 байти");
        return;
    }
    DeviceSettings candidate = settings;
    if (os_get_random(candidate.adminSalt, sizeof(candidate.adminSalt)) != 0) {
        fail(503, "Джерело випадкових чисел недоступне");
        return;
    }
    hashPassword(password, candidate.adminSalt, candidate.adminHash);
    candidate.adminConfigured = true;
    if (saveCandidate(candidate)) {
        sessionToken = String();
        csrfToken = String();
    }
}

void WebPortal::preview() {
    if (!authorized(true)) {
        return;
    }
    const String kind = server.arg("kind");
    SignalController::Preview selected = SignalController::Preview::LED;
    if (kind == "startup") {
        selected = SignalController::Preview::STARTUP;
    } else if (kind == "connecting") {
        selected = SignalController::Preview::CONNECTING;
    } else if (kind == "connected") {
        selected = SignalController::Preview::CONNECTED;
    } else if (kind != "led") {
        fail(400, "Невідомий попередній перегляд");
        return;
    }
    if (kind != "led" && server.arg("enabled") != "1") {
        fail(409, "Увімкніть цей звук у формі для перегляду");
        return;
    }
    const String modeValue = server.arg("mode");
    if (kind == "led" && modeValue != "0" && modeValue != "1" && modeValue != "2") {
        fail(400, "Неправильний режим світлодіода");
        return;
    }
    if (!signals.startPreview(selected, static_cast<LedMode>(modeValue.toInt()), millis())) {
        fail(409, "Перегляд недоступний під час запуску або коли світлодіод вимкнений");
        return;
    }
    sendJson(202, "{\"preview\":true}");
}

void WebPortal::startTest() {
    if (!authorized(true) || testActive || !settings.configured || !connection.isConnected() ||
        !time.isReady() || !signals.startupFinished() ||
        (notifications.outcome() != NotificationService::Outcome::DELIVERED &&
         notifications.outcome() != NotificationService::Outcome::EXHAUSTED)) {
        fail(409, "Тест недоступний, поки триває стартова доставка");
        return;
    }
    const String mask = server.arg("mask");
    for (size_t index = 0; index < mask.length(); ++index) {
        if (mask[index] < '0' || mask[index] > '9') {
            fail(400, "Виберіть одержувачів");
            return;
        }
    }
    if (mask.isEmpty() || mask.length() > 3 || mask.toInt() <= 0 || mask.toInt() > 255) {
        fail(400, "Виберіть одержувачів");
        return;
    }
    testMask = static_cast<uint8_t>(mask.toInt());
    if ((testMask >> settings.recipientCount) != 0) {
        fail(400, "Невідомий одержувач");
        return;
    }
    memset(testState, 0, sizeof(testState));
    testNext = 0;
    testLastSend = millis();
    testActive = true;
    telegram.setToken(settings.botToken);
    sendJson(202, "{\"queued\":true}");
}

void WebPortal::reset() {
    if (!authorized(true) || !checkRevision()) {
        return;
    }
    DeviceSettings candidate = {};
    candidate.revision = settings.revision + 1;
    if (!store.reset(candidate)) {
        fail(500, "Не вдалося скинути налаштування");
        return;
    }
    settings = candidate;
    sendJson(200, "{\"saved\":true}");
    testActive = false;
    sessionToken = String();
    csrfToken = String();
    connection.reconfigure(millis());
    startAccessPoint();
}

void WebPortal::restart() {
    if (!authorized(true)) {
        return;
    }
    restartRequested = true;
    restartAt = millis();
    sendJson(202, "{\"restarting\":true}");
}

void WebPortal::startAccessPoint() {
    if (accessPointActive || !validSetupSecret(setupPassword)) {
        return;
    }
    WiFi.mode(WIFI_AP_STA);
    char ssid[32] = {};
    snprintf(ssid, sizeof(ssid), "LightOn-%06X", ESP.getChipId());
    accessPointActive = WiFi.softAP(ssid, setupPassword);
}

void WebPortal::stopAccessPoint() {
    if (accessPointActive) {
        WiFi.softAPdisconnect(true);
        accessPointActive = false;
    }
}

void WebPortal::enterRecovery(uint32_t now) {
    recovery = true;
    recoveryStarted = now;
    startAccessPoint();
}

void WebPortal::update(uint32_t now) {
    if (uptimeStarted) {
        uptimeMs += uint32_t(now - lastUptimeTick);
    } else {
        uptimeStarted = true;
        uptimeMs = now;
    }
    lastUptimeTick = now;
    if (restartRequested && uint32_t(now - restartAt) >= 1000) {
        ESP.restart();
    }
    if (wifiPending && !wifiReconfigureScheduled &&
        (connection.state() == ConnectionService::State::INVALID ||
         connection.state() == ConnectionService::State::RETRY_WAIT ||
         uint32_t(now - wifiStarted) >= WIFI_CONFIRM_MS)) {
        rollbackWifi(now);
    }
    if (recovery && !wifiPending && uint32_t(now - recoveryStarted) >= RECOVERY_MS) {
        recovery = false;
        if (sessionBootstrap && settings.adminConfigured) {
            sessionToken = String();
            csrfToken = String();
            sessionBootstrap = false;
        }
        if (settings.configured) {
            stopAccessPoint();
        }
    }
    if (testActive && uint32_t(now - testLastSend) >= NotificationService::SEND_GAP_MS &&
        connection.isConnected() && time.isReady()) {
        while (testNext < settings.recipientCount && !(testMask & (1U << testNext))) {
            ++testNext;
        }
        if (testNext >= settings.recipientCount) {
            testActive = false;
        } else {
            char message[NotificationService::MAX_MESSAGE_BYTES + 1] = "Тест: ";
            const size_t prefix = strlen(message);
            const size_t capacity = NotificationService::MAX_MESSAGE_BYTES - prefix;
            size_t length = strlen(settings.message);
            if (length > capacity) {
                length = capacity;
                while (length != 0 &&
                       (static_cast<uint8_t>(settings.message[length]) & 0xC0) == 0x80) {
                    --length;
                }
            }
            memcpy(message + prefix, settings.message, length);
            message[prefix + length] = '\0';
            const bool sent = telegram.send(settings.recipients[testNext], message);
            testState[testNext] = sent ? 1 : 2;
            ++testNext;
            testLastSend = millis();
        }
    }
    server.handleClient();
    if (wifiReconfigureScheduled &&
        uint32_t(millis() - wifiReconfigureRequestedAt) >= WIFI_RESPONSE_MS) {
        wifiReconfigureScheduled = false;
        wifiStarted = millis();
        connection.reconfigure(wifiStarted);
    }
    if (stopAccessPointPending) {
        stopAccessPointPending = false;
        stopAccessPoint();
    }
}
