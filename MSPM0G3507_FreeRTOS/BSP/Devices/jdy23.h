/**
 * @file    jdy23.h
 * @brief   Transport-independent JDY-23 BLE serial/AT command driver.
 */
#ifndef JDY23_H
#define JDY23_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    JDY23_OK = 0,
    JDY23_ERR_INVALID_PARAM = -1,
    JDY23_ERR_NOT_INIT = -2,
    JDY23_ERR_IO = -3,
    JDY23_ERR_TIMEOUT = -4,
    JDY23_ERR_RESPONSE_TOO_LONG = -5,
    JDY23_ERR_UNEXPECTED_RESPONSE = -6,
} jdy23_status_t;

typedef enum {
    JDY23_LINE_END_NONE = 0,
    JDY23_LINE_END_CRLF,
} jdy23_line_end_t;

typedef enum {
    JDY23_COMMAND_VER = 0,
    JDY23_COMMAND_RST,
    JDY23_COMMAND_DISC,
    JDY23_COMMAND_STAT,
    JDY23_COMMAND_MAC,
    JDY23_COMMAND_BAUD,
    JDY23_COMMAND_SLEEP,
    JDY23_COMMAND_NAME,
    JDY23_COMMAND_STARTEN,
    JDY23_COMMAND_ADVIN,
    JDY23_COMMAND_HOSTEN,
    JDY23_COMMAND_IBUUID,
    JDY23_COMMAND_MAJOR,
    JDY23_COMMAND_MINOR,
    JDY23_COMMAND_COUNT,
} jdy23_command_t;

typedef enum {
    JDY23_COMMAND_KIND_QUERY = 0,
    JDY23_COMMAND_KIND_ACTION,
} jdy23_command_kind_t;

typedef struct {
    const char *name;
    const char *at_command;
    const char *response_prefix;
    jdy23_command_kind_t kind;
    bool response_prefix_verified;
} jdy23_command_info_t;

typedef struct {
    void *context;
    bool (*write)(void *context, const uint8_t *data, uint16_t len);
    bool (*read_byte)(void *context, uint8_t *data);
    void (*flush_rx)(void *context);
    uint32_t (*time_ms)(void *context);
    void (*delay_ms)(void *context, uint32_t delay_ms);
} jdy23_transport_t;

typedef struct {
    jdy23_transport_t transport;
    bool initialized;
    bool detected;
} jdy23_t;


const jdy23_command_info_t *jdy23_get_command_info(jdy23_command_t command);
bool jdy23_find_command(const char *name, jdy23_command_t *command);
jdy23_status_t jdy23_execute_command(jdy23_t *dev, jdy23_command_t command,
                                     char *response, uint16_t response_size,
                                     uint32_t timeout_ms);
jdy23_status_t jdy23_extract_response_value(jdy23_command_t command,
                                             const char *response,
                                             char *value,
                                             uint16_t value_size,
                                             bool *prefix_matched);

jdy23_status_t jdy23_init(jdy23_t *dev, const jdy23_transport_t *transport);
jdy23_status_t jdy23_probe(jdy23_t *dev, char *response,
                           uint16_t response_size, uint32_t timeout_ms);
jdy23_status_t jdy23_send_at(jdy23_t *dev, const char *command,
                             jdy23_line_end_t line_end, char *response,
                             uint16_t response_size, uint32_t timeout_ms);
jdy23_status_t jdy23_send(jdy23_t *dev, const uint8_t *data, uint16_t len);
jdy23_status_t jdy23_receive(jdy23_t *dev, uint8_t *data, uint16_t capacity,
                             uint16_t *received);
bool jdy23_is_detected(const jdy23_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* JDY23_H */
