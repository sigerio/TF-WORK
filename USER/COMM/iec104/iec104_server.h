#ifndef IEC104_SERVER_H
#define IEC104_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 通信任务周期：TX DMA 收尾、ringbuf 收帧解析、出队发送、协议定时器。 */
#ifndef IEC104_SERVER_COMM_PERIOD_MS
#define IEC104_SERVER_COMM_PERIOD_MS 10U
#endif

/* 轮询任务周期：采集数据、变位遥信组帧入队（主动上送）。 */
#ifndef IEC104_SERVER_POLL_PERIOD_MS
#define IEC104_SERVER_POLL_PERIOD_MS 100U
#endif

void iec104_server_init(void);

/* 快周期：数据收发与协议维护。 */
void iec104_server_comm_task(void);

/* 慢周期：主动组帧入队。 */
void iec104_server_poll_task(void);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_SERVER_H */
