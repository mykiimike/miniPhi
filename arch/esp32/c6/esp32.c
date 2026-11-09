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

/**
@defgroup mpArchESP32Machine Machine

@ingroup mpArchESP32

@{
*/

#if defined(SUPPORT_ESP32_RF) && defined(SUPPORT_COMMON_DEVICE)
void mp_c6_phy(mp_kernel_t *kernel);
mp_device_phy_t *mp_c6_phy_face(void);
void mp_c6_phy_fini(mp_kernel_t *kernel);
void mp_c6_mac(mp_kernel_t *kernel, mp_device_phy_t *phy);
void mp_c6_mac_fini(mp_kernel_t *kernel);
#endif

static void _name(mp_kernel_t *kernel) {
	snprintf(kernel->arch.mcuName, sizeof(kernel->arch.mcuName), "ESP32-C6, 512 KB SRAM");
	kernel->mcuVendor = "Espressif";
	kernel->mcuName = kernel->arch.mcuName;
}

const char *mp_machine_serial(void) {
	static char text[13];
	unsigned long lo;
	unsigned long hi;

	if(text[0])
		return(text);
	/* factory MAC, low 32 bits then the high 16 */
	lo = c6_read(C6_EFUSE + 0x44UL);
	hi = c6_read(C6_EFUSE + 0x48UL);
	snprintf(text, sizeof(text), "%02lx%02lx%02lx%02lx%02lx%02lx",
		lo & 0xffUL, (lo >> 8) & 0xffUL, (lo >> 16) & 0xffUL, (lo >> 24) & 0xffUL,
		hi & 0xffUL, (hi >> 8) & 0xffUL);
	return(text);
}

mp_ret_t mp_machine_init(mp_kernel_t *kernel) {
	_name(kernel);

	mp_gate_init(kernel);
	mp_interrupt_init();
	mp_gpio_init();
	mp_adc_init();
	mp_timer_init(kernel);
	mp_clock_init(kernel);
	mp_uart_init();
	mp_spi_init();
	mp_i2c_init();
	mp_interrupt_enable();
#if defined(SUPPORT_ESP32_RF) && defined(SUPPORT_COMMON_DEVICE)
	mp_c6_phy(kernel);
	mp_c6_mac(kernel, mp_c6_phy_face());
#endif
	return(TRUE);
}

mp_ret_t mp_machine_fini(mp_kernel_t *kernel) {
#if defined(SUPPORT_ESP32_RF) && defined(SUPPORT_COMMON_DEVICE)
	mp_c6_mac_fini(kernel);
	mp_c6_phy_fini(kernel);
#endif
	mp_interrupt_disable();
	mp_i2c_fini();
	mp_spi_fini();
	mp_uart_fini();
	mp_clock_fini(kernel);
	mp_timer_fini(kernel);
	mp_gpio_fini();
	mp_interrupt_fini();
	mp_gate_fini(kernel);
	return(TRUE);
}

void mp_machine_reset(void) {
	c6_write(C6_LP_AON + 0x34UL, 1UL << 31);
	for(;;)
		;
}

#ifdef SUPPORT_COMMON_DEVICE
/* Die temperature. The SAR sensor is in the HP domain, so it does not need the radio.
 * PCR.tsens_clk_conf is C6_PCR+0x88: bit 20 selects XTAL, bit 22 enables the clock,
 * bit 23 held at 0 leaves the block out of reset.
 * APB_SARADC.saradc_apb_tsens_ctrl is 0x6000E058: bits 7:0 are the code, bits 21:14
 * the divider, bit 22 the power. The room range is DAC offset 0, so
 * °C = 0.4386 * code - 20.52 - efuse/10. The factory correction is the 9-bit
 * TEMP_CALIB field at the start of efuse block 2 word 4; bit 8 is the sign. */
#define _DIE_PCR   (C6_PCR + 0x88UL)
#define _DIE_CTRL  0x6000E058UL
#define _DIE_EFUSE (C6_EFUSE + 0x6CUL)

static mp_device_driver_t _mp_arch_sensors;
static mp_device_registry_t _mp_arch_tempDev;
static mp_device_measure_t _mp_arch_temp;
static mp_task_t *_mp_arch_temp_task;
static mp_kernel_t *_mp_arch_kernel;
static int _mp_arch_temp_live;
static int _mp_arch_temp_wait;

static unsigned long _die_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);
	ticks = (ms / 1000UL) * hz + ((ms % 1000UL) * hz) / 1000UL;
	return(ticks ? ticks : 1);
}

static float _die_cal(void) {
	unsigned long word = c6_read(_DIE_EFUSE) & 0x1ffUL;
	float mag = (float)(word & 0xffUL) / 10.0f;

	return((word & 0x100UL) ? -mag : mag);
}

static void _die_power(int on) {
	unsigned long clk = c6_read(_DIE_PCR);
	unsigned long ctrl = c6_read(_DIE_CTRL);

	clk |= (1UL << 20) | (1UL << 22);
	clk &= ~(1UL << 23);
	c6_write(_DIE_PCR, clk);
	ctrl &= ~(0xffUL << 14);
	ctrl |= (6UL << 14);
	if(on)
		ctrl |= (1UL << 22);
	else
		ctrl &= ~(1UL << 22);
	c6_write(_DIE_CTRL, ctrl);
}

static float _die_celsius(void) {
	unsigned long code = c6_read(_DIE_CTRL) & 0xffUL;

	return(0.4386f * (float)code - 20.52f - _die_cal());
}

static MP_TASK(_die_task) {
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	if(_mp_arch_temp_wait) {
		_mp_arch_temp_wait = 0;
		mp_task_delay(task, _die_ticks(20));
		return;
	}
	mp_device_measure_publish(&_mp_arch_temp, _die_celsius());
	mp_task_delay(task, _die_ticks(1000));
}

static void _die_start(void *self) {
	(void)self;
	_die_power(1);
	_mp_arch_temp_wait = 1;
	_mp_arch_temp_task = mp_task_create(&_mp_arch_kernel->tasks, "die", _die_task, NULL, _die_ticks(20));
}

static void _die_stop(void *self) {
	(void)self;
	if(_mp_arch_temp_task) {
		mp_task_destroy(_mp_arch_temp_task);
		_mp_arch_temp_task = NULL;
	}
	_die_power(0);
}

static void _die_publish(mp_kernel_t *kernel) {
	static const mp_device_driver_ops_t ops = {
		"arch/esp32c6", "ESP32-C6 die temperature", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_TEMPERATURE),
		NULL, NULL, NULL, _die_start, _die_stop
	};

	if(_mp_arch_temp_live)
		return;
	_mp_arch_kernel = kernel;
	memset(&_mp_arch_temp, 0, sizeof(_mp_arch_temp));
	if(mp_device_driver_register(kernel, &_mp_arch_sensors, &ops, kernel, NULL) == FALSE)
		return;
	if(!mp_device_registry_register(&_mp_arch_sensors, &_mp_arch_tempDev, MP_DEVICE_TEMPERATURE, 0, &_mp_arch_temp, "esp32c6", mp_machine_serial(), NULL)) {
		mp_device_driver_unregister(kernel, &_mp_arch_sensors);
		return;
	}
	_mp_arch_tempDev.label = "CPU";
	_mp_arch_temp_live = 1;
}

static void _die_withdraw(mp_kernel_t *kernel) {
	if(!_mp_arch_temp_live)
		return;
	mp_device_registry_unregister(&_mp_arch_tempDev);
	mp_device_driver_unregister(kernel, &_mp_arch_sensors);
	_mp_arch_temp_live = 0;
	_mp_arch_kernel = NULL;
}
#endif

void mp_machine_state_set(mp_kernel_t *kernel) {
#ifdef SUPPORT_COMMON_DEVICE
	_die_publish(kernel);
#else
	(void)kernel;
#endif
}

void mp_machine_state_unset(mp_kernel_t *kernel) {
#ifdef SUPPORT_COMMON_DEVICE
	_die_withdraw(kernel);
#endif
#if defined(SUPPORT_ESP32_RF) && defined(SUPPORT_COMMON_DEVICE)
	mp_c6_mac_fini(kernel);
	mp_c6_phy_fini(kernel);
#else
	(void)kernel;
#endif
}

/**@}*/
