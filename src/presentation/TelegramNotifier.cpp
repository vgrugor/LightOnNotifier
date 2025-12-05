#include "presentation/TelegramNotifier.h"

TelegramNotifier::TelegramNotifier(
    const String& botToken,
    const char* chatIds[],
    size_t count
) : botToken(botToken), chatIds(chatIds), chatIdsCount(count)
{
}

void TelegramNotifier::init() {
    this->cert = new X509List(TELEGRAM_CERTIFICATE_ROOT);
    this->client.setTrustAnchors(cert);
    this->bot = new UniversalTelegramBot(this->botToken, this->client);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    int tries = 0;
    while (tries < 100) {
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
    if (!timeSynced) {
        EventNotifier::getInstance().notifyObservers(EventType::MESSAGE_SEND_SKIP, "Time is not synced");
        return false;
    }

    if (!bot) {
        EventNotifier::getInstance().notifyObservers(EventType::MESSAGE_SEND_SKIP, "Bot is not initialized");
        return false;
    }

    bool allSent = true;

    for (size_t i = 0; i < chatIdsCount; i++) {
        bool result = bot->sendMessage(chatIds[i], text);

        if (result) {
            EventNotifier::getInstance().notifyObservers(EventType::MESSAGE_SEND);
        } else {
            allSent = false;
            EventNotifier::getInstance().notifyObservers(
                EventType::MESSAGE_SEND_SKIP,
                String("Failed to send to chat_id: ") + chatIds[i]
            );
        }

        delay(200);
        yield();
    }

    return allSent;
}
