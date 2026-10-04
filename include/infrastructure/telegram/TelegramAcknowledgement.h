#ifndef LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_TELEGRAMACKNOWLEDGEMENT_H
#define LIGHTONNOTIFIER_INFRASTRUCTURE_TELEGRAM_TELEGRAMACKNOWLEDGEMENT_H

#include "infrastructure/telegram/HttpResponse.h"

bool isAcknowledged(const HttpResponse& response);
#endif
