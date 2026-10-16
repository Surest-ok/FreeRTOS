#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#ifndef configNUMBER_OF_CORES
#define configNUMBER_OF_CORES                    2
#endif
#define configRUN_MULTIPLE_PRIORITIES            1
#define configUSE_CORE_AFFINITY                  1
#ifndef configSMP_TICK_BROADCAST
#define configSMP_TICK_BROADCAST                 1
#endif
#define configSMP_PRIME_INITIAL_TASKS            1
#ifndef configPORT_SMP_DIAGNOSTICS
#define configPORT_SMP_DIAGNOSTICS               0
#endif
#define configUNIQUE_INTERRUPT_PRIORITIES        32
#define configMAX_API_CALL_INTERRUPT_PRIORITY     18
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configUSE_TICKLESS_IDLE                  0
#define configCPU_CLOCK_HZ                       0
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     5
#define configMINIMAL_STACK_SIZE                 ((uint16_t)256)
#define configMAX_TASK_NAME_LEN                  16
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_64_BITS
#define configIDLE_SHOULD_YIELD                  1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configQUEUE_REGISTRY_SIZE                0
#define configENABLE_BACKWARD_COMPATIBILITY      0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS  2
#define configUSE_MINI_LIST_ITEM                 1
#define configSTACK_DEPTH_TYPE                   size_t
#define configMESSAGE_BUFFER_LENGTH_TYPE         size_t
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                (configMAX_PRIORITIES - 2)
#define configTIMER_SERVICE_TASK_CORE_AFFINITY   ((UBaseType_t)1U)
#define configTIMER_TASK_STACK_DEPTH             configMINIMAL_STACK_SIZE
#define configTIMER_QUEUE_LENGTH                 10
#define configUSE_EVENT_GROUPS                   1
#define configUSE_STREAM_BUFFERS                 1
#define configUSE_TASK_NOTIFICATIONS             1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_QUEUE_SETS                     0
#define configUSE_APPLICATION_TASK_TAG           0
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    ((size_t)(64 * 1024 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP         1
#define configHEAP_CLEAR_MEMORY_ON_FREE          0
#define configUSE_TASK_FPU_SUPPORT               1
extern void vConfigureTickInterrupt(void);
extern void vClearTickInterrupt(void);
#define configSETUP_TICK_INTERRUPT()             vConfigureTickInterrupt()
#define configCLEAR_TICK_INTERRUPT()             vClearTickInterrupt()
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configUSE_PASSIVE_IDLE_HOOK              0
void vAssertCalled(const char *file, int line);
#define configASSERT(x)   do { if ((x) == 0) { vAssertCalled(__FILE__, __LINE__); } } while (0)
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_xTaskGetIdleTaskHandle           1
#define INCLUDE_uxTaskGetStackHighWaterMark      0
extern void cherry_trace_sched_switch_hook(void);
#ifndef traceTASK_SWITCHED_IN
#define traceTASK_SWITCHED_IN() cherry_trace_sched_switch_hook()
#endif
#include "port_smp.h"
#endif
