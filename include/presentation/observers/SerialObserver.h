#ifndef SERIAL_OBSERVER_H
    #define SERIAL_OBSERVER_H

    #include <Arduino.h>
    #include "Observer.h"
    #include "presentation/EventType.h"

    class SerialObserver : public Observer {
        public:
            void update(EventType eventType, const String& message = "") override;
    };

#endif // SERIAL_OBSERVER_H
