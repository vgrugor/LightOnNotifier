#ifndef TELEGRAM_NOTIFIER_H
#define TELEGRAM_NOTIFIER_H

    #include <Arduino.h>
    #include <WiFiClientSecure.h>
    #include <UniversalTelegramBot.h>
    #include "presentation/EventNotifier.h"

    class TelegramNotifier {
        private:
            String botToken;
            const char** chatIds;
            size_t chatIdsCount;

            WiFiClientSecure client;
            X509List* cert = nullptr;
            UniversalTelegramBot* bot = nullptr;

            bool timeSynced = false;

        public:
            TelegramNotifier(const String& botToken, const char* chatIds[], size_t count);
            void init();
            bool sendMessage(const String& text);
    };

#endif
