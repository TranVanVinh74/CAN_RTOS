/**
 * @file app_modem_reply.h
 * @brief Classification of modem reset, call-end and error responses.
 */

#ifndef APP_MODEM_REPLY_H
#define APP_MODEM_REPLY_H

typedef enum {
    MODEM_REPLY_NONE = 0,
    MODEM_REPLY_CALL_ENDED = 1,
    MODEM_REPLY_ERROR = 2,
    MODEM_REPLY_MODEM_RESET = 3
} ModemReply_t;

/**
 * @brief Classify modem text, prioritizing reset indications, call termination and then errors.
 * @param[in] reply Null-terminated modem response text, or NULL.
 * @return Detected ModemReply_t category, or MODEM_REPLY_NONE for no recognized marker or NULL input.
 */
ModemReply_t app_modem_reply_classify(const char *reply);

#endif
