/* Host test of serial printk vs a terminal. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define TRUE 1
#define FALSE 0
#define YES TRUE
#define NO FALSE

typedef signed char mp_ret_t;
typedef signed char mp_bool_t;
typedef struct mp_kernel_s { int unused; } mp_kernel_t;
typedef struct mp_options_s { char *key; char *value; } mp_options_t;
typedef struct mp_task_s mp_task_t;
struct mp_task_s { int unused; };

#define MP_POWER_LOW 1
typedef int mp_power_level_t;
typedef struct { int unused; } mp_power_hold_t;

typedef struct mp_uart_s mp_uart_t;
typedef void (*mp_uart_on_t)(mp_uart_t *uart);
struct mp_uart_s {
	void *user;
	mp_uart_on_t onRead;
	mp_uart_on_t onWrite;
};

typedef struct mp_circular_s mp_circular_t;
typedef void (*mp_circular_int_t)(mp_circular_t *cir);
struct mp_circular_s {
	void *user;
	mp_circular_int_t enable;
	mp_circular_int_t disable;
	int totalSize;
};

typedef void (*mp_printk_call_t)(void *user, char *fmt, ...);
static void _dummy_printk(void *user, char *fmt, ...) {
	(void)user;
	(void)fmt;
}
mp_printk_call_t mp_printk_call = _dummy_printk;
void *mp_printk_user;

mp_ret_t mp_printk_set(mp_printk_call_t call, void *user) {
	mp_printk_call = call;
	mp_printk_user = user;
	return(TRUE);
}

mp_ret_t mp_printk_unset(void) {
	mp_printk_call = _dummy_printk;
	mp_printk_user = NULL;
	return(TRUE);
}

#define mp_printk(a, args...) mp_printk_call(mp_printk_user, a, ##args)

#define _MP_H
#define SUPPORT_COMMON_SERIAL
#define MP_SERIAL_AWAKE 30000

#include "../serial.h"

static unsigned char tx[512];
static int txLen;
static int failed;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

mp_ret_t mp_circular_init(mp_kernel_t *kernel, mp_circular_t *cir, mp_circular_int_t enable, mp_circular_int_t disable) {
	(void)kernel;
	memset(cir, 0, sizeof(*cir));
	cir->enable = enable;
	cir->disable = disable;
	return(TRUE);
}

void mp_circular_fini(mp_circular_t *cir) {
	(void)cir;
}

void mp_circular_start(mp_circular_t *cir) {
	if(cir->enable)
		cir->enable(cir);
}

mp_ret_t mp_circular_write(mp_circular_t *cir, unsigned char *data, int size) {
	(void)cir;
	if(size < 0 || (txLen + size) > (int)sizeof(tx))
		return(FALSE);
	memcpy(tx + txLen, data, (size_t)size);
	txLen += size;
	return(TRUE);
}

void mp_circular_rxInterrupt(mp_circular_t *cir, unsigned char c) {
	(void)cir;
	(void)c;
}

unsigned char mp_circular_txInterrupt(mp_circular_t *cir, mp_bool_t *done) {
	(void)cir;
	*done = YES;
	return(0);
}

mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who) {
	(void)kernel;
	(void)options;
	(void)who;
	memset(uart, 0, sizeof(*uart));
	return(TRUE);
}

mp_ret_t mp_uart_close(mp_uart_t *uart) {
	(void)uart;
	return(TRUE);
}

unsigned char mp_uart_rx(mp_uart_t *uart) {
	(void)uart;
	return(0);
}

void mp_uart_tx(mp_uart_t *uart, unsigned char data) {
	(void)uart;
	(void)data;
}

void mp_uart_enable_tx_int(mp_uart_t *uart) { (void)uart; }
void mp_uart_disable_tx_int(mp_uart_t *uart) { (void)uart; }
void mp_uart_enable_rx_int(mp_uart_t *uart) { (void)uart; }
void mp_uart_disable_rx_int(mp_uart_t *uart) { (void)uart; }

mp_bool_t mp_uart_sync(mp_uart_t *uart) {
	(void)uart;
	return(NO);
}

void mp_uart_write_sync(const unsigned char *data, int size) {
	(void)data;
	(void)size;
}

mp_bool_t mp_uart_tx_ok(void) {
	return(YES);
}

mp_bool_t mp_uart_host_back(void) {
	return(NO);
}

int mp_uart_tx_room(mp_uart_t *uart) {
	(void)uart;
	return(0);
}

void mp_uart_write(mp_uart_t *uart, const unsigned char *data, int n) {
	(void)uart;
	(void)data;
	(void)n;
}

int mp_circular_txTake(mp_circular_t *cir, unsigned char *buf, int max, mp_bool_t *done) {
	(void)cir;
	(void)buf;
	(void)max;
	*done = YES;
	return(0);
}

void mp_circular_txStop(mp_circular_t *cir) {
	(void)cir;
}

void mp_power_hold_for(mp_kernel_t *kernel, mp_power_hold_t *hold, mp_power_level_t floor, unsigned long ms, char *who) {
	(void)kernel;
	(void)hold;
	(void)floor;
	(void)ms;
	(void)who;
}

void mp_power_release(mp_kernel_t *kernel, mp_power_hold_t *hold) {
	(void)kernel;
	(void)hold;
}

void mp_task_wake(mp_task_t *task) {
	(void)task;
}

#include "../serial.c"

static void _tx_reset(void) {
	txLen = 0;
	memset(tx, 0, sizeof(tx));
}

int main(void) {
	mp_kernel_t kernel;
	mp_uart_t uart;
	mp_serial_t serial;
	mp_serial_t other;
	mp_options_t options[] = { { NULL, NULL } };

	memset(&kernel, 0, sizeof(kernel));
	memset(&uart, 0, sizeof(uart));
	mp_printk_unset();

	expect(mp_serial_printk(&serial) == FALSE, "printk on a closed serial");
	expect(mp_serial_terminal(&serial) == FALSE, "terminal on a closed serial");

	expect(mp_serial_initUART(&kernel, &serial, &uart, "t") == TRUE, "initUART");
	expect(serial.use == MP_SERIAL_NONE, "open leaves the line free");

	_tx_reset();
	expect(mp_serial_printk(&serial) == TRUE, "wire printk");
	expect(serial.use == MP_SERIAL_PRINTK, "use is printk");
	mp_printk("hi");
	expect(txLen == 4 && memcmp(tx, "hi\n\r", 4) == 0, "printk writes a CR LF line");

	expect(mp_serial_terminal(&serial) == TRUE, "terminal takes the line");
	expect(serial.use == MP_SERIAL_TERMINAL, "use is terminal");
	expect(mp_serial_printk(&serial) == FALSE, "printk refused while a terminal has the line");
	_tx_reset();
	mp_printk("no");
	expect(txLen == 0, "unwired printk writes nothing");

	_tx_reset();
	mp_serial_write(&serial, (unsigned char *)"ok", 2);
	expect(txLen == 2 && memcmp(tx, "ok", 2) == 0, "write stays the byte pipe");

	mp_serial_give(&serial);
	expect(serial.use == MP_SERIAL_NONE, "give frees the line");
	expect(mp_serial_printk(&serial) == TRUE, "printk after give");
	_tx_reset();
	mp_printk("up");
	expect(txLen == 4 && memcmp(tx, "up\n\r", 4) == 0, "printk works again");

	expect(mp_serial_initUART(&kernel, &other, &uart, "u") == TRUE, "second serial");
	expect(mp_serial_printk(&other) == TRUE, "second serial steals printk");
	expect(serial.use == MP_SERIAL_NONE, "first serial is given back");
	expect(other.use == MP_SERIAL_PRINTK, "second serial is the sink");
	expect(mp_serial_printk(&serial) == TRUE, "first serial steals back");
	expect(other.use == MP_SERIAL_NONE, "second serial is given back");

	expect(mp_serial_open(&kernel, &serial, options, "Console") == TRUE, "open");
	expect(mp_serial_printk(&serial) == TRUE, "printk after open");
	expect(mp_serial_fini(&serial) == TRUE, "fini");
	expect(mp_printk_user == NULL, "fini unwires printk");
	_tx_reset();
	mp_printk("gone");
	expect(txLen == 0, "fini leaves printk silent");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
