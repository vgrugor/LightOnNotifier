#include "application/SignalController.h"

void SignalController::begin() {
    output.setLedBrightness(0);
    output.setBuzzer(false);
}

bool SignalController::quietNow() const {
    if (settings == nullptr || !settings->quietHoursEnabled) {
        return false;
    }
    uint8_t hour = 0;
    if (!time.localHour(hour) || hour > 23) {
        return true;
    }
    const uint8_t start = settings->quietStartHour;
    const uint8_t end = settings->quietEndHour;
    return start < end ? hour >= start && hour < end : hour >= start || hour < end;
}

bool SignalController::quietHoursActive() const {
    return quietNow();
}

void SignalController::setLed(bool active) {
    output.setLedBrightness(active && (settings == nullptr || settings->ledEnabled) ? ledBrightness
                                                                                    : 0);
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
    if (event.type == EventType::WIFI_START_CONNECT || event.type == EventType::WIFI_RECONNECT ||
        event.type == EventType::WIFI_CONNECTED) {
        previewNeedsSilence = previewActive && previewKind != Preview::LED;
        previewActive = false;
    }
    switch (event.type) {
    case EventType::LIGHT_ON:
        startupWindow = settings != nullptr;
        startupLedStartedFlag = false;
        if (settings == nullptr || settings->startupSound) {
            request(Pattern::STARTUP);
        }
        break;
    case EventType::WIFI_START_CONNECT:
    case EventType::WIFI_RECONNECT:
        ledBase = true;
        break;
    case EventType::WIFI_TRY_CONNECT:
        if (settings == nullptr || settings->wifiProgressSound) {
            request(Pattern::CONNECTING);
        }
        break;
    case EventType::WIFI_CONNECTED:
        ledBase = false;
        if (settings == nullptr || settings->wifiConnectedSound) {
            request(Pattern::CONNECTED);
        }
        break;
    case EventType::MESSAGE_SEND:
        ledBlinkRequested = settings == nullptr || settings->deliveryBlink;
        ledBase = true;
        break;
    default:
        break;
    }
}

bool SignalController::startPreview(Preview kind, LedMode mode, uint32_t now) {
    if (settings == nullptr || !startupFinished() ||
        (kind == Preview::LED && !settings->ledEnabled) || (kind != Preview::LED && quietNow())) {
        return false;
    }
    previewKind = kind;
    previewLedMode = mode;
    previewStarted = now;
    previewActive = true;
    return true;
}

void SignalController::stop() {
    active = Pattern::NONE;
    requested = Pattern::NONE;
    patternStartedFlag = false;
    output.setBuzzer(false);
}

void SignalController::update(uint32_t now) {
    const bool quiet = quietNow();
    ledBrightness = quiet ? static_cast<uint8_t>(settings->quietLedBrightnessPercent) : 100;
    if (quiet) {
        if (previewActive && previewKind != Preview::LED) {
            previewActive = false;
        }
        stop();
    }
    if (settings != nullptr) {
        if (startupWindow && !startupLedStartedFlag) {
            startupLedStarted = now;
            startupLedStartedFlag = true;
        }
        if (startupWindow && startupLedStartedFlag &&
            uint32_t(now - startupLedStarted) >= STARTUP_MS) {
            startupWindow = false;
        }
        const bool activeDisabled =
            (active == Pattern::STARTUP && !settings->startupSound) ||
            (active == Pattern::CONNECTING && !settings->wifiProgressSound) ||
            (active == Pattern::CONNECTED && !settings->wifiConnectedSound);
        const bool requestedDisabled =
            (requested == Pattern::STARTUP && !settings->startupSound) ||
            (requested == Pattern::CONNECTING && !settings->wifiProgressSound) ||
            (requested == Pattern::CONNECTED && !settings->wifiConnectedSound);
        if (activeDisabled) {
            active = Pattern::NONE;
            patternStartedFlag = false;
            output.setBuzzer(false);
        }
        if (requestedDisabled) {
            requested = Pattern::NONE;
        }
    }
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
            const uint32_t duration =
                active == Pattern::STARTUP
                    ? (settings == nullptr ? STARTUP_MS : settings->startupSoundSeconds * 1000UL)
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
    if (previewNeedsSilence) {
        previewNeedsSilence = false;
        if (!patternStartedFlag) {
            output.setBuzzer(false);
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
            setLed(elapsed >= 2 * LED_PHASE_MS);
        }
    }
    if (settings == nullptr) {
        if (!ledBlinkActive) {
            setLed(ledBase);
        }
        return;
    }
    if (operatingState == OperatingState::ERROR) {
        ledBlinkActive = false;
    }
    if (!settings->ledEnabled) {
        ledBlinkActive = false;
        if (previewActive && previewKind == Preview::LED) {
            previewActive = false;
        }
        setLed(false);
    } else {
        if (ledBlinkActive && !previewActive) {
            return;
        }
        LedMode mode = settings->startupLed;
        if (operatingState == OperatingState::ERROR) {
            mode = settings->errorLed;
        } else if (!startupWindow) {
            switch (operatingState) {
            case OperatingState::CONNECTING:
                mode = settings->connectingLed;
                break;
            case OperatingState::WAITING:
                mode = settings->waitingLed;
                break;
            case OperatingState::IDLE:
                mode = settings->idleLed;
                break;
            case OperatingState::ERROR:
                mode = settings->errorLed;
                break;
            }
        }
        if (operatingState != lastOperatingState) {
            lastOperatingState = operatingState;
            stateStarted = now;
        }
        const uint32_t phaseStart = startupWindow ? startupLedStarted : stateStarted;
        setLed(mode == LedMode::STEADY ||
               (mode == LedMode::BLINK && uint32_t(now - phaseStart) % 1000 < 500));
    }
    if (previewActive) {
        const uint32_t elapsed = uint32_t(now - previewStarted);
        if (elapsed >= 3000 || (previewKind != Preview::LED && button.isPressed())) {
            previewActive = false;
            output.setBuzzer(false);
            return;
        }
        if (previewKind == Preview::LED) {
            setLed(previewLedMode == LedMode::STEADY ||
                   (previewLedMode == LedMode::BLINK && elapsed % 1000 < 500));
        } else {
            bool on = previewKind == Preview::STARTUP;
            if (previewKind == Preview::CONNECTING) {
                on = elapsed >= CONNECT_PULSE_MS && elapsed < 2 * CONNECT_PULSE_MS;
            } else if (previewKind == Preview::CONNECTED) {
                on = (elapsed >= CONNECTED_PULSE_MS && elapsed < 2 * CONNECTED_PULSE_MS) ||
                     (elapsed >= 4 * CONNECTED_PULSE_MS && elapsed < 5 * CONNECTED_PULSE_MS);
            }
            output.setBuzzer(on);
        }
    }
}
