#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_LOADERS_OTALOADER_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_LOADERS_OTALOADER_H

#include "application/Ports.h"

class OTALoader : public OtaPort {
public:
    OTALoader(const char* hostname, const char* password = nullptr)
        : hostname(hostname), password(password) {}
    void begin() override;
    void handle() override;

private:
    const char* hostname;
    const char* password;
};
#endif
