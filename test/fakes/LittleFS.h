#ifndef LIGHTONNOTIFIER_TEST_FAKES_LITTLEFS_H
#define LIGHTONNOTIFIER_TEST_FAKES_LITTLEFS_H

#include <stddef.h>
#include <stdint.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

class File {
public:
    File() = default;
    File(std::vector<uint8_t>* data, bool writable, bool* failWrite)
        : data(data), writable(writable), failWrite(failWrite) {}
    explicit operator bool() const {
        return data != nullptr;
    }
    size_t size() const {
        return data == nullptr ? 0 : data->size();
    }
    size_t read(uint8_t* output, size_t length) {
        if (data == nullptr || cursor + length > data->size()) {
            return 0;
        }
        memcpy(output, data->data() + cursor, length);
        cursor += length;
        return length;
    }
    size_t write(const uint8_t* input, size_t length) {
        if (data == nullptr || !writable || (failWrite != nullptr && *failWrite)) {
            return 0;
        }
        data->insert(data->end(), input, input + length);
        return length;
    }
    void flush() {}
    void close() {}

private:
    std::vector<uint8_t>* data = nullptr;
    bool writable = false;
    bool* failWrite = nullptr;
    size_t cursor = 0;
};

class FakeLittleFS {
public:
    std::map<std::string, std::vector<uint8_t>> files;
    bool failWrite = false;
    bool failRename = false;
    bool begin() {
        return true;
    }
    File open(const char* path, const char* mode) {
        if (mode[0] == 'w') {
            auto& data = files[path];
            data.clear();
            return File(&data, true, &failWrite);
        }
        const auto found = files.find(path);
        return found == files.end() ? File() : File(&found->second, false, nullptr);
    }
    bool exists(const char* path) const {
        return files.count(path) != 0;
    }
    bool remove(const char* path) {
        return files.erase(path) != 0;
    }
    bool rename(const char* from, const char* to) {
        if (failRename || !exists(from)) {
            return false;
        }
        files[to] = files[from];
        files.erase(from);
        return true;
    }
};

inline FakeLittleFS LittleFS;

#endif
