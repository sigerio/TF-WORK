#ifndef MODULE_MODBUS_H
#define MODULE_MODBUS_H

#include <stdint.h>

#include "tf_board_config.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 缓冲大小宏定义
 * ─────────────────────────────────────────────────────────────────────
 *  所有缓冲区大小均可通过外部 -D 选项覆盖。
 *  RX/TX 的 DMA 缓冲区与环形 FIFO 独立，层次不同：
 *
 *    DMA 缓冲区 (dma_buf)       ← 一级缓冲，直接收 DMA 数据，帧边界由 IDLE 划分
 *        ↓ IDLE 中断推入
 *    环形 FIFO (ring)           ← 二级缓冲，暂存未处理帧，防 CPU 来不及消费
 *        ↓ 上层读取
 *    应用层 (master task)       ← 解析处理
 *
 *  典型大小：RX_BUF=256, RINGBUF=512 → 环形 FIFO 为 DMA 缓冲的 2 倍，
 *  应对连续多帧堆积的场景。
 * ─────────────────────────────────────────────────────────────────────
 */

#ifndef MODULE_MODBUS_RX_BUF_SIZE
#define MODULE_MODBUS_RX_BUF_SIZE 256U    /**< DMA 接收缓冲区大小（字节）         */
#endif

#ifndef MODULE_MODBUS_RINGBUF_SIZE
#define MODULE_MODBUS_RINGBUF_SIZE 512U   /**< 接收环形 FIFO 大小（字节）          */
#endif

#ifndef MODULE_MODBUS_TX_BUF_SIZE
#define MODULE_MODBUS_TX_BUF_SIZE 256U    /**< DMA 发送缓冲区大小（字节）         */
#endif

#ifndef MODULE_MODBUS_SEND_BUF_SIZE
#define MODULE_MODBUS_SEND_BUF_SIZE 256U  /**< 发送队列环形缓冲区大小（字节）     */
#endif

#ifndef MODULE_MODBUS_SEND_QUEUE_MAX
#define MODULE_MODBUS_SEND_QUEUE_MAX 16U  /**< 发送队列最大条目数                  */
#endif

#ifndef MODULE_MODBUS_MASTER_PERIOD_MS
#define MODULE_MODBUS_MASTER_PERIOD_MS 10U   /**< master_task 轮询周期（毫秒）     */
#endif

#ifndef MODULE_MODBUS_QUEUE_FILL_PERIOD_MS
#define MODULE_MODBUS_QUEUE_FILL_PERIOD_MS 100U /**< queue_fill_task 填充周期（毫秒）*/
#endif

#ifndef MODULE_MODBUS_ENABLE_DIAG
#define MODULE_MODBUS_ENABLE_DIAG 0U  /**< 诊断功能开关：1=启用，0=禁用           */
#endif

/** @brief 支持的 Modbus 从机设备类型 */
typedef enum {
    MODULE_MODBUS_DEV_XM7828_TH = 0,    /**< XM7828 温湿度传感器                  */
    MODULE_MODBUS_DEV_XM8189_WIND,       /**< XM8189 风速风向传感器                */
    MODULE_MODBUS_DEV_XM8537_WATER,      /**< XM8537 水浸传感器                   */
    MODULE_MODBUS_DEV_XM9505_RAIN,       /**< XM9505 雨量传感器                   */
    MODULE_MODBUS_DEV_AC,                /**< 空调设备                            */
    MODULE_MODBUS_DEV_CUSTOM,            /**< 自定义设备（使用通用解析）           */
} module_modbus_device_type_t;

/** @brief 轮询操作的功能类型 */
typedef enum {
    MODULE_MODBUS_POLL_READ_REGS = 0, /**< 读保持寄存器（Modbus 功能码 0x03） */
    MODULE_MODBUS_POLL_READ_BITS,     /**< 读线圈/输入位（Modbus 功能码 0x01/0x02） */
} module_modbus_poll_function_t;

#if MODULE_MODBUS_ENABLE_DIAG
/**
 * @brief Modbus 主站全局诊断信息
 *        汇总主站任务层和发送队列层的运行统计数据，
 *        可用于串口输出调试、性能分析或远程监控。
 *
 * @note  tick_count 在每次 master_task 调用时递增，
 *        其余计数字段归零时机由 module_modbus_clear_diag() 控制。
 */
typedef struct {
    /* ── 主站任务层统计 ── */
    uint32_t tick_count;            /**< master_task 累计执行次数（tick）       */
    uint32_t request_count;         /**< 累计发送的请求帧数                    */
    uint32_t response_count;        /**< 累计收到的有效响应帧数                */
    uint32_t timeout_count;         /**< 等待响应超时次数                      */
    uint32_t crc_error_count;       /**< 接收帧 CRC 校验失败次数               */
    uint32_t frame_error_count;     /**< 帧格式错误（地址/功能码/长度不匹配）  */
    uint32_t unmatched_count;       /**< 接收帧地址/功能码不匹配被丢弃次数     */
    uint32_t parse_ok_count;        /**< 响应解析成功次数                      */
    uint32_t parse_error_count;     /**< 响应解析失败（非 CRC）次数            */

    /* ── 发送队列层统计 ── */
    uint16_t send_buf_write_index;  /**< 发送环形缓冲区当前写入位置索引        */
    uint32_t enqueue_count;         /**< 累计入队次数                          */
    uint32_t dequeue_count;         /**< 累计出队次数                          */
    uint32_t queue_overwrite_drop_count; /**< 入队覆盖旧数据丢弃计数           */
    uint32_t queue_full_drop_count;      /**< 队列满丢弃计数                   */
    uint32_t queue_invalid_count;   /**< 无效入队请求计数                      */
} module_modbus_diag_t;
#endif

bool module_modbus_init(void);
void module_modbus_master_task(void);
void module_modbus_queue_fill_task(void);
#if MODULE_MODBUS_ENABLE_DIAG
void module_modbus_get_diag(module_modbus_diag_t *diag);
void module_modbus_clear_diag(void);
#endif
void module_modbus_usart2_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_H */
