/**
 * @file    tf_debug.h
 * @brief   调试打印宏 — TF_DEBUG 通过编译宏控制，TF_PRINTF 始终有效
 *
 * ============ 宏说明 ============
 *
 * TF_PRINTF(fmt, ...)    — 基础打印，始终有效
 * TF_DEBUG(tag, fmt, ...) — 调试追踪打印，由 TF_DEBUG_ENABLE 控制，
 *                           自动为标签添加颜色：
 *
 *     [TIMER ] [PORT  ] [HAL   ] [DEV   ] [LED   ] [APP   ]
 *
 * ============ 使用方式 ============
 *
 * make sim       # TF_PRINTF 输出 ✔   TF_DEBUG 静默 ✘
 * make sim-dbg   # TF_PRINTF 输出 ✔   TF_DEBUG 输出 ✔（带颜色）
 *
 * ============ 适用场景 ============
 *
 * TF_PRINTF — 固定输出（demo 信息、测试结果、错误提示）
 * TF_DEBUG — 调试追踪（调用链、参数值、状态变化），临时开启/关闭
 *
 *
 * ============ 标签 → 颜色映射 ============
 *
 *   TIMER → 红     PORT → 绿     HAL → 青
 *   DEV   → 黄     LED  → 蓝     APP → 紫
 *   TEST  → 白
 */
#ifndef TF_DEBUG_H
#define TF_DEBUG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int tf_debug_printf(const char *fmt, ...);


/* ==================================================================
 * TF_DEBUG — 受 TF_DEBUG_ENABLE 控制
 *
 * 用于调试追踪，定义宏时输出，未定义时编译为空。
 * 自动添加标签颜色 + __func__。
 * ================================================================*/
// #define TF_DEBUG_ENABLE


/* ==================================================================

 * ================================================================*/
#ifdef TF_DEBUG_ENABLE
#define TF_PRINTF(fmt, ...) \
    tf_debug_printf(fmt, ##__VA_ARGS__)
#else
    #define TF_PRINTF(fmt,  ...)  ((void)0)
#endif
/* ==================================================================
 * ANSI 颜色定义（仅 TF_DEBUG 内部使用）
 * ================================================================*/
#define TF_COLOR_RED     "\033[31m"
#define TF_COLOR_GREEN   "\033[32m"
#define TF_COLOR_YELLOW  "\033[33m"
#define TF_COLOR_BLUE    "\033[34m"
#define TF_COLOR_MAGENTA "\033[35m"
#define TF_COLOR_CYAN    "\033[36m"
#define TF_COLOR_WHITE   "\033[37m"
#define TF_COLOR_RESET   "\033[0m"

/* ==================================================================
 * 标签 → 颜色映射函数
 *
 * 按首个字符匹配，仅 T 开头的标签需额外判断第二位。
 * 编译器可内联为 switch + 条件跳转，零开销。
 * ================================================================*/
static inline const char* tf_tag_color(const char *tag)
{
    switch (tag[0]) {
        case 'P': return TF_COLOR_GREEN;    /* PORT  */
        case 'D': return TF_COLOR_YELLOW;   /* DEV   */
        case 'H': return TF_COLOR_CYAN;     /* HAL   */
        case 'L': return TF_COLOR_BLUE;     /* LED   */
        case 'A': return TF_COLOR_MAGENTA;  /* APP   */
        case 'E':
            if (tag[1] == 'V') return TF_COLOR_WHITE;   /* EVENT */
            return TF_COLOR_GREEN;                       /* EXTI  */
        case 'O': return TF_COLOR_RED;      /* OS    */
        case 'T':
            if (tag[1] == 'I') return TF_COLOR_RED;     /* TIMER */
            return TF_COLOR_WHITE;                       /* TEST  */
        default:  return TF_COLOR_RESET;
    }
}


#ifdef TF_DEBUG_ENABLE

    #define TF_DEBUG(tag, fmt, ...) \
        tf_debug_printf(TF_COLOR_RESET "%s[%-6s]" TF_COLOR_RESET " %s: " fmt, \
                        tf_tag_color(tag), tag, __func__, ##__VA_ARGS__)

#else
    #define TF_DEBUG(tag, fmt, ...)  ((void)0)
#endif /* TF_DEBUG_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* TF_DEBUG_H */
