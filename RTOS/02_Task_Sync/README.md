# 2단계: 태스크 동기화와 통신

> **학습 목표**: FreeRTOS의 동기화/통신 메커니즘인 Semaphore, Queue, Mutex를 이해하고
> 태스크 간 데이터 교환과 실행 순서 제어를 구현할 수 있다.

---

## 개념 학습

### 1. 왜 동기화가 필요한가?

태스크가 여러 개일 때 다음과 같은 문제가 발생합니다:

| 문제 | 설명 | 예시 |
|------|------|------|
| **Race Condition** | 여러 태스크가 동시에 공유 자원에 접근 | printf() 출력이 섞임 |
| **Deadlock** | 태스크들이 서로의 자원을 기다리며 무한 대기 | A가 Mutex1, B가 Mutex2 보유, 서로 상대방 요청 |
| **Starvation** | 특정 태스크가 실행 기회를 얻지 못함 | 우선순위가 낮은 태스크가 CPU를 못 받음 |

FreeRTOS는 이러한 문제를 해결하기 위해 다양한 동기화 도구를 제공합니다.

---

### 2. Binary Semaphore (이진 세마포어)

```
[Give]                     [Take]
Task A ───── Semaphore ───→ Task B
(Producer)   0 또는 1      (Consumer)
```

| 특징 | 설명 |
|------|------|
| 값의 범위 | 0 또는 1 |
| 초기값 | 0 (생성 직후 = 사용 불가) |
| 주요 용도 | **이벤트 알림**, ISR → Task 신호 |
| 특징 | 데이터 전달 없음, 순수 신호 전달 |

**동작 원리:**
1. Task B가 `xSemaphoreTake()` 호출 → 세마포어가 0이므로 Blocked
2. Task A(또는 ISR)가 `xSemaphoreGive()` 호출 → 세마포어 1로 설정
3. 스케줄러가 Task B를 Ready 상태로 전환 → 실행 재개

---

### 3. Queue (큐)

```
Producer                    Consumer
Task_A ───── [Q][Q][Q][ ] ──→ Task_B
Send        FIFO 버퍼         Receive
```

| 특징 | 설명 |
|------|------|
| 데이터 전달 | **값 복사** (포인터 아님) |
| 버퍼링 | 최대 N개 항목 저장 가능 |
| 동기화 | Queue가 비면 Receive가 Blocked, 차면 Send가 Blocked |
| 용도 | 태스크 간 데이터 스트리밍, Producer-Consumer 패턴 |

---

### 4. Mutex (뮤텍스)

```
공유 자원 (printf, UART, ...)
    ↑   ┌─────────┐
    │   │  Mutex  │  ← Priority Inheritance 지원
    │   └─────────┘
    ├── Task_A (우선순위 1)
    └── Task_B (우선순위 1)
```

| 특징 | 설명 |
|------|------|
| 소유권 | Take한 태스크만 Give 가능 |
| Priority Inheritance | 소유자의 우선순위를 대기자의 우선순위로 일시 상승 |
| 재귀 호출 | `xSemaphoreCreateRecursiveMutex()` 사용 |
| 용도 | 공유 자원 보호 (임계 영역) |

**Priority Inheritance 시나리오:**
```
1. Low(prio=1)가 Mutex 획득
2. High(prio=3)가 동일 Mutex 요청 → Blocked
3. → Low의 우선순위가 1→3으로 일시 상승 (Inheritance)
4. → Medium(prio=2)이 Low를 선점하지 못함 → Priority Inversion 방지
5. Low가 Mutex 반환 → 원래 우선순위 1로 복원
```

---

### 5. Counting Semaphore (카운팅 세마포어)

```
리소스 풀 (3개)
[리소스A] [리소스B] [리소스C]
    ↑ Counting Semaphore (초기값=3)
    ├── Worker_1 (Take → 사용 → Give)
    ├── Worker_2
    ├── Worker_3
    ├── Worker_4 (리소스 없으면 Blocked)
    └── Worker_5 (리소스 없으면 Blocked)
```

| 특징 | 설명 |
|------|------|
| 값의 범위 | 0 ~ max |
| 초기값 | 생성 시 설정 (보통 max) |
| 용도 | **리소스 풀 관리**, 제한된 버퍼 슬롯 |
| Binary Semaphore와의 차이 | 여러 개의 "자원"을 표현 |

---

## 실습 예제

### 예제 2.1: Binary Semaphore

**동작:** 버튼(PC13)을 누르면 LED(PA5)가 토글됩니다.

```
[BTN] Button Pressed! Semaphore Given.
[LED] Semaphore Taken! Toggling LED.
[BTN] Semaphore already 1 (no waiting task)
[BTN] Button Pressed! Semaphore Given.
[LED] Semaphore Taken! Toggling LED.
```

### 예제 2.2: Queue

**동작:** Producer가 1~3초 간격으로 숫자를 전송하고 Consumer가 LED로 표시합니다.

```
[Producer] Sent: 1
[Consumer] Received: 1 (LED: OFF)
[Producer] Sent: 2
[Consumer] Received: 2 (LED: ON)
[Producer] Sent: 3
[Consumer] Received: 3 (LED: OFF)
```

### 예제 2.3: Mutex

**동작:** 두 태스크가 printf를 Mutex로 보호하여 출력이 섞이지 않습니다.

```
===========================
[Task_A] Inside critical section
===========================
---------------------------
[Task_B] Inside critical section
---------------------------
===========================
[Task_A] Inside critical section
===========================
```

### 예제 2.4: Counting Semaphore

**동작:** 5개 Worker가 3개 리소스를 두고 경쟁합니다.

```
[Worker 0] Acquired resource! (use #1, remaining: 2)
[Worker 1] Acquired resource! (use #1, remaining: 1)
[Worker 2] Acquired resource! (use #1, remaining: 0)
[Worker 1] Released resource.
[Worker 3] Acquired resource! (use #1, remaining: 0)
[Worker 0] Released resource.
[Worker 4] Acquired resource! (use #1, remaining: 0)
```

---

## 실습 방법

`main.h`에서 한 번에 하나의 예제만 활성화하세요:

```c
#define EXAMPLE_BINARY_SEMAPHORE    1   // 이 예제 실행
#define EXAMPLE_QUEUE               0
#define EXAMPLE_MUTEX               0
#define EXAMPLE_COUNTING_SEM        0
```

---

## 직접 해보기

### ✏️ 실습 과제 (난이도: ★★☆☆☆)

1. **Queue 크기를 1로 변경**해보세요. Producer가 얼마나 자주 Blocked되는지 확인하세요.
2. **Mutex 예제에서 Mutex를 제거**하고 출력이 어떻게 섞이는지 관찰하세요.
   - `xSemaphoreTake()` / `xSemaphoreGive()`를 주석 처리
3. **Counting Semaphore의 max를 3에서 1로 변경**하고 동작 차이를 확인하세요.
4. **Binary Semaphore 대신 Queue(size=1)를 사용**하여 동일한 기능을 구현해보세요.
5. **Recursive Mutex**를 사용하는 예제를 추가로 만들어보세요. (중첩된 함수 호출에서 Mutex 사용)

### ❓ 생각해볼 질문

1. Binary Semaphore와 Queue(size=1)의 동작 차이는 무엇인가요?
2. Mutex 없이 printf를 사용하면 왜 출력이 섞일까요? (printf 내부 버퍼 구조)
3. Priority Inheritance가 없으면 어떤 문제가 발생하나요?
4. Counting Semaphore의 초기값을 0으로 설정하면 어떻게 되나요?

---

## 주요 API 정리

```c
// Binary Semaphore
SemaphoreHandle_t xSemaphoreCreateBinary(void);

// Mutex
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);

// Counting Semaphore
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount);

// 공통 Take / Give (Task Context)
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);

// FromISR (ISR Context에서 사용)
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);

// Queue
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken);

// Utility
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t xSemaphore);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);
UBaseType_t uxQueueSpacesAvailable(QueueHandle_t xQueue);
```

---

## 응용: 동기화 기법 선택 가이드

| 상황 | 권장 도구 | 이유 |
|------|----------|------|
| ISR → Task 알림 | Binary Semaphore | FromISR API 지원, 가벼움 |
| ISR → Task 데이터 | Queue | FromISR API 지원 |
| Task → Task 데이터 | Queue | 버퍼링, 복수 소비자 |
| 공유 자원 보호 | Mutex | Priority Inheritance |
| 함수 재진입 보호 | Recursive Mutex | 동일 태스크 중복 Take |
| 리소스 풀 관리 | Counting Semaphore | 가용 리소스 추적 |
| 단순 깨움 (성능 중요) | Task Notification | 2배 빠름, RAM 절약 |

---

## 다음 단계 예고

**3단계 - 스케줄링과 타이머**: 우선순위 기반 스케줄링의 동작 원리와 FreeRTOS Software Timer를 학습합니다.
- Priority Inversion 실습
- One-shot / Auto-reload Timer
- Task Notification (경량 동기화)
