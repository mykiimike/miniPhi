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

#include <mp.h>

#if defined(MP_ARCH_RF_TEST) && defined(SUPPORT_COMMON_DEVICE)

mp_ret_t mp_c6_mac_up(unsigned long hz);
void mp_c6_mac_down(void);
const unsigned char *mp_c6_mac_addr(void);

/* MAC bring-up on channel 6, then the tone, send or listen bench the
 * board picked with MP_ARCH_RF_TONE, MP_ARCH_RF_SEND or MP_ARCH_RF_LISTEN. */
void mp_arch_rf_test(mp_device_phy_t *phy) {
	const unsigned char *addr;

	(void)phy;
	if(mp_c6_mac_up(2437000000UL) == TRUE && (addr = mp_c6_mac_addr()) != NULL) {
		mp_printk("MAC up ok, %02x:%02x:%02x:%02x:%02x:%02x",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
#if defined(MP_ARCH_RF_TONE)
		{
			void mp_c6_rf_tone(int on);
			unsigned long t0;

			mp_c6_rf_tone(1);
			mp_printk("RF tone on");
			t0 = mp_clock_ticks();
			while((mp_clock_ticks() - t0) < 8000UL)
				;
			mp_c6_rf_tone(0);
			mp_printk("RF tone off");
		}
#elif defined(MP_ARCH_RF_SEND)
		{
			mp_ret_t mp_c6_mac_tx(void);
			mp_ret_t mp_c6_mac_rx(void);
			unsigned long mp_c6_mac_tx_word(void);
			unsigned long mp_c6_mac_tx_pwr(void);
			unsigned long mp_c6_mac_tx_ctrl(void);
			unsigned long mp_c6_mac_tx_state(void);
			int mp_c6_rf_cca(void);
			unsigned long t0;
			unsigned long step;
			unsigned long kicks;

			mp_printk("RF rx %s", mp_c6_mac_rx() == TRUE ? "armed" : "failed");
			if(mp_c6_mac_tx() != TRUE)
				mp_printk("RF tx failed");
			else {
				mp_printk("RF tx %08lx pwr %08lx cca %d ctrl %08lx st %08lx",
					mp_c6_mac_tx_word(), mp_c6_mac_tx_pwr(),
					mp_c6_rf_cca(), mp_c6_mac_tx_ctrl(),
					mp_c6_mac_tx_state());
				t0 = mp_clock_ticks();
				while((mp_clock_ticks() - t0) < 500UL)
					;
				mp_printk("RF tx wait %08lx", mp_c6_mac_tx_word());
			}
			kicks = 0;
			t0 = mp_clock_ticks();
			while((mp_clock_ticks() - t0) < 14000UL) {
				if(mp_c6_mac_tx() != TRUE)
					break;
				kicks++;
				step = mp_clock_ticks();
				while((mp_clock_ticks() - step) < 500UL)
					;
			}
			mp_printk("RF tx off %lu %08lx", kicks, mp_c6_mac_tx_word());
		}
#elif defined(MP_ARCH_RF_LISTEN)
		{
			int mp_c6_rf_rssi(void);
			mp_ret_t mp_c6_mac_rx(void);
			unsigned long mp_c6_mac_rx_word(void);
			const unsigned char *mp_c6_mac_rx_buf(void);
			const unsigned char *buf;
			int rssi, rssi_hi;
			int n;
			unsigned long t0, step;
			unsigned long word;
			unsigned char b[8];
			unsigned i;

			mp_printk("RF rx %s", mp_c6_mac_rx() == TRUE ? "armed" : "failed");
			mp_device_phy_tune(phy, 2437000000UL, 20000000UL);
			for(n = 0; n < 12; n++) {
				rssi_hi = -32768;
				t0 = mp_clock_ticks();
				while((mp_clock_ticks() - t0) < 1000UL) {
					rssi = mp_c6_rf_rssi();
					if(rssi > rssi_hi)
						rssi_hi = rssi;
					step = mp_clock_ticks();
					while((mp_clock_ticks() - step) < 100UL)
						;
				}
			word = mp_c6_mac_rx_word();
			buf = mp_c6_mac_rx_buf();
			for(i = 0; i < 8; i++)
				b[i] = buf ? buf[i] : 0;
			/* C6 MAC RX hardware state, all read in the assembly:
			 * 0x600A4088 next desc (hal_mac_rx_read_rxdscrnext),
			 * 0x600A408C last desc (hal_mac_rx_read_rxdscrlast),
			 * 0x600A4E24/4E28 RX end info (hal_mac_rx_get_end_info),
			 * 0x600A4C48 event, bit 2 = RX done (hal_mac_interrupt_get_event).
			 * The SRAM descriptor alone never tells us whether the
			 * hardware accepted a frame; these do. */
			{
				volatile unsigned long *r = (volatile unsigned long *)0x600A4000UL;
				mp_printk("RF hear rssi %d rx %08lx %02x%02x%02x%02x%02x%02x%02x%02x nx %08lx la %08lx st %08lx ev %08lx",
					rssi_hi, word,
					b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
					r[0x88/4], r[0x8C/4], r[0xA8/4], r[0xC48/4]);
			}
			}
		}
#endif
	} else
		mp_printk("MAC up failed");
	mp_c6_mac_down();
}

#endif
