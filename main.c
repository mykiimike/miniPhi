#ifdef __OLIMEX_PROTOBOARD__

#include <mp.h>

/* use reserver number 1 to define OLIMEX state machine  */
#define OLIMEX_OP MP_KERNEL_RES01

typedef struct olimex_msp430_s olimex_msp430_t;

struct olimex_msp430_s {
	mp_kernel_t kernel;
	mp_led_t green_led;
	mp_led_t red_led;
	mp_serial_t uart_usb_rs232;

	mp_button_t bUp;
	mp_button_t bDown;
	mp_button_t bLeft;
	mp_button_t bRight;
	mp_button_t bCenter;

	/** implementation of power on/off */
	mp_button_event_t bPower;
};

static void __olimex_onBoot(void *user);
static void __olimex_state_op_set(void *user);
static void __olimex_state_op_unset(void *user);
static void __olimex_state_op_tick(void *user);

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

	mp_led_t *led = task->user;
	mp_led_turn(led);
}

static void __olimex_state_op_set(void *user) {
	olimex_msp430_t *olimex = user;

	/* initialize green led */
	mp_led_init(&olimex->green_led, 10, 7, "Green LED");
	mp_led_init(&olimex->red_led, 10, 6, "Red LED");

	/* initialize UART USB RS232 */
	memset(&olimex->uart_usb_rs232, 0, sizeof(olimex->uart_usb_rs232));
	olimex->uart_usb_rs232.uart.gateId = 3; /* UCA3 */
	olimex->uart_usb_rs232.uart.baudRate = 9600;

	olimex->uart_usb_rs232.uart.rxd.port = 10;
	olimex->uart_usb_rs232.uart.rxd.pin = 5;

	olimex->uart_usb_rs232.uart.txd.port = 10;
	olimex->uart_usb_rs232.uart.txd.pin = 4;
	mp_serial_init(&olimex->uart_usb_rs232, "USB RS232");

	/* create buttons */
	olimex->bLeft.user = olimex;
	olimex->bLeft.onSwitch = __olimex_on_button_left;
	mp_button_init(&olimex->bLeft, 1, 6, "LEFT B");

	olimex->bRight.user = olimex;
	olimex->bRight.onSwitch = __olimex_on_button_right;
	mp_button_init(&olimex->bRight, 1, 5, "RIGHT B");

	/* simply create center button */
	mp_button_init(&olimex->bCenter, 1, 4, "CENTER B");

	olimex->bUp.user = olimex;
	olimex->bUp.onSwitch = __olimex_on_button_up;
	mp_button_init(&olimex->bUp, 1, 3, "UP B");

	olimex->bDown.user = olimex;
	olimex->bDown.onSwitch = __olimex_on_button_down;
	mp_button_init(&olimex->bDown, 1, 2, "DOWN B");

	/* create a button event based on center button in order to turn a led on/off */
	mp_button_event_create(
			&olimex->bCenter, &olimex->bPower,
			1000, 2, __olimex_on_button_power, olimex
	);

	mp_pinout_onoff(&olimex->kernel, olimex->green_led.gpio, ON, 10, 2010, 0, "Blinking green - Power ON");

	//mp_task_create(&olimex->kernel.tasks, "Blinking RED", blinkTask, &olimex->red_led, 500);
	//mp_task_create(&olimex->kernel.tasks, "Blinking GREEN", blinkTask, &olimex->green_led, 1000);

}

static void __olimex_state_op_unset(void *user) {
	volatile olimex_msp430_t *olimex = user;

	mp_button_fini(&olimex->bLeft);
	mp_button_fini(&olimex->bRight);
	mp_button_fini(&olimex->bCenter);
	mp_button_fini(&olimex->bUp);
	mp_button_fini(&olimex->bDown);

	mp_serial_fini(&olimex->uart_usb_rs232);

	mp_led_fini(&olimex->red_led);
	mp_led_fini(&olimex->green_led);
}


static void __olimex_state_op_tick(void *user) {
	/* master loop, better to use tasks */
}

void __olimex_on_button_left(void *user) {
	//olimex_msp430_t *olimex = user;
	//mp_led_turn(&olimex->red_led);
}

void __olimex_on_button_right(void *user) {
	//olimex_msp430_t *olimex = user;
	//mp_led_turn(&olimex->green_led);

}

void __olimex_on_button_up(void *user) {
	//olimex_msp430_t *olimex = user;
	//mp_led_turn(&olimex->green_led);

}

void __olimex_on_button_down(void *user) {
	//olimex_msp430_t *olimex = user;
	//mp_led_turn(&olimex->red_led);

}

void __olimex_on_button_power(void *user) {
	olimex_msp430_t *olimex = user;
	mp_led_turn(&olimex->green_led);
}

#endif

