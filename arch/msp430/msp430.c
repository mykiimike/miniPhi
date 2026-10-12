/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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

static void _set_machine_temperature(mp_kernel_t *kernel);
static void _unset_machine_temperature(mp_kernel_t *kernel);
static void _processor_temp(mp_adc_t *adc);


mp_ret_t mp_machine_init(mp_kernel_t *kernel) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

	kernel->mcuVendor = "Texas Instrument";

#if defined(__msp430x54xA)
	if ((*((char *)0x1A04) == 0x05) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5438A";
	else if ((*((char *)0x1A04) == 0x04) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5437A";
	else if ((*((char *)0x1A04) == 0x03) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5436A";
	else if ((*((char *)0x1A04) == 0x02) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5435A";
	else if ((*((char *)0x1A04) == 0x01) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5419A";
	else if ((*((char *)0x1A04) == 0x00) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5418A";

#elif defined(__msp430x54x)
	if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x38))
		kernel->mcuName = "MSP430F5438";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x37))
		kernel->mcuName = "MSP430F5437";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x36))
		kernel->mcuName = "MSP430F5436";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x35))
		kernel->mcuName = "MSP430F5435";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x19))
		kernel->mcuName = "MSP430F5419";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x18))
			kernel->mcuName = "MSP430F5418";
#endif

	/* initialize GATEs */
	mp_gate_init(kernel);

	/* initialize interrupts */
	mp_interrupt_init();

	/* initialize GPIO */
	mp_gpio_init();

	/* initialize timer */
	mp_timer_init(kernel);

	/* intialize clock */
	mp_clock_init(kernel);

	/* set high energy */
	mp_clock_high_energy(kernel);

	/* intialize UART */
	mp_uart_init();

	/* initialize SPI */
	mp_spi_init();

	/* enter in interruptible mode */
	mp_interrupt_enable();

	/* initialize temp processor sensor */
	//_set_machine_temperature(kernel);

	return(TRUE);
}

mp_ret_t mp_machine_fini(mp_kernel_t *kernel) {

	/* terminate SPI */
	mp_spi_fini();

	/* intialize UART */
	mp_uart_fini();

	/* terminate clock */
	mp_clock_fini(kernel);

	/* initialize timer */
	mp_timer_fini(kernel);

	/* terminate GPIO */
	mp_gpio_fini();

	/* terminate interrupts */
	mp_interrupt_fini();

	/* terminate GATEs */
	mp_gate_fini(kernel);

	return(TRUE);
}


void mp_machine_state_set(mp_kernel_t *kernel) {
	_set_machine_temperature(kernel);

}

void mp_machine_state_unset(mp_kernel_t *kernel) {
	_unset_machine_temperature(kernel);

}

static void _set_machine_temperature(mp_kernel_t *kernel) {
#ifdef SUPPORT_COMMON_SENSOR
#if defined(__msp430x54x) || defined(__msp430x54xA)
	/* create mcu temperature */
	{
		mp_options_t options[] = {
				{ "channel", "A10" },
				{ "delay", "500" },
				{ NULL, NULL }
		};
		mp_adc_create(kernel, &kernel->internalTemp, options, "MCU temperature");

		kernel->sensorMCU = mp_sensor_register(kernel, MP_SENSOR_TEMPERATURE, "MSP430 Internal");

		kernel->internalTemp.callback = _processor_temp;
	}
#endif
#endif
}


static void _unset_machine_temperature(mp_kernel_t *kernel) {
#ifdef SUPPORT_COMMON_SENSOR
#if defined(__msp430x54x) || defined(__msp430x54xA)
	mp_sensor_unregister(kernel, kernel->sensorMCU);
	mp_adc_remove(&kernel->internalTemp);
#endif
#endif
}


#if defined(__msp430x54x)
	#define _CALADC12_15V_30C  *((unsigned int *)0x1A1C)   // Temperature Sensor Calibration-30 C
	#define _CALADC12_15V_85C  *((unsigned int *)0x1A1E)   // Temperature Sensor Calibration-85 C
#elif defined(__msp430x54xA)
	#define _CALADC12_15V_30C  *((unsigned int *)0x1A1A)   // Temperature Sensor Calibration-30 C
	#define _CALADC12_15V_85C  *((unsigned int *)0x1A1C)   // Temperature Sensor Calibration-85 C
#endif

#ifdef SUPPORT_COMMON_SENSOR
#if defined(__msp430x54x) || defined(__msp430x54xA)

static void _processor_temp(mp_adc_t *adc) {
    adc->kernel->sensorMCU->temperature.result = (float)(((long)adc->result - _CALADC12_15V_30C) * (85 - 30)) /
		(_CALADC12_15V_85C - _CALADC12_15V_30C) + 30.0f;
}

#endif
#endif
