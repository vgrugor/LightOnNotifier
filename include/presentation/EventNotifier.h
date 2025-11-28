#ifndef EVENTNOTIFIER_H
    #define EVENTNOTIFIER_H

    #include <vector>
    #include <algorithm>
    #include "presentation/observers/Observer.h"
    #include "presentation/EventType.h"

    class EventNotifier {
        private:
            std::vector<Observer*> observers;
            EventNotifier();
            EventNotifier(const EventNotifier&) = delete;
            EventNotifier& operator=(const EventNotifier&) = delete;

        public:
            static EventNotifier& getInstance();
            void addObserver(Observer* observer);
            void removeObserver(Observer* observer);
            void notifyObservers(EventType eventType, const String& message = "");
    };

#endif // EVENTNOTIFIER_H
