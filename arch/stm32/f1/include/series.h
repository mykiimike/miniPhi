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
@brief STM32F1 registers (RM0008), only the blocks miniPhi drives
*/

#ifndef _HAVE_STM32_SERIES_H
	#define _HAVE_STM32_SERIES_H

	#include <stdint.h>

	#define __IO volatile

	/* Cortex-M3 core */
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

	/* priority bits implemented by the F1 */
	#define NVIC_PRIO_BITS 4

	/* peripherals */
	typedef struct {
		__IO uint32_t CR, CFGR, CIR, APB2RSTR, APB1RSTR, AHBENR, APB2ENR, APB1ENR, BDCR, CSR;
	} RCC_TypeDef;

	typedef struct {
		__IO uint32_t ACR, KEYR, OPTKEYR, SR, CR, AR, RESERVED, OBR, WRPR;
	} FLASH_TypeDef;

	typedef struct {
		__IO uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR;
	} GPIO_TypeDef;

	typedef struct {
		__IO uint32_t EVCR, MAPR, EXTICR[4], RESERVED, MAPR2;
	} AFIO_TypeDef;

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
		__IO uint32_t CR, CSR;
	} PWR_TypeDef;

	/* 16-bit registers on a 32-bit stride */
	typedef struct {
		__IO uint32_t CRH, CRL, PRLH, PRLL, DIVH, DIVL, CNTH, CNTL, ALRH, ALRL;
	} RTC_TypeDef;

	#define RTC    ((RTC_TypeDef *)0x40002800)
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
	#define AFIO   ((AFIO_TypeDef *)0x40010000)
	#define EXTI   ((EXTI_TypeDef *)0x40010400)
	#define GPIOA  ((GPIO_TypeDef *)0x40010800)
	#define GPIOB  ((GPIO_TypeDef *)0x40010C00)
	#define GPIOC  ((GPIO_TypeDef *)0x40011000)
	#define GPIOD  ((GPIO_TypeDef *)0x40011400)
	#define GPIOE  ((GPIO_TypeDef *)0x40011800)
	#define GPIOF  ((GPIO_TypeDef *)0x40011C00)
	#define GPIOG  ((GPIO_TypeDef *)0x40012000)
	#define ADC1   ((ADC_TypeDef *)0x40012400)
	#define TIM1   ((TIM_TypeDef *)0x40012C00)
	#define SPI1   ((SPI_TypeDef *)0x40013000)
	#define TIM8   ((TIM_TypeDef *)0x40013400)
	#define USART1 ((USART_TypeDef *)0x40013800)
	#define RCC    ((RCC_TypeDef *)0x40021000)
	#define FLASH  ((FLASH_TypeDef *)0x40022000)

	/* FSMC bank 1: BTCR holds BCR1, BTR1, BCR2, BTR2... and BWTR sits 0x104 further */
	typedef struct {
		__IO uint32_t BTCR[8];
	} FSMC_Bank1_TypeDef;

	typedef struct {
		__IO uint32_t BWTR[7];
	} FSMC_Bank1E_TypeDef;

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
	#define FLASH_SIZE_KB (*(__IO uint16_t *)0x1FFFF7E0)

	/* RCC */
	#define RCC_CR_HSION      (1UL << 0)
	#define RCC_CR_HSIRDY     (1UL << 1)
	#define RCC_CR_HSEON      (1UL << 16)
	#define RCC_CR_HSERDY     (1UL << 17)
	#define RCC_CR_PLLON      (1UL << 24)
	#define RCC_CR_PLLRDY     (1UL << 25)
	#define RCC_CFGR_SW       (3UL << 0)
	#define RCC_CFGR_SWS      (3UL << 2)
	#define RCC_CFGR_PPRE1    (7UL << 8)
	#define RCC_CFGR_PPRE2    (7UL << 11)
	#define RCC_CFGR_ADCPRE   (3UL << 14)
	#define RCC_CFGR_PLLSRC   (1UL << 16)
	#define RCC_CFGR_PLLMUL   (15UL << 18)
	#define RCC_CFGR_PLLXTPRE (1UL << 17)
	/* 0: PLL / 1.5 (72 MHz -> 48), 1: PLL (48 MHz) */
	#define RCC_CFGR_USBPRE   (1UL << 22)
	#define RCC_BDCR_LSEON    (1UL << 0)
	#define RCC_BDCR_LSERDY   (1UL << 1)
	#define RCC_BDCR_RTCSEL   (3UL << 8)
	#define RCC_BDCR_RTCSEL_LSE (1UL << 8)
	#define RCC_BDCR_RTCEN    (1UL << 15)
	#define RCC_BDCR_BDRST    (1UL << 16)

	#define RCC_AHB_FSMC      (1UL << 8)

	#define RCC_APB2_AFIO     (1UL << 0)
	#define RCC_APB2_IOPA     (1UL << 2)
	#define RCC_APB2_IOPB     (1UL << 3)
	#define RCC_APB2_IOPC     (1UL << 4)
	#define RCC_APB2_IOPD     (1UL << 5)
	#define RCC_APB2_IOPE     (1UL << 6)
	#define RCC_APB2_IOPF     (1UL << 7)
	#define RCC_APB2_IOPG     (1UL << 8)
	#define RCC_APB2_ADC1     (1UL << 9)
	#define RCC_APB2_TIM1     (1UL << 11)
	#define RCC_APB2_SPI1     (1UL << 12)
	#define RCC_APB2_TIM8     (1UL << 13)
	#define RCC_APB2_USART1   (1UL << 14)
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
	#define RCC_APB1_BKP      (1UL << 27)
	#define RCC_APB1_PWR      (1UL << 28)

	#define PWR_CR_LPDS       (1UL << 0)
	#define PWR_CR_PDDS       (1UL << 1)
	#define PWR_CR_CWUF       (1UL << 2)
	#define PWR_CR_DBP        (1UL << 8)

	#define RTC_CRH_ALRIE     (1UL << 1)
	#define RTC_CRL_ALRF      (1UL << 1)
	#define RTC_CRL_RSF       (1UL << 3)
	#define RTC_CRL_CNF       (1UL << 4)
	#define RTC_CRL_RTOFF     (1UL << 5)

	/* EXTI line wired to the RTC alarm */
	#define EXTI_LINE_RTC_ALARM 17

	/* FLASH */
	#define FLASH_ACR_LATENCY (7UL << 0)
	#define FLASH_ACR_PRFTBE  (1UL << 4)
	#define FLASH_SR_BSY      (1UL << 0)
	#define FLASH_SR_PGERR    (1UL << 2)
	#define FLASH_SR_WRPRTERR (1UL << 4)
	#define FLASH_SR_EOP      (1UL << 5)
	#define FLASH_CR_PG       (1UL << 0)
	#define FLASH_CR_PER      (1UL << 1)
	#define FLASH_CR_STRT     (1UL << 6)
	#define FLASH_CR_LOCK     (1UL << 7)
	#define FLASH_OBR_RDPRT   (1UL << 1)
	#define FLASH_KEY1        0x45670123UL
	#define FLASH_KEY2        0xCDEF89ABUL

	/* AFIO */
	#define AFIO_MAPR_SWJ_CFG        (7UL << 24)
	#define AFIO_MAPR_SWJ_JTAG_NOJNTRST (1UL << 24)
	#define AFIO_MAPR_SWJ_SWD        (2UL << 24)

	/* USART */
	#define USART_SR_PE     (1UL << 0)
	#define USART_SR_FE     (1UL << 1)
	#define USART_SR_ORE    (1UL << 3)
	#define USART_SR_RXNE   (1UL << 5)
	#define USART_SR_TC     (1UL << 6)
	#define USART_SR_TXE    (1UL << 7)
	#define USART_CR1_RE    (1UL << 2)
	#define USART_CR1_TE    (1UL << 3)
	#define USART_CR1_RXNEIE (1UL << 5)
	#define USART_CR1_TXEIE (1UL << 7)
	#define USART_CR1_PS    (1UL << 9)
	#define USART_CR1_PCE   (1UL << 10)
	#define USART_CR1_M     (1UL << 12)
	#define USART_CR1_UE    (1UL << 13)
	#define USART_CR2_STOP2 (2UL << 12)

	/* SPI */
	#define SPI_CR1_CPHA     (1UL << 0)
	#define SPI_CR1_CPOL     (1UL << 1)
	#define SPI_CR1_MSTR     (1UL << 2)
	#define SPI_CR1_BR       (7UL << 3)
	#define SPI_CR1_SPE      (1UL << 6)
	#define SPI_CR1_LSBFIRST (1UL << 7)
	#define SPI_CR1_SSI      (1UL << 8)
	#define SPI_CR1_SSM      (1UL << 9)
	#define SPI_CR2_RXNEIE   (1UL << 6)
	#define SPI_CR2_TXEIE    (1UL << 7)
	#define SPI_SR_RXNE      (1UL << 0)
	#define SPI_SR_TXE       (1UL << 1)
	#define SPI_SR_OVR       (1UL << 6)
	#define SPI_SR_BSY       (1UL << 7)

	/* I2C */
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

	/* TIM */
	#define TIM_CR1_CEN      (1UL << 0)
	#define TIM_CR1_URS      (1UL << 2)
	#define TIM_CR1_ARPE     (1UL << 7)
	#define TIM_CR1_CMS_CENTER (1UL << 5)
	#define TIM_DIER_UIE     (1UL << 0)
	#define TIM_SR_UIF       (1UL << 0)
	#define TIM_EGR_UG       (1UL << 0)
	#define TIM_BDTR_MOE     (1UL << 15)

	/* ADC */
	#define ADC_SR_EOC       (1UL << 1)
	#define ADC_CR1_EOCIE    (1UL << 5)
	#define ADC_CR2_ADON     (1UL << 0)
	#define ADC_CR2_CAL      (1UL << 2)
	#define ADC_CR2_RSTCAL   (1UL << 3)
	#define ADC_CR2_EXTSEL_SW (7UL << 17)
	#define ADC_CR2_EXTTRIG  (1UL << 20)
	#define ADC_CR2_SWSTART  (1UL << 22)
	#define ADC_CR2_TSVREFE  (1UL << 23)

	/* IRQ numbers. 0 to 42 exist on every density; 43 to 59 are high density and XL */
	typedef enum {
		WWDG_IRQn = 0,
		PVD_IRQn = 1,
		TAMPER_IRQn = 2,
		RTC_IRQn = 3,
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
		/* USB_HP shares CAN1 TX, USB_LP shares CAN1 RX0 */
		USB_HP_CAN1_TX_IRQn = 19,
		USB_LP_CAN1_RX0_IRQn = 20,
		RTCAlarm_IRQn = 41,
		TIM8_UP_IRQn = 44,
		TIM5_IRQn = 50,
		SPI3_IRQn = 51,
		UART4_IRQn = 52,
		UART5_IRQn = 53,
		TIM6_IRQn = 54,
		TIM7_IRQn = 55,
	} IRQn_Type;

	/* STM32F10X_HD and STM32F10X_XL come from the MCU entry */
	#if defined(STM32F10X_HD) || defined(STM32F10X_XL)
		#define MP_ARCH_IRQ_COUNT 60
		#define MP_ARCH_GPIO_PORTS 7
	#else
		#define MP_ARCH_IRQ_COUNT 43
		#define MP_ARCH_GPIO_PORTS 4
	#endif

#endif
