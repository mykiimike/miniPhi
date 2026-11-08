/*
 * miniPhi - RTOS
 * Copyright (C) 2026  Michael VERGOZ
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file mp_c6_mac.c
 * @brief Direct driver of the ESP32-C6 Wi-Fi MAC for miniPhi.
 * @defgroup mpArchESP32C6Mac MAC Wi-Fi ESP32-C6
 * @ingroup mpArchESP32
 * @{
 *
 * MMIO accesses are deliberately kept in source order. Registers,
 * delays, ROM calls and descriptors are specific to the C6.
 *
 * Hardware assumptions to confirm in mp.h and the linker script:
 * - RV32 ABI: 32-bit pointer and unsigned long;
 * - DMA SRAM in the 0x40800000 - 0x4086E000 range;
 * - ROM calls by address and the g_phyFuns table valid for this firmware;
 * - cache/memory coherence handled by the platform.
 *
 * This file is not a proof of correct silicon behaviour.
 */

#include <mp.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(SUPPORT_ESP32_RF) && defined(SUPPORT_COMMON_DEVICE)

#define MAC_POLL_LIMIT          100000UL
#define MAC_TX_REARM_DELAY_US   20000U
#define MAC_TX_PREFIX_BYTES    8UL

/* 2.4 GHz band covered and the only channel width supported (20 MHz). */
#define MAC_WIFI_FREQ_MIN_HZ  2412000000UL
#define MAC_WIFI_FREQ_MAX_HZ  2484000000UL
#define MAC_WIFI_CHANNEL_WIDTH_HZ   20000000UL

/* hal_init: set bit 1, then bit 0 reads back while the block is in use. */
#define MAC_REG_BASE    0x600A4000UL
#define MAC_REG_CONTROL     (MAC_REG_BASE + 0xDDCUL)
#define MAC_CONTROL_ENABLE  (1UL << 1)
#define MAC_CONTROL_READY  (1UL << 0)

/* hal_mac_set_addr slot 0. Bit 16 is what that function sets in the high half. */
#define MAC_REG_ADDR_LOW   (MAC_REG_BASE + 0x5CUL)
#define MAC_REG_ADDR_HIGH   (MAC_REG_BASE + 0x60UL)
#define MAC_ADDR_VALID (1UL << 16)

/* hal_mac_rx_enable is bit 31. mac_rxbuf_init clears the low eight bits
 * of the word before the base, then hal_mac_rx_set_base stores the descriptor. */
#define MAC_REG_RX_CONTROL      (MAC_REG_BASE + 0x80UL)
#define MAC_RX_ENABLE   (1UL << 31)
#define MAC_REG_RX_GATE (MAC_REG_BASE + 0x7CUL)
#define MAC_REG_RX_DESCRIPTOR (MAC_REG_BASE + 0x84UL)

/* wDev_Rxbuf_Init programs 0x8a4 when its menu flag is set, then allocates
 * four bytes past that. The length is written twice, in bits 13:0 and in
 * bits 27:14. Bit 31 is set. Bits 30 and 29 are cleared. */
#define MAC_RX_BUFFER_BYTES  0x8a4UL
#define MAC_DESC_DMA_OWNER  (1UL << 31)

/* --------------------- MAC state --------------------- */
static struct {
    mp_device_phy_t *phy;
    unsigned char addr[6];
    mp_bool_t bound;
    mp_bool_t up;
    mp_bool_t opened;
    mp_bool_t modem;
    mp_bool_t rx;
    mp_bool_t tx;
} s_mac;

/* Word 1 is the buffer. Word 2 is the next descriptor. rx_buffer_init
 * links the last back to the first: a terminated list is never filled. */
/* Two circular DMA descriptors: three 32-bit words each. */
static volatile unsigned long s_rx_descriptors[6];
static unsigned long s_rx_buffers[2][(MAC_RX_BUFFER_BYTES + 4UL) / 4UL];
/* 0x600A4088 is read-only. A JTAG CPU reset leaves it on a stale
 * SRAM address, and DMA will not leave that word until it is a
 * descriptor. */
static volatile unsigned long *s_rx_live;

/* A radio that only overlaps the band is refused. tune, hold and a 20 MHz width have to be published. */
static int mac_phy_supported(mp_device_phy_t *phy) {
    if (phy == NULL || phy->kind != MP_DEVICE_PHY_RADIO)
        return 0;
    if (!phy->hold || !phy->tune)
        return 0;
    if (phy->hz_min > MAC_WIFI_FREQ_MIN_HZ || phy->hz_max < MAC_WIFI_FREQ_MAX_HZ)
        return 0;
    if (phy->width_min > MAC_WIFI_CHANNEL_WIDTH_HZ || phy->width_max < MAC_WIFI_CHANNEL_WIDTH_HZ)
        return 0;
    return 1;
}

/* phy_get_mac_addr byte order. mp_machine_serial prints these words the other way. */
static void mac_read_efuse_address(void) {
    unsigned long lo = c6_read(C6_EFUSE + 0x44UL);
    unsigned long hi = c6_read(C6_EFUSE + 0x48UL);

    s_mac.addr[0] = (unsigned char)((hi >> 8) & 0xffUL);
    s_mac.addr[1] = (unsigned char)(hi & 0xffUL);
    s_mac.addr[2] = (unsigned char)((lo >> 24) & 0xffUL);
    s_mac.addr[3] = (unsigned char)((lo >> 16) & 0xffUL);
    s_mac.addr[4] = (unsigned char)((lo >> 8) & 0xffUL);
    s_mac.addr[5] = (unsigned char)(lo & 0xffUL);
}

static void mac_program_station_address(void) {
    unsigned long lo;
    unsigned long hi;

    lo = (unsigned long)s_mac.addr[0]
        | ((unsigned long)s_mac.addr[1] << 8)
        | ((unsigned long)s_mac.addr[2] << 16)
        | ((unsigned long)s_mac.addr[3] << 24);
    hi = (unsigned long)s_mac.addr[4] | ((unsigned long)s_mac.addr[5] << 8);
    c6_write(MAC_REG_ADDR_LOW, lo);
    c6_write(MAC_REG_ADDR_HIGH, hi);
    c6_write(MAC_REG_ADDR_HIGH, hi | MAC_ADDR_VALID);
}

static mp_ret_t mac_enable_block(void) {
    unsigned long left = MAC_POLL_LIMIT;
    unsigned long word;

    word = c6_read(MAC_REG_CONTROL);
    c6_write(MAC_REG_CONTROL, word | MAC_CONTROL_ENABLE);
    while (left--) {
        if (c6_read(MAC_REG_CONTROL) & MAC_CONTROL_READY)
            return TRUE;
    }
    return FALSE;
}

/* mac_txrx_init. The hung-time store is overwritten by the 0xF0 write
 * below, so that call is not repeated. hal_init writes 0x600A4C4C first. */
static void mac_configure_txrx(void) {
    unsigned long word;

    c6_write(0x600A4C4CUL, 0xffffffffUL);
    c6_set(0x600A4C8CUL, 0x8080a000UL);
    c6_set(0x600A4C8CUL, 0x1000UL);
    c6_set(0x600A4C8CUL, 0x10000000UL);
    c6_clr(0x600A4C98UL, 8UL);
    c6_write(0x600A40F8UL, c6_read(0x600A40F8UL) & 0xffffUL);
    c6_write(0x600A40FCUL, c6_read(0x600A40FCUL) & 0xffffUL);
    c6_write(0x600A4100UL, c6_read(0x600A4100UL) & 0xffffUL);
    c6_write(0x600A4104UL, c6_read(0x600A4104UL) & 0xffffUL);
    c6_set(0x600A40F8UL, 0x01000000UL);
    c6_set(0x600A40FCUL, 0x01000000UL);
    c6_set(0x600A40F8UL, 0x04000000UL);
    c6_set(0x600A40FCUL, 0x04000000UL);
    c6_set(0x600A4C8CUL, 0x200UL);
    c6_set(0x600A4110UL, 1UL);
    c6_set(0x600A4110UL, 0x10UL);
    c6_set(0x600A4114UL, 0x80000000UL);
    c6_write(0x600A4114UL, (c6_read(0x600A4114UL) & 0xf00fffffUL) | 0x01b00000UL);
    c6_set(0x600A4C9CUL, 3UL);
    /* hal_he_set_mac_delay when the clock callback returns 0, then
     * hal_he_set_ack_rate(0). mac_txrx_init calls both before the
     * 0x600A4C14 stores. */
    word = c6_read(0x600A4C58UL);
    word = (word & 0xffe003ffUL) | 0x119c00UL;
    word = (word & ~0x3ffUL) | 327UL;
    word = (word & 0x801fffffUL) | 0x0bc00000UL;
    c6_write(0x600A4C58UL, word);
    word = c6_read(0x600A4C54UL);
    word = (word & 0x801fffffUL) | 0x29e00000UL;
    word = (word & 0xffe003ffUL) | 0x94000UL;
    c6_write(0x600A4C54UL, word);
    c6_write(0x600A4440UL, 0x90a0bUL);
    c6_write(0x600A444CUL, 0x90a0bUL);
    c6_write(0x600A4444UL, 0x50100UL);
    c6_write(0x600A4450UL, 0x50100UL);
    c6_set(0x600A4C14UL, 0x80000000UL);
    c6_set(0x600A4C14UL, 0x40000000UL);
    c6_write(0x600A4C18UL, (c6_read(0x600A4C18UL) & 0xfffff000UL) | 0xf0UL);
    c6_write(0x600A4C1CUL, (c6_read(0x600A4C1CUL) & 0xfffff000UL) | 0xf0UL);
    c6_write(0x600A4CA4UL, (c6_read(0x600A4CA4UL) & ~0xf0UL) | 0x40UL);
    c6_set(0x600A4C60UL, 0x7fff0000UL);
    c6_set(0x600A4C60UL, 0x80000000UL);
    c6_set(0x600A4308UL, 2UL);
    c6_clr(MAC_REG_RX_CONTROL, MAC_RX_ENABLE);
}

/* hal_init walks four slots at 0x600A40D8, then hal_mac_rx_set_policy(i, 0, 0, 0).
 * (0x18029036 + i) << 2 is that slot. Slot 3 returns at once. */
static void mac_configure_rx_policy(unsigned int i) {
    unsigned long slot = 0x600A40D8UL + (unsigned long)i * 4UL;
    unsigned long link = 0x600A4004UL + ((unsigned long)i << 3);
    unsigned long word;

    c6_set(slot, 0x280UL);
    c6_clr(slot, 0x400UL);
    c6_set(slot, 5UL);
    c6_write(slot, c6_read(slot) & 0xffffdfbfUL);
    if (i > 2)
        return;
    c6_write(slot, c6_read(slot) & ~0x411UL);
    if (i == 1)
        c6_set(0x600A400CUL, 0x40000000UL);
    else
        c6_write(link, c6_read(link) & 0xbfffffffUL);
    c6_write(slot, c6_read(slot) & ~0x40UL);
    word = c6_read(link);
    c6_write(link, (word << 1) >> 1);
    c6_clr(0x600A4060UL + ((unsigned long)i << 3), 0x10000UL);
}

/* hal_sniffer_enable. Bit 17 of the last policy slot, and the frame-type
 * mask below it clear. Without that a data frame from another station
 * never reaches the descriptor. */
static void mac_enable_promiscuous_rx(void) {
    unsigned long word;

    word = c6_read(0x600A40E4UL);
    word |= 1UL << 17;
    word &= ~0x38fUL;
    c6_write(0x600A40E4UL, word);
}

/* mac_last_rxbuf_init. Fixed table, then two words it updates in place. */
static void mac_configure_last_rx_buffer(void) {
    c6_write(0x600A4120UL, 0x23006UL);
    c6_write(0x600A413CUL, 1544UL);
    c6_write(0x600A4158UL, 0xffffUL);
    c6_write(0x600A4124UL, 0x23006UL);
    c6_write(0x600A4140UL, 0x808UL);
    c6_write(0x600A415CUL, 0xffffUL);
    c6_write(0x600A4128UL, 0x23006UL);
    c6_write(0x600A4144UL, 0x8e88UL);
    c6_write(0x600A4160UL, 0xffffUL);
    c6_write(0x600A412CUL, 0x2301cUL);
    c6_write(0x600A4148UL, 0x44004300UL);
    c6_write(0x600A4164UL, 0xffffffffUL);
    c6_write(0x600A4130UL, 0x2301cUL);
    c6_write(0x600A414CUL, 0x43004400UL);
    c6_write(0x600A4168UL, 0xffffffffUL);
    c6_write(0x600A4134UL, 0x23011UL);
    c6_write(0x600A4150UL, 1UL);
    c6_write(0x600A416CUL, 255UL);
    c6_set(0x600A411CUL, 0x3f00UL);
    c6_set(0x600A411CUL, 0x7eUL);
    c6_set(0x600A4098UL, 0x80000000UL);
}

/* hal_enable_mac clears bits 7:4 of 0x600A4C00, then stores the same
 * constant hal_init leaves in 0x600A4C40. hal_coex_pti_init sets bit 5
 * of 0x600A4DD8. Neither runs from mac_txrx_init. */
static void mac_enable_core(void) {
    unsigned long word;

    word = c6_read(0x600A4C00UL);
    c6_write(0x600A4C00UL, word & ~0xf0UL);
    /* 0x19a849e0 is the word hal_init stores. Bit 2 is
     * hal_sniffer_rx_set_promis(0, 1): without it the RX-done
     * event never latches and do_rx_poll sees a zero C48. */
    c6_write(0x600A4C40UL, 0x19a849e4UL);
    c6_set(0x600A4DD8UL, 0x20UL);
}

/* The stores hal_init does after mac_last_rxbuf_init that touch this block.
 * Bit 15 of the RX control is hal_mac_set_rxbuf_reload_use_hw_beacon_enable. */
static void mac_complete_rx_setup(void) {
    unsigned long word;

    c6_set(0x600A4C8CUL, 0x10000000UL);
    word = (c6_read(0x600A4098UL) & ~0xffUL) | 1UL;
    c6_write(0x600A4098UL, word);
    word = (c6_read(0x600A4098UL) & 0xffff00ffUL) | 0x100UL;
    c6_write(0x600A4098UL, word);
    c6_set(MAC_REG_RX_CONTROL, 0x8000UL);
}

/* rx_buffer_init: owner and the buffer size in bits 13:0. Bits 27:14
 * stay 0: that field is the received length, written by the DMA. */
static unsigned long mac_rx_descriptor_control(void) {
    return MAC_DESC_DMA_OWNER | (MAC_RX_BUFFER_BYTES & 0x3fffUL);
}

/* Meet the live next pointer. Reconstruct the SRAM address the same
 * way hal_mac_rx_get_last_dscr does: high bits from 0x600A4C70, low
 * 20 from 0x600A4088. */
static void mac_repair_active_rx_descriptor(void) {
    unsigned long next;
    unsigned long live;

    next = c6_read(0x600A4088UL) & 0xfffffUL;
    if (next == 0) {
        s_rx_live = NULL;
        return;
    }
    live = (c6_read(0x600A4C70UL) & 0xfff00000UL) | next;
    /* Presumed SRAM range; still lacks validation against the official
     * memory map before allowing DMA on this pointer. */
    if (live < 0x40800000UL || live > (0x4086E000UL - 12UL)) {
        s_rx_live = NULL;
        return;
    }
    s_rx_live = (volatile unsigned long *)(uintptr_t)live;
    s_rx_live[0] = mac_rx_descriptor_control();
    s_rx_live[1] = (unsigned long)s_rx_buffers[0];
    s_rx_live[2] = (unsigned long)s_rx_descriptors;
}

/* Queue 0, rate byte 0. hal_mac_txq_enable ORs bits 31 and 30 of the
 * same word mac_tx_set_plcp0 stored. hal_mac_txq_disable shifts them off. */
#define MAC_REG_TX_PLCP0  0x600A4D6CUL
#define MAC_REG_TX_PLCP1  0x600A5488UL
#define MAC_REG_TX_LENGTH    0x600A54BCUL
#define MAC_REG_TX_POWER    0x600A54ACUL
#define MAC_REG_TX_GATE   0x600A4D64UL
#define MAC_REG_TX_EDCA   0x600A4D68UL
#define MAC_REG_TX_PROTECTION   0x600A4D60UL
#define MAC_TX_START_BITS   0xC0000000UL
#define MAC_TX_MPDU_BYTES  35UL

/* RF/ROM dependencies provided by the miniPhi platform. */
extern unsigned long phy_get_max_pwr(unsigned long rate, signed char *out);
extern void phy_enable_cca_new(void);
extern int phy_get_pwr_index(unsigned int rate);
extern void wifi_set_tx_gain(unsigned int channel, unsigned int log);
extern unsigned char *g_phyFuns;
extern void mp_arch_rf_delay_us(unsigned int us);

/* phy_force_pwr_index, the ROM entry set_rate_power_index tails into. */
static void mac_force_power_index(int on, unsigned int index) {
    void (*fn)(int, unsigned int);

    fn = (void (*)(int, unsigned int))0x400013E0UL;
    fn(on, index);
}

/* pa_reg_opt(rate, 1, 40): two writes through g_phyFuns+96. */
static void mac_optimize_power_amplifier(void) {
    unsigned long (*fn)(unsigned long, unsigned long, unsigned long,
        unsigned long, unsigned long, unsigned long);

    if (!g_phyFuns)
        return;
    fn = *(unsigned long (**)(unsigned long, unsigned long, unsigned long,
        unsigned long, unsigned long, unsigned long))(g_phyFuns + 96);
    if (!fn)
        return;
    (void)fn(97UL, 0UL, 7UL, 5UL, 5UL, 0xfeUL);
    (void)fn(97UL, 0UL, 10UL, 5UL, 0UL, 40UL);
}

/* One TX DMA descriptor; memory retained between calls. */
static volatile unsigned long s_tx_descriptor[3];
static unsigned long s_tx_frame[16];
static unsigned long s_tx_power;
static mp_bool_t s_tx_ready;

static uint32_t mac_crc32(const unsigned char *p, unsigned n) {
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    unsigned i;
    unsigned b;

    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (b = 0; b < 8; b++) {
            if (crc & 1UL)
                crc = (crc >> 1) ^ UINT32_C(0xEDB88320);
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

/* fill_tx_frame first fragment: owner+valid, bit 12, DMA length = MPDU+8
 * in bits 27:14. The extra 8 bytes are the prefix that function writes
 * at the start of the buffer. */
static unsigned long mac_tx_descriptor_control(unsigned long mpdu_length) {
    return (1UL << 31) | (1UL << 30) | (1UL << 12) | ((mpdu_length + MAC_TX_PREFIX_BYTES) << 14);
}

static void mac_disable_tx(void) {
    unsigned long word;

    if (s_mac.tx != YES)
        return;
    word = c6_read(MAC_REG_TX_PLCP0);
    c6_write(MAC_REG_TX_PLCP0, (word << 2) >> 2);
    s_mac.tx = NO;
}

static void mac_disable_rx(void) {
    unsigned long word;

    if (s_mac.rx != YES)
        return;
    word = c6_read(MAC_REG_RX_CONTROL);
    c6_write(MAC_REG_RX_CONTROL, (word << 1) >> 1);
    s_mac.rx = NO;
}

static void mac_release_resources(void) {
    if (s_mac.opened == YES)
        mp_device_phy_close(s_mac.phy);
    else if (s_mac.phy && s_mac.phy->held == YES)
        mp_device_phy_release(s_mac.phy);
    s_mac.opened = NO;
    if (s_mac.modem == YES)
        mp_arch_modem_put(MP_ARCH_MODEM_WIFI);
    s_mac.modem = NO;
}

/* ----------------------- Public API ------------------------ */
/* Bind a compatible PHY face; no automatic bring-up. */
void mp_c6_mac(mp_kernel_t *kernel, mp_device_phy_t *phy) {
    (void)kernel;
    memset(&s_mac, 0, sizeof(s_mac));
    if (!mac_phy_supported(phy))
        return;
    s_mac.phy = phy;
    s_mac.bound = YES;
}

/* Open/hold the PHY, tune its frequency, then enable the MAC block. */
mp_ret_t mp_c6_mac_up(unsigned long frequency_hz) {
    mp_device_phy_t *phy;

    if (s_mac.bound != YES)
        return FALSE;
    if (s_mac.up == YES)
        return TRUE;
    phy = s_mac.phy;
    s_mac.opened = NO;
    if (phy->opened != YES) {
        if (mp_device_phy_open(phy) != TRUE)
            return FALSE;
        s_mac.opened = YES;
    }
    if (mp_arch_modem_get(MP_ARCH_MODEM_WIFI) != TRUE) {
        mac_release_resources();
        return FALSE;
    }
    s_mac.modem = YES;
    if (mp_device_phy_hold(phy) != TRUE) {
        mac_release_resources();
        return FALSE;
    }
    if (mp_device_phy_tune(phy, frequency_hz, MAC_WIFI_CHANNEL_WIDTH_HZ) != TRUE || mac_enable_block() != TRUE) {
        mac_release_resources();
        return FALSE;
    }
    mac_read_efuse_address();
    mac_program_station_address();
    s_mac.up = YES;
    return TRUE;
}

void mp_c6_mac_down(void) {
    if (s_mac.up != YES)
        return;
    mac_disable_tx();
    mac_disable_rx();
    mac_release_resources();
    s_tx_ready = NO;
    s_mac.up = NO;
}

/* Program two ring RX descriptors and enable reception. */
mp_ret_t mp_c6_mac_rx(void) {
    unsigned long word;
    unsigned int i;

    if (s_mac.up != YES)
        return FALSE;
    if (s_mac.rx == YES)
        return TRUE;
    mac_configure_txrx();
    for (i = 0; i < 4; i++)
        mac_configure_rx_policy(i);
    mac_enable_promiscuous_rx();
    mac_program_station_address();
    mac_configure_last_rx_buffer();
    mac_complete_rx_setup();
    for (i = 0; i < (sizeof(s_rx_buffers[0]) / sizeof(s_rx_buffers[0][0])); i++) {
        s_rx_buffers[0][i] = 0;
        s_rx_buffers[1][i] = 0;
    }
    s_rx_descriptors[0] = mac_rx_descriptor_control();
    s_rx_descriptors[1] = (unsigned long)s_rx_buffers[0];
    s_rx_descriptors[2] = (unsigned long)&s_rx_descriptors[3];
    s_rx_descriptors[3] = mac_rx_descriptor_control();
    s_rx_descriptors[4] = (unsigned long)s_rx_buffers[1];
    s_rx_descriptors[5] = (unsigned long)&s_rx_descriptors[0];
    mac_repair_active_rx_descriptor();
    word = c6_read(MAC_REG_RX_GATE);
    c6_write(MAC_REG_RX_GATE, word & ~0xffUL);
    c6_write(MAC_REG_RX_DESCRIPTOR, s_rx_live ? (unsigned long)s_rx_live
        : (unsigned long)s_rx_descriptors);
    mac_enable_core();
    c6_set(0x600A4C40UL, 4UL);
    c6_write(MAC_REG_RX_CONTROL, 0);
    c6_write(MAC_REG_RX_CONTROL, MAC_RX_ENABLE);
    c6_set(MAC_REG_RX_CONTROL, 1UL);
    word = MAC_POLL_LIMIT;
    while (word--) {
        if ((c6_read(MAC_REG_RX_CONTROL) & 1UL) == 0)
            break;
    }
    /* The START-bit timeout is not currently propagated to the caller.
     * This historical semantics is kept on purpose. */
    s_mac.rx = YES;
    return TRUE;
}

unsigned long mp_c6_mac_rx_word(void) {
    unsigned long fill = mac_rx_descriptor_control();

    if (s_rx_live && s_rx_live[0] != fill)
        return s_rx_live[0];
    if (s_rx_descriptors[0] != fill)
        return s_rx_descriptors[0];
    if (s_rx_descriptors[3] != fill)
        return s_rx_descriptors[3];
    return s_rx_live ? s_rx_live[0] : s_rx_descriptors[0];
}

/* Build an 802.11 test frame and kick TX queue 0. */
mp_ret_t mp_c6_mac_tx(void) {
    unsigned char *frame;
    unsigned long crc;
    unsigned long ptr;
    unsigned long word;
    signed char sample[2];
    signed long b0;
    signed long b1;
    unsigned i;

    if (s_mac.up != YES)
        return FALSE;
    if (s_tx_ready != YES) {
        mac_configure_txrx();
        frame = (unsigned char *)s_tx_frame;
        for (i = 0; i < sizeof(s_tx_frame); i++)
            frame[i] = 0;
        /* fill_tx_frame prefix: MPDU length in the low 14 bits, then
         * the MPDU at offset 8. */
        frame[0] = (unsigned char)MAC_TX_MPDU_BYTES;
        frame[8] = 0x08;
        for (i = 0; i < 6; i++)
            frame[12 + i] = 0xff;
        for (i = 0; i < 6; i++) {
            frame[18 + i] = s_mac.addr[i];
            frame[24 + i] = s_mac.addr[i];
        }
        frame[32] = 'm';
        frame[33] = 'i';
        frame[34] = 'n';
        frame[35] = 'i';
        frame[36] = 'P';
        frame[37] = 'h';
        frame[38] = 'i';
        crc = mac_crc32(frame + 8, 31);
        frame[39] = (unsigned char)crc;
        frame[40] = (unsigned char)(crc >> 8);
        frame[41] = (unsigned char)(crc >> 16);
        frame[42] = (unsigned char)(crc >> 24);
        phy_get_max_pwr(0, sample);
        b0 = (signed long)sample[0];
        b1 = (signed long)sample[1];
        /* Signed values are converted the same way as the original. If
         * b0/b1 are negative this can propagate set bits: check the
         * TX_POWER register format. */
        s_tx_power = (unsigned long)b0
            | ((unsigned long)b0 << 16)
            | ((unsigned long)b1 << 24);
        mac_enable_core();
        phy_enable_cca_new();
        /* set_rate_power_index for 1 Mbps. Channel 6 is what
         * phy_chan_set stored for 2437 MHz. */
        /* NOTE: channel 6 is hard-coded even if up() tuned another channel.
         * Keep the original value until the PHY calculation is validated. */
        wifi_set_tx_gain(6, 0);
        mac_force_power_index(1, (unsigned int)phy_get_pwr_index(0));
        mac_optimize_power_amplifier();
        s_tx_ready = YES;
    }
    s_tx_descriptor[0] = mac_tx_descriptor_control(MAC_TX_MPDU_BYTES);
    s_tx_descriptor[1] = (unsigned long)s_tx_frame;
    s_tx_descriptor[2] = 0;
    ptr = (unsigned long)s_tx_descriptor;
    /* WifiTxStart clears this before it programs the slot. Bit 16 comes
     * up set, and a later clear, once the address is already installed,
     * leaves the valid bits set with no fetch. */
    c6_write(0x600A4CB0UL, 0);
    c6_write(MAC_REG_TX_PLCP0, (ptr & 0xfffffUL) | 0x01600000UL);
    word = c6_read(MAC_REG_TX_PROTECTION);
    c6_write(MAC_REG_TX_PROTECTION, (word << 1) >> 1);
    /* fill_tx_frame for a rate <= 15 stores (rate<<12)|length at
     * 0x600A5488 and (length<<8)|rate at 0x600A5494. Rate 0 is 1 Mbps.
     * A zero length there is a zero-length PPDU. */
    c6_write(MAC_REG_TX_PLCP1, MAC_TX_MPDU_BYTES);
    c6_write(0x600A5494UL, MAC_TX_MPDU_BYTES << 8);
    c6_clr(MAC_REG_TX_GATE, 8UL);
    c6_write(MAC_REG_TX_LENGTH, 0x00400004UL);
    c6_write(0x600A54B8UL, 0);
    word = c6_read(MAC_REG_TX_PROTECTION);
    c6_write(MAC_REG_TX_PROTECTION, (word << 1) >> 1);
    c6_clr(MAC_REG_TX_PLCP0, 1UL << 22);
    c6_write(MAC_REG_TX_POWER, s_tx_power);
    word = c6_read(MAC_REG_TX_EDCA);
    word = (word & 0xf0ffffffUL) | (2UL << 24);
    /* tx_a_frame forces the backoff field to 1 and a saturated
     * lifetime. A zero lifetime ends the slot with the done bit and
     * never raises the transmit bit. */
    word = (word & 0xffc00fffUL) | (1UL << 12);
    word &= 0xff3fffffUL;
    word |= 0xfffUL;
    c6_write(MAC_REG_TX_EDCA, word);
    /* phy_enable_cca_new clears bits 31:28 of 0x600A4C5C. The shift form
     * of hal_mac_tx_set_cca(0) only clears bits 31:30. */
    phy_enable_cca_new();
    /* seed_vs_rate for rate 0 writes 107 into the low 7 bits of
     * 0x600A7400, then WifiTxStart clears bit 21 of that word and
     * bit 20 of 0x600A7418. */
    word = c6_read(0x600A7400UL);
    word = (word & ~0x7fUL) | 107UL;
    c6_write(0x600A7400UL, word);
    c6_clr(0x600A7400UL, 1UL << 21);
    c6_clr(0x600A7418UL, 1UL << 20);
    mac_optimize_power_amplifier();
    /* tx_a_frame arms bit 1 of the control word and clears the two
     * pending bits before it kicks. tx_data_frame clears the done bit
     * the same way. */
    c6_set(0x600A4C40UL, 2UL);
    c6_set(0x600A4C4CUL, 0x83UL);
    if (s_mac.rx == YES) {
        word = c6_read(MAC_REG_RX_CONTROL);
        c6_write(MAC_REG_RX_CONTROL, word | MAC_RX_ENABLE);
    }
    /* A full store of the valid bits, then the same word with them
     * clear, then them set again. The clear has to stay visible: two
     * stores with only a read between them leave the slot idle. The
     * gap that finishes it is the one between an OpenOCD clear and the
     * following set. */
    word = (ptr & 0xfffffUL) | 0x01200000UL;
    c6_write(MAC_REG_TX_PLCP0, word | MAC_TX_START_BITS);
    (void)c6_read(MAC_REG_TX_PLCP0);
    c6_write(MAC_REG_TX_PLCP0, word);
    (void)c6_read(MAC_REG_TX_PLCP0);
    mp_arch_rf_delay_us(MAC_TX_REARM_DELAY_US);
    c6_write(MAC_REG_TX_PLCP0, word | MAC_TX_START_BITS);
    (void)c6_read(MAC_REG_TX_PLCP0);
    s_mac.tx = YES;
    return TRUE;
}

unsigned long mp_c6_mac_tx_ctrl(void) {
    return c6_read(0x600A4C00UL);
}

unsigned long mp_c6_mac_tx_state(void) {
    return c6_read(0x600A4CB0UL);
}

unsigned long mp_c6_mac_tx_word(void) {
    return c6_read(MAC_REG_TX_PLCP0);
}

unsigned long mp_c6_mac_tx_pwr(void) {
    return s_tx_power;
}

const unsigned char *mp_c6_mac_rx_buf(void) {
    unsigned long fill = mac_rx_descriptor_control();

    if (s_rx_live && s_rx_live[0] != fill && s_rx_live[1])
        return (const unsigned char *)s_rx_live[1];
    if (s_rx_descriptors[0] != fill)
        return (const unsigned char *)s_rx_buffers[0];
    if (s_rx_descriptors[3] != fill)
        return (const unsigned char *)s_rx_buffers[1];
    return (const unsigned char *)s_rx_buffers[0];
}

const unsigned char *mp_c6_mac_addr(void) {
    if (s_mac.up != YES)
        return NULL;
    return s_mac.addr;
}

void mp_c6_mac_fini(mp_kernel_t *kernel) {
    (void)kernel;
    mp_c6_mac_down();
    memset(&s_mac, 0, sizeof(s_mac));
}

#else

void mp_c6_mac(mp_kernel_t *kernel, void *phy) {
    (void)kernel;
    (void)phy;
}

mp_ret_t mp_c6_mac_up(unsigned long hz) {
    (void)hz;
    return FALSE;
}

void mp_c6_mac_down(void) {
}

mp_ret_t mp_c6_mac_rx(void) {
    return FALSE;
}

unsigned long mp_c6_mac_rx_word(void) {
    return 0;
}

const unsigned char *mp_c6_mac_rx_buf(void) {
    return NULL;
}

const unsigned char *mp_c6_mac_addr(void) {
    return NULL;
}

/* Same interface when RF or device support is disabled. */
mp_ret_t mp_c6_mac_tx(void) {
    return FALSE;
}

unsigned long mp_c6_mac_tx_ctrl(void) {
    return 0UL;
}

unsigned long mp_c6_mac_tx_state(void) {
    return 0UL;
}

unsigned long mp_c6_mac_tx_word(void) {
    return 0UL;
}

unsigned long mp_c6_mac_tx_pwr(void) {
    return 0UL;
}

void mp_c6_mac_fini(mp_kernel_t *kernel) {
    (void)kernel;
}

#endif

/**@}*/
