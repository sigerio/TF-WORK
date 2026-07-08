#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 环形缓冲区控制块
 *        使用 mirror 位技巧区分"空"和"满"两种状态：
 *          空: read_mirror == write_mirror && read_index == write_index
 *          满: read_mirror != write_mirror && read_index == write_index
 *
 *  读/写指针各配合一个 mirror 翻转位：
 *    - 读指针 (read_index) 追上写指针时，检查 mirror 是否一致判断空/满
 *    - 写指针 (write_index) 追上读指针时，翻转 write_mirror 标记满
 *    - 读指针追上写指针时，翻转 read_mirror 标记空
 *
 *  @note  本实现在 RS485 总线接收通道中用作二级缓冲
 *         （tf_485_rx_t.fifo），暂存 IDLE 中断推送来的数据帧，
 *         供主站任务轮询读取。所有读写操作须关中断保护。
 */
typedef struct {
    uint8_t *buf;               /**< 底层存储区指针（由外部提供，不在此结构内部分配）*/
    uint32_t read_mirror;       /**< 读指针 mirror 位，用于区分空/满                */
    uint32_t read_index;        /**< 读指针索引（下一个读出位置）                    */
    uint32_t write_mirror;      /**< 写指针 mirror 位                                */
    uint32_t write_index;       /**< 写指针索引（下一个写入位置）                    */
    uint32_t buffer_size;       /**< 缓冲区总容量（字节），必须是 2 的幂             */
    uint32_t overflow;          /**< 累计溢出字节数（写入超出可用空间时累加）        */
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb, uint8_t *buf, uint32_t size);
void ringbuf_reset(ringbuf_t *rb);
uint32_t ringbuf_capacity(const ringbuf_t *rb);
uint32_t ringbuf_available(const ringbuf_t *rb);
uint32_t ringbuf_free(const ringbuf_t *rb);
uint32_t ringbuf_overflow_count(const ringbuf_t *rb);
uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint32_t len);
uint32_t ringbuf_write_force(ringbuf_t *rb, const uint8_t *data, uint32_t len);
uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint32_t len);
uint32_t ringbuf_peek(const ringbuf_t *rb, uint8_t *data, uint32_t len);
uint32_t ringbuf_drop(ringbuf_t *rb, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* RINGBUF_H */
