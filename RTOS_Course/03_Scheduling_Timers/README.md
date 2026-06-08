# 3단계: 스케줄링과 타이머

> **학습 목표**: FreeRTOS의 스케줄링 동작 원리와 소프트웨어 타이머를 이해하고,
> 우선순위 역전 문제를 진단하며 Task Notification을 활용할 수 있다.

---

## 실습 준비

### ① 파일 복사

| 파일 | 원본 (이 저장소) | 대상 (CubeIDE 프로젝트) |
|------|----------------|----------------------|
| `main.h` | `03_Scheduling_Timers/Core/Inc/main.h` | `RTOS_Study/Core/Inc/main.h` |
| `main.c` | `03_Scheduling_Timers/Core/Src/main.c` | `RTOS_Study/Core/Src/main.c` |

> 💡 기존 파일은 백업(`main.c.bak`) 후 덮어쓰세요.

### ② .ioc 설정 확인

이 단계에서는 Software Timer를 사용하므로 FreeRTOS 설정에서 Timer Daemon을 활성화해야 합니다:

| 확인 항목 | 필요 조건 | 설정 위치 |
|----------|----------|----------|
| `configUSE_TIMERS` | **ENABLED** | FREERTOS → Config Parameters |
| `configTIMER_TASK_PRIORITY` | **2** | FREERTOS → Config Parameters |
| `configTIMER_TASK_STACK_DEPTH` | **128** (words) | FREERTOS → Config Parameters |
| `configUSE_TASK_NOTIFICATIONS` | **ENABLED** (Task Notification 예제) | FREERTOS → Config Parameters |

**Timer Daemon Task**는 FreeRTOS 내부에서 타이머 콜백을 실행하는 전담 태스크입니다. `configUSE_TIMERS=ENABLED`로 설정해야 Software Timer API(`xTimerCreate`, `xTimerStart` 등)를 사용할 수 있습니다.

### ③ 빌드 및 실행

1. **Build Project** (Ctrl+B) → **Debug** (F11)
2. UART 터미널: **115200 baud**
3. `main.h`에서 실행할 예제 선택 후 빌드

### ④ main.c 코드 구조

```c
/* 1. includes */
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"     /* Software Timer API */
#include "semphr.h"     /* Mutex (Priority Inversion 예제) */

/* 2. 핸들 선언 */
TimerHandle_t xAutoReloadTimer;     /* Auto-reload 타이머 */
TimerHandle_t xOneShotTimer;        /* One-shot 타이머 */
TaskHandle_t  xTaskHighHandle;      /* 태스크 핸들 (Suspend/Resume 용) */

/* 3. Timer Callback 함수 */
void vAutoReloadCallback(TimerHandle_t xTimer);
void vOneShotCallback(TimerHandle_t xTimer);

/* 4. main() */
int main(void) {
    HAL_Init();  SystemClock_Config();
    MX_GPIO_Init();  MX_USART2_UART_Init();

    /* Software Timer 생성 */
    xAutoReloadTimer = xTimerCreate("Auto", pdMS_TO_TICKS(2000),
                                    pdTRUE,         /* Auto-reload */
                                    NULL, vAutoReloadCallback);
    xOneShotTimer    = xTimerCreate("OneShot", pdMS_TO_TICKS(5000),
                                    pdFALSE,        /* One-shot */
                                    NULL, vOneShotCallback);

    /* 태스크 생성 */
    xTaskCreate(Task_High, "High", 128, NULL, 3, &xTaskHighHandle);
    xTaskCreate(Task_Mid,  "Mid",  128, NULL, 2, NULL);
    xTaskCreate(Task_Low,  "Low",  128, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1);
}

/* 5. Timer Callback 본문 (Daemon Task 컨텍스트에서 실행) */
void vAutoReloadCallback(TimerHandle_t xTimer) {
    /* 짧게 유지! vTaskDelay() 사용 불가 */
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}
```

> **핵심 차이점**: 이 단계부터 `timers.h`를 include하고 Software Timer를 사용합니다.
> Timer Callback은 **Timer Daemon Task** 컨텍스트에서 실행되므로
> 내부에서 `vTaskDelay()` 등 blocking API를 호출할 수 없습니다.

---

## 개념 학습

### 1. Preemptive Scheduling (선점형 스케줄링)

```
실행 시간 ──────────────────────────────────────→
                                                      
Task_High (prio=3)  ████░░░░░░░░████░░░░░░░░████░░░░
                                                      
Task_Mid  (prio=2)  ░░░░████░░░░░░░░████░░░░░░░░████
                                                      
Task_Low  (prio=1)  ░░░░░░░░████░░░░░░░░░░░░░░░░░░░
                                                      
                    ↑ High Ready → Mid 즉시 선점
```

| 특성 | 설명 |
|------|------|
| **선점 (Preemption)** | 높은 우선순위 태스크가 Ready되면 현재 태스크를 즉시 중단 |
| **Time Slicing** | 동일 우선순위: 1 Tick 단위로 Round-Robin |
| **vTaskDelay()** | 태스크를 Blocked 상태로 → CPU 양보 |
| **Idle Task** | 실행 가능한 태스크가 없을 때 실행 (우선순위 0) |

**주의**: Preemption은 인터럽트처럼 보일 수 있지만, **FreeRTOS의 스케줄러가 관리**합니다.
태스크는 자신이 선점되고 있다는 것을 인지하지 못합니다.

---

### 2. Priority Inversion (우선순위 역전)

**정의**: 낮은 우선순위 태스크가 높은 우선순위 태스크보다 먼저 실행되는 현상

**발생 시나리오:**

```
Normal (기대 동작):
Low (prio=1)  ──[Mutex]──→ High 기다림
Mid  (prio=2)  ────────→ 실행됨
High (prio=3)  ──────────────→ 실행됨 (가장 먼저!)
```

```
Priority Inversion (문제 상황):
Low (prio=1)  ──[Mutex]──→ ────────────────────────→ Mutex 반환
Mid  (prio=2)  ──────────→ 선점! ──────────────→ (Low가 반환 못 함)
High (prio=3)  ──────────→ Blocked (Mutex 없음) ──→ 드디어 실행
                            ↑ 여기서 High가 Mid보다 늦게 실행됨!
```

```
Priority Inheritance (FreeRTOS Mutex):
Low (prio=1→3) ──[Mutex]──→ (우선순위 상승!) ──→ Mutex 반환
Mid  (prio=2)   ──────────→ Low보다 낮으므로 선점 못 함
High (prio=3)   ──────────→ 잠시 Blocked ──────→ 실행!
```

**결론**: FreeRTOS의 Mutex는 Priority Inheritance를 기본 지원합니다.
Binary Semaphore를 Mutex 대신 사용하면 Inversion이 발생할 수 있습니다.

---

### 3. Software Timer (소프트웨어 타이머)

```
Timer Daemon Task (전용 태스크)
       ↓
┌──────────────────┐
│  Timer Command   │  xTimerStart(), xTimerStop(), xTimerReset()
│     Queue        │  → Daemon이 명령을 수신하여 처리
└──────────────────┘
       ↓
┌──────────────────┐
│  Timer Callback  │  Daemon Task 컨텍스트에서 실행
│    Functions     │  → 짧게 유지, vTaskDelay() 사용 불가
└──────────────────┘
```

| 종류 | 생성 파라미터 | 동작 |
|------|-------------|------|
| **One-shot** | `uxAutoReload = pdFALSE` | 한 번 실행 후 정지 |
| **Auto-reload** | `uxAutoReload = pdTRUE` | 주기적으로 계속 실행 |

**타이머 API 실행 컨텍스트:**

| 함수 | Task Context | ISR Context |
|------|:---:|:---:|
| `xTimerCreate()` | ✅ | ❌ |
| `xTimerStart()` | ✅ | ✅ (`xTimerStartFromISR`) |
| `xTimerStop()` | ✅ | ✅ (`xTimerStopFromISR`) |
| `xTimerReset()` | ✅ | ✅ (`xTimerResetFromISR`) |
| `xTimerDelete()` | ✅ | ❌ |

---

### 4. Task Notification (태스크 알림)

**FreeRTOS에서 가장 빠른 동기화 방법** (Semaphore보다 ~45% 빠름)

```
각 Task는 32-bit Notification 값을 내장:
┌─────────────────────────┐
│  Task Control Block     │
│  ┌───────────────────┐  │
│  │ ulNotifiedValue   │  │  ← 32-bit 알림 값
│  └───────────────────┘  │
└─────────────────────────┘

xTaskNotifyGive()  →  ulNotifiedValue += 1
ulTaskNotifyTake() →  ulNotifiedValue가 0보다 클 때까지 Blocked
```

| 비교 | Binary Semaphore | Task Notification |
|------|:---:|:---:|
| 속도 | 기준 | **~45% 빠름** |
| RAM 사용 | 큐/세마포어 생성 필요 | **0 추가 RAM** (내장) |
| 한계 | 없음 | **1:1 매핑만 가능** |
| ISR 사용 | ✅ | ✅ |

---

## 실습 예제

### 예제 3.1: Priority Scheduling

3개의 태스크가 각각 다른 우선순위로 실행됩니다.

```
[High-prio=3] Running (count=1)
[High-prio=3] Running (count=2)
[High-prio=3] Running (count=3)
[High-prio=3] Task complete. Suspending...
[Mid-prio=2] Running (count=1)     ← High이 Suspended된 후 Mid 실행
[Mid-prio=2] Running (count=2)
[Low-prio=1] Running (High/Mid blocked)  ← Mid도 Blocked → Low 실행
```

### 예제 3.2: Priority Inversion

우선순위 역전 시나리오: High가 Low의 Mutex를 기다리면서 Mid에게 선점당함

```
[Low-prio=1] Mutex ACQUIRED. Working... (prio=1)
// Low가 Mutex를 쥐고 3초 대기 중...
[Mid-prio=2] Running (preempted Low!)        ← Mid가 Low를 선점!
[Mid-prio=2] Running (preempted Low!)        ← Low는 Mutex를 반환 못 함
[High-prio=3] Waiting for mutex...           ← High가 Mutex 요청
// ...하지만 Mid가 계속 실행 (Priority Inversion 상태)
// Priority Inheritance가 활성화되면 Low의 우선순위가 3으로 상승
```

### 예제 3.3: Software Timer

One-shot (5초)과 Auto-reload (2초) 타이머가 동작합니다.

```
[Timer] Auto-reload tick!          ← 2초
[Timer] Auto-reload tick!          ← 4초
[Timer] One-shot fired! (5s elapsed)  ← 5초 (LED ON)
[Timer] Auto-reload tick!          ← 6초
[BTN] Auto-reload timer STOPPED    ← 버튼 누름
[BTN] Auto-reload timer STARTED    ← 다시 누름
```

### 예제 3.4: Task Notification

Producer가 3초마다 Consumer에게 Notification을 보냅니다.

```
[Producer] Giving notification to consumer...
[Consumer] Received notification! (count=1)
[Producer] Giving notification to consumer...
[Consumer] Received notification! (count=1)
[BTN] Notifying consumer via button!    ← 버튼으로 직접 전송
[Consumer] Received notification! (count=1)
```

---

## 실습 방법

`main.h`에서 예제를 선택하세요:

```c
#define EXAMPLE_PRIORITY_SCHED      1   // 우선순위 스케줄링
#define EXAMPLE_PRIORITY_INVERSION  0   // 우선순위 역전
#define EXAMPLE_SOFTWARE_TIMER      0   // 소프트웨어 타이머
#define EXAMPLE_TASK_NOTIFY         0   // 태스크 알림
```

---

## 직접 해보기

### ✏️ 실습 과제 (난이도: ★★★☆☆)

1. **Priority Scheduling 예제에서 Task_High의 우선순위를 1로 낮춰보세요.**
   - 세 태스크가 모두 우선순위 1이면 Round-Robin으로 동작합니다.
2. **Priority Inversion 예제에서 Mutex 대신 Binary Semaphore를 사용해보세요.**
   - Priority Inheritance가 없을 때 High의 대기 시간이 어떻게 달라지는지 측정하세요.
3. **Software Timer 예제에 10초 One-shot 타이머를 하나 더 추가해보세요.**
   - 두 One-shot 타이머의 실행 순서를 관찰하세요.
4. **Task Notification 대신 Binary Semaphore로 동일한 기능을 구현하고 속도를 비교해보세요.**
   - `HAL_GetTick()`으로 실제 실행 시간을 측정하세요.
5. **configUSE_PREEMPTION을 0으로 설정**하고 Cooperative Scheduling으로 변경해보세요.
   - 태스크 동작이 어떻게 달라지는지 관찰하세요. (vTaskDelay()로만 CPU 양보)

### ❓ 생각해볼 질문

1. 우선순위가 높은 태스크만 계속 실행되면 낮은 우선순위 태스크는 언제 실행되나요? → **Starvation**
2. Software Timer 콜백에서 왜 vTaskDelay()를 사용할 수 없나요?
3. Task Notification의 가장 큰 제한은 무엇인가요? (여러 태스크에서 동시에 Notify?)
4. Mutex의 Priority Inheritance가 항상 정답일까요? 단점은? (오버헤드, 데드락 가능성)

---

## 주요 API 정리

```c
// 우선순위 제어
UBaseType_t uxTaskPriorityGet(TaskHandle_t xTask);
void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority);

// 태스크 제어
void vTaskSuspend(TaskHandle_t xTaskToSuspend);
void vTaskResume(TaskHandle_t xTaskToResume);

// Software Timer
TimerHandle_t xTimerCreate(const char *pcName, TickType_t xPeriod,
                           UBaseType_t uxAutoReload, void *pvTimerID,
                           TimerCallbackFunction_t pxCallbackFunction);
BaseType_t xTimerStart(TimerHandle_t xTimer, TickType_t xBlockTime);
BaseType_t xTimerStop(TimerHandle_t xTimer, TickType_t xBlockTime);
BaseType_t xTimerReset(TimerHandle_t xTimer, TickType_t xBlockTime);
BaseType_t xTimerChangePeriod(TimerHandle_t xTimer, TickType_t xNewPeriod,
                              TickType_t xBlockTime);

// Task Notification
BaseType_t xTaskNotifyGive(TaskHandle_t xTaskToNotify);
uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit, TickType_t xTicksToWait);
BaseType_t xTaskNotify(TaskHandle_t xTaskToNotify, uint32_t ulValue,
                       eNotifyAction eAction);
BaseType_t xTaskNotifyWait(uint32_t ulBitsToClearOnEntry,
                           uint32_t ulBitsToClearOnExit,
                           uint32_t *pulNotificationValue,
                           TickType_t xTicksToWait);

// ISR 용
void vTaskNotifyGiveFromISR(TaskHandle_t xTaskToNotify,
                            BaseType_t *pxHigherPriorityTaskWoken);

// 스택 정보
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);
```

---

## 다음 단계 예고

**4단계 - 인터럽트와 리소스 관리**: 실제 인터럽트를 RTOS와 연동하고, 스택 오버플로우 탐지, 메모리 관리 전략을 학습합니다.
- EXTI 인터럽트 → Semaphore → Task (Deferred Interrupt Handling)
- UART RX Interrupt → Queue → Data Processing
- Stack Overflow Detection
- Heap 관리 전략 (heap_1 ~ heap_5)
