#ifndef LED_OBSERVER_H
    #define LED_OBSERVER_H

    #include <Arduino.h>
    #include "Observer.h"
    #include "presentation/EventType.h"
    #include "infrastructure/actuators/InternalLedActuator.h"

    class LedObserver : public Observer {
        private:
            InternalLedActuator internalLedActuator;

        public:
            LedObserver(InternalLedActuator internalLedActuator);
            void update(EventType eventType, const String& message = "") override;
    };

#endif // LED_OBSERVER_H
