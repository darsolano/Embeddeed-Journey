/*
 * ext_includes.h
 *
 *  Created on: Jan 29, 2025
 *      Author: Daruin Solano
 */

#ifndef HTTP_INC_EXT_INCLUDES_H_
#define HTTP_INC_EXT_INCLUDES_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
//#include "stm32l4xx_hal_iwdg.h"
#include "version.h"

#define SENSORS
#define USE_WIFI
#define USE_MBED_TLS

#ifdef RFU
#include "rfu.h"
#endif

#ifdef SENSORS
#include "stm32l475e_iot01.h"
#include "stm32l475e_iot01_accelero.h"
#include "stm32l475e_iot01_psensor.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_magneto.h"
//#include "vl53l0x_proximity.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "flash.h"


#ifdef FIREWALL_MBEDLIB
#include "firewall_wrapper.h"
#include "firewall.h"
#endif /* FIREWALL_MBEDLIB */

#include "net.h"
#include "iot_flash_config.h"
#include "msg.h"
#include "timer.h"
#include "rtc.h"

/*Define the use of time source*/
#define USE_NTP_TIMESOURCE		1
#define USE_HTTPS_TIMESOURCE		0
#define USE_PAHO_TIMER			0



#ifdef USE_MBED_TLS
extern int mbedtls_hardware_poll( void *data, unsigned char *output, size_t len, size_t *olen );
#endif /* USE_MBED_TLS */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#if defined(USE_WIFI)
#define NET_IF  NET_IF_WLAN
#elif defined(USE_LWIP)
#define NET_IF  NET_IF_ETH
#elif defined(USE_C2C)
#define NET_IF  NET_IF_C2C
#endif

#define WIFI_STORED_CREDENTIALS		1


/* Exported functions --------------------------------------------------------*/
void    Error_Handler(void);
//uint8_t Button_WaitForPush(uint32_t timeout);
//uint8_t Button_WaitForMultiPush(uint32_t timeout);
//void    Periph_Config(void);
extern SPI_HandleTypeDef hspi3;
extern RNG_HandleTypeDef hrng;
extern RTC_HandleTypeDef hrtc;
extern net_hnd_t hnet;
extern RTC_t rtc;


extern const user_config_t *lUserConfigPtr;
static volatile uint8_t button_flags = 0;


enum {BP_NOT_PUSHED=0, BP_SINGLE_PUSH, BP_MULTIPLE_PUSH};

extern uint8_t Button_WaitForMultiPush(uint32_t delay);
extern uint8_t Button_WaitForPush(uint32_t delay);
extern void Button_ISR(void);
void    Led_SetState(bool on);
void    Led_Blink(int period, int duty, int count);
void 	SPI3_IRQHandler(void);


extern void TimerInit(Timer*);
extern char TimerIsExpired(Timer*);
extern void TimerCountdownMS(Timer*, unsigned int);
extern void TimerCountdown(Timer*, unsigned int);
extern int TimerLeftMS(Timer*);



#endif /* HTTP_INC_EXT_INCLUDES_H_ */
