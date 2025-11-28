#ifndef OBSERVER_H
    #define OBSERVER_H

    #include "presentation/EventType.h"
    #include <Arduino.h>

    class Observer {
        public:
            virtual void update(EventType eventType, const String& message = "") = 0;
    };

#endif
