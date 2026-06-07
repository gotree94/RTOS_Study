/**
  ******************************************************************************
  * @file    main.c
  * @author  RTOS Study
  * @brief   3단계: 스케줄링과 타이머
  *
  * @note    STM32F103 NUCLEO + FreeRTOS (STM32CubeIDE)
  *
  * [학습 목표]
  * 1. 우선순위 기반 선점형 스케줄링 동작 이해
  * 2. Priority Inversion 문제와 Priority Inheritance 해결책
  * 3. Software Timer (One-shot / Auto-reload)
  * 4. Task Notification을 통한 경량 동기화
  *
  * [UART 출력] (115200 baud, ST-Link VCP)
  ******************************************************************************
  */

#include "main.h"
#include <stdio.h>

UART_HandleTypeDef huart2;

/* printf 리다이렉션 */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}


/*===========================================================================*/
/*  예제 3.1: 우선순위 기반 선점형 스케줄링                                   */
/*                                                                           */
/*  [개념]                                                                   */
/*  - FreeRTOS는 기본적으로 Preemptive Scheduling (선점형)                   */
/*  - configUSE_PREEMPTION = 1: 우선순위가 높은 태스크가 준비되면 즉시 실행   */
/*  - 동일 우선순위: Round-Robin (Time Slicing)                              */
/*  - vTaskDelay(): Blocked 상태로 전환, 낮은 우선순위 태스크 실행 기회 부여  */
/*                                                                           */
/*  [동작]                                                                   */
/*  - Task_High (prio=3): 1초마다 실행, 3회 반복 후 대기                     */
/*  - Task_Mid  (prio=2): 2초마다 실행, CPU 연산 작업 시뮬레이션              */
/*  - Task_Low  (prio=1): 500ms마다 실행, High/Mid가 Blocked일 때만 실행     */
/*===========================================================================*/
#if (EXAMPLE_PRIORITY_SCHED == 1)

void Task_High(void const *pvParameters)
{
    (void)pvParameters;
    uint32_t count = 0;

    for (;;)
    {
        count++;
        printf("[High-prio=3] Running (count=%lu)\r\n", count);

        /* 1초 Blocked → 이 시간 동안 Mid/Low 실행 */
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* 3번 실행 후 자신을 Suspend (더 이상 실행 안 함) */
        if (count >= 3)
        {
            printf("[High-prio=3] Task complete. Suspending...\r\n");
            vTaskSuspend(NULL);  /* NULL = 자기 자신 */
        }
    }
}

void Task_Mid(void const *pvParameters)
{
    (void)pvParameters;
    uint32_t count = 0;

    for (;;)
    {
        count++;
        printf("[Mid-prio=2] Running (count=%lu)\r\n", count);

        /*
         * Busy-wait 시뮬레이션: 500ms 동안 CPU를 놓지 않음
         * - High가 Ready가 되어도 Mid가 CPU를 점유 중이면
         *   High는 Mid의 Time Slice가 끝날 때까지 대기...❌
         * - FreeRTOS는 Preemptive이므로 High가 Ready가 되면
         *   Mid를 즉시 선점하고 High가 실행됨 ✅
         */
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 500);  /* busy-wait */

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void Task_Low(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        printf("[Low-prio=1] Running (High/Mid blocked)\r\n");

        /*
         * Low 태스크가 실행 중이라는 것은
         * High와 Mid가 모두 Blocked 상태라는 의미
         */
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#endif /* EXAMPLE_PRIORITY_SCHED */


/*===========================================================================*/
/*  예제 3.2: Priority Inversion (우선순위 역전)                              */
/*                                                                           */
/*  [문제]                                                                   */
/*  다음과 같은 상황에서 우선순위 역전이 발생:                                */
/*    Low (prio=1): Mutex 소유 중 → sleep                                    */
/*    Mid (prio=2): Low를 선점 → CPU 사용 (Mutsu 필요 없음)                  */
/*    High (prio=3): Mutex 필요 → Low가 반환할 때까지 Blocked                */
/*                                                                           */
/*  결과: Low < Mid < High 순으로 실행되어야 하지만                           */
/*        High가 Mid보다 늦게 실행됨 = 우선순위 역전                          */
/*                                                                           */
/*  [해결: Priority Inheritance]                                             */
/*  - FreeRTOS Mutex는 자동으로 Priority Inheritance 지원                     */
/*  - Low가 Mutex를 소유 중일 때 High가 요청 → Low의 우선순위가 3으로 상승   */
/*  - → Mid가 Low를 선점하지 못함 → High가 더 빨리 Mutex 획득                */
/*===========================================================================*/
#if (EXAMPLE_PRIORITY_INVERSION == 1)

SemaphoreHandle_t xInvMutex = NULL;

void Task_Low_PI(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        printf("[Low-prio=1] Attempting to take mutex...\r\n");
        xSemaphoreTake(xInvMutex, portMAX_DELAY);
        printf("[Low-prio=1] Mutex ACQUIRED. Working... (prio=%lu)\r\n",
               (unsigned long)uxTaskPriorityGet(NULL));

        /* Mutex를 쥔 상태로 3초 대기 (이 사이에 Mid가 끼어들 수 있음) */
        vTaskDelay(pdMS_TO_TICKS(3000));

        printf("[Low-prio=1] Releasing mutex.\r\n");
        xSemaphoreGive(xInvMutex);

        /* 4초 후 재시도 */
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

void Task_Mid_PI(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        printf("[Mid-prio=2] Running (preempted Low!)\r\n");
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);

        /*
         * 2초 동안 CPU 사용 (Mutex 불필요)
         * - Priority Inversion 발생 시:
         *   Mid가 Low를 선점 → High는 Mutex 못 얻어서 Blocked
         * - Priority Inheritance 적용 시:
         *   Low의 우선순위가 3으로 상승 → Mid는 Low를 선점 못함
         *   → Low가 먼저 Mutex 반환 → High 실행
         */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void Task_High_PI(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        printf("[High-prio=3] Waiting for mutex...\r\n");
        uint32_t t_start = HAL_GetTick();

        /*
         * High가 Mutex 요청 → Low가 가지고 있음 → Blocked
         * [Inversion 없음] Low가 반환할 때까지 3초 + Mid 선점...
         * [Inheritance] Low의 우선순위가 3으로 상승 → Mid가 Low를
         *   선점하지 못함 → Low가 빨리 반환 → High가 빨리 실행
         */
        xSemaphoreTake(xInvMutex, portMAX_DELAY);

        uint32_t elapsed = HAL_GetTick() - t_start;
        printf("[High-prio=3] Mutex ACQUIRED! Waited %lu ms\r\n", elapsed);

        xSemaphoreGive(xInvMutex);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#endif /* EXAMPLE_PRIORITY_INVERSION */


/*===========================================================================*/
/*  예제 3.3: Software Timer (소프트웨어 타이머)                              */
/*                                                                           */
/*  [개념]                                                                   */
/*  - Software Timer = RTOS가 관리하는 타이머 (하드웨어 타이머 불필요)        */
/*  - Timer Daemon Task: 타이머 콜백을 실행하는 전용 태스크                   */
/*  - 콜백은 태스크 컨텍스트에서 실행 (ISR 아님!)                              */
/*  - 콜백 내에서는 vTaskDelay() 등의 Blocking API 사용 불가                  */
/*                                                                           */
/*  [종류]                                                                   */
/*  - One-shot: 1회 실행 후 정지                                             */
/*  - Auto-reload: 주기적으로 계속 실행                                       */
/*                                                                           */
/*  [동작]                                                                   */
/*  - One-shot Timer: 5초 후 1회 실행 → LED ON                              */
/*  - Auto-reload Timer: 2초마다 실행 → LED 토글 + 메시지                    */
/*  - 버튼: Auto-reload Timer 정지/재시작                                    */
/*===========================================================================*/
#if (EXAMPLE_SOFTWARE_TIMER == 1)

/* 타이머 핸들 */
TimerHandle_t xOneShotTimer = NULL;
TimerHandle_t xAutoReloadTimer = NULL;

/* 타이머 콜백에서 사용할 플래그 (volatile 필수) */
static volatile uint8_t one_shot_fired = 0;
static volatile uint8_t btn_pressed_flag = 0;

/**
 * @brief  One-shot Timer 콜백
 *
 * 타이머 만료 시 1회 실행
 * 콜백 내에서는 vTaskDelay() 불가, 짧게 유지
 */
void vOneShotCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    one_shot_fired = 1;
    printf("[Timer] One-shot fired! (5s elapsed)\r\n");
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
}

/**
 * @brief  Auto-reload Timer 콜백
 *
 * 2초마다 주기적으로 실행
 */
void vAutoReloadCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    printf("[Timer] Auto-reload tick!\r\n");
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
}

/**
 * @brief  버튼 모니터 태스크
 *
 * 버튼으로 Auto-reload 타이머 제어
 * - 첫 누름: 타이머 정지
 * - 두 번째 누름: 타이머 재시작
 */
void Task_ButtonCtrl(void const *pvParameters)
{
    (void)pvParameters;
    uint8_t prev = 1;
    uint8_t timer_running = 1;

    for (;;)
    {
        uint8_t cur = HAL_GPIO_ReadPin(BTN_GPIO_PORT, BTN_PIN);

        if ((cur == GPIO_PIN_RESET) && (prev == GPIO_PIN_SET))
        {
            if (timer_running)
            {
                /*
                 * xTimerStop(): 타이머 정지
                 * - Timer Daemon Task에 정지 명령 전송
                 * - 100ms 동안 Daemon이 처리할 때까지 대기
                 */
                if (xTimerStop(xAutoReloadTimer, pdMS_TO_TICKS(100)) == pdPASS)
                {
                    printf("[BTN] Auto-reload timer STOPPED\r\n");
                    timer_running = 0;
                }
            }
            else
            {
                /*
                 * xTimerStart(): 타이머 재시작
                 */
                if (xTimerStart(xAutoReloadTimer, pdMS_TO_TICKS(100)) == pdPASS)
                {
                    printf("[BTN] Auto-reload timer STARTED\r\n");
                    timer_running = 1;
                }
            }
            /* 버튼 디바운스 */
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        prev = cur;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* 상태 표시 태스크 (One-shot 상태 출력) */
void Task_Status(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (one_shot_fired)
        {
            static int printed = 0;
            if (!printed)
            {
                printf("[Status] One-shot timer has fired. LED is ON.\r\n");
                printed = 1;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#endif /* EXAMPLE_SOFTWARE_TIMER */


/*===========================================================================*/
/*  예제 3.4: Task Notification (태스크 알림)                                */
/*                                                                           */
/*  [개념]                                                                   */
/*  - Task Notification = 각 태스크가 가진 32-bit 알림 값                    */
/*  - Semaphore/Queue보다 2배 빠르고 RAM도 적게 사용                         */
/*  - 모든 FreeRTOS 태스크가 기본적으로 1개의 Notification 보유              */
/*  - 용도: 단순 이벤트 알림, 1:1 동기화                                     */
/*                                                                           */
/*  [API 비교]                                                               */
/*    xSemaphoreGive()     → xTaskNotifyGive()                               */
/*    xSemaphoreTake()     → ulTaskNotifyTake()                              */
/*    xSemaphoreGiveFromISR → vTaskNotifyGiveFromISR()                       */
/*                                                                           */
/*  [동작]                                                                   */
/*  - Task_Producer: 3초마다 Task_Consumer에 Notify                          */
/*  - Task_Consumer: Notify를 기다렸다가 LED 토글 + 메시지                    */
/*  - ulTaskNotifyTake()가 Semaphore Take()보다 ~45% 빠름                    */
/*===========================================================================*/
#if (EXAMPLE_TASK_NOTIFY == 1)

/* Consumer 태스크의 핸들 (Notify 대상 지정용) */
TaskHandle_t xConsumerHandle = NULL;

/**
 * @brief  Producer: 3초마다 Consumer에게 알림 전송
 *
 * xTaskNotifyGive():
 * - 대상 태스크의 Notification 값을 1 증가
 * - 대상 태스크가 ulTaskNotifyTake()에서 대기 중이면 즉시 깨움
 * - Semaphore Give보다 약 2배 빠름
 */
void Task_Producer_NT(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));

        printf("[Producer] Giving notification to consumer...\r\n");

        /*
         * xTaskNotifyGive(xConsumerHandle):
         * - Consumer의 Notification 값 +1
         * - xTaskNotify()의 간편 버전 (increment only)
         * - 리턴: pdPASS (항상 성공, Notification overflow 가능)
         */
        xTaskNotifyGive(xConsumerHandle);
    }
}

/**
 * @brief  Consumer: Producer의 Notify를 기다렸다가 LED 제어
 *
 * ulTaskNotifyTake(clear, timeout):
 * - clear = pdTRUE: Notification 값을 0으로 리셋 후 이전 값 리턴
 * - clear = pdFALSE: Notification 값만 -1
 * - 리턴: 받은 Notification 횟수
 */
void Task_Consumer_NT(void const *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        /*
         * ulTaskNotifyTake(pdTRUE, portMAX_DELAY):
         * - Notification 값이 0보다 클 때까지 Blocked
         * - 값이 0보다 크면: (값-1)을 리턴하고 내부 카운트 감소
         * - Semaphore의 Take()와 동일한 동작, 더 빠름
         */
        uint32_t notify_count = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        printf("[Consumer] Received notification! (count=%lu)\r\n", notify_count);
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
    }
}

/**
 * @brief  버튼으로 Notify 테스트
 *
 * 버튼을 누를 때마다 Consumer에게 직접 Notify
 */
void Task_Button_NT(void const *pvParameters)
{
    (void)pvParameters;
    uint8_t prev = 1;

    for (;;)
    {
        uint8_t cur = HAL_GPIO_ReadPin(BTN_GPIO_PORT, BTN_PIN);

        if ((cur == GPIO_PIN_RESET) && (prev == GPIO_PIN_SET))
        {
            printf("[BTN] Notifying consumer via button!\r\n");
            xTaskNotifyGive(xConsumerHandle);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        prev = cur;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#endif /* EXAMPLE_TASK_NOTIFY */


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
    printf("  RTOS Study - Stage 3: Scheduling & Timers\r\n");
    printf("  STM32F103 NUCLEO + FreeRTOS\r\n");
    printf("=============================================\r\n");
    printf("\r\n");

/*-----------------------------------------------------------------*/
/*  예제 3.1: Priority Scheduling                                   */
/*-----------------------------------------------------------------*/
#if (EXAMPLE_PRIORITY_SCHED == 1)
    printf("[Mode] Priority Scheduling Demo\r\n");
    printf("  - High (prio=3): runs every 1s, stops after 3\r\n");
    printf("  - Mid  (prio=2): runs every 2s, busy 500ms\r\n");
    printf("  - Low  (prio=1): runs when others blocked\r\n");
    printf("\r\n");

    xTaskCreate(Task_High, "High", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
    xTaskCreate(Task_Mid,  "Mid",  configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(Task_Low,  "Low",  configMINIMAL_STACK_SIZE, NULL, 1, NULL);

/*-----------------------------------------------------------------*/
/*  예제 3.2: Priority Inversion                                    */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_PRIORITY_INVERSION == 1)
    printf("[Mode] Priority Inversion & Inheritance Demo\r\n");
    printf("  - Low  (prio=1): holds mutex for 3s\r\n");
    printf("  - Mid  (prio=2): preempts Low (no mutex needed)\r\n");
    printf("  - High (prio=3): waits for mutex\r\n");
    printf("  Observe: High's wait time (Inversion vs Inheritance)\r\n");
    printf("\r\n");

    xInvMutex = xSemaphoreCreateMutex();
    if (xInvMutex == NULL) Error_Handler();

    xTaskCreate(Task_Low_PI,  "Low_PI",  configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(Task_Mid_PI,  "Mid_PI",  configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(Task_High_PI, "High_PI", configMINIMAL_STACK_SIZE, NULL, 3, NULL);

/*-----------------------------------------------------------------*/
/*  예제 3.3: Software Timer                                        */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_SOFTWARE_TIMER == 1)
    printf("[Mode] Software Timer Demo\r\n");
    printf("  - One-shot timer: fires ONCE after 5s\r\n");
    printf("  - Auto-reload timer: fires every 2s\r\n");
    printf("  - Button: toggle auto-reload ON/OFF\r\n");
    printf("\r\n");

    /*
     * xTimerCreate():
     * - One-Shot Timer: 5초 후 1회 실행
     * - uxAutoReload = pdFALSE
     */
    xOneShotTimer = xTimerCreate(
        "OneShot",                      /* 이름 (디버깅용) */
        pdMS_TO_TICKS(5000),            /* 주기: 5000ms */
        pdFALSE,                        /* pdFALSE = One-shot */
        (void *)0,                      /* Timer ID */
        vOneShotCallback                /* 콜백 함수 */
    );
    if (xOneShotTimer == NULL) Error_Handler();

    /*
     * Auto-Reload Timer: 2초마다 반복
     * - uxAutoReload = pdTRUE
     */
    xAutoReloadTimer = xTimerCreate(
        "AutoReload",
        pdMS_TO_TICKS(2000),            /* 주기: 2000ms */
        pdTRUE,                         /* pdTRUE = Auto-reload */
        (void *)0,
        vAutoReloadCallback
    );
    if (xAutoReloadTimer == NULL) Error_Handler();

    /* 타이머 시작 (Task Context) */
    if (xTimerStart(xOneShotTimer, pdMS_TO_TICKS(100)) != pdPASS) Error_Handler();
    if (xTimerStart(xAutoReloadTimer, pdMS_TO_TICKS(100)) != pdPASS) Error_Handler();

    xTaskCreate(Task_ButtonCtrl, "BTN", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(Task_Status,     "Status", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

/*-----------------------------------------------------------------*/
/*  예제 3.4: Task Notification                                     */
/*-----------------------------------------------------------------*/
#elif (EXAMPLE_TASK_NOTIFY == 1)
    printf("[Mode] Task Notification Demo\r\n");
    printf("  - Producer: notifies consumer every 3s\r\n");
    printf("  - Consumer: toggles LED on notification\r\n");
    printf("  - Button: direct notification to consumer\r\n");
    printf("  - Task Notification is 2x faster than semaphore!\r\n");
    printf("\r\n");

    /* Consumer를 먼저 생성하여 핸들 얻기 */
    xTaskCreate(Task_Consumer_NT, "Consumer",
                configMINIMAL_STACK_SIZE, NULL, 1, &xConsumerHandle);

    xTaskCreate(Task_Producer_NT, "Producer",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    xTaskCreate(Task_Button_NT, "BTN",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

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

    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

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
