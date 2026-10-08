#include "presentation/EventNotifier.h"

bool EventNotifier::addObserver(Observer* observer) {
    if (dispatching || observer == nullptr || count == MAX_OBSERVERS) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (observers[i] == observer) {
            return false;
        }
    }
    observers[count++] = observer;
    return true;
}

bool EventNotifier::removeObserver(Observer* observer) {
    if (dispatching) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (observers[i] == observer) {
            for (size_t j = i + 1; j < count; ++j) {
                observers[j - 1] = observers[j];
            }
            --count;
            return true;
        }
    }
    return false;
}

void EventNotifier::publish(const Event& event) {
    if (dispatching) {
        return;
    }
    dispatching = true;
    for (size_t i = 0; i < count; ++i) {
        observers[i]->onEvent(event);
    }
    dispatching = false;
}
