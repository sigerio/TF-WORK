/**
 * @file    os.c
 * @brief   OS 层实现 —— 主循环、初始化
 *
 * 时间基准由 board_time.c 统一管理，HAL 只提供底层 SysTick 配置，
 * OS 层通过 os_tick_get() 获取，不涉及任何硬件寄存器操作。
 *
 * ============ 主循环流程 ============
 *
 *   while (1):
 *     Timer → Event → Pool
 *
 * 执行顺序固定，不可调整。
 */

#include "OS/os.h"
#include "OS/os_internal.h"

#include "DEBUG/debug_log.h"

/*
 * ┌─────────────────────────────────────────────────────────────┐
 * │  os_tick_get 声明于 os.h，实现在 board_time.c           │
 * │                                                              │
 * │  OS 层不定义此函数——与 delay 共享同一时间基准。           │
 * │  HAL 只提供 SysTick 底层配置函数。                            │
 * └─────────────────────────────────────────────────────────────┘
 */

/* ==================================================================
 * 对外接口实现
 * ================================================================*/

void os_init(void)
{
    DEBUG("OS", "OS init...\n");

    os_timer_init();
    os_event_init();
    os_pool_init();

    PRINTF("[OS    ] OS initialized\n");
}

void os_main_loop(void)
{
    PRINTF("[OS    ] Entering main loop...\n\n");

    while (1) {
        uint32_t now = os_tick_get();

        /* Step 1: Timer 任务 */
        os_timer_process(now);

        /* Step 2: Event 任务 */
        os_event_process();

        /* Step 3: Pool 任务（仅队头一个） */
        os_pool_process();
    }
}
