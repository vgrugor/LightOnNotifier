#ifndef TELEGRAM_NOTIFIER_H
    #define TELEGRAM_NOTIFIER_H

    #include <Arduino.h>
    #include <WiFiClientSecure.h>
    #include <UniversalTelegramBot.h>

    class TelegramNotifier {
    private:
        String botToken;
        String chatId;

        WiFiClientSecure* client;
        UniversalTelegramBot* bot;

    public:
        TelegramNotifier(const String& botToken, const String& chatId);
        bool sendMessage(const String& text);
    };

#endif
