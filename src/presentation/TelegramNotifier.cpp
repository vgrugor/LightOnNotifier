#include "presentation/TelegramNotifier.h"

TelegramNotifier::TelegramNotifier(
    const String& botToken,
    const String& chatId
) : botToken(botToken), chatId(chatId)
{
}

void TelegramNotifier::init() {
    this->cert = new X509List(TELEGRAM_CERTIFICATE_ROOT);
    this->client.setTrustAnchors(cert);
    this->bot = new UniversalTelegramBot(botToken, client);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    int tries = 0;
    while (tries < 50) {
        time_t now = time(nullptr);
        if (now > 100000) {
            this->timeSynced = true;
            break;
        }
        delay(100);
        yield();
        tries++;
    }
}

bool TelegramNotifier::sendMessage(const String& text) {
    if (!this->timeSynced) {
        EventNotifier::getInstance().notifyObservers(EventType::MESSAGE_SEND_SKIP, "Time is not synced");

        return false;
    }

    if (!this->bot) {
        EventNotifier::getInstance().notifyObservers(EventType::MESSAGE_SEND_SKIP, "Bot is not initialized");

        return false;
    }

    bool result = this->bot->sendMessage(chatId, text);

    EventNotifier::getInstance().notifyObservers(EventType::MESSAGE_SEND);

    return result;
}
