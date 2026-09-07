/*
 * FreeRTOS Kernel V11.1.0
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

/*******************************************************************************
 * This file provides an example FreeRTOSConfig.h header file, inclusive of an
 * abbreviated explanation of each configuration item.  Online and reference
 * documentation provides more information.
 * https://www.freertos.org/a00110.html
 *
 * Constant values enclosed in square brackets ('[' and ']') must be completed
 * before this file will build.
 *
 * Use the FreeRTOSConfig.h supplied with the RTOS port in use rather than this
 * generic file, if one is available.
 ******************************************************************************/

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

//使用断言时，需要包含串口头文件
//需要根据实际运行的芯片来包含库函数
#include "gd32f10x.h"

#include <stdio.h>

//针对不同的编译器调用不同的stdint.h 文件
 #if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__) 
 #include <stdint.h>
 extern uint32_t SystemCoreClock;
 #endif



 //断言
 #define vAssertCalled(char,int) printf("Error:%s,%d\r\n",char,int)
 #define configASSERT(x) if((x)==0) vAssertCalled(__FILE__,__LINE__) 
	 
 /* Cortex M23 port configuration. */
#define configENABLE_MPU								0
#define configENABLE_FPU								0
#define configENABLE_TRUSTZONE							0
#define configRUN_FREERTOS_SECURE_ONLY					0
 

/******************************************************************************/
/* Hardware description related definitions. **********************************/
/******************************************************************************/

/*
 * 写入实际的CPU 内核时钟频率，也就是CPU 指令执行频率，通常称为Fclk
 * Fclk 为供给CPU 内核的时钟信号，我们所说的cpu 主频为 XX MHz，
 * 就是指的这个时钟信号，相应的，1/Fclk 即为cpu 时钟周期；
 * 一般rtos使用systick，即外部晶振或者内置时钟源经过分频等操作实际提供给芯片的频率
 */
#define configCPU_CLOCK_HZ    ( SystemCoreClock )

/* 默认情况下，ARM Cortex-M 移植使用 Cortex-M SysTick 定时器生成 RTOS 滴答中断。
 * 大多数 Cortex-M MCU 以与其本身相同的频率运行 SysTick 定时器。在这种情况下， 无需 configSYSTICK_CLOCK_HZ
 * ，应让其保持未定义状态。如果 SysTick 定时器的时钟频率与 MCU 核心的频率不同，则应将 
 * configCPU_CLOCK_HZ 设置为 MCU 时钟频率（正常情况下），
 * 并将 configSYSTICK_CLOCK_HZ 设置为 SysTick 时钟频率。 
 */


/*
 #define configSYSTICK_CLOCK_HZ                  [Platform specific]
 */

/******************************************************************************/
/* Scheduling behaviour related definitions. **********************************/
/******************************************************************************/

/* RTOS 滴答中断的频率。
 * 滴答中断用于测量时间。因此，滴答频率越高，测量的时间越精确 。
 * 但是，滴答频率高也意味着 RTOS 内核将占用更多的 CPU 时间，
 * 因此 效率会降低。RTOS 演示应用程序使用的滴答频率均为 1000Hz。
 * 这是为了测试 RTOS 内核， 比通常所需的频率要高。
 * 
 * 多项任务可以具有相同的优先级。RTOS 调度器 可在每个 RTOS 滴答期间在具有相同优先级的任务之间切换，
 * 以共享处理器时间。因此，滴答频率高也会 减少分配给每项任务的“时间切片”。 */

#define configTICK_RATE_HZ                         ((TickType_t)1000)

/* 如果希望使用抢占式 RTOS 调度器，请将其设置为 1；如果希望使用协同式 RTOS 调度器，请将其设置为 0。
 * 抢占式：高优先级且处于就绪态的任务会占用cpu资源
 * 协同式：只有当阻塞发生时才可执行其他任务，否则最高优先级任务将永远不会释放资源
 * See https://www.freertos.org/single-core-amp-smp-rtos-scheduling.html. */
#define configUSE_PREEMPTION                       1

/* 默认情况下（如果未定义 configUSE_TIME_SLICING ，或者 configUSE_TIME_SLICING
 定义为 1），FreeRTOS 使用带时间切片的抢占式优先级调度。 
 这意味着 RTOS 调度器将始终运行优先级最高且处于就绪状态的任务，并在每个 RTOS 滴答中断时在优先级相同的任务之间切换。
 每个tick中断时间称为一个时间片，该配置会在中断发生后执行其他同等优先级任务，依次轮询直至该等级任务全部结束
 如果 configUSE_TIME_SLICING 设置为 0，
 则 RTOS 调度器 仍会运行优先级最高且处于就绪状态的任务，但不会 因为发生滴答中断而在优先级相同的任务之间切换。
 * See https://freertos.org/single-core-amp-smp-rtos-scheduling.html. */
#define configUSE_TIME_SLICING                     1

/* 此参数用于控制处于空闲优先级的任务的行为，仅在以下情况下有效：

 * 1、使用抢占式调度器。
 * 2、应用程序创建的任务以空闲优先级运行。
 * 如果 configUSE_TIME_SLICING 设置为 1（或未定义），则具有相同优先级的任务将共用时间切片。
 * 如果没有任务被抢占，则可以假设具有给定优先级的各项任务 都会分配到相同的处理时间，
 * 如果该优先级高于空闲优先级， 情况确实如此。
 * 
 * 如果任务均为空闲优先级，行为可能会略有不同。如果 configIDLE_SHOULD_YIELD 设置为 1， 
 * 则在其他具有空闲优先级的任务准备运行时，空闲任务将立即让出 CPU。
 * 这可确保 在有应用程序任务可供调度时，空闲任务所占用的时间最少。 */
#define configIDLE_SHOULD_YIELD                    1

/* Set configUSE_PORT_OPTIMISED_TASK_SELECTION to 1 to select the next task to
 * run using an algorithm optimised to the instruction set of the target hardware -
 * normally using a count leading zeros assembly instruction.  Set to 0 to select
 * the next task to run using a generic C algorithm that works for all FreeRTOS
 * ports.  Not all FreeRTOS ports have this option.  Defaults to 0 if left
 * undefined. 
 * 典型值为0 不需要更改*/
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    0

/* 如果希望使用低功耗无滴答模式，请将 configUSE_TICKLESS_IDLE
 * 设置为 1； 如果希望保持滴答中断始终运行，请将其设置为 0。并未针对 所有 FreeRTOS 移植提供低功耗无滴答实现。
 * See https://www.freertos.org/low-power-tickless-rtos.html
 * Defaults to 0 if left undefined. */
#define configUSE_TICKLESS_IDLE                    0

/* 应用程序任务可用的优先级数量。多项任务（数量不限） 可以具有相同的优先级。
 * 协程的优先级是独立设置的，请参阅 configMAX_CO_ROUTINE_PRIORITIES
 * configMAX_PRIORITIES 设置可用任务优先级的数量。
 * 可以为任务分配 0 至 (configMAX_PRIORITIES - 1) 的优先级。
 * 0是最低优先级。 
 * 所以rtos并未限制任务数量，只是限制了最大优先级，该配置需要根据实际情况给调整*/
#define configMAX_PRIORITIES                       20


/* configMAX_CO_ROUTINE_PRIORITIES defines the number of priorities available
 * to the application co-routines. Any number of co-routines can share the same
 * priority. Defaults to 0 if left undefined.
 * 仅用于资源过于受限的芯片 */
#define configMAX_CO_ROUTINE_PRIORITIES    2

/* configMINIMAL_STACK_SIZE 定义空闲任务使用的堆栈的大小
 *（以文字表示，而不是以字节表示！）。1字 = 4字节  
 * The kernel does not use this constant for any other
 * purpose.  Demo applications use the constant to make the demos somewhat portable
 * across hardware architectures. 
 * 一般不需要修改*/
#define configMINIMAL_STACK_SIZE                   ((uint16_t)128)

/* 创建任务时，赋予该任务的描述性名称的最大允许长度。长度以字符数指定，包括 NULL 终止字节 */
#define configMAX_TASK_NAME_LEN                    16

/* Time is measured in 'ticks' - which is the number of times the tick interrupt
 * has executed since the RTOS kernel was started.
 * The tick count is held in a variable of type TickType_t.
 *
 * configTICK_TYPE_WIDTH_IN_BITS controls the type (and therefore bit-width) of TickType_t:
 *
 * Defining configTICK_TYPE_WIDTH_IN_BITS as TICK_TYPE_WIDTH_16_BITS causes
 * TickType_t to be defined (typedef'ed) as an unsigned 16-bit type.
 *
 * Defining configTICK_TYPE_WIDTH_IN_BITS as TICK_TYPE_WIDTH_32_BITS causes
 * TickType_t to be defined (typedef'ed) as an unsigned 32-bit type.
 *
 * Defining configTICK_TYPE_WIDTH_IN_BITS as TICK_TYPE_WIDTH_64_BITS causes
 * TickType_t to be defined (typedef'ed) as an unsigned 64-bit type. 
 * 系统运行计时，一般设置为u32型*/
#define configTICK_TYPE_WIDTH_IN_BITS              TICK_TYPE_WIDTH_32_BITS


/* Each task has an array of task notifications.
 * configTASK_NOTIFICATION_ARRAY_ENTRIES sets the number of indexes in the array.
 * See https://www.freertos.org/RTOS-task-notifications.html  Defaults to 1 if
 * left undefined.
 * “任务通知”数组的最大长度， */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES      4

/* configQUEUE_REGISTRY_SIZE sets the maximum number of queues and semaphores
 * that can be referenced from the queue registry.  Only required when using a
 * kernel aware debugger.  Defaults to 0 if left undefined. 
 * 设置可注册的信号量和消息队列的个数，同时在注册时需要绑定队列名称，方便调试*/
#define configQUEUE_REGISTRY_SIZE                  10

/* Set configENABLE_BACKWARD_COMPATIBILITY to 1 to map function names and
 * datatypes from old version of FreeRTOS to their latest equivalent.  Defaults to
 * 1 if left undefined. 
 * 固定为0，不用理会，仅为了排除低版本的宏*/
#define configENABLE_BACKWARD_COMPATIBILITY        0

/* Each task has its own array of pointers that can be used as thread local
 * storage.  configNUM_THREAD_LOCAL_STORAGE_POINTERS set the number of indexes in
 * the array.  See https://www.freertos.org/thread-local-storage-pointers.html
 * Defaults to 0 if left undefined. 
 * 设置每个任务的线程本地存储指针数组大小。
 * 线程本地存储允许应用程序在任务的控制块中存储一些值，每个任务都有自己独立的储存空间，
 * 宏 configNUM_THREAD_LOCAL_STORAGE_POINTERS 指定每个任务线程本地存储指针数组的大小。
 * API函数vTaskSetThreadLocalStoragePointer()用于向指针数组中写入值，API函数pvTaskGetThreadLocalStoragePointer()
 * 用于从指针数组中读取值。
 * 
 * 比如，许多库函数都包含一个叫做errno的全局变量。某些库函数使用errno返回库函数错误信息，应用程序检查这个全局变量来确定发生了那些错误。
 * 在单线程程序中，将errno定义成全局变量是可以的，但是在多线程应用中，每个线程（任务）必须具有自己独有的errno值，
 * 否则，一个任务可能会读取到另一个任务的errno值。*/
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS    0

/* When configUSE_MINI_LIST_ITEM is set to 0, MiniListItem_t and ListItem_t are
 * both the same. When configUSE_MINI_LIST_ITEM is set to 1, MiniListItem_t contains
 * 3 fewer fields than ListItem_t which saves some RAM at the cost of violating
 * strict aliasing rules which some compilers depend on for optimization. Defaults
 * to 1 if left undefined. 
 * 默认为1*/
#define configUSE_MINI_LIST_ITEM                   1

/* Sets the type used by the parameter to xTaskCreate() that specifies the stack
 * size of the task being created.  The same type is used to return information
 * about stack usage in various other API calls.  Defaults to size_t if left
 * undefined. 
 * 设置在调用 xTaskCreate() 时用于指定堆栈深度的类型， 以及在许多其他使用堆栈大小的地方（例如，返回 堆栈高水位线时）
 * 使用的类型。
 * 本质是设置其返回的数据类型*/
#define configSTACK_DEPTH_TYPE                     uint16_t

/* configMESSAGE_BUFFER_LENGTH_TYPE sets the type used to store the length of
 * each message written to a FreeRTOS message buffer (the length is also written to
 * the message buffer.  Defaults to size_t if left undefined - but that may waste
 * space if messages never go above a length that could be held in a uint8_t. 
 * 指定消息缓冲区的长度的数据类型，比如如果缓冲区只有250个数据，则uint8_t 就已经足够*/
#define configMESSAGE_BUFFER_LENGTH_TYPE           size_t

/* 如果 configHEAP_CLEAR_MEMORY_ON_FREE 设置为 1，
 * 则使用 pvPortMalloc() 分配的内存块将在使用 vPortFree() 释放时
 * 被清除（即设置为零）。如果未定义，则默认为 0。*/
#define configHEAP_CLEAR_MEMORY_ON_FREE            1

/* vTaskList and vTaskGetRunTimeStats APIs take a buffer as a parameter and assume
 * that the length of the buffer is configSTATS_BUFFER_MAX_LENGTH. Defaults to
 * 0xFFFF if left undefined.
 * New applications are recommended to use vTaskListTasks and
 * vTaskGetRunTimeStatistics APIs instead and supply the length of the buffer
 * explicitly to avoid memory corruption. 
 * 新版的vTaskListTasks 和 vTaskGetRunTimeStatistics会手动声明数组大小，此配置项可不定义*/
#define configSTATS_BUFFER_MAX_LENGTH              0xFFFF

/* Set configUSE_NEWLIB_REENTRANT to 1 to have a newlib reent structure
 * allocated for each task.  Set to 0 to not support newlib reent structures.
 * Default to 0 if left undefined.
 *
 * Note Newlib support has been included by popular demand, but is not used or
 * tested by the FreeRTOS maintainers themselves. FreeRTOS is not responsible for
 * resulting newlib operation. User must be familiar with newlib and must provide
 * system-wide implementations of the necessary stubs. Note that (at the time of
 * writing) the current newlib design implements a system-wide malloc() that must
 * be provided with locks. 
 * 设置为1会使用nwelib的嵌入式c系统，一般不要开启*/
#define configUSE_NEWLIB_REENTRANT                 0

/******************************************************************************/
/* Software timer related definitions. ****************************************/
/******************************************************************************/

/* Set configUSE_TIMERS to 1 to include software timer functionality in the
 * build.  Set to 0 to exclude software timer functionality from the build.  The
 * FreeRTOS/source/timers.c source file must be included in the build if
 * configUSE_TIMERS is set to 1.  Default to 0 if left undefined.  See
 * https://www.freertos.org/RTOS-software-timer.html. 
 * 如果配置为1，则需要用户主动创建一个定时器任务，来周期性的执行这个定时器任务
 * 如果配置为0，则不需要
 * 软件定时器和mcu的定时器中断没有任何关系*/
#define configUSE_TIMERS                1

/* 配置软件定时器任务的优先级 ，仅在 configUSE_TIMERS 配置为1时生效 
 * See
 * https://www.freertos.org/RTOS-software-timer-service-daemon-task.html  Only used
 * if configUSE_TIMERS is set to 1. */
#define configTIMER_TASK_PRIORITY       ( configMAX_PRIORITIES - 1 )


/* 配置软件定时器任务的任务堆栈的大小
 *   See https://www.freertos.org/RTOS-software-timer-service-daemon-task.html
 * 仅在 configUSE_TIMERS 配置为1时生效  */
#define configTIMER_TASK_STACK_DEPTH    configMINIMAL_STACK_SIZE * 2

/* 软件定时器任务的队列长度
 *  See https://www.freertos.org/RTOS-software-timer-service-daemon-task.html 
 * 仅在 configUSE_TIMERS 配置为1时生效 */
#define configTIMER_QUEUE_LENGTH        10

/******************************************************************************/
/* Event Group related definitions. *******************************************/
/******************************************************************************/

/* Set configUSE_EVENT_GROUPS to 1 to include event group functionality in the
 * build. Set to 0 to exclude event group functionality from the build. The
 * FreeRTOS/source/event_groups.c source file must be included in the build if
 * configUSE_EVENT_GROUPS is set to 1. Defaults to 1 if left undefined. 
 * 默认为1*/

#define configUSE_EVENT_GROUPS    1

/******************************************************************************/
/* Stream Buffer related definitions. *****************************************/
/******************************************************************************/

/* Set configUSE_STREAM_BUFFERS to 1 to include stream buffer functionality in
 * the build. Set to 0 to exclude event group functionality from the build. The
 * FreeRTOS/source/stream_buffer.c source file must be included in the build if
 * configUSE_STREAM_BUFFERS is set to 1. Defaults to 1 if left undefined. 
 * 默认为1*/

#define configUSE_STREAM_BUFFERS    1

/******************************************************************************/
/* Memory allocation related definitions. *************************************/
/******************************************************************************/

/* 用于指示程序是否支持用户自己分配任务及队列等所使用的堆栈,即静态内存分配
 * 设置为 1，则 RTOS 对象可以 通过应用程序编写者提供的 RAM 创建。
 * 设置为0，RTOS对象只能通过RTOS的堆中分配的RAM创建
 * See https://www.freertos.org/Static_Vs_Dynamic_Memory_Allocation.html. */
#define configSUPPORT_STATIC_ALLOCATION              0

/* 用于指示程序是否支持系统自动分配任务及队列等所使用的堆栈,即动态内存分配
 * 设置为 1，RTOS对象在创建时，系统会自动为其分配所占用的内存
 * 设置为 0，只能由用户自己手动分配内存
 * allocated objects from the build.  Defaults to 1 if left undefined.  See
 * https://www.freertos.org/Static_Vs_Dynamic_Memory_Allocation.html. 
 * 一般都使用动态分配*/
#define configSUPPORT_DYNAMIC_ALLOCATION             1

/*  程序中包含heap_1.c, heap_2.c or heap_4.c 时，必须设置此项
 * 标志RTOS堆的总大小
 * 
 * See https://www.freertos.org/a00111.html. */
#define configTOTAL_HEAP_SIZE                        ((size_t)(24*1024))

/* 如果置为1，使用者可以将分配的堆放于内存的任意位置
 * 如果使用 heap_1.c、heap_2.c 或 heap_4.c，并且 configAPPLICATION_ALLOCATED_HEAP 设置为 1，
 * 则应用程序编写者必须创建一个 uint8_t 数组， 其名称和维度必须与下面所示完全一致。
 * 该数组将用作 FreeRTOS 堆。 
 * 如何将数组放置在特定内存位置取决于所使用的编译器，
 * 如果置为0，将由系统创建该数组
 * uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
 */
#define configAPPLICATION_ALLOCATED_HEAP             1

/* 将 configSTACK_ALLOCATION_FROM_SEPARATE_HEAP 设置为 1，
 * 以使任务堆栈从 FreeRTOS 堆以外的某个位置分配。如果您
 * 想要确保堆栈保存在快速内存中，这将非常有用。设置为 0，以使任务堆栈来自标准 FreeRTOS 堆。
 * 如果设置为 1，应用程序编写者必须提供 pvPortMallocStack() 和 vPortFreeStack() 的实现。
 * 默认为 0  */
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP    0

/* 将 configENABLE_HEAP_PROTECTOR 设置为 1，
 * 以启用对 heap_4.c 和 heap_5.c 中的内部堆块指针的边界检查和混淆，以帮助捕获指针损坏。
 *如果未定义，则默认为 0。*/
#define configENABLE_HEAP_PROTECTOR                  0

/******************************************************************************/
/* Interrupt nesting behaviour configuration. *********************************/
/******************************************************************************/

/*
 * configPRIO_BITS 用于指定所使用的设备的中断优先级的位数例如stm32g070只是用高2位
 * configLIBRARY_LOWEST_INTERRUPT_PRIORITY  设备中断的最低优先级
 * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY freertos所能管理的最高优先级，
 * 高于此优先级的中断rtos无法打断
 * 
 * 
*/
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
/* Use the system definition, if there is one. */
#ifdef __NVIC_PRIO_BITS
	#define configPRIO_BITS								__NVIC_PRIO_BITS
#else
	#define configPRIO_BITS								2	 /* 4 priority levels. */
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority"
 * function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY           0x0F

/* Interrupt priorities used by the kernel port layer itself.  These are generic
 * to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY					( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )




/* 设置rtos所能接管的系统中断的最高优先级，低于此优先级的中断，会因rtos的操作而延迟响应，即rtos会合理安排其
 * 响应情况，所以，该类中断中可以使用rtos的api，即以"FromISR"结尾的API
 * 同时高于此优先级的系统中断，仍然由cpu接管，rtos无权操作，同理也无法使用以"FromISR"结尾的API
 * See https://www.freertos.org/RTOS-Cortex-M3-M4.html for information specific to
 * ARM Cortex-M devices. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* configMAX_SYSCALL_INTERRUPT_PRIORITY 的另一种叫法，二者功能一致 */
#define configMAX_API_CALL_INTERRUPT_PRIORITY    configMAX_SYSCALL_INTERRUPT_PRIORITY

/******************************************************************************/
/* Hook and callback function related definitions. ****************************/
/******************************************************************************/

/* 置1：使用空闲钩子（Idle Hook 类似于回调函数）；置0：忽略空闲钩子
 * 空闲任务钩子是一个函数，这个函数由用户来实现，
 * FreeRTOS 规定了函数的名字和参数：void vApplicationIdleHook(void )，
 * 只要空闲任务正在运行，就会重复调用空闲钩子
 * 对于已经删除的RTOS 任务，空闲任务可以释放分配给它们的堆栈内存。
 * 因此必须保证空闲任务可以被CPU 执行
 * 使用空闲钩子函数设置CPU 进入省电模式是很常见的（低功耗）
 * 不可以调用会引起空闲任务阻塞的API 函数
 */
#define configUSE_IDLE_HOOK                   1


/* 置1：使用时间片钩子（Tick Hook）；置0：忽略时间片钩子
 * 时间片钩子是一个函数，这个函数由用户来实现，
 * FreeRTOS 规定了函数的名字和参数：void vApplicationTickHook(void )
 * 时间片中断会调用该函数
 * 函数必须非常短小，不能大量使用堆栈，
 * 不能调用以”FromISR" 或 "FROM_ISR”结尾的API 函数
 */
#define configUSE_TICK_HOOK                   1


/*动态申请内存失败时调用的钩子函数
 * 置1时需要用户实现
 * API格式： void vApplicationMallocFailedHook( void );
 */
#define configUSE_MALLOC_FAILED_HOOK          1

/*该钩子函数用于主动访问守护任务，一般来说守护任务只能通过消息队列来访问，
 * 需要实现 void vApplicationDaemonTaskStartupHook(void) 函数，
 * 用于完成用户级的初始化代码，使用这个选项的优点是在初始化过程中就可以使用FreeRTOS的功能，
 * 并且可以将初始化过程集中在一起完成。这个函数在初始化FreeRTOS内核完成之后，
 * 调用用户任务之前会且只会被执行一遍。
 */
#define configUSE_DAEMON_TASK_STARTUP_HOOK    1

/* Set configUSE_SB_COMPLETED_CALLBACK to 1 to have send and receive completed
 * callbacks for each instance of a stream buffer or message buffer. When the
 * option is set to 1, APIs xStreamBufferCreateWithCallback() and
 * xStreamBufferCreateStaticWithCallback() (and likewise APIs for message
 * buffer) can be used to create a stream buffer or message buffer instance
 * with application provided callbacks. Defaults to 0 if left undefined. 
 * 流缓冲等API，后续再研究*/
#define configUSE_SB_COMPLETED_CALLBACK       0

/* Set configCHECK_FOR_STACK_OVERFLOW to 1 or 2 for FreeRTOS to check for a
 * stack overflow at the time of a context switch.  Set to 0 to not look for a
 * stack overflow.  If configCHECK_FOR_STACK_OVERFLOW is 1 then the check only
 * looks for the stack pointer being out of bounds when a task's context is saved
 * to its stack - this is fast but somewhat ineffective.  If
 * configCHECK_FOR_STACK_OVERFLOW is 2 then the check looks for a pattern written
 * to the end of a task's stack having been overwritten.  This is slower, but will
 * catch most (but not all) stack overflows.  The application writer must provide
 * the stack overflow callback when configCHECK_FOR_STACK_OVERFLOW is set to 1.
 * See https://www.freertos.org/Stacks-and-stack-overflow-checking.html  Defaults
 * to 0 if left undefined. 
 * 堆栈溢出检测相关*/
#define configCHECK_FOR_STACK_OVERFLOW        0

/******************************************************************************/
/* Run time and task stats gathering related definitions. *********************/
/******************************************************************************/

/* Set configGENERATE_RUN_TIME_STATS to 1 to have FreeRTOS collect data on the
 * processing time used by each task.  Set to 0 to not collect the data.  The
 * application writer needs to provide a clock source if set to 1.  Defaults to 0
 * if left undefined.  See https://www.freertos.org/rtos-run-time-stats.html. 
 * 任务运行时间检测，后续学习*/
#define configGENERATE_RUN_TIME_STATS           0

/* Set configUSE_TRACE_FACILITY to include additional task structure members
 * are used by trace and visualisation functions and tools.  Set to 0 to exclude
 * the additional information from the structures. Defaults to 0 if left
 * undefined. 
 * 调试用，可视化跟踪调试，配合 configUSE_STATS_FORMATTING_FUNCTIONS 使用*/
#define configUSE_TRACE_FACILITY                1

/* Set to 1 to include the vTaskList() and vTaskGetRunTimeStats() functions in
 * the build.  Set to 0 to exclude these functions from the build.  These two
 * functions introduce a dependency on string formatting functions that would
 * otherwise not exist - hence they are kept separate.  Defaults to 0 if left
 * undefined. 
 * 调试用*/
#define configUSE_STATS_FORMATTING_FUNCTIONS    1




/******************************************************************************/
/* Definitions that include or exclude functionality. *************************/
/******************************************************************************/

/* Set the following configUSE_* constants to 1 to include the named feature in
 * the build, or 0 to exclude the named feature from the build. */
/*配置是否使能任务通知*/
#define configUSE_TASK_NOTIFICATIONS           1
/* 是否使能互斥锁
 * 如果希望在构建中包含互斥锁功能，请将其设置为 1；如果希望从构建中忽略互斥锁功能，
 * 请将其设置为 0。 读者应该熟悉 与 FreeRTOS 功能相关的互斥锁和二进制信号量之间的区别。*/
#define configUSE_MUTEXES                      1
/*是否使能递归互斥锁
 *如果希望在构建中包含递归互斥锁功能，请将其设置为 1；如果希望从构建中忽略递归互斥锁功能， 请将其设置为 0。*/
#define configUSE_RECURSIVE_MUTEXES            1
/*如果希望在构建中包含计数信号量功能，请将其设置为 1；如果希望从构建中忽略计数信号量功能， 请将其设置为 0。*/
#define configUSE_COUNTING_SEMAPHORES          1
/*如果希望包含队列集功能（即 在多个队列和信号量上阻塞或挂起的能力），
 *请将其设置为 1；如果希望忽略队列集功能，请将其设置为 0。*/
#define configUSE_QUEUE_SETS                   0
/*是否使用任务标签，可以为任务分配对应的值，*/
#define configUSE_APPLICATION_TASK_TAG         0

/* Set the following INCLUDE_* constants to 1 to incldue the named API function,
 * or 0 to exclude the named API function.  Most linkers will remove unused
 * functions even when the constant is 1. */
/*包含或排除相关API  1使能*/
/*设置任务优先级相关API*/
#define INCLUDE_vTaskPrioritySet               1
/*获取任务优先级相关API*/
#define INCLUDE_uxTaskPriorityGet              1
/*删除任务相关API*/
#define INCLUDE_vTaskDelete                    1
/*挂起任务相关API*/
#define INCLUDE_vTaskSuspend                   1
/*恢复在中断中挂起的任务 */
#define INCLUDE_xResumeFromISR                 1
/*任务绝对延时*/
#define INCLUDE_vTaskDelayUntil                1
/*任务延时*/
#define INCLUDE_vTaskDelay                     1
/*获取任务调度器状态*/
#define INCLUDE_xTaskGetSchedulerState         1
/*获取当前任务的任务句柄*/
#define INCLUDE_xTaskGetCurrentTaskHandle      1
/*获取任务堆栈历史剩余最小值*/
#define INCLUDE_uxTaskGetStackHighWaterMark    1
/*获取空闲任务的任务句柄*/
#define INCLUDE_xTaskGetIdleTaskHandle         1
/*获取任务状态*/
#define INCLUDE_eTaskGetState                  1
/*在中断中设置事件标志位*/
#define INCLUDE_xEventGroupSetBitFromISR       1
/*将函数的执行挂到定时器服务任务*/
#define INCLUDE_xTimerPendFunctionCall         1
/*中断任务延时*/
#define INCLUDE_xTaskAbortDelay                1
/*通过任务名获取任务句柄*/
#define INCLUDE_xTaskGetHandle                 1
/*恢复在中断中挂起的任务*/
#define INCLUDE_xTaskResumeFromISR             1

/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS
standard names. */


/* IMPORTANT: This define is commented when used with STM32Cube firmware, when the timebase source is SysTick,
              to prevent overwriting SysTick_Handler defined within STM32Cube HAL */

#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

/******************************************************************************/
/* Co-routine related definitions. ********************************************/
/******************************************************************************/

/* Set configUSE_CO_ROUTINES to 1 to include co-routine functionality in the
 * build, or 0 to omit co-routine functionality from the build. To include
 * co-routines, croutine.c must be included in the project. Defaults to 0 if left
 * undefined. 
 * 是否支持协程，默认为0*/
#define configUSE_CO_ROUTINES              0

#endif /* FREERTOS_CONFIG_H */
