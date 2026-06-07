# 5단계: 실무 종합 애플리케이션

> **학습 목표**: 지금까지 배운 모든 RTOS 개념을 통합하여 실제 제품 수준의
> 멀티-태스크 데이터 수집 시스템을 설계하고 구현한다.

---

## 실습 준비

### ① 파일 복사

| 파일 | 원본 (이 저장소) | 대상 (CubeIDE 프로젝트) |
|------|----------------|----------------------|
| `main.h` | `05_RealWorld_App/Core/Inc/main.h` | `RTOS_Study/Core/Inc/main.h` |
| `main.c` | `05_RealWorld_App/Core/Src/main.c` | `RTOS_Study/Core/Src/main.c` |

> 💡 기존 파일은 백업(`main.c.bak`) 후 덮어쓰세요.

### ② .ioc 설정 확인

이 단계는 1~4단계의 모든 기능을 통합하므로 다음 설정이 모두 필요합니다:

| 확인 항목 | 설정값 | 설정 위치 |
|----------|--------|----------|
| UART2 | `Asynchronous`, 115200 Baud | Pinout → USART2 |
| USART2 NVIC | ✅ **ENABLED** | Pinout → USART2 → NVIC |
| LED (LD2) | `PA5`, GPIO_Output | Pinout → PA5 |
| Button (B1) | `PC13`, EXTI13 Falling Edge | Pinout → PC13 |
| EXTI line13 NVIC | ✅ **ENABLED** | Pinout → PC13 → NVIC |
| FREERTOS Interface | `CMSIS_V2` | Middleware → FREERTOS |
| `configUSE_PREEMPTION` | **ENABLED** | FREERTOS → Config Parameters |
| `configMAX_PRIORITIES` | **5** | FREERTOS → Config Parameters |
| `configUSE_MUTEXES` | **ENABLED** | FREERTOS → Config Parameters |
| `configUSE_COUNTING_SEMAPHORES` | **ENABLED** | FREERTOS → Config Parameters |
| `configUSE_TIMERS` | **ENABLED** (Timer 2개 사용) | FREERTOS → Config Parameters |
| `configUSE_TASK_NOTIFICATIONS` | **ENABLED** | FREERTOS → Config Parameters |
| `configUSE_TRACE_FACILITY` | **ENABLED** | FREERTOS → Config Parameters |
| `configCHECK_FOR_STACK_OVERFLOW` | **2** (Method 2) | FREERTOS → Config Parameters |

### ③ 힙 크기 확인

5단계는 태스크 5개 + Queue 3개 + Timer 2개 + Semaphore/Mutex 등으로
FreeRTOS 힙 사용량이 증가합니다. CubeIDE 기본 힙(10240 bytes)으로도
충분하지만, 부족할 경우 `configTOTAL_HEAP_SIZE`를 늘리세요:

```c
#define configTOTAL_HEAP_SIZE    ((size_t)12288)   /* 필요시 12KB로 증가 */
```

### ④ 주요 개념별 코드 위치

| 개념 | main.c 내 위치 | 설명 |
|------|---------------|------|
| `vTaskDelayUntil()` | `Task_SensorAcq` | 정확한 500ms 주기 유지 |
| `printf()` Mutex 보호 | `__io_putchar()` 재정의 | 출력 직렬화 (Race Condition 방지) |
| `xSemaphoreGiveFromISR()` | `HAL_GPIO_EXTI_Callback()` | 버튼 → ButtonCtrl Task |
| `xQueueSendFromISR()` | `HAL_UART_RxCpltCallback()` | UART → UARTCmd Task |
| `xTimerReset()` | `Task_SensorAcq` | Watchdog 연장 (태스크 생존 신호) |
| `xTaskNotifyGive()` | Timer Callback → Logger Task | 경량 깨움 |
| System State | `g_sysState` (전역 변수) | IDLE / RUNNING / ERROR |

---

## 시스템 아키텍처

```
                    ┌──────────────────────────────────────┐
                    │         System Architecture          │
                    │     Multi-Sensor DAQ System          │
                    └──────────────────────────────────────┘

  ┌─ UART (PC) ─┐      ┌─ Button (PC13) ─┐
  │  s = start  │      │   EXTI Interrupt │
  │  p = pause  │      │   (Falling Edge) │
  │  ? = status │      └────────┬─────────┘
  └──────┬──────┘               │
         │ (Queue)              │ (SemaphoreFromISR)
         ↓                      ↓
  ┌──────────────┐     ┌──────────────────┐
  │  UARTCmd     │     │  ButtonCtrl      │  prio=3
  │  Task        │────→│  Task            │──────→ System State
  └──────────────┘     └────────┬─────────┘       (IDLE/RUNNING/ERROR)
                                │ Resume/Suspend
                                ↓
  ┌─ Sim Sensor ─┐     ┌──────────────────┐
  │  Sensor 0    │     │  SensorAcq       │  prio=2
  │  Sensor 1    │────→│  Task            │──┐
  └──────────────┘     │  vTaskDelayUntil  │  │ Queue
                       └──────────────────┘  │
                                             ↓
                                  ┌──────────────────┐
                                  │  DataProc        │  prio=2
                                  │  Task            │──┐
                                  │  Moving Average   │  │ Queue
                                  │  Threshold Check  │  │
                                  └──────────────────┘  │
                                                        ↓
  ┌────────────────────┐     ┌──────────────────┐
  │  Timer_StatusReport│────→│  Logger          │  prio=1
  │  (5s Auto-reload)  │     │  Task            │──→ UART Output
  ├────────────────────┤     │  LED Control     │──→ LED (PA5)
  │  Timer_Watchdog    │     │  System Status   │
  │  (10s One-shot)    │     └──────────────────┘
  └────────────────────┘
```

---

## 태스크 설계

| 태스크 | 우선순위 | 역할 | 동기화 | 주기 |
|--------|:-------:|------|--------|:----:|
| **ButtonCtrl** | 3 (최고) | 시스템 상태 제어 | Binary Semaphore (ISR→Task) | Event-driven |
| **SensorAcq** | 2 | 센서 데이터 수집 | vTaskDelayUntil | 정확히 **500ms** |
| **DataProc** | 2 | 필터링/분석 | Queue (Sensor→Proc) | Data-driven |
| **Logger** | 1 (최저) | UART 출력 + LED | Queue (Proc→Log) + Notification | 100ms poll |
| **UARTCmd** | 1 | UART 명령어 수신 | Queue (ISR→Task) | Event-driven |

### 우선순위 설계 이유

```
ButtonCtrl (prio=3):  사용자 입력 → 최우선 응답
    ↓
SensorAcq (prio=2):   정밀한 타이밍 유지 필요 (vTaskDelayUntil)
DataProc  (prio=2):   Sensor와 동일, 데이터 흐름 유지
    ↓
Logger    (prio=1):   출력 지연 OK, 시스템 영향 없음
UARTCmd   (prio=1):   명령어 처리, 느려도 무방
```

---

## 데이터 흐름

```
Sensor 0 ──RAW=2048──→ ┌──────────┐ ──RAW=2048──→ ┌──────────┐
                       │ SensorAcq │               │ DataProc  │
Sensor 1 ──RAW=2500──→ │ (Queue)   │ ──RAW=2500──→ │ (Queue)   │
                       └──────────┘               └─────┬─────┘
                                                         │ Moving Average
                                                         │ Threshold Check
                                                         ↓
                                              ┌────────────────────┐
                                              │ Logger             │
                                              │ [S0] RAW=2048     │
                                              │ AVG=2050 NORMAL   │
                                              │ [S1] RAW=2500     │
                                              │ AVG=2480 HIGH     │
                                              └────────────────────┘
```

**데이터 구조체:**
```c
typedef struct {
    uint8_t  sensor_id;          // 0=온도, 1=조도
    uint16_t raw_value;          // 원시 ADC 값 (0~4095)
    uint16_t filtered_value;     // 이동 평균 필터 결과
    uint32_t timestamp;          // HAL_GetTick() 기준
    uint8_t  status;             // 0=OK, 1=HIGH, 2=LOW, 3=NOISE
} SensorData_t;
```

---

## 주요 개념 적용

### ✅ vTaskDelayUntil() — 정밀 주기 실행

```c
TickType_t xLastWakeTime = xTaskGetTickCount();

for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    // ↑ 실행 시간에 관계없이 정확히 500ms 간격 유지
    ReadSensor();
    SendToQueue();
}
```

### ✅ Mutex로 printf() 보호

```c
int __io_putchar(int ch) {
    xSemaphoreTake(xUartMutex, portMAX_DELAY);  // ← 다른 Task가 printf할 때 대기
    HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, 100);
    xSemaphoreGive(xUartMutex);
    return ch;
}
```

### ✅ Task Notification으로 경량 깨움

Timter Callback → Logger Task를 Notification으로 깨움 (Semaphore보다 45% 빠름)

### ✅ Watchdog 패턴

```c
// SensorAcq Task에서 매 주기마다:
g_watchdogFed = 1;  // "나는 살아있다"
xTimerReset(xWatchdogTimer, ...);  // 타이머 연장

// 10초 후 Watchdog Timer Callback:
if (!g_watchdogFed) {  // 데이터 수집 중단 = 에러!
    g_sysState = SYS_ERROR;
}
```

### ✅ Deferred Interrupt Handling

```
Button Press → EXTI ISR: xSemaphoreGiveFromISR() (1μs)
                   ↓
         ButtonCtrl Task: 상태 변경, SensorAcq Resume (실제 처리)
```

---

## UART 출력 예시

### 시스템 시작

```
╔═══════════════════════════════════════════╗
║  RTOS Study - Stage 5: Real World App    ║
║  Multi-Sensor Data Acquisition System    ║
╚═══════════════════════════════════════════╝

[INIT] Creating synchronization objects...
[INIT] Creating tasks...
[INIT] All objects created. Starting scheduler...

[BTN] System ready. Press button or send 's' to start.
[SENSOR] Acquisition task started.
[PROC] Data processor started.
[LOG] Logger started.

=========================================
  RTOS Multi-Sensor DAQ System
  Press button or send 's' to start
=========================================

>
```

### 데이터 수집 시작

```
> s

[BTN] ▶ Acquisition STARTED

[S0] RAW=2048 AVG=2050 TS=5000 [NORMAL]
[S1] RAW=3800 AVG=3750 TS=5000 [HIGH]
[S0] RAW=2100 AVG=2080 TS=5500 [NORMAL]
[S1] RAW=3600 AVG=3650 TS=5500 [HIGH]
[S0] RAW=2200 AVG=2150 TS=6000 [NORMAL]
[S1] RAW=1500 AVG=2000 TS=6000 [LOW]

===== SYSTEM STATUS =====                   ← '?' 명령어 또는 5초 자동
  State:       RUNNING
  Samples:     24
  Errors:      0
  Heap free:   8960 bytes
  Sensor queue: 2
=========================
```

### 일시 정지

```
> p

[BTN] ⏸ Acquisition PAUSED
```

---

## 직접 해보기

### ✏️ 실습 과제 (난이도: ★★★★★)

1. **실제 ADC를 연결하세요.**
   - `ReadSimSensor()` 대신 `HAL_ADC_Start()`와 실제 아날로그 센서 사용
   - PA0, PA1에 가변저항이나 온도센서(LM35 등) 연결

2. **3번째 센서를 추가하세요.**
   - SensorData_t 구조체 확장, Task_SensorAcq에 PA2 추가 등

3. **I2C/SPI 센서를 추가하세요.**
   - BMI160(가속도) 또는 SHT30(온습도) I2C 센서
   - I2C 통신을 별도 태스크로 분리

4. **RTT(Real-Time Transfer)로 출력을 변경하세요.**
   - SEGGER RTT를 사용하여 UART보다 빠른 로깅 구현

5. **Tickless Idle 모드를 활성화하세요.**
   - `configUSE_TICKLESS_IDLE = 1`
   - 전류 소모 측정 (NUCLEO 보드의 IDD 측정 핀 활용)

6. **시스템에 "threshold 설정" 명령어를 추가하세요.**
   - `t 2000 3000` → LOW=2000, HIGH=3000으로 설정
   - Queue로 명령어를 DataProc에 전달

### ❓ 생각해볼 질문

1. SensorAcq(prio=2)와 DataProc(prio=2)는 동일 우선순위입니다. Round-Robin으로 동작할 때 데이터 흐름에 지연이 발생할 수 있을까요?
2. Logger의 우선순위를 1로 낮춘 이유는 무엇일까요? (시스템의 어느 부분이 지연되어도 되는가?)
3. Watchdog Timer를 One-shot으로 생성하고 xTimerReset()으로 계속 연장하는 이유는?
4. 실제 제품에서는 vTaskDelayUntil() 대신 어떤 타이밍 방식을 사용할까요? (HAL_TIM, RTC 등)
5. printf()를 Mutex로 보호하면 모든 printf 호출이 직렬화됩니다. 더 나은 방법은? (별도 Log Task에 문자열 Queue 전송)

---

## 1~5단계 종합 정리

| 단계 | 주제 | 핵심 학습 내용 |
|:----:|------|---------------|
| **1** | Task Basics | xTaskCreate, vTaskDelay, 멀티태스킹, Round-Robin |
| **2** | Task Sync | Semaphore, Queue, Mutex, Priority Inheritance |
| **3** | Scheduling | Preemption, Priority Inversion, Software Timer, Notification |
| **4** | ISR/Resources | FromISR API, Deferred Interrupt, Stack Overflow, Heap 관리 |
| **5** | **Real World** | **전체 통합**, vTaskDelayUntil, Watchdog, 시스템 아키텍처 설계 |

---

## 권장 도구 및 자료

| 도구 | 용도 | 링크 |
|------|------|------|
| **TeraTerm** | UART 터미널 | https://ttssh2.osdn.jp/ |
| **STM32CubeIDE** | 개발/디버깅 | https://www.st.com/stm32cubeide |
| **FreeRTOS Manual** | 공식 레퍼런스 | https://www.freertos.org/Documentation/ |
| **FreeRTOS+Trace** | 실시간 모니터링 | https://www.freertos.org/FreeRTOS-Plus/ |

---

## 마치며

축하합니다! 🎉 5개 단계를 모두 완료하셨습니다.

이 커리큘럼을 통해 여러분은:
1. RTOS의 **기본 개념** (태스크, 스케줄링)
2. **동기화** (Semaphore, Queue, Mutex)
3. **고급 기능** (Timer, Notification, Priority Inheritance)
4. **인터럽트 처리** (Deferred Interrupt Handling)
5. **실무 시스템 설계** (통합, Watchdog, 에러 처리)

까지 학습하셨습니다.

이제 여러분은 STM32 + FreeRTOS 환경에서 실제 제품 수준의
RTOS 기반 애플리케이션을 설계하고 구현할 수 있는 역량을 갖추었습니다.

**Happy RTOS coding! 🚀**
