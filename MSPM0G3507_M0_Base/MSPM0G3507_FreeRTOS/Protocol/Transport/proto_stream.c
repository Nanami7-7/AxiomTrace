/**
 * @file    proto_stream.c
 * @brief   COBS 字节流分帧器实现。
 */
#include "proto_stream.h"
#include "proto_cobs.h"
#include <string.h>

void proto_stream_init(proto_stream_t *stream)
{
    if (stream == NULL) {
        return;
    }
    memset(stream, 0, sizeof(*stream));
}

proto_stream_result_t proto_stream_feed(proto_stream_t *stream,
                                         uint8_t byte,
                                         uint8_t *decoded,
                                         size_t decoded_cap,
                                         size_t *decoded_len)
{
    if (stream == NULL) {
        return PROTO_STREAM_NONE;
    }

    if (byte != PROTO_DELIMITER) {
        if (stream->discarding) {
            return PROTO_STREAM_NONE;
        }
        if (stream->wire_len >= sizeof(stream->wire)) {
            stream->discarding = true;
            stream->overflow_count++;
            return PROTO_STREAM_OVERFLOW;
        }
        stream->wire[stream->wire_len++] = byte;
        return PROTO_STREAM_NONE;
    }

    /* 分隔符同时完成当前帧收尾和异常帧重新同步。 */
    if (stream->discarding) {
        stream->wire_len = 0u;
        stream->discarding = false;
        return PROTO_STREAM_OVERFLOW;
    }

    if (stream->wire_len == 0u) {
        stream->empty_count++;
        return PROTO_STREAM_EMPTY;
    }

    if (decoded == NULL || decoded_len == NULL ||
        !proto_cobs_decode(stream->wire, stream->wire_len,
                           decoded, decoded_cap, decoded_len)) {
        stream->wire_len = 0u;
        stream->cobs_error_count++;
        return PROTO_STREAM_COBS_ERROR;
    }

    stream->wire_len = 0u;
    return PROTO_STREAM_FRAME;
}
