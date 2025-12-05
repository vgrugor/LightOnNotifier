#include "presentation/observers/LedObserver.h"

LedObserver::LedObserver(ExternalLedActuator externalLedActuator) : externalLedActuator(externalLedActuator) {}

void LedObserver::update(EventType eventType, const String& message) {
    switch (eventType) {
        //WIFI events
        case EventType::WIFI_START_CONNECT: 
            this->externalLedActuator.setState(HIGH);
            
            break;
        case EventType::WIFI_TRY_CONNECT: 
            break;
        case EventType::WIFI_CONNECTED: 
            this->externalLedActuator.setState(LOW);
            break;
        case EventType::WIFI_RECONNECT: 
            break;

        //telegram bot events
        case EventType::MESSAGE_SEND:
            delay(500);
            this->externalLedActuator.setState(LOW);
            delay(500);
            this->externalLedActuator.setState(HIGH);
            delay(500);
            break;
    }
}
