# P01 · 스마트 환경 모니터링 스테이션

> STM32F103 + FreeRTOS 기반 다중 센서 실시간 모니터링 시스템  
> **핵심 RTOS 개념 : `xQueue` · `xMutex` · `xEventGroup` · `vTaskDelayUntil`**

---

## 📌 프로젝트 개요

온습도·조도·거리·ToF 4종 센서 데이터를 FreeRTOS 태스크로 **동시에** 수집하고,  
ILI9341 TFT LCD에 실시간 대시보드를 표시합니다.  
RTC 타임스탬프와 함께 UART로 PC에 로그를 전송하고,  
임계값 초과 시 Buzzer와 WS2812B LED로 경보를 발생시키는 **IoT 센서 스테이션**입니다.

### 학습 목표

- `xQueueSend / xQueueReceive` 로 센서 데이터를 태스크 간 안전하게 전달한다
- `xSemaphoreCreateMutex` 로 I2C 버스 공유 자원을 보호한다
- `xEventGroupSetBits` 로 다중 조건 알람을 트리거한다
- `vTaskDelayUntil` 로 주기적 태스크의 정밀 실행 주기를 보장한다

---

## 🔧 하드웨어 구성

### 부품 목록 (BOM)

| 부품 | 모델 | 인터페이스 | 수량 |
|------|------|-----------|------|
| MCU 보드 | STM32F103C8T6 (Blue Pill) | - | 1 |
| 온습도 센서 | DHT11 | GPIO 1-Wire | 1 |
| 조도 센서 | CDS (광 저항) | ADC | 1 |
| 초음파 거리 센서 | HC-SR04 | GPIO / TIM | 1 |
| ToF 거리 센서 | VL53L0X | I2C | 1 |
| TFT LCD | ILI9341 2.8인치 (240×320) | SPI | 1 |
| RTC 모듈 | DS1307 또는 내장 RTC | I2C / 내장 | 1 |
| EEPROM | AT24C256 | I2C | 1 |
| 부저 | 능동 Buzzer | GPIO PWM | 1 |
| LED | WS2812B × 8 | GPIO DMA | 1 |
| USB-UART | CH340 / CP2102 | UART | 1 |

### 핀 배치

```
STM32F103C8T6
┌─────────────────────────────────────────┐
│  PA0  ── CDS (ADC1_CH0)                 │
│  PA1  ── HC-SR04 TRIG                   │
│  PA2  ── HC-SR04 ECHO (TIM2_CH3)        │
│  PA3  ── DHT11 DATA                     │
│  PA9  ── USART1 TX  → PC               │
│  PA10 ── USART1 RX  ← PC               │
│                                         │
│  PB5  ── WS2812B DIN (SPI1_MOSI / DMA) │
│  PB6  ── Buzzer (TIM4_CH1 PWM)         │
│                                         │
│  PB8  ── I2C1 SCL (VL53L0X, EEPROM,    │
│  PB9  ── I2C1 SDA  DS1307)             │
│                                         │
│  PB12 ── ILI9341 CS                    │
│  PB13 ── ILI9341 SCK  (SPI2)           │
│  PB14 ── ILI9341 MISO (SPI2)           │
│  PB15 ── ILI9341 MOSI (SPI2)           │
│  PC13 ── ILI9341 DC                    │
│  PC14 ── ILI9341 RST                   │
└─────────────────────────────────────────┘
```

### 회로 연결도 (텍스트)

```
VCC 3.3V ──┬── DHT11 VCC        ┬── VL53L0X VCC
           ├── CDS (10kΩ 분압)  ├── AT24C256 VCC
           └── WS2812B VCC(5V↑) └── ILI9341 VCC(3.3V)

GND ───────┬── 모든 모듈 GND
           └── ILI9341 GND

※ VL53L0X, AT24C256, DS1307 은 I2C1 버스(PB8/PB9) 병렬 연결
※ WS2812B 는 레벨 시프터(3.3V→5V) 권장
```

---

## 🗂️ 프로젝트 구조

```
P01_Smart_EnvMonitor/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── sensor_task.h       ← DHT11, CDS, HC-SR04 태스크 헤더
│   │   ├── tof_task.h          ← VL53L0X 태스크 헤더
│   │   ├── display_task.h      ← ILI9341 대시보드 헤더
│   │   ├── log_task.h          ← UART + EEPROM 로깅 헤더
│   │   ├── alarm_task.h        ← Buzzer + WS2812B 알람 헤더
│   │   └── rtos_objects.h      ← Queue/Mutex/EventGroup 핸들 선언
│   └── Src/
│       ├── main.c
│       ├── sensor_task.c
│       ├── tof_task.c
│       ├── display_task.c
│       ├── log_task.c
│       ├── alarm_task.c
│       └── rtos_objects.c
├── Drivers/
│   ├── DHT11/
│   ├── ILI9341/
│   ├── VL53L0X/
│   └── WS2812B/
├── .ioc                        ← STM32CubeMX 설정 파일
└── README.md                   ← 이 파일
```

---

## 🔄 FreeRTOS 태스크 설계

### 태스크 구성표

| 태스크 이름 | 우선순위 | 스택 (words) | 주기 | 역할 |
|------------|---------|-------------|------|------|
| `SensorTask` | 3 | 256 | 100ms | DHT11 · CDS · HC-SR04 수집 |
| `ToFTask` | 3 | 256 | 50ms | VL53L0X I2C 거리 측정 |
| `DisplayTask` | 2 | 512 | 200ms | ILI9341 대시보드 렌더링 |
| `LogTask` | 1 | 384 | 1000ms | UART 출력 + EEPROM 기록 |
| `AlarmTask` | 4 | 256 | 이벤트 구동 | Buzzer + WS2812B 알람 |

> 우선순위 : `AlarmTask` > `SensorTask` = `ToFTask` > `DisplayTask` > `LogTask`

### 태스크 간 통신 구조

```
┌──────────────┐   xQueueSend()    ┌──────────────────┐
│  SensorTask  │ ─────────────────▶│                  │
│  (DHT11/CDS/ │                   │   DisplayTask    │
│   HC-SR04)   │   xQueueSend()    │  (ILI9341 SPI)   │
└──────┬───────┘ ─────────────────▶│                  │
       │                           └──────────────────┘
       │ xEventGroupSetBits()
       ▼                           ┌──────────────────┐
┌──────────────┐                   │                  │
│  xEventGroup │ ─────────────────▶│   AlarmTask      │
│  (임계값 플래그)│  xEventGroupWait  │  (Buzzer/WS2812B)│
└──────────────┘                   └──────────────────┘

┌──────────────┐   xQueueSend()    ┌──────────────────┐
│   ToFTask    │ ─────────────────▶│                  │
│  (VL53L0X)   │  [xMutex_I2C]    │    LogTask        │
└──────────────┘  I2C 버스 보호    │  (UART + EEPROM) │
                                   └──────────────────┘
```

### RTOS 오브젝트 목록

```c
/* rtos_objects.h */

/* Queues */
extern QueueHandle_t xQueue_SensorData;   // SensorTask → Display/Log
extern QueueHandle_t xQueue_ToFData;      // ToFTask    → Display/Log

/* Mutex */
extern SemaphoreHandle_t xMutex_I2C;      // I2C1 버스 공유 보호
extern SemaphoreHandle_t xMutex_SPI;      // SPI2 버스 공유 보호

/* Event Group */
extern EventGroupHandle_t xEvt_Alarm;

/* Event Bits 정의 */
#define EVT_TEMP_HIGH     ( 1 << 0 )   // 온도 임계값 초과
#define EVT_HUMID_HIGH    ( 1 << 1 )   // 습도 임계값 초과
#define EVT_DIST_NEAR     ( 1 << 2 )   // 거리 근접 감지
#define EVT_LIGHT_LOW     ( 1 << 3 )   // 조도 임계값 이하
```

---

## 💻 핵심 코드

### 1. RTOS 오브젝트 생성 (`main.c`)

```c
void MX_FREERTOS_Init(void)
{
    /* Queue 생성 - 센서 데이터 구조체 10개 버퍼 */
    xQueue_SensorData = xQueueCreate(10, sizeof(SensorData_t));
    xQueue_ToFData    = xQueueCreate(10, sizeof(uint16_t));

    /* Mutex 생성 - I2C / SPI 버스 보호 */
    xMutex_I2C = xSemaphoreCreateMutex();
    xMutex_SPI = xSemaphoreCreateMutex();

    /* Event Group 생성 - 알람 플래그 */
    xEvt_Alarm = xEventGroupCreate();

    /* 태스크 생성 */
    xTaskCreate(SensorTask,  "Sensor",  256, NULL, 3, NULL);
    xTaskCreate(ToFTask,     "ToF",     256, NULL, 3, NULL);
    xTaskCreate(DisplayTask, "Display", 512, NULL, 2, NULL);
    xTaskCreate(LogTask,     "Log",     384, NULL, 1, NULL);
    xTaskCreate(AlarmTask,   "Alarm",   256, NULL, 4, NULL);
}
```

### 2. SensorTask — Queue 전송 + EventGroup 설정

```c
typedef struct {
    float    temperature;   // °C
    float    humidity;      // %RH
    uint16_t distance_cm;   // HC-SR04 거리 (cm)
    uint16_t lux;           // 조도 (ADC raw)
    uint32_t timestamp;     // RTC tick
} SensorData_t;

/* 임계값 정의 */
#define TEMP_THRESHOLD_HIGH    35.0f   // 온도 35°C 초과
#define HUMID_THRESHOLD_HIGH   80.0f   // 습도 80% 초과
#define DIST_THRESHOLD_NEAR    20U     // 거리 20cm 미만
#define LIGHT_THRESHOLD_LOW    300U    // 조도 ADC 300 이하

void SensorTask(void *pvParameters)
{
    SensorData_t data;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    EventBits_t alarmBits;

    for (;;)
    {
        /* 1. 센서 데이터 수집 */
        DHT11_Read(&data.temperature, &data.humidity);
        data.distance_cm = HCSR04_GetDistance();
        data.lux         = ADC_ReadChannel(ADC_CHANNEL_CDS);
        data.timestamp   = HAL_GetTick();

        /* 2. Queue 전송 (Display / Log 태스크 소비) */
        xQueueOverwrite(xQueue_SensorData, &data);

        /* 3. 임계값 검사 → EventGroup 플래그 설정 */
        alarmBits = 0;
        if (data.temperature  > TEMP_THRESHOLD_HIGH)  alarmBits |= EVT_TEMP_HIGH;
        if (data.humidity     > HUMID_THRESHOLD_HIGH) alarmBits |= EVT_HUMID_HIGH;
        if (data.distance_cm  < DIST_THRESHOLD_NEAR)  alarmBits |= EVT_DIST_NEAR;
        if (data.lux          < LIGHT_THRESHOLD_LOW)  alarmBits |= EVT_LIGHT_LOW;

        if (alarmBits)
            xEventGroupSetBits(xEvt_Alarm, alarmBits);
        else
            xEventGroupClearBits(xEvt_Alarm, 0x0F);

        /* 4. 정밀 주기 대기 (100ms) */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
```

### 3. ToFTask — I2C Mutex 보호

```c
void ToFTask(void *pvParameters)
{
    uint16_t distance_mm;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    VL53L0X_Init();   // I2C 초기화 (Mutex 획득 후 진행)

    for (;;)
    {
        /* I2C 버스 Mutex 획득 (최대 10ms 대기) */
        if (xSemaphoreTake(xMutex_I2C, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            VL53L0X_ReadDistance(&distance_mm);
            xSemaphoreGive(xMutex_I2C);   // 반드시 반환

            xQueueOverwrite(xQueue_ToFData, &distance_mm);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}
```

### 4. DisplayTask — 대시보드 렌더링

```c
void DisplayTask(void *pvParameters)
{
    SensorData_t sensor;
    uint16_t     tof_mm;
    char         buf[32];
    TickType_t   xLastWakeTime = xTaskGetTickCount();

    ILI9341_Init();
    ILI9341_FillScreen(ILI9341_BLACK);
    DrawDashboardLayout();   // 정적 레이아웃 1회 그리기

    for (;;)
    {
        /* Queue에서 최신 데이터 수신 (비블로킹) */
        xQueuePeek(xQueue_SensorData, &sensor, 0);
        xQueuePeek(xQueue_ToFData,    &tof_mm, 0);

        /* SPI Mutex 획득 후 LCD 업데이트 */
        if (xSemaphoreTake(xMutex_SPI, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            snprintf(buf, sizeof(buf), "TEMP  %.1f C ", sensor.temperature);
            ILI9341_DrawString(10, 40, buf, FONT_16, ILI9341_CYAN,  ILI9341_BLACK);

            snprintf(buf, sizeof(buf), "HUMI  %.1f %% ", sensor.humidity);
            ILI9341_DrawString(10, 70, buf, FONT_16, ILI9341_GREEN, ILI9341_BLACK);

            snprintf(buf, sizeof(buf), "DIST  %3u cm ", sensor.distance_cm);
            ILI9341_DrawString(10, 100, buf, FONT_16, ILI9341_YELLOW, ILI9341_BLACK);

            snprintf(buf, sizeof(buf), "ToF   %4u mm", tof_mm);
            ILI9341_DrawString(10, 130, buf, FONT_16, ILI9341_WHITE, ILI9341_BLACK);

            snprintf(buf, sizeof(buf), "LUX   %4u   ", sensor.lux);
            ILI9341_DrawString(10, 160, buf, FONT_16, ILI9341_MAGENTA, ILI9341_BLACK);

            xSemaphoreGive(xMutex_SPI);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
    }
}
```

### 5. AlarmTask — EventGroup 대기

```c
void AlarmTask(void *pvParameters)
{
    EventBits_t bits;

    for (;;)
    {
        /* ANY 조건 - 하나라도 플래그 세워지면 즉시 진입 */
        bits = xEventGroupWaitBits(
                    xEvt_Alarm,
                    EVT_TEMP_HIGH | EVT_HUMID_HIGH | EVT_DIST_NEAR | EVT_LIGHT_LOW,
                    pdFALSE,    // 비트 자동 클리어 안 함 (SensorTask에서 클리어)
                    pdFALSE,    // OR 조건 (ANY)
                    portMAX_DELAY);

        /* 플래그별 알람 처리 */
        if (bits & EVT_TEMP_HIGH)  { Buzzer_Beep(2, 200); WS2812B_SetAll(COLOR_RED);    }
        if (bits & EVT_HUMID_HIGH) { Buzzer_Beep(1, 100); WS2812B_SetAll(COLOR_BLUE);   }
        if (bits & EVT_DIST_NEAR)  { Buzzer_Beep(3, 50);  WS2812B_SetAll(COLOR_YELLOW); }
        if (bits & EVT_LIGHT_LOW)  {                       WS2812B_SetAll(COLOR_WHITE);  }

        vTaskDelay(pdMS_TO_TICKS(500));   // 알람 후 0.5초 대기
    }
}
```

### 6. LogTask — UART 출력 + EEPROM 기록

```c
void LogTask(void *pvParameters)
{
    SensorData_t sensor;
    uint16_t     tof_mm;
    char         log_buf[96];
    uint16_t     eeprom_addr = 0x0000;
    TickType_t   xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        xQueuePeek(xQueue_SensorData, &sensor, 0);
        xQueuePeek(xQueue_ToFData,    &tof_mm, 0);

        /* UART 로그 출력 */
        snprintf(log_buf, sizeof(log_buf),
                 "[%08lu] T:%.1f H:%.1f D:%u ToF:%u L:%u\r\n",
                 sensor.timestamp,
                 sensor.temperature, sensor.humidity,
                 sensor.distance_cm, tof_mm, sensor.lux);

        HAL_UART_Transmit(&huart1, (uint8_t*)log_buf, strlen(log_buf), 100);

        /* EEPROM 기록 (I2C Mutex 보호) */
        if (xSemaphoreTake(xMutex_I2C, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            EEPROM_Write(eeprom_addr, (uint8_t*)&sensor, sizeof(SensorData_t));
            xSemaphoreGive(xMutex_I2C);
            eeprom_addr += sizeof(SensorData_t);
            if (eeprom_addr >= 0x7FFF) eeprom_addr = 0x0000;  // 순환 기록
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}
```

---

## 📊 시스템 동작 흐름

```
전원 ON
  │
  ├─ FreeRTOS Init (Queue, Mutex, EventGroup 생성)
  ├─ 태스크 5개 생성 및 스케줄러 시작
  │
  ├─ [100ms] SensorTask ──▶ Queue 전송 ──▶ DisplayTask (200ms 갱신)
  │                     └──▶ EventGroup ──▶ AlarmTask (이벤트 구동)
  │
  ├─ [50ms]  ToFTask ─────▶ Queue 전송 ──▶ DisplayTask
  │          (I2C Mutex)
  │
  └─ [1000ms] LogTask ────▶ UART 출력 + EEPROM 기록
              (I2C Mutex)
```

### UART 출력 예시

```
[00000100] T:24.5 H:62.3 D:145 ToF:1423 L:512
[00001100] T:24.6 H:62.5 D:143 ToF:1418 L:508
[00002100] T:35.2 H:81.0 D:18  ToF:182  L:280  ← 알람 발생!
```

---

## ⚠️ 주요 설계 고려사항

### I2C 버스 충돌 방지 (Mutex)

VL53L0X, AT24C256, DS1307이 **동일한 I2C1 버스를 공유**합니다.  
ToFTask(50ms)와 LogTask(1000ms)가 동시에 I2C를 접근할 수 있으므로  
반드시 `xMutex_I2C`로 보호해야 합니다.

```c
/* 잘못된 방법 - 버스 충돌 발생 가능 */
VL53L0X_ReadDistance(&dist);   // ToFTask
EEPROM_Write(addr, data, len); // LogTask (동시 접근 시 데이터 깨짐)

/* 올바른 방법 - Mutex로 순서 보장 */
if (xSemaphoreTake(xMutex_I2C, pdMS_TO_TICKS(10)) == pdTRUE) {
    VL53L0X_ReadDistance(&dist);
    xSemaphoreGive(xMutex_I2C);
}
```

### xQueueOverwrite vs xQueueSend

센서 데이터는 **가장 최신값만 필요**하므로 Queue 길이를 1로 설정하고  
`xQueueOverwrite`를 사용합니다. 이전 값이 소비되지 않아도 덮어씁니다.

```c
/* Queue 생성 - 길이 1, 최신값 유지 */
xQueue_SensorData = xQueueCreate(1, sizeof(SensorData_t));

/* 덮어쓰기 전송 - 비블로킹 */
xQueueOverwrite(xQueue_SensorData, &data);
```

### 스택 사이즈 점검

```c
/* 디버그 시 각 태스크 스택 여유 확인 */
void vApplicationIdleHook(void)
{
    UBaseType_t uxHighWater;
    uxHighWater = uxTaskGetStackHighWaterMark(NULL);
    // uxHighWater 가 0에 가까우면 스택 오버플로 위험
}
```

---

## 🧪 실습 과제

### 기본 과제

1. `SensorTask`의 주기를 50ms → 200ms로 변경하고 디스플레이 응답성 변화를 관찰한다
2. `xMutex_I2C`를 제거한 뒤 ToFTask와 LogTask 동시 실행 시 I2C 충돌 현상을 확인한다
3. UART 로그에 RTC 실제 시각(HH:MM:SS)을 포함하도록 LogTask를 수정한다

### 심화 과제

4. 알람 이력을 EEPROM에 저장하고 전원 재투입 후 마지막 알람 시각을 복원한다
5. `DisplayTask`에 간단한 바 그래프를 추가하여 온도/습도 추이를 시각화한다
6. `configUSE_RUNTIME_STATS`를 활성화하여 각 태스크의 CPU 점유율을 UART로 출력한다

---

## 🔍 트러블슈팅

| 증상 | 원인 | 해결 방법 |
|------|------|----------|
| LCD 화면이 깨짐 | SPI 버스 동시 접근 | `xMutex_SPI` 적용 확인 |
| VL53L0X 값이 0 반환 | I2C 주소 충돌 또는 Mutex 미해제 | `xSemaphoreGive` 누락 점검 |
| AlarmTask가 반응 없음 | EventGroup 비트 설정 오류 | `xEventGroupSetBits` 반환값 확인 |
| UART 출력 글자 깨짐 | LogTask 스택 부족 | 스택 384 → 512 words로 증가 |
| 스케줄러 시작 후 하드폴트 | 힙 부족 | `configTOTAL_HEAP_SIZE` 증가 |

---

## 📚 참고 자료

- [FreeRTOS Queue API](https://www.freertos.org/a00018.html)
- [FreeRTOS EventGroup API](https://www.freertos.org/FreeRTOS-Event-Groups.html)
- [VL53L0X STM32 HAL 드라이버](https://github.com/lamik/VL53L0X_API_STM32_HAL)
- [ILI9341 STM32 SPI 라이브러리](https://github.com/martnak/STM32-ILI9341)
- STM32CubeF1 FreeRTOS 예제: `STM32Cube_FW_F1_Vx.x.x/Projects/`

---

*P01 · 스마트 환경 모니터링 스테이션 | 광주인력개발원 임베디드 시스템 과정*
