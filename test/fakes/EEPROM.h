#ifndef LIGHTONNOTIFIER_TEST_FAKES_EEPROM_H
#define LIGHTONNOTIFIER_TEST_FAKES_EEPROM_H

#include <stddef.h>
#include <stdint.h>

#include <cstring>

class FakeEEPROM {
public:
    uint8_t bytes[8] = {};
    bool failCommit = false;
    void begin(size_t) {}
    const uint8_t* getConstDataPtr() const {
        return bytes;
    }
    template <typename T> T& get(int address, T& output) {
        memcpy(&output, bytes + address, sizeof(T));
        return output;
    }
    template <typename T> const T& put(int address, const T& input) {
        memcpy(bytes + address, &input, sizeof(T));
        return input;
    }
    bool commit() {
        return !failCommit;
    }
};

inline FakeEEPROM EEPROM;

#endif
