# RTOS (Real-Time Operating System) 종류 및 분류 가이드

> 실시간 운영체제의 종류, 적용 분야, 라이선스, 개발환경 및 인증 정보를 정리한 문서입니다.

---

## 📋 목차

1. [개요](#개요)
2. [오픈소스 RTOS](#1-오픈소스-rtos)
3. [산업용 RTOS](#2-산업용-rtos-industrial-grade)
4. [방산/우주/항공 전용 RTOS](#3-방산우주항공-전용-rtos)
5. [자동차 전용 RTOS](#4-자동차-전용-rtos)
6. [분야별 비교표](#5-분야별-비교표)
7. [개발환경 비용 비교](#6-개발환경-비용-비교)
8. [인증 표준 설명](#7-인증-표준-설명)
9. [선택 가이드](#8-선택-가이드)

---

## 개요

RTOS는 정해진 시간 내에 작업을 완료해야 하는 실시간 시스템에 사용되는 운영체제입니다. 적용 분야에 따라 요구되는 안전 인증 수준이 다르며, 이에 따라 무료 오픈소스부터 고가의 상용 RTOS까지 다양한 선택지가 있습니다.

### RTOS 선택 시 고려사항

- **안전 인증 요구사항** (IEC 61508, ISO 26262, DO-178C 등)
- **개발 비용** (라이선스, 개발도구, 로열티)
- **기술 지원** (커뮤니티 vs 상용 지원)
- **하드웨어 지원** (MCU/MPU 호환성)
- **미들웨어 생태계** (네트워크, 파일시스템, USB 등)

---

## 1. 오픈소스 RTOS

### 1.1 FreeRTOS

| 항목 | 내용 |
|------|------|
| **라이선스** | MIT License (완전 무료) |
| **개발사** | Amazon Web Services (AWS) |
| **개발환경** | 무료 (GCC, IAR, Keil, STM32CubeIDE 등) |
| **적용 분야** | 일반/IoT/산업용 |
| **지원 아키텍처** | ARM Cortex-M/A/R, RISC-V, Xtensa, x86 등 |

**특징:**
- 세계에서 가장 널리 사용되는 RTOS
- AWS IoT 서비스와 완벽한 통합
- STM32CubeIDE에 기본 통합
- 풍부한 문서와 활발한 커뮤니티
- 상용 인증 버전(SafeRTOS) 별도 제공

**GitHub:** https://github.com/FreeRTOS/FreeRTOS

---

### 1.2 Zephyr

| 항목 | 내용 |
|------|------|
| **라이선스** | Apache 2.0 (무료) |
| **개발사** | Linux Foundation |
| **개발환경** | 무료 (West build system, VS Code) |
| **적용 분야** | 일반/IoT/산업용 |
| **지원 아키텍처** | ARM, RISC-V, x86, ARC, Xtensa 등 |

**특징:**
- Linux Foundation 주관 프로젝트
- 강력한 BLE/네트워크 스택
- Nordic, NXP, Intel 등 적극 지원
- 디바이스 트리(Device Tree) 기반 하드웨어 추상화
- IEC 61508 SIL 3 인증 진행 중

**GitHub:** https://github.com/zephyrproject-rtos/zephyr

---

### 1.3 Azure RTOS (ThreadX)

| 항목 | 내용 |
|------|------|
| **라이선스** | MIT License (무료, Microsoft 인수 후 오픈소스화) |
| **개발사** | Microsoft (구 Express Logic) |
| **개발환경** | 무료 |
| **적용 분야** | 일반/산업/의료용 |
| **지원 아키텍처** | ARM Cortex-M/A/R, RISC-V 등 |

**특징:**
- 풍부한 미들웨어 스택 (FileX, NetX, GUIX, USBX, LevelX)
- 기존 상용 인증 유지
- 62억 개 이상 디바이스에 배포된 검증된 코드
- 매우 작은 메모리 풋프린트 (최소 2KB ROM)

**인증:**
- IEC 61508 SIL 4
- IEC 62304 Class C
- ISO 26262 ASIL D
- EN 50128 SW-SIL 4

**GitHub:** https://github.com/eclipse-threadx/threadx

---

### 1.4 RT-Thread

| 항목 | 내용 |
|------|------|
| **라이선스** | Apache 2.0 (무료) |
| **개발사** | RT-Thread Development Team (중국) |
| **개발환경** | 무료 (RT-Thread Studio, Env Tool) |
| **적용 분야** | 일반/IoT/산업용 |
| **지원 아키텍처** | ARM, RISC-V, MIPS, Xtensa 등 |

**특징:**
- 중국 기반 오픈소스 RTOS
- 풍부한 소프트웨어 패키지 (400개 이상)
- POSIX/CMSIS 호환 API
- 전용 IDE (RT-Thread Studio) 제공
- 활발한 중국 커뮤니티

**GitHub:** https://github.com/RT-Thread/rt-thread

---

### 1.5 NuttX

| 항목 | 내용 |
|------|------|
| **라이선스** | Apache 2.0 (무료) |
| **개발사** | Apache Software Foundation |
| **개발환경** | 무료 (GCC, VS Code) |
| **적용 분야** | 일반/산업/드론/우주 |
| **지원 아키텍처** | ARM, RISC-V, x86, AVR, Xtensa 등 |

**특징:**
- POSIX 호환성 우수 (리눅스 코드 포팅 용이)
- PX4 드론 플랫폼 기본 OS
- Sony, Samsung, Xiaomi 등 상용 제품에 사용
- 소형 위성에도 적용

**GitHub:** https://github.com/apache/nuttx

---

### 1.6 RIOT

| 항목 | 내용 |
|------|------|
| **라이선스** | LGPLv2.1 (무료) |
| **개발사** | RIOT Community |
| **개발환경** | 무료 (GCC, native 포트) |
| **적용 분야** | IoT/센서네트워크 |
| **지원 아키텍처** | ARM Cortex-M, MSP430, RISC-V 등 |

**특징:**
- IoT 특화 설계
- 저전력 네트워크 프로토콜 지원 (6LoWPAN, CoAP)
- C/C++ 지원
- 학술 연구에 많이 활용

**GitHub:** https://github.com/RIOT-OS/RIOT

---

### 1.7 µC/OS-III

| 항목 | 내용 |
|------|------|
| **라이선스** | Apache 2.0 (무료, 2020년 오픈소스화) |
| **개발사** | Silicon Labs (구 Micrium) |
| **개발환경** | 무료 |
| **적용 분야** | 일반/산업용 |
| **지원 아키텍처** | ARM, ColdFire, MIPS, x86 등 |

**특징:**
- 교과서적인 코드 구조 (학습용 최적)
- 풍부한 기술 문서
- 기존 상용 인증 유지

**인증:**
- DO-178C DAL A
- IEC 61508 SIL 3
- IEC 62304 Class C

**GitHub:** https://github.com/SiliconLabs/uC-OS3

---

## 2. 산업용 RTOS (Industrial Grade)

### 2.1 VxWorks

| 항목 | 내용 |
|------|------|
| **라이선스** | 상용 (고가) |
| **개발사** | Wind River Systems |
| **개발환경** | Wind River Workbench (유료) |
| **적용 분야** | 산업/방산/우주/의료/네트워크 |
| **지원 아키텍처** | ARM, x86, PowerPC, MIPS, RISC-V |

**특징:**
- 가장 오래된 상용 RTOS (1987년~)
- 화성 탐사 로버 (Curiosity, Perseverance)에 사용
- 보잉 787, 에어버스 A400M 탑재
- 강력한 네트워킹/가상화 기능

**인증:**
- DO-178C DAL A (항공)
- IEC 61508 SIL 3 (산업)
- ISO 26262 ASIL D (자동차)
- IEC 62304 Class C (의료)

**예상 비용:**
- 개발 라이선스: $5,000 ~ $50,000+
- 런타임 로열티: 프로젝트별 협의
- 연간 유지보수: 라이선스의 15~20%

---

### 2.2 QNX

| 항목 | 내용 |
|------|------|
| **라이선스** | 상용 (비상업용 무료) |
| **개발사** | BlackBerry QNX |
| **개발환경** | QNX Momentics IDE (유료) |
| **적용 분야** | 자동차/의료/산업/로봇 |
| **지원 아키텍처** | ARM, x86 |

**특징:**
- 마이크로커널 아키텍처 (높은 안정성)
- 유일한 POSIX 인증 상용 RTOS
- 전 세계 1억 7500만 대 이상 차량에 탑재
- 자동차 IVI/ADAS 시장 선도

**인증:**
- ISO 26262 ASIL D (자동차)
- IEC 62304 Class C (의료)
- IEC 61508 SIL 3 (산업)

**예상 비용:**
- 개발 키트: $3,000 ~ $10,000
- 런타임 로열티: 디바이스당 협의

---

### 2.3 Integrity RTOS

| 항목 | 내용 |
|------|------|
| **라이선스** | 상용 (고가) |
| **개발사** | Green Hills Software |
| **개발환경** | MULTI IDE (유료) |
| **적용 분야** | 방산/항공/우주/보안 |
| **지원 아키텍처** | ARM, x86, PowerPC |

**특징:**
- 세계 최고 수준 보안 인증 (EAL 6+)
- F-35 전투기에 탑재
- 하드웨어 기반 메모리 보호
- 완벽한 시간/공간 파티셔닝

**인증:**
- EAL 6+ High Robustness (보안 최고 등급)
- DO-178C DAL A
- ISO 26262 ASIL D
- IEC 61508 SIL 4

**예상 비용:**
- 매우 고가 (프로젝트 규모에 따라 수억원대)
- 정부/방산 계약 중심

---

### 2.4 LynxOS

| 항목 | 내용 |
|------|------|
| **라이선스** | 상용 |
| **개발사** | Lynx Software Technologies |
| **개발환경** | LynxOS IDE (유료) |
| **적용 분야** | 방산/항공/통신 |
| **지원 아키텍처** | ARM, x86, PowerPC |

**특징:**
- POSIX 호환 실시간 운영체제
- 하이퍼바이저 버전 (LynxSecure) 제공
- 군용 항공기, 위성에 적용

**인증:**
- DO-178C DAL A
- ARINC 653 호환
- EAL 7 (LynxSecure)

---

### 2.5 SafeRTOS

| 항목 | 내용 |
|------|------|
| **라이선스** | 상용 (프로젝트별 라이선스) |
| **개발사** | WITTENSTEIN High Integrity Systems |
| **개발환경** | 기존 툴체인 사용 |
| **적용 분야** | 산업/의료/자동차 |
| **지원 아키텍처** | FreeRTOS 지원 플랫폼 동일 |

**특징:**
- FreeRTOS와 동일한 API
- FreeRTOS에서 쉬운 마이그레이션
- 사전 인증된 코드 제공
- 소스코드 및 설계 문서 포함

**인증:**
- IEC 61508 SIL 3
- ISO 26262 ASIL D
- IEC 62304 Class C
- FDA 510(k) 대응

**예상 비용:**
- 프로젝트별 일회성 라이선스
- 인증 수준에 따라 가격 상이

---

## 3. 방산/우주/항공 전용 RTOS

### 3.1 RTEMS

| 항목 | 내용 |
|------|------|
| **라이선스** | 무료 (수정 GPL/BSD) |
| **개발사** | OAR Corporation / 커뮤니티 |
| **개발환경** | 무료 (GCC, Eclipse) |
| **적용 분야** | 우주/과학장비/방산 |
| **지원 아키텍처** | ARM, PowerPC, SPARC, RISC-V, x86 |

**특징:**
- NASA, ESA 프로젝트 다수 사용
- 허블 우주망원경 업그레이드에 적용
- 화성 탐사선에 사용
- 완전 오픈소스로 우주용 무료 옵션

**적용 사례:**
- Hubble Space Telescope (WFC3)
- Mars missions (various)
- ESA missions
- Large Hadron Collider (CERN)

**GitHub:** https://github.com/RTEMS/rtems

---

### 3.2 PikeOS

| 항목 | 내용 |
|------|------|
| **라이선스** | 상용 |
| **개발사** | SYSGO AG (독일) |
| **개발환경** | CODEO IDE (유료) |
| **적용 분야** | 항공/방산/철도 |
| **지원 아키텍처** | ARM, x86, PowerPC |

**특징:**
- 세계 유일 안전+보안 동시 인증 하이퍼바이저
- 멀티 OS 동시 실행 (Linux, ARINC 653, POSIX)
- 유럽 항공/방산 시장 선도
- ARINC 653 Part 1 호환

**인증:**
- DO-178C DAL A
- EN 50128 SIL 4
- IEC 61508 SIL 3
- Common Criteria EAL 5+

---

### 3.3 Deos

| 항목 | 내용 |
|------|------|
| **라이선스** | 상용 |
| **개발사** | DDC-I |
| **개발환경** | OpenArbor IDE (유료) |
| **적용 분야** | 항공/방산 |
| **지원 아키텍처** | ARM, PowerPC, x86 |

**특징:**
- 시간 파티셔닝 보장
- FACE (Future Airborne Capability Environment) 호환
- 미국 방산 시장 중심

**인증:**
- DO-178C DAL A
- ARINC 653 호환
- FACE 인증

---

### 3.4 ARINC 653 호환 RTOS 목록

| RTOS | 개발사 | 비고 |
|------|--------|------|
| VxWorks 653 | Wind River | 가장 널리 사용 |
| PikeOS | SYSGO | 유럽 시장 강세 |
| LynxOS-178 | Lynx Software | POSIX 호환 |
| Deos | DDC-I | 미국 방산 중심 |
| Integrity-178 tuMP | Green Hills | 최고 보안 등급 |

---

## 4. 자동차 전용 RTOS

### 4.1 AUTOSAR OS

| 항목 | 내용 |
|------|------|
| **라이선스** | AUTOSAR 멤버십 기반 |
| **개발사** | AUTOSAR 컨소시엄 |
| **적용 분야** | 자동차 전용 |
| **플랫폼** | Classic Platform / Adaptive Platform |

**특징:**
- 자동차 산업 표준 OS
- Classic: 리소스 제약 ECU용
- Adaptive: 고성능 ECU용 (POSIX 기반)
- 전 세계 OEM/Tier 1 채택

**인증:**
- ISO 26262 ASIL D

**AUTOSAR OS 벤더:**
| 벤더 | 제품명 |
|------|--------|
| Vector | MICROSAR OS |
| EB (Elektrobit) | EB tresos AutoCore |
| ETAS | RTA-OS |
| Mentor | Nucleus AUTOSAR |

---

### 4.2 자동차 RTOS 비교

| RTOS | ASIL D 인증 | Hypervisor | POSIX | 비용 |
|------|------------|------------|-------|------|
| AUTOSAR OS | ✅ | ❌ | ❌ (Classic) | 높음 |
| QNX | ✅ | ✅ | ✅ | 중간 |
| SafeRTOS | ✅ | ❌ | ❌ | 중간 |
| VxWorks | ✅ | ✅ | ✅ | 높음 |
| PikeOS | ✅ | ✅ | ✅ | 높음 |

---

## 5. 분야별 비교표

### 5.1 분야별 RTOS 선택 가이드

| 분야 | 무료 옵션 | 유료 (인증) 옵션 | 필수 인증 |
|------|----------|-----------------|----------|
| **학습/프로토타입** | FreeRTOS, Zephyr, RT-Thread | - | 없음 |
| **일반 IoT** | FreeRTOS, Zephyr, NuttX | - | 없음 |
| **산업용 IoT** | Azure RTOS, FreeRTOS | SafeRTOS, µC/OS-III | IEC 61508 |
| **의료기기** | Azure RTOS | QNX, SafeRTOS, Integrity | IEC 62304, FDA |
| **자동차 (IVI)** | - | QNX, Linux | - |
| **자동차 (ADAS/AD)** | - | QNX, SafeRTOS, AUTOSAR | ISO 26262 ASIL B~D |
| **항공 (민간)** | RTEMS | VxWorks, Integrity, PikeOS | DO-178C DAL A~C |
| **항공 (군용)** | - | Integrity, VxWorks, LynxOS | DO-178C DAL A |
| **우주** | RTEMS | VxWorks, Integrity | NASA/ESA 기준 |
| **방산** | - | Integrity, VxWorks, LynxOS | MIL-STD, EAL 6+ |
| **철도** | - | PikeOS, VxWorks | EN 50128 |

### 5.2 주요 RTOS 기능 비교

| RTOS | 라이선스 | 최소 RAM | 최소 ROM | POSIX | 가상화 |
|------|----------|----------|----------|-------|--------|
| FreeRTOS | MIT | 4KB | 9KB | ❌ | ❌ |
| Zephyr | Apache 2.0 | 8KB | 50KB | 일부 | ❌ |
| Azure RTOS | MIT | 2KB | 2KB | ❌ | ❌ |
| NuttX | Apache 2.0 | 8KB | 32KB | ✅ | ❌ |
| VxWorks | 상용 | 20KB | 40KB | ✅ | ✅ |
| QNX | 상용 | 100KB | 200KB | ✅ | ✅ |
| Integrity | 상용 | 10KB | 30KB | ✅ | ✅ |

---

## 6. 개발환경 비용 비교

### 6.1 무료 개발환경

| RTOS | IDE/빌드시스템 | 디버거/프로브 | 비고 |
|------|---------------|--------------|------|
| FreeRTOS | STM32CubeIDE, VS Code, Eclipse | ST-Link (무료), J-Link EDU ($60) | 가장 접근성 좋음 |
| Zephyr | VS Code + West | J-Link, OpenOCD | 설정 복잡도 있음 |
| Azure RTOS | STM32CubeIDE, IAR 평가판 | ST-Link | ThreadX 통합 |
| RTEMS | Eclipse, VS Code | GDB, OpenOCD | 빌드 시스템 학습 필요 |
| NuttX | VS Code, nuttx-tools | OpenOCD | NSH 셸 편리 |
| RT-Thread | RT-Thread Studio | ST-Link, DAP-Link | 중국어 자료 많음 |

### 6.2 유료 개발환경

| 도구 | 제품명 | 예상 비용 | 대상 RTOS |
|------|--------|----------|----------|
| **IAR Systems** | IAR Embedded Workbench | $3,000 ~ $8,000/seat | 범용 (FreeRTOS, Zephyr 등) |
| **Keil (ARM)** | MDK-ARM | $3,000 ~ $10,000/seat | 범용 ARM |
| **Wind River** | Wind River Workbench | $5,000 ~ $50,000+ | VxWorks |
| **BlackBerry** | QNX Momentics | $3,000 ~ $10,000/seat | QNX |
| **Green Hills** | MULTI IDE | $10,000 ~ $100,000+ | Integrity |
| **SEGGER** | Embedded Studio | $1,000 ~ $2,000/seat | 범용 |
| **Lauterbach** | TRACE32 | $10,000 ~ $50,000+ | 범용 (고급 디버깅) |

### 6.3 총 소유 비용 (TCO) 예시

#### 시나리오 1: 학습/프로토타입
```
FreeRTOS + STM32CubeIDE + ST-Link
총 비용: $0 (보드 비용 제외)
```

#### 시나리오 2: 산업용 제품 (IEC 61508 SIL 2)
```
SafeRTOS 라이선스: $10,000 ~ $30,000
IAR EWARM: $5,000
J-Link Pro: $1,000
인증 컨설팅: 별도
총 비용: ~$40,000+
```

#### 시나리오 3: 자동차 ADAS (ISO 26262 ASIL D)
```
QNX 라이선스: $50,000+
QNX Momentics: $10,000
JTAG 디버거: $5,000
인증 패키지: 별도 협의
총 비용: $100,000+
```

#### 시나리오 4: 항공 시스템 (DO-178C DAL A)
```
VxWorks 653: $100,000+
Wind River Workbench: $50,000+
Lauterbach TRACE32: $30,000+
인증 문서 패키지: 별도
총 비용: $500,000+ (인증 포함 시 수백만 달러)
```

---

## 7. 인증 표준 설명

### 7.1 산업 안전 (IEC 61508)

| SIL | 설명 | 적용 예시 |
|-----|------|----------|
| SIL 1 | 약간의 부상 가능 | 간단한 경보 시스템 |
| SIL 2 | 심각한 부상 가능 | 공장 자동화 |
| SIL 3 | 사망 사고 가능 | 화학 플랜트, 원자력 |
| SIL 4 | 다수 사망 가능 | 원자력 발전소 |

### 7.2 자동차 (ISO 26262)

| ASIL | 설명 | 적용 예시 |
|------|------|----------|
| QM | 안전 관련 없음 | 인포테인먼트, 에어컨 |
| ASIL A | 낮은 위험 | 후방 카메라 |
| ASIL B | 중간 위험 | ABS, ESC |
| ASIL C | 높은 위험 | 에어백 |
| ASIL D | 매우 높은 위험 | 조향, 자율주행 |

### 7.3 항공 (DO-178C)

| DAL | 설명 | 고장 시 결과 |
|-----|------|-------------|
| DAL E | 영향 없음 | 영향 없음 |
| DAL D | 경미 | 승무원 부담 증가 |
| DAL C | 중대 | 승객 불편/부상 |
| DAL B | 위험 | 심각한 부상/사망 |
| DAL A | 치명적 | 항공기 추락 |

### 7.4 의료기기 (IEC 62304)

| Class | 설명 | 적용 예시 |
|-------|------|----------|
| Class A | 부상 불가능 | 데이터 표시 |
| Class B | 중상 불가능 | 진단 보조 |
| Class C | 사망/중상 가능 | 생명 유지 장치 |

### 7.5 보안 (Common Criteria)

| EAL | 설명 | 대표 제품 |
|-----|------|----------|
| EAL 1~2 | 기능 테스트 | 일반 소프트웨어 |
| EAL 3~4 | 구조적 테스트 | 기업용 보안 제품 |
| EAL 5~6 | 준정형적 설계/검증 | 정부/군용 시스템 |
| EAL 7 | 정형적 설계/검증 | 최고 기밀 시스템 |

---

## 8. 선택 가이드

### 8.1 빠른 선택 가이드

```
학습/취미 프로젝트
└── FreeRTOS (가장 많은 자료, STM32CubeIDE 통합)

스타트업/프로토타입  
├── IoT/웨어러블 → Zephyr (BLE 스택 강력)
├── AWS 연동 → FreeRTOS
└── 빠른 개발 → Azure RTOS (미들웨어 풍부)

산업용 제품 (인증 필요)
├── 비용 효율 → SafeRTOS (FreeRTOS에서 쉬운 전환)
├── 의료기기 → Azure RTOS 또는 QNX
└── 공장 자동화 → SafeRTOS, µC/OS-III

자동차
├── 인포테인먼트 → QNX, Linux
├── ADAS/자율주행 → QNX, AUTOSAR
└── Body 제어 → AUTOSAR Classic

항공/방산
├── 민간 항공 → VxWorks 653, PikeOS
├── 군용 항공 → Integrity, VxWorks
├── 우주 (예산 제약) → RTEMS
└── 우주 (상용) → VxWorks
```

### 8.2 마이그레이션 경로

```
[학습/프로토타입]          [산업용]              [안전 필수]
    FreeRTOS    ────────>   SafeRTOS   ────────>  ISO 26262/
       │                        │                 IEC 61508
       │                        │                 인증 제품
       v                        v
    Zephyr     ────────>   Zephyr 인증판
       │                  (진행 중)
       v
   Azure RTOS  ────────>   사전 인증 활용
  (ThreadX)                    │
       │                       v
       └───────────────>   QNX, VxWorks
                          (고급 기능 필요 시)
```

### 8.3 하드웨어별 추천

| MCU/MPU | 추천 RTOS | 이유 |
|---------|----------|------|
| STM32 (Cortex-M) | FreeRTOS, Azure RTOS | CubeIDE 통합 |
| Nordic nRF | Zephyr | 공식 지원 SDK |
| ESP32 | FreeRTOS | 제조사 기본 제공 |
| NXP i.MX | FreeRTOS, Zephyr, Linux | 폭넓은 선택지 |
| TI MSP/TMS | FreeRTOS, TI-RTOS | TI SDK 통합 |
| Renesas RA/RX | FreeRTOS, Azure RTOS | FSP 통합 |
| Xilinx Zynq | FreeRTOS, VxWorks | 하드 리얼타임 필요 시 |
| Raspberry Pi | Linux, Zephyr (RP2040) | 일반/실시간 선택 |

---

## 📚 참고 자료

### 공식 문서
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/)
- [Zephyr Documentation](https://docs.zephyrproject.org/)
- [Azure RTOS Documentation](https://learn.microsoft.com/en-us/azure/rtos/)
- [NuttX Documentation](https://nuttx.apache.org/docs/latest/)
- [RTEMS Documentation](https://docs.rtems.org/)

### 인증 표준
- [IEC 61508 Overview](https://www.iec.ch/functional-safety)
- [ISO 26262 Road vehicles](https://www.iso.org/standard/68383.html)
- [DO-178C](https://www.rtca.org/)
- [IEC 62304 Medical Device Software](https://www.iso.org/standard/38421.html)

### 커뮤니티
- [FreeRTOS Forum](https://forums.freertos.org/)
- [Zephyr Discord](https://discord.gg/zephyrproject)
- [Reddit r/embedded](https://www.reddit.com/r/embedded/)

---

## 📝 문서 이력

| 버전 | 날짜 | 변경 내용 |
|------|------|----------|
| 1.0 | 2025-02 | 최초 작성 |

---

## 📄 라이선스

이 문서는 교육 목적으로 작성되었으며 자유롭게 사용하실 수 있습니다.

---

> **Note**: RTOS 선택 시 최신 정보와 벤더 공식 문서를 반드시 확인하세요. 가격 및 인증 상태는 변경될 수 있습니다.
