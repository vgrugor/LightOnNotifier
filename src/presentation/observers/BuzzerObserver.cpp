#include "presentation/observers/BuzzerObserver.h"

BuzzerObserver::BuzzerObserver(BuzzerActuator buzzerActuator) : buzzerActuator(buzzerActuator) {}

void BuzzerObserver::update(EventType eventType, const String& message) {
    switch (eventType) {
        //WIFI events
        case EventType::WIFI_START_CONNECT: break;
        case EventType::WIFI_TRY_CONNECT: this->signal(1, 100, 100); break;
        case EventType::WIFI_CONNECTED: this->signal(2, 500, 500); break;
        case EventType::WIFI_RECONNECT: break;

        //light events
        case EventType::LIGHT_ON: this->playLongSignalWithCancel(10000); break;
    }
}

void BuzzerObserver::signal(int count, int onTime, int offTime) {
    for (int i = 0; i < count; ++i) {
        if (this->isStopRequested()) {
            break;
        }

        delay(offTime);

        if (this->isStopRequested()) {
            break;
        }

        this->buzzerActuator.setState(HIGH);
        delay(onTime);
        this->buzzerActuator.setState(LOW);

        if (this->isStopRequested()) {
            break;
        }

        delay(offTime);
    }
}

void BuzzerObserver::playLongSignalWithCancel(unsigned long durationMs) {
    if (this->isStopRequested()) {
        return;
    }

    unsigned long start = millis();
    this->buzzerActuator.setState(HIGH);

    while ((millis() - start) < durationMs) {
        if (this->isStopRequested()) {
            break;
        }

        delay(5);
        yield();
    }

    this->buzzerActuator.setState(LOW);
}

bool BuzzerObserver::isStopRequested() const {
    return digitalRead(BUTTON_PIN) == LOW;
}
