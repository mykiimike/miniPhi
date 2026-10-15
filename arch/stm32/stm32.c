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
@defgroup mpArchSTM32Machine Machine

@ingroup mpArchSTM32

@{
*/

#ifdef SUPPORT_COMMON_SENSOR
static void _mp_machine_temp(mp_adc_t *adc);
static void _mp_machine_vref(mp_adc_t *adc);
#endif

/* RM0008 / datasheet typical values */
#define _VREFINT 1.20f
#define _TEMP_V25 1.43f
#define _TEMP_SLOPE 0.0043f

static const char *_mp_machine_density(uint32_t devId) {
	switch(devId) {
		case 0x412: return("low-density");
		case 0x410: return("medium-density");
		case 0x414: return("high-density");
		case 0x430: return("XL-density");
		case 0x418: return("connectivity line");
		case 0x420: return("value line");
		case 0x428: return("value line high-density");
	}
	return(NULL);
}

static void _mp_machine_name(mp_kernel_t *kernel) {
	const char *density;
	uint32_t idcode;

	/* reads 0 without a debugger on most F1 revisions (errata 2.3) */
	idcode = DBGMCU_IDCODE;
	density = _mp_machine_density(idcode & 0xFFF);

	if(density)
		snprintf(kernel->arch.mcuName, sizeof(kernel->arch.mcuName), "STM32F1 %s, %u KB flash, rev %04lX",
			density, (unsigned int)FLASH_SIZE_KB, (unsigned long)(idcode >> 16));
	else
		snprintf(kernel->arch.mcuName, sizeof(kernel->arch.mcuName), "STM32F1, %u KB flash", (unsigned int)FLASH_SIZE_KB);

	kernel->mcuVendor = "STMicroelectronics";
	kernel->mcuName = kernel->arch.mcuName;
}

mp_ret_t mp_machine_init(mp_kernel_t *kernel) {
	_mp_machine_name(kernel);

	mp_gate_init(kernel);

	mp_interrupt_init();

	mp_gpio_init();

	mp_timer_init(kernel);

	mp_clock_init(kernel);

	mp_uart_init();

	mp_spi_init();

	mp_i2c_init();

	mp_interrupt_enable();

	return(TRUE);
}

mp_ret_t mp_machine_fini(mp_kernel_t *kernel) {
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
	__asm volatile ("dsb" ::: "memory");
	SCB->AIRCR = SCB_AIRCR_RESET;
	__asm volatile ("dsb" ::: "memory");
	while(1);
}

void mp_machine_state_set(mp_kernel_t *kernel) {
#ifdef SUPPORT_COMMON_SENSOR
	mp_options_t vref[] = {
		{ "channel", "A17" },
		{ "delay", "5000" },
		{ NULL, NULL }
	};
	mp_options_t temp[] = {
		{ "channel", "A16" },
		{ "delay", "5000" },
		{ NULL, NULL }
	};

	kernel->arch.vdda = 3.3f;

	if(mp_adc_create(kernel, &kernel->arch.internalVref, vref, "MCU VDDA") == TRUE) {
		kernel->arch.sensorVDDA = mp_sensor_register(kernel, MP_SENSOR_VOLTAGE, "STM32 VDDA");
		kernel->arch.internalVref.callback = _mp_machine_vref;
	}

	if(mp_adc_create(kernel, &kernel->arch.internalTemp, temp, "MCU temperature") == TRUE) {
		kernel->arch.sensorMCU = mp_sensor_register(kernel, MP_SENSOR_TEMPERATURE, "STM32 Internal");
		kernel->arch.internalTemp.callback = _mp_machine_temp;
	}
#endif
}

void mp_machine_state_unset(mp_kernel_t *kernel) {
#ifdef SUPPORT_COMMON_SENSOR
	mp_sensor_unregister(kernel, &kernel->arch.sensorMCU);
	if(kernel->arch.internalTemp.callback) {
		mp_adc_remove(&kernel->arch.internalTemp);
		kernel->arch.internalTemp.callback = NULL;
	}

	mp_sensor_unregister(kernel, &kernel->arch.sensorVDDA);
	if(kernel->arch.internalVref.callback) {
		mp_adc_remove(&kernel->arch.internalVref);
		kernel->arch.internalVref.callback = NULL;
	}
#endif
}

#ifdef SUPPORT_COMMON_SENSOR
static void _mp_machine_vref(mp_adc_t *adc) {
	mp_kernel_t *kernel = adc->kernel;

	if(adc->result <= 0)
		return;

	kernel->arch.vdda = _VREFINT * 4095.0f / (float)adc->result;
	mp_sensor_publish(kernel->arch.sensorVDDA, kernel->arch.vdda);
}

static void _mp_machine_temp(mp_adc_t *adc) {
	mp_kernel_t *kernel = adc->kernel;
	float vsense = (float)adc->result * kernel->arch.vdda / 4095.0f;

	mp_sensor_publish(kernel->arch.sensorMCU, (_TEMP_V25 - vsense) / _TEMP_SLOPE + 25.0f);
}
#endif

/**@}*/
