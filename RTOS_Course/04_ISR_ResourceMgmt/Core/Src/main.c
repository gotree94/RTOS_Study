/**
  ******************************************************************************
  * @file    main.c
  * @author  RTOS Study
  * @brief   4단계: 인터럽트와 리소스 관리
  *
  * @note    STM32F103 NUCLEO + FreeRTOS (STM32CubeIDE)
  *
  * [학습 목표]
  * 1. ISR → Task 통신 (Deferred Interrupt Handling 패턴)
  * 2. FromISR API 사용법 (xSemaphoreGiveFromISR, xQueueSendFromISR)
  * 3. UART RX 인터럽트를 활용한 데이터 수신
  * 4. 스택 오버플로우 탐지와 메모리 관리 전략
  *
  * [핵심 개념]
  * - RTOS에서 ISR은 최대한 짧게!
  * - ISR에서는 FromISR API만 사용 가능
  * - "Deferred Interrupt Handling": ISR은 신호만 보내고 실제 처리는 Task가
  *
  * [UART 출력] (115200 baud, ST-Link VCP)
  ******************************************************************************
  */

#include "main.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart2;

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}


/*===========================================================================*/
/*  예제 4.1: EXTI 인터럽트 → Binary Semaphore → Task                        */
/*                                                                           */
/*  [개념: Deferred Interrupt Handling]                                      */
/*  - ISR에서는 최소한의 작업만 수행 (Semaphore Give)                        */
/*  - 실제 처리 (LED 토글, 메시지 출력)는 Task에서 수행                      */
/*  - ISR 실행 시간을 최소화 → 시스템의 실시간성 보장                        */
/*                                                                           */
/*  [FromISR API]                                                            */
/*  - ISR 컨텍스트에서 FreeRTOS API 호출 시 FromISR 접미사 필요              */
/*  - xSemaphoreGiveFromISR() 👈 올바른 사용                                  */
/*  - xSemaphoreGive() 👈 ISR에서 호출 시 크래시!                             */
/*                                                                           */
/*  [pxHigherPriorityTaskWoken]                                              */
/*  - ISR에서 Task를 깨웠을 때, 그 Task의 우선순위가 현재 실행 중인          */
/*    태스크보다 높으면 이 변수가 pdTRUE로 설정됨                             */
/*  - pdTRUE이면 portYIELD_FROM_ISR() 호출로 즉시 컨텍스트 스위치             */
/*===========================================================================*/
#if (EXAMPLE_EXTI_SEMAPHORE == 1)

SemaphoreHandle_t xButtonSem = NULL;

/* 태스크 핸들 (ISR에서 직접 Notify할 때 필요) */
static TaskHandle_t xButtonTaskHandle = NULL;

/**
 * @brief  EXTI 인터럽트 핸들러 (PC13 = Button)
 *
 * STM32CubeIDE 생성 코드:
 *   HAL_GPIO_EXTI_IRQHandler(BTN_PIN) 호출
 *   → HAL_GPIO_EXTI_Callback() 호출
 *
 * [중요] ISR 컨텍스트:
 * - 실행 시간 최소화 (Semaphore Give만!)
 * - FromISR API 사용
 * - pxHigherPriorityTaskWoken 처리
 */

/*
 * EXTI15_10_IRQHandler는 startup_stm32f103xb.s에서 정의된
 * 인터럽트 벡터입니다. CUBEIDE가 이미 생성한 핸들러와 충돌할 수 있으므로,
 * 여기서는 HAL_GPIO_EXTI_Callback()만 구현합니다.
 *
 * CUBEIDE가 생성한 stm32f1xx_it.c 내의 EXTI15_10_IRQHandler가
 * HAL_GPIO_EXTI_IRQHandler()를 호출하고,
 * 이 함수가 다시 HAL_GPIO_EXTI_Callback()을 호출하는 구조입니다.
 */

/**
 * @brief  EXTI 콜백 (ISR 컨텍스트에서 실행)
 *
 * @param  GPIO_Pin: 인터럽트를 발생시킨 핀 번호
 *
 * [FromISR 패턴]
 * 1. BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 * 2. xSemaphoreGiveFromISR(handle, &xHigherPriorityTaskWoken);
 * 3. portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* 우리가 설정한 핀(PC13)의 인터럽트인지 확인 */
    if (GPIO_Pin == BTN_PIN)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        /* ISR에서 Semaphore Give (FromISR 버전 필수!) */
        xSemaphoreGiveFromISR(xButtonSem, &xHigherPriorityTaskWoken);

        /*
         * portYIELD_FROM_ISR():
         * - xHigherPriorityTaskWoken이 pdTRUE이면
         *   ISR 종료 후 즉시 컨텍스트 스위치 수행
         * - pdFALSE이면 아무 일도 안 함
         */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief  버튼 이벤트 처리 태스크
 *
 * Deferred Interrupt Handling:
 * - ISR은 Semaphore Give만 (빠르게!)
 * - 실제 처리 (LED, UART)는 이 Task에서
 */
void Task_ButtonHandler(void const *pvParameters)
{
    (void)pvParameters;

    /* NVIC에서 EXTI 인터럽트 활성화 */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);  /* 우선순위 5 (낮음) */
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    printf("[Task] Button handler started. Press PC13 to trigger ISR.\r\n");
    printf("[Task] ISR → Semaphore → Task (Deferred Interrupt Handling)\r\n");
    printf("\r\n");

    for (;;)
    {
        /* Semaphore를 기다림 (ISR이 Give할 때까지 Blocked) */
        if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE)
        {
            /*
             * 이 코드는 ISR이 아니라 Task 컨텍스트에서 실행됨
             * → printf(), vTaskDelay() 모두 자유롭게 사용 가능
             * → 이것이 Deferred Interrupt Handling의 장점
             */
            printf("[Task] Button event received! Toggling LED.\r\n");
            HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);

            /* 약간의 디바운스 + 연속 누름 방지 */
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

#endif /* EXAMPLE_EXTI_SEMAPHORE */


/*===========================================================================*/
/*  예제 4.2: UART RX 인터럽트 → Queue → Task                                */
/*                                                                           */
/*  [개념]                                                                   */
/*  - UART 수신 인터럽트로 바이트 단위 수신                                  */
/*  - ISR에서 Queue에 데이터 전송                                            */
/*  - Task에서 Queue 수신 → 데이터 처리 (echo, command parsing)              */
/*                                                                           */
/*  [명령어 인터페이스]                                                      */
/*  '1' or 'o': LED ON                                                      */
/*  '0' or 'f': LED OFF                                                     */
/*  't':        LED Toggle                                                   */
/*  '?':        상태 출력                                                     */
/*  '\n':       명령어 종료 (줄 단위 처리)                                    */
/*===========================================================================*/
#if (EXAMPLE_UART_QUEUE == 1)

/* UART RX용 Queue */
QueueHandle_t xUartQueue = NULL;

/* UART RX용 버퍼 (HAL_UART_Receive_IT에서 사용) */
static uint8_t rx_byte;

/**
 * @brief  UART RX 수신 완료 콜백 (ISR 컨텍스트)
 *
 * HAL_UART_Receive_IT()로 시작된 비동기 수신이 완료되면 호출
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        /* 수신된 바이트를 Queue에 전송 (FromISR 버전) */
        xQueueSendFromISR(xUartQueue, &rx_byte, &xHigherPriorityTaskWoken);

        /* 다음 바이트 수신 재시작 */
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief  UART 데이터 처리 태스크
 *
 * Queue에서 바이트를 수신하여 명령어 처리
 * '\n'이 올 때까지 버퍼링 후 줄 단위 처리
 */
void Task_UARTProcessor(void const *pvParameters)
{
    (void)pvParameters;
    char line_buffer[64];
    int idx = 0;

    /* UART RX 인터럽트 시작 */
    memset(line_buffer, 0, sizeof(line_buffer));
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    printf("[UART] Command interface ready.\r\n");
    printf("  Commands: 1=LED ON, 0=LED OFF, t=Toggle, ?=Status\r\n");
    printf("\r\n");

    for (;;)
    {
        uint8_t received;

        /*
         * Queue에서 바이트 수신 (ISR이 보낸 데이터)
         * 포트MAX_DELAY: 데이터가 올 때까지 Blocked
         * → Task가 Blocked 되어 있는 동안 CPU는 다른 태스크 사용 가능
         */
        if (xQueueReceive(xUartQueue, &received, portMAX_DELAY) == pdPASS)
        {
            /* echo back */
            HAL_UART_Transmit(&huart2, &received, 1, 0);

            /* 줄 버퍼에 추가 */
            if (received == '\r' || received == '\n')
            {
                /* 줄 종료: 명령어 처리 */
                line_buffer[idx] = '\0';

                if (idx > 0)
                {
                    printf("\r\n[CMD] Processing: '%s'\r\n", line_buffer);

                    switch (line_buffer[0])
                    {
                        case '1':
                        case 'o':
                        case 'O':
                            HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
                            printf("[CMD] LED ON\r\n");
                            break;

                        case '0':
                        case 'f':
                        case 'F':
                            HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
                            printf("[CMD] LED OFF\r\n");
                            break;

                        case 't':
                        case 'T':
                            HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
                            printf("[CMD] LED Toggled\r\n");
                            break;

                        case '?':
                            printf("[CMD] LED is %s\r\n",
                                   HAL_GPIO_ReadPin(LED_GPIO_PORT, LED_PIN) ?
                                   "ON" : "OFF");
                            printf("[CMD] System uptime: %lu ms\r\n",
                                   xTaskGetTickCount());
                            break;

                        default:
                            printf("[CMD] Unknown: '%c'. Try 1,0,t,?\r\n",
                                   line_buffer[0]);
                            break;
                    }
                }

                /* 버퍼 리셋 */
                idx = 0;
                memset(line_buffer, 0, sizeof(line_buffer));
                printf("\r\n> ");  /* 프롬프트 */
            }
            else if (idx < (int)sizeof(line_buffer) - 1)
            {
                line_buffer[idx++] = (char)received;
            }
        }
    }
}

#endif /* EXAMPLE_UART_QUEUE */


/*===========================================================================*/
/*  예제 4.3: Stack Overflow Detection (스택 오버플로우 탐지)                */
/*                                                                           */
/*  [개념]                                                                   */
/*  - 각 FreeRTOS 태스크는 자신만의 스택을 가짐                               */
/*  - 스택 오버플로우 = 메모리 침범 → 시스템 크래시                          */
/*  - 탐지 방법:                                                            */
/*    Method 1: 스택 포인터가 범위를 벗어났는지 확인 (빠름, 덜 정확)         */
/*    Method 2: 스택 끝에 "Canary" 값을 쓰고 변조 여부 확인 (느림, 정확)     */
/*                                                                           */
/*  [uxTaskGetStackHighWaterMark()]                                          */
/*  - 태스크 스택의 "최대 사용량" 이후 남은 공간 조회                       */
/*  - 값이 0에 가까울수록 스택이 부족하다는 의미                             */
/*===========================================================================*/
#if (EXAMPLE_STACK_OVERFLOW == 1)

/** 스택 오버플로우 탐지용: 인터럽트 비활성화 시점에서 호출됨 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /*
     * 이 Hook은 스택 오버플로우가 감지되었을 때 호출됨
     * 이 시점에서는 시스템이 불안정할 수 있으므로 최소한의 처리만
     */
    printf("\r\n=====================================\r\n");
    printf("[FATAL] Stack Overflow Detected!\r\n");
    printf("  Task: %s\r\n", pcTaskName);
    printf("=====================================\r\n");

    /* LED 고속 깜빡임으로 오버플로우 표시 */
    Error_Handler();
}

/**
 * @brief  오버플로우를 유발하는 태스크 (매우 작은 스택 사용)
 *
 * configMINIMAL_STACK_SIZE의 절반만 사용
 * 지역 변수 + 재귀 호출로 스택 오버플로우 유발
 */
void Task_Overflow(void const *pvParameters)
{
    (void)pvParameters;
    uint32_t loop = 0;

    for (;;)
    {
        loop++;

        /*
         * 스택 사용량 모니터링 (오버플로우 전에 체크)
         * uxTaskGetStackHighWaterMark(NULL):
         *   - 지금까지 사용한 스택 중 "가장 많이 사용했을 때"의 여유 공간
         *   - NULL = 자기 자신
         */
        UBaseType_t free_stack = uxTaskGetStackHighWaterMark(NULL);
        printf("[Overflow] Loop=%lu, Free stack=%u words\r\n",
               loop, (unsigned int)free_stack);

        /*
         * 의도적으로 큰 지역 변수 사용 → 스택 소모
         * (컴파일러가 최적화로 제거하지 못하도록 volatile 사용)
         */
        {
            volatile uint8_t large_buffer[256];  /* 256 bytes = 64 words */
            memset((void*)large_buffer, loop, sizeof(large_buffer));
            printf("[Overflow] Allocated 256-byte buffer (stack -64 words)\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief  스택 모니터링 태스크
 *
 * 다른 태스크들의 스택 상태를 주기적으로 출력
 */
void Task_StackMonitor(void const *pvParameters)
{
    (void)pvParameters;

    vTaskDelay(pdMS_TO_TICKS(5000));  /* 다른 태스크들이 실행될 시간 확보 */

    for (;;)
    {
        printf("\r\n[Monitor] Stack Usage Report:\r\n");

        /* 모든 태스크의 스택 상태 출력 */
        TaskStatus_t task_status;
        UBaseType_t total_tasks = uxTaskGetNumberOfTasks();
        TaskHandle_t *task_handles = pvPortMalloc(
            sizeof(TaskHandle_t) * total_tasks);

        if (task_handles != NULL)
        {
            UBaseType_t count = uxTaskGetSystemState(
                NULL, 0, NULL);  /* 실제 개수 확인 */
            printf("[Monitor] Total tasks: %u\r\n", (unsigned int)count);
            vPortFree(task_handles);
        }

        /* 고수위 마크 출력 (간단히 현재 태스크만) */
        printf("[Monitor] This task free stack: %u words\r\n",
               (unsigned int)uxTaskGetStackHighWaterMark(NULL));

        printf("\r\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

#endif /* EXAMPLE_STACK_OVERFLOW */


/*===========================================================================*/
/*  예제 4.4: Heap 메모리 관리                                                */
/*                                                                           */
/*  [개념: heap_x.c 비교]                                                    */
/*                                                                           */
/*  heap_1: 단순 배열, 할당만 가능 (해제 불가). 단순함.                      */
/*  heap_2: 최초 적합(First Fit), 단편화 발생 가능                           */
/*  heap_3: malloc/free 래퍼 (thread-safe)                                   */
/*  heap_4: 최초 적합 + 인접 블록 병합 → 단편화 감소 ✅ (우리가 사용)        */
/*  heap_5: heap_4 + 비연속 메모리 영역 지원                                 */
/*                                                                           */
/*  [메모리 관련 API]                                                        */
/*  - pvPortMalloc() / vPortFree(): FreeRTOS 메모리 할당/해제                */
/*  - xPortGetFreeHeapSize(): 현재 사용 가능한 힙 크기                       */
/*  - xPortGetMinimumEverFreeHeapSize(): 최저 힙 워터마크                    */
/*===========================================================================*/
#if (EXAMPLE_HEAP_MGMT == 1)

/**
 * @brief  메모리 관리 데모 태스크
 *
 * 주기적으로 메모리를 할당/해제하며 힙 상태 출력
 * 단편화(fragmentation) 개념을 시각적으로 확인
 */
void Task_HeapDemo(void const *pvParameters)
{
    (void)pvParameters;

    /* 동적 할당할 메모리 블록들의 포인터 배열 */
    #define NUM_BLOCKS 8
    void *blocks[NUM_BLOCKS] = {NULL};

    printf("[Heap] heap_4 allocator demo\r\n");
    printf("  Total heap size: %u bytes\r\n",
           (unsigned int)configTOTAL_HEAP_SIZE);
    printf("\r\n");

    for (int cycle = 1;; cycle++)
    {
        printf("====== Cycle %d ======\r\n", cycle);

        /* 현재 힙 상태 출력 */
        printf("  Free now:       %u bytes\r\n",
               (unsigned int)xPortGetFreeHeapSize());
        printf("  Min ever free:  %u bytes\r\n",
               (unsigned int)xPortGetMinimumEverFreeHeapSize());

        /* 다양한 크기의 메모리 할당 */
        size_t sizes[] = {50, 100, 200, 80, 150, 60, 300, 120};
        printf("  Allocating %d blocks...\r\n", NUM_BLOCKS);

        for (int i = 0; i < NUM_BLOCKS; i++)
        {
            blocks[i] = pvPortMalloc(sizes[i]);
            if (blocks[i] != NULL)
            {
                memset(blocks[i], 0xA5 + i, sizes[i]);  /* 패턴 쓰기 */
                printf("    [%d] malloc(%u) = 0x%p OK\r\n",
                       i, (unsigned int)sizes[i], blocks[i]);
            }
            else
            {
                printf("    [%d] malloc(%u) FAILED!\r\n",
                       i, (unsigned int)sizes[i]);
            }
        }

        printf("  Free after alloc: %u bytes\r\n",
               (unsigned int)xPortGetFreeHeapSize());

        /* 짝수 인덱스 블록만 해제 (단편화 유발) */
        printf("  Freeing even blocks (0,2,4,6)...\r\n");
        for (int i = 0; i < NUM_BLOCKS; i += 2)
        {
            if (blocks[i] != NULL)
            {
                vPortFree(blocks[i]);
                blocks[i] = NULL;
                printf("    [%d] freed\r\n", i);
            }
        }

        printf("  Free after partial free: %u bytes\r\n",
               (unsigned int)xPortGetFreeHeapSize());

        /*
         * [관찰 포인트]
         * - 해제 후 Free 크기가 할당 전과 같지 않을 수 있음
         * - → 메모리 단편화 (Fragmentation)
         * - heap_4는 인접 블록 병합으로 완화하지만 완전 방지는 불가
         */
        size_t delta = xPortGetFreeHeapSize();
        printf("  Note: Free size delta due to fragmentation\r\n");
        printf("\r\n");

        /* 모든 블록 해제 */
        for (int i = 1; i < NUM_BLOCKS; i += 2)
        {
            if (blocks[i] != NULL)
            {
                vPortFree(blocks[i]);
                blocks[i] = NULL;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#endif /* EXAMPLE_HEAP_MGMT */


/*---------------------------------------------------------------------------*/
/*  시스템 Hook                                                               */
/*---------------------------------------------------------------------------*/

void vApplicationIdleHook(void)
{
    static uint32_t cnt = 0;
    cnt++;
}

void vApplicationMallocFailedHook(void)
{
    printf("[FATAL] Malloc failed!\r\n");
    Error_Handler();
}

void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    printf("[ASSERT] %s:%lu\r\n", pcFile, ulLine);
    Error_Handler();
}


/*===========================================================================*/
/*  메인 함수                                                                 */
/*===========================================================================*/

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    printf("\r\n");
    printf("=============================================\r\n");
    printf("  RTOS Study - Stage 4: ISR & Resource Mgmt\r\n");
    printf("  STM32F103 NUCLEO + FreeRTOS\r\n");
    printf("=============================================\r\n");
    printf("\r\n");

/*-----------------------------------------------------------------*/
/*  예제 4.1: EXTI → Semaphore → Task                               */
/*-----------------------------------------------------------------*/
#if (EXAMPLE_EXTI_SEMAPHORE == 1)
    printf("[Mode] EXTI Interrupt → Semaphore → Task\r\n");
    printf("  - PC13 (Button) EXTI 인터럽트 사용\r\n");
    printf("  - ISR: Semaphore Give (최소 작업)\r\n");
    printf("  - Task: LED 토글 + 메시지 출력 (실제 처리)\r\n");
    printf("  - = Deferred Interrupt Handling 패턴\r\n");
    printf("\r\n");

    xButtonSem = xSemaphoreCreateBinary();
    if (xButtonSem == NULL) Error_Handler();

    xTaskCreate(Task_ButtonHandler, "BtnHandler",
                configMINIMAL_STACK_SIZE, NULL, 2, &xButtonTaskHandle);

/*-----------------------------------------------------------------*/
/*  예제 4.2: UART RX → Queue → Task                                */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_UART_QUEUE == 1)
    printf("[Mode] UART RX Interrupt → Queue → Task\r\n");
    printf("  - USART2 RX 인터럽트로 바이트 수신\r\n");
    printf("  - Queue를 통해 Task로 데이터 전달\r\n");
    printf("  - 명령어: 1=ON, 0=OFF, t=Toggle, ?=Status\r\n");
    printf("\r\n");

    /*
     * Queue 크기: 32바이트 (입력 버퍼링)
     * 각 항목: 1바이트 (uint8_t)
     */
    xUartQueue = xQueueCreate(32, sizeof(uint8_t));
    if (xUartQueue == NULL) Error_Handler();

    xTaskCreate(Task_UARTProcessor, "UARTProc",
                configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

/*-----------------------------------------------------------------*/
/*  예제 4.3: Stack Overflow                                       */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_STACK_OVERFLOW == 1)
    printf("[Mode] Stack Overflow Detection\r\n");
    printf("  - Task with very small stack\r\n");
    printf("  - Large local buffer allocation\r\n");
    printf("  - vApplicationStackOverflowHook()\r\n");
    printf("  - uxTaskGetStackHighWaterMark()\r\n");
    printf("\r\n");

    /*
     * 매우 작은 스택 (32 words = 128 bytes)으로 태스크 생성
     * -> 지역 변수나 함수 호출로 쉽게 오버플로우 발생
     */
    xTaskCreate(Task_Overflow, "Overflow",
                32,  /* 매우 작은 스택! */
                NULL, 1, NULL);

    xTaskCreate(Task_StackMonitor, "Monitor",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

/*-----------------------------------------------------------------*/
/*  예제 4.4: Heap Management                                      */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_HEAP_MGMT == 1)
    printf("[Mode] Heap Memory Management\r\n");
    printf("  - heap_4 allocator: first fit + merge\r\n");
    printf("  - pvPortMalloc / vPortFree demo\r\n");
    printf("  - Fragmentation observation\r\n");
    printf("\r\n");

    xTaskCreate(Task_HeapDemo, "HeapDemo",
                configMINIMAL_STACK_SIZE + 64, NULL, 1, NULL);

#else
    printf("[ERROR] No example selected!\r\n");
#endif

    vTaskStartScheduler();
    printf("[FATAL] Scheduler failed!\r\n");
    Error_Handler();
}


/*---------------------------------------------------------------------------*/
/*  주변장치 초기화                                                            */
/*---------------------------------------------------------------------------*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLL;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PA5 = LED */
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    /*
     * PC13 = Button + EXTI (Falling Edge Interrupt)
     * - GPIO_MODE_IT_FALLING: Falling edge에서 인터럽트 발생
     * - NUCLEO 보드의 버튼은 누르면 LOW
     */
    GPIO_InitStruct.Pin = BTN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;  /* Interrupt mode! */
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BTN_GPIO_PORT, &GPIO_InitStruct);
}

void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}


/*---------------------------------------------------------------------------*/
/*  Error Handler                                                            */
/*---------------------------------------------------------------------------*/

void Error_Handler(void)
{
    printf("[FATAL] Error Handler!\r\n");
    while (1)
    {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
        for (volatile uint32_t i = 0; i < 3600000; i++);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("[HAL ASSERT] %s:%ld\r\n", file, line);
}
#endif
