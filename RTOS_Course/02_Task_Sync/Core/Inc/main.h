/*
 * main.h
 * STM32F103 NUCLEO - 2단계: 태스크 동기화와 통신
 */

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/* LED 핀 정의 (NUCLEO-F103RB: LD2 = PA5) */
#define LED_GPIO_PORT               GPIOA
#define LED_PIN                     GPIO_PIN_5

/* 버튼 핀 정의 (NUCLEO-F103RB: B1 = PC13) */
#define BTN_GPIO_PORT               GPIOC
#define BTN_PIN                     GPIO_PIN_13

/* 예제 선택 (1개씩 활성화하여 테스트) */
#define EXAMPLE_BINARY_SEMAPHORE    1   /* 예제 2.1: 이진 세마포어 */
#define EXAMPLE_QUEUE               0   /* 예제 2.2: 큐 */
#define EXAMPLE_MUTEX               0   /* 예제 2.3: 뮤텍스 */
#define EXAMPLE_COUNTING_SEM        0   /* 예제 2.4: 카운팅 세마포어 */

/* 함수 프로토타입 */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);

#endif /* __MAIN_H */
