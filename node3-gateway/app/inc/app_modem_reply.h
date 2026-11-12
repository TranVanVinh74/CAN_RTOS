#ifndef APP_MODEM_REPLY_H
#define APP_MODEM_REPLY_H

typedef enum {
    MODEM_REPLY_NONE = 0,
    MODEM_REPLY_CALL_ENDED = 1,
    MODEM_REPLY_ERROR = 2,
    MODEM_REPLY_MODEM_RESET = 3
} ModemReply_t;

ModemReply_t app_modem_classify_reply(const char *reply);

#endif
