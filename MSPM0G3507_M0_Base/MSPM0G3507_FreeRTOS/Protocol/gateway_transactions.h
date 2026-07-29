/**
 * @file    gateway_transactions.h
 * @brief   Board B 网关事务管理模块 (规范 §8.2, §8.3)
 * @note    事务表大小 PROTO_GATEWAY_TXN_TABLE_SIZE=8。
 *          Board B 等待 Board A ACK/NACK 超时: 40ms。
 *          普通命令最多重试 2 次，重试使用同一 board_seq 和相同 payload。
 *          Host 事务总超时: 250ms。
 *          HEARTBEAT 不走普通事务表。
 *          不分配堆内存。
 */
#ifndef GATEWAY_TRANSACTIONS_H
#define GATEWAY_TRANSACTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "proto_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 事务表管理器上下文
 * ======================================================================== */

/**
 * @brief  事务表管理器
 * @note   table[] 为事务槽位数组 (PROTO_GATEWAY_TXN_TABLE_SIZE=8)。
 *         created_ms[] 平行数组记录每个事务的首次发送时间，用于 Host 总超时判断
 *         (区别于 per-attempt ACK 超时使用的 txn->sent_ms)。
 *         next_board_seq 为下一条发往 Board A 的事务分配的序号。
 */
typedef struct {
    gateway_transaction_t table[PROTO_GATEWAY_TXN_TABLE_SIZE];
    uint32_t              created_ms[PROTO_GATEWAY_TXN_TABLE_SIZE];
    uint16_t              next_board_seq;
    uint32_t              timeout_count;   /**< 统计: ACK 超时次数 */
    uint32_t              retry_count;     /**< 统计: 重试次数 */
    void                 *user_data;       /**< 调用方上下文 (如 gateway_router_t*)，
                                               供超时回调访问路由器 */
} gateway_txn_manager_t;

/* ========================================================================
 * 超时回调类型
 * ======================================================================== */

/**
 * @brief  事务超时回调
 * @param  mgr       事务管理器
 * @param  txn       超时的事务指针
 * @param  can_retry true=可重试 (retry_count 未超限且 Host 总超时未到)，
 *                   此时 tick 已递增 retry_count 并重置 sent_ms，
 *                   回调应重新发送 Board A 帧；
 *                   false=不可重试 (重试耗尽或 Host 总超时)，
 *                   此时 tick 已将事务标记为 FAILED，
 *                   回调应向 Host 生成本地通信错误。
 */
typedef void (*gateway_txn_timeout_cb)(gateway_txn_manager_t *mgr,
                                        gateway_transaction_t *txn,
                                        bool can_retry);

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief  初始化事务表管理器
 * @param  mgr 事务管理器
 * @note   next_board_seq 初始化为 1 (推荐从 1 开始，0 保留)。
 */
void gateway_txn_init(gateway_txn_manager_t *mgr);

/**
 * @brief  分配空闲事务槽
 * @param  mgr 事务管理器
 * @retval 非空  分配成功，返回事务指针 (state 已置为 WAIT_BOARD_ACK)
 * @retval NULL  事务表已满
 * @note   分配时 board_seq 取 next_board_seq 并递增 (0xFFFF 后回绕到 0)。
 *         retry_count 初始化为 0。created_ms 和 sent_ms 初始化为 0，
 *         调用方在首次发送后应调用 gateway_txn_mark_sent 设置时间戳。
 */
gateway_transaction_t* gateway_txn_alloc(gateway_txn_manager_t *mgr);

/**
 * @brief  按 board_seq 查找待处理事务
 * @param  mgr       事务管理器
 * @param  board_seq Board A 链路序号
 * @retval 非空     匹配的事务 (used 且 board_seq 匹配)
 * @retval NULL     未找到
 */
gateway_transaction_t* gateway_txn_find_by_board_seq(gateway_txn_manager_t *mgr,
                                                      uint16_t board_seq);

/**
 * @brief  按 host_seq 查找待处理事务
 * @param  mgr      事务管理器
 * @param  host_seq 上位机序号
 * @retval 非空    匹配的事务 (used 且 host_seq 匹配)
 * @retval NULL    未找到
 */
gateway_transaction_t* gateway_txn_find_by_host_seq(gateway_txn_manager_t *mgr,
                                                     uint16_t host_seq);

/**
 * @brief  标记事务完成
 * @param  mgr 事务管理器
 * @param  txn 事务指针
 * @note   将 state 置为 DONE。不释放槽位，调用方应在处理完结果后调用
 *         gateway_txn_free 释放。
 */
void gateway_txn_complete(gateway_txn_manager_t *mgr, gateway_transaction_t *txn);

/**
 * @brief  标记事务失败
 * @param  mgr 事务管理器
 * @param  txn 事务指针
 * @note   将 state 置为 FAILED。不释放槽位，调用方应在处理完结果后调用
 *         gateway_txn_free 释放。
 */
void gateway_txn_fail(gateway_txn_manager_t *mgr, gateway_transaction_t *txn);

/**
 * @brief  释放事务槽
 * @param  mgr 事务管理器
 * @param  txn 事务指针
 * @note   将 used 置为 false，清空 created_ms。此后该槽位可被重新分配。
 */
void gateway_txn_free(gateway_txn_manager_t *mgr, gateway_transaction_t *txn);

/**
 * @brief  待处理事务数
 * @param  mgr 事务管理器
 * @retval 当前 used 且 state 为 WAIT_BOARD_ACK / WAIT_BOARD_STATUS 的事务数
 */
size_t gateway_txn_pending_count(const gateway_txn_manager_t *mgr);

/**
 * @brief  超时检查 — 在每个周期 tick 中调用
 * @param  mgr      事务管理器
 * @param  now_ms   当前时间戳
 * @param  callback 超时回调 (可为 NULL)
 * @note   对每个 WAIT_BOARD_ACK / WAIT_BOARD_STATUS 状态的事务:
 *           1. 检查 Host 总超时 (now - created_ms >= 250ms):
 *              若超时 → 标记 FAILED, 调用 callback(can_retry=false)
 *           2. 检查 per-attempt ACK 超时 (now - sent_ms >= 40ms):
 *              若 retry_count < MAX_RETRIES → retry_count++, sent_ms=now,
 *              调用 callback(can_retry=true)
 *              否则 → 标记 FAILED, 调用 callback(can_retry=false)
 *         时间差使用无符号减法，支持 uint32_t 回绕。
 */
void gateway_txn_tick(gateway_txn_manager_t *mgr, uint32_t now_ms,
                      gateway_txn_timeout_cb callback);

/* ========================================================================
 * 辅助 API
 * ======================================================================== */

/**
 * @brief  记录事务发送时间
 * @param  mgr    事务管理器
 * @param  txn    事务指针
 * @param  now_ms 当前时间戳
 * @note   首次调用设置 created_ms (用于 Host 总超时判断)；
 *         每次调用更新 txn->sent_ms (用于 per-attempt ACK 超时判断)。
 *         路由器在首次发送和重发后均应调用此函数。
 */
void gateway_txn_mark_sent(gateway_txn_manager_t *mgr,
                           gateway_transaction_t *txn, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif /* GATEWAY_TRANSACTIONS_H */
