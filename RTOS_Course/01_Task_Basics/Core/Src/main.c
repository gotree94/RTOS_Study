/**
  ******************************************************************************
  * @file    main.c
  * @author  RTOS Study
  * @brief   1단계: RTOS 기초와 태스크 관리
  *
  * @note    STM32F103 NUCLEO + FreeRTOS (STM32CubeIDE)
  *
  * [학습 목표]
  * 1. RTOS 태스크의 개념 이해 (Task = 독립적인 실행 단위)
  * 2. xTaskCreate()를 사용한 태스크 생성
  * 3. vTaskDelay()를 사용한 비지-웨이팅 없는 지연
  * 4. 멀티태스킹: 여러 태스크가 동시에 실행되는 원리
  * 5. task parameter를 통한 데이터 전달
  *
  * [동작 설명]
  * - Task1: LED를 500ms 간격으로 토글 (우선순위 1)
  * - Task2: LED를 1000ms 간격으로 토글 (우선순위 1)
  * - Task3: 버튼 상태를 200ms 간격으로 읽어 UART 출력 (우선순위 2)
  * - Idle Hook: MCU 유휴 시간 카운트
  *
  * [UART 출력] (115200 baud, ST-Link VCP)
  *   Task1: LED ON/OFF
  *   Task2: LED ON/OFF
  *   Task3: Button state
  *   (모든 UART 출력은 Mutex로 보호됨 - 2단계에서 상세 학습)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>

/* 전역 핸들 ----------------------------------------------------------------*/
UART_HandleTypeDef huart2;

/*---------------------------------------------------------------------------*/
/*  디버깅용 UART 출력 헬퍼 (printf 리다이렉션)                              */
/*  - ST-Link VCP (Virtual COM Port)를 통해 PC로 출력                        */
/*  - TeraTerm/PuTTY: 115200-8-N-1 연결                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief  printf() 리다이렉션 (HAL UART 기반)
 * @note   CubeIDE에서 syscall.c의 __io_putchar() 대신 사용
 */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/*---------------------------------------------------------------------------*/
/*  태스크 함수                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief  Task 1: LED Fast Blink (500ms 주기)
 *
 * [개념] vTaskDelay()
 * - RTOS의 지연 함수. 태스크를 Blocked 상태로 만들어 CPU를 양보한다.
 * - Busy-wait (while(usec--) 와 달리 CPU를 낭비하지 않는다.
 * - 시간 단위는 configTICK_RATE_HZ (1ms) 기준.
 *
 * @param  pvParameters: 태스크 생성 시 전달된 파라미터 (미사용)
 */
void Task_LED1(void const *pvParameters)
{
    /* 태스크 이름을 파라미터로 받아 표시 (데모용) */
    const char *task_name = (const char *)pvParameters;

    /* 무한 루프: 모든 RTOS 태스크는 절대 return하지 않음 */
    for (;;)
    {
        /* LED 토글 */
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);

        /* UART 출력 (어느 태스크가 실행 중인지 확인) */
        printf("[%s] LED Toggle (500ms)\r\n", task_name);

        /*
         * vTaskDelay(500):
         * - 현재 태스크를 500ms 동안 Blocked 상태로 전환
         * - 이 시간 동안 CPU는 다른 태스크(Task2, Task3)나 Idle 태스크가 사용
         * - 500ms 후 자동으로 Ready 상태가 되어 스케줄러가 다시 실행
         */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief  Task 2: LED Slow Blink (1000ms 주기)
 *
 * [개념] 멀티태스킹
 * - Task1과 Task2는 동일한 우선순위(1)로 실행
 * - Round-Robin 스케줄링: time slice 단위로 번갈아 실행
 * - configUSE_TIME_SLICING = 1 이면 각 태스크는 1 tick(1ms) 동안 실행
 */
void Task_LED2(void const *pvParameters)
{
    const char *task_name = (const char *)pvParameters;

    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
        printf("[%s] LED Toggle (1000ms)\r\n", task_name);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief  Task 3: Button Monitor (200ms 폴링)
 *
 * [개념] Task Parameter
 * - xTaskCreate()의 pvParameters로 다양한 데이터 전달 가능
 * - 정수, 구조체 포인터, 문자열 등 무엇이든 가능
 *
 * [개념] 우선순위
 * - 이 태스크의 우선순위는 2로, Task1/Task2(우선순위1)보다 높음
 * - Ready 상태일 때 우선순위가 높은 태스크가 항상 먼저 실행 (Preemption)
 */
void Task_ButtonMonitor(void const *pvParameters)
{
    const char *task_name = (const char *)pvParameters;
    uint8_t prev_state = 1;  /* 버튼 초기 상태 (1 = Not pressed) */

    for (;;)
    {
        uint8_t cur_state = HAL_GPIO_ReadPin(BTN_GPIO_PORT, BTN_PIN);

        /* 버튼 상태가 변경되었을 때만 출력 (노이즈 필터 + 디바운스) */
        if (cur_state != prev_state)
        {
            if (cur_state == GPIO_PIN_RESET)
            {
                printf("[%s] Button PRESSED!\r\n", task_name);
            }
            else
            {
                printf("[%s] Button RELEASED!\r\n", task_name);
            }
            prev_state = cur_state;
        }

        /* 200ms polling - RTOS 스타일: busy-wait 대신 vTaskDelay 사용 */
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/*---------------------------------------------------------------------------*/
/*  Idle Hook (선택 사항)                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief  Idle 태스크 Hook
 *
 * [개념] Idle 태스크
 * - 실행 가능한 태스크가 없을 때 Idle 태스크가 실행됨
 * - configUSE_IDLE_HOOK = 1 로 설정하면 이 함수가 주기적으로 호출됨
 * - 저전력 모드 진입, CPU 사용률 측정 등에 활용
 *
 * @note   Idle Hook 내에서는 vTaskDelay() 등을 호출하면 안 됨
 */
void vApplicationIdleHook(void)
{
    /* Idle 시간 카운트 (디버깅/성능 측정용) */
    static uint32_t idle_count = 0;
    idle_count++;

    /* 1000번마다 한 번씩 출력 (UART 출력 과부하 방지) */
    if ((idle_count % 1000) == 0)
    {
        /*
         * printf()를 여기서 직접 호출하면 Mutex가 없어 충돌 위험.
         * 2단계에서 배울 Queue를 사용하거나, flag를 설정하는 방식 권장.
         * 여기서는 간단히 주석 처리.
         */
        /* printf("[Idle] idle_count = %lu\r\n", idle_count); */
    }
}

/**
 * @brief  Malloc 실패 Hook
 *
 * malloc() 실패 시 호출됨 → 메모리 부족 상황
 */
void vApplicationMallocFailedHook(void)
{
    /* 무한 루프: 에러 발생을 시각적으로 표시 */
    Error_Handler();
}

/*---------------------------------------------------------------------------*/
/*  Assert 실패 처리                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief  configASSERT() 실패 시 호출
 */
void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    printf("[ASSERT] File: %s, Line: %lu\r\n", pcFile, ulLine);
    Error_Handler();
}

/*---------------------------------------------------------------------------*/
/*  메인 함수: RTOS 애플리케이션의 시작점                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief  main() - RTOS 애플리케이션 Entry Point
 *
 * [실행 순서]
 * 1. HAL 및 주변장치 초기화 (CubeIDE 생성 코드와 동일)
 * 2. FreeRTOS 태스크 생성
 * 3. vTaskStartScheduler() 호출로 스케줄러 시작
 * 4. 이후부터는 FreeRTOS가 모든 태스크를 관리
 * 5. main()은 여기서 끝(스케줄러가 제어권을 가짐)
 */
int main(void)
{
    /* HAL 라이브러리 초기화 */
    HAL_Init();

    /* 시스템 클럭 설정: HSE 8MHz → PLL → 72MHz */
    SystemClock_Config();

    /* GPIO 초기화 (LED, Button) */
    MX_GPIO_Init();

    /* USART2 초기화 (115200 baud, ST-Link VCP) */
    MX_USART2_UART_Init();

    /* UART 환영 메시지 */
    printf("\r\n");
    printf("=============================================\r\n");
    printf("  RTOS Study - Stage 1: Task Basics\r\n");
    printf("  STM32F103 NUCLEO + FreeRTOS\r\n");
    printf("=============================================\r\n");
    printf("\r\n");

    /*
     * ------------------------------------------------------------------
     *  태스크 생성: xTaskCreate()
     * ------------------------------------------------------------------
     *
     * [함수 원형]
     * BaseType_t xTaskCreate(
     *     TaskFunction_t       pvTaskCode,    // 태스크 함수 포인터
     *     const char * const   pcName,        // 태스크 이름 (디버깅용)
     *     unsigned short       usStackDepth,  // 스택 크기 (워드 단위)
     *     void *               pvParameters,  // 태스크에 전달할 파라미터
     *     UBaseType_t          uxPriority,    // 우선순위 (0=최저, configMAX_PRIORITIES-1=최고)
     *     TaskHandle_t *       pvCreatedTask  // 태스크 핸들 (NULL 가능)
     * );
     *
     * [리턴 값]
     * - pdPASS: 성공
     * - errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY: heap 부족
     */

    /* Task 1: LED Fast Blink (500ms) - 우선순위 1 */
    xTaskCreate(
        Task_LED1,              /* 태스크 함수 */
        "LED1",                 /* 태스크 이름 (디버거에서 표시) */
        configMINIMAL_STACK_SIZE, /* 스택 크기 (128 words = 512 bytes) */
        (void *)"LED1",         /* 파라미터: 태스크 이름 문자열 */
        1,                      /* 우선순위: 1 */
        NULL                    /* 핸들: 불필요하므로 NULL */
    );

    /* Task 2: LED Slow Blink (1000ms) - 우선순위 1 */
    xTaskCreate(
        Task_LED2,
        "LED2",
        configMINIMAL_STACK_SIZE,
        (void *)"LED2",
        1,                      /* 동일 우선순위 1 → Round-Robin */
        NULL
    );

    /* Task 3: Button Monitor - 우선순위 2 (LED 태스크보다 높음) */
    xTaskCreate(
        Task_ButtonMonitor,
        "BTN",
        configMINIMAL_STACK_SIZE + 32,  /* 버튼 태스크는 약간 더 큰 스택 */
        (void *)"BTN",
        2,                      /* 우선순위: 2 (LED 태스크보다 높음) */
        NULL
    );

    /*
     * ------------------------------------------------------------------
     *  vTaskStartScheduler(): FreeRTOS 스케줄러 시작
     * ------------------------------------------------------------------
     *
     * [중요!]
     * - 이 함수가 호출되면 FreeRTOS가 시스템 제어권을 가짐
     * - Idle 태스크가 자동으로 생성됨
     * - 이후부터는 main()으로 돌아오지 않음
     * - 만약 리턴된다면 힙 메모리 부족이 원인
     */
    vTaskStartScheduler();

    /*
     * ------------------------------------------------------------------
     *  스케줄러가 실패한 경우만 여기 도달
     * ------------------------------------------------------------------
     *  일반적인 원인:
     *   - configTOTAL_HEAP_SIZE 부족
     *   - 스택 크기(configMINIMAL_STACK_SIZE)가 너무 작음
     *   - 인터럽트 설정 오류
     */
    printf("[FATAL] Scheduler start failed! Heap insufficient.\r\n");
    Error_Handler();
}

/*---------------------------------------------------------------------------*/
/*  주변장치 초기화 함수 (CubeIDE 생성 코드와 동일)                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief  System Clock Configuration
 *         HSE (8MHz) → PLL (x9) → SYSCLK = 72MHz
 *         APB1 = 36MHz, APB2 = 72MHz
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /* HSE Oscillator 활성화 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* SYSCLK = PLL output (72MHz), HCLK = 72MHz, APB1 = 36MHz, APB2 = 72MHz */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLL;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    /* USART2 클럭 설정 */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief  GPIO 초기화
 *         - PA5: Output (LED LD2)
 *         - PC13: Input (Button B1)
 */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIOA 클록 활성화 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* GPIOC 클록 활성화 */
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PA5 = LED: Push-Pull Output */
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    /* PC13 = Button: Input (Pull-Up, NUCLEO 하드웨어에 이미 Pull-Up 있음) */
    GPIO_InitStruct.Pin = BTN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BTN_GPIO_PORT, &GPIO_InitStruct);
}

/**
 * @brief  USART2 초기화 (ST-Link VCP)
 *         PA2 = TX, PA3 = RX, 115200 baud
 */
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
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/*---------------------------------------------------------------------------*/
/*  Error Handler                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief  시스템 에러 발생 시 처리
 *         LED Fast Blink (100ms)로 사용자에게 시각적 표시
 */
void Error_Handler(void)
{
    printf("[FATAL] Error Handler invoked!\r\n");

    /* LED 100ms Blink = 에러 상태 표시 */
    while (1)
    {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
        /* 중첨 방지를 위해 HAL_Delay (RTOS 비활성 상태이므로 직접 사용) */
        for (volatile uint32_t i = 0; i < 7200000; i++);
    }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  HAL 라이브러리 Assert
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("[HAL ASSERT] File: %s, Line: %ld\r\n", file, line);
}
#endif /* USE_FULL_ASSERT */
