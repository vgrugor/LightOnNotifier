#include "presentation/EventNotifier.h"

EventNotifier::EventNotifier() {}

EventNotifier& EventNotifier::getInstance() {
    static EventNotifier instance;
    return instance;
}

void EventNotifier::addObserver(Observer* observer) {
    observers.push_back(observer);
}

void EventNotifier::removeObserver(Observer* observer) {
    observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
}

void EventNotifier::notifyObservers(EventType eventType, const String& message) {
    for (auto observer : observers) {
        observer->update(eventType, message);
    }
}
