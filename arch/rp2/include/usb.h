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
@file usb.h
@brief RP2350 USB device controller and PLL_USB registers

The console gate is "USB". The kernel still calls mp_uart_open(),
mp_uart_setup(), mp_uart_close(), mp_uart_quiet() and the uart.h
byte helpers. uart.c forwards gate USB to the mp_usb_uart_* functions,
and the uart.h inlines call mp_usb_tx(), mp_usb_rx() and the interrupt
helpers when uart->usb is set. Those are the same shapes as the PL011
helpers: putc is mp_usb_tx(), getc is mp_usb_rx(), and the TX flush is
mp_usb_flush() (also run from mp_usb_disable_tx_int() at the end of a burst).

There is no fixed-function USB Serial/JTAG block on the RP2350.
0x400d0000 is XIP_QMI (QSPI), not USB DPRAM. The USB device controller
is the block below. It does not enumerate by itself; usb.c is the CDC ACM
function on it. Sources for the addresses are the RP2350 datasheet address
map and the pico-sdk rp2350 register headers (addressmap.h, usb.h,
usb_dpram.h, clocks.h, resets.h, pll.h).
*/

#ifndef _HAVE_RP2350_USB_H
	#define _HAVE_RP2350_USB_H

	#include "rp2350.h"

	/* pico-sdk src/rp2350/.../addressmap.h: USBCTRL_DPRAM_BASE, USBCTRL_REGS_BASE.
	 * PLL_USB_BASE is RP2350_PLL_USB_BASE (0x40058000) in rp2350.h. */
	#define RP2350_USBCTRL_DPRAM_BASE 0x50100000UL
	#define RP2350_USBCTRL_REGS_BASE  0x50110000UL

	/* RESETS bit 28. pico-sdk resets.h RESETS_RESET_USBCTRL_LSB.
	 * PLL_USB is already RESETS_PLL_USB (bit 15) in rp2350.h. */
	#define RESETS_USBCTRL (1UL << 28)

	/* PLL_USB, same map as PLL_SYS (pico-sdk hardware/regs/pll.h).
	 * 12 MHz * FBDIV 100 = 1200 MHz VCO, post-dividers 5 and 5 = 48 MHz.
	 * VCO must sit between 750 and 1600 MHz (pico-sdk hardware/pll.h);
	 * FBDIV 40 / 480 MHz is below that floor. */
	#define PLL_USB_CS        RP2350_REG(RP2350_PLL_USB_BASE, 0x00)
	#define PLL_USB_PWR       RP2350_REG(RP2350_PLL_USB_BASE, 0x04)
	#define PLL_USB_FBDIV_INT RP2350_REG(RP2350_PLL_USB_BASE, 0x08)
	#define PLL_USB_PRIM      RP2350_REG(RP2350_PLL_USB_BASE, 0x0C)

	/* clk_usb. pico-sdk rp2350 clocks.h: CLK_USB_CTRL is 0x60, DIV is 0x64.
	 * CLK_HSTX occupies 0x54 on the RP2350, so this is not the RP2040
	 * CLK_USB offset. AUXSRC 0 is clksrc_pll_usb. ENABLE is bit 11.
	 * There is no glitchless mux; SELECTED at 0x68 is hardwired to 1. */
	#define CLOCKS_CLK_USB_CTRL RP2350_REG(RP2350_CLOCKS_BASE, 0x60)
	#define CLOCKS_CLK_USB_DIV  RP2350_REG(RP2350_CLOCKS_BASE, 0x64)
	#define CLOCKS_USB_ENABLE   (1UL << 11)
	#define CLOCKS_USB_ENABLED  (1UL << 28)
	#define CLOCKS_USB_AUX_PLL  0UL

	/* Bring PLL_USB up and route clk_usb. 1 on success. */
	int mp_arch_pll_usb_init(void);

	/* USBCTRL_REGS. Offsets from pico-sdk rp2350 hardware/regs/usb.h. */
	#define USB_ADDR_ENDP    RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x00)
	#define USB_MAIN_CTRL    RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x40)
	#define USB_SIE_CTRL     RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x4C)
	#define USB_SIE_STATUS   RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x50)
	#define USB_BUFF_STATUS  RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x58)
	/* Which double buffer completed. Meaningful while that BUFF_STATUS bit is set. */
	#define USB_BUFF_CPU_SHOULD_HANDLE RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x5c)
	#define USB_EP_STALL_ARM RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x68)
	#define USB_MUXING       RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x74)
	#define USB_PWR          RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x78)
	#define USB_INTE         RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x90)
	#define USB_INTS         RP2350_REG(RP2350_USBCTRL_REGS_BASE, 0x98)

	#define USB_MAIN_CTRL_CONTROLLER_EN (1UL << 0)
	#define USB_MAIN_CTRL_HOST_NDEVICE  (1UL << 1)
	#define USB_SIE_CTRL_PULLUP_EN      (1UL << 16)
	#define USB_SIE_CTRL_EP0_INT_1BUF   (1UL << 29)
	#define USB_SIE_STATUS_SETUP_REC    (1UL << 17)
	#define USB_SIE_STATUS_BUS_RESET    (1UL << 19)
	#define USB_EP_STALL_ARM_EP0_IN     (1UL << 0)
	#define USB_EP_STALL_ARM_EP0_OUT    (1UL << 1)
	#define USB_MUXING_TO_PHY           (1UL << 0)
	#define USB_MUXING_SOFTCON          (1UL << 3)
	#define USB_PWR_VBUS_DETECT         (1UL << 2)
	#define USB_PWR_VBUS_DETECT_OVERRIDE_EN (1UL << 3)
	#define USB_INTS_BUFF_STATUS        (1UL << 4)
	#define USB_INTS_BUS_RESET          (1UL << 12)
	#define USB_INTS_SETUP_REQ          (1UL << 16)

	/* Endpoint control, DPRAM. pico-sdk hardware/structs/usb_dpram.h. */
	#define USB_EP_CTRL_ENABLE    (1UL << 31)
	#define USB_EP_CTRL_DOUBLE    (1UL << 30)
	#define USB_EP_CTRL_INT_1BUF  (1UL << 29)
	#define USB_EP_CTRL_TYPE_LSB  26
	#define USB_EP_TYPE_BULK      2UL
	#define USB_EP_TYPE_INT       3UL

	/* Buffer control bits. Same header: FULL bit 15, DATA1 bit 13, AVAIL bit 10. */
	#define USB_BUF_CTRL_FULL      0x00008000UL
	#define USB_BUF_CTRL_DATA1_PID 0x00002000UL
	/* forces the double-buffer selector to the buffer written with AVAILABLE */
	#define USB_BUF_CTRL_SEL       0x00001000UL
	#define USB_BUF_CTRL_STALL     0x00000800UL
	#define USB_BUF_CTRL_AVAIL     0x00000400UL
	#define USB_BUF_CTRL_LEN_MASK  0x000003FFUL

	/* Device-mode DPRAM. setup at 0, EP1..15 control at 8, buffer control at
	 * 0x80, EP0 single buffer at 0x100, spare buffers at 0x180.
	 * pico-sdk usb_device_dpram_t, offsetof(epx_data) == 0x180. */
	typedef struct {
		volatile uint8_t setup_packet[8];
		struct {
			volatile uint32_t in;
			volatile uint32_t out;
		} ep_ctrl[15];
		struct {
			volatile uint32_t in;
			volatile uint32_t out;
		} ep_buf_ctrl[16];
		volatile uint8_t ep0_buf_a[64];
		volatile uint8_t ep0_buf_b[64];
		volatile uint8_t epx_data[4096 - 0x180];
	} usb_dpram_t;

	#define USB_DPRAM ((usb_dpram_t *)RP2350_USBCTRL_DPRAM_BASE)

	/* EP2 OUT is one 64-byte buffer at 0x180. EP2 IN is double buffered: buffer 0
	 * at 0x1C0, buffer 1 at 0x200. EP1 interrupt IN starts after that, at 0x240. */
	#define USB_EP2_OUT_OFF 0x180UL
	#define USB_EP2_IN_OFF  0x1C0UL
	#define USB_EP1_IN_OFF  0x240UL

	/*
	 * Same contract as uart.h for gate "USB". mp_uart_tx/rx and the
	 * interrupt masks are static inlines in uart.h; they call these
	 * when uart->usb is set, so the serial layer does not grow a second API.
	 */
	mp_ret_t mp_usb_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who);
	mp_ret_t mp_usb_uart_setup(mp_uart_t *uart, mp_options_t *options);
	mp_ret_t mp_usb_uart_close(mp_uart_t *uart);
	mp_bool_t mp_usb_quiet(void);
	/* YES once the host has set a configuration. */
	mp_bool_t mp_usb_linked(void);

	void mp_usb_enable_tx_int(mp_uart_t *uart);
	void mp_usb_disable_tx_int(mp_uart_t *uart);
	void mp_usb_enable_rx_int(mp_uart_t *uart);
	void mp_usb_disable_rx_int(mp_uart_t *uart);
	void mp_usb_tx(mp_uart_t *uart, unsigned char data);
	int mp_usb_tx_room(mp_uart_t *uart);
	int mp_usb_write(mp_uart_t *uart, const unsigned char *data, int n);
	void mp_usb_flush(mp_uart_t *uart);
	unsigned char mp_usb_rx(mp_uart_t *uart);
	char mp_usb_is_busy(mp_uart_t *uart);

#endif
