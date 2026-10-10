#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_SETTINGS_SETTINGSSTORE_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_SETTINGS_SETTINGSSTORE_H

#include "application/DeviceSettings.h"

class SettingsStore {
public:
    bool begin();
    bool load(DeviceSettings& settings);
    bool hasRecords() const;
    bool wasProvisioned() const {
        return provisionedMarker;
    }
    bool save(const DeviceSettings& settings);
    bool reset(const DeviceSettings& settings);
    bool hasUnsupportedSchema() const {
        return unsupportedSchema;
    }
    bool available() const {
        return mounted;
    }

private:
    bool mounted = false;
    bool unsupportedSchema = false;
    bool provisionedMarker = false;
    int activeSlot = -1;
};
#endif
