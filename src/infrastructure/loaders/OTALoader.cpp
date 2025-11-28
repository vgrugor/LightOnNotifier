#include "infrastructure/loaders/OTALoader.h"

OTALoader::OTALoader(const char* hostname, const char* password) 
    : hostname(hostname), password(password) {}

void OTALoader::begin() {
    ArduinoOTA.setHostname(this->hostname);

    if (this->password != nullptr) {
        ArduinoOTA.setPassword(this->password);
    }

    ArduinoOTA.begin();
}

void OTALoader::handle() {
    ArduinoOTA.handle();
}

void OTALoader::setPassword(const char* password) {
    this->password = password;
    ArduinoOTA.setPassword(this->password);
}
