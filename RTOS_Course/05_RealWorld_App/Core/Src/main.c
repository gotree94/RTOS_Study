/**
  ******************************************************************************
  * @file    main.c
  * @author  RTOS Study
  * @brief   5단계: 실무 종합 애플리케이션
  *
  *         === Multi-Sensor Data Acquisition System ===
  *
  * @note    STM32F103 NUCLEO + FreeRTOS (STM32CubeIDE)
  *
  * [시스템 개요]
  * 지금까지 배운 모든 RTOS 개념을 통합한 실무 수준의 데이터 수집 시스템입니다.
  * 버튼 인터럽트로 시작/정지, 센서 데이터 수집 및 필터링, 로깅, 상태 모니터링,
  * Watchdog, LED 표시까지 하나의 완전한 시스템으로 동작합니다.
  *
  * [태스크 구성]
  *   EXTI Button ISR
  *       ↓ (xSemaphoreGiveFromISR)
  *   Task_ButtonCtrl  (prio=3)  → 시스템 상태 제어
  *       ↓ (flag)
  *   Task_SensorAcq   (prio=2)  → 센서 데이터 수집 (vTaskDelayUntil)
  *       ↓ (xQueueSend)
  *   Task_DataProc    (prio=2)  → 데이터 필터링/분석
  *       ↓ (xQueueSend)
  *   Task_Logger      (prio=1)  → UART 출력 + LED 제어
  *
  *   Timer_StatusReport (5s)    → 시스템 상태 주기적 출력
  *   Timer_Watchdog     (10s)   → 데이터 타임아웃 감시
  *
  * [LED 패턴]
  *   - IDLE:     계속 ON
  *   - RUNNING:  500ms Blink
  *   - ERROR:    100ms Fast Blink
  *
  * [UART 명령어]
  *   s = start  (수집 시작)
  *   p = pause  (수집 정지)
  *   ? = status (상태 출력)
  *
  * [UART 출력] (115200 baud, ST-Link VCP)
  ******************************************************************************
  */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------*/
/*  전역 핸들 및 시스템 상태                                                   */
/*---------------------------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* 태스크 핸들 */
static TaskHandle_t xSensorTaskHandle = NULL;
static TaskHandle_t xLoggerTaskHandle = NULL;

/* 동기화 객체 */
static SemaphoreHandle_t xButtonSem = NULL;     /* 버튼 ISR → Task */
static QueueHandle_t xSensorQueue = NULL;       /* Sensor → Processor */
static QueueHandle_t xLogQueue = NULL;          /* Processor → Logger */
static SemaphoreHandle_t xUartMutex = NULL;     /* UART 출력 보호 */

/* 타이머 */
static TimerHandle_t xStatusTimer = NULL;
static TimerHandle_t xWatchdogTimer = NULL;

/* 시스템 상태 (전역, volatile = 여러 태스크에서 접근) */
static volatile SystemState_t g_sysState = SYS_IDLE;
static volatile uint32_t g_sampleCount = 0;
static volatile uint32_t g_errorCount = 0;

/* Watchdog 피드 플래그 */
static volatile uint8_t g_watchdogFed = 0;

/* UART RX 인터럽트용 */
static uint8_t rx_byte;
static QueueHandle_t xUartCmdQueue = NULL;

/* printf 리다이렉션 (Mutex로 보호) */
int __io_putchar(int ch)
{
    /*
     * Mutex가 있으면 Take 후 전송
     * 아직 초기화 전이거나 Mutex가 없으면 직접 전송
     */
    if (xUartMutex != NULL)
    {
        xSemaphoreTake(xUartMutex, portMAX_DELAY);
        HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 100);
        xSemaphoreGive(xUartMutex);
    }
    else
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 100);
    }
    return ch;
}


/*===========================================================================*/
/*  시뮬레이션 센서 드라이버                                                  */
/*                                                                           */
/*  실제 하드웨어가 없으므로 소프트웨어로 센서 값을 시뮬레이션                */
/*  실제 환경에서는 HAL_ADC_Start() 등으로 대체                                */
/*                                                                           */
/*  Sensor 0 (온도): 1500~3500 범위, 서서히 변함                             */
/*  Sensor 1 (조도): 500~4000 범위, 급격히 변할 수 있음                      */
/*===========================================================================*/

/** 센서 상태 구조체 */
typedef struct {
    uint16_t current_value;
    int16_t  trend;       /* 변화 방향 */
    uint32_t last_update;
} SimSensor_t;

static SimSensor_t sim_sensors[2] = {
    {2048,  50, 0},  /* Sensor 0: 온도 (느린 변화) */
    {2500, -150, 0}  /* Sensor 1: 조도 (빠른 변화) */
};

/**
 * @brief  시뮬레이션 센서 값 읽기
 *
 * @param  sensor_id: 0 또는 1
 * @return uint16_t: 0~4095 범위의 ADC 시뮬레이션 값
 */
static uint16_t ReadSimSensor(uint8_t sensor_id)
{
    SimSensor_t *s = &sim_sensors[sensor_id];

    /* 현재 값에 trend 적용 */
    int32_t new_val = (int32_t)s->current_value + s->trend;

    /* 범위 제한 및 방향 전환 */
    if (new_val > 4000) { new_val = 4000; s->trend = -s->trend; }
    if (new_val < 100)  { new_val = 100;  s->trend = -s->trend; }

    /* 일정 확률로 노이즈 추가 */
    if ((s->last_update + 3) < HAL_GetTick())
    {
        int16_t noise = (rand() % 100) - 50;
        new_val += noise;
        if (new_val > 4095) new_val = 4095;
        if (new_val < 0)    new_val = 0;
    }

    s->current_value = (uint16_t)new_val;
    s->last_update = HAL_GetTick();

    /* 일정 주기로 trend 변경 (자연스러운 변화 시뮬레이션) */
    if (sensor_id == 1 && (rand() % 10) == 0)
    {
        s->trend = (rand() % 300) - 150;
    }

    return s->current_value;
}


/*===========================================================================*/
/*  ISP: EXTI Button Interrupt                                               */
/*===========================================================================*/

/**
 * @brief  EXTI 콜백 (ISR 컨텍스트)
 *
 * 버튼 누름 → Semaphore Give → ButtonCtrl Task 실행
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BTN_PIN)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        /* ISR → Task 신호 */
        xSemaphoreGiveFromISR(xButtonSem, &xHigherPriorityTaskWoken);

        /* ButtonCtrl task가 현재 태스크보다 높은 우선순위면 즉시 전환 */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief  UART RX 콜백 (ISR 컨텍스트)
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        BaseType_t xWoken = pdFALSE;
        xQueueSendFromISR(xUartCmdQueue, &rx_byte, &xWoken);
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
        portYIELD_FROM_ISR(xWoken);
    }
}


/*===========================================================================*/
/*  Timer Callbacks                                                          */
/*===========================================================================*/

/**
 * @brief  상태 리포트 타이머 (Auto-reload, 5초)
 *
 * Timer Daemon Task 컨텍스트에서 실행
 * Logger Task에 Notify하여 상태 출력 유도
 */
void vStatusReportCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    /* Logger Task에 상태 출력 요청 (간단히 플래그 설정) */
    if (xLoggerTaskHandle != NULL)
    {
        xTaskNotifyGive(xLoggerTaskHandle);
    }
}

/**
 * @brief  Watchdog 타이머 (One-shot, 10초)
 *
 * 센서 데이터가 10초 동안 들어오지 않으면 시스템 에러
 */
void vWatchdogCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (!g_watchdogFed)
    {
        /* Watchdog이 리셋되지 않음 = 데이터 미수신 = 에러 */
        printf("\r\n[WATCHDOG] Sensor data timeout! System ERROR.\r\n");
        g_sysState = SYS_ERROR;
        g_errorCount++;
    }
    else
    {
        g_watchdogFed = 0;  /* 다음 주기를 위해 리셋 */
    }
}


/*===========================================================================*/
/*  Task: Button Control (우선순위 3 - 최고)                                 */
/*                                                                           */
/*  시스템의 start/stop/pause를 제어                                         */
/*  EXTI ISR로부터 Semaphore를 받아 상태 변경                                */
/*===========================================================================*/
void Task_ButtonCtrl(void const *pvParameters)
{
    (void)pvParameters;

    /* NVIC: EXTI 인터럽트 활성화 */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    printf("[BTN] System ready. Press button or send 's' to start.\r\n");

    for (;;)
    {
        /*
         * Semaphore를 기다림 (ISR 또는 UART 명령어로 Give)
         * - portMAX_DELAY: 버튼 누를 때까지 Blocked
         * - 우선순위 3이므로 대기 중에는 다른 태스크 실행
         */
        if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE)
        {
            switch (g_sysState)
            {
                case SYS_IDLE:
                    g_sysState = SYS_RUNNING;
                    g_sampleCount = 0;
                    printf("\r\n[BTN] ▶ Acquisition STARTED\r\n");

                    /* Sensor Task 재개 */
                    if (xSensorTaskHandle != NULL)
                        vTaskResume(xSensorTaskHandle);

                    /* Watchdog 타이머 시작 */
                    xTimerReset(xWatchdogTimer, pdMS_TO_TICKS(100));
                    break;

                case SYS_RUNNING:
                    g_sysState = SYS_IDLE;
                    printf("\r\n[BTN] ⏸ Acquisition PAUSED\r\n");

                    /* Sensor Task 일시 정지 */
                    if (xSensorTaskHandle != NULL)
                        vTaskSuspend(xSensorTaskHandle);

                    /* Watchdog 정지 */
                    xTimerStop(xWatchdogTimer, pdMS_TO_TICKS(100));
                    break;

                case SYS_ERROR:
                    g_sysState = SYS_IDLE;
                    printf("\r\n[BTN] 🔄 System RESET after error\r\n");
                    g_errorCount = 0;
                    break;

                default:
                    break;
            }
        }
    }
}


/*===========================================================================*/
/*  Task: Sensor Acquisition (우선순위 2)                                    */
/*                                                                           */
/*  vTaskDelayUntil()로 정밀한 주기 유지                                     */
/*  시뮬레이션 센서 값을 읽어 Sensor Queue로 전송                            */
/*===========================================================================*/
void Task_SensorAcq(void const *pvParameters)
{
    (void)pvParameters;

    /*
     * vTaskDelayUntil() 사용을 위한 마지막 웨이크 타임
     * 정밀한 주기 실행이 필요할 때 사용 (vTaskDelay보다 정확)
     */
    TickType_t xLastWakeTime = xTaskGetTickCount();

    printf("[SENSOR] Acquisition task started.\r\n");

    for (;;)
    {
        /*
         * [중요] vTaskDelayUntil() vs vTaskDelay()
         *
         * vTaskDelay(500): 마지막 실행 후 500ms 대기
         *   - 실행 시간이 10ms 걸리면 실제 주기는 510ms
         *   - 시간이 지날수록 오차 누적
         *
         * vTaskDelayUntil(&prev, 500): 이전 실행 시점부터 500ms
         *   - 실행 시간이 10ms 걸려도 주기는 정확히 500ms
         *   - 장기 실행에도 오차 누적 없음
         */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(ACQUISITION_INTERVAL_MS));

        /* 두 센서 값 읽기 */
        uint16_t val0 = ReadSimSensor(0);
        uint16_t val1 = ReadSimSensor(1);

        /* Sensor 0 데이터 전송 */
        SensorData_t data0;
        data0.sensor_id = 0;
        data0.raw_value = val0;
        data0.filtered_value = val0;  /* Processor에서 필터링 */
        data0.timestamp = HAL_GetTick();
        data0.status = 0;

        if (xQueueSend(xSensorQueue, &data0, pdMS_TO_TICKS(100)) != pdPASS)
        {
            printf("[SENSOR] Sensor0 queue FULL!\r\n");
        }

        /* Sensor 1 데이터 전송 */
        SensorData_t data1;
        data1.sensor_id = 1;
        data1.raw_value = val1;
        data1.filtered_value = val1;
        data1.timestamp = HAL_GetTick();
        data1.status = 0;

        if (xQueueSend(xSensorQueue, &data1, pdMS_TO_TICKS(100)) != pdPASS)
        {
            printf("[SENSOR] Sensor1 queue FULL!\r\n");
        }

        g_sampleCount += 2;

        /* Watchdog 피드 (데이터가 정상 수집 중임을 표시) */
        g_watchdogFed = 1;
    }
}


/*===========================================================================*/
/*  Task: Data Processor (우선순위 2)                                        */
/*                                                                           */
/*  - Sensor Queue에서 데이터 수신                                          */
/*  - 이동 평균 필터 적용 (Moving Average Filter)                           */
/*  - 임계값 기반 상태 판단 (HIGH/LOW/NORMAL)                               */
/*  - 필터링된 데이터를 Log Queue로 전송                                     */
/*===========================================================================*/
void Task_DataProc(void const *pvParameters)
{
    (void)pvParameters;

    /* 각 센서의 이동 평균 버퍼 */
    uint16_t history[2][MOVING_AVERAGE_WINDOW] = {{0}};
    int hist_idx[2] = {0};

    printf("[PROC] Data processor started.\r\n");

    for (;;)
    {
        SensorData_t data;

        /* Sensor Queue에서 데이터 수신 (무한 대기) */
        if (xQueueReceive(xSensorQueue, &data, portMAX_DELAY) == pdPASS)
        {
            uint8_t sid = data.sensor_id;

            /* ===== 이동 평균 필터 ===== */
            history[sid][hist_idx[sid]] = data.raw_value;
            hist_idx[sid] = (hist_idx[sid] + 1) % MOVING_AVERAGE_WINDOW;

            uint32_t sum = 0;
            for (int i = 0; i < MOVING_AVERAGE_WINDOW; i++)
                sum += history[sid][i];

            data.filtered_value = (uint16_t)(sum / MOVING_AVERAGE_WINDOW);

            /* ===== 임계값 분석 ===== */
            if (data.filtered_value > SENSOR_HIGH_THRESHOLD)
            {
                data.status = 1;  /* HIGH */
            }
            else if (data.filtered_value < SENSOR_LOW_THRESHOLD)
            {
                data.status = 2;  /* LOW */
            }
            else
            {
                data.status = 0;  /* NORMAL */
            }

            /* 이상값 감지 (RAW와 FILTERED 차이가 크면 노이즈) */
            uint16_t diff = (data.raw_value > data.filtered_value) ?
                            (data.raw_value - data.filtered_value) :
                            (data.filtered_value - data.raw_value);
            if (diff > 500)
            {
                data.status = 3;  /* NOISE */
            }

            /* Log Queue로 전송 (Logger Task가 출력) */
            if (xQueueSend(xLogQueue, &data, pdMS_TO_TICKS(100)) != pdPASS)
            {
                printf("[PROC] Log queue FULL!\r\n");
            }

            /* Watchdog 타이머 리셋 (10초 연장) */
            xTimerReset(xWatchdogTimer, pdMS_TO_TICKS(50));
        }
    }
}


/*===========================================================================*/
/*  Task: Logger (우선순위 1 - 최저)                                         */
/*                                                                           */
/*  - Log Queue에서 데이터 수신하여 UART 출력                                */
/*  - LED 패턴 제어 (시스템 상태에 따라)                                    */
/*  - 타이머로부터 주기적 상태 리포트 출력                                   */
/*===========================================================================*/
void Task_Logger(void const *pvParameters)
{
    (void)pvParameters;
    uint32_t last_sample_count = 0;

    printf("[LOG] Logger started.\r\n");
    printf("\r\n");
    printf("=========================================\r\n");
    printf("  RTOS Multi-Sensor DAQ System\r\n");
    printf("  Press button or send 's' to start\r\n");
    printf("=========================================\r\n");
    printf("\r\n");

    for (;;)
    {
        /*
         * Log Queue와 Notification을 동시에 기다림
         * - Log Queue: 센서 데이터
         * - Notification: 상태 리포트 요청
         *
         * xQueueReceive() 타임아웃을 100ms로 설정하여
         * 정기적으로 LED 업데이트 가능
         */
        SensorData_t data;
        if (xQueueReceive(xLogQueue, &data, pdMS_TO_TICKS(100)) == pdPASS)
        {
            /* 센서 데이터 포맷 출력 */
            const char *status_str = "";
            switch (data.status)
            {
                case 0: status_str = "NORMAL"; break;
                case 1: status_str = "HIGH";   break;
                case 2: status_str = "LOW";    break;
                case 3: status_str = "NOISE";  break;
            }

            printf("[S%d] RAW=%4u AVG=%4u TS=%lu [%s]\r\n",
                   data.sensor_id,
                   data.raw_value,
                   data.filtered_value,
                   (unsigned long)data.timestamp,
                   status_str);
        }

        /* Notification 확인 (상태 리포트 요청) */
        uint32_t notify = ulTaskNotifyTake(pdFALSE, 0);
        if (notify > 0)
        {
            /* 시스템 상태 리포트 */
            printf("\r\n");
            printf("===== SYSTEM STATUS =====\r\n");
            printf("  State:       %s\r\n",
                   g_sysState == SYS_IDLE    ? "IDLE" :
                   g_sysState == SYS_RUNNING ? "RUNNING" :
                   g_sysState == SYS_ERROR   ? "ERROR" : "UNKNOWN");
            printf("  Samples:     %lu\r\n", (unsigned long)g_sampleCount);
            printf("  Errors:      %lu\r\n", (unsigned long)g_errorCount);
            printf("  Heap free:   %u bytes\r\n",
                   (unsigned int)xPortGetFreeHeapSize());
            printf("  Sensor queue: %u\r\n",
                   (unsigned int)uxQueueMessagesWaiting(xSensorQueue));
            printf("=========================\r\n");
            printf("\r\n");
        }

        /* LED 패턴 업데이트 */
        switch (g_sysState)
        {
            case SYS_IDLE:
                HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
                break;

            case SYS_RUNNING:
                /* 500ms Blink */
                HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
                break;

            case SYS_ERROR:
                /* 100ms Fast Blink (pull in Error_Handler path) */
                HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
                /* 로거 자체가 빠르게 토글할 수 없으므로 플래그 설정 */
                break;

            default:
                break;
        }
    }
}


/*===========================================================================*/
/*  Task: UART Command Handler (UART 명령어 수신)                            */
/*===========================================================================*/
void Task_UARTCmd(void const *pvParameters)
{
    (void)pvParameters;
    char line[32];
    int idx = 0;

    /* UART RX 인터럽트 시작 */
    memset(line, 0, sizeof(line));
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    printf("> ");

    for (;;)
    {
        uint8_t ch;
        if (xQueueReceive(xUartCmdQueue, &ch, portMAX_DELAY) == pdPASS)
        {
            /* Echo */
            HAL_UART_Transmit(&huart2, &ch, 1, 0);

            if (ch == '\r' || ch == '\n')
            {
                if (idx > 0)
                {
                    line[idx] = '\0';
                    printf("\r\n");

                    switch (line[0])
                    {
                        case 's':
                        case 'S':
                            /* Start: ButtonCtrl Task에 신호 */
                            xSemaphoreGive(xButtonSem);
                            break;

                        case 'p':
                        case 'P':
                            /* Pause: ButtonCtrl Task에 신호 */
                            xSemaphoreGive(xButtonSem);
                            break;

                        case '?':
                            /* Status: Logger Task에 Notify */
                            if (xLoggerTaskHandle != NULL)
                                xTaskNotifyGive(xLoggerTaskHandle);
                            break;

                        default:
                            printf("[CMD] Unknown: s=start, p=pause, ?=status\r\n");
                            break;
                    }

                    idx = 0;
                    memset(line, 0, sizeof(line));
                }
                printf("\r\n> ");
            }
            else if (idx < (int)sizeof(line) - 1)
            {
                line[idx++] = (char)ch;
            }
        }
    }
}


/*===========================================================================*/
/*  시스템 Hook 함수                                                          */
/*===========================================================================*/

void vApplicationIdleHook(void)
{
    /*
     * 실제 제품에서는 Idle Hook에서 저전력 모드 진입 (WFI 등)
     * configUSE_TICKLESS_IDLE 활성화 시 자동으로 처리됨
     */
    static uint32_t cnt = 0;
    cnt++;
}

void vApplicationMallocFailedHook(void)
{
    printf("\r\n[FATAL] Heap exhausted! malloc failed.\r\n");
    g_sysState = SYS_ERROR;
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("\r\n[FATAL] Stack overflow in '%s'!\r\n", pcTaskName);
    g_sysState = SYS_ERROR;
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
 * @brief  main() - 시스템 시작
 *
 * 1. HAL 및 주변장치 초기화
 * 2. 동기화 객체 생성 (Semaphore, Queue, Mutex, Timer)
 * 3. 태스크 생성
 * 4. 스케줄러 시작
 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* 시스템 시작 메시지 */
    printf("\r\n");
    printf("╔═══════════════════════════════════════════╗\r\n");
    printf("║  RTOS Study - Stage 5: Real World App    ║\r\n");
    printf("║  Multi-Sensor Data Acquisition System    ║\r\n");
    printf("║  STM32F103 NUCLEO + FreeRTOS             ║\r\n");
    printf("╚═══════════════════════════════════════════╝\r\n");
    printf("\r\n");
    printf("[INIT] Creating synchronization objects...\r\n");

    /* ===== 동기화 객체 생성 ===== */

    /* 버튼 Semaphore (초기값 0) */
    xButtonSem = xSemaphoreCreateBinary();
    if (xButtonSem == NULL) { printf("[FAIL] Button sem\r\n"); Error_Handler(); }

    /* UART Mutex */
    xUartMutex = xSemaphoreCreateMutex();
    if (xUartMutex == NULL) { printf("[FAIL] UART mutex\r\n"); Error_Handler(); }

    /* 센서 데이터 Queue (16개 항목, 각 SensorData_t 크기) */
    xSensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorData_t));
    if (xSensorQueue == NULL) { printf("[FAIL] Sensor queue\r\n"); Error_Handler(); }

    /* 로그 Queue (8개 항목) */
    xLogQueue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(SensorData_t));
    if (xLogQueue == NULL) { printf("[FAIL] Log queue\r\n"); Error_Handler(); }

    /* UART 명령어 Queue */
    xUartCmdQueue = xQueueCreate(16, sizeof(uint8_t));
    if (xUartCmdQueue == NULL) { printf("[FAIL] UART cmd queue\r\n"); Error_Handler(); }

    /* ===== Software Timer 생성 ===== */

    /*
     * Timer Note:
     * - Timer callback은 Timer Daemon Task 컨텍스트에서 실행
     * - 콜백 내에서는 vTaskDelay() 등의 Blocking API 사용 불가
     * - 짧은 처리만 수행하고, 긴 작업은 Task Notification 등으로 위임
     */

    /* 상태 리포트 타이머 (5초, Auto-reload) */
    xStatusTimer = xTimerCreate(
        "StatusTimer",
        pdMS_TO_TICKS(STATUS_REPORT_INTERVAL_MS),
        pdTRUE,                /* Auto-reload */
        (void *)0,
        vStatusReportCallback
    );
    if (xStatusTimer == NULL) { printf("[FAIL] Status timer\r\n"); Error_Handler(); }

    /* Watchdog 타이머 (10초, One-shot, 리셋하며 계속 사용) */
    xWatchdogTimer = xTimerCreate(
        "Watchdog",
        pdMS_TO_TICKS(10000),
        pdFALSE,               /* One-shot */
        (void *)0,
        vWatchdogCallback
    );
    if (xWatchdogTimer == NULL) { printf("[FAIL] Watchdog timer\r\n"); Error_Handler(); }

    /* ===== 태스크 생성 ===== */

    printf("[INIT] Creating tasks...\r\n");

    /*
     * 우선순위 전략:
     * - ButtonCtrl (prio=3): 사용자 입력 → 빠른 응답 필요
     * - SensorAcq (prio=2): 정밀 주기 유지 (vTaskDelayUntil)
     * - DataProc (prio=2): SensorAcq와 동일, 데이터 흐름 유지
     * - Logger (prio=1): 출력이 지연되어도 시스템에 영향 없음
     * - UARTCmd (prio=1): 명령어 처리, 느려도 무방
     */

    xTaskCreate(Task_ButtonCtrl, "BtnCtrl",
                configMINIMAL_STACK_SIZE, NULL, 3, NULL);

    xTaskCreate(Task_SensorAcq, "SensorAcq",
                configMINIMAL_STACK_SIZE + 64, NULL, 2, &xSensorTaskHandle);

    xTaskCreate(Task_DataProc, "DataProc",
                configMINIMAL_STACK_SIZE + 64, NULL, 2, NULL);

    xTaskCreate(Task_Logger, "Logger",
                configMINIMAL_STACK_SIZE + 128, NULL, 1, &xLoggerTaskHandle);

    xTaskCreate(Task_UARTCmd, "UARTCmd",
                configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    /* Sensor Task는 처음에 Suspend (IDLE 상태) */
    vTaskSuspend(xSensorTaskHandle);

    /* ===== 타이머 시작 ===== */

    /* 상태 리포트 타이머 (항상 동작) */
    if (xTimerStart(xStatusTimer, pdMS_TO_TICKS(100)) != pdPASS)
    {
        printf("[FAIL] Status timer start\r\n");
        Error_Handler();
    }

    printf("[INIT] All objects created. Starting scheduler...\r\n");
    printf("\r\n");

    /* ===== 스케줄러 시작 ===== */
    vTaskStartScheduler();

    /* 여기는 도달하지 않음 (스케줄러 실패 시) */
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

    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
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

    /* PC13 = Button + EXTI (Falling Edge) */
    GPIO_InitStruct.Pin = BTN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
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
    printf("[FATAL] System Error! LED fast blink.\r\n");
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
