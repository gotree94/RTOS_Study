# P04 · 얼굴 표정 인식 반응형 IoT 디바이스

> STM32F103 + FreeRTOS 기반 OV7670 카메라 + 픽셀 분석 + 멀티 액추에이터 동시 제어  
> **핵심 RTOS 개념 : `Producer-Consumer Pattern` · `Double Buffer` · `Stack Sizing` · `DMA Callback Safety`**

---

## 📌 프로젝트 개요

OV7670 카메라로 영상을 캡처하고 **픽셀 밝기·분포 분석**으로 표정(밝음/보통/어둠)을 분류합니다.  
인식 결과에 따라 **WS2812B LED 감정 색상**, **Servo 물리적 반응**, **Buzzer 사운드 피드백**을  
FreeRTOS 태스크로 **동시에** 출력합니다.  
카메라 캡처 → 처리 → 출력의 파이프라인이 **Producer-Consumer 패턴**의 핵심 학습 구조입니다.

### 학습 목표

- **Producer-Consumer 패턴** — Queue depth 설계와 백프레셔(Backpressure) 처리를 이해한다
- **프레임버퍼 더블버퍼링** — 캡처 중 분석 접근 차단으로 테어링(Tearing)을 방지한다
- **`uxTaskGetStackHighWaterMark`** 로 태스크별 스택 사용량을 측정하고 최적화한다
- **DMA 완료 콜백** 내 안전한 RTOS API 호출 방법(`FromISR` 패밀리)을 실습한다
- 다중 액추에이터(LED·Servo·Buzzer)를 단일 이벤트로 **동기 구동**하는 설계를 이해한다

---

## 🔧 하드웨어 구성

### 부품 목록 (BOM)

| 부품 | 모델 | 인터페이스 | 수량 |
|------|------|-----------|------|
| MCU 보드 | STM32F103C8T6 (Blue Pill) | - | 1 |
| 카메라 모듈 | OV7670 (VGA, 640×480) | DCMI / GPIO | 1 |
| TFT LCD | ILI9341 2.8인치 (240×320) | SPI | 1 |
| RGB LED | WS2812B × 8 | GPIO DMA | 1 |
| 서보 모터 | MG996R (180°) | TIM PWM | 1 |
| 부저 | 능동 Buzzer | TIM PWM | 1 |
| 표정 매트릭스 | Vector 표정 LED 패널 | GPIO / SPI | 1 |
| USB-UART | CH340 / CP2102 | UART | 1 |
| 전원 | 5V 2A DC 어댑터 | - | 1 |

### OV7670 핀 배치

```
STM32F103C8T6  ←→  OV7670
┌──────────────────────────────────────────────┐
│  PA4  ── OV7670 HREF  (수평 동기)            │
│  PA5  ── OV7670 VSYNC (수직 동기, EXTI5)     │
│  PA6  ── OV7670 PCLK  (픽셀 클럭)           │
│                                              │
│  PC0  ── OV7670 D0   (데이터 비트 0)         │
│  PC1  ── OV7670 D1                          │
│  PC2  ── OV7670 D2                          │
│  PC3  ── OV7670 D3                          │
│  PC4  ── OV7670 D4                          │
│  PC5  ── OV7670 D5                          │
│  PC6  ── OV7670 D6                          │
│  PC7  ── OV7670 D7   (데이터 비트 7)         │
│                                              │
│  PB10 ── OV7670 SIOC (I2C2 SCL, SCCB)      │
│  PB11 ── OV7670 SIOD (I2C2 SDA, SCCB)      │
│  PB12 ── OV7670 RESET                       │
│  PB13 ── OV7670 PWDN                        │
│  PB14 ── OV7670 XCLK (MCO 출력, 8MHz)      │
│                                              │
│  PB3  ── ILI9341 SCK  (SPI1)               │
│  PB4  ── ILI9341 MISO (SPI1)               │
│  PB5  ── ILI9341 MOSI (SPI1, WS2812B DMA)  │
│  PB6  ── ILI9341 CS                        │
│  PB7  ── ILI9341 DC                        │
│  PB8  ── ILI9341 RST                        │
│                                              │
│  PA8  ── Servo MG996R (TIM1_CH1 PWM 50Hz)  │
│  PA9  ── USART1 TX → PC                    │
│  PA10 ── USART1 RX ← PC                    │
│  PA11 ── Buzzer (TIM1_CH4 PWM)             │
└──────────────────────────────────────────────┘
```

### 시스템 블록 다이어그램

```
         ┌─────────────┐
         │   OV7670    │ ── SCCB(I2C2) 설정
         │  카메라 모듈 │ ── VSYNC / HREF / PCLK
         └──────┬──────┘ ── D[7:0] 픽셀 데이터
                │
         GPIO + TIM (소프트 DCMI)
                │
    ┌───────────▼───────────┐
    │   프레임버퍼 A / B     │  QCIF 176×144 RGB565
    │   (더블 버퍼, SRAM)   │  = 176×144×2 = 50,688 bytes
    └───────────┬───────────┘       ※ F103 SRAM 20KB 한계
                │                   → QQVGA 80×60 = 9,600 bytes 사용
    ┌───────────▼───────────┐
    │    분석 태스크         │── 표정 분류 (밝기/분포)
    └───────────┬───────────┘
                │ Queue (ExpressionResult_t)
    ┌───────────▼───────────┐
    │   액추에이터 태스크들  │
    ├── WS2812B LED          │── 감정 색상
    ├── MG996R Servo         │── 물리적 반응 (각도)
    ├── Buzzer               │── 사운드 피드백
    └── ILI9341 디스플레이   │── 라이브 뷰 + 표정 이모지
```

---

## 🗂️ 프로젝트 구조

```
P04_FaceReaction_IoT/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── camera_task.h       ← OV7670 캡처 태스크
│   │   ├── analysis_task.h     ← 픽셀 분석 + 표정 분류
│   │   ├── led_task.h          ← WS2812B DMA 제어
│   │   ├── actuator_task.h     ← Servo + Buzzer 제어
│   │   ├── display_task.h      ← ILI9341 라이브 뷰
│   │   ├── frame_buffer.h      ← 더블 버퍼 관리
│   │   └── rtos_objects.h      ← 전역 RTOS 핸들
│   └── Src/
│       ├── main.c
│       ├── camera_task.c
│       ├── analysis_task.c
│       ├── led_task.c
│       ├── actuator_task.c
│       ├── display_task.c
│       ├── frame_buffer.c
│       └── rtos_objects.c
├── Drivers/
│   ├── OV7670/                 ← SCCB 설정 + 캡처 드라이버
│   ├── ILI9341/
│   ├── WS2812B/
│   └── MG996R/
├── .ioc
└── README.md
```

---

## 🔄 FreeRTOS 태스크 설계

### 태스크 구성표

| 태스크 이름 | 우선순위 | 스택 (words) | 주기 / 트리거 | 역할 |
|------------|---------|-------------|-------------|------|
| `CameraTask` | 4 (최고) | 512 | VSYNC 이벤트 | OV7670 프레임 캡처 |
| `AnalysisTask` | 3 | 768 | Queue 구동 | 픽셀 분석 + 표정 분류 |
| `LEDTask` | 3 | 256 | Queue 구동 | WS2812B DMA 색상 출력 |
| `ActuatorTask` | 3 | 256 | Queue 구동 | Servo + Buzzer 피드백 |
| `DisplayTask` | 2 | 512 | 100ms | ILI9341 라이브 뷰 |
| `StackMonTask` | 1 | 128 | 2000ms | 스택 사용량 UART 출력 |

> **Producer-Consumer 구조**  
> `CameraTask` (Producer) → Queue → `AnalysisTask` (Consumer + Producer) → Queue → `LED/Actuator/DisplayTask` (Consumer)

### 더블 버퍼 구조

```
프레임버퍼 A [9,600 bytes]   프레임버퍼 B [9,600 bytes]
┌─────────────────────┐     ┌─────────────────────┐
│   CameraTask 캡처   │     │  AnalysisTask 분석  │
│   (쓰기 중)         │     │  (읽기 중)          │
└─────────────────────┘     └─────────────────────┘
          │                           │
          └─────────── 스왑 ──────────┘
               (VSYNC 완료 후 교체)
```

```c
/* 더블 버퍼 상태 머신 */
typedef enum {
    BUF_STATE_FREE      = 0,   // 미사용
    BUF_STATE_CAPTURING = 1,   // CameraTask 캡처 중
    BUF_STATE_READY     = 2,   // 캡처 완료, 분석 대기
    BUF_STATE_ANALYSING = 3,   // AnalysisTask 분석 중
} BufState_t;
```

### RTOS 오브젝트 목록

```c
/* rtos_objects.h */

/* Queues */
extern QueueHandle_t xQueue_FrameReady;     // CameraTask → AnalysisTask
extern QueueHandle_t xQueue_Expression;     // AnalysisTask → LED/Actuator/Display

/* Mutex */
extern SemaphoreHandle_t xMutex_SPI;        // SPI1 버스 (ILI9341 + WS2812B)
extern SemaphoreHandle_t xMutex_FrameBuf;   // 프레임버퍼 스왑 보호

/* Binary Semaphore */
extern SemaphoreHandle_t xSem_VSYNC;        // VSYNC ISR → CameraTask

/* Task Handles - StackMon 감시용 */
extern TaskHandle_t xHandle_Camera;
extern TaskHandle_t xHandle_Analysis;
extern TaskHandle_t xHandle_LED;
extern TaskHandle_t xHandle_Actuator;
extern TaskHandle_t xHandle_Display;

/* 표정 분류 결과 구조체 */
typedef enum {
    EXPR_UNKNOWN  = 0,
    EXPR_HAPPY    = 1,   // 밝음 (평균 픽셀 높음)
    EXPR_NEUTRAL  = 2,   // 보통 (중간)
    EXPR_SAD      = 3,   // 어둠 (평균 픽셀 낮음)
    EXPR_ANGRY    = 4,   // 고대비 (분산 높음)
    EXPR_SURPRISE = 5,   // 중앙 집중 (중앙 밝기 높음)
} Expression_t;

typedef struct {
    Expression_t expr;
    uint8_t      confidence;    // 0~100%
    uint16_t     avg_brightness;
    uint16_t     contrast;
    uint32_t     timestamp;
} ExpressionResult_t;

/* 프레임버퍼 (QQVGA 80×60 RGB565) */
#define FRAME_WIDTH    80
#define FRAME_HEIGHT   60
#define FRAME_BYTES    (FRAME_WIDTH * FRAME_HEIGHT * 2)   // 9,600 bytes

extern uint8_t frame_buf_A[FRAME_BYTES];
extern uint8_t frame_buf_B[FRAME_BYTES];
```

---

## 💻 핵심 코드

### 1. RTOS 오브젝트 생성 (`main.c`)

```c
void MX_FREERTOS_Init(void)
{
    /* Queue 생성 */
    /* depth=2: 분석보다 캡처가 빠를 때 최대 2프레임 버퍼링 */
    xQueue_FrameReady  = xQueueCreate(2, sizeof(uint8_t*));        // 버퍼 포인터 전달
    xQueue_Expression  = xQueueCreate(4, sizeof(ExpressionResult_t));

    /* Mutex / Semaphore */
    xMutex_SPI      = xSemaphoreCreateMutex();
    xMutex_FrameBuf = xSemaphoreCreateMutex();
    xSem_VSYNC      = xSemaphoreCreateBinary();

    /* 태스크 생성 */
    xTaskCreate(CameraTask,   "Camera",   512, NULL, 4, &xHandle_Camera);
    xTaskCreate(AnalysisTask, "Analysis", 768, NULL, 3, &xHandle_Analysis);
    xTaskCreate(LEDTask,      "LED",      256, NULL, 3, &xHandle_LED);
    xTaskCreate(ActuatorTask, "Actuator", 256, NULL, 3, &xHandle_Actuator);
    xTaskCreate(DisplayTask,  "Display",  512, NULL, 2, &xHandle_Display);
    xTaskCreate(StackMonTask, "StackMon", 128, NULL, 1, NULL);
}
```

### 2. CameraTask — VSYNC ISR + 더블버퍼 캡처

```c
/* VSYNC 상승 엣지 ISR (프레임 시작) */
void EXTI9_5_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_5))
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xSem_VSYNC, &xHigherPriorityTaskWoken);
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_5);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void CameraTask(void *pvParameters)
{
    uint8_t  active_buf = 0;   // 0=A, 1=B
    uint8_t *capture_ptr;
    uint8_t *ready_ptr;

    OV7670_Init();   // SCCB 레지스터 설정 (QQVGA, RGB565)

    for (;;)
    {
        /* VSYNC 신호 대기 (프레임 시작) */
        xSemaphoreTake(xSem_VSYNC, portMAX_DELAY);

        /* 활성 캡처 버퍼 선택 */
        capture_ptr = (active_buf == 0) ? frame_buf_A : frame_buf_B;

        /* 픽셀 데이터 캡처 (HREF/PCLK 기반 GPIO 읽기) */
        OV7670_CaptureFrame(capture_ptr, FRAME_WIDTH, FRAME_HEIGHT);

        /* 캡처 완료 - 버퍼 스왑 (Mutex 보호) */
        if (xSemaphoreTake(xMutex_FrameBuf, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            ready_ptr  = capture_ptr;
            active_buf ^= 1;   // 다음 캡처는 반대 버퍼
            xSemaphoreGive(xMutex_FrameBuf);
        }

        /* AnalysisTask에 버퍼 포인터 전달 */
        /* Queue 가득 참(백프레셔) → 0ms 타임아웃으로 프레임 드롭 */
        if (xQueueSend(xQueue_FrameReady, &ready_ptr, 0) != pdTRUE)
        {
            /* 프레임 드롭 카운트 (디버그용) */
            gFrameDropCount++;
        }
    }
}
```

### 3. AnalysisTask — 픽셀 분석 + 표정 분류

```c
/*
 * QQVGA(80×60) RGB565 픽셀 분석 알고리즘
 *
 * RGB565 → 밝기 변환:
 *   R = (pixel >> 11) & 0x1F  (5비트 → 0~31)
 *   G = (pixel >>  5) & 0x3F  (6비트 → 0~63)
 *   B = (pixel >>  0) & 0x1F  (5비트 → 0~31)
 *   Y = 0.299R + 0.587G + 0.114B  (루마)
 *
 * 표정 분류 기준:
 *   HAPPY   : avg_brightness > 180  (밝은 얼굴)
 *   NEUTRAL : 120 < avg < 180       (중간)
 *   SAD     : avg_brightness < 120  (어두운 얼굴)
 *   ANGRY   : contrast > 80         (고대비, 그림자 강함)
 *   SURPRISE: center_ratio > 1.4    (중앙 밝기 / 전체 밝기)
 */

static uint16_t CalcAverageBrightness(uint8_t *buf, uint16_t w, uint16_t h)
{
    uint32_t sum = 0;
    uint16_t total = w * h;

    for (uint16_t i = 0; i < total; i++)
    {
        uint16_t pixel = ((uint16_t)buf[i*2] << 8) | buf[i*2+1];
        uint8_t r = (pixel >> 11) & 0x1F;
        uint8_t g = (pixel >>  5) & 0x3F;
        uint8_t b = (pixel >>  0) & 0x1F;
        /* 루마 근사: Y ≈ (R×2 + G×3 + B) / 6 (정수 연산) */
        sum += (r * 2 + g * 3 + b) * 255 / (31*2 + 63*3 + 31);
    }
    return (uint16_t)(sum / total);
}

static uint16_t CalcContrast(uint8_t *buf, uint16_t w, uint16_t h,
                              uint16_t avg)
{
    uint32_t variance = 0;
    uint16_t total = w * h;

    for (uint16_t i = 0; i < total; i++)
    {
        uint16_t pixel = ((uint16_t)buf[i*2] << 8) | buf[i*2+1];
        uint8_t g = (pixel >> 5) & 0x3F;
        int16_t diff = (int16_t)(g * 255 / 63) - (int16_t)avg;
        variance += (uint32_t)(diff * diff);
    }
    return (uint16_t)sqrtf((float)(variance / total));
}

void AnalysisTask(void *pvParameters)
{
    uint8_t            *frame_ptr;
    ExpressionResult_t  result;

    for (;;)
    {
        /* 프레임 포인터 수신 대기 */
        if (xQueueReceive(xQueue_FrameReady, &frame_ptr,
                          portMAX_DELAY) == pdTRUE)
        {
            /* 프레임버퍼 읽기 (Mutex 보호) */
            if (xSemaphoreTake(xMutex_FrameBuf, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                result.avg_brightness = CalcAverageBrightness(
                                            frame_ptr, FRAME_WIDTH, FRAME_HEIGHT);
                result.contrast       = CalcContrast(
                                            frame_ptr, FRAME_WIDTH, FRAME_HEIGHT,
                                            result.avg_brightness);
                xSemaphoreGive(xMutex_FrameBuf);
            }

            /* 중앙 영역 밝기 (20×20 중앙 픽셀) */
            uint16_t center_bright = 0;
            for (uint8_t y = 20; y < 40; y++)
                for (uint8_t x = 30; x < 50; x++) {
                    uint16_t px = ((uint16_t)frame_ptr[(y*FRAME_WIDTH+x)*2] << 8)
                                | frame_ptr[(y*FRAME_WIDTH+x)*2+1];
                    center_bright += (px >> 5) & 0x3F;
                }
            center_bright /= (20 * 20);

            /* 표정 분류 */
            result.timestamp = xTaskGetTickCount();
            float center_ratio = (float)center_bright /
                                 (float)(result.avg_brightness + 1);

            if      (center_ratio > 1.4f)               result.expr = EXPR_SURPRISE;
            else if (result.contrast > 80)               result.expr = EXPR_ANGRY;
            else if (result.avg_brightness > 180)        result.expr = EXPR_HAPPY;
            else if (result.avg_brightness > 120)        result.expr = EXPR_NEUTRAL;
            else                                         result.expr = EXPR_SAD;

            result.confidence = (uint8_t)
                ((fabsf(center_ratio - 1.0f) * 50.0f) +
                 (result.contrast / 2.0f));
            if (result.confidence > 100) result.confidence = 100;

            /* 결과 Queue 전송 (LED / Actuator / Display 모두 소비) */
            xQueueOverwrite(xQueue_Expression, &result);
        }
    }
}
```

### 4. LEDTask — WS2812B DMA 감정 색상

```c
/* 표정별 WS2812B 색상 매핑 */
static const uint32_t ExprColor[6] = {
    [EXPR_UNKNOWN]  = 0x000000,   // 꺼짐
    [EXPR_HAPPY]    = 0xFFFF00,   // 노랑  (밝음)
    [EXPR_NEUTRAL]  = 0x00FF00,   // 초록  (안정)
    [EXPR_SAD]      = 0x0000FF,   // 파랑  (차가움)
    [EXPR_ANGRY]    = 0xFF0000,   // 빨강  (강렬)
    [EXPR_SURPRISE] = 0xFF00FF,   // 마젠타 (놀람)
};

/* WS2812B 애니메이션 패턴 */
typedef enum {
    ANIM_SOLID   = 0,   // 단색 고정
    ANIM_PULSE   = 1,   // 페이드 인/아웃
    ANIM_CHASE   = 2,   // 순차 점등
    ANIM_RAINBOW = 3,   // 무지개
} AnimType_t;

void LEDTask(void *pvParameters)
{
    ExpressionResult_t result;
    ExpressionResult_t prev_result = { .expr = EXPR_UNKNOWN };
    uint8_t            anim_step = 0;
    TickType_t         xLastWakeTime = xTaskGetTickCount();

    WS2812B_Init();

    for (;;)
    {
        /* 새 표정 결과 수신 (비블로킹) */
        xQueuePeek(xQueue_Expression, &result, 0);

        /* 표정 변경 시 전환 애니메이션 */
        if (result.expr != prev_result.expr)
        {
            WS2812B_FadeOut(200);     // 200ms 페이드 아웃
            prev_result = result;
        }

        /* 표정별 애니메이션 */
        uint32_t color = ExprColor[result.expr];
        switch (result.expr)
        {
            case EXPR_HAPPY:
                WS2812B_Chase(color, anim_step, 8);   // 빠른 체이스
                break;
            case EXPR_SAD:
                WS2812B_Pulse(color, anim_step, 30);  // 느린 펄스
                break;
            case EXPR_ANGRY:
                WS2812B_Flash(color, 5);              // 빠른 깜박임
                break;
            case EXPR_SURPRISE:
                WS2812B_Rainbow(anim_step);           // 무지개
                break;
            default:
                WS2812B_SetAll(color);
                break;
        }
        anim_step = (anim_step + 1) % 255;

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(30));   // ~33fps 애니메이션
    }
}
```

### 5. ActuatorTask — Servo + Buzzer 동기 피드백

```c
/* 표정별 서보 각도 + 부저 패턴 */
typedef struct {
    uint8_t  servo_angle;    // 0~180도
    uint16_t buzzer_freq;    // Hz (0 = 무음)
    uint16_t buzzer_dur_ms;  // 지속시간
    uint8_t  buzzer_count;   // 반복 횟수
} ActuatorPattern_t;

static const ActuatorPattern_t ActPattern[6] = {
    [EXPR_UNKNOWN]  = { 90,    0,   0, 0 },
    [EXPR_HAPPY]    = { 45, 1047, 100, 3 },   // 서보 앞으로, 밝은 삼화음
    [EXPR_NEUTRAL]  = { 90,  523, 200, 1 },   // 중앙, 단음
    [EXPR_SAD]      = {135,  262, 500, 1 },   // 서보 뒤로, 저음
    [EXPR_ANGRY]    = { 10,  200,  50, 5 },   // 최대 앞, 빠른 저음
    [EXPR_SURPRISE] = { 90, 2093,  80, 4 },   // 중앙, 고음 빠르게
};

void ActuatorTask(void *pvParameters)
{
    ExpressionResult_t result;
    Expression_t       prev_expr = EXPR_UNKNOWN;

    Servo_Init();
    Buzzer_Init();
    Servo_SetAngle(90);   // 초기 중앙 위치

    for (;;)
    {
        /* 새 표정 결과 대기 (블로킹, 최대 500ms) */
        if (xQueuePeek(xQueue_Expression, &result,
                       pdMS_TO_TICKS(500)) == pdTRUE)
        {
            /* 표정 변경 시에만 액추에이터 구동 */
            if (result.expr != prev_expr)
            {
                const ActuatorPattern_t *pat = &ActPattern[result.expr];

                /* 서보 각도 설정 */
                Servo_SetAngle(pat->servo_angle);

                /* 부저 패턴 출력 */
                for (uint8_t i = 0; i < pat->buzzer_count; i++)
                {
                    Buzzer_Tone(pat->buzzer_freq, pat->buzzer_dur_ms);
                    vTaskDelay(pdMS_TO_TICKS(pat->buzzer_dur_ms / 2));
                }

                prev_expr = result.expr;

                /* UART 로그 */
                char log[64];
                snprintf(log, sizeof(log),
                         "[EXPR] %s (conf:%d%%) bright:%d cont:%d\r\n",
                         ExprName[result.expr], result.confidence,
                         result.avg_brightness, result.contrast);
                HAL_UART_Transmit(&huart1, (uint8_t*)log, strlen(log), 50);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

### 6. DisplayTask — ILI9341 라이브 뷰 + 표정 이모지

```c
/* 표정별 이모지 문자열 (Vector 표정 패널 연동 가능) */
static const char *ExprName[6]  = { "???", "HAPPY", "NEUTRAL",
                                    "SAD",  "ANGRY", "SURPRISE" };
static const char *ExprEmoji[6] = { " _ ", " :D", "  |",
                                    " :(", " >(",   " :O" };
/* ILI9341 표정별 색상 */
static const uint16_t ExprILI_Color[6] = {
    ILI9341_WHITE, ILI9341_YELLOW, ILI9341_GREEN,
    ILI9341_BLUE,  ILI9341_RED,    ILI9341_MAGENTA
};

void DisplayTask(void *pvParameters)
{
    ExpressionResult_t result;
    uint8_t           *disp_buf;
    char               info_str[32];
    TickType_t         xLastWakeTime = xTaskGetTickCount();

    ILI9341_Init();
    ILI9341_FillScreen(ILI9341_BLACK);

    /* 정적 레이아웃 그리기 */
    ILI9341_DrawString(0,   0, "FaceReact IoT",  FONT_16, ILI9341_WHITE, ILI9341_BLACK);
    ILI9341_DrawLine(0, 20, 240, 20, ILI9341_GRAY);

    for (;;)
    {
        xQueuePeek(xQueue_Expression, &result, 0);

        if (xSemaphoreTake(xMutex_SPI, pdMS_TO_TICKS(30)) == pdTRUE)
        {
            /* 1. 카메라 라이브 뷰 (80×60 → 160×120 스케일업 표시) */
            disp_buf = (gActiveBuffer == 0) ? frame_buf_B : frame_buf_A;
            ILI9341_DrawRGB565Scaled(30, 25, 80, 60, 2, disp_buf);

            /* 2. 표정 레이블 */
            snprintf(info_str, sizeof(info_str), "Expr: %-8s %s",
                     ExprName[result.expr], ExprEmoji[result.expr]);
            ILI9341_DrawString(10, 155, info_str, FONT_16,
                               ExprILI_Color[result.expr], ILI9341_BLACK);

            /* 3. 신뢰도 바 그래프 */
            ILI9341_FillRect(10, 175, 200, 12, ILI9341_DARKGRAY);
            ILI9341_FillRect(10, 175, result.confidence * 2, 12,
                             ExprILI_Color[result.expr]);

            /* 4. 밝기 / 대비 수치 */
            snprintf(info_str, sizeof(info_str),
                     "B:%3d  C:%3d  Conf:%2d%%",
                     result.avg_brightness,
                     result.contrast,
                     result.confidence);
            ILI9341_DrawString(5, 195, info_str, FONT_12,
                               ILI9341_LIGHTGRAY, ILI9341_BLACK);

            xSemaphoreGive(xMutex_SPI);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
```

### 7. StackMonTask — 스택 사용량 모니터링

```c
/*
 * uxTaskGetStackHighWaterMark() 반환값:
 *   스택에서 한 번도 사용되지 않은 최소 여유 words 수
 *   → 값이 0에 가까울수록 스택 오버플로 위험
 *   → 초기 설계 스택의 20% 이상 여유 권장
 */

void StackMonTask(void *pvParameters)
{
    char buf[128];

    for (;;)
    {
        snprintf(buf, sizeof(buf),
            "\r\n[STACK] Camera:%3u  Analysis:%3u  LED:%3u"
            "  Actuator:%3u  Display:%3u  StackMon:%3u\r\n",
            uxTaskGetStackHighWaterMark(xHandle_Camera),
            uxTaskGetStackHighWaterMark(xHandle_Analysis),
            uxTaskGetStackHighWaterMark(xHandle_LED),
            uxTaskGetStackHighWaterMark(xHandle_Actuator),
            uxTaskGetStackHighWaterMark(xHandle_Display),
            uxTaskGetStackHighWaterMark(NULL));   // 자기 자신

        HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 100);

        /* 드롭 프레임 수도 출력 */
        snprintf(buf, sizeof(buf), "[CAM ] FrameDrop: %lu\r\n", gFrameDropCount);
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 50);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

---

## 📊 시스템 동작 흐름

### Producer-Consumer 파이프라인

```
OV7670 VSYNC ISR (약 10fps @ QQVGA)
    │
    │ xSemaphoreGiveFromISR()
    ▼
CameraTask (P4) ─── 프레임 캡처 (~30ms)
    │
    │ xQueueSend(xQueue_FrameReady, &ptr, 0)  ← 가득 차면 드롭
    ▼
AnalysisTask (P3) ─── 픽셀 분석 (~15ms)
    │ avg_brightness, contrast, center_ratio 계산
    │ 표정 분류
    │
    │ xQueueOverwrite(xQueue_Expression, &result)
    ▼
    ├──▶ LEDTask     (P3) ─── WS2812B 색상/애니메이션
    ├──▶ ActuatorTask(P3) ─── Servo 각도 + Buzzer 패턴
    └──▶ DisplayTask (P2) ─── ILI9341 라이브 뷰 + 레이블
```

### UART 출력 예시

```
[STACK] Camera:312  Analysis:498  LED:189  Actuator:201  Display:287  StackMon:98
[CAM ] FrameDrop: 0
[EXPR] HAPPY    (conf:78%) bright:187 cont:42
[EXPR] NEUTRAL  (conf:55%) bright:134 cont:31
[EXPR] ANGRY    (conf:82%) bright:141 cont:95
[EXPR] SURPRISE (conf:71%) bright:158 cont:58
```

---

## ⚠️ 주요 설계 고려사항

### STM32F103 SRAM 한계와 해상도 선택

```
STM32F103C8T6 SRAM = 20KB (20,480 bytes)

해상도별 프레임버퍼 크기 (RGB565 = 2bytes/픽셀):
  VGA   (640×480) = 614,400 bytes  → 불가 (30배 초과)
  QVGA  (320×240) = 153,600 bytes  → 불가 (7배 초과)
  QQVGA ( 80× 60) =   9,600 bytes  → 더블버퍼 19.2KB → 한계
  커스텀(64× 48)  =   6,144 bytes  → 더블버퍼 12.3KB → 권장

→ 본 프로젝트: QQVGA(80×60) 사용, 힙 최소화 설정 필요

FreeRTOSConfig.h 메모리 배분 (총 20KB):
  프레임버퍼 A    :  9,600 bytes (전역 배열)
  프레임버퍼 B    :  9,600 bytes (전역 배열)
  FreeRTOS 힙     :  6,144 bytes (configTOTAL_HEAP_SIZE)
  스택 + 기타     :  나머지
```

### Queue Depth와 백프레셔 설계

```c
/* 잘못된 설계 - Queue 가득 차면 CameraTask 블로킹 */
xQueueSend(xQueue_FrameReady, &ptr, portMAX_DELAY);
/* → CameraTask가 AnalysisTask보다 빠를 때 무기한 대기
      → 다음 VSYNC 놓침 → 프레임 누락 */

/* 올바른 설계 - 타임아웃 0으로 즉시 드롭 */
if (xQueueSend(xQueue_FrameReady, &ptr, 0) != pdTRUE) {
    gFrameDropCount++;   // 드롭 카운트만 기록
}
/* → CameraTask는 항상 즉시 반환 → 다음 프레임 준비 */
```

### DMA 콜백 내 안전한 RTOS API 호출

```c
/* DMA 전송 완료 콜백은 ISR 컨텍스트에서 호출됨 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* ISR 컨텍스트 → FromISR 버전 필수 */
    /* 일반 API 호출 시 하드폴트 발생! */

    /* 잘못된 방법 */
    // xSemaphoreGive(xMutex_SPI);           ← 절대 금지

    /* 올바른 방법 */
    xSemaphoreGiveFromISR(xSem_DMAComplete,
                          &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### 스택 최적화 프로세스

```
1단계: 넉넉하게 시작 (모든 태스크 512 words)
2단계: 시스템 실행 후 StackMonTask로 High Water Mark 측정
3단계: 측정값 기준 최적화

측정 예시:
  CameraTask   : 여유 312 words → 512 - 312 = 실사용 200 words
                 → 256 words로 감소 가능 (여유 20% 확보)
  AnalysisTask : 여유 120 words → 실사용 648 words
                 → 768 words 유지 필요 (sqrtf 등 수학함수 스택 소비 큼)
```

---

## 🧪 실습 과제

### 기본 과제

1. 표정 분류 임계값(`avg_brightness > 180` 등)을 조정하고 분류 정확도 변화를 관찰한다
2. `xQueue_FrameReady` depth를 1로 줄이고 프레임 드롭 발생 빈도를 측정한다
3. `StackMonTask` 출력을 보고 가장 스택 여유가 적은 태스크를 찾아 스택을 재조정한다

### 심화 과제

4. `HAL_SPI_TxCpltCallback`에서 일반 `xSemaphoreGive()`를 호출하고 하드폴트 발생을 확인한 뒤 `FromISR` 버전으로 수정한다
5. 프레임버퍼를 더블버퍼에서 단일버퍼로 변경했을 때 LCD 표시에서 **테어링 현상**을 관찰한다
6. `configUSE_TRACE_FACILITY = 1` 로 활성화한 뒤 `vTaskList()`로 모든 태스크 상태를 UART에 출력하고 스케줄링 동작을 분석한다

---

## 🔍 트러블슈팅

| 증상 | 원인 | 해결 방법 |
|------|------|----------|
| OV7670 화면 전혀 안 나옴 | SCCB 초기화 실패 | I2C2 속도 100kHz 확인, PWDN 핀 LOW 설정 |
| 화면 색상 이상 | RGB565 바이트 순서 오류 | 상위/하위 바이트 swap 적용 |
| 하드폴트 발생 | DMA ISR에서 일반 API 호출 | 모든 ISR 내 RTOS 호출 → `FromISR` 변환 |
| 분석 결과 항상 UNKNOWN | AnalysisTask 스택 오버플로 | 스택 768 → 1024 words로 증가 |
| LCD 테어링 | 더블버퍼 Mutex 미적용 | `xMutex_FrameBuf` 적용 확인 |
| WS2812B 색상 깨짐 | SPI DMA 완료 전 다음 전송 | DMA TC 플래그 또는 Semaphore 대기 추가 |
| SRAM 부족 하드폴트 | 프레임버퍼 + 힙 초과 | 해상도를 64×48로 축소 |

---

## 📚 참고 자료

- [FreeRTOS Queue API](https://www.freertos.org/a00018.html)
- [OV7670 레지스터 설정 가이드](https://www.voti.nl/docs/OV7670.pdf)
- [STM32F103 SRAM 배치 최적화](https://www.st.com/resource/en/application_note/an4289-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf)
- [WS2812B STM32 DMA 드라이버](https://github.com/nicowillis/stm32-ws2812b)
- [Double Buffering in Embedded Systems](https://embeddedinventor.com/double-buffering-in-embedded-systems/)

---

*P04 · 얼굴 표정 인식 반응형 IoT 디바이스 | 광주인력개발원 임베디드 시스템 과정*
