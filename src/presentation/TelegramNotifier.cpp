#include "presentation/TelegramNotifier.h"
#include <vector>

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

    const uint8_t maxAttempts = 5;
    std::vector<uint8_t> attempts(chatIdsCount, 0);
    std::vector<uint8_t> delivered(chatIdsCount, 0);

    auto hasPending = [&]() {
        for (size_t i = 0; i < chatIdsCount; ++i) {
            if (!delivered[i] && attempts[i] < maxAttempts) {
                return true;
            }
        }
        return false;
    };

    while (hasPending()) {
        for (size_t i = 0; i < chatIdsCount; ++i) {
            if (delivered[i] || attempts[i] >= maxAttempts) {
                continue;
            }

            attempts[i]++;
            bool result = bot->sendMessage(chatIds[i], text);

            if (result) {
                delivered[i] = 1;
                EventNotifier::getInstance().notifyObservers(EventType::MESSAGE_SEND);
            } else {
                EventNotifier::getInstance().notifyObservers(
                    EventType::MESSAGE_SEND_SKIP,
                    String("Attempt ") + attempts[i] + "/" + maxAttempts + " failed for chat_id: " + chatIds[i]
                );
            }

            delay(200);
            yield();
        }
    }

    bool allSent = true;

    for (size_t i = 0; i < chatIdsCount; ++i) {
        if (!delivered[i]) {
            allSent = false;

            if (attempts[i] >= maxAttempts) {
                EventNotifier::getInstance().notifyObservers(
                    EventType::MESSAGE_SEND_SKIP,
                    String("Failed to send after ") + attempts[i] + " attempts for chat_id: " + chatIds[i]
                );
            }
        }
    }

    return allSent;
}
