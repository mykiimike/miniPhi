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

#ifndef _HAVE_ESP32_SERIES_H
	#define _HAVE_ESP32_SERIES_H

	/* HIGH runs the CPU at 240 MHz from the 480 MHz PLL and APB at 80 MHz.
	 * The other levels run both on the crystal. REF_TICK stays 1 MHz. */
	#define CL_XTAL_HZ     40000000UL
	#define CL_PLL_APB_HZ  80000000UL
	#define CL_PLL_CPU_HZ  240000000UL
	#define CL_REF_TICK_HZ 1000000UL
	#define CL_TICK_HZ     1000UL
	#define CL_TIMER_HZ    1000000UL

	#define CL_DPORT    0x3FF00000UL
	#define CL_UART0    0x3FF40000UL
	#define CL_GPIO     0x3FF44000UL
	#define CL_RTC      0x3FF48000UL
	#define CL_RTCIO    0x3FF48400UL
	#define CL_IOMUX    0x3FF49000UL
	#define CL_UART1    0x3FF50000UL
	#define CL_I2C0     0x3FF53000UL
	#define CL_EFUSE    0x3FF5A000UL
	#define CL_TIMG0    0x3FF5F000UL
	#define CL_TIMG1    0x3FF60000UL
	#define CL_SPI2     0x3FF64000UL
	#define CL_SPI3     0x3FF65000UL
	#define CL_APBCTL   0x3FF66000UL
	#define CL_I2C1     0x3FF67000UL
	#define CL_UART2    0x3FF6E000UL

	#define CL_WDT_KEY  0x50D83AA1UL

	#define CL_PIN_COUNT 40

	/* PRO CPU interrupt-matrix sources. The map word is 0x3FF00104 + source*4.
	 * The APP CPU map is the next block, 0x3FF00218 + source*4. */
	#define CL_INTMTX        (CL_DPORT + 0x104UL)
	#define CL_INTMTX_APP    (CL_DPORT + 0x218UL)
	#define CL_SRC_TG0_T0    14
	#define CL_SRC_TG1_T0    18
	#define CL_SRC_GPIO      22
	#define CL_SRC_FROM_CPU  24
	#define CL_SRC_FROM_CPU1 25
	#define CL_SRC_SPI2      30
	#define CL_SRC_SPI3      31
	#define CL_SRC_UART0     34
	#define CL_SRC_UART1     35
	#define CL_SRC_UART2     36
	#define CL_SRC_I2C0      49
	#define CL_SRC_I2C1      50

	/* Lines 19 and 20 are level 2. A level-1 handler shares EPS1 with a
	 * window spill, so the tick and FROM_CPU stay on level 2. GPIO is
	 * level-1 line 12. The mask is the LX6 XCHAL_INTLEVEL1_MASK. */
	#define CL_CPU_TICK   19
	#define CL_CPU_SCHED  20
	#define CL_CPU_UART   10
	#define CL_CPU_GPIO   12
	#define CL_INTLEVEL1_MASK 0x000637FFUL
	/* DPORT_CPU_INTR_FROM_CPU_0 and _1. Write 1 to pend, 0 to clear. */
	#define CL_FROM_CPU  (CL_DPORT + 0x0DCUL)
	#define CL_FROM_CPU1 (CL_DPORT + 0x0E0UL)

	/* APP CPU reset, clock, stall, boot address. */
	#define CL_APP_A (CL_DPORT + 0x2CUL)
	#define CL_APP_B (CL_DPORT + 0x30UL)
	#define CL_APP_C (CL_DPORT + 0x34UL)
	#define CL_APP_D (CL_DPORT + 0x38UL)
	/* PERI_CLK_EN bit 15 is TIMG1. Bit 13 is TIMG0. */
	#define CL_TG1_CLK (1UL << 15)

	/* PRID bit 13 is 0 on the PRO CPU and 1 on the APP CPU. */
	static inline int cl_cpu_id(void) {
		unsigned long id;

		__asm__ volatile ("rsr.prid %0" : "=r"(id));
		return((int)((id >> 13) & 1UL));
	}

	#define CL_CLK_EN  (CL_DPORT + 0x0C0UL)
	#define CL_RST_EN  (CL_DPORT + 0x0C4UL)

	static inline unsigned long cl_read(unsigned long addr) {
		return(*(volatile unsigned long *)addr);
	}

	static inline void cl_write(unsigned long addr, unsigned long value) {
		*(volatile unsigned long *)addr = value;
	}

	static inline void cl_set(unsigned long addr, unsigned long bits) {
		cl_write(addr, cl_read(addr) | bits);
	}

	static inline void cl_clr(unsigned long addr, unsigned long bits) {
		cl_write(addr, cl_read(addr) & ~bits);
	}

	/* A DPORT read that an interrupt cuts returns the wrong word. Read it
	 * twice above the level-2 lines and keep the second value. */
	static inline unsigned long cl_dport_read(unsigned long addr) {
		unsigned long ps;
		unsigned long value;

		__asm__ volatile ("rsil %0, 3" : "=r"(ps) :: "memory");
		value = *(volatile unsigned long *)addr;
		value = *(volatile unsigned long *)addr;
		__asm__ volatile ("wsr %0, ps\n\trsync" :: "r"(ps) : "memory");
		return(value);
	}

	/* RTC mux bit 1 is the RTC function. Clearing it selects the digital path. GPIO and UART call this before the IO_MUX write. */
	void cl_pad_digital(int pin);

	/* APB now; it moves with the clock level */
	unsigned long cl_apb_hz(void);
	/* UARTs clocked from APB, which pin the clock level */
	int cl_uart_apb(void);
	/* the APP CPU timer divider follows APB */
	void cl_cpu1_rate(void);

	/* APP CPU timer and the flash-park spin. Both run on that core. */
	void cl_cpu1_tick(void);
	int cl_cpu_should_park(void);
	void cl_cpu_park_spin(void);

	static inline void cl_periph(unsigned long bit, int on) {
		unsigned long clk = cl_dport_read(CL_CLK_EN);
		unsigned long rst = cl_dport_read(CL_RST_EN);

		if(on) {
			clk |= bit;
			rst &= ~bit;
		}
		else {
			clk &= ~bit;
			rst |= bit;
		}
		cl_write(CL_CLK_EN, clk);
		cl_write(CL_RST_EN, rst);
	}

#endif
