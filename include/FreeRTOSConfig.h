/*
 * FreeRTOSConfig.h for QEMU virt AArch64 (GICv3 SRE, EL1)
 *
 * Uses official ARM_AARCH64_SRE port with -DGUEST -DQEMU.
 * Tick: EL1 Physical Timer (INTID 30) at 1000Hz.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*======= Hardware / GICv3 =======*/
#define configUNIQUE_INTERRUPT_PRIORITIES        32  /* QEMU virt GICv3: 32 priority levels */
#define configMAX_API_CALL_INTERRUPT_PRIORITY     18  /* Must be > 32/2=16. Priorities 18-31 are FreeRTOS-safe. */

/*======= Scheduler =======*/
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configUSE_TICKLESS_IDLE                  0
#define configCPU_CLOCK_HZ                       0   /* Not used; timer is ARM Generic Timer */
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     5
#define configMINIMAL_STACK_SIZE                 ((uint16_t)256)  /* words = 2KB */
#define configMAX_TASK_NAME_LEN                  16
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_64_BITS
#define configIDLE_SHOULD_YIELD                  1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configQUEUE_REGISTRY_SIZE                0
#define configENABLE_BACKWARD_COMPATIBILITY      0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS  0
#define configUSE_MINI_LIST_ITEM                 1
#define configSTACK_DEPTH_TYPE                   size_t
#define configMESSAGE_BUFFER_LENGTH_TYPE         size_t

/*======= Software timers =======*/
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                (configMAX_PRIORITIES - 1)
#define configTIMER_TASK_STACK_DEPTH             configMINIMAL_STACK_SIZE
#define configTIMER_QUEUE_LENGTH                 10

/*======= Feature enables =======*/
#define configUSE_EVENT_GROUPS                   1
#define configUSE_STREAM_BUFFERS                 1
#define configUSE_TASK_NOTIFICATIONS             1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_QUEUE_SETS                     0
#define configUSE_APPLICATION_TASK_TAG           0

/*======= Memory allocation =======*/
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    ((size_t)(128 * 1024))  /* 128KB */
#define configAPPLICATION_ALLOCATED_HEAP         0
#define configHEAP_CLEAR_MEMORY_ON_FREE          0

/*======= FPU support =======*/
#define configUSE_TASK_FPU_SUPPORT               1  /* No FPU context by default (matching -mgeneral-regs-only) */

/*======= Tick interrupt hooks (implemented in gtimer.c) =======*/
extern void vConfigureTickInterrupt(void);
extern void vClearTickInterrupt(void);
#define configSETUP_TICK_INTERRUPT()             vConfigureTickInterrupt()
#define configCLEAR_TICK_INTERRUPT()             vClearTickInterrupt()

/*======= Hook functions =======*/
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2

/*======= Assert =======*/
void vAssertCalled(const char *file, int line);
#define configASSERT(x)   if ((x) == 0) { vAssertCalled(__FILE__, __LINE__); }

/*======= Included API functions =======*/
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      0

#endif /* FREERTOS_CONFIG_H */
