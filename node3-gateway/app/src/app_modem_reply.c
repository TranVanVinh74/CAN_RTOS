/**
 * @file app_modem_reply.c
 * @brief Classification of modem reset, call-end and error responses.
 */

#include "app_modem_reply.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static const char *const reset_markers[] = {"+CPIN: READY", "*ATREADY", "\nRDY\r"};
static const char *const call_end_markers[] = {"NO CARRIER", "BUSY", "NO ANSWER", "NO DIALTONE",
                                               "VOICE CALL: END"};

/**
 * @brief Search response text for any marker in a supplied list.
 * @param[in] reply Null-terminated modem response text.
 * @param[in] markers Input array of null-terminated marker strings.
 * @param[in] count Number of marker strings in the array.
 * @return true if any marker occurs in the response.
 */
static bool contains_any(const char *reply, const char *const *markers, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strstr(reply, markers[i]) != NULL) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Classify modem text, prioritizing reset indications, call termination and then errors.
 * @param[in] reply Null-terminated modem response text, or NULL.
 * @return Detected ModemReply_t category, or MODEM_REPLY_NONE for no recognized marker or NULL input.
 */
ModemReply_t app_modem_classify_reply(const char *reply) {
    if (reply == NULL) {
        return MODEM_REPLY_NONE;
    }
    if (contains_any(reply, reset_markers, sizeof(reset_markers) / sizeof(reset_markers[0]))) {
        return MODEM_REPLY_MODEM_RESET;
    }
    if (contains_any(reply, call_end_markers,
                     sizeof(call_end_markers) / sizeof(call_end_markers[0]))) {
        return MODEM_REPLY_CALL_ENDED;
    }
    if (strstr(reply, "ERROR") != NULL) {
        return MODEM_REPLY_ERROR;
    }
    return MODEM_REPLY_NONE;
}
