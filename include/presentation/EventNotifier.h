#ifndef LIGHTONNOTIFIER_PRESENTATION_EVENTNOTIFIER_H
#define LIGHTONNOTIFIER_PRESENTATION_EVENTNOTIFIER_H

#include <stddef.h>

#include "domain/Event.h"

// Observers are borrowed, must outlive dispatcher. Mutations and nested dispatch are rejected.
class EventNotifier : public EventSink {
public:
    static constexpr size_t MAX_OBSERVERS = 4;
    bool addObserver(Observer* observer);
    bool removeObserver(Observer* observer);
    void publish(const Event& event) override;

private:
    Observer* observers[MAX_OBSERVERS] = {};
    size_t count = 0;
    bool dispatching = false;
};
#endif
