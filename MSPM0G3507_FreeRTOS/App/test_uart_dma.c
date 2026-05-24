/**
 * @file    test_uart_dma.c
 * @brief   串口DMA功能测试
 * @note    在调度器启动前执行，使用bsp_delay_ms()进行延时
 *          （osal_delay_ms依赖SysTick，调度器启动前不可用）
 */
#include "test_uart_dma.h"
#include "bsp_uart.h"
#include "bsp_timer.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <string.h>

#define TEST_PASS(name)  printf("[PASS] %s\r\n", (name))
#define TEST_FAIL(name)  printf("[FAIL] %s\r\n", (name))
#define TEST_INFO(fmt, ...) printf("  " fmt "\r\n", ##__VA_ARGS__)

/* ======================== 测试用例 ======================== */

/**
 * @brief  测试1: 基本DMA发送
 * @note   发送固定长度数据，验证DMA是否正常启动和完成
 */
static bool test_dma_basic_send(void)
{
    const char *msg = "DMA Basic Send Test\r\n";
    bsp_status_t ret = bsp_uart_send_dma((const uint8_t *)msg,
                                          (uint16_t)strlen(msg));

    if (ret != BSP_OK) {
        TEST_INFO("bsp_uart_send_dma returned %d", ret);
        return false;
    }

    /* 等待DMA完成 */
    uint32_t timeout = 1000;
    while (!bsp_uart_tx_idle() && timeout > 0) {
        bsp_delay_ms(1);
        timeout--;
    }

    if (timeout == 0) {
        TEST_INFO("DMA timeout");
        return false;
    }

    return true;
}

/**
 * @brief  测试2: 连续DMA发送
 * @note   连续发送多帧数据，验证DMA非阻塞发送
 */
static bool test_dma_continuous_send(void)
{
    char buf[64];
    const int frame_count = 10;
    int success_count = 0;

    for (int i = 0; i < frame_count; i++) {
        int len = snprintf(buf, sizeof(buf),
                           "Frame %d/%d\r\n", i + 1, frame_count);

        /* 等待上一次发送完成 */
        uint32_t wait = 1000;
        while (!bsp_uart_tx_idle() && wait > 0) {
            bsp_delay_ms(1);
            wait--;
        }

        bsp_status_t ret = bsp_uart_send_dma((const uint8_t *)buf,
                                              (uint16_t)len);
        if (ret == BSP_OK) {
            success_count++;
        } else {
            TEST_INFO("Frame %d failed: %d", i, ret);
        }
    }

    /* 等待最后一帧发送完成 */
    uint32_t timeout = 2000;
    while (!bsp_uart_tx_idle() && timeout > 0) {
        bsp_delay_ms(1);
        timeout--;
    }

    TEST_INFO("Sent %d/%d frames", success_count, frame_count);
    return (success_count == frame_count);
}

/**
 * @brief  测试3: 大数据发送
 * @note   发送超过单次DMA传输的数据，验证DMA正常工作
 */
static bool test_dma_large_send(void)
{
    /* 构造300字节数据 */
    uint8_t data[300];
    for (int i = 0; i < 300; i++) {
        data[i] = (uint8_t)('A' + (i % 26));
    }

    /* 等待上一次发送完成 */
    uint32_t wait = 1000;
    while (!bsp_uart_tx_idle() && wait > 0) {
        bsp_delay_ms(1);
        wait--;
    }

    bsp_status_t ret = bsp_uart_send_dma(data, sizeof(data));
    if (ret != BSP_OK) {
        TEST_INFO("bsp_uart_send_dma returned %d", ret);
        return false;
    }

    /* 等待DMA完成 */
    uint32_t timeout = 3000;
    while (!bsp_uart_tx_idle() && timeout > 0) {
        bsp_delay_ms(1);
        timeout--;
    }

    if (timeout == 0) {
        TEST_INFO("DMA timeout");
        return false;
    }

    TEST_INFO("Sent %d bytes", (int)sizeof(data));
    return true;
}

/**
 * @brief  测试4: 忙碌状态检测
 * @note   发送数据后立即再次发送，验证返回BSP_ERR_BUSY
 */
static bool test_dma_buffer_full(void)
{
    /* 先发送一些数据 */
    uint8_t data[256];
    memset(data, 'X', sizeof(data));

    /* 等待空闲 */
    uint32_t wait = 1000;
    while (!bsp_uart_tx_idle() && wait > 0) {
        bsp_delay_ms(1);
        wait--;
    }

    bsp_status_t ret = bsp_uart_send_dma(data, sizeof(data));
    if (ret != BSP_OK) {
        TEST_INFO("First send failed: %d", ret);
        return false;
    }

    /* 立即再次发送，应该返回BUSY */
    ret = bsp_uart_send_dma(data, sizeof(data));
    if (ret == BSP_ERR_BUSY) {
        TEST_INFO("Correctly returned BUSY");
    } else {
        TEST_INFO("Expected BUSY, got %d", ret);
        return false;
    }

    /* 等待DMA完成 */
    uint32_t timeout = 5000;
    while (!bsp_uart_tx_idle() && timeout > 0) {
        bsp_delay_ms(1);
        timeout--;
    }

    return true;
}

/**
 * @brief  测试5: 空闲检测
 * @note   验证bsp_uart_tx_idle()是否正确
 */
static bool test_dma_idle_detection(void)
{
    /* 初始状态应该是空闲 */
    if (!bsp_uart_tx_idle()) {
        TEST_INFO("Not idle initially");
        return false;
    }

    /* 发送数据 */
    const char *msg = "Idle Detection Test\r\n";
    bsp_status_t ret = bsp_uart_send_dma((const uint8_t *)msg,
                                          (uint16_t)strlen(msg));
    if (ret != BSP_OK) {
        TEST_INFO("Send failed: %d", ret);
        return false;
    }

    /* 发送后应该忙碌 */
    if (bsp_uart_tx_idle()) {
        TEST_INFO("Should be busy after send");
        return false;
    }

    /* 等待完成 */
    uint32_t timeout = 1000;
    while (!bsp_uart_tx_idle() && timeout > 0) {
        bsp_delay_ms(1);
        timeout--;
    }

    if (timeout == 0) {
        TEST_INFO("DMA timeout");
        return false;
    }

    /* 完成后应该恢复空闲 */
    if (!bsp_uart_tx_idle()) {
        TEST_INFO("Not idle after completion");
        return false;
    }

    return true;
}

/**
 * @brief  测试6: 阻塞发送对比
 * @note   对比bsp_uart_puts()和bsp_uart_send_dma()
 */
static bool test_blocking_vs_dma(void)
{
    const char *msg1 = "[Blocking] Hello World\r\n";
    const char *msg2 = "[DMA] Hello World\r\n";

    /* 阻塞发送 */
    bsp_status_t ret1 = bsp_uart_puts(msg1);
    if (ret1 != BSP_OK) {
        TEST_INFO("Blocking send failed: %d", ret1);
        return false;
    }

    /* DMA发送 */
    bsp_status_t ret2 = bsp_uart_send_dma((const uint8_t *)msg2,
                                           (uint16_t)strlen(msg2));
    if (ret2 != BSP_OK) {
        TEST_INFO("DMA send failed: %d", ret2);
        return false;
    }

    /* 等待DMA完成 */
    uint32_t timeout = 1000;
    while (!bsp_uart_tx_idle() && timeout > 0) {
        bsp_delay_ms(1);
        timeout--;
    }

    return true;
}

/* ======================== 测试入口 ======================== */

void test_uart_dma_run(void)
{
    printf("\r\n============================\r\n");
    printf("UART DMA Test Suite\r\n");
    printf("============================\r\n\r\n");

    int pass_count = 0;
    int fail_count = 0;

    struct {
        const char *name;
        bool (*func)(void);
    } tests[] = {
        {"Basic DMA Send",      test_dma_basic_send},
        {"Continuous DMA Send", test_dma_continuous_send},
        {"Large Data Send",     test_dma_large_send},
        {"Buffer Full",         test_dma_buffer_full},
        {"Idle Detection",      test_dma_idle_detection},
        {"Blocking vs DMA",     test_blocking_vs_dma},
    };

    int test_count = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < test_count; i++) {
        /* 清零计数器 */
        dbg_isr_count = 0;
        dbg_uart_isr_total = 0;
        dbg_iidx_value = 0;
        dbg_eot_count = 0;
        dbg_rx_count = 0;
        dbg_other_count = 0;

        printf("Test %d/%d: %s\r\n", i + 1, test_count, tests[i].name);

        bool result = tests[i].func();
        if (result) {
            TEST_PASS(tests[i].name);
            pass_count++;
        } else {
            TEST_FAIL(tests[i].name);
            fail_count++;
        }

        /* 打印详细调试计数器 */
        TEST_INFO("isr_total=%lu iidx=%lu dma_tx=%lu eot=%lu rx=%lu other=%lu",
            dbg_uart_isr_total, dbg_iidx_value, dbg_isr_count,
            dbg_eot_count, dbg_rx_count, dbg_other_count);

        /* 测试间隔 */
        bsp_delay_ms(100);
    }

    printf("\r\n============================\r\n");
    printf("Result: %d PASS, %d FAIL\r\n", pass_count, fail_count);
    printf("============================\r\n\r\n");
}
