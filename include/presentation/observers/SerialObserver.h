#ifndef LIGHTONNOTIFIER_PRESENTATION_OBSERVERS_SERIALOBSERVER_H
#define LIGHTONNOTIFIER_PRESENTATION_OBSERVERS_SERIALOBSERVER_H

#include "domain/Event.h"

class SerialObserver : public Observer {
public:
    void onEvent(const Event& event) override;
};
#endif
