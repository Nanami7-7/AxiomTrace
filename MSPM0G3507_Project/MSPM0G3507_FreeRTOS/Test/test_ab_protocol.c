/**
 * @file    test_ab_protocol.c
 * @brief   AB 板通信最小测试实现。
 */
#include "test_ab_protocol.h"
#include "proto_frame.h"
#include "proto_types.h"
#include <stdio.h>

#ifndef AB_PROTOCOL_TEST_ENABLE
#define AB_PROTOCOL_TEST_ENABLE (1U)
#endif

#define TEST_AB_PAYLOAD_PRINT_MAX (16U)

static void test_ab_print_payload(const uint8_t *payload, uint16_t payload_len)
{
    uint16_t i;
    uint16_t print_len = payload_len;

    if (print_len > TEST_AB_PAYLOAD_PRINT_MAX) {
        print_len = TEST_AB_PAYLOAD_PRINT_MAX;
    }

    if (print_len == 0U) {
        return;
    }

    printf(" payload=");
    for (i = 0U; i < print_len; i++) {
        printf("%02X", payload[i]);
        if ((i + 1U) < print_len) {
            printf(" ");
        }
    }
    if (payload_len > print_len) {
        printf(" ...");
    }
}

void test_ab_protocol_print_decoded(const uint8_t *decoded, size_t decoded_len)
{
#if (AB_PROTOCOL_TEST_ENABLE != 0U)
    proto_frame_view_t frame;
    bool valid;

    if (decoded == NULL) {
        printf("[AB TEST] 空指针\r\n");
        return;
    }

    /* Board A 接受 Board B 转发帧，也允许直接发送 Host 测试帧。 */
    valid = proto_frame_validate(decoded, decoded_len,
                                 PROTO_ADDR_GATEWAY,
                                 PROTO_ADDR_CONTROLLER,
                                 &frame);
    if (!valid) {
        valid = proto_frame_validate(decoded, decoded_len,
                                     PROTO_ADDR_HOST,
                                     PROTO_ADDR_CONTROLLER,
                                     &frame);
    }

    if (!valid) {
        printf("[AB TEST] 无效帧 decoded_len=%u\r\n",
               (unsigned)decoded_len);
        return;
    }

    printf("[AB TEST] src=0x%02X dst=0x%02X class=0x%02X opcode=0x%02X "
           "seq=%u len=%u",
           frame.src,
           frame.dst,
           frame.msg_class,
           frame.opcode,
           (unsigned)frame.seq,
           (unsigned)frame.payload_len);
    test_ab_print_payload(frame.payload, frame.payload_len);
    printf("\r\n");
#else
    (void)decoded;
    (void)decoded_len;
#endif
}