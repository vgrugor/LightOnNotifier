#ifndef BUZZER_OBSERVER_H
    #define BUZZER_OBSERVER_H

    #include <Arduino.h>
    #include "Observer.h"
    #include "presentation/EventType.h"
    #include "infrastructure/actuators/BuzzerActuator.h"
    #include "infrastructure/env.h"

    class BuzzerObserver : public Observer {
        private:
            BuzzerActuator buzzerActuator;

        public:
            BuzzerObserver(BuzzerActuator buzzerActuator);
            void update(EventType eventType, const String& message = "") override;
            void signal(int count, int onTime, int offTime);
    };

#endif // BUZZER_OBSERVER_H
