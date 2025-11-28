#include "presentation/observers/LedObserver.h"

LedObserver::LedObserver(InternalLedActuator internalLedActuator) : internalLedActuator(internalLedActuator) {}

void LedObserver::update(EventType eventType, const String& message) {
    switch (eventType) {
        //WIFI events
        case EventType::WIFI_START_CONNECT: 
            this->internalLedActuator.setState(HIGH);
            
            break;
        case EventType::WIFI_TRY_CONNECT: 
            break;
        case EventType::WIFI_CONNECTED: 
            this->internalLedActuator.setState(LOW);
            break;
        case EventType::WIFI_RECONNECT: 
            break;

        //telegram bot events
        case EventType::MESSAGE_SEND:
            delay(500);
            this->internalLedActuator.setState(LOW);
            delay(500);
            this->internalLedActuator.setState(HIGH);
            delay(500);
            break;
    }
}
