# P02 · RTOS 기반 자율주행 로봇카

> STM32F103 + FreeRTOS 기반 실시간 장애물 회피 + 자세 안정화 로봇카  
> **핵심 RTOS 개념 : `Task Priority` · `Binary Semaphore` · `vTaskPrioritySet` · `Watchdog Task`**

---

## 📌 프로젝트 개요

MPU6050 IMU로 로봇 자세를 실시간 계산하고, 전방 초음파·ToF 센서로 장애물을 감지해  
**자율 회피 주행**하는 FreeRTOS 기반 로봇카입니다.  
TRCT5000 라인트레이서와 블루투스 원격 제어를 **동시에** 처리하며,  
PID 제어 알고리즘을 전용 태스크로 분리하여 **RTOS 실시간 제어의 핵심**을 학습합니다.

### 학습 목표

- 태스크 **우선순위 계층 설계** — 모터 제어가 항상 최고 응답성을 유지하는 구조 이해
- `xSemaphoreCreateBinary` 로 **ISR → 태스크 동기화** (인터럽트 기반 장애물 감지)
- `vTaskPrioritySet` 으로 **긴급 상황 시 동적 우선순위 변경**
- **Watchdog 태스크** 패턴으로 데드락/태스크 정지 감지 및 시스템 복구
- PID 루프와 `vTaskDelayUntil` 을 결합한 **정밀 제어 주기 보장**

---

## 🔧 하드웨어 구성

### 부품 목록 (BOM)

| 부품 | 모델 | 인터페이스 | 수량 |
|------|------|-----------|------|
| MCU 보드 | STM32F103C8T6 (Blue Pill) | - | 1 |
| IMU 센서 | MPU6050 | I2C | 1 |
| 초음파 센서 | HC-SR04 | GPIO / TIM | 1 |
| ToF 센서 | VL53L0X | I2C | 1 |
| 라인 센서 | TRCT5000 × 3 | ADC / GPIO | 1세트 |
| 모터 드라이버 | L298N 또는 BTS7960 | GPIO / PWM | 1 |
| DC 모터 | TT Motor × 4 | PWM | 4 |
| 서보 모터 | MG996R | TIM PWM | 1 |
| TFT LCD | GC9A01 1.28인치 원형 (240×240) | SPI | 1 |
| 블루투스 모듈 | HC-06 / HC-05 | UART | 1 |
| 배터리 | 18650 2S 또는 7.4V LiPo | - | 1 |
| 레벨 시프터 | 3.3V ↔ 5V | - | 1 |

### 핀 배치

```
STM32F103C8T6
┌─────────────────────────────────────────────┐
│  PA0  ── TRCT5000 LEFT   (ADC1_CH0)         │
│  PA1  ── TRCT5000 CENTER (ADC1_CH1)         │
│  PA2  ── TRCT5000 RIGHT  (ADC1_CH2)         │
│  PA3  ── HC-SR04 ECHO    (TIM2_CH4 / EXTI3) │
│  PA4  ── HC-SR04 TRIG    (GPIO OUT)         │
│                                             │
│  PA6  ── Motor IN1 (TIM3_CH1 PWM)          │
│  PA7  ── Motor IN2 (TIM3_CH2 PWM)          │
│  PB0  ── Motor IN3 (TIM3_CH3 PWM)          │
│  PB1  ── Motor IN4 (TIM3_CH4 PWM)          │
│                                             │
│  PB6  ── Servo (TIM4_CH1 PWM 50Hz)         │
│                                             │
│  PB8  ── I2C1 SCL (MPU6050, VL53L0X)       │
│  PB9  ── I2C1 SDA (MPU6050, VL53L0X)       │
│                                             │
│  PA9  ── USART1 TX → BT Module             │
│  PA10 ── USART1 RX ← BT Module             │
│                                             │
│  PB12 ── GC9A01 CS                         │
│  PB13 ── GC9A01 SCK  (SPI2)               │
│  PB15 ── GC9A01 MOSI (SPI2)               │
│  PC13 ── GC9A01 DC                         │
│  PC14 ── GC9A01 RST                        │
└─────────────────────────────────────────────┘
```

### 로봇 기구 구조

```
        [Servo + HC-SR04]  ← 좌우 스캔
              │
    ┌─────────┴─────────┐
    │   STM32F103       │
    │   (Blue Pill)     │
    │   MPU6050         │
    │   VL53L0X         │
    │   GC9A01 LCD      │
    └──────┬────────────┘
           │
    ┌──────┴──────────────────┐
    │  L298N / BTS7960        │
    │  Motor Driver           │
    └──┬──────────────────┬───┘
       │                  │
  [FL Motor] [FR Motor]  [RL Motor] [RR Motor]
       │                  │
  ─────┴──────────────────┴─────
          로봇카 섀시
  ─────────────────────────────
  [TRCT L]  [TRCT C]  [TRCT R]   ← 라인 감지
```

---

## 🗂️ 프로젝트 구조

```
P02_RTOS_RobotCar/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── imu_task.h          ← MPU6050 자세 계산 태스크
│   │   ├── obstacle_task.h     ← HC-SR04 + VL53L0X 장애물 감지
│   │   ├── motor_task.h        ← PWM 모터 제어 + PID
│   │   ├── bt_ctrl_task.h      ← UART 블루투스 명령 수신
│   │   ├── display_task.h      ← GC9A01 상태 표시
│   │   ├── watchdog_task.h     ← 태스크 생존 감시
│   │   └── rtos_objects.h      ← 전역 RTOS 핸들 선언
│   └── Src/
│       ├── main.c
│       ├── imu_task.c
│       ├── obstacle_task.c
│       ├── motor_task.c
│       ├── bt_ctrl_task.c
│       ├── display_task.c
│       ├── watchdog_task.c
│       └── rtos_objects.c
├── Drivers/
│   ├── MPU6050/
│   ├── VL53L0X/
│   ├── GC9A01/
│   └── PID/
├── .ioc
└── README.md
```

---

## 🔄 FreeRTOS 태스크 설계

### 태스크 구성표

| 태스크 이름 | 우선순위 | 스택 (words) | 주기 | 역할 |
|------------|---------|-------------|------|------|
| `MotorTask` | **5** (최고) | 256 | 10ms | PID 연산 + PWM 모터 출력 |
| `IMUTask` | 4 | 384 | 10ms | MPU6050 자세각 계산 |
| `ObstacleTask` | 4 | 256 | 50ms | HC-SR04 + VL53L0X 감지 |
| `BTCtrlTask` | 3 | 256 | 이벤트 구동 | UART 원격 명령 수신 |
| `DisplayTask` | 2 | 512 | 200ms | GC9A01 상태 시각화 |
| `WatchdogTask` | 1 | 128 | 500ms | 태스크 생존 감시 |

> **우선순위 설계 원칙**  
> `MotorTask` 는 항상 최고 우선순위 — 제어 지연이 물리적 충돌로 직결됨  
> `WatchdogTask` 는 최저 우선순위 — 다른 태스크가 모두 정상일 때만 실행됨

### 태스크 간 통신 구조

```
┌─────────────┐  xQueueSend()   ┌──────────────┐
│   IMUTask   │────────────────▶│              │
│  (자세각도)  │                 │  MotorTask   │
└─────────────┘  xQueueSend()   │  (PID + PWM) │
┌─────────────┐────────────────▶│              │
│ObstacleTask │                 └──────┬───────┘
│(거리/충돌)   │                        │
└──────┬──────┘                        │ xQueueSend()
       │                               ▼
       │ xSemaphoreGive()      ┌──────────────┐
       │ (충돌 긴급 신호)       │ DisplayTask  │
       ▼                       │ (GC9A01)     │
┌─────────────┐                └──────────────┘
│Binary Sema  │
│xSem_Emergency│
└──────┬──────┘
       │ xSemaphoreTake()
       ▼
  MotorTask 즉시 정지
  + vTaskPrioritySet()

┌─────────────┐  xQueueSend()   ┌──────────────┐
│ BTCtrlTask  │────────────────▶│  MotorTask   │
│ (UART 명령) │                 │  (명령 적용)  │
└─────────────┘                 └──────────────┘

┌─────────────┐  울워치독 체크
│WatchdogTask │─── 각 태스크 알림 타임아웃 감시
└─────────────┘─── 이상 시 NVIC_SystemReset()
```

### RTOS 오브젝트 목록

```c
/* rtos_objects.h */

/* Queues */
extern QueueHandle_t xQueue_IMUData;       // IMUTask → MotorTask / Display
extern QueueHandle_t xQueue_ObstacleData;  // ObstacleTask → MotorTask / Display
extern QueueHandle_t xQueue_BTCommand;     // BTCtrlTask → MotorTask

/* Binary Semaphore */
extern SemaphoreHandle_t xSem_Emergency;   // ObstacleTask → MotorTask 긴급 정지

/* Mutex */
extern SemaphoreHandle_t xMutex_I2C;       // I2C 버스 보호 (MPU6050 + VL53L0X)
extern SemaphoreHandle_t xMutex_SPI;       // SPI 버스 보호 (GC9A01)

/* Task Handles - Watchdog 감시용 */
extern TaskHandle_t xHandle_IMU;
extern TaskHandle_t xHandle_Obstacle;
extern TaskHandle_t xHandle_Motor;

/* 드라이브 모드 */
typedef enum {
    MODE_STOP      = 0,
    MODE_AUTO      = 1,   // 자율 주행
    MODE_LINE      = 2,   // 라인 트레이싱
    MODE_MANUAL    = 3,   // BT 원격 제어
    MODE_EMERGENCY = 4,   // 긴급 정지
} DriveMode_t;

extern volatile DriveMode_t gDriveMode;
```

---

## 💻 핵심 코드

### 1. RTOS 오브젝트 생성 (`main.c`)

```c
void MX_FREERTOS_Init(void)
{
    /* Queue 생성 */
    xQueue_IMUData      = xQueueCreate(1,  sizeof(IMUData_t));
    xQueue_ObstacleData = xQueueCreate(1,  sizeof(ObstacleData_t));
    xQueue_BTCommand    = xQueueCreate(10, sizeof(BTCmd_t));

    /* Binary Semaphore - 긴급 정지 신호 */
    xSem_Emergency = xSemaphoreCreateBinary();

    /* Mutex */
    xMutex_I2C = xSemaphoreCreateMutex();
    xMutex_SPI = xSemaphoreCreateMutex();

    /* 태스크 생성 (우선순위 순) */
    xTaskCreate(MotorTask,    "Motor",    256, NULL, 5, &xHandle_Motor);
    xTaskCreate(IMUTask,      "IMU",      384, NULL, 4, &xHandle_IMU);
    xTaskCreate(ObstacleTask, "Obstacle", 256, NULL, 4, &xHandle_Obstacle);
    xTaskCreate(BTCtrlTask,   "BTCtrl",   256, NULL, 3, NULL);
    xTaskCreate(DisplayTask,  "Display",  512, NULL, 2, NULL);
    xTaskCreate(WatchdogTask, "Watchdog", 128, NULL, 1, NULL);
}
```

### 2. IMUTask — MPU6050 자세 계산

```c
typedef struct {
    float pitch;        // 앞뒤 기울기 (도)
    float roll;         // 좌우 기울기 (도)
    float yaw_rate;     // 회전 각속도 (°/s)
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
} IMUData_t;

void IMUTask(void *pvParameters)
{
    IMUData_t imu;
    MPU6050_Raw_t raw;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    float dt = 0.01f;   // 10ms 주기

    /* Watchdog 알림 카운터 */
    uint32_t wdg_counter = 0;

    MPU6050_Init();

    for (;;)
    {
        /* I2C Mutex 획득 */
        if (xSemaphoreTake(xMutex_I2C, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            MPU6050_ReadAll(&raw);
            xSemaphoreGive(xMutex_I2C);
        }

        /* 상보 필터 (Complementary Filter) - 자세각 계산 */
        float accel_pitch = atan2f(raw.accel_x,
                                   sqrtf(raw.accel_y * raw.accel_y +
                                         raw.accel_z * raw.accel_z)) * 180.0f / M_PI;
        float accel_roll  = atan2f(raw.accel_y, raw.accel_z) * 180.0f / M_PI;

        imu.pitch    = 0.98f * (imu.pitch + raw.gyro_y * dt) + 0.02f * accel_pitch;
        imu.roll     = 0.98f * (imu.roll  + raw.gyro_x * dt) + 0.02f * accel_roll;
        imu.yaw_rate = raw.gyro_z;

        /* Queue 전송 (최신값 덮어쓰기) */
        xQueueOverwrite(xQueue_IMUData, &imu);

        /* Watchdog 알림 - 500ms마다 1회 */
        if (++wdg_counter >= 50) {
            xTaskNotify(xHandle_Watchdog_IMU, 0x01, eSetBits);
            wdg_counter = 0;
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
```

### 3. ObstacleTask — Binary Semaphore 긴급 신호

```c
typedef struct {
    uint16_t front_cm;      // HC-SR04 전방 거리 (cm)
    uint16_t front_tof_mm;  // VL53L0X 정밀 거리 (mm)
    uint8_t  left_line;     // TRCT 라인 감지 (0/1)
    uint8_t  center_line;
    uint8_t  right_line;
    uint8_t  emergency;     // 긴급 정지 플래그
} ObstacleData_t;

#define OBSTACLE_STOP_CM     15    // 15cm 이하 즉시 정지
#define OBSTACLE_SLOW_CM     40    // 40cm 이하 감속

void ObstacleTask(void *pvParameters)
{
    ObstacleData_t obs;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        /* 전방 초음파 측정 */
        obs.front_cm = HCSR04_GetDistance_cm();

        /* ToF 정밀 측정 (I2C Mutex 보호) */
        if (xSemaphoreTake(xMutex_I2C, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            uint16_t tof_raw;
            VL53L0X_ReadDistance(&tof_raw);
            obs.front_tof_mm = tof_raw;
            xSemaphoreGive(xMutex_I2C);
        }

        /* 라인 센서 ADC 읽기 */
        obs.left_line   = (ADC_ReadChannel(ADC_CH_TRCT_L) < LINE_THRESHOLD) ? 1 : 0;
        obs.center_line = (ADC_ReadChannel(ADC_CH_TRCT_C) < LINE_THRESHOLD) ? 1 : 0;
        obs.right_line  = (ADC_ReadChannel(ADC_CH_TRCT_R) < LINE_THRESHOLD) ? 1 : 0;

        /* 긴급 정지 조건 - Binary Semaphore로 즉시 신호 */
        if (obs.front_cm < OBSTACLE_STOP_CM ||
            obs.front_tof_mm < (OBSTACLE_STOP_CM * 10))
        {
            obs.emergency = 1;
            xSemaphoreGive(xSem_Emergency);   // MotorTask에 즉시 신호
        }
        else
        {
            obs.emergency = 0;
        }

        xQueueOverwrite(xQueue_ObstacleData, &obs);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}
```

### 4. MotorTask — PID 제어 + 우선순위 동적 변경

```c
/* PID 구조체 */
typedef struct {
    float Kp, Ki, Kd;
    float prev_error;
    float integral;
    float output_min, output_max;
} PID_t;

static PID_t pid_straight = { .Kp=1.2f, .Ki=0.01f, .Kd=0.05f,
                               .output_min=-100.0f, .output_max=100.0f };

void MotorTask(void *pvParameters)
{
    IMUData_t      imu;
    ObstacleData_t obs;
    BTCmd_t        cmd;
    TickType_t     xLastWakeTime = xTaskGetTickCount();

    Motor_Init();

    for (;;)
    {
        /* ① 긴급 정지 확인 - Binary Semaphore 비블로킹 체크 */
        if (xSemaphoreTake(xSem_Emergency, 0) == pdTRUE)
        {
            Motor_Stop();
            gDriveMode = MODE_EMERGENCY;

            /* 긴급 상황 시 우선순위를 최고로 올려 복구 루틴 독점 */
            vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);

            Servo_Scan_Obstacle();    // 서보로 좌우 스캔
            Motor_Reverse(30, 500);   // 0.5초 후진
            Motor_TurnRight(50, 300); // 회피 회전

            /* 정상 우선순위 복원 */
            vTaskPrioritySet(NULL, 5);
            gDriveMode = MODE_AUTO;
            continue;
        }

        /* ② 최신 센서 데이터 수신 */
        xQueuePeek(xQueue_IMUData,      &imu, 0);
        xQueuePeek(xQueue_ObstacleData, &obs, 0);

        /* ③ BT 명령 수신 (있으면 Manual 모드) */
        if (xQueueReceive(xQueue_BTCommand, &cmd, 0) == pdTRUE)
        {
            gDriveMode = MODE_MANUAL;
            Motor_ApplyCommand(&cmd);
        }

        /* ④ 드라이브 모드별 처리 */
        switch (gDriveMode)
        {
            case MODE_AUTO:
                if (obs.front_cm < OBSTACLE_SLOW_CM)
                    Motor_SetSpeed(30);          // 감속
                else
                    Motor_SetSpeed(60);          // 정속

                /* IMU Yaw 보정 PID */
                float correction = PID_Compute(&pid_straight, 0.0f, imu.yaw_rate);
                Motor_SetDifferential(correction);
                break;

            case MODE_LINE:
                LineTrace_Run(&obs);
                break;

            case MODE_MANUAL:
                /* BTCtrlTask가 이미 처리 - 3초 후 AUTO 복귀 */
                if (xTaskGetTickCount() - cmd.timestamp > pdMS_TO_TICKS(3000))
                    gDriveMode = MODE_AUTO;
                break;

            case MODE_STOP:
            default:
                Motor_Stop();
                break;
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
```

### 5. BTCtrlTask — UART 원격 명령 수신

```c
/* 블루투스 명령 프로토콜
   'F' = 전진,  'B' = 후진
   'L' = 좌회전, 'R' = 우회전
   'S' = 정지,  'A' = 자율주행
   'X' = 라인트레이싱
   '0'~'9' = 속도 설정 (0=0%, 9=90%)
*/
typedef struct {
    char     cmd;
    uint8_t  speed;
    uint32_t timestamp;
} BTCmd_t;

void BTCtrlTask(void *pvParameters)
{
    uint8_t rx_byte;
    BTCmd_t cmd;

    for (;;)
    {
        /* UART 수신 대기 (블로킹 - CPU 낭비 없음) */
        if (HAL_UART_Receive(&huart1, &rx_byte, 1, portMAX_DELAY) == HAL_OK)
        {
            cmd.cmd       = (char)rx_byte;
            cmd.timestamp = xTaskGetTickCount();

            switch (rx_byte)
            {
                case 'A': gDriveMode = MODE_AUTO;   break;
                case 'X': gDriveMode = MODE_LINE;   break;
                case 'S': gDriveMode = MODE_STOP;   break;
                case '0' ... '9':
                    cmd.speed = (rx_byte - '0') * 10;
                    break;
                default:
                    cmd.speed = 60;
                    xQueueSend(xQueue_BTCommand, &cmd, 0);
                    break;
            }
        }
    }
}
```

### 6. WatchdogTask — 태스크 생존 감시

```c
/*
 * 각 태스크는 500ms마다 xTaskNotify 로 Watchdog에 알림을 보냄
 * Watchdog은 1000ms(2주기) 내에 알림이 없으면 해당 태스크 사망 판정
 * → 시스템 리셋 또는 태스크 재생성
 */

#define WDG_TIMEOUT_MS   1000

void WatchdogTask(void *pvParameters)
{
    uint32_t notified_bits;

    for (;;)
    {
        /* 1000ms 내에 모든 태스크로부터 알림 대기 */
        BaseType_t result = xTaskNotifyWait(
                                0,          // 진입 시 클리어 비트 없음
                                0xFFFFFFFF, // 종료 시 모든 비트 클리어
                                &notified_bits,
                                pdMS_TO_TICKS(WDG_TIMEOUT_MS));

        if (result == pdFALSE)
        {
            /* 타임아웃 - 응답 없는 태스크 존재 */
            /* UART 에러 로그 출력 */
            HAL_UART_Transmit(&huart1,
                (uint8_t*)"[WDG] TASK TIMEOUT! RESET\r\n", 27, 100);

            HAL_Delay(100);
            NVIC_SystemReset();   // 시스템 리셋
        }

        /* 정상 응답 확인 로그 */
        /* notified_bits: bit0=IMU, bit1=Obstacle, bit2=Motor */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### 7. PID 연산 함수

```c
float PID_Compute(PID_t *pid, float setpoint, float measured)
{
    float error    = setpoint - measured;
    float dt       = 0.01f;   // 10ms 고정

    pid->integral  += error * dt;
    float derivative = (error - pid->prev_error) / dt;

    /* 적분 와인드업 방지 */
    if (pid->integral > pid->output_max / pid->Ki)
        pid->integral = pid->output_max / pid->Ki;
    if (pid->integral < pid->output_min / pid->Ki)
        pid->integral = pid->output_min / pid->Ki;

    float output = pid->Kp * error
                 + pid->Ki * pid->integral
                 + pid->Kd * derivative;

    /* 출력 클램핑 */
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;

    pid->prev_error = error;
    return output;
}
```

---

## 📊 시스템 동작 흐름

### 자율주행 시퀀스

```
전원 ON → FreeRTOS 스케줄러 시작
       │
       ├─ [10ms]  IMUTask      → 자세각 계산 → Queue
       ├─ [50ms]  ObstacleTask → 거리 측정  → Queue
       ├─ [10ms]  MotorTask    → PID 연산   → PWM 출력
       ├─ [Event] BTCtrlTask   → UART 수신  → Queue
       ├─ [200ms] DisplayTask  → GC9A01 표시
       └─ [500ms] WatchdogTask → 생존 감시
       │
       ├─ 전방 15cm 미만 감지
       │       │
       │       └─ ObstacleTask → xSemaphoreGive(xSem_Emergency)
       │               │
       │               └─ MotorTask (우선순위 최고로 승격)
       │                       → 정지 → 스캔 → 후진 → 회피
       │                       → 우선순위 정상 복원
       └─ 계속 주행
```

### UART 상태 출력 예시

```
[BOOT] FreeRTOS RobotCar v1.0 Start
[IMU ] Pitch: -0.3  Roll:  0.1  YawRate:  0.2
[OBS ] Front: 125cm  ToF: 1248mm  Line: 0-1-0
[MTR ] Mode: AUTO  Speed: 60  Diff: +2.3
[OBS ] Front:  12cm  ToF:  118mm  ← 장애물!
[WDG ] EMERGENCY! Stopping...
[MTR ] Reversing 500ms...
[MTR ] Turning Right 300ms...
[MTR ] Resume AUTO mode
```

---

## ⚠️ 주요 설계 고려사항

### 태스크 우선순위 계층 설계 원칙

```
우선순위 5 : MotorTask    → 물리적 충돌 방지, 항상 즉각 응답
우선순위 4 : IMU/Obstacle → 센서 데이터 신선도 보장
우선순위 3 : BTCtrlTask   → 원격 명령 처리
우선순위 2 : DisplayTask  → 시각화, 지연 허용
우선순위 1 : WatchdogTask → 모든 태스크 정상 시만 실행
```

> DisplayTask가 SPI 블로킹으로 오래 걸려도 MotorTask는 선점하여 즉시 실행됨

### Binary Semaphore vs Mutex 선택 기준

```c
/* Mutex - 자원 보호 (소유권 개념, 우선순위 상속 지원) */
xMutex_I2C = xSemaphoreCreateMutex();   // I2C 버스 공유

/* Binary Semaphore - 신호 전달 (소유권 없음, ISR에서 사용 가능) */
xSem_Emergency = xSemaphoreCreateBinary(); // 장애물 긴급 신호

/* ISR에서는 반드시 FromISR 버전 사용 */
void EXTI3_IRQHandler(void)   // HC-SR04 ECHO 인터럽트
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xSem_Emergency, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### vTaskDelayUntil vs vTaskDelay

```c
/* vTaskDelay - 실행 완료 후 N ms 대기 (주기 drift 발생) */
vTaskDelay(pdMS_TO_TICKS(10));
// 실행 시간이 3ms 걸리면 실제 주기 = 13ms

/* vTaskDelayUntil - 절대 시각 기준 (정밀 주기 보장) */
TickType_t xLastWakeTime = xTaskGetTickCount();
vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
// 실행 시간 무관, 항상 정확히 10ms 주기 유지
// → PID 루프에서 dt 상수 가정이 성립함
```

---

## 🧪 실습 과제

### 기본 과제

1. `MotorTask` 우선순위를 `ObstacleTask`보다 낮게 설정하고 장애물 회피 응답 지연을 관찰한다
2. `xSem_Emergency`를 `Mutex`로 교체했을 때 ISR 호환 문제를 확인한다
3. PID 게인(`Kp`, `Ki`, `Kd`)을 조정하며 직진 안정성 변화를 측정한다

### 심화 과제

4. `WatchdogTask`가 실제로 동작하는지 검증하기 위해 IMUTask를 의도적으로 `vTaskSuspend`로 정지시키고 리셋 동작을 확인한다
5. 서보 모터로 HC-SR04를 좌/우 스캔하여 **더 넓은 쪽으로 회피**하는 스마트 회피 알고리즘을 구현한다
6. `uxTaskGetStackHighWaterMark`로 각 태스크 스택 여유를 측정하고 최적 스택 크기를 산출한다

---

## 🔍 트러블슈팅

| 증상 | 원인 | 해결 방법 |
|------|------|----------|
| 모터가 즉시 정지하지 않음 | MotorTask 우선순위 낮음 | 우선순위 5(최고) 설정 확인 |
| MPU6050 값이 튐 | I2C Mutex 미적용 | `xMutex_I2C` xSemaphoreTake/Give 쌍 확인 |
| 장애물 감지 후 무한 정지 | `xSem_Emergency` 클리어 누락 | 회피 후 Semaphore 소비 확인 |
| Watchdog 오작동 리셋 | 태스크 알림 주기 오류 | `xTaskNotify` 호출 주기 확인 |
| 라인 트레이싱 이탈 | ADC 기준값 미조정 | `LINE_THRESHOLD` 환경에 맞게 보정 |
| PID 직진 불안정 | dt 부정확 (vTaskDelay 사용) | `vTaskDelayUntil`로 교체 |

---

## 📚 참고 자료

- [FreeRTOS Task Priority](https://www.freertos.org/RTOS-task-priority.html)
- [FreeRTOS Binary Semaphore](https://www.freertos.org/Embedded-RTOS-Binary-Semaphores.html)
- [MPU6050 Complementary Filter](https://ahrs.readthedocs.io/en/latest/filters/complementary.html)
- [GC9A01 STM32 드라이버](https://github.com/Naned/GC9A01)
- STM32 FreeRTOS 예제: `STM32Cube_FW_F1/Projects/STM3210C_EVAL/Applications/FreeRTOS/`

---

*P02 · RTOS 기반 자율주행 로봇카 | 광주인력개발원 임베디드 시스템 과정*
