/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software Foundation, *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
@file series.h
@brief STM32F407 registers (RM0090), only the blocks miniPhi drives
*/

#ifndef _HAVE_STM32_SERIES_H
	#define _HAVE_STM32_SERIES_H

	#include <stdint.h>

	#define __IO volatile

	typedef struct {
		__IO uint32_t CTRL, LOAD, VAL, CALIB;
	} SysTick_Type;

	typedef struct {
		__IO uint32_t CPUID, ICSR, VTOR, AIRCR, SCR, CCR;
		__IO uint8_t SHP[12];
		__IO uint32_t SHCSR, CFSR, HFSR, DFSR, MMFAR, BFAR, AFSR;
	} SCB_Type;

	typedef struct {
		__IO uint32_t ISER[8];
		uint32_t RESERVED0[24];
		__IO uint32_t ICER[8];
		uint32_t RESERVED1[24];
		__IO uint32_t ISPR[8];
		uint32_t RESERVED2[24];
		__IO uint32_t ICPR[8];
		uint32_t RESERVED3[24];
		__IO uint32_t IABR[8];
		uint32_t RESERVED4[56];
		__IO uint8_t IP[240];
	} NVIC_Type;

	typedef struct {
		__IO uint32_t CTRL, CYCCNT;
	} DWT_Type;

	#define SysTick ((SysTick_Type *)0xE000E010)
	#define NVIC    ((NVIC_Type *)0xE000E100)
	#define SCB     ((SCB_Type *)0xE000ED00)
	#define DWT     ((DWT_Type *)0xE0001000)
	#define DEMCR   (*(__IO uint32_t *)0xE000EDFC)
	#define DHCSR   (*(__IO uint32_t *)0xE000EDF0)
	#define CPACR   (*(__IO uint32_t *)0xE000ED88)

	#define SysTick_CTRL_ENABLE    (1UL << 0)
	#define SysTick_CTRL_TICKINT   (1UL << 1)
	#define SysTick_CTRL_CLKSOURCE (1UL << 2)
	#define SysTick_CTRL_COUNTFLAG (1UL << 16)
	#define SCB_ICSR_VECTACTIVE    0x1FFUL
	#define SCB_ICSR_PENDSTSET     (1UL << 26)
	#define SCB_ICSR_PENDSTCLR     (1UL << 25)
	#define SCB_SCR_SLEEPDEEP      (1UL << 2)
	#define DHCSR_C_DEBUGEN        (1UL << 0)
	#define SCB_AIRCR_RESET        (0x05FAUL << 16 | 1UL << 2)
	#define DEMCR_TRCENA           (1UL << 24)
	#define DWT_CTRL_CYCCNTENA     (1UL << 0)

	#define NVIC_PRIO_BITS 4

	typedef struct {
		__IO uint32_t CR, PLLCFGR, CFGR, CIR;
		__IO uint32_t AHB1RSTR, AHB2RSTR, AHB3RSTR, RESERVED0;
		__IO uint32_t APB1RSTR, APB2RSTR, RESERVED1[2];
		__IO uint32_t AHB1ENR, AHB2ENR, AHB3ENR, RESERVED2;
		__IO uint32_t APB1ENR, APB2ENR;
		uint32_t RESERVED3[2];
		__IO uint32_t AHB1LPENR, AHB2LPENR, AHB3LPENR, RESERVED4;
		__IO uint32_t APB1LPENR, APB2LPENR, RESERVED5[2];
		__IO uint32_t BDCR, CSR;
	} RCC_TypeDef;

	typedef struct {
		__IO uint32_t ACR, KEYR, OPTKEYR, SR, CR, OPTCR;
	} FLASH_TypeDef;

	typedef struct {
		__IO uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFRL, AFRH;
	} GPIO_TypeDef;

	typedef struct {
		__IO uint32_t MEMRMP, PMC, EXTICR[4];
	} SYSCFG_TypeDef;

	typedef struct {
		__IO uint32_t IMR, EMR, RTSR, FTSR, SWIER, PR;
	} EXTI_TypeDef;

	typedef struct {
		__IO uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
	} USART_TypeDef;

	typedef struct {
		__IO uint32_t CR1, CR2, SR, DR, CRCPR, RXCRCR, TXCRCR, I2SCFGR, I2SPR;
	} SPI_TypeDef;

	typedef struct {
		__IO uint32_t CR1, CR2, OAR1, OAR2, DR, SR1, SR2, CCR, TRISE;
	} I2C_TypeDef;

	typedef struct {
		__IO uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR, RCR;
		__IO uint32_t CCR[4];
		__IO uint32_t BDTR, DCR, DMAR;
	} TIM_TypeDef;

	typedef struct {
		__IO uint32_t SR, CR1, CR2, SMPR1, SMPR2, JOFR1, JOFR2, JOFR3, JOFR4, HTR, LTR;
		__IO uint32_t SQR1, SQR2, SQR3, JSQR, JDR1, JDR2, JDR3, JDR4, DR;
	} ADC_TypeDef;

	typedef struct {
		__IO uint32_t CSR, CCR, CDR;
	} ADC_Common_TypeDef;

	typedef struct {
		__IO uint32_t CR, CSR;
	} PWR_TypeDef;

	typedef struct {
		__IO uint32_t BTCR[8];
	} FSMC_Bank1_TypeDef;

	typedef struct {
		__IO uint32_t BWTR[7];
	} FSMC_Bank1E_TypeDef;

	#define RCC    ((RCC_TypeDef *)0x40023800)
	#define FLASH  ((FLASH_TypeDef *)0x40023C00)
	#define GPIOA  ((GPIO_TypeDef *)0x40020000)
	#define GPIOB  ((GPIO_TypeDef *)0x40020400)
	#define GPIOC  ((GPIO_TypeDef *)0x40020800)
	#define GPIOD  ((GPIO_TypeDef *)0x40020C00)
	#define GPIOE  ((GPIO_TypeDef *)0x40021000)
	#define GPIOF  ((GPIO_TypeDef *)0x40021400)
	#define GPIOG  ((GPIO_TypeDef *)0x40021800)
	#define GPIOH  ((GPIO_TypeDef *)0x40021C00)
	#define SYSCFG ((SYSCFG_TypeDef *)0x40013800)
	#define EXTI   ((EXTI_TypeDef *)0x40013C00)
	#define PWR    ((PWR_TypeDef *)0x40007000)

	#define TIM2   ((TIM_TypeDef *)0x40000000)
	#define TIM3   ((TIM_TypeDef *)0x40000400)
	#define TIM4   ((TIM_TypeDef *)0x40000800)
	#define TIM5   ((TIM_TypeDef *)0x40000C00)
	#define TIM6   ((TIM_TypeDef *)0x40001000)
	#define TIM7   ((TIM_TypeDef *)0x40001400)
	#define SPI2   ((SPI_TypeDef *)0x40003800)
	#define SPI3   ((SPI_TypeDef *)0x40003C00)
	#define USART2 ((USART_TypeDef *)0x40004400)
	#define USART3 ((USART_TypeDef *)0x40004800)
	#define UART4  ((USART_TypeDef *)0x40004C00)
	#define UART5  ((USART_TypeDef *)0x40005000)
	#define I2C1   ((I2C_TypeDef *)0x40005400)
	#define I2C2   ((I2C_TypeDef *)0x40005800)

	#define TIM1   ((TIM_TypeDef *)0x40010000)
	#define TIM8   ((TIM_TypeDef *)0x40010400)
	#define USART1 ((USART_TypeDef *)0x40011000)
	#define USART6 ((USART_TypeDef *)0x40011400)
	#define ADC1   ((ADC_TypeDef *)0x40012000)
	#define ADC_Common ((ADC_Common_TypeDef *)0x40012300)
	#define SPI1   ((SPI_TypeDef *)0x40013000)

	#define FSMC_Bank1  ((FSMC_Bank1_TypeDef *)0xA0000000)
	#define FSMC_Bank1E ((FSMC_Bank1E_TypeDef *)0xA0000104)
	#define FSMC_BANK1_MEMORY 0x60000000UL
	#define FSMC_BCR_MBKEN   (1UL << 0)
	#define FSMC_BCR_MWID_16 (1UL << 4)
	#define FSMC_BCR_WREN    (1UL << 12)
	#define FSMC_BCR_EXTMOD  (1UL << 14)

	#define DBGMCU_IDCODE (*(__IO uint32_t *)0xE0042000)
	#define DBGMCU_CR     (*(__IO uint32_t *)0xE0042004)
	#define DBGMCU_CR_SLEEP (1UL << 0)
	#define DBGMCU_CR_STOP  (1UL << 1)
	#define FLASH_SIZE_KB (*(__IO uint16_t *)0x1FFF7A22)

	#define RCC_CR_HSION      (1UL << 0)
	#define RCC_CR_HSIRDY     (1UL << 1)
	#define RCC_CR_HSEON      (1UL << 16)
	#define RCC_CR_HSERDY     (1UL << 17)
	#define RCC_CR_PLLON      (1UL << 24)
	#define RCC_CR_PLLRDY     (1UL << 25)
	#define RCC_CFGR_SW       (3UL << 0)
	#define RCC_CFGR_SWS      (3UL << 2)
	#define RCC_CFGR_PPRE1    (7UL << 10)
	#define RCC_CFGR_PPRE2    (7UL << 13)
	#define RCC_PLLCFGR_PLLM  (0x3FUL << 0)
	#define RCC_PLLCFGR_PLLN  (0x1FFUL << 6)
	#define RCC_PLLCFGR_PLLP  (3UL << 16)
	#define RCC_PLLCFGR_PLLSRC (1UL << 22)
	#define RCC_PLLCFGR_PLLQ  (0xFUL << 24)
	#define RCC_BDCR_LSEON    (1UL << 0)
	#define RCC_BDCR_LSERDY   (1UL << 1)
	#define RCC_BDCR_RTCSEL   (3UL << 8)
	#define RCC_BDCR_RTCSEL_LSE (1UL << 8)
	#define RCC_BDCR_RTCEN    (1UL << 15)
	#define RCC_BDCR_BDRST    (1UL << 16)

	#define RCC_AHB1_GPIOA    (1UL << 0)
	#define RCC_AHB1_GPIOB    (1UL << 1)
	#define RCC_AHB1_GPIOC    (1UL << 2)
	#define RCC_AHB1_GPIOD    (1UL << 3)
	#define RCC_AHB1_GPIOE    (1UL << 4)
	#define RCC_AHB1_GPIOF    (1UL << 5)
	#define RCC_AHB1_GPIOG    (1UL << 6)
	#define RCC_AHB1_GPIOH    (1UL << 7)
	#define RCC_AHB3_FSMC     (1UL << 0)

	#define RCC_APB2_TIM1     (1UL << 0)
	#define RCC_APB2_TIM8     (1UL << 1)
	#define RCC_APB2_USART1   (1UL << 4)
	#define RCC_APB2_USART6   (1UL << 5)
	#define RCC_APB2_ADC1     (1UL << 8)
	#define RCC_APB2_SPI1     (1UL << 12)
	#define RCC_APB2_SYSCFG   (1UL << 14)

	#define RCC_APB1_TIM2     (1UL << 0)
	#define RCC_APB1_TIM3     (1UL << 1)
	#define RCC_APB1_TIM4     (1UL << 2)
	#define RCC_APB1_TIM5     (1UL << 3)
	#define RCC_APB1_TIM6     (1UL << 4)
	#define RCC_APB1_TIM7     (1UL << 5)
	#define RCC_APB1_SPI2     (1UL << 14)
	#define RCC_APB1_SPI3     (1UL << 15)
	#define RCC_APB1_USART2   (1UL << 17)
	#define RCC_APB1_USART3   (1UL << 18)
	#define RCC_APB1_UART4    (1UL << 19)
	#define RCC_APB1_UART5    (1UL << 20)
	#define RCC_APB1_I2C1     (1UL << 21)
	#define RCC_APB1_I2C2     (1UL << 22)
	#define RCC_APB1_PWR      (1UL << 28)

	#define PWR_CR_DBP        (1UL << 8)
	/* one bit on the F407: 1 is scale 1, required at 168 MHz */
	#define PWR_CR_VOS        (1UL << 14)
	#define PWR_CSR_VOSRDY    (1UL << 14)

	#define FLASH_ACR_LATENCY (0xFUL << 0)
	#define FLASH_ACR_PRFTEN  (1UL << 8)
	#define FLASH_ACR_ICEN    (1UL << 9)
	#define FLASH_ACR_DCEN    (1UL << 10)
	#define FLASH_SR_EOP      (1UL << 0)
	#define FLASH_SR_OPERR    (1UL << 1)
	#define FLASH_SR_WRPERR   (1UL << 4)
	#define FLASH_SR_PGAERR   (1UL << 5)
	#define FLASH_SR_PGPERR   (1UL << 6)
	#define FLASH_SR_PGSERR   (1UL << 7)
	#define FLASH_SR_BSY      (1UL << 16)
	#define FLASH_SR_ERR      (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR)
	#define FLASH_CR_PG       (1UL << 0)
	#define FLASH_CR_SER      (1UL << 1)
	#define FLASH_CR_SNB      (0xFUL << 3)
	#define FLASH_CR_PSIZE    (3UL << 8)
	#define FLASH_CR_PSIZE_16 (1UL << 8)
	#define FLASH_CR_STRT     (1UL << 16)
	#define FLASH_CR_LOCK     (1UL << 31)
	#define FLASH_KEY1        0x45670123UL
	#define FLASH_KEY2        0xCDEF89ABUL

	#define USART_SR_PE      (1UL << 0)
	#define USART_SR_FE      (1UL << 1)
	#define USART_SR_ORE     (1UL << 3)
	#define USART_SR_RXNE    (1UL << 5)
	#define USART_SR_TC      (1UL << 6)
	#define USART_SR_TXE     (1UL << 7)
	#define USART_CR1_RE     (1UL << 2)
	#define USART_CR1_TE     (1UL << 3)
	#define USART_CR1_RXNEIE (1UL << 5)
	#define USART_CR1_TXEIE  (1UL << 7)
	#define USART_CR1_PS     (1UL << 9)
	#define USART_CR1_PCE    (1UL << 10)
	#define USART_CR1_M      (1UL << 12)
	#define USART_CR1_UE     (1UL << 13)
	#define USART_CR2_STOP2  (2UL << 12)

	#define SPI_CR1_CPHA     (1UL << 0)
	#define SPI_CR1_CPOL     (1UL << 1)
	#define SPI_CR1_MSTR     (1UL << 2)
	#define SPI_CR1_BR       (7UL << 3)
	#define SPI_CR1_SPE      (1UL << 6)
	#define SPI_CR1_LSBFIRST (1UL << 7)
	#define SPI_CR1_SSI      (1UL << 8)
	#define SPI_CR1_SSM      (1UL << 9)
	#define SPI_CR2_RXDMAEN  (1UL << 0)
	#define SPI_CR2_TXDMAEN  (1UL << 1)
	#define SPI_CR2_RXNEIE   (1UL << 6)
	#define SPI_CR2_TXEIE    (1UL << 7)
	#define SPI_SR_RXNE      (1UL << 0)
	#define SPI_SR_TXE       (1UL << 1)
	#define SPI_SR_OVR       (1UL << 6)
	#define SPI_SR_BSY       (1UL << 7)

	#define I2C_CR1_PE       (1UL << 0)
	#define I2C_CR1_START    (1UL << 8)
	#define I2C_CR1_STOP     (1UL << 9)
	#define I2C_CR1_ACK      (1UL << 10)
	#define I2C_CR1_SWRST    (1UL << 15)
	#define I2C_CR2_FREQ     (0x3FUL << 0)
	#define I2C_CR2_ITERREN  (1UL << 8)
	#define I2C_CR2_ITEVTEN  (1UL << 9)
	#define I2C_CR2_ITBUFEN  (1UL << 10)
	#define I2C_SR1_SB       (1UL << 0)
	#define I2C_SR1_ADDR     (1UL << 1)
	#define I2C_SR1_BTF      (1UL << 2)
	#define I2C_SR1_STOPF    (1UL << 4)
	#define I2C_SR1_RXNE     (1UL << 6)
	#define I2C_SR1_TXE      (1UL << 7)
	#define I2C_SR1_BERR     (1UL << 8)
	#define I2C_SR1_ARLO     (1UL << 9)
	#define I2C_SR1_AF       (1UL << 10)
	#define I2C_SR1_OVR      (1UL << 11)
	#define I2C_SR1_ERRORS   (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)
	#define I2C_SR2_BUSY     (1UL << 1)
	#define I2C_CCR_FS       (1UL << 15)
	#define I2C_OAR1_BIT14   (1UL << 14)

	#define TIM_CR1_CEN      (1UL << 0)
	#define TIM_CR1_URS      (1UL << 2)
	#define TIM_CR1_CMS_CENTER (1UL << 5)
	#define TIM_CR1_ARPE     (1UL << 7)
	#define TIM_DIER_UIE     (1UL << 0)
	#define TIM_SR_UIF       (1UL << 0)
	#define TIM_EGR_UG       (1UL << 0)
	#define TIM_BDTR_MOE     (1UL << 15)

	#define ADC_SR_EOC       (1UL << 1)
	#define ADC_CR1_EOCIE    (1UL << 5)
	#define ADC_CR2_ADON     (1UL << 0)
	#define ADC_CR2_SWSTART  (1UL << 30)
	#define ADC_CCR_ADCPRE   (3UL << 16)
	#define ADC_CCR_TSVREFE  (1UL << 23)

	typedef enum {
		WWDG_IRQn = 0,
		PVD_IRQn = 1,
		TAMP_STAMP_IRQn = 2,
		RTC_WKUP_IRQn = 3,
		FLASH_IRQn = 4,
		RCC_IRQn = 5,
		EXTI0_IRQn = 6,
		EXTI1_IRQn = 7,
		EXTI2_IRQn = 8,
		EXTI3_IRQn = 9,
		EXTI4_IRQn = 10,
		ADC1_2_IRQn = 18,
		EXTI9_5_IRQn = 23,
		TIM1_UP_IRQn = 25,
		TIM2_IRQn = 28,
		TIM3_IRQn = 29,
		TIM4_IRQn = 30,
		I2C1_EV_IRQn = 31,
		I2C1_ER_IRQn = 32,
		I2C2_EV_IRQn = 33,
		I2C2_ER_IRQn = 34,
		SPI1_IRQn = 35,
		SPI2_IRQn = 36,
		USART1_IRQn = 37,
		USART2_IRQn = 38,
		USART3_IRQn = 39,
		EXTI15_10_IRQn = 40,
		TIM8_UP_IRQn = 44,
		TIM5_IRQn = 50,
		SPI3_IRQn = 51,
		UART4_IRQn = 52,
		UART5_IRQn = 53,
		TIM6_IRQn = 54,
		TIM7_IRQn = 55,
		USART6_IRQn = 71,
		OTG_FS_IRQn = 67,
		OTG_HS_IRQn = 77,
	} IRQn_Type;

	#define MP_ARCH_IRQ_COUNT 82
	#define MP_ARCH_GPIO_PORTS 8

#endif
