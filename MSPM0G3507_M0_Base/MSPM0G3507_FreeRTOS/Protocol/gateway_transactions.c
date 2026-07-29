/**
 * @file    gateway_transactions.c
 * @brief   Board B 网关事务管理模块实现 (规范 §8.2, §8.3)
 * @note    事务表大小 PROTO_GATEWAY_TXN_TABLE_SIZE=8。
 *          ACK 超时 40ms，最多重试 2 次，Host 总超时 250ms。
 *          重试使用同一 board_seq 和相同 payload (§8.3)。
 *          不分配堆内存。
 */
#include "gateway_transactions.h"
#include <string.h>

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** 哨兵值: 表示事务尚未发送 (mark_sent 未调用)。
 *  使用 UINT32_MAX 而非 0，因为 t=0 是有效时间戳。 */
#define TXN_NOT_SENT_MS  ((uint32_t)0xFFFFFFFFu)

/* ========================================================================
 * 内部工具
 * ======================================================================== */

/** 根据事务指针计算在表中的索引 (返回 -1 表示不在表中) */
static int txn_index(const gateway_txn_manager_t *mgr,
                     const gateway_transaction_t *txn)
{
    if (mgr == NULL || txn == NULL) {
        return -1;
    }

    /* 指针减法计算索引 */
    ptrdiff_t idx = txn - mgr->table;
    if (idx < 0 || idx >= (ptrdiff_t)PROTO_GATEWAY_TXN_TABLE_SIZE) {
        return -1;
    }
    return (int)idx;
}

/** 判断事务是否处于待处理状态 */
static bool txn_is_pending(const gateway_transaction_t *txn)
{
    return (txn->used &&
            (txn->state == PROTO_TXN_WAIT_BOARD_ACK ||
             txn->state == PROTO_TXN_WAIT_BOARD_STATUS));
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化函数 gateway_txn_init，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @return 函数执行结果。
 */
void gateway_txn_init(gateway_txn_manager_t *mgr)
{
    if (mgr == NULL) {
        return;
    }

    memset(mgr, 0, sizeof(*mgr));
    mgr->next_board_seq = 1u;  /* 推荐从 1 开始，0 保留 (§8.1) */
}

/**
 * @brief 执行函数 gateway_txn_alloc，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @return 函数执行结果。
 */
gateway_transaction_t* gateway_txn_alloc(gateway_txn_manager_t *mgr)
{
    if (mgr == NULL) {
        return NULL;
    }

    for (size_t i = 0u; i < PROTO_GATEWAY_TXN_TABLE_SIZE; i++) {
        if (!mgr->table[i].used) {
            /* 找到空闲槽位，初始化 */
            gateway_transaction_t *txn = &mgr->table[i];
            memset(txn, 0, sizeof(*txn));

            txn->used         = true;
            txn->board_seq    = mgr->next_board_seq;
            txn->state        = PROTO_TXN_WAIT_BOARD_ACK;
            txn->retry_count  = 0u;
            txn->sent_ms      = TXN_NOT_SENT_MS;  /* 哨兵: 尚未发送 */

            /* created_ms 清零，等待 gateway_txn_mark_sent 设置 */
            mgr->created_ms[i] = 0u;

            /* 递增 board_seq (0xFFFF 后回绕到 0，§8.1) */
            mgr->next_board_seq++;

            return txn;
        }
    }

    return NULL;  /* 事务表已满 */
}

/**
 * @brief 查找函数 gateway_txn_find_by_board_seq，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @param board_seq 函数参数 board_seq。
 * @return 函数执行结果。
 */
gateway_transaction_t* gateway_txn_find_by_board_seq(gateway_txn_manager_t *mgr,
                                                      uint16_t board_seq)
{
    if (mgr == NULL) {
        return NULL;
    }

    for (size_t i = 0u; i < PROTO_GATEWAY_TXN_TABLE_SIZE; i++) {
        if (mgr->table[i].used && mgr->table[i].board_seq == board_seq) {
            return &mgr->table[i];
        }
    }
    return NULL;
}

/**
 * @brief 查找函数 gateway_txn_find_by_host_seq，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @param host_seq 函数参数 host_seq。
 * @return 函数执行结果。
 */
gateway_transaction_t* gateway_txn_find_by_host_seq(gateway_txn_manager_t *mgr,
                                                     uint16_t host_seq)
{
    if (mgr == NULL) {
        return NULL;
    }

    for (size_t i = 0u; i < PROTO_GATEWAY_TXN_TABLE_SIZE; i++) {
        if (mgr->table[i].used && mgr->table[i].host_seq == host_seq) {
            return &mgr->table[i];
        }
    }
    return NULL;
}

/**
 * @brief 执行函数 gateway_txn_complete，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @param txn 函数参数 txn。
 * @return 函数执行结果。
 */
void gateway_txn_complete(gateway_txn_manager_t *mgr, gateway_transaction_t *txn)
{
    (void)mgr;  /* mgr 仅用于一致性检查，当前无需使用 */

    if (txn == NULL) {
        return;
    }

    txn->state = PROTO_TXN_DONE;
}

/**
 * @brief 执行函数 gateway_txn_fail，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @param txn 函数参数 txn。
 * @return 函数执行结果。
 */
void gateway_txn_fail(gateway_txn_manager_t *mgr, gateway_transaction_t *txn)
{
    (void)mgr;

    if (txn == NULL) {
        return;
    }

    txn->state = PROTO_TXN_FAILED;
}

/**
 * @brief 执行函数 gateway_txn_free，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @param txn 函数参数 txn。
 * @return 函数执行结果。
 */
void gateway_txn_free(gateway_txn_manager_t *mgr, gateway_transaction_t *txn)
{
    if (mgr == NULL || txn == NULL) {
        return;
    }

    int idx = txn_index(mgr, txn);
    if (idx < 0) {
        return;
    }

    /* 释放槽位 */
    txn->used = false;
    mgr->created_ms[(size_t)idx] = 0u;
}

/**
 * @brief 执行函数 gateway_txn_pending_count，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @return 函数执行结果。
 */
size_t gateway_txn_pending_count(const gateway_txn_manager_t *mgr)
{
    if (mgr == NULL) {
        return 0u;
    }

    size_t count = 0u;
    for (size_t i = 0u; i < PROTO_GATEWAY_TXN_TABLE_SIZE; i++) {
        if (txn_is_pending(&mgr->table[i])) {
            count++;
        }
    }
    return count;
}

/**
 * @brief 执行函数 gateway_txn_mark_sent，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @param txn 函数参数 txn。
 * @param now_ms 函数参数 now_ms。
 * @return 函数执行结果。
 */
void gateway_txn_mark_sent(gateway_txn_manager_t *mgr,
                           gateway_transaction_t *txn, uint32_t now_ms)
{
    if (mgr == NULL || txn == NULL) {
        return;
    }

    int idx = txn_index(mgr, txn);
    if (idx < 0) {
        return;
    }

    /* 首次发送设置 created_ms (用于 Host 总超时判断)。
     * 使用 sent_ms 哨兵判断是否首次发送，而非 created_ms==0 (t=0 是有效时间戳)。 */
    if (txn->sent_ms == TXN_NOT_SENT_MS) {
        mgr->created_ms[(size_t)idx] = now_ms;
    }

    /* 每次发送/重发更新 sent_ms (用于 per-attempt ACK 超时判断) */
    txn->sent_ms = now_ms;
}

/**
 * @brief 执行周期处理函数 gateway_txn_tick，完成对应模块的功能处理。
 * @param mgr 函数参数 mgr。
 * @param now_ms 函数参数 now_ms。
 * @param callback 函数参数 callback。
 * @return 函数执行结果。
 */
void gateway_txn_tick(gateway_txn_manager_t *mgr, uint32_t now_ms,
                      gateway_txn_timeout_cb callback)
{
    if (mgr == NULL) {
        return;
    }

    for (size_t i = 0u; i < PROTO_GATEWAY_TXN_TABLE_SIZE; i++) {
        gateway_transaction_t *txn = &mgr->table[i];

        if (!txn_is_pending(txn)) {
            continue;
        }

        uint32_t created = mgr->created_ms[i];
        uint32_t sent    = txn->sent_ms;

        /*
         * 如果事务尚未发送 (mark_sent 未调用)，跳过超时检查。
         * 使用 sent_ms 哨兵判断，而非 created_ms==0 (t=0 是有效时间戳)。
         */
        if (txn->sent_ms == TXN_NOT_SENT_MS) {
            continue;
        }

        /* 1. Host 总超时检查 (250ms, §8.3) */
        uint32_t host_age = (uint32_t)(now_ms - created);
        if (host_age >= PROTO_GATEWAY_HOST_TIMEOUT_MS) {
            mgr->timeout_count++;
            txn->state = PROTO_TXN_FAILED;
            if (callback != NULL) {
                callback(mgr, txn, false);
            }
            continue;
        }

        /*
         * 2. per-attempt ACK 超时检查 (40ms, §8.3)
         *    仅适用于 WAIT_BOARD_ACK 状态。
         *    WAIT_BOARD_STATUS 状态 (QUERY 已收到 ACK，等待后续 STATUS 帧)
         *    仅受 Host 总超时约束，不触发 per-attempt 重试。
         */
        if (txn->state != PROTO_TXN_WAIT_BOARD_ACK) {
            continue;  /* WAIT_BOARD_STATUS: 跳过 ACK 重试检查 */
        }

        uint32_t ack_age = (uint32_t)(now_ms - sent);
        if (ack_age < PROTO_GATEWAY_ACK_TIMEOUT_MS) {
            continue;  /* 未超时 */
        }

        /* ACK 超时 */
        mgr->timeout_count++;

        if (txn->retry_count < PROTO_GATEWAY_MAX_RETRIES) {
            /* 可重试: 递增 retry_count，重置 sent_ms，通知回调重发 (§8.3) */
            txn->retry_count++;
            mgr->retry_count++;
            txn->sent_ms = now_ms;  /* 重置 per-attempt 计时 */

            if (callback != NULL) {
                callback(mgr, txn, true);
            }
        } else {
            /* 重试次数已耗尽 → 最终失败 (§8.3) */
            txn->state = PROTO_TXN_FAILED;
            if (callback != NULL) {
                callback(mgr, txn, false);
            }
        }
    }
}
