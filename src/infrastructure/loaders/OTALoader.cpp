#include "infrastructure/loaders/OTALoader.h"

#include <ArduinoOTA.h>

void OTALoader::begin() {
    ArduinoOTA.setHostname(hostname);
    if (password != nullptr && password[0] != '\0') {
        ArduinoOTA.setPassword(password);
    }
    ArduinoOTA.begin();
}
void OTALoader::handle() {
    ArduinoOTA.handle();
}
