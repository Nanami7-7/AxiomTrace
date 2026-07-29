/**
 * @file    proto_stream.h
 * @brief   COBS 字节流分帧器。
 *
 * 中断只负责把字节放入 UART 环形缓冲；本模块在协议任务中逐字节调用，
 * 遇到 0x00 时完成一帧 COBS 解码，不执行 CRC 和业务分发。
 */
#ifndef PROTO_STREAM_H
#define PROTO_STREAM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "proto_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROTO_STREAM_NONE = 0,
    PROTO_STREAM_FRAME,
    PROTO_STREAM_EMPTY,
    PROTO_STREAM_COBS_ERROR,
    PROTO_STREAM_OVERFLOW
} proto_stream_result_t;

typedef struct {
    uint8_t  wire[PROTO_MAX_ENCODED];
    size_t   wire_len;
    bool     discarding;
    uint32_t empty_count;
    uint32_t overflow_count;
    uint32_t cobs_error_count;
} proto_stream_t;

void proto_stream_init(proto_stream_t *stream);

proto_stream_result_t proto_stream_feed(proto_stream_t *stream,
                                         uint8_t byte,
                                         uint8_t *decoded,
                                         size_t decoded_cap,
                                         size_t *decoded_len);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_STREAM_H */
