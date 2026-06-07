# RTOS 실무 학습 커리큘럼 — STM32F103 NUCLEO + FreeRTOS

> **5단계로 배우는 RTOS:** 개념 이해 → 기본 활용 → 심화 스케줄링 → 인터럽트/리소스 → 실무 종합

---

## 개요

본 커리큘럼은 **STM32F103 NUCLEO 보드**와 **FreeRTOS**를 사용하여 RTOS를 단계별로 학습할 수 있도록 구성되었습니다.

| 항목 | 내용 |
|------|------|
| **타겟 보드** | NUCLEO-F103RB (STM32F103RBT6, Cortex-M3) |
| **개발 환경** | STM32CubeIDE (FreeRTOS 내장) |
| **RTOS** | FreeRTOS (STM32CubeIDE에 통합된 버전) |
| **언어** | C (HAL 드라이버 기반) |
| **난이도** | ★☆☆☆☆ (1단계) → ★★★★★ (5단계) |

### NUCLEO-F103RB 보드 리소스

| 리소스 | 사양 | 비고 |
|--------|------|------|
| MCU | STM32F103RBT6 | Cortex-M3 @ 72MHz |
| Flash | 128 KB | |
| SRAM | 20 KB | FreeRTOS 힙: ~10KB 할당 |
| 사용자 LED | **PA5** (LD2) | Active High |
| 사용자 버튼 | **PC13** (B1) | 누르면 LOW |
| USART2 (ST-Link VCP) | PA2(TX), PA3(RX) | 115200 baud |
| 디버거 | 내장 ST-Link/V2 | |

---

## 커리큘럼 구조

```
RTOS/
├── Common/
│   └── FreeRTOSConfig.h         # 공통 FreeRTOS 설정 (모든 스테이지 공용)
├── 01_Task_Basics/              # ★ 1단계: 태스크 기초
├── 02_Task_Sync/                # ★★ 2단계: 동기화와 통신
├── 03_Scheduling_Timers/        # ★★★ 3단계: 스케줄링과 타이머
├── 04_ISR_ResourceMgmt/         # ★★★★ 4단계: 인터럽트와 리소스
├── 05_RealWorld_App/            # ★★★★★ 5단계: 실무 종합 애플리케이션
└── README.md
```

각 스테이지는 다음 파일로 구성됩니다:

| 파일 | 설명 |
|------|------|
| `README.md` | 해당 스테이지의 학습 내용 및 개념 설명 |
| `Core/Inc/main.h` | 메인 헤더 (HAL 설정 함수 프로토타입 등) |
| `Core/Src/main.c` | 완전한 CUBEIDE 프로젝트용 main.c |

---

## CUBEIDE 프로젝트 설정 방법

각 스테이지의 `main.c`는 **STM32CubeIDE에서 FreeRTOS를 포함한 프로젝트**를 전제로 작성되었습니다.

### 1회: 기본 프로젝트 생성 (첫 스테이지만)

1. **STM32CubeIDE 실행** → File → New → STM32 Project
2. 보드 선택: **NUCLEO-F103RB**
   - 또는 MCU: **STM32F103RBT6** 직접 선택
3. 프로젝트 이름: `RTOS_Study` (또는 원하는 이름)
4. **Additional Software** → **FreeRTOS** 체크 (Middlewares → FreeRTOS)
   - 또는 Project Settings → "Software Packs"에서 FreeRTOS 선택
5. **Pinout & Configuration** 탭:
   - **SYS → Timebase Source**: **TIM1** (SysTick은 FreeRTOS가 사용)
   - **SYS → FreeRTOS**: **Enabled** (CMSIS_V2 또는 일반 버전)
   - **USART2**: Asynchronous Mode (115200 baud)
   - **PA5**: GPIO_Output (LED LD2)
   - **PC13**: GPIO_Input (Button B1)
6. Clock Configuration: HSE 8MHz → PLL → 72MHz
7. Code Generator: **Generate peripheral initialization as a pair of .c/.h files per peripheral**
8. **Generate Code**

> ⚠️ **주의**: STM32CubeIDE의 FreeRTOS 기본 설정은 본 커리큘럼의 `Common/FreeRTOSConfig.h`와 다를 수 있습니다.
> 각 스테이지 실습 시 `Core/Inc/FreeRTOSConfig.h`를 `Common/FreeRTOSConfig.h`로 **교체**하거나,
> 해당 내용을 참고하여 CubeIDE 설정을 조정하세요.

### 각 스테이지 실습 방법

```
1. CUBEIDE에서 생성한 프로젝트 열기
2. Core/Src/main.c를 해당 스테이지의 main.c로 교체
3. Core/Inc/main.h를 해당 스테이지의 main.h로 교체 (필요시)
4. FreeRTOSConfig.h를 Common/FreeRTOSConfig.h로 교체 (권장)
5. 빌드 → 보드에 다운로드 → 실행
```

> 💡 **Tip**: 각 스테이지마다 별도의 CUBEIDE 프로젝트를 만들어 두면
> 스테이지 간 비교가 용이합니다. (예: RTOS_Stage1, RTOS_Stage2, ...)

---

## 스테이지별 학습 로드맵

### 1단계: RTOS 기초와 태스크 관리
| 개념 | 실습 |
|------|------|
| Task vs Bare-metal main loop | 단일 태스크 LED Blink |
| Task state (Ready/Running/Blocked) | 2개 태스크 교차 실행 |
| Context switching | Task parameter 전달 |
| vTaskDelay()와 Busy-wait 비교 | 우선순위 차이 관찰 |

### 2단계: 태스크 동기화와 통신
| 개념 | 실습 |
|------|------|
| Binary Semaphore | 버튼 → Semaphore → LED 제어 |
| Queue | Producer-Consumer 데이터 전송 |
| Mutex / Recursive Mutex | 공유 자원 보호 (printf) |
| Counting Semaphore | 다중 리소스 관리 |

### 3단계: 스케줄링과 타이머
| 개념 | 실습 |
|------|------|
| Preemptive vs Cooperative | 우선순위 기반 선점 스케줄링 |
| Priority Inversion | 우선순위 역전 현상 시뮬레이션 |
| Software Timer | One-shot / Auto-reload 타이머 |
| Task Notification | 경량 동기화 (Notify) |

### 4단계: 인터럽트와 리소스 관리
| 개념 | 실습 |
|------|------|
| ISR → Task 통신 | EXTI 버튼 → SemaphoreFromISR |
| UART + DMA + RTOS | UART RX → Queue → Processing |
| Stack Overflow 탐지 | vApplicationStackOverflowHook |
| 메모리 관리 전략 | heap_1 ~ heap_5 비교 |

### 5단계: 실무 종합 애플리케이션
| 개념 | 실습 |
|------|------|
| 멀티 태스크 아키텍처 설계 | 센서 데이터 수집 시스템 |
| Task 간 데이터 흐름 설계 | Producer-Consumer-Processor 패턴 |
| Watchdog + Task Monitoring | IWDG + health check 태스크 |
| 저전력 Tickless 모드 | configUSE_TICKLESS_IDLE 적용 |
| 에러 처리 전략 | Assert, Error task, 복구 로직 |

---

## 학습 진행 팁

1. **순서대로 학습**하세요. 각 스테이지는 이전 스테이지의 개념을 기반으로 합니다.
2. **main.c를 직접 수정**해보세요. LED 핀을 바꾸거나, delay 시간을 조정하는 것만으로도 학습 효과가 큽니다.
3. **UART 출력을 활용**하세요. 각 예제는 `printf()`를 통해 태스크 상태 변화를 출력합니다.
   - TeraTerm, PuTTY 등으로 ST-Link VCP 포트(115200-8N1)에 연결하세요.
4. **디버깅하며 학습**하세요. RTOS-aware debugging을 통해 태스크 상태, 스택 사용량을 실시간으로 확인하세요.

---

## 참고 자료

- [FreeRTOS 공식 문서](https://www.freertos.org/Documentation/)
- [STM32CubeIDE 사용자 가이드](https://www.st.com/resource/en/user_manual/dm00629892.pdf)
- [NUCLEO-F103RB 데이터시트](https://www.st.com/resource/en/user_manual/um1724.pdf)
- [Mastering the FreeRTOS Real Time Kernel (PDF)](https://www.freertos.org/Documentation/161204_Mastering_the_FreeRTOS_Real_Time_Kernel-A_Hands-On_Tutorial_Guide.pdf)
