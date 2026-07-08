#ifndef TF_485_TXQ_H
#define TF_485_TXQ_H

#include "tf_485_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 发送队列结构体层级关系
 * ─────────────────────────────────────────────────
 *
 *  发送队列（tf_485_txq）从主站层接收
 *  待发送的 Modbus 帧，按 FIFO 顺序排队，等待总线空闲后逐帧发送。
 *
 *  每个队列条目（tf_485_txq_item_t）包含：
 *    - 帧数据在队列环形缓冲区中的位置（frame_pos / frame_len）
 *    - 发完该帧后续等待的响应特征（expect: tf_485_rsp_expect_t）
 *
 *  当 tf_485_master_task() 检测到总线空闲时，
 *  调用 tf_485_txq_pop() 取出下一帧，拷贝到
 *  tf_485_t.tx.dma_buf 并启动 DMA 发送。
 *
 *  发出帧后，将 tf_485_txq_pop() 拿到的 expect 存入 master.rsp_expect，
 *  用于在接收 FIFO 中匹配对应响应。
 * ─────────────────────────────────────────────────
 */

/**
 * @brief 期望响应描述
 *        在发送一个 Modbus 请求帧之前，构造本结构描述期望从机返回的
 *        响应特征。当接收通道收到数据时，用此结构与接收帧的头部进行
 *        匹配（地址、功能码），匹配成功后才进行 CRC 校验和解析。
 *        匹配失败则丢弃该字节继续扫描（见 tf_485_master.c 的
 *        tf_485_wait_pop_frame()）。
 */
typedef struct {
    uint8_t slave_addr;                 /**< 从机 Modbus 地址                              */
    tf_485_dev_type_t dev_type;         /**< 设备类型枚举，决定响应由哪个设备解析函数处理  */
    tf_485_read_type_t read_type;       /**< 读请求类型：读寄存器 / 读线圈                  */
    tf_485_req_type_t req_type;         /**< 主站请求类型，决定响应帧形态和解析分发          */
    uint16_t start_addr;                /**< 起始寄存器地址或起始位地址                     */
    uint16_t quantity;                  /**< 期望读取的寄存器数量或位数量                   */
    uint8_t rsp_func;                   /**< 期望的 Modbus 功能码（如 0x03 读保持寄存器）   */
    uint8_t rsp_data_bytes;             /**< 期望响应中的数据字节数（不含地址、功能码、CRC）*/
    uint16_t echo_addr;                 /**< 写单线圈/寄存器响应中期望回显的地址             */
    uint16_t echo_value;                /**< 写单线圈/寄存器响应中期望回显的值               */
} tf_485_rsp_expect_t;

/**
 * @brief 发送队列条目
 *        发送环形队列中的一个元素，记录待发帧在队列缓冲区中的位置
 *        以及该帧对应的响应期望。
 *
 * @note  队列使用环形缓冲区（帧数据区）+ 本条目结构体数组的形式管理：
 *        - frame_pos 指向帧数据在数据区中的起始位置；
 *        - frame_len 为帧长度；
 *        - expect 在入队时拷贝，供出队后传递给 master 层的 rsp_expect。
 */
typedef struct {
    uint16_t frame_pos;             /**< 帧数据在发送环形缓冲区中的起始索引       */
    uint16_t frame_len;             /**< 帧长度（字节）                          */
    tf_485_rsp_expect_t expect;     /**< 该帧对应的期望响应描述（出队时使用）     */
} tf_485_txq_item_t;

#if MODULE_MODBUS_ENABLE_DIAG
/**
 * @brief 发送队列诊断信息
 *        记录队列的运行统计，用于调试和性能分析。
 *        通过 MODULE_MODBUS_ENABLE_DIAG 控制编译。
 */
typedef struct {
    uint16_t write_pos;             /**< 当前环形缓冲区的写入位置索引              */
    uint32_t enqueue_count;         /**< 累计入队次数（自复位以来）               */
    uint32_t dequeue_count;         /**< 累计出队次数                             */
    uint32_t overwrite_drop_count;  /**< 因入队时覆盖旧数据而丢弃的旧帧计数       */
    uint32_t full_drop_count;       /**< 因队列已满而丢弃的新帧计数               */
    uint32_t invalid_count;         /**< 无效入队请求（参数错误等）计数           */
} tf_485_txq_diag_t;
#endif

void tf_485_txq_reset(void);
bool tf_485_txq_push(const uint8_t *frame,
                     uint16_t frame_len,
                     const tf_485_rsp_expect_t *expect);
bool tf_485_txq_pop(uint8_t *frame,
                    uint16_t frame_capacity,
                    uint16_t *frame_len,
                    tf_485_rsp_expect_t *expect);
uint8_t tf_485_txq_count(void);
#if MODULE_MODBUS_ENABLE_DIAG
void tf_485_txq_get_diag(tf_485_txq_diag_t *diag);
void tf_485_txq_clear_diag(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* TF_485_TXQ_H */
