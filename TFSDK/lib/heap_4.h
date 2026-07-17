#ifndef HEAP_4_H
#define HEAP_4_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef portBYTE_ALIGNMENT
#define portBYTE_ALIGNMENT            8U
#endif

#ifndef configHEAP_POOL_MAX
#define configHEAP_POOL_MAX           4U
#endif

#if ( ( configHEAP_POOL_MAX < 1U ) || ( configHEAP_POOL_MAX > 127U ) )
#error configHEAP_POOL_MAX must be in the range 1..127
#endif

typedef int8_t HeapHandle_t;

#define portHEAP_INVALID_HANDLE       ((HeapHandle_t)-1)

HeapHandle_t vPortHeapInit(uint8_t *pucHeap, size_t xHeapSize);
void *pvPortMalloc(HeapHandle_t xHeap, size_t xWantedSize);
void vPortFree(HeapHandle_t xHeap, void *pv);
size_t xPortGetFreeHeapSize(HeapHandle_t xHeap);
size_t xPortGetMinimumEverFreeHeapSize(HeapHandle_t xHeap);
void vPortInitialiseBlocks(HeapHandle_t xHeap);

#ifdef __cplusplus
}
#endif

#endif /* HEAP_4_H */
