# 4단계: 인터럽트와 리소스 관리

> **학습 목표**: RTOS 환경에서 인터럽트를 처리하는 방법과 메모리/스택 리소스를
> 관리하는 전략을 이해한다.

---

## 개념 학습

### 1. RTOS에서의 인터럽트 처리

#### 전통적인 방식 vs RTOS 방식

```
[전통적인 방식 (Bare-metal)]
외부 인터럽트 → ISR에서 모든 처리 (LED, UART, 계산...)
                 → ISR이 길어짐 → 다른 인터럽트 지연
                 → 실시간성 저하

[RTOS 방식 (Deferred Interrupt Handling)]
외부 인터럽트 → ISR: Semaphore Give만 (1μs)
                 → Task: LED ON, UART 출력 (100μs)
                 → ISR이 짧음 → 시스템 응답성 향상
```

#### ISR에서 FreeRTOS API 호출 규칙

| API 종류 | Task Context | ISR Context |
|----------|:-----------:|:-----------:|
| `xSemaphoreGive()` | ✅ | ❌ |
| `xSemaphoreGiveFromISR()` | ❌ | ✅ |
| `xQueueSend()` | ✅ | ❌ |
| `xQueueSendFromISR()` | ❌ | ✅ |
| `xTaskNotifyGive()` | ✅ | ❌ |
| `vTaskNotifyGiveFromISR()` | ❌ | ✅ |
| `vTaskDelay()` | ✅ | ❌ |
| `printf()` | ✅ | ❌ (권장하지 않음) |

**핵심 규칙**: ISR에서는 `FromISR` 접미사가 있는 API만 호출할 수 있습니다.

---

### 2. Deferred Interrupt Handling 패턴

```
  시간 ──────────────────────────────────────────────→

  Hardware Event (버튼 누름)
       ↓
  ┌─── ISR ────────────────────────────────────────┐
  │   xSemaphoreGiveFromISR(handle, &woken);        │  ← 1-2μs
  │   portYIELD_FROM_ISR(woken);                    │
  └─────────────────────────────────────────────────┘
       ↓ (Semaphore가 Task를 깨움)
  ┌─── Task ────────────────────────────────────────┐
  │   xSemaphoreTake(sem, portMAX_DELAY);            │
  │   HAL_GPIO_TogglePin(LED);                       │  ← 50-100μs
  │   printf("Button pressed!\r\n");                 │
  └─────────────────────────────────────────────────┘
```

**장점:**

| 항목 | ISR에서 다 처리 | Deferred 방식 |
|------|:--------------:|:-------------:|
| ISR 실행 시간 | 100μs | **1-2μs** |
| 다른 인터럽트 지연 | 큼 | **없음** |
| Task에서 사용 가능 API | 제한적 | **모든 API 사용 가능** |
| 우선순위 역전 가능성 | 없음 | **Task 레벨에서 관리** |

---

### 3. UART RX 인터럽트 + Queue 패턴

실무에서 가장 많이 사용하는 패턴 중 하나입니다.

```
UART 주변장치 → 바이트 수신
       ↓
┌─── UART RX ISR ──────────────────────────┐
│   HAL_UART_RxCpltCallback()               │
│   → xQueueSendFromISR(queue, &byte, ...)   │  ← 바이트를 Queue에 저장
│   → HAL_UART_Receive_IT() (다음 수신 시작)  │
└───────────────────────────────────────────┘
       ↓
┌─── Task_UARTProcessor ───────────────────┐
│   xQueueReceive(queue, &byte, ...)         │  ← Queue에서 바이트 읽기
│   → 버퍼링 (줄 단위)                      │
│   → 명령어 파싱 ('1'=ON, '0'=OFF, ...)    │
│   → 실행                                  │
└───────────────────────────────────────────┘
```

---

### 4. 스택 오버플로우 (Stack Overflow)

각 FreeRTOS 태스크는 고정된 크기의 스택을 가집니다.

```
메모리 맵:
┌──────────────────────────┐
│      Stack (아래로 성장)    │ ← Task 스택
├──────────────────────────┤
│        [자유 영역]         │ ← Stack이 이 영역 침범 = Overflow!
├──────────────────────────┤
│   TCB + Heap 데이터       │ ← 여기 덮어쓰면 크래시
└──────────────────────────┘
```

**탐지 방법:**

| 방법 | 설명 | 속도 | 정확도 |
|------|------|:---:|:-----:|
| Method 1 | 스택 포인터가 범위 밖인지 확인 | 빠름 | 낮음 (놓칠 수 있음) |
| Method 2 | Canary 값 변조 확인 | 느림 | 높음 |

**예방:**

```c
// 생성 시 충분한 스택 크기 할당
xTaskCreate(Task, "Name", 256, NULL, 1, NULL);
//  256 words = 1024 bytes (Cortex-M3)

// 실행 중 모니터링
UBaseType_t free = uxTaskGetStackHighWaterMark(NULL);
//  free가 0에 가까우면 스택 부족!
```

---

### 5. 메모리 관리 (heap_x.c)

FreeRTOS는 5가지 메모리 할당 방식을 제공합니다:

| 방식 | 할당 | 해제 | 단편화 병합 | 특징 |
|:----:|:---:|:---:|:---------:|------|
| heap_1 | ✅ | ❌ | - | 가장 단순, 해제 불필요한 경우 |
| heap_2 | ✅ | ✅ | ❌ | 단편화 위험 |
| heap_3 | ✅ | ✅ | malloc 사용 | 표준 malloc wrapper |
| **heap_4** | ✅ | ✅ | **✅** | 🏆 **권장 (우리가 사용)** |
| heap_5 | ✅ | ✅ | ✅ | 비연속 메모리 영역 |

---

## 실습 예제

### 예제 4.1: EXTI → Semaphore → Task

버튼(PC13)을 누르면 EXTI 인터럽트 발생 → ISR에서 Semaphore Give → Task에서 LED 토글

```
[Task] Button handler started. Press PC13 to trigger ISR.
[Task] ISR → Semaphore → Task (Deferred Interrupt Handling)

[Task] Button event received! Toggling LED.  ← 버튼 누름
[Task] Button event received! Toggling LED.  ← 다시 누름
```

### 예제 4.2: UART RX → Queue → Task

PC에서 UART로 명령어 전송 → ISR이 Queue에 저장 → Task가 처리

```
[UART] Command interface ready.
  Commands: 1=LED ON, 0=LED OFF, t=Toggle, ?=Status

> 1
[CMD] Processing: '1'
[CMD] LED ON
> t
[CMD] Processing: 't'
[CMD] LED Toggled
> ?
[CMD] LED is OFF
[CMD] System uptime: 12345 ms
```

### 예제 4.3: Stack Overflow Detection

작은 스택(32 words)을 가진 태스크가 큰 버퍼를 할당하여 오버플로우 유발

```
[Overflow] Loop=1, Free stack=12 words
[Overflow] Allocated 256-byte buffer (stack -64 words)
[Overflow] Loop=2, Free stack=6 words
[Monitor] Stack Usage Report:
[Monitor] Total tasks: 4
[Monitor] This task free stack: 84 words

=====================================
[FATAL] Stack Overflow Detected!    ← 오버플로우 발생!
  Task: Overflow
=====================================
```

### 예제 4.4: Heap Memory Management

메모리 할당/해제 반복으로 힙 상태 변화 관찰

```
[Heap] heap_4 allocator demo
  Total heap size: 10240 bytes

====== Cycle 1 ======
  Free now:       10000 bytes
  Min ever free:  10000 bytes
  Allocating 8 blocks...
    [0] malloc(50) = 0x20000000 OK
    ...
  Free after alloc: 7800 bytes
  Freeing even blocks (0,2,4,6)...
  Free after partial free: 8900 bytes  ← 7800+1100? 단편화!
```

---

## 직접 해보기

### ✏️ 실습 과제 (난이도: ★★★★☆)

1. **EXTI 예제에서 버튼을 눌렀을 때 PC13의 노이즈(채터링)를 관찰하세요.**
   - Task에 카운터를 추가하여 한 번 누를 때 Semaphore가 몇 번 Give되는지 확인
   - 디바운스 로직을 추가해보세요.
2. **UART 예제에 "led blink N" 명령어를 추가해보세요.**
   - N번 LED를 깜빡이는 명령어 파서 작성
3. **Stack Overflow 예제에서 Task_Overflow의 스택 크기를 64, 128, 256으로 늘려보세요.**
   - 각각에서 `uxTaskGetStackHighWaterMark()` 값이 어떻게 달라지는지 확인
4. **Heap 예제에서 할당 크기를 크게(500바이트 이상) 변경하고 단편화를 더 극단적으로 만들어보세요.**
5. **configUSE_TICKLESS_IDLE을 1로 설정하고 전류 소모를 비교해보세요.**
   - (고급) 저전력 모드 적용

### ❓ 생각해볼 질문

1. ISR에서 `xSemaphoreGive()` (FromISR 아님)를 호출하면 어떤 일이 발생하나요?
2. `portYIELD_FROM_ISR()`을 호출하지 않으면 어떻게 되나요?
3. UART RX 패턴에서 Queue 크기를 1로 설정하면 어떤 문제가 발생하나요?
4. Stack Overflow 탐지 Method 1과 Method 2의 차이는 무엇인가요?
5. heap_4에서 단편화를 완전히 없앨 수 없는 이유는 무엇인가요?

---

## 주요 API 정리

```c
// ISR-세이프 API (FromISR)
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore,
                                  BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue,
                              const void *pvItemToQueue,
                              BaseType_t *pxHigherPriorityTaskWoken);
void vTaskNotifyGiveFromISR(TaskHandle_t xTaskToNotify,
                             BaseType_t *pxHigherPriorityTaskWoken);
void portYIELD_FROM_ISR(BaseType_t xHigherPriorityTaskWoken);

// 인터럽트 제어 (HAL)
void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority,
                          uint32_t SubPriority);
void HAL_NVIC_EnableIRQ(IRQn_Type IRQn);
void HAL_NVIC_DisableIRQ(IRQn_Type IRQn);

// 스택 모니터링
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

// 메모리 관리
void *pvPortMalloc(size_t xWantedSize);
void vPortFree(void *pv);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);

// 태스크 정보
UBaseType_t uxTaskGetNumberOfTasks(void);
UBaseType_t uxTaskGetSystemState(TaskStatus_t *pxTaskStatusArray,
                                  UBaseType_t uxArraySize,
                                  unsigned long *pulTotalRunTime);
```

---

## 다음 단계 예고

**5단계 - 실무 종합 애플리케이션**: 지금까지 배운 모든 개념을 통합한 실제 시스템을 구축합니다.
- Multi-Sensor Data Acquisition System
- Task 간 데이터 흐름 설계
- Watchdog + Health Monitoring
- Error Handling 전략
- (종합) 전체 시스템 통합
