#include "presentation/TelegramNotifier.h"

TelegramNotifier::TelegramNotifier(
    const String& botToken,
    const String& chatId
) : botToken(botToken),
    chatId(chatId)
{
    client = new WiFiClientSecure;
    client->setTrustAnchors(new X509List(TELEGRAM_CERTIFICATE_ROOT));

    bot = new UniversalTelegramBot(botToken, *client);

    configTime(0, 0, "pool.ntp.org");
}

bool TelegramNotifier::sendMessage(const String& text) {
    return bot->sendMessage(chatId, text);
}
