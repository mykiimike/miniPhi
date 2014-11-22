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

#ifdef __OLIMEX_PROTOBOARD__

#include <mp.h>

/* use reserver number 1 to define OLIMEX state machine  */
#define OLIMEX_OP MP_KERNEL_RES01

typedef struct olimex_msp430_s olimex_msp430_t;

struct olimex_msp430_s {
	mp_kernel_t kernel;
	mp_drv_led_t green_led;
	mp_drv_led_t red_led;
	mp_serial_t uart_usb_rs232;

	mp_drv_button_t bUp;
	mp_drv_button_t bDown;
	mp_drv_button_t bLeft;
	mp_drv_button_t bRight;
	mp_drv_button_t bCenter;

	/** implementation of power on/off */
	mp_drv_button_event_t bPower;



};

static void __olimex_onBoot(void *user);
static void __olimex_state_op_set(void *user);
static void __olimex_state_op_unset(void *user);
static void __olimex_state_op_tick(void *user);

static void __olimex_processor_temp(mp_adc_t *adc);
void __olimex_on_button_left(void *user);
void __olimex_on_button_right(void *user);
void __olimex_on_button_up(void *user);
void __olimex_on_button_down(void *user);
void __olimex_on_button_power(void *user);

static olimex_msp430_t __olimex;

int main(void) {
	/* initialize kernel */
	mp_kernel_init(&__olimex.kernel, __olimex_onBoot, &__olimex);

	/* define OLIMEX OP machine state */
	mp_state_define(
		&__olimex.kernel.states,
		OLIMEX_OP, "OP", &__olimex,
		__olimex_state_op_set,
		__olimex_state_op_unset,
		__olimex_state_op_tick
	);

	/* master loop */
	mp_kernel_loop(&__olimex.kernel);

	/* terminate kernel */
	mp_kernel_fini(&__olimex.kernel);

	return 0;
}

static void __olimex_onBoot(void *user) {
	olimex_msp430_t *olimex;
	olimex = user;

	/* switch to olimex operationnal state */
	mp_kernel_state(&olimex->kernel, OLIMEX_OP);
}

MP_TASK(blinkTask) {
	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	mp_drv_led_t *led = task->user;
	mp_drv_led_turn(led);
}

static void __olimex_state_op_set(void *user) {
	olimex_msp430_t *olimex = user;

	/* initialize green led */
	{
		mp_options_t options[] = {
				{ "port", "p10.6" },
				{ NULL, NULL }
		};
		mp_drv_led_init(&olimex->kernel, &olimex->red_led, options, "Red LED");
	}
	{
		mp_options_t options[] = {
				{ "port", "p10.7" },
				{ NULL, NULL }
		};
		mp_drv_led_init(&olimex->kernel, &olimex->green_led, options, "Green LED");
	}


	/* initialize UART USB RS232 */
	memset(&olimex->uart_usb_rs232, 0, sizeof(olimex->uart_usb_rs232));
	olimex->uart_usb_rs232.uart.gateId = "USCI_A3";
	olimex->uart_usb_rs232.uart.baudRate = 9600;

	olimex->uart_usb_rs232.uart.rxd.port = 10;
	olimex->uart_usb_rs232.uart.rxd.pin = 5;

	olimex->uart_usb_rs232.uart.txd.port = 10;
	olimex->uart_usb_rs232.uart.txd.pin = 4;
	mp_serial_init(&olimex->uart_usb_rs232, "USB RS232");

	/* create buttons */
	{
		mp_options_t options[] = {
				{ "port", "p1.6" },
				{ NULL, NULL }
		};
		olimex->bLeft.user = olimex;
		olimex->bLeft.onSwitch = __olimex_on_button_left;
		mp_drv_button_init(&olimex->kernel, &olimex->bLeft, options, "LEFT B");
	}


	{
		mp_options_t options[] = {
				{ "port", "p1.5" },
				{ NULL, NULL }
		};
		olimex->bRight.user = olimex;
		olimex->bRight.onSwitch = __olimex_on_button_right;
		mp_drv_button_init(&olimex->kernel, &olimex->bRight, options, "RIGHT B");
	}


	/* simply create center button */
	{
		mp_options_t options[] = {
				{ "port", "p1.4" },
				{ NULL, NULL }
		};
		olimex->bCenter.user = NULL;
		olimex->bCenter.onSwitch = NULL;
		mp_drv_button_init(&olimex->kernel, &olimex->bCenter, options, "CENTER B");
	}

	{
		mp_options_t options[] = {
				{ "port", "p1.3" },
				{ NULL, NULL }
		};
		olimex->bUp.user = olimex;
		olimex->bUp.onSwitch = __olimex_on_button_up;
		mp_drv_button_init(&olimex->kernel, &olimex->bUp, options, "UP B");
	}

	{
		mp_options_t options[] = {
				{ "port", "p1.2" },
				{ NULL, NULL }
		};
		olimex->bDown.user = olimex;
		olimex->bDown.onSwitch = __olimex_on_button_down;
		mp_drv_button_init(&olimex->kernel, &olimex->bDown, options, "DOWN B");
	}




	/* create a button event based on center button in order to turn a led on/off */
	mp_drv_button_event_create(
			&olimex->bCenter, &olimex->bPower,
			1000, 2, __olimex_on_button_power, olimex
	);

	/* pinout */
	mp_pinout_onoff(&olimex->kernel, olimex->green_led.gpio, ON, 10, 2010, 0, "Blinking green - Power ON");

	char buffer[100];
	int size;
	size = snprintf(buffer, sizeof(buffer)-1, "miniPhi version %s\n\r", olimex->kernel.version);
	mp_serial_write(&olimex->uart_usb_rs232, buffer, size);

	//mp_drv_led_turn(&olimex->red_led);

	//mp_task_create(&olimex->kernel.tasks, "Blinking RED", blinkTask, &olimex->red_led, 500);
	//mp_task_create(&olimex->kernel.tasks, "Blinking GREEN", blinkTask, &olimex->green_led, 1000);

	mp_machine_state_set(&olimex->kernel);
}



static void __olimex_state_op_unset(void *user) {
	olimex_msp430_t *olimex = user;

	mp_drv_button_fini(&olimex->bLeft);
	mp_drv_button_fini(&olimex->bRight);
	mp_drv_button_fini(&olimex->bCenter);
	mp_drv_button_fini(&olimex->bUp);
	mp_drv_button_fini(&olimex->bDown);

	mp_serial_fini(&olimex->uart_usb_rs232);

	mp_drv_led_fini(&olimex->red_led);
	mp_drv_led_fini(&olimex->green_led);

	mp_machine_state_unset(&olimex->kernel);
}


static void __olimex_state_op_tick(void *user) {
	/* master loop, better to use tasks */
}




void __olimex_on_button_left(void *user) {
	//olimex_msp430_t *olimex = user;
	//mp_drv_led_turn(&olimex->red_led);
}

void __olimex_on_button_right(void *user) {
	//olimex_msp430_t *olimex = user;
	//mp_drv_led_turn(&olimex->green_led);

}

void __olimex_on_button_up(void *user) {
	//olimex_msp430_t *olimex = user;
	//mp_drv_led_turn(&olimex->green_led);

}

void __olimex_on_button_down(void *user) {
	//olimex_msp430_t *olimex = user;
	//mp_drv_led_turn(&olimex->red_led);

}

void __olimex_on_button_power(void *user) {
	olimex_msp430_t *olimex = user;
	mp_drv_led_turn(&olimex->red_led);
}

#endif

