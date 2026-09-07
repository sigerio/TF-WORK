
#include "FreeRTOS.h"
#include "task.h"
#include "gd32f10x_misc.h"
#include "task_scheduler.h"

int main(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    task_scheduler();
}
