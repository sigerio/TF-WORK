/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*
 * A sample implementation of pvPortMalloc() and vPortFree() that combines
 * (coalescences) adjacent memory blocks as they are freed, and in so doing
 * limits memory fragmentation.
 *
 * See heap_1.c, heap_2.c and heap_3.c for alternative implementations, and the
 * memory management pages of https://www.FreeRTOS.org for more information.
 */
#include <stdlib.h>
#include <string.h>

#include "COMMON/heap_4.h"

#ifndef portBYTE_ALIGNMENT_MASK
    #define portBYTE_ALIGNMENT_MASK             ( portBYTE_ALIGNMENT - 1U )
#endif

#ifndef portPOINTER_SIZE_TYPE
    #define portPOINTER_SIZE_TYPE               uintptr_t
#endif

#ifndef PRIVILEGED_DATA
    #define PRIVILEGED_DATA
#endif

#ifndef PRIVILEGED_FUNCTION
    #define PRIVILEGED_FUNCTION
#endif

#ifndef configASSERT
    #define configASSERT( x )                   ( ( void ) ( x ) )
#endif

#ifndef SIZE_MAX
    #define SIZE_MAX                            ( ( size_t ) -1 )
#endif

typedef struct xHEAP_STATS
{
    size_t xAvailableHeapSpaceInBytes;
    size_t xSizeOfLargestFreeBlockInBytes;
    size_t xSizeOfSmallestFreeBlockInBytes;
    size_t xNumberOfFreeBlocks;
    size_t xMinimumEverFreeBytesRemaining;
    size_t xNumberOfSuccessfulAllocations;
    size_t xNumberOfSuccessfulFrees;
} HeapStats_t;

#ifndef configHEAP_CLEAR_MEMORY_ON_FREE
    #define configHEAP_CLEAR_MEMORY_ON_FREE    0
#endif

/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE    ( ( size_t ) ( xHeapStructSize << 1 ) )

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE         ( ( size_t ) 8 )

/* Max value that fits in a size_t type. */
#define heapSIZE_MAX              ( ~( ( size_t ) 0 ) )

/* Check if multiplying a and b will result in overflow. */
#define heapMULTIPLY_WILL_OVERFLOW( a, b )     ( ( ( a ) > 0 ) && ( ( b ) > ( heapSIZE_MAX / ( a ) ) ) )

/* Check if adding a and b will result in overflow. */
#define heapADD_WILL_OVERFLOW( a, b )          ( ( a ) > ( heapSIZE_MAX - ( b ) ) )

/* Check if the subtraction operation ( a - b ) will result in underflow. */
#define heapSUBTRACT_WILL_UNDERFLOW( a, b )    ( ( a ) < ( b ) )

/* MSB of the xBlockSize member of an BlockLink_t structure is used to track
 * the allocation status of a block.  When MSB of the xBlockSize member of
 * an BlockLink_t structure is set then the block belongs to the application.
 * When the bit is free the block is still part of the free heap space. */
#define heapBLOCK_ALLOCATED_BITMASK    ( ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 ) )
#define heapBLOCK_SIZE_IS_VALID( xBlockSize )    ( ( ( xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) == 0 )
#define heapBLOCK_IS_ALLOCATED( pxBlock )        ( ( ( pxBlock->xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) != 0 )
#define heapALLOCATE_BLOCK( pxBlock )            ( ( pxBlock->xBlockSize ) |= heapBLOCK_ALLOCATED_BITMASK )
#define heapFREE_BLOCK( pxBlock )                ( ( pxBlock->xBlockSize ) &= ~heapBLOCK_ALLOCATED_BITMASK )

/*-----------------------------------------------------------*/

/* Define the linked list structure.  This is used to link free blocks in order
 * of their memory address. */
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK * pxNextFreeBlock; /**< The next free block in the list. */
    size_t xBlockSize;                     /**< The size of the free block. */
} BlockLink_t;

typedef struct A_HEAP_POOL
{
    uint8_t * pucHeap;
    size_t xHeapSize;
    BlockLink_t xStart;
    BlockLink_t * pxEnd;
    size_t xFreeBytesRemaining;
    size_t xMinimumEverFreeBytesRemaining;
    size_t xNumberOfSuccessfulAllocations;
    size_t xNumberOfSuccessfulFrees;
    uint8_t ucUsed;
} HeapPool_t;

#define heapPROTECT_BLOCK_POINTER( pxBlock )    ( pxBlock )

/* Assert that a heap block pointer is within the heap bounds. */
#define heapVALIDATE_BLOCK_POINTER( pxPool, pxBlock ) \
    configASSERT( ( ( uint8_t * ) ( pxBlock ) >= ( pxPool )->pucHeap ) && ( ( uint8_t * ) ( pxBlock ) < ( ( pxPool )->pucHeap + ( pxPool )->xHeapSize ) ) )

/*-----------------------------------------------------------*/

/*
 * Inserts a block of memory that is being freed into the correct position in
 * the list of free memory blocks.  The block being freed will be merged with
 * the block in front it and/or the block behind it if the memory blocks are
 * adjacent to each other.
 */
static void prvInsertBlockIntoFreeList( HeapPool_t * pxPool, BlockLink_t * pxBlockToInsert ) PRIVILEGED_FUNCTION;

/*
 * Called automatically to setup the required heap structures the first time
 * pvPortMalloc() is called.
 */
static void prvHeapInit( HeapPool_t * pxPool ) PRIVILEGED_FUNCTION;

static HeapPool_t * prvGetHeapPool( HeapHandle_t xHeap ) PRIVILEGED_FUNCTION;
static void * prvMallocFromPool( HeapPool_t * pxPool, size_t xWantedSize ) PRIVILEGED_FUNCTION;
static void prvFreeFromPool( HeapPool_t * pxPool, void * pv ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

/* The size of the structure placed at the beginning of each allocated memory
 * block must by correctly byte aligned. */
static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );

PRIVILEGED_DATA static HeapPool_t xHeapPools[ configHEAP_POOL_MAX ];

/*-----------------------------------------------------------*/

HeapHandle_t vPortHeapInit( uint8_t * pucHeap, size_t xHeapSize )
{
    HeapHandle_t xHeap;
    HeapPool_t * pxPool;

    if( ( pucHeap == NULL ) || ( xHeapSize <= ( xHeapStructSize << 1 ) ) )
    {
        return portHEAP_INVALID_HANDLE;
    }

    for( xHeap = ( HeapHandle_t ) 0; xHeap < ( HeapHandle_t ) configHEAP_POOL_MAX; xHeap++ )
    {
        if( xHeapPools[ xHeap ].ucUsed == 0U )
        {
            pxPool = &( xHeapPools[ xHeap ] );
            ( void ) memset( pxPool, 0, sizeof( *pxPool ) );
            pxPool->pucHeap = pucHeap;
            pxPool->xHeapSize = xHeapSize;
            pxPool->ucUsed = 1U;
            prvHeapInit( pxPool );
            break;
        }
    }

    if( xHeap >= ( HeapHandle_t ) configHEAP_POOL_MAX )
    {
        xHeap = portHEAP_INVALID_HANDLE;
    }

    return xHeap;
}
/*-----------------------------------------------------------*/

static HeapPool_t * prvGetHeapPool( HeapHandle_t xHeap )
{
    HeapPool_t * pxPool = NULL;

    if( ( xHeap >= ( HeapHandle_t ) 0 ) && ( xHeap < ( HeapHandle_t ) configHEAP_POOL_MAX ) )
    {
        if( xHeapPools[ xHeap ].ucUsed != 0U )
        {
            pxPool = &( xHeapPools[ xHeap ] );
        }
    }

    return pxPool;
}
/*-----------------------------------------------------------*/

void * pvPortMalloc( HeapHandle_t xHeap, size_t xWantedSize )
{
    HeapPool_t * pxPool = prvGetHeapPool( xHeap );

    configASSERT( pxPool != NULL );

    if( pxPool == NULL )
    {
        return NULL;
    }

    return prvMallocFromPool( pxPool, xWantedSize );
}
/*-----------------------------------------------------------*/

static void * prvMallocFromPool( HeapPool_t * pxPool, size_t xWantedSize )
{
    BlockLink_t * pxBlock;
    BlockLink_t * pxPreviousBlock;
    BlockLink_t * pxNewBlockLink;
    void * pvReturn = NULL;
    size_t xAdditionalRequiredSize;

    if( xWantedSize > 0 )
    {
        /* The wanted size must be increased so it can contain a BlockLink_t
         * structure in addition to the requested amount of bytes. */
        if( heapADD_WILL_OVERFLOW( xWantedSize, xHeapStructSize ) == 0 )
        {
            xWantedSize += xHeapStructSize;

            /* Ensure that blocks are always aligned to the required number
             * of bytes. */
            if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
            {
                /* Byte alignment required. */
                xAdditionalRequiredSize = portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK );

                if( heapADD_WILL_OVERFLOW( xWantedSize, xAdditionalRequiredSize ) == 0 )
                {
                    xWantedSize += xAdditionalRequiredSize;
                }
                else
                {
                    xWantedSize = 0;
                }
            }
        }
        else
        {
            xWantedSize = 0;
        }
    }

    /* If this is the first call to malloc then the heap will require
     * initialisation to setup the list of free blocks. */
    if( pxPool->pxEnd == NULL )
    {
        prvHeapInit( pxPool );
    }

    /* Check the block size we are trying to allocate is not so large that the
     * top bit is set.  The top bit of the block size member of the BlockLink_t
     * structure is used to determine who owns the block - the application or
     * the kernel, so it must be free. */
    if( heapBLOCK_SIZE_IS_VALID( xWantedSize ) != 0 )
    {
        if( ( xWantedSize > 0 ) && ( xWantedSize <= pxPool->xFreeBytesRemaining ) )
        {
            /* Traverse the list from the start (lowest address) block until
             * one of adequate size is found. */
            pxPreviousBlock = &( pxPool->xStart );
            pxBlock = heapPROTECT_BLOCK_POINTER( pxPool->xStart.pxNextFreeBlock );
            heapVALIDATE_BLOCK_POINTER( pxPool, pxBlock );

            while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != heapPROTECT_BLOCK_POINTER( NULL ) ) )
            {
                pxPreviousBlock = pxBlock;
                pxBlock = heapPROTECT_BLOCK_POINTER( pxBlock->pxNextFreeBlock );
                heapVALIDATE_BLOCK_POINTER( pxPool, pxBlock );
            }

            /* If the end marker was reached then a block of adequate size
             * was not found. */
            if( pxBlock != pxPool->pxEnd )
            {
                /* Return the memory space pointed to - jumping over the
                 * BlockLink_t structure at its start. */
                pvReturn = ( void * ) ( ( ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxPreviousBlock->pxNextFreeBlock ) ) + xHeapStructSize );
                heapVALIDATE_BLOCK_POINTER( pxPool, pvReturn );

                /* This block is being returned for use so must be taken out
                 * of the list of free blocks. */
                pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                /* If the block is larger than required it can be split into
                 * two. */
                configASSERT( heapSUBTRACT_WILL_UNDERFLOW( pxBlock->xBlockSize, xWantedSize ) == 0 );

                if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                {
                    /* This block is to be split into two.  Create a new
                     * block following the number of bytes requested. The void
                     * cast is used to prevent byte alignment warnings from the
                     * compiler. */
                    pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                    configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 );

                    /* Calculate the sizes of two blocks split from the
                     * single block. */
                    pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
                    pxBlock->xBlockSize = xWantedSize;

                    /* Insert the new block into the list of free blocks. */
                    pxNewBlockLink->pxNextFreeBlock = pxPreviousBlock->pxNextFreeBlock;
                    pxPreviousBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxNewBlockLink );
                }

                pxPool->xFreeBytesRemaining -= pxBlock->xBlockSize;

                if( pxPool->xFreeBytesRemaining < pxPool->xMinimumEverFreeBytesRemaining )
                {
                    pxPool->xMinimumEverFreeBytesRemaining = pxPool->xFreeBytesRemaining;
                }

                /* The block is being returned - it is allocated and owned
                 * by the application and has no "next" block. */
                heapALLOCATE_BLOCK( pxBlock );
                pxBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( NULL );
                pxPool->xNumberOfSuccessfulAllocations++;
            }
        }
    }

    configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
    return pvReturn;
}
/*-----------------------------------------------------------*/

void vPortFree( HeapHandle_t xHeap, void * pv )
{
    HeapPool_t * pxPool = prvGetHeapPool( xHeap );

    configASSERT( pxPool != NULL );

    if( pxPool == NULL )
    {
        return;
    }

    prvFreeFromPool( pxPool, pv );
}
/*-----------------------------------------------------------*/

static void prvFreeFromPool( HeapPool_t * pxPool, void * pv )
{
    uint8_t * puc = ( uint8_t * ) pv;
    BlockLink_t * pxLink;

    if( pv != NULL )
    {
        /* The memory being freed will have an BlockLink_t structure immediately
         * before it. */
        puc -= xHeapStructSize;

        /* This casting is to keep the compiler from issuing warnings. */
        pxLink = ( void * ) puc;

        heapVALIDATE_BLOCK_POINTER( pxPool, pxLink );
        configASSERT( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 );
        configASSERT( pxLink->pxNextFreeBlock == heapPROTECT_BLOCK_POINTER( NULL ) );

        if( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 )
        {
            if( pxLink->pxNextFreeBlock == heapPROTECT_BLOCK_POINTER( NULL ) )
            {
                /* The block is being returned to the heap - it is no longer
                 * allocated. */
                heapFREE_BLOCK( pxLink );
                #if ( configHEAP_CLEAR_MEMORY_ON_FREE == 1 )
                {
                    /* Check for underflow as this can occur if xBlockSize is
                     * overwritten in a heap block. */
                    if( heapSUBTRACT_WILL_UNDERFLOW( pxLink->xBlockSize, xHeapStructSize ) == 0 )
                    {
                        ( void ) memset( puc + xHeapStructSize, 0, pxLink->xBlockSize - xHeapStructSize );
                    }
                }
                #endif

                /* Add this block to the list of free blocks. */
                pxPool->xFreeBytesRemaining += pxLink->xBlockSize;
                prvInsertBlockIntoFreeList( pxPool, ( ( BlockLink_t * ) pxLink ) );
                pxPool->xNumberOfSuccessfulFrees++;
            }
        }
    }
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize( HeapHandle_t xHeap )
{
    HeapPool_t * pxPool = prvGetHeapPool( xHeap );

    configASSERT( pxPool != NULL );

    return ( pxPool != NULL ) ? pxPool->xFreeBytesRemaining : ( size_t ) 0;
}
/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize( HeapHandle_t xHeap )
{
    HeapPool_t * pxPool = prvGetHeapPool( xHeap );

    configASSERT( pxPool != NULL );

    return ( pxPool != NULL ) ? pxPool->xMinimumEverFreeBytesRemaining : ( size_t ) 0;
}
/*-----------------------------------------------------------*/

void xPortResetHeapMinimumEverFreeHeapSize( HeapHandle_t xHeap )
{
    HeapPool_t * pxPool = prvGetHeapPool( xHeap );

    configASSERT( pxPool != NULL );

    if( pxPool != NULL )
    {
        pxPool->xMinimumEverFreeBytesRemaining = pxPool->xFreeBytesRemaining;
    }
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks( HeapHandle_t xHeap )
{
    HeapPool_t * pxPool = prvGetHeapPool( xHeap );

    configASSERT( pxPool != NULL );

    if( pxPool != NULL )
    {
        prvHeapInit( pxPool );
    }
}
/*-----------------------------------------------------------*/

void * pvPortCalloc( HeapHandle_t xHeap, size_t xNum, size_t xSize )
{
    void * pv = NULL;

    if( heapMULTIPLY_WILL_OVERFLOW( xNum, xSize ) == 0 )
    {
        pv = pvPortMalloc( xHeap, xNum * xSize );

        if( pv != NULL )
        {
            ( void ) memset( pv, 0, xNum * xSize );
        }
    }

    return pv;
}
/*-----------------------------------------------------------*/

static void prvHeapInit( HeapPool_t * pxPool ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxFirstFreeBlock;
    portPOINTER_SIZE_TYPE uxStartAddress, uxEndAddress;
    size_t xTotalHeapSize = pxPool->xHeapSize;

    /* Ensure the heap starts on a correctly aligned boundary. */
    uxStartAddress = ( portPOINTER_SIZE_TYPE ) pxPool->pucHeap;

    if( ( uxStartAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
    {
        uxStartAddress += ( portBYTE_ALIGNMENT - 1 );
        uxStartAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK );
        xTotalHeapSize -= ( size_t ) ( uxStartAddress - ( portPOINTER_SIZE_TYPE ) pxPool->pucHeap );
    }

    /* xStart is used to hold a pointer to the first item in the list of free
     * blocks.  The void cast is used to prevent compiler warnings. */
    pxPool->xStart.pxNextFreeBlock = ( void * ) heapPROTECT_BLOCK_POINTER( uxStartAddress );
    pxPool->xStart.xBlockSize = ( size_t ) 0;

    /* pxEnd is used to mark the end of the list of free blocks and is inserted
     * at the end of the heap space. */
    uxEndAddress = uxStartAddress + ( portPOINTER_SIZE_TYPE ) xTotalHeapSize;
    uxEndAddress -= ( portPOINTER_SIZE_TYPE ) xHeapStructSize;
    uxEndAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK );
    pxPool->pxEnd = ( BlockLink_t * ) uxEndAddress;
    pxPool->pxEnd->xBlockSize = 0;
    pxPool->pxEnd->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( NULL );

    /* To start with there is a single free block that is sized to take up the
     * entire heap space, minus the space taken by pxEnd. */
    pxFirstFreeBlock = ( BlockLink_t * ) uxStartAddress;
    pxFirstFreeBlock->xBlockSize = ( size_t ) ( uxEndAddress - ( portPOINTER_SIZE_TYPE ) pxFirstFreeBlock );
    pxFirstFreeBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxPool->pxEnd );

    /* Only one block exists - and it covers the entire usable heap space. */
    pxPool->xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
    pxPool->xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
    pxPool->xNumberOfSuccessfulAllocations = ( size_t ) 0U;
    pxPool->xNumberOfSuccessfulFrees = ( size_t ) 0U;
}
/*-----------------------------------------------------------*/

static void prvInsertBlockIntoFreeList( HeapPool_t * pxPool,
                                        BlockLink_t * pxBlockToInsert ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxIterator;
    uint8_t * puc;

    /* Iterate through the list until a block is found that has a higher address
     * than the block being inserted. */
    for( pxIterator = &( pxPool->xStart ); heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) < pxBlockToInsert; pxIterator = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        /* Nothing to do here, just iterate to the right position. */
    }

    if( pxIterator != &( pxPool->xStart ) )
    {
        heapVALIDATE_BLOCK_POINTER( pxPool, pxIterator );
    }

    /* Do the block being inserted, and the block it is being inserted after
     * make a contiguous block of memory? */
    puc = ( uint8_t * ) pxIterator;

    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;
    }

    /* Do the block being inserted, and the block it is being inserted before
     * make a contiguous block of memory? */
    puc = ( uint8_t * ) pxBlockToInsert;

    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        if( heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) != pxPool->pxEnd )
        {
            /* Form one big block from the two blocks. */
            pxBlockToInsert->xBlockSize += heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->pxNextFreeBlock;
        }
        else
        {
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxPool->pxEnd );
        }
    }
    else
    {
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* If the block being inserted plugged a gap, so was merged with the block
     * before and the block after, then it's pxNextFreeBlock pointer will have
     * already been set, and should not be set here as that would make it point
     * to itself. */
    if( pxIterator != pxBlockToInsert )
    {
        pxIterator->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxBlockToInsert );
    }
}
/*-----------------------------------------------------------*/

void vPortGetHeapStats( HeapHandle_t xHeap, HeapStats_t * pxHeapStats )
{
    HeapPool_t * pxPool = prvGetHeapPool( xHeap );
    BlockLink_t * pxBlock;
    size_t xBlocks = 0, xMaxSize = 0, xMinSize = SIZE_MAX;

    configASSERT( pxPool != NULL );

    if( pxPool == NULL )
    {
        return;
    }

    pxBlock = heapPROTECT_BLOCK_POINTER( pxPool->xStart.pxNextFreeBlock );

    /* pxBlock will be NULL if the heap has not been initialised.  The heap
     * is initialised automatically when the first allocation is made. */
    if( pxBlock != NULL )
    {
        while( pxBlock != pxPool->pxEnd )
        {
            /* Increment the number of blocks and record the largest block seen
             * so far. */
            xBlocks++;

            if( pxBlock->xBlockSize > xMaxSize )
            {
                xMaxSize = pxBlock->xBlockSize;
            }

            if( pxBlock->xBlockSize < xMinSize )
            {
                xMinSize = pxBlock->xBlockSize;
            }

            /* Move to the next block in the chain until the last block is
             * reached. */
            pxBlock = heapPROTECT_BLOCK_POINTER( pxBlock->pxNextFreeBlock );
        }
    }

    pxHeapStats->xSizeOfLargestFreeBlockInBytes = xMaxSize;
    pxHeapStats->xSizeOfSmallestFreeBlockInBytes = xMinSize;
    pxHeapStats->xNumberOfFreeBlocks = xBlocks;

    pxHeapStats->xAvailableHeapSpaceInBytes = pxPool->xFreeBytesRemaining;
    pxHeapStats->xNumberOfSuccessfulAllocations = pxPool->xNumberOfSuccessfulAllocations;
    pxHeapStats->xNumberOfSuccessfulFrees = pxPool->xNumberOfSuccessfulFrees;
    pxHeapStats->xMinimumEverFreeBytesRemaining = pxPool->xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

/*
 * Reset the state in this file. This state is normally initialized at start up.
 * This function must be called by the application before restarting the
 * scheduler.
 */
void vPortHeapResetState( void )
{
    ( void ) memset( xHeapPools, 0, sizeof( xHeapPools ) );
}
/*-----------------------------------------------------------*/
