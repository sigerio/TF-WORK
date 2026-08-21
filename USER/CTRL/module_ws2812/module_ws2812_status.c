#include "module_ws2812_status.h"

#include "board.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "module_ws2812_effect.h"

/*
 * 单条装饰灯带（SPI0/PA7），按机库业务状态驱动。
 * 灯带使能脚 PC5（拉低触发）由本模块统一管理：
 * 有任何灯效需求时拉低点亮，全部空闲时拉高熄灭。
 */

static strip_flash_state_t door_flash_state = {0};
static strip_run_state_t platform_run_state = {0};
static uint8_t strip_enable_inited;

/* 红色警戒灯：门动作时，前 8 颗 200ms 亮 / 200ms 灭 */
static void dev_door_actioning(uint16_t cyctime)
{
    for (int i = 0; i < 4; i++) {
        ws2812_flash_with(MODULE_WS2812_STRIP_0, (i << 1), (i << 1) + 1,
                          WS2812_RGB(252, 3, 3), WS2812_RGB(0, 0, 0),
                          -1, 90, &door_flash_state);
    }
    ws2812_flash_update(4, 4, -1, &door_flash_state);
}

/* 平台动作：尾部跑马灯 */
static void dev_platform_actioning(uint16_t cyctime)
{
    ws2812_run_with(MODULE_WS2812_STRIP_0, WS2812_RGB(255, 255, 255), 2,
                    &platform_run_state);
}

/* 平台导航：四角白色定位 + 绿色边界 */
static void dev_platform_navigate(uint16_t cyctime)
{
    for (int i = 0; i < 4; i++) {
        ws2812_flash_with(MODULE_WS2812_STRIP_0, (i << 2), (i << 2) + 1,
                          WS2812_RGB(255, 255, 255), WS2812_RGB(0, 0, 0),
                          2, 90, &door_flash_state);
    }
    ws2812_flash_update(2, 2, 2, &door_flash_state);
    module_ws2812_effect_at(MODULE_WS2812_STRIP_0, 8, 9,
                            WS2812_RGB(0, 252, 93), 90);
}

static door_running_in_e door_running;
static platform_running_in_e platform_running;

static void strip_enable_write(bool on)
{
#if MODULE_WS2812_STRIP_ENABLE_PIN
    if (!strip_enable_inited) {
        rcu_periph_clock_enable(RCU_GPIOC);
        gpio_init(BOARD_PIN_PORT(BOARD_PIN_LED_STRIP_ENABLE),
                  board_gpio_configs[BOARD_PIN_LED_STRIP_ENABLE].mode,
                  board_gpio_configs[BOARD_PIN_LED_STRIP_ENABLE].speed,
                  BOARD_PIN_PIN(BOARD_PIN_LED_STRIP_ENABLE));
        strip_enable_inited = 1U;
    }

    /* PC5 拉低触发：点亮拉低，熄灭拉高 */
    gpio_bit_write(BOARD_PIN_PORT(BOARD_PIN_LED_STRIP_ENABLE),
                   BOARD_PIN_PIN(BOARD_PIN_LED_STRIP_ENABLE),
                   on ? RESET : SET);
#else
    (void)on;
#endif
}

void set_door_running_in(door_running_in_e in)
{
    door_running = in;
}

void set_platform_running_in(platform_running_in_e in)
{
    platform_running = in;
}

void module_ws2812_status_update(uint16_t cyctime)
{
    bool any_effect = false;

    module_ws2812_clear_work_frame();

    switch (door_running) {
    case E_DOOR_RUN_IN_ACTION:
        dev_door_actioning(cyctime);
        any_effect = true;
        break;
    default:
        break;
    }

    switch (platform_running) {
    case E_PLATFORM_RUN_IN_ACTION:
        dev_platform_actioning(cyctime);
        any_effect = true;
        break;
    case E_PLATFORM_NAVIGATE:
        dev_platform_navigate(cyctime);
        any_effect = true;
        break;
    default:
        break;
    }

    strip_enable_write(any_effect);
    module_ws2812_copy_work_frame_to_dma(MODULE_WS2812_STRIP_0);
    module_ws2812_send_frame_dma(MODULE_WS2812_STRIP_0);
}
