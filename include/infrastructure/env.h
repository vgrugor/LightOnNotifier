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

    extern const int BOARD_LED_PIN;
    extern const int EXTERNAL_LED_PIN;
    extern const int BUZZER_PIN;

    extern const char* BOT_TOKEN;
    extern const char* CHAT_IDS[];
    extern const size_t CHAT_IDS_COUNT;

    extern const char* LIGHT_ON_MESSAGE;

#endif // ENV_H
