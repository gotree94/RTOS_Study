/**
  ******************************************************************************
  * @file    main.c
  * @author  RTOS Study
  * @brief   2단계: 태스크 동기화와 통신
  *
  * @note    STM32F103 NUCLEO + FreeRTOS (STM32CubeIDE)
  *
  * [학습 목표]
  * 1. Binary Semaphore: 태스크 간 이벤트 신호 전달
  * 2. Queue: 태스크 간 데이터 통신 (Producer-Consumer)
  * 3. Mutex: 공유 자원 보호와 Priority Inheritance
  * 4. Counting Semaphore: 다중 리소스 풀 관리
  *
  * [동작 설명]
  * main.h의 EXAMPLE_x 매크로로 각 예제를 1개씩 선택하여 실행합니다.
  * 각 예제는 독립적이며 FreeRTOS 동기화/통신의 핵심 개념을 다룹니다.
  *
  * [UART 출력] (115200 baud, ST-Link VCP)
  *   각 예제별로 태스크 상태 변화와 데이터 흐름을 출력
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 전역 핸들 ----------------------------------------------------------------*/
UART_HandleTypeDef huart2;

/*---------------------------------------------------------------------------*/
/*  printf 리다이렉션                                                         */
/*---------------------------------------------------------------------------*/
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/*===========================================================================*/
/*  예제 2.1: Binary Semaphore (이진 세마포어)                               */
/*                                                                           */
/*  [개념]                                                                   */
/*  - Binary Semaphore = 이진 신호등 (0 또는 1 값만 가짐)                    */
/*  - Task가 Semaphore를 Take하면 0이 되고, Give하면 1이 됨                  */
/*  - 주 용도: ISR → Task, 또는 Task → Task 간 "이벤트 발생" 알림           */
/*  - Queue size=1인 Queue와 유사하지만, 데이터를 전달하지 않는다는 차이     */
/*                                                                           */
/*  [동작]                                                                   */
/*  - Task_ButtonMonitor: 버튼 누르면 Semaphore Give (50ms 폴링)             */
/*  - Task_LED: Semaphore Take 시 LED 토글 + 메시지 출력                     */
/*===========================================================================*/
#if (EXAMPLE_BINARY_SEMAPHORE == 1)

/* 세마포어 핸들 (전역 선언) */
SemaphoreHandle_t xBinarySemaphore = NULL;

/**
 * @brief  버튼 모니터 태스크 (Producer: Semaphore 제공)
 *
 * 50ms마다 버튼 상태 확인
 * 버튼이 눌리면(Pressed) xSemaphoreGive() 호출
 * - Give: 세마포어 값을 1로 설정
 * - 만약 이미 1이면 아무 효과 없음 (Binary Semaphore 특성)
 */
void Task_ButtonMonitor(void const *pvParameters)
{
    (void)pvParameters;
    uint8_t prev_state = 1;

    for (;;)
    {
        uint8_t cur_state = HAL_GPIO_ReadPin(BTN_GPIO_PORT, BTN_PIN);

        /* 버튼이 눌렸을 때만 Give (falling edge detection) */
        if ((cur_state == GPIO_PIN_RESET) && (prev_state == GPIO_PIN_SET))
        {
            /*
             * xSemaphoreGive():
             * - 세마포어 값을 1로 설정
             * - 이 세마포어를 Take() 중인 태스크가 있으면 즉시 깨움
             * - 리턴: pdTRUE(성공) / pdFALSE(이미 1)
             */
            if (xSemaphoreGive(xBinarySemaphore) == pdTRUE)
            {
                printf("[BTN] Button Pressed! Semaphore Given.\r\n");
            }
            else
            {
                printf("[BTN] Semaphore already 1 (no waiting task)\r\n");
            }
        }
        prev_state = cur_state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief  LED 제어 태스크 (Consumer: Semaphore 소비)
 *
 * xSemaphoreTake()로 세마포어를 기다림
 * - 세마포어가 1이면 Take 성공 → 0으로 리셋 → LED 토글
 * - 세마포어가 0이면 Blocked 상태로 대기
 *
 * [중요] portMAX_DELAY로 대기 → 버튼 누를 때까지 무한 대기
 */
void Task_LED(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        /*
         * xSemaphoreTake(xBinarySemaphore, portMAX_DELAY):
         * - 세마포어가 1이 될 때까지 태스크 Blocked 상태로 대기
         * - Give()가 호출되면 Ready 상태가 되어 실행 재개
         * - portMAX_DELAY = 영원히 대기
         */
        if (xSemaphoreTake(xBinarySemaphore, portMAX_DELAY) == pdTRUE)
        {
            printf("[LED] Semaphore Taken! Toggling LED.\r\n");
            HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
        }
    }
}

#endif /* EXAMPLE_BINARY_SEMAPHORE */


/*===========================================================================*/
/*  예제 2.2: Queue (큐)                                                    */
/*                                                                           */
/*  [개념]                                                                   */
/*  - Queue = FIFO (First-In-First-Out) 데이터 버퍼                          */
/*  - Producer가 데이터를 Send, Consumer가 Receive                           */
/*  - 데이터 복사 방식 (값을 전달, 포인터 아님)                              */
/*  - Thread-safe: 내부적으로 Mutex로 보호됨                                 */
/*                                                                           */
/*  [동작]                                                                   */
/*  - Task_Producer: 0부터 시작하는 숫자를 1~3초 간격으로 Queue에 전송       */
/*  - Task_Consumer: Queue에서 숫자를 수신하여 LED 토글 + 출력               */
/*===========================================================================*/
#if (EXAMPLE_QUEUE == 1)

/* 큐 핸들 (전역) */
QueueHandle_t xDataQueue = NULL;

/**
 * @brief  Producer 태스크 (데이터 생산)
 *
 * 1~3초 간격으로 카운터 값을 Queue에 전송
 * Queue가 가득 차면 xQueueSend()는 Blocked 상태로 대기
 * (Queue size=5이므로 5개까지 쌓임)
 */
void Task_Producer(void const *pvParameters)
{
    (void)pvParameters;
    uint32_t send_count = 0;

    for (;;)
    {
        send_count++;

        /*
         * xQueueSend(xDataQueue, &send_count, 1000):
         * - send_count 값을 Queue에 복사하여 전송
         * - Queue가 가득 찼으면 1000ms까지 Blocked 대기
         * - 리턴: pdPASS(성공) / errQUEUE_FULL(시간 초과)
         */
        if (xQueueSend(xDataQueue, &send_count, pdMS_TO_TICKS(1000)) == pdPASS)
        {
            printf("[Producer] Sent: %lu\r\n", send_count);
        }
        else
        {
            printf("[Producer] Queue FULL! Failed to send %lu\r\n", send_count);
        }

        /* 1~3초 랜덤 간격 (간단히 count % 3 사용) */
        TickType_t delay = pdMS_TO_TICKS(1000 + (send_count % 3) * 750);
        vTaskDelay(delay);
    }
}

/**
 * @brief  Consumer 태스크 (데이터 소비)
 *
 * Queue에서 데이터를 수신하여 처리
 * Queue가 비어있으면 Blocked 상태로 대기
 */
void Task_Consumer(void const *pvParameters)
{
    (void)pvParameters;
    uint32_t received_value;

    for (;;)
    {
        /*
         * xQueueReceive(xDataQueue, &received_value, portMAX_DELAY):
         * - Queue에서 데이터를 꺼내 received_value에 저장
         * - Queue가 빌 때까지 Blocked 상태로 무한 대기
         * - 리턴: pdPASS(성공) / errQUEUE_EMPTY(시간 초과)
         */
        if (xQueueReceive(xDataQueue, &received_value, portMAX_DELAY) == pdPASS)
        {
            printf("[Consumer] Received: %lu (LED: %s)\r\n",
                   received_value,
                   (received_value % 2 == 0) ? "ON" : "OFF");

            /* 짝수일 때 LED ON, 홀수일 때 LED OFF */
            if (received_value % 2 == 0)
            {
                HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
            }
            else
            {
                HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
            }
        }
    }
}

#endif /* EXAMPLE_QUEUE */


/*===========================================================================*/
/*  예제 2.3: Mutex (뮤텍스)                                                */
/*                                                                           */
/*  [개념]                                                                   */
/*  - Mutex = Mutual Exclusion (상호 배제)                                   */
/*  - Binary Semaphore와 유사하지만 Priority Inheritance 지원                */
/*  - 주 용도: 공유 자원 (printf, UART, I2C 등) 보호                         */
/*  - 반드시 Take한 태스크만 Give 가능 (소유권 개념)                         */
/*                                                                           */
/*  [Priority Inheritance]                                                   */
/*  - Low priority 태스크가 Mutex를 소유 중일 때                              */
/*  - High priority 태스크가 동일 Mutex를 요청하면                            */
/*  - Low priority 태스크의 우선순위가 일시적으로 High로 상승                 */
/*  - → Priority Inversion 문제 해결                                         */
/*                                                                           */
/*  [동작]                                                                   */
/*  - Task_A: Mutex를 잡고 3줄 출력 후 해제 (우선순위 1)                    */
/*  - Task_B: Mutex를 잡고 3줄 출력 후 해제 (우선순위 1)                    */
/*  - Mutex 없으면: 출력이 섞임 (race condition)                             */
/*  - Mutex 있으면: 한 태스크가 완전히 출력 후 다른 태스크 실행              */
/*===========================================================================*/
#if (EXAMPLE_MUTEX == 1)

/* 뮤텍스 핸들 */
SemaphoreHandle_t xMutex = NULL;

/**
 * @brief  Mutex 사용 태스크 A
 *
 * Mutex로 보호된 공유 자원(printf)에 접근
 * Mutex Take → 3줄 출력 → Mutex Give
 * 출력 도중 다른 태스크가 끼어들 수 없음
 */
void Task_A(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        /*
         * xSemaphoreTake(xMutex, portMAX_DELAY):
         * - Mutex 사용권 획득 (다른 태스크는 Blocked)
         * - portMAX_DELAY: 획득할 때까지 무한 대기
         */
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            /* ===== 임계 영역 (Critical Section) 시작 ===== */
            printf("===========================\r\n");
            printf("[Task_A] Inside critical section\r\n");
            printf("===========================\r\n");
            /* ===== 임계 영역 끝 ===== */

            /*
             * xSemaphoreGive(xMutex):
             * - Mutex 반환 (대기 중인 다른 태스크가 있으면 깨움)
             * - 소유권: 반드시 Take한 태스크만 Give 가능
             */
            xSemaphoreGive(xMutex);
        }

        /* 1초 대기 후 재시도 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief  Mutex 사용 태스크 B
 *
 * Task_A와 동일한 Mutex 사용
 * Task_A가 Mutex를 쥐고 있으면 xSemaphoreTake()에서 Blocked
 */
void Task_B(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            /* ===== 임계 영역 시작 ===== */
            printf("---------------------------\r\n");
            printf("[Task_B] Inside critical section\r\n");
            printf("---------------------------\r\n");
            /* ===== 임계 영역 끝 ===== */

            xSemaphoreGive(xMutex);
        }

        /* Task_A와 다른 타이밍 (1.5초) */
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

#endif /* EXAMPLE_MUTEX */


/*===========================================================================*/
/*  예제 2.4: Counting Semaphore (카운팅 세마포어)                           */
/*                                                                           */
/*  [개념]                                                                   */
/*  - Binary Semaphore와 달리 0 이상의 정수 값을 가짐                        */
/*  - xSemaphoreCreateCounting(max, initial)로 생성                          */
/*    - max: 최대 카운트 값 (동시 사용 가능한 리소스 수)                     */
/*    - initial: 초기 카운트 값 (처음에 사용 가능한 리소스 수)               */
/*  - Take: 카운트 -1, Give: 카운트 +1                                       */
/*  - 용도: 한정된 리소스 풀 관리 (버퍼 슬롯, 연결 소켓 등)                 */
/*                                                                           */
/*  [동작]                                                                   */
/*  - 리소스 3개 (Counting Semaphore 초기값 3)                                */
/*  - 5개의 Worker 태스크가 리소스를 사용하려고 경쟁                         */
/*  - 최대 3개만 동시에 리소스 사용 가능                                      */
/*===========================================================================*/
#if (EXAMPLE_COUNTING_SEM == 1)

/* 카운팅 세마포어 핸들 */
SemaphoreHandle_t xCountingSem = NULL;

/* 리소스 이름 배열 */
const char *resource_names[] = {"Resource-A", "Resource-B", "Resource-C"};

/**
 * @brief  Worker 태스크 (리소스 사용자)
 *
 * 각 Worker는 Counting Semaphore를 Take하여 리소스 확보
 * 리소스 사용 후 Give하여 반환
 * 최대 3개만 동시 접근 가능 → 5개 중 2개는 대기
 */
void Task_Worker(void const *pvParameters)
{
    int worker_id = (int)(intptr_t)pvParameters;
    uint32_t use_count = 0;

    for (;;)
    {
        /*
         * xSemaphoreTake(xCountingSem, portMAX_DELAY):
         * - 카운트가 0보다 크면 즉시 Take (카운트 -1)
         * - 카운트가 0이면 다른 Worker가 Give할 때까지 Blocked
         */
        if (xSemaphoreTake(xCountingSem, portMAX_DELAY) == pdTRUE)
        {
            use_count++;
            printf("[Worker %d] Acquired resource! (use #%lu, ", worker_id, use_count);

            /* 남은 리소스 수 출력 */
            UBaseType_t remaining = uxSemaphoreGetCount(xCountingSem);
            printf("remaining: %d)\r\n", remaining);

            /* 리소스 사용 중 (시뮬레이션: 2~4초) */
            vTaskDelay(pdMS_TO_TICKS(2000 + worker_id * 500));

            /* 리소스 반환 */
            xSemaphoreGive(xCountingSem);
            printf("[Worker %d] Released resource.\r\n", worker_id);

            /* 다음 사용까지 대기 */
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

#endif /* EXAMPLE_COUNTING_SEM */


/*---------------------------------------------------------------------------*/
/*  시스템 Hook 함수                                                          */
/*---------------------------------------------------------------------------*/

void vApplicationIdleHook(void)
{
    /* Idle 시간 측정 (실제 구현에서는 저전력 모드 진입 등에 사용) */
    static uint32_t idle_loops = 0;
    idle_loops++;
}

void vApplicationMallocFailedHook(void)
{
    printf("[FATAL] malloc failed! Heap exhausted.\r\n");
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

/**
 * @brief  main() - 2단계 시작점
 *
 * 각 예제는 main.h의 EXAMPLE_x 매크로로 선택
 * 한 번에 하나의 예제만 활성화하여 테스트
 */
int main(void)
{
    /* HAL 초기화 */
    HAL_Init();

    /* 시스템 클럭 72MHz */
    SystemClock_Config();

    /* GPIO (LED, Button) */
    MX_GPIO_Init();

    /* USART2 (115200 baud) */
    MX_USART2_UART_Init();

    printf("\r\n");
    printf("=============================================\r\n");
    printf("  RTOS Study - Stage 2: Task Sync & Comm\r\n");
    printf("  STM32F103 NUCLEO + FreeRTOS\r\n");
    printf("=============================================\r\n");
    printf("\r\n");

/*-----------------------------------------------------------------*/
/*  예제 2.1: Binary Semaphore                                      */
/*-----------------------------------------------------------------*/
#if (EXAMPLE_BINARY_SEMAPHORE == 1)
    printf("[Mode] Binary Semaphore Example\r\n");
    printf("  - Button (PC13) → Semaphore → LED\r\n");
    printf("  - Press button to toggle LED via semaphore\r\n");
    printf("\r\n");

    /*
     * xSemaphoreCreateBinary():
     * - Binary Semaphore 생성 (초기값: 0)
     * - Give() → 1, Take() → 0
     * - 리턴: NULL이면 Heap 부족
     */
    xBinarySemaphore = xSemaphoreCreateBinary();
    if (xBinarySemaphore == NULL)
    {
        printf("[FATAL] Failed to create binary semaphore!\r\n");
        Error_Handler();
    }

    /* 버튼 모니터: 우선순위 2 (Give 제공자) */
    xTaskCreate(Task_ButtonMonitor, "BTN",
                configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    /* LED 제어: 우선순위 1 (Take 소비자) */
    xTaskCreate(Task_LED, "LED",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

/*-----------------------------------------------------------------*/
/*  예제 2.2: Queue                                                */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_QUEUE == 1)
    printf("[Mode] Queue Example (Producer-Consumer)\r\n");
    printf("  - Producer: sends incrementing counter every 1-3s\r\n");
    printf("  - Consumer: receives and controls LED\r\n");
    printf("  - Queue size: 5 items\r\n");
    printf("\r\n");

    /*
     * xQueueCreate(5, sizeof(uint32_t)):
     * - Queue 생성: 최대 5개 항목, 각 항목 4바이트
     * - 내부적으로 메모리 할당 (xQueueCreate는 실패 시 NULL 반환)
     */
    xDataQueue = xQueueCreate(5, sizeof(uint32_t));
    if (xDataQueue == NULL)
    {
        printf("[FATAL] Failed to create queue!\r\n");
        Error_Handler();
    }

    xTaskCreate(Task_Producer, "Producer",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    xTaskCreate(Task_Consumer, "Consumer",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

/*-----------------------------------------------------------------*/
/*  예제 2.3: Mutex                                                */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_MUTEX == 1)
    printf("[Mode] Mutex Example (Resource Protection)\r\n");
    printf("  - Task_A and Task_B share printf resource\r\n");
    printf("  - Mutex prevents output interleaving\r\n");
    printf("  - Observe: clean vs interleaved output\r\n");
    printf("\r\n");

    /*
     * xSemaphoreCreateMutex():
     * - Mutex 생성 (Priority Inheritance 기능 포함)
     * - 초기 상태: Released (누구나 Take 가능)
     * - Binary Semaphore와의 차이: 소유권, Priority Inheritance
     */
    xMutex = xSemaphoreCreateMutex();
    if (xMutex == NULL)
    {
        printf("[FATAL] Failed to create mutex!\r\n");
        Error_Handler();
    }

    xTaskCreate(Task_A, "Task_A",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    xTaskCreate(Task_B, "Task_B",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

/*-----------------------------------------------------------------*/
/*  예제 2.4: Counting Semaphore                                    */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_COUNTING_SEM == 1)
    printf("[Mode] Counting Semaphore Example\r\n");
    printf("  - 3 resources available (init count = 3)\r\n");
    printf("  - 5 workers compete for resources\r\n");
    printf("  - Max 3 workers can access simultaneously\r\n");
    printf("\r\n");

    /*
     * xSemaphoreCreateCounting(3, 3):
     *   max = 3: 최대 3개의 리소스
     *   initial = 3: 처음에 3개 모두 사용 가능
     */
    xCountingSem = xSemaphoreCreateCounting(3, 3);
    if (xCountingSem == NULL)
    {
        printf("[FATAL] Failed to create counting semaphore!\r\n");
        Error_Handler();
    }

    /* 5개의 Worker 태스크 생성 (각각 ID 0~4) */
    for (int i = 0; i < 5; i++)
    {
        xTaskCreate(Task_Worker, "Worker",
                    configMINIMAL_STACK_SIZE,
                    (void *)(intptr_t)i,  /* worker ID */
                    1,                    /* 우선순위 1 */
                    NULL);
    }

#else
    printf("[ERROR] No example selected! Set one EXAMPLE_x to 1 in main.h\r\n");
#endif

    /* 스케줄러 시작 */
    vTaskStartScheduler();

    /* 스케줄러 시작 실패 */
    printf("[FATAL] Scheduler start failed!\r\n");
    Error_Handler();
}


/*---------------------------------------------------------------------------*/
/*  주변장치 초기화 (CubeIDE 생성 코드와 동일)                                */
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

    /* PA5 = LED (Output) */
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    /* PC13 = Button (Input) */
    GPIO_InitStruct.Pin = BTN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
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
    printf("[FATAL] Error Handler invoked!\r\n");
    while (1)
    {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
        for (volatile uint32_t i = 0; i < 3600000; i++);  /* ~100ms @72MHz */
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("[HAL ASSERT] File: %s, Line: %ld\r\n", file, line);
}
#endif
