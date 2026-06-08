/*
 * main.h
 * STM32F103 NUCLEO - 1단계: RTOS 기초와 태스크 관리
 */

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/* LED 핀 정의 (NUCLEO-F103RB: LD2 = PA5) */
#define LED_GPIO_PORT               GPIOA
#define LED_PIN                     GPIO_PIN_5

/* 버튼 핀 정의 (NUCLEO-F103RB: B1 = PC13) */
#define BTN_GPIO_PORT               GPIOC
#define BTN_PIN                     GPIO_PIN_13

/* 함수 프로토타입 */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);

#endif /* __MAIN_H */
