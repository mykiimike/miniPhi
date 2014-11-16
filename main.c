#include <mp.h>

#define OLIMEX_OP MP_KERNEL_RES01

typedef struct olimex_msp430_s olimex_msp430_t;

struct olimex_msp430_s {
	mp_kernel_t kernel;
	mp_led_t green_led;
	mp_led_t red_led;
	mp_serial_t uart_usb_rs232;
};

static void __olimex_onBoot(void *user);
static void __olimex_state_op_set(void *user);
static void __olimex_state_op_unset(void *user);
static void __olimex_state_op_tick(void *user);

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
	olimex_msp430_t *olimex = user;

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

	mp_task_create(&olimex->kernel.tasks, "Blinking RED", blinkTask, &olimex->red_led, 500);
	mp_task_create(&olimex->kernel.tasks, "Blinking GREEN", blinkTask, &olimex->green_led, 1000);

}

static void __olimex_state_op_unset(void *user) {
	olimex_msp430_t *olimex = user;

	mp_serial_fini(&olimex->uart_usb_rs232);

	mp_led_fini(&olimex->red_led);

	mp_led_fini(&olimex->green_led);
}


static void __olimex_state_op_tick(void *user) {
	/* master loop, better to use tasks */
}


