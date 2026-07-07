/**
 * @file    tf_os.c
 * @brief   OS 层实现 —— 主循环、初始化
 *
 * 时间基准由 tf_delay.c 统一管理，HAL 只提供底层 SysTick 配置，
 * OS 层通过 tf_os_tick_get() 获取，不涉及任何硬件寄存器操作。
 *
 * ============ 主循环流程 ============
 *
 *   while (1):
 *     Timer → Event → Pool
 *
 * 执行顺序固定，不可调整。
 */

#include "tf_os.h"
 
#include "../tf_debug.h"

/*
 * ┌─────────────────────────────────────────────────────────────┐
 * │  tf_os_tick_get 声明于 tf_os.h，实现在 tf_delay.c           │
 * │                                                              │
 * │  OS 层不定义此函数——与 tf_delay 共享同一时间基准。           │
 * │  HAL 只提供 SysTick 底层配置函数。                            │
 * └─────────────────────────────────────────────────────────────┘
 */

/*
 * ┌─────────────────────────────────────────────────────────────┐
 * │  子模块内部函数声明                                           │
 * │  （实现在各子模块 .c 文件中）                                  │
 * └─────────────────────────────────────────────────────────────┘
 */

/* tf_os_timer.c */
void tf_os_timer_init(void);
void tf_os_timer_process(uint32_t now);

/* tf_os_event.c */
void tf_os_event_init(void);
void tf_os_event_process(void);

/* tf_os_pool.c */
void tf_os_pool_init(void);
int8_t tf_os_pool_process(void);

/* ==================================================================
 * 对外接口实现
 * ================================================================*/

void tf_os_init(void)
{
    TF_DEBUG("OS", "OS init...\n");

    tf_os_timer_init();
    tf_os_event_init();
    tf_os_pool_init();

    TF_PRINTF("[OS    ] OS initialized\n");
}

void tf_os_main_loop(void)
{
    TF_PRINTF("[OS    ] Entering main loop...\n\n");

    while (1) {
        uint32_t now = tf_os_tick_get();

        /* Step 1: Timer 任务 */
        tf_os_timer_process(now);

        /* Step 2: Event 任务 */
        tf_os_event_process();

        /* Step 3: Pool 任务（仅队头一个） */
        tf_os_pool_process();
    }
}
