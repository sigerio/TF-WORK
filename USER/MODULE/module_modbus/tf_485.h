#ifndef TF_485_H
#define TF_485_H

#include "tf_485_master.h"
#include "ringbuf.h"
#include "tf_drv_dma.h"
#include "tf_drv_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ─────────────────────────────────────────────────────────────────────
 * RS485 驱动层结构体层级关系
 * ─────────────────────────────────────────────────────────────────────
 *
 *  tf_485_t              ← 485 驱动上下文
 *   ├── hw: tf_485_hw_t  ← 硬件资源（UART + DMA 句柄）
 *   ├── rx: tf_485_rx_t  ← 接收通道（DMA 缓冲 + 环形 FIFO）
 *   └── tx: tf_485_tx_t  ← 发送通道（DMA 缓冲 + 忙标志）
 *
 *  数据流（接收）：
 *      USART RX → DMA(dma_buf) → IDLE 中断 → fifo(环形 FIFO) → 上层读取
 *
 *  数据流（发送）：
 *      上层写入 → dma_buf → DMA → USART TX
 *      （busy 标志在传输期间置位，防止并发）
 *
 *  物理配置：USART2 + DMA1（CH2=RX, CH4=TX），
 *            使用自动换向 TTL→RS485 电路，无需 GPIO 控制 DE/RE。
 * ─────────────────────────────────────────────────────────────────────
 */

/**
 * @brief RS485 硬件资源聚合
 *        将 USART 和 DMA 的驱动句柄捆绑为一个硬件单元，
 *        供驱动层统一管理收发过程。
 */
typedef struct {
    tf_drv_uart_handle_t uart;  /**< USART 驱动句柄（当前固定为 USART2） */
    tf_drv_dma_handle_t dma;    /**< DMA 驱动句柄（当前固定为 DMA1）   */
} tf_485_hw_t;

/**
 * @brief RS485 接收通道
 *        DMA 从 USART 数据寄存器连续搬运到 dma_buf；
 *        当 USART IDLE 中断（一帧结束）触发时，将 dma_buf 中本次收到的
 *        有效数据推入 fifo（环形 FIFO），然后重启 DMA 开始下一帧接收。
 *        上层通过 fifo 以 FIFO 方式读取接收数据。
 *
 * @note  dma_buf 与 fifo 是两级缓冲：
 *        - dma_buf 负责 DMA 连续搬运，帧边界由 IDLE 中断划分；
 *        - fifo 负责缓存尚未被上层消费的历史数据，防止 CPU 来不及处理。
 */
typedef struct {
    uint8_t dma_buf[MODULE_MODBUS_RX_BUF_SIZE];       /**< DMA 接收缓冲区，USART DR → DMA 直接写入          */
    uint8_t fifo_buf[MODULE_MODBUS_RINGBUF_SIZE];     /**< 环形 FIFO 的底层存储区（由 fifo 管理）           */
    ringbuf_t fifo;                                   /**< 环形 FIFO 控制块，提供线程安全的字节流 FIFO 存取 */
} tf_485_rx_t;

/**
 * @brief RS485 发送通道
 *        上层将待发帧填入 dma_buf，然后启动 DMA 传输；
 *        busy 标志在 DMA 传输期间置位，防止上层重复提交发送请求。
 *        DMA 传输完成 + USART TC（发送完成）后由 tf_485_tx_service() 清除 busy。
 */
typedef struct {
    uint8_t dma_buf[MODULE_MODBUS_TX_BUF_SIZE]; /**< DMA 发送缓冲区，数据从此经 DMA 写入 USART DR   */
    volatile bool busy;                         /**< 发送忙标志：true = DMA 传输中，不可发送新帧     */
} tf_485_tx_t;

/**
 * @brief RS485 驱动上下文
 *        组合硬件资源、接收通道、发送通道三个子模块，
 *        是 RS485 驱动层的核心句柄，所有 485 操作函数均以本结构指针为第一参数。
 *
 * @note  在 tf_485_bus_t 中以 port 成员嵌入使用：
 *            tf_485_bus_t → port (tf_485_t) → hw / rx / tx
 */
typedef struct {
    tf_485_hw_t hw; /**< 硬件资源：UART + DMA 驱动句柄                */
    tf_485_rx_t rx; /**< 接收通道：DMA 缓冲区 + 环形 FIFO             */
    tf_485_tx_t tx; /**< 发送通道：DMA 缓冲区 + 忙标志                 */
} tf_485_t;

bool tf_485_init(tf_485_t *ctx);
void tf_485_reset(tf_485_t *ctx);

bool tf_485_send(tf_485_t *ctx, const uint8_t *data, uint32_t len);
bool tf_485_tx_start(tf_485_t *ctx, uint32_t len);
void tf_485_tx_service(tf_485_t *ctx);
bool tf_485_tx_busy(const tf_485_t *ctx);
uint8_t *tf_485_tx_buffer(tf_485_t *ctx);

uint32_t tf_485_rx_available(tf_485_t *ctx);
uint32_t tf_485_rx_read(tf_485_t *ctx, uint8_t *buf, uint32_t buf_size);
uint32_t tf_485_rx_peek(tf_485_t *ctx, uint8_t *buf, uint32_t len);
uint32_t tf_485_rx_drop(tf_485_t *ctx, uint32_t len);
uint32_t tf_485_rx_overflow_count(tf_485_t *ctx);
void tf_485_rx_clear(tf_485_t *ctx);

void tf_485_irq(tf_485_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TF_485_H */
