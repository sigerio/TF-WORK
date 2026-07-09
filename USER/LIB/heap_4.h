#ifndef HEAP_4_H
#define HEAP_4_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE         4096U
#endif

#ifndef portBYTE_ALIGNMENT
#define portBYTE_ALIGNMENT            8U
#endif

#ifndef configHEAP_POOL_MAX
#define configHEAP_POOL_MAX           4U
#endif

#ifndef configDEFAULT_HEAP_INDEX
#define configDEFAULT_HEAP_INDEX      0U
#endif


void *pvPortMalloc(size_t xWantedSize);
void vPortFree(void *pv);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
void vPortInitialiseBlocks(void);

#ifdef __cplusplus
}
#endif

#endif /* HEAP_4_H */
