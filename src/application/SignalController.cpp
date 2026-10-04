#include "application/SignalController.h"

void SignalController::begin() {
    output.setLed(false);
    output.setBuzzer(false);
}

void SignalController::request(Pattern pattern) {
    // Startup has priority. While it plays, coalesce connectivity into one pending pattern.
    if (active == Pattern::STARTUP && pattern != Pattern::STARTUP) {
        if (pattern == Pattern::CONNECTED || requested != Pattern::CONNECTED) {
            requested = pattern;
        }
        return;
    }
    if (pattern == Pattern::CONNECTING &&
        (active == Pattern::CONNECTED || requested == Pattern::CONNECTED)) {
        return;
    }
    requested = pattern;
}

void SignalController::onEvent(const Event& event) {
    switch (event.type) {
    case EventType::LIGHT_ON:
        request(Pattern::STARTUP);
        break;
    case EventType::WIFI_START_CONNECT:
    case EventType::WIFI_RECONNECT:
        ledBase = true;
        break;
    case EventType::WIFI_TRY_CONNECT:
        request(Pattern::CONNECTING);
        break;
    case EventType::WIFI_CONNECTED:
        ledBase = false;
        request(Pattern::CONNECTED);
        break;
    case EventType::MESSAGE_SEND:
        ledBlinkRequested = true;
        ledBase = true;
        break;
    default:
        break;
    }
}

void SignalController::stop() {
    active = Pattern::NONE;
    requested = Pattern::NONE;
    patternStartedFlag = false;
    output.setBuzzer(false);
}

void SignalController::update(uint32_t now) {
    if (button.isPressed()) {
        stop();
    } else {
        if (requested != Pattern::NONE && active != Pattern::STARTUP) {
            active = requested;
            requested = Pattern::NONE;
            patternStarted = now;
            patternStartedFlag = true;
        }
        if (patternStartedFlag) {
            const uint32_t elapsed = uint32_t(now - patternStarted);
            const uint32_t duration = active == Pattern::STARTUP
                                          ? STARTUP_MS
                                          : (active == Pattern::CONNECTED ? 6 * CONNECTED_PULSE_MS
                                                                          : 3 * CONNECT_PULSE_MS);
            if (elapsed >= duration) {
                active = Pattern::NONE;
                patternStartedFlag = false;
                output.setBuzzer(false);
            } else {
                bool on = active == Pattern::STARTUP;
                if (active == Pattern::CONNECTING) {
                    on = elapsed >= CONNECT_PULSE_MS && elapsed < 2 * CONNECT_PULSE_MS;
                } else if (active == Pattern::CONNECTED) {
                    on = (elapsed >= CONNECTED_PULSE_MS && elapsed < 2 * CONNECTED_PULSE_MS) ||
                         (elapsed >= 4 * CONNECTED_PULSE_MS && elapsed < 5 * CONNECTED_PULSE_MS);
                }
                output.setBuzzer(on);
            }
        }
    }
    if (ledBlinkRequested) {
        ledBlinkRequested = false;
        ledBlinkActive = true;
        ledStarted = now;
    }
    if (ledBlinkActive) {
        const uint32_t elapsed = uint32_t(now - ledStarted);
        if (elapsed >= 3 * LED_PHASE_MS) {
            ledBlinkActive = false;
        } else {
            output.setLed(elapsed >= 2 * LED_PHASE_MS);
        }
    }
    if (!ledBlinkActive) {
        output.setLed(ledBase);
    }
}
