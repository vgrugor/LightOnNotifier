#include "presentation/observers/BuzzerObserver.h"

BuzzerObserver::BuzzerObserver(BuzzerActuator buzzerActuator) : buzzerActuator(buzzerActuator) {}

void BuzzerObserver::update(EventType eventType, const String& message) {
    unsigned int delayMillis;

    switch (eventType) {
        //WIFI events
        case EventType::WIFI_START_CONNECT: break;
        case EventType::WIFI_TRY_CONNECT: this->signal(1, 100, 100); break;
        case EventType::WIFI_CONNECTED: this->signal(2, 500, 500); break;
        case EventType::WIFI_RECONNECT: break;

        //light events
        case EventType::LIGHT_ON: this->signal(3, 10000, 2000); break;
    }
}

void BuzzerObserver::signal(int count, int onTime, int offTime) {
    for (int i = 0; i < count; ++i) {
        delay(offTime);
        this->buzzerActuator.setState(HIGH);
        delay(onTime);
        this->buzzerActuator.setState(LOW);
        delay(offTime);
    }
}
