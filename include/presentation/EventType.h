#ifndef EVENT_TYPE_H
    #define EVENT_TYPE_H

    enum class EventType {
        //WiFi events
        WIFI_START_CONNECT,
        WIFI_TRY_CONNECT,
        WIFI_CONNECTED,
        WIFI_RECONNECT,

        //telegram bot events
        MESSAGE_SEND
    };

#endif
