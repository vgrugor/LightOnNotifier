#ifndef BUZZER_ACTUATOR_H
    #define BUZZER_ACTUATOR_H

    #include "domain/Actuator.h"
    #include <Arduino.h>

    class BuzzerActuator : public Actuator {
        private:
            int pin;

        public:
            BuzzerActuator(int pin);
            void setState(bool state) override;
            void update() override;
    };

#endif // BUZZER_ACTUATOR_H
