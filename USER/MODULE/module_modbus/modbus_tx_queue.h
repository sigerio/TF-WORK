#ifndef MODBUS_TX_QUEUE_H
#define MODBUS_TX_QUEUE_H

#include "module_modbus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 发送队列结构体层级关系
 * ─────────────────────────────────────────────────
 *
 *  发送队列（modbus_tx_queue）从 module_modbus 层接收
 *  待发送的 Modbus 帧，按 FIFO 顺序排队，等待总线空闲后逐帧发送。
 *
 *  每个队列条目（modbus_tx_queue_item_t）包含：
 *    - 帧数据在队列环形缓冲区中的位置（buf_index / len）
 *    - 发完该帧后续等待的响应特征（response: modbus_response_expect_t）
 *
 *  当 module_modbus_master_task() 检测到总线空闲时，
 *  调用 modbus_tx_queue_dequeue() 取出下一帧，拷贝到
 *  modbus_rs485_bus_t.tx.dma_buf 并启动 DMA 发送。
 *
 *  发出帧后，将 dequeue 拿到的 response 存入 master.waiting_item，
 *  用于在接收 FIFO 中匹配对应响应。
 * ─────────────────────────────────────────────────
 */

/**
 * @brief 期望响应描述
 *        在发送一个 Modbus 请求帧之前，构造本结构描述期望从机返回的
 *        响应特征。当接收通道收到数据时，用此结构与接收帧的头部进行
 *        匹配（地址、功能码），匹配成功后才进行 CRC 校验和解析。
 *        匹配失败则丢弃该字节继续扫描（见 module_modbus.c 的
 *        module_modbus_pop_waiting_response()）。
 */
typedef struct {
    uint8_t addr;                              /**< 从机 Modbus 地址                              */
    module_modbus_device_type_t device;         /**< 设备类型枚举，决定响应由哪个设备解析函数处理  */
    module_modbus_poll_function_t function;     /**< 轮询功能类型：读寄存器 / 读线圈                */
    uint16_t start;                            /**< 起始寄存器地址或起始位地址                     */
    uint16_t count;                            /**< 期望读取的寄存器数量或位数量                   */
    uint8_t expected_func;                     /**< 期望的 Modbus 功能码（如 0x03 读保持寄存器）   */
    uint8_t expected_bytes;                    /**< 期望响应中的数据字节数（不含地址、功能码、CRC）*/
} modbus_response_expect_t;

/**
 * @brief 发送队列条目
 *        发送环形队列中的一个元素，记录待发帧在队列缓冲区中的位置
 *        以及该帧对应的响应期望。
 *
 * @note  队列使用环形缓冲区（帧数据区）+ 本条目结构体数组的形式管理：
 *        - buf_index 指向帧数据在数据区中的起始位置；
 *        - len 为帧长度；
 *        - response 在入队时拷贝，供出队后传递给 master 层的 waiting_item。
 */
typedef struct {
    uint16_t buf_index;             /**< 帧数据在发送环形缓冲区中的起始索引       */
    uint16_t len;                   /**< 帧长度（字节）                          */
    modbus_response_expect_t response; /**< 该帧对应的期望响应描述（出队时使用） */
} modbus_tx_queue_item_t;

#if MODULE_MODBUS_ENABLE_DIAG
/**
 * @brief 发送队列诊断信息
 *        记录队列的运行统计，用于调试和性能分析。
 *        通过 MODULE_MODBUS_ENABLE_DIAG 控制编译。
 */
typedef struct {
    uint16_t write_index;           /**< 当前环形缓冲区的写入位置索引              */
    uint32_t enqueue_count;         /**< 累计入队次数（自复位以来）               */
    uint32_t dequeue_count;         /**< 累计出队次数                             */
    uint32_t overwrite_drop_count;  /**< 因入队时覆盖旧数据而丢弃的旧帧计数       */
    uint32_t full_drop_count;       /**< 因队列已满而丢弃的新帧计数               */
    uint32_t invalid_count;         /**< 无效入队请求（参数错误等）计数           */
} modbus_tx_queue_diag_t;
#endif

void modbus_tx_queue_reset(void);
bool modbus_tx_queue_enqueue(const uint8_t *frame,
                             uint16_t len,
                             const modbus_response_expect_t *response);
bool modbus_tx_queue_dequeue(uint8_t *frame,
                             uint16_t frame_capacity,
                             uint16_t *frame_len,
                             modbus_response_expect_t *response);
uint8_t modbus_tx_queue_count(void);
#if MODULE_MODBUS_ENABLE_DIAG
void modbus_tx_queue_get_diag(modbus_tx_queue_diag_t *diag);
void modbus_tx_queue_clear_diag(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TX_QUEUE_H */
