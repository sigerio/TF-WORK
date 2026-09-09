#ifndef CIRCULAR_INDEX_H
#define CIRCULAR_INDEX_H

#include <stdint.h>

/* 返回 idx 的循环递增结果；到达 max 后回到 0。 */
int32_t index_increase_loop(int32_t idx, int32_t max);

#endif
