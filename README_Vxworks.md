# VxWorks 저비용 학습 가이드

> 상용 RTOS인 VxWorks를 적은 비용으로 학습하기 위한 종합 가이드

## 📋 목차

- [개요](#개요)
- [무료/저비용 학습 방법](#무료저비용-학습-방법)
- [대안 RTOS 비교](#대안-rtos-비교)
- [VxWorks 핵심 개념](#vxworks-핵심-개념)
- [추천 학습 로드맵](#추천-학습-로드맵)
- [참고 자료](#참고-자료)

---

## 개요

VxWorks는 Wind River Systems에서 개발한 상용 실시간 운영체제(RTOS)로, 항공우주, 방위산업, 의료기기, 산업자동화 등 미션 크리티컬 시스템에서 널리 사용됩니다.

### VxWorks 특징

| 항목 | 설명 |
|------|------|
| **개발사** | Wind River Systems (Intel 자회사) |
| **라이선스** | 상용 (고가) |
| **주요 적용 분야** | 항공우주, 방산, 의료기기, 산업자동화, 네트워크 장비 |
| **지원 아키텍처** | ARM, x86, PowerPC, MIPS, RISC-V 등 |
| **인증** | DO-178C (항공), IEC 62304 (의료), ISO 26262 (자동차) |

### 학습이 어려운 이유

- 라이선스 비용이 매우 높음 (수천만 원 이상)
- 개인 개발자에게 공개되지 않음
- 평가판 접근도 기업/기관 단위로 제한적

---

## 무료/저비용 학습 방법

### 1. Wind River 공식 경로

#### 평가판 요청

```
Wind River 웹사이트: https://www.windriver.com/
평가판 신청 경로: Products → VxWorks → Free Trial
```

- 30일 평가판 제공 (기업/기관 이메일 필요)
- VxWorks Simulator (VxSim) 포함
- Wind River Workbench IDE 포함

#### Wind River Academy

```
URL: https://academy.windriver.com/
```

- 무료 온라인 교육 자료
- VxWorks 기초 과정
- 실습 환경 일부 제공

#### 교육용 라이선스

- 대학교/연구기관 소속 시 교육용 라이선스 요청 가능
- 지도교수/연구책임자 명의로 신청
- 비용 대폭 할인 또는 무료

### 2. 시뮬레이터 활용

#### VxSim (VxWorks Simulator)

평가판에 포함된 시뮬레이터로 실제 하드웨어 없이 학습 가능합니다.

**지원 기능:**
- 태스크 생성/관리
- 세마포어, 메시지 큐
- 타이머, 인터럽트 시뮬레이션
- 네트워크 스택

**제한 사항:**
- 실제 하드웨어 I/O 불가
- 실시간 성능 측정 제한
- 디바이스 드라이버 개발 제한

### 3. 중고 장비 활용

일부 VxWorks 탑재 장비는 중고로 구할 수 있습니다.

| 장비 유형 | 예상 가격 | 비고 |
|-----------|-----------|------|
| 구형 네트워크 장비 | 5~20만원 | Cisco 일부 모델 |
| 산업용 PLC/컨트롤러 | 10~50만원 | 제조사별 상이 |
| 개발 보드 (중고) | 20~100만원 | Wind River 공식 보드 |

> ⚠️ 중고 장비는 개발 환경 구축이 어렵고, 최신 버전 학습에 제한적입니다.

### 4. 실무 접근 경로

#### 관련 기업 취업

VxWorks 라이선스를 보유한 기업에서 실무 경험을 쌓는 방법입니다.

**국내 VxWorks 사용 분야:**
- 항공우주: 한국항공우주산업(KAI), 한화시스템
- 방위산업: LIG넥스원, 한화디펜스
- 의료기기: 삼성메디슨, 인성정보
- 통신장비: 삼성전자 네트워크사업부

#### 대학 연구실

- 임베디드 시스템 관련 연구실 중 VxWorks 라이선스 보유 확인
- 석/박사 과정 또는 연구원으로 참여

---

## 대안 RTOS 비교

VxWorks의 핵심 개념은 다른 RTOS와 유사합니다. 무료 RTOS로 기본기를 익힌 후 VxWorks로 전환하면 효율적입니다.

### RTOS 비교표

| RTOS | 비용 | VxWorks 유사도 | 학습 난이도 | 추천 보드 |
|------|------|----------------|-------------|-----------|
| **FreeRTOS** | 무료 | ★★★★☆ | 낮음 | STM32, ESP32 |
| **Zephyr** | 무료 | ★★★☆☆ | 중간 | nRF52, STM32 |
| **RT-Thread** | 무료 | ★★★★★ | 낮음 | STM32, RISC-V |
| **NuttX** | 무료 | ★★★★☆ | 중간 | STM32, Zynq |
| **µC/OS-III** | 교육용 무료 | ★★★★☆ | 중간 | STM32 |

### 각 RTOS 상세

#### FreeRTOS

```
공식 사이트: https://www.freertos.org/
라이선스: MIT
```

**장점:**
- 가장 널리 사용되는 오픈소스 RTOS
- AWS IoT 통합 (Amazon 인수)
- 풍부한 예제와 문서
- STM32CubeIDE 기본 통합

**VxWorks 대응 API:**

| VxWorks | FreeRTOS |
|---------|----------|
| `taskSpawn()` | `xTaskCreate()` |
| `semBCreate()` | `xSemaphoreCreateBinary()` |
| `msgQCreate()` | `xQueueCreate()` |
| `taskDelay()` | `vTaskDelay()` |

#### RT-Thread

```
공식 사이트: https://www.rt-thread.io/
라이선스: Apache 2.0
```

**장점:**
- VxWorks 스타일 API 직접 지원
- 중국 오픈소스 커뮤니티 활발
- 풍부한 미들웨어 (파일시스템, 네트워크, GUI)
- POSIX 호환 레이어

**VxWorks 호환 API 예시:**

```c
/* RT-Thread의 VxWorks 호환 API */
#include <rtthread.h>

/* 태스크 생성 - VxWorks 스타일 */
rt_thread_t tid = rt_thread_create("task1", 
                                    task_entry, 
                                    RT_NULL,
                                    512, 
                                    10, 
                                    20);
```

#### NuttX

```
공식 사이트: https://nuttx.apache.org/
라이선스: Apache 2.0
```

**장점:**
- POSIX 호환성 높음
- VxWorks와 유사한 아키텍처
- NASA, Sony 등에서 사용
- 파일시스템, 네트워크 스택 완비

#### Zephyr

```
공식 사이트: https://zephyrproject.org/
라이선스: Apache 2.0
```

**장점:**
- Linux Foundation 지원
- 현대적인 빌드 시스템 (CMake + Kconfig)
- 보안 기능 강화
- 다양한 보드 지원

---

## VxWorks 핵심 개념

대안 RTOS로 학습할 때 집중해야 할 VxWorks 핵심 개념입니다.

### 1. 태스크 관리

```c
/* VxWorks 태스크 생성 예시 */
#include <taskLib.h>

int taskId;

void myTask(int arg)
{
    while(1) {
        /* 태스크 로직 */
        taskDelay(100);  /* 100 tick 대기 */
    }
}

void startTask(void)
{
    taskId = taskSpawn("tMyTask",    /* 태스크 이름 */
                       100,          /* 우선순위 (0=최고) */
                       0,            /* 옵션 */
                       4096,         /* 스택 크기 */
                       (FUNCPTR)myTask,
                       0,0,0,0,0,0,0,0,0,0);
}
```

**FreeRTOS 대응:**

```c
/* FreeRTOS 태스크 생성 */
#include "FreeRTOS.h"
#include "task.h"

TaskHandle_t xTaskHandle;

void myTask(void *pvParameters)
{
    while(1) {
        /* 태스크 로직 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void startTask(void)
{
    xTaskCreate(myTask,
                "MyTask",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xTaskHandle);
}
```

### 2. 세마포어

```c
/* VxWorks 바이너리 세마포어 */
#include <semLib.h>

SEM_ID semBinary;

void initSemaphore(void)
{
    semBinary = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
}

void waitSemaphore(void)
{
    semTake(semBinary, WAIT_FOREVER);
}

void signalSemaphore(void)
{
    semGive(semBinary);
}
```

**FreeRTOS 대응:**

```c
/* FreeRTOS 바이너리 세마포어 */
#include "semphr.h"

SemaphoreHandle_t xSemaphore;

void initSemaphore(void)
{
    xSemaphore = xSemaphoreCreateBinary();
}

void waitSemaphore(void)
{
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
}

void signalSemaphore(void)
{
    xSemaphoreGive(xSemaphore);
}
```

### 3. 메시지 큐

```c
/* VxWorks 메시지 큐 */
#include <msgQLib.h>

MSG_Q_ID msgQId;

void initMsgQueue(void)
{
    msgQId = msgQCreate(10,      /* 최대 메시지 수 */
                        100,     /* 메시지 크기 */
                        MSG_Q_FIFO);
}

void sendMessage(char *msg, int len)
{
    msgQSend(msgQId, msg, len, WAIT_FOREVER, MSG_PRI_NORMAL);
}

void receiveMessage(char *buf, int maxLen)
{
    msgQReceive(msgQId, buf, maxLen, WAIT_FOREVER);
}
```

### 4. 인터럽트 처리

```c
/* VxWorks 인터럽트 연결 */
#include <intLib.h>

void myISR(int arg)
{
    /* 인터럽트 서비스 루틴 */
    /* 최소한의 작업만 수행 */
}

void connectInterrupt(void)
{
    intConnect(INUM_TO_IVEC(INT_NUM), myISR, 0);
    intEnable(INT_NUM);
}
```

### 5. 타이머

```c
/* VxWorks 워치독 타이머 */
#include <wdLib.h>

WDOG_ID wdId;

void timerHandler(int arg)
{
    /* 타이머 만료 시 호출 */
}

void initTimer(void)
{
    wdId = wdCreate();
    wdStart(wdId, 
            sysClkRateGet() * 5,  /* 5초 후 */
            (FUNCPTR)timerHandler, 
            0);
}
```

---

## 추천 학습 로드맵

### Phase 1: RTOS 기초 (4주)

**목표:** RTOS 핵심 개념 이해

**사용 도구:**
- FreeRTOS + STM32F411 (또는 보유 중인 STM32 보드)
- STM32CubeIDE

**학습 내용:**

| 주차 | 주제 | 실습 프로젝트 |
|------|------|---------------|
| 1주 | 태스크 생성/관리 | LED 깜빡임 멀티태스크 |
| 2주 | 세마포어, 뮤텍스 | 버튼 인터럽트 + LED 동기화 |
| 3주 | 메시지 큐 | UART 데이터 처리 |
| 4주 | 타이머, 이벤트 그룹 | 센서 데이터 주기적 수집 |

### Phase 2: 고급 RTOS (4주)

**목표:** VxWorks 수준의 기능 구현

**학습 내용:**

| 주차 | 주제 | 실습 프로젝트 |
|------|------|---------------|
| 5주 | 메모리 관리 | 동적 메모리 풀 구현 |
| 6주 | 디바이스 드라이버 | I2C/SPI 드라이버 작성 |
| 7주 | 파일시스템 | FatFS 통합 |
| 8주 | 네트워크 스택 | TCP/IP 에코 서버 |

### Phase 3: VxWorks 직접 학습 (평가판 확보 시)

**목표:** VxWorks 실습

**학습 내용:**
1. Wind River Workbench 설치 및 환경 구성
2. VxSim으로 기본 예제 실행
3. FreeRTOS 프로젝트를 VxWorks로 포팅
4. BSP (Board Support Package) 구조 분석

### 학습 체크리스트

```
[ ] RTOS 스케줄링 알고리즘 이해 (Round-Robin, Priority-based)
[ ] 우선순위 역전 문제와 해결책 (Priority Inheritance)
[ ] 교착상태(Deadlock) 방지 기법
[ ] 인터럽트 지연시간(Latency) 개념
[ ] Rate Monotonic Scheduling (RMS) 이론
[ ] 실시간 시스템 설계 패턴
[ ] 태스크 간 통신 (IPC) 메커니즘
[ ] 메모리 보호 및 MMU 활용
[ ] 부트로더와 BSP 구조
[ ] 디버깅 및 프로파일링 기법
```

---

## 참고 자료

### 공식 문서

| 자료 | URL |
|------|-----|
| VxWorks Documentation | https://docs.windriver.com/ |
| Wind River Academy | https://academy.windriver.com/ |
| FreeRTOS Documentation | https://www.freertos.org/Documentation/ |
| Zephyr Documentation | https://docs.zephyrproject.org/ |

### 추천 도서

| 도서명 | 저자 | 비고 |
|--------|------|------|
| Real-Time Concepts for Embedded Systems | Qing Li | VxWorks 기반 설명 |
| MicroC/OS-II: The Real-Time Kernel | Jean Labrosse | RTOS 내부 구조 상세 |
| Mastering the FreeRTOS Real Time Kernel | Richard Barry | FreeRTOS 공식 가이드 |
| 임베디드 시스템 프로그래밍 | 이만영 | 국내 서적 |

### 온라인 강좌

| 플랫폼 | 강좌명 | 비고 |
|--------|--------|------|
| Udemy | Mastering RTOS: Hands on FreeRTOS and STM32Fx | 유료, 실습 중심 |
| Coursera | Real-Time Embedded Systems | 이론 중심 |
| YouTube | Shawn Hymel's RTOS Series | 무료, 입문자용 |

### 커뮤니티

| 커뮤니티 | URL | 특징 |
|----------|-----|------|
| Reddit r/embedded | reddit.com/r/embedded | 영문, 활발한 토론 |
| EEVblog Forum | eevblog.com/forum | 하드웨어 중심 |
| 네이버 임베디드 카페 | cafe.naver.com/embeddedcrazyboys | 국내, 정보 공유 |

---

## 부록: API 매핑 테이블

### 태스크 관리

| 기능 | VxWorks | FreeRTOS | RT-Thread |
|------|---------|----------|-----------|
| 태스크 생성 | `taskSpawn()` | `xTaskCreate()` | `rt_thread_create()` |
| 태스크 삭제 | `taskDelete()` | `vTaskDelete()` | `rt_thread_delete()` |
| 태스크 지연 | `taskDelay()` | `vTaskDelay()` | `rt_thread_delay()` |
| 태스크 일시정지 | `taskSuspend()` | `vTaskSuspend()` | `rt_thread_suspend()` |
| 태스크 재개 | `taskResume()` | `vTaskResume()` | `rt_thread_resume()` |

### 동기화

| 기능 | VxWorks | FreeRTOS | RT-Thread |
|------|---------|----------|-----------|
| 바이너리 세마포어 | `semBCreate()` | `xSemaphoreCreateBinary()` | `rt_sem_create()` |
| 카운팅 세마포어 | `semCCreate()` | `xSemaphoreCreateCounting()` | `rt_sem_create()` |
| 뮤텍스 | `semMCreate()` | `xSemaphoreCreateMutex()` | `rt_mutex_create()` |
| 세마포어 획득 | `semTake()` | `xSemaphoreTake()` | `rt_sem_take()` |
| 세마포어 해제 | `semGive()` | `xSemaphoreGive()` | `rt_sem_release()` |

### 메시지 큐

| 기능 | VxWorks | FreeRTOS | RT-Thread |
|------|---------|----------|-----------|
| 큐 생성 | `msgQCreate()` | `xQueueCreate()` | `rt_mq_create()` |
| 메시지 전송 | `msgQSend()` | `xQueueSend()` | `rt_mq_send()` |
| 메시지 수신 | `msgQReceive()` | `xQueueReceive()` | `rt_mq_recv()` |
| 큐 삭제 | `msgQDelete()` | `vQueueDelete()` | `rt_mq_delete()` |

---

## 라이선스

이 문서는 자유롭게 사용, 수정, 배포할 수 있습니다.

---

## 기여

오류 수정이나 내용 추가는 Issue 또는 Pull Request로 기여해 주세요.
