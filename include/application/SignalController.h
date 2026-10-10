#ifndef LIGHTONNOTIFIER_APPLICATION_SIGNALCONTROLLER_H
#define LIGHTONNOTIFIER_APPLICATION_SIGNALCONTROLLER_H

#include <stdint.h>

#include "application/DeviceSettings.h"
#include "application/Ports.h"
#include "domain/Event.h"

class SignalController : public Observer {
public:
    static constexpr uint32_t STARTUP_MS = 10000;
    static constexpr uint32_t CONNECT_PULSE_MS = 100;
    static constexpr uint32_t CONNECTED_PULSE_MS = 500;
    static constexpr uint32_t LED_PHASE_MS = 500;
    SignalController(SignalOutput& output, ButtonPort& button, TimePort& time)
        : output(output), button(button), time(time) {}
    void begin();
    void onEvent(const Event& event) override;
    void update(uint32_t now);
    enum class OperatingState { CONNECTING, WAITING, IDLE, ERROR };
    void setSettings(const DeviceSettings& value) {
        settings = &value;
    }
    void setOperatingState(OperatingState value) {
        operatingState = value;
    }
    bool startupFinished() const {
        return !startupWindow && active != Pattern::STARTUP && requested != Pattern::STARTUP;
    }
    bool quietHoursActive() const;
    enum class Preview { STARTUP, CONNECTING, CONNECTED, LED };
    bool startPreview(Preview kind, LedMode mode, uint32_t now);

private:
    enum class Pattern { NONE, STARTUP, CONNECTING, CONNECTED };
    void request(Pattern pattern);
    void stop();
    void setLed(bool active);
    bool quietNow() const;
    SignalOutput& output;
    ButtonPort& button;
    TimePort& time;
    Pattern active = Pattern::NONE;
    Pattern requested = Pattern::NONE;
    uint32_t patternStarted = 0;
    uint32_t ledStarted = 0;
    bool patternStartedFlag = false;
    bool ledBlinkRequested = false;
    bool ledBlinkActive = false;
    bool ledBase = false;
    uint8_t ledBrightness = 100;
    const DeviceSettings* settings = nullptr;
    OperatingState operatingState = OperatingState::CONNECTING;
    bool startupWindow = false;
    uint32_t startupLedStarted = 0;
    bool startupLedStartedFlag = false;
    bool previewActive = false;
    bool previewNeedsSilence = false;
    Preview previewKind = Preview::STARTUP;
    LedMode previewLedMode = LedMode::OFF;
    uint32_t previewStarted = 0;
    uint32_t stateStarted = 0;
    OperatingState lastOperatingState = OperatingState::CONNECTING;
};
#endif
