#include "COMMON/circular_index.h"


int32_t index_increase_loop(int32_t idx, int32_t max)
{

    idx++;
    if (idx >= max) {
        idx = 0U;
    }

    return idx;
}

