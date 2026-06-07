# STM32F103 FreeRTOS 통합 프로젝트 교육과정

> **HAL 개별 모듈 → FreeRTOS 멀티태스킹 통합** 심화 실습 과정  
> STM32F103 (NUCLEO-F103RB / Blue Pill) + FreeRTOS v10.x 기반

---

## 📌 과정 개요

본 과정은 STM32F103으로 개별 구현했던 HAL 드라이버 프로젝트들을 **FreeRTOS 기반 멀티태스킹 시스템**으로 통합·재설계하는 실습 중심 교육입니다.  
단순한 모듈 결합이 아니라, **RTOS의 핵심 개념(Queue, Mutex, Semaphore, EventGroup, StreamBuffer)을 반드시 적용해야만 해결**되는 구조로 설계되었습니다.

```
선수 과정: STM32F103 HAL 개별 모듈 실습 (01~36번 완료 권장)
개발 환경: STM32CubeIDE + FreeRTOS (Native API, CubeIDE .ioc에서 CMSIS_V2 선택)
대상 보드: NUCLEO-F103RB (STM32F103RBT6, 128KB Flash) / Blue Pill (STM32F103C8T6, 64KB Flash)
```

---

## 🗂️ 프로젝트 목록

| # | 프로젝트명 | 핵심 RTOS 개념 | 난이도 | 폴더 |
|---|-----------|--------------|--------|------|
| P01 | [스마트 환경 모니터링 스테이션](#p01) | Queue, Mutex, EventGroup | ⭐⭐ 중급 | [`P01_Smart_EnvMonitor`](./P01_Smart_EnvMonitor/) |
| P02 | [RTOS 기반 자율주행 로봇카](#p02) | Priority, Binary Semaphore, Watchdog | ⭐⭐⭐ 고급 | [`P02_RTOS_RobotCar`](./P02_RTOS_RobotCar/) |
| P03 | [멀티채널 데이터 로거](#p03) | StreamBuffer, DMA Notify, Priority Inversion | ⭐⭐ 중급 | [`P03_DataLogger`](./P03_DataLogger/) |
| P04 | [얼굴 표정 인식 IoT 디바이스](#p04) | Producer-Consumer, Double Buffer, Stack Sizing | ⭐⭐⭐⭐ 심화 | [`P04_FaceReaction_IoT`](./P04_FaceReaction_IoT/) |
| P05 | [GPS 트래커 + 지도 디스플레이](#p05) | MessageBuffer, UART IDLE, tickless idle | ⭐⭐⭐ 고급 | [`P05_GPS_Tracker`](./P05_GPS_Tracker/) |

---

## 📐 전체 시스템 구조

```
┌─────────────────────────────────────────────────────────┐
│                   FreeRTOS Scheduler                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │
│  │ Sensor   │ │ Display  │ │  Motor   │ │  Log /   │   │
│  │  Tasks   │ │  Tasks   │ │  Tasks   │ │  Comms   │   │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘   │
│       │             │            │             │         │
│  ─────┴─────────────┴────────────┴─────────────┴──────  │
│       Queue / Mutex / Semaphore / EventGroup / StreamBuf │
└─────────────────────────────────────────────────────────┘
│              STM32F103 HAL Drivers                       │
│  SPI · I2C · UART · ADC · TIM · DMA · GPIO · RTC        │
└─────────────────────────────────────────────────────────┘
```

---

## 📚 활용 HAL 모듈 전체 목록

본 과정에서 활용하는 사전 HAL 실습 모듈입니다.

<details>
<summary>전체 목록 펼치기 (36개)</summary>

| # | 모듈명 | 인터페이스 | 활용 프로젝트 |
|---|--------|-----------|-------------|
| 01 | LED_Blink | GPIO | 전체 (상태 표시) |
| 02 | USART_Print | UART | P01, P03, P05 |
| 03 | EXTI | GPIO IRQ | P02, P04 |
| 04 | TM_TimeBase | TIM | P01, P02, P03 |
| 05 | ADC | ADC DMA | P03 |
| 06 | HC-SR04 | GPIO/TIM | P01, P02 |
| 07 | Buzzer | TIM PWM | P01, P04 |
| 08 | CLCD | GPIO | P03 |
| 09 | ServoMotor | TIM PWM | P02, P04 |
| 10 | GM009605_LCD | SPI | - |
| 11 | LCD-SPI | SPI | P01 |
| 12 | DHT11 | GPIO 1-Wire | P01 |
| 13 | JoyStick | ADC | P03 |
| 14 | ILI9341 | SPI | P01, P04, P05 |
| 15 | CDS | ADC | P01 |
| 16 | I2C-EEPROM | I2C | P01, P05 |
| 17 | MPU6050 | I2C | P02 |
| 18 | Rotary_Encoder | TIM Encoder | P05 |
| 19 | SimpleFOC_BLDC | TIM ADC | - |
| 20 | VL53L0X | I2C | P01, P02 |
| 21 | OV7670-103 | DCMI/GPIO | P04 |
| 22 | I2C_EEPROM_GPIO | GPIO | P05 |
| 23 | Waveshare_1.3inch_oled | SPI/I2C | - |
| 24 | StepMotor_NEMA-17_L298N | GPIO/TIM | P02 |
| 25 | StepMotor_28BYJ-48_ULN2003 | GPIO | - |
| 26 | MicroSD_NS-SD04 | SPI/FATFS | P03, P05 |
| 27 | MicroSD_Arduino_Adapter | SPI/FATFS | P03 |
| 28 | USB HOST | USB | - |
| 29 | GY-GPS6MV2 | UART | P05 |
| 30 | 1.28_TFT_GC9A01 | SPI | P02 |
| 31 | SPI_Serial_Flash_Winbond | SPI | P03 |
| 32 | FATFS | FatFs MW | P03, P05 |
| 33 | TRCT5000 | ADC/GPIO | P02 |
| 34 | 2.4TFT_SPI_240_320 | SPI | P05 |
| 35 | RTC | RTC | P01, P03, P05 |
| 36 | Vector 표정 | GPIO/SPI | P04 |

</details>

---

## 🎯 RTOS 핵심 개념 커버리지

| RTOS 개념 | 설명 | 적용 프로젝트 |
|-----------|------|-------------|
| `xQueueSend / Receive` | 태스크 간 데이터 전달 | P01, P02, P04 |
| `xSemaphoreCreateMutex` | 공유 자원(I2C/SPI 버스) 보호 | P01, P03 |
| `xSemaphoreCreateBinary` | ISR → 태스크 동기화 | P02, P03 |
| `xEventGroupSetBits` | 다중 조건 이벤트 처리 | P01, P05 |
| `xStreamBufferSend` | 고속 데이터 스트리밍 | P03 |
| `xMessageBufferSend` | 가변 길이 메시지 전달 | P05 |
| `vTaskPrioritySet` | 동적 우선순위 변경 | P02 |
| `uxTaskGetStackHighWaterMark` | 스택 사용량 측정 | P04 |
| `portTICK_PERIOD_MS` | 정밀 타이밍 제어 | 전체 |
| `vTaskDelayUntil` | 주기적 태스크 정밀 실행 | P01, P03 |

---

## 📈 교육 진행 권장 순서

```
P03 데이터 로거          → RTOS 기초 (StreamBuffer, DMA)
    ↓
P01 환경 모니터링         → Queue + Mutex + EventGroup 복합 적용
    ↓
P05 GPS 트래커           → MessageBuffer + 파일시스템 통합
    ↓
P02 자율주행 로봇카        → 실시간 우선순위 제어 + Watchdog
    ↓
P04 얼굴 표정 인식 IoT    → Producer-Consumer 심화 + 메모리 최적화
```

> 각 프로젝트는 독립적으로 진행 가능하지만, 위 순서를 따를 때 학습 연계성이 가장 높습니다.

---

## 🛠️ 공통 개발 환경

| 항목 | 버전 / 사양 |
|------|------------|
| IDE | STM32CubeIDE 1.14 이상 |
| HAL | STM32CubeF1 v1.8.x |
| RTOS | FreeRTOS v10.4.x (Native API, CubeIDE .ioc → FREERTOS → Interface: CMSIS_V2) |
| 컴파일러 | ARM GCC 12.x |
| 디버거 | ST-LINK v2 / SWD |
| MCU (주력) | NUCLEO-F103RB: STM32F103RBT6 (72MHz, 128KB Flash, 20KB SRAM) |
| MCU (호환) | Blue Pill: STM32F103C8T6 (72MHz, 64KB Flash, 20KB SRAM) |

> ⚠️ **Blue Pill 사용 시 주의:** 본 과정의 모든 코드는 **NUCLEO-F103RB** 기준입니다.
> - LED: PA5 (NUCLEO) ↔ **PC13** (Blue Pill on-board LED)
> - 버튼: PC13 (NUCLEO) ↔ 외부 버튼 필요 (Blue Pill on-board LED와 충돌)
> - Flash: 64KB로 제한되므로 P04/P05는 컴파일 시 메모리 부족 가능성 있음
>
> Blue Pill 사용 시 `main.h`의 **LED/버튼 핀 정의**를 반드시 수정하세요.

### STM32CubeIDE 프로젝트 생성 (1회차)

본 과정의 모든 실습은 STM32CubeIDE에서 FreeRTOS를 포함한 프로젝트를 생성한 후,
각 단계의 소스 코드(`main.c`, `main.h`)로 교체하는 방식으로 진행됩니다.

**Step 1: 기본 프로젝트 생성**

1. **STM32CubeIDE 실행** → **File** → **New** → **STM32 Project**
2. 보드 선택: **NUCLEO-F103RB** 선택 (또는 좌측 **MCU Selector**에서 **STM32F103RBT6** 직접 선택)
3. **Next** 클릭
4. 프로젝트 설정:
   - **Project Name**: `RTOS_Study` (또는 원하는 이름)
   - **Use default location**: ✅ 체크
   - **Target Language**: `C`
   - **Targeted Binary Type**: `Executable`
   - **Target Project Type**: `STM32Cube`
5. **Next** 클릭
6. **Target & Firmware Package**: `NUCLEO-F103RB FW_F1 V1.8.7` (또는 최신 버전)
7. **Copy only the necessary library files**: ✅ 체크
8. **Finish** → 프로젝트 생성 완료

**Step 2: FreeRTOS 활성화 (.ioc 설정)**

1. 프로젝트 루트의 **`RTOS_Study.ioc`** 파일을 더블클릭하여 엽니다.
2. 좌측 **Pinout & Configuration** 탭에서
   **Middleware and Software Packs** → **FREERTOS** 선택
3. **Interface** 드롭다운에서 **`CMSIS_V2`** 선택
   > `CMSIS_V1`은 레거시 API, `CMSIS_V2`가 최신 표준입니다.
   > 본 과정의 모든 코드는 **Native FreeRTOS API**를 직접 사용하지만,
   > CubeIDE가 내부적으로 CMSIS_V2 래퍼를 통해 FreeRTOS와 연결합니다.
4. **Ctrl+S** 저장 → CubeIDE가 FreeRTOS 관련 코드 및 `FreeRTOSConfig.h` 자동 생성

**Step 3: 프로젝트에 소스 코드 적용**

각 실습 단계마다 다음 파일을 교체합니다:

| 교체할 파일 | 위치 | 설명 |
|-----------|------|------|
| `main.h` | `Core/Inc/main.h` | LED/버튼 핀 정의, 예제 선택 매크로 |
| `main.c` | `Core/Src/main.c` | FreeRTOS 태스크 및 전체 로직 |

> 예: 1단계 실습 시 `01_Task_Basics/Core/Src/main.c`를
> 프로젝트의 `Core/Src/main.c`에 복사하여 덮어씁니다.

**Step 4: FreeRTOSConfig.h 설정**

CubeIDE가 자동 생성한 `FreeRTOSConfig.h`를 본 과정에 맞게 변경합니다.
`Common/FreeRTOSConfig.h` 파일을 참고하여 수동으로 설정하거나
해당 파일로 덮어씁니다. (주요 설정 값은 아래 참조)

---

### FreeRTOSConfig.h 주요 설정

본 과정의 `Common/FreeRTOSConfig.h`에 정의된 주요 설정입니다.
(CubeIDE 자동 생성 값에서 일부를 변경하였습니다.)

```c
/*-----------------------------------------------------------
 * MCU 클럭 및 Tick (STM32F103 최대 72MHz, SysTick 1ms)
 *-----------------------------------------------------------*/
#define configCPU_CLOCK_HZ                  ((unsigned long)72000000)
#define configTICK_RATE_HZ                  ((TickType_t)1000)  /* 1ms tick */
#define configSYSTICK_CLOCK_HZ              (configCPU_CLOCK_HZ / 8)

/*-----------------------------------------------------------
 * 스케줄링
 *-----------------------------------------------------------*/
#define configUSE_PREEMPTION                1   /* 선점형 스케줄링 */
#define configUSE_TIME_SLICING              1   /* 동일 우선순위 Round-Robin */
#define configUSE_PORT_OPTIMISED_TASK_SYNC  1   /* Cortex-M3 최적화 */

/*-----------------------------------------------------------
 * 우선순위 (0=Idle, 1~4=사용 가능)
 *-----------------------------------------------------------*/
#define configMAX_PRIORITIES                (5) /* 0~4, 총 5단계 */
#define configUSE_MUTEXES                   1   /* Mutex + Priority Inheritance */

/*-----------------------------------------------------------
 * 메모리 (STM32F103RB: 20KB RAM 중 10KB를 FreeRTOS 힙으로)
 *-----------------------------------------------------------*/
#define configMINIMAL_STACK_SIZE            ((unsigned short)128)  /* 512 bytes */
#define configTOTAL_HEAP_SIZE               ((size_t)(10 * 1024)) /* 10KB */
#define configFRTOS_MEMORY_SCHEME           4   /* heap_4 (단편화 병합) */

/*-----------------------------------------------------------
 * 동기화 객체
 *-----------------------------------------------------------*/
#define configUSE_RECURSIVE_MUTEXES         1
#define configUSE_COUNTING_SEMAPHORES       1
#define configUSE_QUEUE_SETS                0
#define configUSE_TIMERS                    1   /* Software Timer 사용 */

/*-----------------------------------------------------------
 * 디버깅 및 모니터링
 *-----------------------------------------------------------*/
#define configUSE_IDLE_HOOK                 1   /* Idle Hook 활성화 */
#define configUSE_MALLOC_FAILED_HOOK        1
#define configUSE_TRACE_FACILITY            1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
#define configUSE_TASK_NOTIFICATIONS        1
```

> 전체 설정은 [`Common/FreeRTOSConfig.h`](./Common/FreeRTOSConfig.h) 참조

---

## 📁 저장소 구조

```
STM32F103_FreeRTOS_Projects/
│
├── README.md                       ← 이 파일 (전체 요약)
│
├── P01_Smart_EnvMonitor/
│   └── README.md                   ← P01 상세 문서
│
├── P02_RTOS_RobotCar/
│   └── README.md                   ← P02 상세 문서
│
├── P03_DataLogger/
│   └── README.md                   ← P03 상세 문서
│
├── P04_FaceReaction_IoT/
│   └── README.md                   ← P04 상세 문서
│
└── P05_GPS_Tracker/
    └── README.md                   ← P05 상세 문서
```

---

## 📝 라이선스 / 저작권

- 본 교육 자료는 **광주인력개발원 임베디드 시스템 과정** 교육용으로 작성되었습니다.
- 코드 및 문서의 무단 상업적 이용을 금합니다.
- STM32 HAL 라이브러리: © STMicroelectronics (MIT License)
- FreeRTOS: © Amazon Web Services (MIT License)

---

*last updated: 2025*
