/*
 * FreeRTOSConfig.h
 *
 * STM32F103 (Cortex-M3) NUCLEO 보드용 FreeRTOS 설정
 * - SysTick 사용 (HAL과 공유)
 * - 20KB RAM 중 ~10KB를 FreeRTOS 힙으로 할당
 * - 최대 10개 태스크, 10개 큐/세마포어
 *
 * CUBEIDE에서 FreeRTOS를 추가하면 자동 생성되는 설정을
 * 수동으로 최적화한 버전입니다.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * STM32CubeIDE 관련 설정
 *-----------------------------------------------------------*/
#define USE_FULL_ASSERT                     1
#define configUSE_PREEMPTION                1
#define configUSE_PORT_OPTIMISED_TASK_SYNC  1
#define configUSE_QUEUE_SETS                0
#define configUSE_TIME_SLICING              1
#define configUSE_TICKLESS_IDLE             0
#define configUSE_TASK_NOTIFICATIONS         1
#define configUSE_MUTEXES                   1
#define configUSE_RECURSIVE_MUTEXES         1
#define configUSE_COUNTING_SEMAPHORES       1
#define configUSE_TRACE_FACILITY            1
#define configUSE_16_BIT_TICKS              0  /* 32-bit MCU */
#define configUSE_IDLE_HOOK                 1
#define configUSE_TICK_HOOK                 0
#define configUSE_MALLOC_FAILED_HOOK        1
#define configUSE_APPLICATION_TASK_TAG      0
#define configUSE_NEWLIB_REENTRANT          0
#define configUSE_CO_ROUTINES               0
#define configUSE_TIMERS                    1
#define configTIMER_TASK_PRIORITY           (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH            10
#define configTIMER_TASK_STACK_DEPTH        configMINIMAL_STACK_SIZE
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
#define configUSE_DAEMON_TASK_STARTUP_HOOK  0
#define configUSE_POSIX_ERRNO               0

/*-----------------------------------------------------------
 * MCU 클럭 및 Tick 설정
 * STM32F103은 최대 72MHz, SysTick 1ms 인터럽트 사용
 *-----------------------------------------------------------*/
#define configCPU_CLOCK_HZ                  ((unsigned long)72000000)
#define configTICK_RATE_HZ                  ((TickType_t)1000)  /* 1ms tick */
#define configSYSTICK_CLOCK_HZ              (configCPU_CLOCK_HZ / 8)  /* SysTick = HCLK/8 */

/*-----------------------------------------------------------
 * 최대 우선순위 (Cortex-M3는 256단계, FreeRTOS는 5단계로 제한)
 * configMAX_PRIORITIES가 낮을수록 RAM 사용량 감소
 *-----------------------------------------------------------*/
#define configMAX_PRIORITIES                (5)

/*-----------------------------------------------------------
 * 힙 및 스택 설정
 * STM32F103RB: 20KB RAM
 * - FreeRTOS 힙: ~10KB (나머지는 HAL 및 전역 변수용)
 * - 최소 스택 크기: 128 words (512 bytes)
 *-----------------------------------------------------------*/
#define configMINIMAL_STACK_SIZE            ((unsigned short)128)
#define configTOTAL_HEAP_SIZE               ((size_t)(10 * 1024))  /* 10KB */
#define configMAX_TASK_NAME_LEN             (16)

/*-----------------------------------------------------------
 * 인터럽트 중첩 설정 (Cortex-M3)
 * configMAX_SYSCALL_INTERRUPT_PRIORITY:
 *   이优先级 이상의 ISR만 FreeRTOS API 호출 가능
 *   (실수로 낮은优先级 ISR에서 API 호출 시 크래시 방지)
 *-----------------------------------------------------------*/
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  191  /* 5단계 중 최하위 (191 = 0xb0 << 3) */
#define configKERNEL_INTERRUPT_PRIORITY       255  /* 최하위 priority */

/*-----------------------------------------------------------
 * 코루틴 관련 (사용 안 함)
 *-----------------------------------------------------------*/
#define configMAX_CO_ROUTINE_PRIORITIES      (2)

/*-----------------------------------------------------------
 * 메모리 할당 방식: heap_4.c (단편화 방지, 병합 지원)
 *-----------------------------------------------------------*/
#define configFRTOS_MEMORY_SCHEME            4

/*-----------------------------------------------------------
 * 인터럽트 서비스 루틴 (IRQ) 매핑
 * STM32F103은 다음 인터럽트를 FreeRTOS와 연동:
 * - SVCall: SVC instruction → pended interrupts
 * - PendSV: Context switching
 * - SysTick: RTOS tick
 *-----------------------------------------------------------*/
#define vPortSVCHandler                      SVC_Handler
#define xPortPendSVHandler                   PendSV_Handler
#define xPortSysTickHandler                  SysTick_Handler

/*-----------------------------------------------------------
 * Assert 설정 (디버깅용)
 *-----------------------------------------------------------*/
extern void vAssertCalled(const char *pcFile, unsigned long ulLine);
#define configASSERT(x)                     if (!(x)) vAssertCalled(__FILE__, __LINE__)

/*-----------------------------------------------------------
 * 선택적 함수 프로토타입
 *-----------------------------------------------------------*/
extern void vApplicationIdleHook(void);
extern void vApplicationMallocFailedHook(void);

#endif /* FREERTOS_CONFIG_H */
