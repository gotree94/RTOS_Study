/*
 * main.h
 * STM32F103 NUCLEO - 5단계: 실무 종합 애플리케이션
 * Multi-Sensor Data Acquisition System
 */

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

/* LED 핀 */
#define LED_GPIO_PORT               GPIOA
#define LED_PIN                     GPIO_PIN_5

/* 버튼 핀 */
#define BTN_GPIO_PORT               GPIOC
#define BTN_PIN                     GPIO_PIN_13

/* 시스템 상태 */
typedef enum {
    SYS_IDLE    = 0,
    SYS_RUNNING = 1,
    SYS_ERROR   = 2,
    SYS_LOWPOWER = 3
} SystemState_t;

/* 센서 데이터 구조체 */
typedef struct {
    uint8_t  sensor_id;          /* 센서 ID (0=온도, 1=조도) */
    uint16_t raw_value;          /* 원시 ADC 값 */
    uint16_t filtered_value;     /* 필터링된 값 */
    uint32_t timestamp;          /* 샘플링 시간 (ms) */
    uint8_t  status;             /* 0=정상, 1=HIGH, 2=LOW, 3=ERROR */
} SensorData_t;

/* 시스템 설정 상수 */
#define SENSOR_QUEUE_LENGTH         16  /* 센서 데이터 큐 크기 */
#define LOG_QUEUE_LENGTH            8   /* 로그 큐 크기 */
#define SENSOR_HIGH_THRESHOLD       3000
#define SENSOR_LOW_THRESHOLD        1000
#define MOVING_AVERAGE_WINDOW       3   /* 이동 평균 윈도우 크기 */
#define ACQUISITION_INTERVAL_MS     500 /* 센서 샘플링 주기 */
#define STATUS_REPORT_INTERVAL_MS   5000

/* 함수 프로토타입 */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);

#endif /* __MAIN_H */
