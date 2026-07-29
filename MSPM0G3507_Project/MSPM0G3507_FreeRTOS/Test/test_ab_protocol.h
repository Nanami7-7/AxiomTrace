/**
 * @file    test_ab_protocol.h
 * @brief   AB 板通信最小测试接口。
 *
 * 测试模块只负责把 Board A 收到的协议帧打印到 UART0，
 * 不修改协议状态机，也不参与电机控制。
 */
#ifndef TEST_AB_PROTOCOL_H
#define TEST_AB_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 打印一帧已经 COBS 解码的协议数据。
 * @param decoded     COBS 解码后的逻辑帧。
 * @param decoded_len 逻辑帧长度。
 * @note  函数在任务上下文调用，不要在中断中调用。
 */
void test_ab_protocol_print_decoded(const uint8_t *decoded, size_t decoded_len);

#ifdef __cplusplus
}
#endif

#endif /* TEST_AB_PROTOCOL_H */