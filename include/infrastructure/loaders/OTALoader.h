#ifndef OTA_LOADER_H
    #define OTA_LOADER_H

    #include <ArduinoOTA.h>
    #include <ESP8266WiFi.h>

    class OTALoader {
        public:
            OTALoader(const char* hostname, const char* password = nullptr);
            void begin();
            void handle();
            void setPassword(const char* password);

        private:
            const char* hostname;
            const char* password;
    };

#endif
