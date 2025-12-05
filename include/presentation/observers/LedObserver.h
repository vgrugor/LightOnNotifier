#ifndef LED_OBSERVER_H
    #define LED_OBSERVER_H

    #include <Arduino.h>
    #include "Observer.h"
    #include "presentation/EventType.h"
    #include "infrastructure/actuators/ExternalLedActuator.h"

    class LedObserver : public Observer {
        private:
            ExternalLedActuator externalLedActuator;

        public:
            LedObserver(ExternalLedActuator externalLedActuator);
            void update(EventType eventType, const String& message = "") override;
    };

#endif // LED_OBSERVER_H
