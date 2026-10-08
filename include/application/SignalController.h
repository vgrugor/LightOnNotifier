#ifndef LIGHTONNOTIFIER_APPLICATION_SIGNALCONTROLLER_H
#define LIGHTONNOTIFIER_APPLICATION_SIGNALCONTROLLER_H

#include <stdint.h>

#include "application/Ports.h"
#include "domain/Event.h"

class SignalController : public Observer {
public:
    static constexpr uint32_t STARTUP_MS = 10000;
    static constexpr uint32_t CONNECT_PULSE_MS = 100;
    static constexpr uint32_t CONNECTED_PULSE_MS = 500;
    static constexpr uint32_t LED_PHASE_MS = 500;
    SignalController(SignalOutput& output, ButtonPort& button) : output(output), button(button) {}
    void begin();
    void onEvent(const Event& event) override;
    void update(uint32_t now);

private:
    enum class Pattern { NONE, STARTUP, CONNECTING, CONNECTED };
    void request(Pattern pattern);
    void stop();
    SignalOutput& output;
    ButtonPort& button;
    Pattern active = Pattern::NONE;
    Pattern requested = Pattern::NONE;
    uint32_t patternStarted = 0;
    uint32_t ledStarted = 0;
    bool patternStartedFlag = false;
    bool ledBlinkRequested = false;
    bool ledBlinkActive = false;
    bool ledBase = false;
};
#endif
