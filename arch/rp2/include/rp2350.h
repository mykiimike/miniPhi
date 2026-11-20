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
@file rp2350.h
@brief RP2350 registers this port touches (datasheet address map, Cortex-M33 side)
*/

#ifndef _HAVE_RP2350_REGS_H
	#define _HAVE_RP2350_REGS_H

	#include <stdint.h>

	#define __IO volatile

	/* Cortex-M33 core. The system-handler priority bytes match CMSIS:
	 * SHP[10] is PendSV, SHP[11] is SysTick. */
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
	#define ACTLR   (*(__IO uint32_t *)0xE000E008)
	#define CPACR   (*(__IO uint32_t *)0xE000ED88)
	#define FPCCR   (*(__IO uint32_t *)0xE000EF34)
	#define DWT_LAR (*(__IO uint32_t *)0xE0001FB0)

	#define SysTick_CTRL_ENABLE    (1UL << 0)
	#define SysTick_CTRL_TICKINT   (1UL << 1)
	#define SysTick_CTRL_CLKSOURCE (1UL << 2)
	#define SysTick_CTRL_COUNTFLAG (1UL << 16)
	#define SCB_ICSR_VECTACTIVE    0x1FFUL
	#define SCB_ICSR_PENDSVSET     (1UL << 28)
	#define SCB_ICSR_PENDSVCLR     (1UL << 27)
	#define SCB_ICSR_PENDSTSET     (1UL << 26)
	#define SCB_ICSR_PENDSTCLR     (1UL << 25)
	#define SCB_SCR_SLEEPDEEP      (1UL << 2)
	#define SCB_AIRCR_VECTKEY      (0x05FAUL << 16)
	#define SCB_AIRCR_RESET        (SCB_AIRCR_VECTKEY | (1UL << 2))
	#define DHCSR_C_DEBUGEN        (1UL << 0)
	#define DEMCR_TRCENA           (1UL << 24)
	#define DWT_CTRL_CYCCNTENA     (1UL << 0)
	/* SRAM is non-shareable in the default map: without this, ldaex/strex
	 * only see the local monitor and do not exclude the other core */
	#define ACTLR_EXTEXCLALL       (1UL << 29)
	#define CPACR_FPU_FULL         (0xFUL << 20)
	#define FPCCR_ASPEN            (1UL << 31)
	#define FPCCR_LSPEN            (1UL << 30)
	#define DWT_LAR_UNLOCK         0xC5ACCE55UL

	/* four implemented priority bits, same encoding as the STM32 port */
	#define NVIC_PRIO_BITS 4

	#define RP2350_XIP_BASE        0x10000000UL
	#define RP2350_SRAM_BASE       0x20000000UL
	#define RP2350_SRAM_END        0x20082000UL
	#define RP2350_SYSINFO_BASE    0x40000000UL
	#define RP2350_CLOCKS_BASE     0x40010000UL
	#define RP2350_PSM_BASE        0x40018000UL
	#define RP2350_TIMER0_BASE     0x400b0000UL
	#define RP2350_TICKS_BASE      0x40108000UL

	/* TICKS: one generator a consumer (proc0, proc1, timer0, timer1,
	 * watchdog, riscv), CTRL/CYCLES/COUNT each. CYCLES is clk_ref
	 * cycles per microsecond. TIMERAWL then counts microseconds. */
	#define TICKS_TIMER0_CTRL      RP2350_REG(RP2350_TICKS_BASE, 0x18)
	#define TICKS_TIMER0_CYCLES    RP2350_REG(RP2350_TICKS_BASE, 0x1c)
	#define RP2350_WATCHDOG_BASE   0x400d8000UL
	#define WATCHDOG_CTRL          RP2350_REG(RP2350_WATCHDOG_BASE, 0x00)
	#define WATCHDOG_CTRL_TRIGGER  (1UL << 31)
	#define TIMER0_TIMERAWL        RP2350_REG(RP2350_TIMER0_BASE, 0x28)
	#define RP2350_RESETS_BASE     0x40020000UL
	#define RP2350_IO_BANK0_BASE   0x40028000UL
	#define RP2350_PADS_BANK0_BASE 0x40038000UL
	#define RP2350_XOSC_BASE       0x40048000UL
	#define RP2350_PLL_SYS_BASE    0x40050000UL
	#define RP2350_PLL_USB_BASE    0x40058000UL
	#define RP2350_UART0_BASE      0x40070000UL
	#define RP2350_UART1_BASE      0x40078000UL
	#define RP2350_SPI0_BASE       0x40080000UL
	#define RP2350_SPI1_BASE       0x40088000UL
	#define RP2350_I2C0_BASE       0x40090000UL
	#define RP2350_I2C1_BASE       0x40098000UL
	#define RP2350_PWM_BASE        0x400a8000UL
	#define RP2350_SIO_BASE        0xD0000000UL

	#define RP2350_REG(base, off) (*(__IO uint32_t *)((base) + (off)))

	/* RESETS: a set bit holds the peripheral in reset */
	#define RESETS_RESET      RP2350_REG(RP2350_RESETS_BASE, 0x00)
	#define RESETS_RESET_DONE RP2350_REG(RP2350_RESETS_BASE, 0x08)
	#define RESETS_ADC        (1UL << 0)
	#define RESETS_I2C0       (1UL << 4)
	#define RESETS_I2C1       (1UL << 5)
	#define RESETS_IO_BANK0   (1UL << 6)
	#define RESETS_PLL_SYS    (1UL << 14)
	#define RESETS_PLL_USB    (1UL << 15)
	#define RESETS_PWM        (1UL << 16)
	#define RESETS_PADS_BANK0 (1UL << 9)
	#define RESETS_SPI0       (1UL << 18)
	#define RESETS_SPI1       (1UL << 19)
	#define RESETS_TIMER0     (1UL << 23)
	#define RESETS_UART0      (1UL << 26)
	#define RESETS_UART1      (1UL << 27)

	/* XOSC: 12-bit enable and range codes, not the RP2040 single-bit form */
	#define XOSC_CTRL    RP2350_REG(RP2350_XOSC_BASE, 0x00)
	#define XOSC_STATUS  RP2350_REG(RP2350_XOSC_BASE, 0x04)
	#define XOSC_STARTUP RP2350_REG(RP2350_XOSC_BASE, 0x0C)
	/* counts down at the crystal rate and stops at 0 */
	#define XOSC_COUNT   RP2350_REG(RP2350_XOSC_BASE, 0x10)
	#define XOSC_CTRL_ENABLE       (0xFABUL << 12)
	#define XOSC_CTRL_RANGE_12MHZ  0xAA0UL
	/* 1 ms of 256-cycle units, x64 like pico-sdk for the Waveshare crystals */
	#define XOSC_STARTUP_DELAY     (((12000UL + 128UL) / 256UL) * 64UL)
	#define XOSC_STATUS_STABLE     (1UL << 31)

	/* PLL_SYS. VCO = 12 MHz * 125 = 1500 MHz, /5 /2 = 150 MHz. */
	#define PLL_SYS_CS        RP2350_REG(RP2350_PLL_SYS_BASE, 0x00)
	#define PLL_SYS_PWR       RP2350_REG(RP2350_PLL_SYS_BASE, 0x04)
	#define PLL_SYS_FBDIV_INT RP2350_REG(RP2350_PLL_SYS_BASE, 0x08)
	#define PLL_SYS_PRIM      RP2350_REG(RP2350_PLL_SYS_BASE, 0x0C)
	#define PLL_CS_LOCK       (1UL << 31)
	#define PLL_CS_REFDIV     0x3FUL
	#define PLL_PWR_VCOPD     (1UL << 5)
	#define PLL_PWR_POSTDIVPD (1UL << 3)
	#define PLL_PWR_DSMPD     (1UL << 2)
	#define PLL_PWR_PD        (1UL << 0)
	#define PLL_PRIM_POSTDIV1_LSB 16
	#define PLL_PRIM_POSTDIV2_LSB 12

	/* CLOCKS. Integer divisor lives in bits 31:16 (RP2350, not the RP2040 24.8 layout). */
	#define CLOCKS_CLK_REF_CTRL     RP2350_REG(RP2350_CLOCKS_BASE, 0x30)
	#define CLOCKS_CLK_REF_DIV      RP2350_REG(RP2350_CLOCKS_BASE, 0x34)
	#define CLOCKS_CLK_REF_SELECTED RP2350_REG(RP2350_CLOCKS_BASE, 0x38)
	#define CLOCKS_CLK_SYS_CTRL     RP2350_REG(RP2350_CLOCKS_BASE, 0x3C)
	#define CLOCKS_CLK_SYS_DIV      RP2350_REG(RP2350_CLOCKS_BASE, 0x40)
	#define CLOCKS_CLK_SYS_SELECTED RP2350_REG(RP2350_CLOCKS_BASE, 0x44)
	#define CLOCKS_CLK_PERI_CTRL    RP2350_REG(RP2350_CLOCKS_BASE, 0x48)
	#define CLOCKS_CLK_PERI_DIV     RP2350_REG(RP2350_CLOCKS_BASE, 0x4C)
	#define CLOCKS_REF_SRC_XOSC     2UL
	#define CLOCKS_SYS_SRC_REF      0UL
	#define CLOCKS_SYS_SRC_AUX      1UL
	#define CLOCKS_SYS_AUX_PLL      0UL
	#define CLOCKS_SYS_AUX_ROSC     2UL
	#define CLOCKS_PERI_ENABLE      (1UL << 11)
	#define CLOCKS_PERI_AUX_SYS     0UL
	/* PLL_USB, so the SPI and UART bit clocks stay put when clk_sys moves. */
	#define CLOCKS_PERI_AUX_USB     2UL
	#define CLOCKS_DIV_INT1         (1UL << 16)

	/* SIO GPIO, low 32 pins. RP2350 interleaves the high-bank register
	 * after each low-bank one, so the set/clear/OE offsets are not the
	 * RP2040 ones. GPIO0-29 of the RP2350A fit in the low word. */
	/* PSM FRCE_OFF holds a block powered down. Bit 24 is core 1. */
	#define PSM_FRCE_OFF         RP2350_REG(RP2350_PSM_BASE, 0x004)
	#define PSM_FRCE_OFF_SET     RP2350_REG(RP2350_PSM_BASE, 0x2004)
	#define PSM_FRCE_OFF_CLR     RP2350_REG(RP2350_PSM_BASE, 0x3004)
	#define PSM_PROC1            (1UL << 24)

	#define SIO_CPUID            RP2350_REG(RP2350_SIO_BASE, 0x000)
	#define SIO_FIFO_ST          RP2350_REG(RP2350_SIO_BASE, 0x050)
	#define SIO_FIFO_WR          RP2350_REG(RP2350_SIO_BASE, 0x054)
	#define SIO_FIFO_RD          RP2350_REG(RP2350_SIO_BASE, 0x058)
	#define SIO_FIFO_ST_VLD      (1UL << 0)
	#define SIO_FIFO_ST_RDY      (1UL << 1)
	#define SIO_GPIO_IN          RP2350_REG(RP2350_SIO_BASE, 0x004)
	#define SIO_GPIO_OUT         RP2350_REG(RP2350_SIO_BASE, 0x010)
	#define SIO_GPIO_OUT_SET     RP2350_REG(RP2350_SIO_BASE, 0x018)
	#define SIO_GPIO_OUT_CLR     RP2350_REG(RP2350_SIO_BASE, 0x020)
	#define SIO_GPIO_OUT_XOR     RP2350_REG(RP2350_SIO_BASE, 0x028)
	#define SIO_GPIO_OE          RP2350_REG(RP2350_SIO_BASE, 0x030)
	#define SIO_GPIO_OE_SET      RP2350_REG(RP2350_SIO_BASE, 0x038)
	#define SIO_GPIO_OE_CLR      RP2350_REG(RP2350_SIO_BASE, 0x040)

	/* IO_BANK0: STATUS at 8*n, CTRL at 8*n+4. FUNCSEL is bits 4:0. */
	#define IO_BANK0_GPIO_CTRL(pin) RP2350_REG(RP2350_IO_BANK0_BASE, 0x004 + 8 * (pin))
	#define IO_GPIO_FUNCSEL_SPI  1UL
	#define IO_GPIO_FUNCSEL_UART 2UL
	#define IO_GPIO_FUNCSEL_I2C  3UL
	#define IO_GPIO_FUNCSEL_PWM  4UL
	#define IO_GPIO_FUNCSEL_SIO  5UL
	#define IO_GPIO_FUNCSEL_NULL 0x1FUL
	#define IO_GPIO_FUNCSEL_MASK 0x1FUL

	/* one register covers 8 pins, 4 event bits each: low, high, fall, rise */
	#define IO_BANK0_INTR(reg)       RP2350_REG(RP2350_IO_BANK0_BASE, 0x230 + 4 * (reg))
	#define IO_BANK0_PROC0_INTE(reg) RP2350_REG(RP2350_IO_BANK0_BASE, 0x248 + 4 * (reg))
	#define IO_BANK0_PROC0_INTS(reg) RP2350_REG(RP2350_IO_BANK0_BASE, 0x278 + 4 * (reg))
	#define IO_GPIO_IRQ_FALL (1UL << 2)
	#define IO_GPIO_IRQ_RISE (1UL << 3)
	#define IO_GPIO_IRQ_EDGE (IO_GPIO_IRQ_FALL | IO_GPIO_IRQ_RISE)
	#define IO_IRQ_BANK0 21

	/* PADS_BANK0: GPIO0 at +0x04, then one word per pin */
	#define PADS_BANK0_GPIO(pin) RP2350_REG(RP2350_PADS_BANK0_BASE, 0x004 + 4 * (pin))
	#define PADS_SLEWFAST (1UL << 0)
	#define PADS_SCHMITT  (1UL << 1)
	#define PADS_PDE      (1UL << 2)
	#define PADS_PUE      (1UL << 3)
	#define PADS_DRIVE_4  (1UL << 4)
	#define PADS_IE       (1UL << 6)
	#define PADS_OD       (1UL << 7)
	/* Resets to 1. While set, the pad is disconnected from the pin. */
	#define PADS_ISO      (1UL << 8)

	/* PL011 UART */
	typedef struct {
		__IO uint32_t DR;
		__IO uint32_t RSR;
		uint32_t RESERVED0[4];
		__IO uint32_t FR;
		uint32_t RESERVED1;
		__IO uint32_t ILPR;
		__IO uint32_t IBRD;
		__IO uint32_t FBRD;
		__IO uint32_t LCR_H;
		__IO uint32_t CR;
		__IO uint32_t IFLS;
		__IO uint32_t IMSC;
		__IO uint32_t RIS;
		__IO uint32_t MIS;
		__IO uint32_t ICR;
	} UART_TypeDef;

	#define UART0 ((UART_TypeDef *)RP2350_UART0_BASE)
	#define UART1 ((UART_TypeDef *)RP2350_UART1_BASE)
	#define UART_FR_BUSY (1UL << 3)
	#define UART_FR_RXFE (1UL << 4)
	#define UART_FR_TXFF (1UL << 5)
	#define UART_FR_TXFE (1UL << 7)
	#define UART_LCR_H_FEN  (1UL << 4)
	#define UART_LCR_H_WLEN_8 (3UL << 5)
	#define UART_LCR_H_STP2 (1UL << 3)
	#define UART_LCR_H_EPS  (1UL << 2)
	#define UART_LCR_H_PEN  (1UL << 1)
	#define UART_CR_UARTEN (1UL << 0)
	#define UART_CR_TXE    (1UL << 8)
	#define UART_CR_RXE    (1UL << 9)
	#define UART_IMSC_RXIM (1UL << 4)
	#define UART_IMSC_TXIM (1UL << 5)
	#define UART_IMSC_RTIM (1UL << 6)
	#define UART_ICR_RXIC  (1UL << 4)
	#define UART_ICR_TXIC  (1UL << 5)
	#define UART_ICR_RTIC  (1UL << 6)
	#define UART_ICR_OEIC  (1UL << 10)
	#define UART_RSR_OE    (1UL << 3)

	/* PrimeCell SSP / SPI */
	typedef struct {
		__IO uint32_t CR0, CR1, DR, SR, CPSR;
	} SPI_TypeDef;

	#define SPI0 ((SPI_TypeDef *)RP2350_SPI0_BASE)
	#define SPI1 ((SPI_TypeDef *)RP2350_SPI1_BASE)
	#define SPI_CR0_SCR_LSB 8
	#define SPI_CR0_DSS_8   7UL
	#define SPI_CR1_SSE     (1UL << 1)
	#define SPI_SR_TFE      (1UL << 0)
	#define SPI_SR_TNF      (1UL << 1)
	#define SPI_SR_RNE      (1UL << 2)
	#define SPI_SR_BSY      (1UL << 4)

	/* DW APB I2C. Offsets, the struct has holes. */
	#define I2C_CON           0x00
	#define I2C_TAR           0x04
	#define I2C_DATA_CMD      0x10
	#define I2C_SS_SCL_HCNT   0x14
	#define I2C_SS_SCL_LCNT   0x18
	#define I2C_FS_SCL_HCNT   0x1C
	#define I2C_FS_SCL_LCNT   0x20
	#define I2C_INTR_STAT     0x2C
	#define I2C_INTR_MASK     0x30
	#define I2C_RAW_INTR_STAT 0x34
	#define I2C_RX_TL         0x38
	#define I2C_TX_TL         0x3C
	#define I2C_CLR_INTR      0x40
	#define I2C_ENABLE        0x6C
	#define I2C_STATUS        0x70
	#define I2C_ENABLE_STATUS 0x9C
	#define I2C_CON_MASTER         (1UL << 0)
	#define I2C_CON_SPEED_STD      (1UL << 1)
	#define I2C_CON_SPEED_FAST     (2UL << 1)
	#define I2C_CON_SPEED          (3UL << 1)
	#define I2C_CON_RESTART        (1UL << 5)
	#define I2C_CON_SLAVE_DISABLE  (1UL << 6)
	#define I2C_DATA_CMD_READ      (1UL << 8)
	#define I2C_DATA_CMD_STOP      (1UL << 9)
	#define I2C_DATA_CMD_RESTART   (1UL << 10)
	#define I2C_STATUS_ACTIVITY    (1UL << 0)
	#define I2C_STATUS_TFNF        (1UL << 1)
	#define I2C_STATUS_TFE         (1UL << 2)
	#define I2C_STATUS_RFNE        (1UL << 3)
	#define I2C_INTR_RX_FULL       (1UL << 2)
	#define I2C_INTR_TX_EMPTY      (1UL << 4)
	#define I2C_INTR_STOP          (1UL << 9)

	/* PWM: 12 slices, 0x14 bytes each (CSR, DIV, CTR, CC, TOP), then EN at 0xF0.
	 * GPIO n is slice (n/2)%8, channel A when n is even. clk_sys clocks it.
	 * DIV INT of 0 means 256. CC above TOP holds the output high. */
	#define PWM_CSR(slice) RP2350_REG(RP2350_PWM_BASE, 0x14UL * (slice))
	#define PWM_DIV(slice) RP2350_REG(RP2350_PWM_BASE, 0x14UL * (slice) + 0x04UL)
	#define PWM_CC(slice)  RP2350_REG(RP2350_PWM_BASE, 0x14UL * (slice) + 0x0CUL)
	#define PWM_TOP(slice) RP2350_REG(RP2350_PWM_BASE, 0x14UL * (slice) + 0x10UL)
	#define PWM_INTR       RP2350_REG(RP2350_PWM_BASE, 0xF4UL)
	#define PWM_IRQ0_INTE  RP2350_REG(RP2350_PWM_BASE, 0xF8UL)
	#define PWM_IRQ0_INTS  RP2350_REG(RP2350_PWM_BASE, 0x100UL)
	#define PWM_CSR_EN         (1UL << 0)
	#define PWM_CSR_PH_CORRECT (1UL << 1)
	#define PWM_CSR_A_INV      (1UL << 2)
	#define PWM_CSR_B_INV      (1UL << 3)

	#define RP2350_GPIO_COUNT 30
	#define MP_ARCH_IRQ_COUNT 52

	typedef enum {
		TIMER0_IRQ_0 = 0,
		TIMER0_IRQ_1 = 1,
		TIMER0_IRQ_2 = 2,
		TIMER0_IRQ_3 = 3,
		TIMER1_IRQ_0 = 4,
		TIMER1_IRQ_1 = 5,
		TIMER1_IRQ_2 = 6,
		TIMER1_IRQ_3 = 7,
		PWM_IRQ_WRAP_0 = 8,
		PWM_IRQ_WRAP_1 = 9,
		DMA_IRQ_0 = 10,
		DMA_IRQ_1 = 11,
		DMA_IRQ_2 = 12,
		DMA_IRQ_3 = 13,
		USBCTRL_IRQ = 14,
		PIO0_IRQ_0 = 15,
		PIO0_IRQ_1 = 16,
		PIO1_IRQ_0 = 17,
		PIO1_IRQ_1 = 18,
		PIO2_IRQ_0 = 19,
		PIO2_IRQ_1 = 20,
		IO_IRQ_BANK0_IRQn = 21,
		IO_IRQ_BANK0_NS_IRQn = 22,
		IO_IRQ_QSPI_IRQn = 23,
		IO_IRQ_QSPI_NS_IRQn = 24,
		SIO_IRQ_FIFO = 25,
		SIO_IRQ_BELL = 26,
		SIO_IRQ_FIFO_NS = 27,
		SIO_IRQ_BELL_NS = 28,
		SIO_IRQ_MTIMECMP = 29,
		CLOCKS_IRQ = 30,
		SPI0_IRQn = 31,
		SPI1_IRQn = 32,
		UART0_IRQn = 33,
		UART1_IRQn = 34,
		ADC_IRQ_FIFO = 35,
		I2C0_IRQn = 36,
		I2C1_IRQn = 37,
		OTP_IRQn = 38,
		TRNG_IRQn = 39,
		PROC0_IRQ_CTI = 40,
		PROC1_IRQ_CTI = 41,
		PLL_SYS_IRQn = 42,
		PLL_USB_IRQn = 43,
		POWMAN_IRQ_POW = 44,
		POWMAN_IRQ_TIMER = 45,
		SPAREIRQ_IRQ_0 = 46,
		SPAREIRQ_IRQ_1 = 47,
		SPAREIRQ_IRQ_2 = 48,
		SPAREIRQ_IRQ_3 = 49,
		SPAREIRQ_IRQ_4 = 50,
		SPAREIRQ_IRQ_5 = 51,
	} IRQn_Type;

#endif
