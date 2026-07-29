/**
 * @file    app_encoder_telemetry.h
 * @brief   编码器累计计数 UART0 DMA 遥测接口。
 * @details 位置环由按键启动后，本模块按固定周期发送 A/M1、B/M2、C/M3、D/M4
 *          四路编码器累计计数。模块只读取 BSP 的累计计数，不参与测速、位置环或
 *          编码器中断处理；收到 stop 命令后由上层调用停止接口。
 */
#ifndef APP_ENCODER_TELEMETRY_H
#define APP_ENCODER_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 启动四路编码器累计计数遥测。
 * @note  启动时不清零编码器计数；输出从当前累计值开始。
 */
void app_encoder_telemetry_start(void);

/**
 * @brief 停止四路编码器累计计数遥测。
 * @note  不会清零编码器计数，也不会停止电机。
 */
void app_encoder_telemetry_stop(void);

/**
 * @brief 查询遥测是否处于持续发送状态。
 * @return true 已启动；false 已停止。
 */
bool app_encoder_telemetry_is_active(void);

/**
 * @brief 在控制任务上下文中推进非阻塞 DMA 发送。
 * @param now_ms 单调递增的毫秒时间戳。
 * @note  不修改测速逻辑；DMA 忙时保留本次发送机会，下个控制周期重试。
 */
void app_encoder_telemetry_process(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_ENCODER_TELEMETRY_H */