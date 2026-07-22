/**
 * @file    jdy23.c
 * @brief   JDY-23 AT command and transparent-data implementation.
 */
#include "jdy23.h"
#include <stddef.h>
#include <string.h>

#define JDY23_RESPONSE_QUIET_MS (30U)

static const jdy23_command_info_t s_command_table[JDY23_COMMAND_COUNT] = {
    /* name, AT text, expected response prefix, kind, prefix verified */
    {"VER",     "AT+VER",     "+VER:",     JDY23_COMMAND_KIND_QUERY,  true},
    {"RST",     "AT+RST",     NULL,         JDY23_COMMAND_KIND_ACTION, false},
    {"DISC",    "AT+DISC",    NULL,         JDY23_COMMAND_KIND_ACTION, false},
    {"STAT",    "AT+STAT",    "+STAT:",    JDY23_COMMAND_KIND_QUERY,  true},
    {"MAC",     "AT+MAC",     "+MAC:",     JDY23_COMMAND_KIND_QUERY,  true},
    {"BAUD",    "AT+BAUD",    "+BAUD:",    JDY23_COMMAND_KIND_QUERY,  true},
    {"SLEEP",   "AT+SLEEP",   NULL,         JDY23_COMMAND_KIND_ACTION, false},
    {"NAME",    "AT+NAME",    "+NAME:",    JDY23_COMMAND_KIND_QUERY,  true},
    {"STARTEN", "AT+STARTEN", "+STARTEN:", JDY23_COMMAND_KIND_QUERY,  true},
    {"ADVIN",   "AT+ADVIN",   "+ADVIN:",   JDY23_COMMAND_KIND_QUERY,  true},
    {"HOSTEN",  "AT+HOSTEN",  "+HOSTEN:",  JDY23_COMMAND_KIND_QUERY,  true},
    {"IBUUID",  "AT+IBUUID",  "+IBUUID:",  JDY23_COMMAND_KIND_QUERY,  true},
    {"MAJOR",   "AT+MAJOR",   "+IBMAJOR:", JDY23_COMMAND_KIND_QUERY,  true},
    {"MINOR",   "AT+MINOR",   "+IBMINOR:", JDY23_COMMAND_KIND_QUERY,  true},
};

static char ascii_upper(char value)
{
    return ((value >= 'a') && (value <= 'z')) ?
           (char)(value - ('a' - 'A')) : value;
}

static bool ascii_equal_ignore_case(const char *lhs, const char *rhs)
{
    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }

    while ((*lhs != '\0') && (*rhs != '\0')) {
        if (ascii_upper(*lhs) != ascii_upper(*rhs)) {
            return false;
        }
        lhs++;
        rhs++;
    }
    return ((*lhs == '\0') && (*rhs == '\0'));
}

const jdy23_command_info_t *jdy23_get_command_info(jdy23_command_t command)
{
    return ((uint32_t)command < (uint32_t)JDY23_COMMAND_COUNT) ?
           &s_command_table[command] : NULL;
}

bool jdy23_find_command(const char *name, jdy23_command_t *command)
{
    uint32_t i;

    if ((name == NULL) || (command == NULL)) {
        return false;
    }

    for (i = 0U; i < (uint32_t)JDY23_COMMAND_COUNT; i++) {
        if (ascii_equal_ignore_case(name, s_command_table[i].name) ||
            ascii_equal_ignore_case(name, s_command_table[i].at_command)) {
            *command = (jdy23_command_t)i;
            return true;
        }
    }
    return false;
}

jdy23_status_t jdy23_execute_command(jdy23_t *dev, jdy23_command_t command,
                                     char *response, uint16_t response_size,
                                     uint32_t timeout_ms)
{
    const jdy23_command_info_t *info = jdy23_get_command_info(command);

    if (info == NULL) {
        return JDY23_ERR_INVALID_PARAM;
    }
    return jdy23_send_at(dev, info->at_command, JDY23_LINE_END_CRLF,
                         response, response_size, timeout_ms);
}

jdy23_status_t jdy23_extract_response_value(jdy23_command_t command,
                                             const char *response,
                                             char *value,
                                             uint16_t value_size,
                                             bool *prefix_matched)
{
    const jdy23_command_info_t *info = jdy23_get_command_info(command);
    const char *start;
    const char *end;
    uint16_t length;
    bool matched = false;

    if ((info == NULL) || (response == NULL) || (value == NULL) ||
        (value_size < 2U)) {
        return JDY23_ERR_INVALID_PARAM;
    }

    start = response;
    if (info->response_prefix != NULL) {
        const char *prefixed = strstr(response, info->response_prefix);
        if (prefixed != NULL) {
            start = prefixed + strlen(info->response_prefix);
            matched = true;
        }
    }

    while ((*start == ' ') || (*start == '\r') || (*start == '\n') ||
           (*start == '\t')) {
        start++;
    }
    end = start;
    while ((*end != '\0') && (*end != '\r') && (*end != '\n')) {
        end++;
    }
    while ((end > start) &&
           ((end[-1] == ' ') || (end[-1] == '\t'))) {
        end--;
    }

    length = (uint16_t)(end - start);
    if (length == 0U) {
        value[0] = '\0';
        if (prefix_matched != NULL) {
            *prefix_matched = matched;
        }
        return JDY23_ERR_UNEXPECTED_RESPONSE;
    }
    if (length >= value_size) {
        value[0] = '\0';
        return JDY23_ERR_RESPONSE_TOO_LONG;
    }

    (void)memcpy(value, start, length);
    value[length] = '\0';
    if (prefix_matched != NULL) {
        *prefix_matched = matched;
    }
    return JDY23_OK;
}

static bool transport_valid(const jdy23_transport_t *transport)
{
    return (transport != NULL) &&
           (transport->write != NULL) &&
           (transport->read_byte != NULL) &&
           (transport->flush_rx != NULL) &&
           (transport->time_ms != NULL) &&
           (transport->delay_ms != NULL);
}

static bool timeout_elapsed(uint32_t start, uint32_t now, uint32_t timeout_ms)
{
    return ((uint32_t)(now - start) >= timeout_ms);
}

jdy23_status_t jdy23_init(jdy23_t *dev, const jdy23_transport_t *transport)
{
    if ((dev == NULL) || !transport_valid(transport)) {
        return JDY23_ERR_INVALID_PARAM;
    }

    dev->transport = *transport;
    dev->initialized = true;
    dev->detected = false;
    return JDY23_OK;
}

jdy23_status_t jdy23_send_at(jdy23_t *dev, const char *command,
                             jdy23_line_end_t line_end, char *response,
                             uint16_t response_size, uint32_t timeout_ms)
{
    uint16_t response_len = 0U;
    uint32_t start;
    uint32_t last_rx;
    bool received_any = false;
    const uint8_t crlf[2] = {'\r', '\n'};

    if ((dev == NULL) || (command == NULL) || (response == NULL) ||
        (response_size < 2U) || (timeout_ms == 0U)) {
        return JDY23_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    if ((line_end != JDY23_LINE_END_NONE) &&
        (line_end != JDY23_LINE_END_CRLF)) {
        return JDY23_ERR_INVALID_PARAM;
    }

    response[0] = '\0';
    dev->transport.flush_rx(dev->transport.context);

    if (!dev->transport.write(dev->transport.context,
                              (const uint8_t *)command,
                              (uint16_t)strlen(command))) {
        return JDY23_ERR_IO;
    }
    if ((line_end == JDY23_LINE_END_CRLF) &&
        !dev->transport.write(dev->transport.context, crlf, sizeof(crlf))) {
        return JDY23_ERR_IO;
    }

    start = dev->transport.time_ms(dev->transport.context);
    last_rx = start;

    for (;;) {
        uint8_t byte;
        uint32_t now;

        while (dev->transport.read_byte(dev->transport.context, &byte)) {
            received_any = true;
            last_rx = dev->transport.time_ms(dev->transport.context);
            if (response_len >= (uint16_t)(response_size - 1U)) {
                response[response_len] = '\0';
                return JDY23_ERR_RESPONSE_TOO_LONG;
            }
            response[response_len++] = (char)byte;
            response[response_len] = '\0';
        }

        now = dev->transport.time_ms(dev->transport.context);
        if (received_any && timeout_elapsed(last_rx, now,
                                            JDY23_RESPONSE_QUIET_MS)) {
            return JDY23_OK;
        }
        if (timeout_elapsed(start, now, timeout_ms)) {
            return received_any ? JDY23_OK : JDY23_ERR_TIMEOUT;
        }
        dev->transport.delay_ms(dev->transport.context, 1U);
    }
}

static bool response_contains(const char *response, const char *token)
{
    return (response != NULL) && (token != NULL) &&
           (strstr(response, token) != NULL);
}

static bool version_response_valid(const char *response)
{
    return response_contains(response, "+VER:") ||
           response_contains(response, "JDY-23") ||
           response_contains(response, "JDY23");
}

static bool basic_at_response_valid(const char *response)
{
    return response_contains(response, "OK") ||
           response_contains(response, "JDY-23") ||
           response_contains(response, "JDY23");
}

jdy23_status_t jdy23_probe(jdy23_t *dev, char *response,
                           uint16_t response_size, uint32_t timeout_ms)
{
    jdy23_status_t status;

    if (dev == NULL) {
        return JDY23_ERR_INVALID_PARAM;
    }

    dev->detected = false;

    /* Current JDY-23 firmware documents CRLF-terminated AT commands. */
    status = jdy23_send_at(dev, "AT+VER", JDY23_LINE_END_CRLF,
                           response, response_size, timeout_ms);
    if ((status == JDY23_OK) && version_response_valid(response)) {
        dev->detected = true;
        return JDY23_OK;
    }

    /* Compatibility fallback for variants accepting the basic AT probe. */
    status = jdy23_send_at(dev, "AT", JDY23_LINE_END_CRLF,
                           response, response_size, timeout_ms);
    if ((status == JDY23_OK) && basic_at_response_valid(response)) {
        dev->detected = true;
        return JDY23_OK;
    }

    return (status == JDY23_OK) ? JDY23_ERR_UNEXPECTED_RESPONSE : status;
}

jdy23_status_t jdy23_send(jdy23_t *dev, const uint8_t *data, uint16_t len)
{
    if ((dev == NULL) || ((data == NULL) && (len != 0U))) {
        return JDY23_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    if (len == 0U) {
        return JDY23_OK;
    }

    return dev->transport.write(dev->transport.context, data, len) ?
           JDY23_OK : JDY23_ERR_IO;
}

jdy23_status_t jdy23_receive(jdy23_t *dev, uint8_t *data, uint16_t capacity,
                             uint16_t *received)
{
    uint16_t count = 0U;

    if ((dev == NULL) || (data == NULL) || (received == NULL) ||
        (capacity == 0U)) {
        return JDY23_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return JDY23_ERR_NOT_INIT;
    }

    while ((count < capacity) &&
           dev->transport.read_byte(dev->transport.context, &data[count])) {
        count++;
    }
    *received = count;
    return JDY23_OK;
}

bool jdy23_is_detected(const jdy23_t *dev)
{
    return (dev != NULL) && dev->initialized && dev->detected;
}
