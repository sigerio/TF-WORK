#include "tf_usr_time_task.h"

#include "../../TFSDK/framework/os/tf_os.h"
#include "../MODULE/KEY/key.h"
#include "../MODULE/MOTO/module_moto_business.h"
#include "../MODULE/MOTO/module_moto_sync.h"
#include "../MODULE/iec104/iec104_server.h"
#include "../MODULE/module_modbus/tf_485_master.h"
#include "../MODULE/module_ws2812/module_ws2812_status.h"
#include "tf_usr_event_task.h"

#define TF_KEY_SCAN_PERIOD_MS 50U

static const uint32_t tf_key_scan_period_ms = TF_KEY_SCAN_PERIOD_MS;
static const uint32_t tf_ws2812_period_ms = MODULE_WS2812_PERIOD_MS;

static void tf_485_master_timer_task(void *arg)
{
    (void)arg;
    tf_485_master_task();
}

static void tf_485_query_timer_task(void *arg)
{
    (void)arg;
    tf_485_query_task();
}

static void iec104_server_comm_timer_task(void *arg)
{
    (void)arg;
    iec104_server_comm_task();
}

static void iec104_server_poll_timer_task(void *arg)
{
    (void)arg;
    iec104_server_poll_task();
}

static void tf_key_scan_timer_task(void *arg)
{
    const uint32_t *period_ms = (const uint32_t *)arg;
    uint32_t cycle_ms = (period_ms != 0) ? *period_ms : TF_KEY_SCAN_PERIOD_MS;
    tf_key_info_t key_info;

    tf_key_scan_task(cycle_ms);
    (void)tf_os_event_trigger(TF_USR_EVENT_EM_STOP, 0, 0U, 1U);

    key_info = tf_key_get_info();

    for (int i = 0; i < TF_KEY_ID_COUNT; i++) {
        if (i == TF_KEY_ID_EM_STOP) {
            continue;
        }

        if (key_info.key_event[i] != TF_KEY_EVENT_RESET) {
            (void)tf_os_event_trigger(TF_USR_EVENT_KEY_TRIG, 0, 0U, 1U);
            return;
        }
    }
}

static void module_moto_sync_update_timer_task(void *arg)
{
    (void)arg;
    module_moto_sync_update();
    module_moto_business_update();
}



static void module_ws2812_run_timer_task(void *arg)
{
    uint16_t* cyctime = (uint16_t*) arg;
    module_ws2812_status_update(*cyctime);
}

void tf_usr_time_task_init(void)
{
    tf_os_soft_timer_task(tf_485_master_timer_task,
                          0,
                          MODULE_MODBUS_MASTER_PERIOD_MS,
                          10U);
    tf_os_soft_timer_task(tf_485_query_timer_task,
                          0,
                          MODULE_MODBUS_QUEUE_FILL_PERIOD_MS,
                          20U);
    tf_os_soft_timer_task(iec104_server_comm_timer_task,
                          0,
                          IEC104_SERVER_COMM_PERIOD_MS,
                          10U);
    tf_os_soft_timer_task(iec104_server_poll_timer_task,
                          0,
                          IEC104_SERVER_POLL_PERIOD_MS,
                          15U);
    tf_os_soft_timer_task(tf_key_scan_timer_task,
                          (void *)&tf_key_scan_period_ms,
                          TF_KEY_SCAN_PERIOD_MS,
                          30U);
    tf_os_soft_timer_task(module_moto_sync_update_timer_task,
                          0,
                          MODULE_MOTO_SYNC_MONITOR_PERIOD_MS,
                          2U);
    tf_os_soft_timer_task(module_ws2812_run_timer_task,
                          (void*)&tf_ws2812_period_ms,
                          MODULE_WS2812_PERIOD_MS,
                          30U);
}
