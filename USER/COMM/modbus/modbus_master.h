#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <stdbool.h>
#include <stdint.h>

#include "COMM/modbus/modbus_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MODULE_MODBUS_ENABLE_DIAG
/**
 * @brief Modbus 主站全局诊断信息
 *        汇总主站任务层和发送队列层的运行统计数据，
 *        可用于串口输出调试、性能分析或远程监控。
 */
typedef struct {
    /* ── 主站任务层统计 ── */
    uint32_t task_tick;            /**< master_task 累计执行次数（tick）       */
    uint32_t request_count;         /**< 累计发送的请求帧数                    */
    uint32_t response_count;        /**< 累计收到的有效响应帧数                */
    uint32_t timeout_count;         /**< 等待响应超时次数                      */
    uint32_t crc_error_count;       /**< 接收帧 CRC 校验失败次数               */
    uint32_t frame_error_count;     /**< 帧格式错误（地址/功能码/长度不匹配）  */
    uint32_t unmatched_count;       /**< 接收帧地址/功能码不匹配被丢弃次数     */
    uint32_t parse_ok_count;        /**< 响应解析成功次数                      */
    uint32_t parse_error_count;     /**< 响应解析失败（非 CRC）次数            */

    /* ── 发送队列层统计 ── */
    uint16_t txq_write_pos;        /**< 发送队列帧缓冲区当前写入位置索引      */
    uint32_t enqueue_count;         /**< 累计入队次数                          */
    uint32_t dequeue_count;         /**< 累计出队次数                          */
    uint32_t txq_overwrite_drop_count; /**< 发送队列覆盖旧帧丢弃计数        */
    uint32_t txq_full_drop_count;      /**< 发送队列满丢弃计数              */
    uint32_t txq_invalid_count;        /**< 发送队列无效请求计数            */
} modbus_diag_t;
#endif

/* 初始化两条 RS-485 总线、接收 FIFO 和 Modbus 发送队列。 */
bool modbus_master_init(void);
/* 快周期收发任务：气象总线空闲时先发 B4 优先槽，再取普通请求；接收响应并驱动超时处理。 */
void modbus_master_task(void);
/* 主循环内调用：覆盖 B4 最新待发状态；不占普通队列、不操作正在发送的 DMA 缓冲。 */
void modbus_master_queue_weather_fault(uint32_t fault_value);
/* 轮询补发最新状态，与变位发送共用一个优先槽。 */
void modbus_master_repeat_weather_fault(void);
/* 调试模拟位仅叠加到 B4 报文；0 退出模拟并立即请求发送真实状态。 */
void modbus_master_set_weather_fault_debug(uint32_t fault_mask);
#if MODULE_MODBUS_ENABLE_DIAG
void modbus_get_diag(modbus_diag_t *diag);
void modbus_clear_diag(void);
#endif

/* 气象总线和空调总线的 USART 中断转发入口。 */
void modbus_weather_irq(void);
void modbus_ac_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MASTER_H */
