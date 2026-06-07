/*
 * main.h
 * STM32F103 NUCLEO - 4단계: 인터럽트와 리소스 관리
 */

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/* LED 핀 정의 */
#define LED_GPIO_PORT               GPIOA
#define LED_PIN                     GPIO_PIN_5

/* 버튼 핀 정의 */
#define BTN_GPIO_PORT               GPIOC
#define BTN_PIN                     GPIO_PIN_13

/* 예제 선택 */
#define EXAMPLE_EXTI_SEMAPHORE      1   /* 예제 4.1: EXTI → Semaphore → Task */
#define EXAMPLE_UART_QUEUE          0   /* 예제 4.2: UART RX → Queue → Task */
#define EXAMPLE_STACK_OVERFLOW      0   /* 예제 4.3: 스택 오버플로우 */
#define EXAMPLE_HEAP_MGMT           0   /* 예제 4.4: 메모리 관리 */

/* 함수 프로토타입 */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);

#endif /* __MAIN_H */
