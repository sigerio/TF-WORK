#ifndef COM_LINK_H
#define COM_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "gd32f10x_misc.h"
#include "ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ─────────────────────────────────────────────────────────────────────
 * com_link UART DMA 字节链路层
 * ─────────────────────────────────────────────────────────────────────
 *
 *  对 RS485(Modbus) 与 IEC104 等串口链路统一 DMA 字节收发：
 *    接收: USART RX → DMA(rx_dma_buf) → IDLE 中断 → 压入模块 FIFO(ringbuf)
 *    发送: 上层填 tx_dma_buf → 启动 DMA → USART TX，busy 标志防并发
 *    方向: 若该链路带 485 方向脚，TX 前拉高(发送)、TC 完成后拉回(接收)；
 *          无方向脚(如主板 IEC104 链路)自动跳过。
 *
 *  不负责协议帧、发送队列、解析或重试；这些均由各协议模块拥有。
 *
 *  资源归属（注册式）：
 *    com_link 不分配任何内存。各模块自行 static 分配 DMA 缓冲、
 *    ringbuf_t FIFO 及 com_link_t 句柄，组装 com_link_resource_t 后
 *    调 com_link_open() 注册。com_link 只保存指针，按指针操作模块内存。
 *
 *  使用方式（谁用谁初始化）：
 *    1. 模块内 static 分配 rx_dma_buf / rx_fifo_buf / ringbuf_t / tx_dma_buf
 *    2. ringbuf_init() 初始化模块自有 FIFO
 *    3. 填 com_link_resource_t（usart 指向 board_usart_configs 条目）
 *    4. 模块自己的 init 里用官方库函数完成硬件初始化（GPIO/USART/DMA/NVIC），
 *       NVIC 可用 com_link_usart_irqn() 查 IRQn
 *    5. com_link_open(&link, &res) 绑定运行时资源并启动接收链路
 *    6. 中断里调 com_link_irq()/com_link_rx_dma_irq()，任务里调收发 API
 * ─────────────────────────────────────────────────────────────────────
 */

/**
 * @brief 链路资源配置表（模块注册时提供，纯指针，不含实际存储）
 *        usart 指向 board 层的 USART 配置（含 io/dma/方向脚 id）；
 *        其余成员指向模块自行分配的缓冲区与 FIFO。
 */
typedef struct {
    const board_usart_config_t *usart;   /**< USART+IO+DMA+方向脚配置（board 层） */
    uint8_t  *rx_dma_buf;                /**< RX DMA 一级缓冲（模块分配）          */
    uint16_t  rx_dma_size;               /**< RX DMA 缓冲大小（字节）              */
    ringbuf_t *rx_fifo;                  /**< RX 二级 FIFO（模块分配并初始化）     */
    uint8_t  *tx_dma_buf;                /**< TX DMA 缓冲（模块分配）              */
    uint16_t  tx_dma_size;               /**< TX DMA 缓冲大小（字节）              */
} com_link_resource_t;

/**
 * @brief 链路句柄（模块持有），保存注册的资源指针与运行时状态
 */
typedef struct {
    const com_link_resource_t *res;      /**< 注册的资源配置                      */
    volatile bool tx_busy;               /**< 发送忙标志：DMA 传输中不可发新帧     */
    bool          initialized;           /**< com_link_open 是否成功              */
} com_link_t;

/* ── 注册 / 注销 ── */
bool com_link_open(com_link_t *link, const com_link_resource_t *res);
void com_link_reset(com_link_t *link);

/* ── 外设基址 → IRQn 查表工具（供使用方使能 NVIC） ── */
IRQn_Type com_link_usart_irqn(uint32_t usart_periph);

/* ── 发送：调用方在 !busy 时借用 TX DMA 缓冲并提交实际长度 ── */
uint8_t *com_link_tx_buffer(com_link_t *link);
bool     com_link_tx_busy(const com_link_t *link);
bool     com_link_tx_start(com_link_t *link, uint16_t len);
void     com_link_tx_service(com_link_t *link);

/* ── 接收：这些操作封装 RX ISR 与任务之间的 FIFO 临界区 ── */
uint16_t com_link_rx_available(com_link_t *link);
uint16_t com_link_rx_read(com_link_t *link, uint8_t *buf, uint16_t len);
uint16_t com_link_rx_peek(com_link_t *link, uint8_t *buf, uint16_t len);
uint16_t com_link_rx_drop(com_link_t *link, uint16_t len);
void     com_link_rx_clear(com_link_t *link);

/* ── 中断入口（由芯片 IRQHandler 调用） ── */
void com_link_irq(com_link_t *link);
void com_link_rx_dma_irq(com_link_t *link);

#ifdef __cplusplus
}
#endif

#endif /* COM_LINK_H */
