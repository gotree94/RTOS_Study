/*
 * main.h
 * STM32F103 NUCLEO - 3단계: 스케줄링과 타이머
 */

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* LED 핀 정의 */
#define LED_GPIO_PORT               GPIOA
#define LED_PIN                     GPIO_PIN_5

/* 버튼 핀 정의 */
#define BTN_GPIO_PORT               GPIOC
#define BTN_PIN                     GPIO_PIN_13

/* 예제 선택 */
#define EXAMPLE_PRIORITY_SCHED      1   /* 예제 3.1: 우선순위 스케줄링 */
#define EXAMPLE_PRIORITY_INVERSION  0   /* 예제 3.2: 우선순위 역전 */
#define EXAMPLE_SOFTWARE_TIMER      0   /* 예제 3.3: 소프트웨어 타이머 */
#define EXAMPLE_TASK_NOTIFY         0   /* 예제 3.4: 태스크 알림 */

/* 함수 프로토타입 */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);

#endif /* __MAIN_H */
