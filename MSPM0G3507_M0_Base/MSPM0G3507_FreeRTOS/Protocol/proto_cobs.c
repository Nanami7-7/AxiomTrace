/**
 * @file    proto_cobs.c
 * @brief   COBS (Consistent Overhead Byte Stuffing) 编解码实现 (规范 §4.2)
 * @note    标准 COBS 算法，O(n) 时间，原地约束 0x00。
 *          编码器输出保证不含 0x00；不追加分隔符。
 *          解码器拒绝含 0x00 的编码数据、code 越界和输出超长。
 *          不分配堆内存。
 */
#include "proto_cobs.h"

/* ========================================================================
 * COBS 编码
 *
 * 算法:
 *   output[0] = 首指针 (指向下一个指针或末尾+1)
 *   遍历 input:
 *     遇到 0x00 -> 写入当前指针值，开始新块
 *     非 0x00   -> 复制字节，指针计数+1
 *     距离达 254 -> 强制写入指针 (code=0xFF)，防止溢出 255
 *   最终写入末尾指针
 *
 * 开销: 最坏 N + 1 + floor(N/254)；140 字节逻辑帧最多 141 字节
 * ======================================================================== */
bool proto_cobs_encode(const uint8_t *src, size_t src_len,
                       uint8_t *dst, size_t dst_cap, size_t *dst_len)
{
    if (dst == NULL || dst_len == NULL) {
        return false;
    }
    if (src == NULL && src_len > 0) {
        return false;
    }

    /* 最坏输出: src_len + 1 (初始 code) + floor(src_len/254) (强制 code) */
    size_t max_out = src_len + 1u + src_len / 254u;
    if (dst_cap < max_out) {
        return false;
    }

    size_t  out_idx  = 1u;   /* output[0] 留给首指针 */
    uint8_t code     = 1u;
    size_t  code_idx = 0u;   /* 当前指针写入位置 */

    for (size_t i = 0u; i < src_len; i++) {
        if (src[i] == 0x00u) {
            /* 遇到 0x00: 写入当前指针值，开始新块 */
            dst[code_idx] = code;
            code_idx = out_idx;
            code = 1u;
            out_idx++;
        } else {
            /* 非 0x00: 复制字节，指针计数+1 */
            dst[out_idx] = src[i];
            out_idx++;
            code++;

            /* 距离达 254: 强制写入指针 (code=0xFF) */
            if (code == 0xFFu) {
                dst[code_idx] = code;
                code_idx = out_idx;
                code = 1u;
                out_idx++;
            }
        }
    }

    /* 写入最终指针 */
    dst[code_idx] = code;
    *dst_len = out_idx;
    return true;
}

/* ========================================================================
 * COBS 解码
 *
 * 算法:
 *   读取首字节作为第一个指针 code
 *   复制 code-1 个非零字节到输出
 *   若 code < 0xFF 且不是末尾，插入 0x00
 *   重复直到编码数据耗尽
 *
 * 拒绝条件:
 *   1. src_len == 0 (无 code 可读)
 *   2. 编码数据中含 0x00 (COBS 保证不应出现)
 *   3. code 指向输入之外 (in_idx + copy_len > src_len)
 *   4. 输出超出 dst_cap
 *   5. 解码出的非零区域中出现 0x00 (数据损坏)
 * ======================================================================== */
/**
 * @brief 解码函数 proto_cobs_decode，完成对应模块的功能处理。
 * @param src 函数参数 src。
 * @param src_len 函数参数 src_len。
 * @param dst 函数参数 dst。
 * @param dst_cap 函数参数 dst_cap。
 * @param dst_len 函数参数 dst_len。
 * @return 函数执行结果。
 */
bool proto_cobs_decode(const uint8_t *src, size_t src_len,
                       uint8_t *dst, size_t dst_cap, size_t *dst_len)
{
    if (src == NULL || dst == NULL || dst_len == NULL || src_len == 0u) {
        return false;
    }

    size_t out_idx = 0u;
    size_t in_idx  = 0u;

    while (in_idx < src_len) {
        uint8_t code = src[in_idx];
        if (code == 0x00u) {
            /* COBS 编码数据不应含 0x00 */
            return false;
        }

        in_idx++;

        /* 复制 code-1 个非零字节 */
        size_t copy_len = (size_t)(code - 1u);
        if (in_idx + copy_len > src_len) {
            /* 指针超出数据范围 */
            return false;
        }
        if (out_idx + copy_len > dst_cap) {
            /* 输出超出容量 */
            return false;
        }

        for (size_t i = 0u; i < copy_len; i++) {
            dst[out_idx] = src[in_idx];
            if (dst[out_idx] == 0x00u) {
                /* 非零区域中出现 0x00，数据损坏 */
                return false;
            }
            out_idx++;
            in_idx++;
        }

        /* 若 code < 0xFF 且不是最后一个指针，插入 0x00 */
        if (code < 0xFFu && in_idx < src_len) {
            if (out_idx >= dst_cap) {
                return false;
            }
            dst[out_idx] = 0x00u;
            out_idx++;
        }
    }

    *dst_len = out_idx;
    return true;
}
