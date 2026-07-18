/**
 * @file    axiom_port.h
 * @brief   AxiomTrace 端口层接口
 * @note    从 AxiomTrace baremetal/port/axiom_port.h 提取
 */
#ifndef AXIOM_PORT_H
#define AXIOM_PORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  字符串输出(日志用)
 * @note   平台移植时实现此函数, 映射到 UART/RTT/stdout
 * @param  str 以 '\0' 结尾的字符串
 */
void axiom_port_string_out(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_PORT_H */