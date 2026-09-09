/**
 * @file    debug_log.h
 * @brief   调试打印宏 — DEBUG 通过编译宏控制，PRINTF 始终有效
 *
 * ============ 宏说明 ============
 *
 * PRINTF(fmt, ...)    — 基础打印，始终有效
 * DEBUG(tag, fmt, ...) — 调试追踪打印，由 DEBUG_ENABLE 控制，
 *                           自动为标签添加颜色：
 *
 *     [TIMER ] [PORT  ] [HAL   ] [DEV   ] [LED   ] [APP   ]
 *
 * ============ 使用方式 ============
 *
 * make sim       # PRINTF 输出 ✔   DEBUG 静默 ✘
 * make sim-dbg   # PRINTF 输出 ✔   DEBUG 输出 ✔（带颜色）
 *
 * ============ 适用场景 ============
 *
 * PRINTF — 固定输出（demo 信息、测试结果、错误提示）
 * DEBUG — 调试追踪（调用链、参数值、状态变化），临时开启/关闭
 *
 *
 * ============ 标签 → 颜色映射 ============
 *
 *   TIMER → 红     PORT → 绿     HAL → 青
 *   DEV   → 黄     LED  → 蓝     APP → 紫
 *   TEST  → 白
 */
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdio.h>

#include "BOARD/board.h"

#ifdef __cplusplus
extern "C" {
#endif

int debug_printf(const char *fmt, ...);


/* ==================================================================
 * DEBUG — 受 DEBUG_ENABLE 控制
 *
 * 用于调试追踪，定义宏时输出，未定义时编译为空。
 * 自动添加标签颜色 + __func__。
 * ================================================================*/
#if (BOARD_USART2_ROLE == BOARD_USART2_ROLE_DEBUG)
#define DEBUG_ENABLE
#endif
#if defined(DEBUG_ENABLE) && (BOARD_USART2_ROLE != BOARD_USART2_ROLE_DEBUG)
#error DEBUG_ENABLE requires BOARD_USART2_ROLE_DEBUG
#endif


/* ==================================================================

 * ================================================================*/
#ifdef DEBUG_ENABLE

typedef union
{
    struct
    {
        uint8_t ac: 1;
        uint8_t weth: 1;
        uint8_t iec: 1;
        uint8_t wse: 3; /* 0=业务灯效，1~6=ws2812_debug_effect_e */
        uint8_t dor: 1;
        uint8_t hei: 1;
    }bits;
    uint8_t byte;

}debug_ctrl_u;
extern debug_ctrl_u debug_ctrl;


#define PRINTF(fmt, ...) \
    debug_printf(fmt, ##__VA_ARGS__)
#else
    #define PRINTF(fmt,  ...)  ((void)0)
#endif
/* ==================================================================
 * ANSI 颜色定义（仅 DEBUG 内部使用）
 * ================================================================*/
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_RESET   "\033[0m"

/* ==================================================================
 * 标签 → 颜色映射函数
 *
 * 按首个字符匹配，仅 T 开头的标签需额外判断第二位。
 * 编译器可内联为 switch + 条件跳转，零开销。
 * ================================================================*/
static inline const char* tag_color(const char *tag)
{
    switch (tag[0]) {
        case 'P': return COLOR_GREEN;    /* PORT  */
        case 'D': return COLOR_YELLOW;   /* DEV   */
        case 'H': return COLOR_CYAN;     /* HAL   */
        case 'L': return COLOR_BLUE;     /* LED   */
        case 'A': return COLOR_MAGENTA;  /* APP   */
        case 'E':
            if (tag[1] == 'V') return COLOR_WHITE;   /* EVENT */
            return COLOR_GREEN;                       /* EXTI  */
        case 'O': return COLOR_RED;      /* OS    */
        case 'T':
            if (tag[1] == 'I') return COLOR_RED;     /* TIMER */
            return COLOR_WHITE;                       /* TEST  */
        default:  return COLOR_RESET;
    }
}


#ifdef DEBUG_ENABLE

    #define DEBUG(tag, fmt, ...) \
    debug_printf(COLOR_RESET "%s[%-6s]" COLOR_RESET " %s: " fmt, tag_color(tag), tag, __func__, ##__VA_ARGS__)

#else
    #define DEBUG(tag, fmt, ...)  ((void)0)
#endif /* DEBUG_ENABLE */


#ifdef __cplusplus
}
#endif

#endif /* DEBUG_LOG_H */
