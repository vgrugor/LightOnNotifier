#include "infrastructure/settings/SettingsStore.h"

#include <stddef.h>
#include <string.h>

#include <memory>
#include <new>

#include <EEPROM.h>
#include <LittleFS.h>

namespace {
constexpr uint32_t MAGIC = 0x4c4f4e31;
constexpr uint32_t PROVISIONED_MARKER = 0x4c4f4e50;
constexpr uint32_t LEGACY_SCHEMA = 1;
constexpr size_t LEGACY_SETTINGS_SIZE = 1132;
const char* const SLOT_PATHS[2] = {"/settings-a.bin", "/settings-b.bin"};
constexpr char TEMP_PATH[] = "/settings.tmp";

static_assert(offsetof(DeviceSettings, startupSoundSeconds) == LEGACY_SETTINGS_SIZE,
              "The version-one settings layout changed");

struct Record {
    uint32_t magic;
    uint32_t length;
    uint32_t checksum;
    DeviceSettings data;
};

uint32_t checksum(const uint8_t* bytes, size_t length) {
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < length; ++i) {
        hash = (hash ^ bytes[i]) * 16777619UL;
    }
    return hash;
}

uint32_t checksum(const DeviceSettings& data) {
    return checksum(reinterpret_cast<const uint8_t*>(&data), sizeof(data));
}

bool readSlot(int slot, Record& record) {
    File file = LittleFS.open(SLOT_PATHS[slot], "r");
    record = {};
    constexpr size_t HEADER_SIZE = offsetof(Record, data);
    if (!file || file.size() < HEADER_SIZE ||
        file.read(reinterpret_cast<uint8_t*>(&record), HEADER_SIZE) != HEADER_SIZE ||
        record.magic != MAGIC ||
        (record.length != sizeof(DeviceSettings) && record.length != LEGACY_SETTINGS_SIZE) ||
        file.size() != HEADER_SIZE + record.length ||
        static_cast<size_t>(file.read(reinterpret_cast<uint8_t*>(&record.data), record.length)) !=
            record.length ||
        record.checksum !=
            checksum(reinterpret_cast<const uint8_t*>(&record.data), record.length)) {
        return false;
    }
    if (record.length == LEGACY_SETTINGS_SIZE && record.data.schema == LEGACY_SCHEMA) {
        record.data.schema = DeviceSettings::SCHEMA;
        record.data.startupSoundSeconds = 10;
    } else if (record.length != sizeof(DeviceSettings) ||
               record.data.schema != DeviceSettings::SCHEMA) {
        return false;
    }
    if (!validSignalSettings(record.data) ||
        (record.data.configured &&
         (!validNetworkSettings(record.data) || !validTelegramSettings(record.data)))) {
        return false;
    }
    return true;
}

bool futureSchema(int slot) {
    File file = LittleFS.open(SLOT_PATHS[slot], "r");
    uint32_t header[4] = {};
    return file && file.size() >= sizeof(header) &&
           file.read(reinterpret_cast<uint8_t*>(header), sizeof(header)) == sizeof(header) &&
           header[0] == MAGIC && header[3] > DeviceSettings::SCHEMA;
}
} // namespace

bool SettingsStore::begin() {
    EEPROM.begin(8);
    if (EEPROM.getConstDataPtr() == nullptr) {
        return false;
    }
    uint32_t marker = 0;
    uint32_t inverse = 0;
    EEPROM.get(0, marker);
    EEPROM.get(4, inverse);
    provisionedMarker = marker == PROVISIONED_MARKER && inverse == ~PROVISIONED_MARKER;
    mounted = LittleFS.begin();
    return mounted;
}

bool SettingsStore::load(DeviceSettings& settings) {
    if (!mounted) {
        return false;
    }
    unsupportedSchema = futureSchema(0) || futureSchema(1);
    if (unsupportedSchema) {
        return false;
    }
    Record record = {};
    const bool hasFirst = readSlot(0, record);
    if (hasFirst) {
        settings = record.data;
        activeSlot = 0;
    }
    const bool hasSecond = readSlot(1, record);
    if (hasSecond && (!hasFirst || record.data.revision > settings.revision)) {
        settings = record.data;
        activeSlot = 1;
    }
    if (!hasFirst && !hasSecond) {
        return false;
    }
    return true;
}

bool SettingsStore::hasRecords() const {
    return mounted && (LittleFS.exists(SLOT_PATHS[0]) || LittleFS.exists(SLOT_PATHS[1]));
}

bool SettingsStore::save(const DeviceSettings& settings) {
    if (!mounted || unsupportedSchema || settings.schema != DeviceSettings::SCHEMA ||
        !validSignalSettings(settings) ||
        (settings.configured &&
         (!validNetworkSettings(settings) || !validTelegramSettings(settings)))) {
        return false;
    }
    if (!provisionedMarker) {
        EEPROM.put(0, PROVISIONED_MARKER);
        EEPROM.put(4, ~PROVISIONED_MARKER);
        if (!EEPROM.commit()) {
            return false;
        }
        provisionedMarker = true;
    }
    const int target = activeSlot == 0 ? 1 : 0;
    std::unique_ptr<Record> record(new (std::nothrow) Record{});
    if (!record) {
        return false;
    }
    record->magic = MAGIC;
    record->length = sizeof(DeviceSettings);
    record->data = settings;
    record->checksum = checksum(record->data);
    File file = LittleFS.open(TEMP_PATH, "w");
    if (!file) {
        return false;
    }
    const bool written = file.write(reinterpret_cast<const uint8_t*>(record.get()),
                                    sizeof(Record)) == sizeof(Record);
    file.flush();
    file.close();
    if (!written) {
        LittleFS.remove(TEMP_PATH);
        return false;
    }
    File verify = LittleFS.open(TEMP_PATH, "r");
    if (!verify || verify.size() != sizeof(Record) ||
        verify.read(reinterpret_cast<uint8_t*>(record.get()), sizeof(Record)) != sizeof(Record) ||
        record->magic != MAGIC || record->length != sizeof(DeviceSettings) ||
        record->checksum != checksum(record->data)) {
        LittleFS.remove(TEMP_PATH);
        return false;
    }
    verify.close();
    // Preserve the active slot until the replacement has been fully written and renamed.
    LittleFS.remove(SLOT_PATHS[target]);
    if (!LittleFS.rename(TEMP_PATH, SLOT_PATHS[target])) {
        return false;
    }
    activeSlot = target;
    return true;
}

bool SettingsStore::reset(const DeviceSettings& settings) {
    if (!mounted) {
        return false;
    }
    LittleFS.remove(SLOT_PATHS[0]);
    LittleFS.remove(SLOT_PATHS[1]);
    activeSlot = -1;
    unsupportedSchema = false;
    return save(settings);
}
