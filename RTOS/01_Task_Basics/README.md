# 1단계: RTOS 기초와 태스크 관리

> **학습 목표**: RTOS의 핵심 개념인 '태스크'를 이해하고, FreeRTOS API를 사용하여
> 멀티태스킹 애플리케이션을 작성할 수 있다.

---

## 개념 학습

### 1.1 Task란?

**Task (태스크)** = RTOS에서의 '실행 단위'. 일반 C 함수와 같지만, 다음과 같은 차이가 있습니다:

| 구분 | 일반 함수 | RTOS Task |
|------|-----------|-----------|
| 실행 | 호출 시 실행 | 스케줄러가 관리 |
| 생명주기 | 함수 리턴 시 종료 | 보통 무한 루프로 영구 실행 |
| 상태 | 없음 | Ready / Running / Blocked / Suspended |
| 스택 | 호출자(caller)의 스택 사용 | 자신만의 전용 스택 보유 |
| 중단 | 불가 (한 번 실행되면 끝까지) | 언제든지 선점(preempt) 가능 |

### 1.2 Task State Machine

```
    ┌──────────┐
    │  Created │  xTaskCreate() 호출
    └────┬─────┘
         ↓
    ┌──────────┐  ←──────  ┌──────────┐
    │  Ready   │ ──────→   │ Running  │  스케줄러가 선택
    └──────────┘           └────┬─────┘
         ↑                      │
         │                 vTaskDelay()
         │                 xSemaphoreTake() (timeout)
         │                 xQueueReceive()
         ↓                      ↓
    ┌──────────┐           ┌──────────┐
    │ Blocked  │ ──────→   │  Ready   │  지연/세마포어/큐 조건 충족
    └──────────┘           └──────────┘
```

### 1.3 우선순위 (Priority)

- **0** = 최저 우선순위 (Idle 태스크)
- **configMAX_PRIORITIES - 1** = 최고 우선순위
- 우선순위가 높은 태스크가 Ready 상태가 되면 **즉시 실행됨 (Preemption)**
- 동일 우선순위: **Round-Robin** 방식으로 Time Slice 단위 교차 실행

### 1.4 vTaskDelay()의 의미

```c
// ❌ Bad: Busy-wait (CPU 100% 점유)
for (volatile int i = 0; i < 1000000; i++);

// ✅ Good: RTOS delay (CPU 양보)
vTaskDelay(pdMS_TO_TICKS(500));
```

- `vTaskDelay()`: 태스크를 **Blocked 상태**로 전환 → CPU를 다른 태스크가 사용
- `pdMS_TO_TICKS(ms)`: 밀리초를 Tick 단위로 변환 (configTICK_RATE_HZ=1000이면 1:1)

---

## 실습 예제

### 동작 설명

| 태스크 | 함수 | 우선순위 | 주기 | 동작 |
|--------|------|---------|------|------|
| LED1 | `Task_LED1` | 1 | 500ms | LED 토글 |
| LED2 | `Task_LED2` | 1 | 1000ms | LED 토글 |
| BTN | `Task_ButtonMonitor` | 2 | 200ms | 버튼 상태 UART 출력 |

### 하드웨어 연결

- **LED (LD2)**: PA5 → 두 LED 태스크가 번갈아 토글
- **Button (B1)**: PC13 → 버튼 모니터 태스크가 폴링
- **UART (ST-Link VCP)**: PA2(TX), PA3(RX) → 115200 baud

### UART 출력 예시 (TeraTerm/PuTTY)

```
=============================================
  RTOS Study - Stage 1: Task Basics
  STM32F103 NUCLEO + FreeRTOS
=============================================

[LED1] LED Toggle (500ms)
[LED1] LED Toggle (500ms)
[BTN] Button PRESSED!
[LED2] LED Toggle (1000ms)
[BTN] Button RELEASED!
[LED1] LED Toggle (500ms)
...
```

---

## 직접 해보기

### ✏️ 실습 과제 (난이도: ★☆☆☆☆)

1. **Task1의 delay를 200ms로 변경**해보고 LED 깜빡임 변화를 관찰하세요.
2. **Task_B ButtonMonitor의 우선순위를 0으로 변경**하면 어떤 일이 발생하나요?
   - 우선순위 0은 Idle 태스크와 동일 → LED 태스크가 실행되지 않을 수 있음
3. **세 번째 LED 태스크를 추가**로 생성해보세요. (PC13에 LED가 없으므로 UART 출력만)
4. `vTaskDelay()` 대신 `for` 루프 busy-wait로 변경해보고, LED 패턴 변화와 CPU 사용률을 비교하세요.

### ❓ 생각해볼 질문

1. Task1과 Task2가 동일한 우선순위인데, 왜 두 LED가 번갈아 깜빡일까요?
2. 버튼 태스크가 우선순위가 더 높은데, 버튼을 누르면 LED 깜빡임이 멈출까요? → **아니요**, LED 태스크도 Ready 상태이므로 버튼 태스크가 vTaskDelay()로 Blocked되면 LED 태스크가 실행됩니다.
3. Idle 태스크는 언제 실행되나요? → **모든 태스크가 Blocked 상태일 때**.

---

## 디버깅 팁

### RTOS-aware Debugging (STM32CubeIDE)

1. **Breakpoint**를 Task 함수 내에 설정
2. 디버그 실행 후 **Window → Show View → FreeRTOS** (또는 RTOS)
3. 각 태스크의 상태(Ready/Running/Blocked), 스택 사용량, 실행 횟수 확인 가능

### 일반적인 문제

| 증상 | 원인 | 해결 |
|------|------|------|
| 스케줄러 시작 실패 | Heap 부족 | configTOTAL_HEAP_SIZE 증가 |
| 태스크가 실행 안 됨 | 스택 부족 또는 우선순위 문제 | usStackDepth 증가, 우선순위 확인 |
| UART 출력 안 됨 | Baud rate 또는 핀 설정 오류 | ST-Link VCP 포트 확인 (115200) |

---

## 주요 API 정리

```c
// 태스크 생성
BaseType_t xTaskCreate(
    TaskFunction_t pvTaskCode,      // 태스크 함수
    const char *pcName,             // 이름 (디버깅용)
    configSTACK_DEPTH_TYPE usStackDepth, // 스택 크기 (워드)
    void *pvParameters,             // 파라미터
    UBaseType_t uxPriority,         // 우선순위 (0=최저)
    TaskHandle_t *pxCreatedTask     // 핸들 (NULL 가능)
);

// 지연 (Blocked 상태로 전환)
void vTaskDelay(const TickType_t xTicksToDelay);
void vTaskDelayUntil(TickType_t *pxPreviousWakeTime, TickType_t xTimeIncrement);

// 스케줄러 제어
void vTaskStartScheduler(void);
void vTaskEndScheduler(void);

// 태스크 제어
void vTaskDelete(TaskHandle_t xTask);
UBaseType_t uxTaskPriorityGet(TaskHandle_t xTask);
void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority);
```

---

## 다음 단계 예고

**2단계 - 태스크 동기화와 통신**: 태스크 간 데이터를 주고받고, 실행 순서를 동기화하는 방법을 배웁니다.
- Semaphore, Mutex, Queue를 사용한 IPC
