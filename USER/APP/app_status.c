#include "APP/app_status.h"

os_state_u os_state = {0};

void set_signal(OS_RUN_SIGNAL_EN signal)
{
    if ((unsigned int)signal < (unsigned int)OS_RUN_SIGNAL_COUNT) {
        os_state.byte |= (uint16_t)(1U << (unsigned int)signal);
    }
}

void cancel_signal(OS_RUN_SIGNAL_EN signal)
{
    if ((unsigned int)signal < (unsigned int)OS_RUN_SIGNAL_COUNT) {
        os_state.byte &= (uint16_t)~(1U << (unsigned int)signal);
    }
}

void clean_signal(void)
{
    os_state.byte = 0U;
}

bool is_signal_set(OS_RUN_SIGNAL_EN signal)
{
    if ((unsigned int)signal >= (unsigned int)OS_RUN_SIGNAL_COUNT) {
        return false;
    }

    return (os_state.byte & (uint16_t)(1U << (unsigned int)signal)) != 0U;
}

os_state_u get_signal_state(void)
{
    return os_state;
}
