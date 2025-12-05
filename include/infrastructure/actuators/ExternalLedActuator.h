#ifndef LED_ACTUATOR_H
    #define LED_ACTUATOR_H

    #include "domain/Actuator.h"
    #include <Arduino.h>

    class ExternalLedActuator : public Actuator {
        private:
            int pin;

        public:
            ExternalLedActuator(int pin);
            void setState(bool state) override;
            void update() override;
    };

#endif // LED_ACTUATOR_H
