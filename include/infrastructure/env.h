// env.h
#ifndef ENV_H
    #define ENV_H

    #include "Arduino.h"

    extern const char* OTA_HOSTNAME;
    extern const char* OTA_PASSWORD;

    extern const char* WIFI_SSID;
    extern const char* WIFI_PASSWORD;
    extern const char* WIFI_IP;
    extern const char* WIFI_GATEWAY;
    extern const char* WIFI_SUBNET;

    extern const int BOARD_LED;

    extern const char* BOT_TOKEN;
    extern const char* CHAT_ID;

    extern const char* LIGHT_ON_MESSAGE;

#endif // ENV_H
