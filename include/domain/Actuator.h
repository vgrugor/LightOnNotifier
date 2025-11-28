#ifndef ACTUATOR_H
    #define ACTUATOR_H

    #include "domain/Device.h"

    class Actuator : public Device {
        public:
            virtual void setState(bool state) = 0;
    };

#endif // ACTUATOR_H
