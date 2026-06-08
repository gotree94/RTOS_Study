# P03 · 멀티채널 데이터 로거

> STM32F103 + FreeRTOS 기반 고속 ADC 샘플링 + FATFS SD카드 + UART 스트리밍  
> **핵심 RTOS 개념 : `StreamBuffer` · `DMA Transfer Notify` · `Priority Inversion` · `vTaskDelayUntil`**

---

## 📌 프로젝트 개요

조이스틱·ADC 다중 채널을 **고속 DMA 샘플링**하여 FATFS로 microSD에 CSV 기록하고,  
UART로 PC에 **실시간 스트리밍**을 동시에 처리합니다.  
SPI Flash를 **링버퍼 캐시**로 활용하여 SD 쓰기 지연을 극복하고,  
RTC 기반 파일명 자동 생성으로 전문적인 **계측 장비 구조**를 구현합니다.

### 학습 목표

- `xStreamBufferSend / Receive` 로 **고속 ADC → 저속 SD 속도 차이**를 완충한다
- **DMA Transfer Complete ISR → 태스크 알림** (`xTaskNotifyFromISR`) 패턴을 이해한다
- **Priority Inversion** 발생 조건과 `Mutex Priority Inheritance` 해결책을 실습한다
- `f_write` 블로킹 처리 전략 — SD 쓰기 지연이 다른 태스크에 미치는 영향을 분석한다
- `xStreamBuffer` 와 `xMessageBuffer` 의 차이점과 용도를 구분한다

---

## 🔧 하드웨어 구성

### 부품 목록 (BOM)

| 부품 | 모델 | 인터페이스 | 수량 |
|------|------|-----------|------|
| MCU 보드 | STM32F103C8T6 (Blue Pill) | - | 1 |
| 조이스틱 | 아날로그 2축 조이스틱 | ADC | 1 |
| 가변저항 | 10kΩ 멀티턴 | ADC | 2 |
| microSD 어댑터 | NS-SD04 / Arduino Micro SD | SPI | 1 |
| microSD 카드 | Class 10 4GB~32GB | - | 1 |
| SPI Flash | Winbond W25Q64 (8MB) | SPI | 1 |
| CLCD | 16×2 LCD + PCF8574 I2C 어댑터 | I2C | 1 |
| RTC 모듈 | DS1307 / STM32 내장 RTC | I2C / 내장 | 1 |
| USB-UART | CH340 / CP2102 | UART | 1 |

### ADC 채널 구성

| 채널 | 핀 | 신호 | 샘플링 용도 |
|------|-----|------|------------|
| ADC1_CH0 | PA0 | Joystick X | 조이스틱 X축 |
| ADC1_CH1 | PA1 | Joystick Y | 조이스틱 Y축 |
| ADC1_CH2 | PA2 | POT1 | 가변저항 1 |
| ADC1_CH3 | PA3 | POT2 | 가변저항 2 |
| ADC1_CH16 | 내부 | VSENSE | 내부 온도 센서 |
| ADC1_CH17 | 내부 | VREFINT | 기준전압 |

> STM32F103 ADC1 스캔 모드 + DMA 채널1 사용 — 6채널 연속 자동 변환

### 핀 배치

```
STM32F103C8T6
┌──────────────────────────────────────────────┐
│  PA0~PA3 ── ADC1 CH0~CH3 (JoyStick, POT)    │
│                                              │
│  PA9   ── USART1 TX → PC (115200 bps)        │
│  PA10  ── USART1 RX ← PC                    │
│                                              │
│  PB3   ── SPI1 SCK  (SD + Flash 공유)        │
│  PB4   ── SPI1 MISO (SD + Flash 공유)        │
│  PB5   ── SPI1 MOSI (SD + Flash 공유)        │
│  PB6   ── SD Card CS  (GPIO)                 │
│  PB7   ── Flash W25Q64 CS (GPIO)             │
│                                              │
│  PB8   ── I2C1 SCL (CLCD PCF8574, DS1307)   │
│  PB9   ── I2C1 SDA (CLCD PCF8574, DS1307)   │
│                                              │
│  PA4   ── Joystick SW (GPIO, EXTI4)          │
└──────────────────────────────────────────────┘
```

### 회로 연결 요약

```
SPI1 버스 (공유) ──┬── SD Card  (CS: PB6)
                  └── W25Q64  (CS: PB7)
※ CS 신호로 디바이스 선택 — 동시 접근 방지를 위해 xMutex_SPI 필수

I2C1 버스 (공유) ──┬── PCF8574 (CLCD, 0x27)
                  └── DS1307  (RTC,  0x68)
```

---

## 🗂️ 프로젝트 구조

```
P03_DataLogger/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── adc_task.h          ← DMA ADC 샘플링 태스크
│   │   ├── cache_task.h        ← SPI Flash 링버퍼 캐시 태스크
│   │   ├── sd_write_task.h     ← FATFS SD 쓰기 태스크
│   │   ├── uart_stream_task.h  ← PC 실시간 스트리밍 태스크
│   │   ├── ui_task.h           ← CLCD + JoyStick 메뉴 태스크
│   │   └── rtos_objects.h      ← 전역 RTOS 핸들 선언
│   └── Src/
│       ├── main.c
│       ├── adc_task.c
│       ├── cache_task.c
│       ├── sd_write_task.c
│       ├── uart_stream_task.c
│       ├── ui_task.c
│       └── rtos_objects.c
├── Middlewares/
│   └── Third_Party/
│       └── FatFs/              ← ST FatFs (FATFS 미들웨어)
├── Drivers/
│   ├── W25Q64/                 ← SPI Flash 드라이버
│   └── PCF8574_LCD/            ← I2C CLCD 드라이버
├── .ioc
└── README.md
```

---

## 🔄 FreeRTOS 태스크 설계

### 태스크 구성표

| 태스크 이름 | 우선순위 | 스택 (words) | 주기 / 트리거 | 역할 |
|------------|---------|-------------|-------------|------|
| `ADCTask` | 4 (최고) | 256 | DMA 완료 알림 | 6채널 DMA ADC 수집 |
| `CacheTask` | 3 | 384 | StreamBuffer 구동 | W25Q64 Flash 링버퍼 관리 |
| `UARTStreamTask` | 3 | 256 | 10ms | PC 실시간 전송 |
| `SDWriteTask` | 2 | 512 | 배치 트리거 | FATFS CSV 배치 쓰기 |
| `UITask` | 1 | 256 | 100ms | CLCD + JoyStick 메뉴 |

> **속도 계층 구조**  
> ADC 샘플링(1kHz) → StreamBuffer → UART 스트리밍(100Hz) → SD 배치 쓰기(1Hz)

### 태스크 간 통신 구조

```
┌──────────────────────────────────────────────────────────┐
│   ADC DMA (1kHz)                                         │
│   HAL_ADC_ConvCpltCallback()                             │
│           │                                              │
│           │ xTaskNotifyFromISR()                         │
│           ▼                                              │
│   ┌──────────────┐   xStreamBufferSend()                 │
│   │   ADCTask    │──────────────────────▶ xStreamBuf_ADC │
│   │  (Priority 4)│                              │        │
│   └──────────────┘                              │        │
│                                                 │        │
│        ┌────────────────────────────────────────┘        │
│        │ xStreamBufferReceive()                          │
│        ▼                                                 │
│   ┌──────────────┐   SPI Flash 링버퍼 기록               │
│   │  CacheTask   │──────────────────────▶ W25Q64         │
│   │  (Priority 3)│   [xMutex_SPI]                        │
│   └──────┬───────┘                                       │
│          │ xQueueSend() (배치 완성 알림)                  │
│          ▼                                               │
│   ┌──────────────┐   f_write() → microSD                 │
│   │ SDWriteTask  │──────────────────────▶ FATFS          │
│   │  (Priority 2)│   [xMutex_SPI]                        │
│   └──────────────┘                                       │
│                                                          │
│   ┌──────────────┐   xStreamBufferReceive()              │
│   │UARTStreamTask│◀──────────── xStreamBuf_ADC (peek)   │
│   │  (Priority 3)│   HAL_UART_Transmit_DMA()             │
│   └──────────────┘                                       │
│                                                          │
│   ┌──────────────┐   xQueueReceive()                     │
│   │   UITask     │◀────────── xQueue_LogStatus           │
│   │  (Priority 1)│   CLCD 상태 표시                      │
│   └──────────────┘                                       │
└──────────────────────────────────────────────────────────┘
```

### RTOS 오브젝트 목록

```c
/* rtos_objects.h */

/* Stream Buffers */
extern StreamBufferHandle_t xStreamBuf_ADC;    // ADCTask → Cache/UART (고속)
extern StreamBufferHandle_t xStreamBuf_UART;   // Cache → UARTStreamTask

/* Queues */
extern QueueHandle_t xQueue_SDWrite;    // CacheTask → SDWriteTask (배치 단위)
extern QueueHandle_t xQueue_LogStatus; // SDWriteTask → UITask (상태)

/* Mutex */
extern SemaphoreHandle_t xMutex_SPI;   // SPI1 버스 공유 보호 (SD + Flash)
extern SemaphoreHandle_t xMutex_I2C;   // I2C1 버스 공유 보호 (CLCD + RTC)

/* Task Handle */
extern TaskHandle_t xHandle_ADCTask;   // DMA ISR → ADCTask 알림용

/* 로그 배치 구조체 */
#define ADC_CHANNELS       6
#define BATCH_SIZE         100    // 100샘플 = 100ms 분량

typedef struct {
    uint16_t adc[ADC_CHANNELS];   // 6채널 ADC 원시값
    uint32_t timestamp_ms;        // HAL_GetTick()
} ADCSample_t;                    // 16bytes

typedef struct {
    ADCSample_t samples[BATCH_SIZE];
    uint32_t    count;
    uint32_t    file_seq;         // 파일 시퀀스 번호
} SDBatch_t;                      // 1616bytes
```

---

## 💻 핵심 코드

### 1. RTOS 오브젝트 생성 (`main.c`)

```c
void MX_FREERTOS_Init(void)
{
    /* StreamBuffer - ADC 고속 스트림
     * 크기: ADCSample_t × 200샘플 = 3200bytes
     * 트리거: ADCSample_t 1개(16bytes) 이상 수신 시 깨움 */
    xStreamBuf_ADC = xStreamBufferCreate(
                        sizeof(ADCSample_t) * 200,  // 버퍼 크기
                        sizeof(ADCSample_t));        // 트리거 레벨

    /* StreamBuffer - UART 출력 스트림 */
    xStreamBuf_UART = xStreamBufferCreate(512, 1);

    /* Queue - SD 배치 쓰기 트리거 */
    xQueue_SDWrite   = xQueueCreate(4, sizeof(SDBatch_t));
    xQueue_LogStatus = xQueueCreate(4, sizeof(LogStatus_t));

    /* Mutex */
    xMutex_SPI = xSemaphoreCreateMutex();
    xMutex_I2C = xSemaphoreCreateMutex();

    /* 태스크 생성 */
    xTaskCreate(ADCTask,        "ADC",      256, NULL, 4, &xHandle_ADCTask);
    xTaskCreate(CacheTask,      "Cache",    384, NULL, 3, NULL);
    xTaskCreate(UARTStreamTask, "UART",     256, NULL, 3, NULL);
    xTaskCreate(SDWriteTask,    "SDWrite",  512, NULL, 2, NULL);
    xTaskCreate(UITask,         "UI",       256, NULL, 1, NULL);
}
```

### 2. ADCTask — DMA 완료 ISR → 태스크 알림

```c
/* DMA Transfer Complete 콜백 (ISR 컨텍스트) */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        /* ISR에서 태스크 알림 전송 - FromISR 버전 필수 */
        vTaskNotifyGiveFromISR(xHandle_ADCTask, &xHigherPriorityTaskWoken);

        /* 더 높은 우선순위 태스크가 깨어나면 즉시 컨텍스트 스위치 */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* DMA 버퍼 (double buffer) */
static uint16_t adc_dma_buf[ADC_CHANNELS * 2];  // Ping-Pong 버퍼

void ADCTask(void *pvParameters)
{
    ADCSample_t sample;
    uint8_t     buf_sel = 0;   // 0 or 1 (더블 버퍼 선택)

    /* ADC DMA 연속 변환 시작 */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buf, ADC_CHANNELS * 2);

    for (;;)
    {
        /* DMA 완료 ISR 알림 대기 (블로킹) */
        ulTaskNotifyTake(pdTRUE,           // 알림 카운트 클리어
                         portMAX_DELAY);   // 무한 대기

        /* 현재 활성 버퍼 선택 (더블 버퍼링) */
        buf_sel ^= 1;
        uint16_t *active_buf = &adc_dma_buf[buf_sel * ADC_CHANNELS];

        /* 샘플 구성 */
        for (int i = 0; i < ADC_CHANNELS; i++)
            sample.adc[i] = active_buf[i];
        sample.timestamp_ms = HAL_GetTick();

        /* StreamBuffer에 전송 (비블로킹 - ISR과 유사) */
        xStreamBufferSend(xStreamBuf_ADC,
                          &sample,
                          sizeof(ADCSample_t),
                          0);   // 타임아웃 0 = 즉시 반환
    }
}
```

### 3. CacheTask — StreamBuffer 수신 + Flash 링버퍼

```c
/*
 * Flash 링버퍼 구조
 * W25Q64 (8MB) = 4096 섹터 × 4096bytes
 * 섹터 0~3071 : 데이터 링버퍼 (96MB 분량 → 3072 × 4096 = ~12MB)
 * 섹터 4090   : 링버퍼 헤드/테일 포인터 저장
 */
#define FLASH_DATA_SECTORS    3072
#define FLASH_SECTOR_SIZE     4096
#define SAMPLES_PER_SECTOR    (FLASH_SECTOR_SIZE / sizeof(ADCSample_t))  // 256샘플

void CacheTask(void *pvParameters)
{
    ADCSample_t  sample;
    SDBatch_t    batch;
    uint32_t     flash_sector = 0;
    uint32_t     batch_count  = 0;
    size_t       received;

    batch.count    = 0;
    batch.file_seq = 0;

    for (;;)
    {
        /* StreamBuffer에서 ADC 샘플 수신 (트리거: 16bytes = 1샘플) */
        received = xStreamBufferReceive(
                        xStreamBuf_ADC,
                        &sample,
                        sizeof(ADCSample_t),
                        pdMS_TO_TICKS(100));   // 100ms 타임아웃

        if (received == sizeof(ADCSample_t))
        {
            /* 배치 버퍼에 누적 */
            batch.samples[batch.count++] = sample;

            /* SPI Flash에 캐시 기록 (SPI Mutex 보호) */
            if (xSemaphoreTake(xMutex_SPI, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                W25Q64_WritePage(flash_sector, batch.count - 1, &sample);
                xSemaphoreGive(xMutex_SPI);
            }

            /* BATCH_SIZE(100샘플) 모이면 SD 쓰기 태스크에 전달 */
            if (batch.count >= BATCH_SIZE)
            {
                /* Queue 전송 - SDWriteTask가 소비 */
                if (xQueueSend(xQueue_SDWrite, &batch,
                               pdMS_TO_TICKS(50)) != pdTRUE)
                {
                    /* Queue 가득 참 - Flash 백업으로 유실 방지 */
                    W25Q64_MarkOverflow(flash_sector);
                }

                batch.count = 0;
                batch.file_seq++;
                flash_sector = (flash_sector + 1) % FLASH_DATA_SECTORS;
            }
        }
    }
}
```

### 4. SDWriteTask — FATFS 배치 쓰기 + Priority Inversion 주의

```c
/*
 * Priority Inversion 시나리오 (주의!)
 *
 * SDWriteTask(P2)가 xMutex_SPI 보유 중 f_write() 블로킹
 *   → CacheTask(P3)가 xMutex_SPI 대기
 *   → ADCTask(P4)는 정상 실행
 *   → UITask(P1)가 xMutex_I2C 보유 중 CLCD 업데이트 블로킹
 *      → SDWriteTask가 UITask 완료를 기다리는 구조 없음
 *
 * FreeRTOS Mutex는 Priority Inheritance 자동 지원
 * → SDWriteTask가 Mutex 보유 시 일시적으로 P3으로 승격
 * → CacheTask 블로킹 시간 최소화
 */

void SDWriteTask(void *pvParameters)
{
    SDBatch_t   batch;
    FIL         file;
    FRESULT     fr;
    char        filename[32];
    char        csv_line[128];
    LogStatus_t status;
    uint32_t    total_written = 0;

    /* SD 마운트 */
    if (f_mount(&fs, "", 1) != FR_OK)
    {
        status.sd_mounted = 0;
        xQueueSend(xQueue_LogStatus, &status, 0);
        vTaskDelete(NULL);   // SD 없으면 태스크 종료
        return;
    }

    /* RTC에서 파일명 생성: LOG_YYYYMMDD_HHMMSS.csv */
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    snprintf(filename, sizeof(filename),
             "LOG_%02d%02d%02d_%02d%02d%02d.csv",
             d.Year, d.Month, d.Date,
             t.Hours, t.Minutes, t.Seconds);

    /* CSV 헤더 쓰기 */
    if (xSemaphoreTake(xMutex_SPI, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
        f_puts("timestamp_ms,JoyX,JoyY,POT1,POT2,TEMP,VREF\r\n", &file);
        f_sync(&file);
        xSemaphoreGive(xMutex_SPI);
    }

    for (;;)
    {
        /* 배치 수신 대기 (최대 2초) */
        if (xQueueReceive(xQueue_SDWrite, &batch,
                          pdMS_TO_TICKS(2000)) == pdTRUE)
        {
            /* SPI Mutex 획득 후 배치 쓰기 */
            if (xSemaphoreTake(xMutex_SPI, pdMS_TO_TICKS(500)) == pdTRUE)
            {
                UINT bw;
                for (uint32_t i = 0; i < batch.count; i++)
                {
                    snprintf(csv_line, sizeof(csv_line),
                             "%lu,%u,%u,%u,%u,%u,%u\r\n",
                             batch.samples[i].timestamp_ms,
                             batch.samples[i].adc[0],
                             batch.samples[i].adc[1],
                             batch.samples[i].adc[2],
                             batch.samples[i].adc[3],
                             batch.samples[i].adc[4],
                             batch.samples[i].adc[5]);
                    f_write(&file, csv_line, strlen(csv_line), &bw);
                    total_written += bw;
                }
                f_sync(&file);   // 버퍼 플러시 (데이터 보호)
                xSemaphoreGive(xMutex_SPI);
            }

            /* 상태 업데이트 */
            status.sd_mounted   = 1;
            status.bytes_written = total_written;
            status.batch_count   = batch.file_seq;
            xQueueOverwrite(xQueue_LogStatus, &status);
        }
    }
}
```

### 5. UARTStreamTask — PC 실시간 스트리밍

```c
/*
 * StreamBuffer vs Queue 선택 이유
 * - ADC 데이터는 고정 크기(16bytes) 연속 스트림
 * - xStreamBuffer: 바이트 단위 연속 데이터, 오버헤드 최소
 * - xMessageBuffer: 가변 길이 메시지 (길이 헤더 포함) → 여기서는 불필요
 */

void UARTStreamTask(void *pvParameters)
{
    ADCSample_t  sample;
    char         tx_buf[96];
    TickType_t   xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        /* StreamBuffer peek (소비하지 않고 읽기 - CacheTask도 소비함) */
        /* 별도 복사본: ADCTask가 xStreamBuf_ADC에 쓸 때 UART용 별도 큐 활용 */
        if (xStreamBufferReceive(xStreamBuf_UART,
                                 &sample,
                                 sizeof(ADCSample_t),
                                 pdMS_TO_TICKS(15)) == sizeof(ADCSample_t))
        {
            int len = snprintf(tx_buf, sizeof(tx_buf),
                               "$%lu,%u,%u,%u,%u,%u,%u\r\n",
                               sample.timestamp_ms,
                               sample.adc[0], sample.adc[1],
                               sample.adc[2], sample.adc[3],
                               sample.adc[4], sample.adc[5]);

            /* DMA UART 전송 (논블로킹) */
            HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_buf, len);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));   // 100Hz 스트리밍
    }
}
```

### 6. UITask — CLCD 메뉴 + JoyStick 조작

```c
typedef enum {
    UI_SCREEN_MAIN   = 0,   // 메인 상태 화면
    UI_SCREEN_ADC    = 1,   // ADC 실시간 값
    UI_SCREEN_SD     = 2,   // SD 기록 상태
    UI_SCREEN_CONFIG = 3,   // 샘플링 속도 설정
} UIScreen_t;

void UITask(void *pvParameters)
{
    LogStatus_t  status;
    UIScreen_t   screen     = UI_SCREEN_MAIN;
    uint16_t     joy_x_prev = 2048;
    TickType_t   xLastWakeTime = xTaskGetTickCount();
    char         line1[17], line2[17];

    LCD_Init();
    LCD_Clear();

    for (;;)
    {
        /* JoyStick SW 버튼 → 화면 전환 */
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET)
        {
            screen = (UIScreen_t)((screen + 1) % 4);
            LCD_Clear();
            vTaskDelay(pdMS_TO_TICKS(200));   // 디바운스
        }

        /* 최신 로그 상태 수신 */
        xQueuePeek(xQueue_LogStatus, &status, 0);

        /* I2C Mutex 획득 후 CLCD 업데이트 */
        if (xSemaphoreTake(xMutex_I2C, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            switch (screen)
            {
                case UI_SCREEN_MAIN:
                    snprintf(line1, 17, "DataLogger v1.0 ");
                    snprintf(line2, 17, "SD:%s Bat:%u   ",
                             status.sd_mounted ? "OK" : "--",
                             status.batch_count);
                    break;

                case UI_SCREEN_ADC:
                    /* ADC 실시간값은 StreamBuf에서 직접 읽지 않고
                     * 별도 전역 변수로 공유 (atomic 접근) */
                    snprintf(line1, 17, "JX:%4u JY:%4u ", gAdcLast[0], gAdcLast[1]);
                    snprintf(line2, 17, "P1:%4u P2:%4u ", gAdcLast[2], gAdcLast[3]);
                    break;

                case UI_SCREEN_SD:
                    snprintf(line1, 17, "Written:%6luKB", status.bytes_written / 1024);
                    snprintf(line2, 17, "Batches:%7lu  ", status.batch_count);
                    break;

                case UI_SCREEN_CONFIG:
                    snprintf(line1, 17, "SampleRate:1kHz ");
                    snprintf(line2, 17, "BatchSz: %4u   ", BATCH_SIZE);
                    break;
            }

            LCD_SetCursor(0, 0); LCD_Print(line1);
            LCD_SetCursor(1, 0); LCD_Print(line2);
            xSemaphoreGive(xMutex_I2C);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
```

---

## 📊 시스템 동작 흐름

### 데이터 파이프라인

```
ADC DMA (1kHz = 1ms 간격)
    │
    │ DMA Complete ISR
    │ vTaskNotifyGiveFromISR()
    ▼
ADCTask (Priority 4)
    │ xStreamBufferSend() ×2 (ADC + UART 각각)
    ▼
    ├──▶ xStreamBuf_ADC ──▶ CacheTask ──▶ W25Q64 Flash (링버퍼)
    │                              └──▶ xQueue_SDWrite ──▶ SDWriteTask ──▶ SD CSV
    │
    └──▶ xStreamBuf_UART ──▶ UARTStreamTask ──▶ PC (100Hz)

UITask (100ms) ─── xQueue_LogStatus ◀── SDWriteTask
    └──▶ CLCD 상태 표시
```

### 생성 파일 예시

```
LOG_250601_143022.csv

timestamp_ms,JoyX,JoyY,POT1,POT2,TEMP,VREF
1000,2048,2050,1024,3200,1850,1500
1001,2049,2048,1025,3198,1851,1500
1002,2050,2051,1024,3200,1850,1501
...
```

### UART 스트리밍 출력 예시 (PC 수신)

```
$1000,2048,2050,1024,3200,1850,1500
$1010,2049,2048,1025,3198,1851,1500
$1020,2052,2047,1026,3201,1849,1500
```

> `$` 헤더로 PC 측 Python/SerialPlot 에서 실시간 파싱 가능

---

## ⚠️ 주요 설계 고려사항

### StreamBuffer 크기 계산

```
ADC 샘플 크기     : 16 bytes (ADCSample_t)
샘플링 속도       : 1,000 samples/sec
SD 쓰기 최대 지연 : 500ms (FAT 클러스터 할당 등)

필요 버퍼 크기 = 16 bytes × 1000 samples/s × 0.5s = 8,000 bytes

→ xStreamBuf_ADC 크기: sizeof(ADCSample_t) × 200 = 3,200 bytes (여유분)
※ STM32F103 SRAM 20KB 한계 고려 — 실제 지연 허용치 200ms로 설계
```

### Priority Inversion 이해

```
시나리오:
  SDWriteTask (P2) ─── xMutex_SPI 보유 ─── f_write() 수행 중
  CacheTask   (P3) ─── xMutex_SPI 대기 (블로킹)
  ADCTask     (P4) ─── StreamBuffer 전송 (정상)
  UITask      (P1) ─── 실행 중

문제: SDWriteTask(P2)가 UITask(P1)에 선점당해 Mutex 해제 지연
     → CacheTask(P3)가 P2보다 낮은 P1(UITask) 때문에 간접 차단됨
     = Priority Inversion!

FreeRTOS 해결책:
  xSemaphoreCreateMutex() → Priority Inheritance 자동 적용
  → SDWriteTask가 Mutex 보유 중 일시적으로 P3으로 우선순위 승격
  → UITask(P1)에 선점되지 않음
  → CacheTask 블로킹 시간 최소화
```

### f_sync() 주기 설계

```c
/* f_sync() 없이 f_write()만 하면 버퍼에만 쓰임 */
/* 전원 차단 시 마지막 데이터 유실 가능 */

/* 방법 1: 매 배치마다 sync (안전, 느림) */
f_write(&file, data, len, &bw);
f_sync(&file);   // 매 100ms마다 호출

/* 방법 2: 일정 바이트마다 sync (균형) */
total_written += bw;
if (total_written % (64 * 1024) == 0)   // 64KB마다
    f_sync(&file);

/* 방법 3: JoyStick 버튼으로 수동 sync */
// UITask에서 SW 버튼 길게 누를 시 f_sync() 호출
```

---

## 🧪 실습 과제

### 기본 과제

1. `xStreamBufferCreate` 크기를 절반으로 줄이고 ADC 오버플로 발생 시점을 관찰한다
2. `xMutex_SPI`를 제거하고 SD 쓰기 중 Flash 동시 접근 시 SPI 충돌을 확인한다
3. UART 스트리밍 데이터를 PC에서 Python + Matplotlib으로 실시간 그래프로 표시한다

### 심화 과제

4. Priority Inversion을 의도적으로 재현한다: `xSemaphoreCreateBinary()`로 Mutex를 대체하고 `configUSE_MUTEXES = 0` 설정 후 동작 차이를 분석한다
5. ADC 샘플링 속도를 `vTaskDelayUntil`로 100Hz / 500Hz / 1kHz로 변경하며 StreamBuffer 충진율을 `xStreamBufferBytesAvailable()`로 측정한다
6. SD 파일을 닫지 않고 전원을 강제 차단한 후 파일 복구 여부를 확인하고 `f_sync()` 주기 설계의 중요성을 분석한다

---

## 🔍 트러블슈팅

| 증상 | 원인 | 해결 방법 |
|------|------|----------|
| SD 마운트 실패 | SPI 속도 과다 | SPI1 초기화 시 Prescaler 128로 낮춤 |
| CSV 파일 깨짐 | SPI Mutex 미적용 | `xMutex_SPI` 적용 및 Give 누락 점검 |
| StreamBuffer 넘침 | 버퍼 크기 부족 | `xStreamBuf_ADC` 크기 증가 또는 샘플링 속도 감소 |
| UART 출력 끊김 | DMA UART 충돌 | HAL_UART_Transmit_DMA 완료 대기 (`TC` 플래그) 확인 |
| f_write 후 파일 0KB | f_sync 미호출 | 배치마다 `f_sync(&file)` 추가 |
| CLCD 표시 깨짐 | I2C Mutex 누락 | `xMutex_I2C` 적용 확인 |
| 하드폴트 발생 | SDWriteTask 스택 부족 | 스택 512 → 768 words로 증가 |

---

## 📚 참고 자료

- [FreeRTOS Stream Buffer API](https://www.freertos.org/RTOS-stream-buffer-API.html)
- [FreeRTOS Message Buffer API](https://www.freertos.org/RTOS-message-buffer-API.html)
- [Priority Inversion 설명 (FreeRTOS)](https://www.freertos.org/Real-time-embedded-RTOS-mutexes.html)
- [STM32 FatFs 공식 문서](http://elm-chan.org/fsw/ff/00index_e.html)
- [W25Q64 Winbond 데이터시트](https://www.winbond.com/resource-files/w25q64jv%20revj%2003272018%20plus.pdf)
- STM32CubeIDE FatFs 미들웨어 설정: `Middleware → FATFS → User-defined`

---

*P03 · 멀티채널 데이터 로거 | 광주인력개발원 임베디드 시스템 과정*
