/* Host stand-in for common/mp.h. make test puts this directory first on the include path. */

#ifndef _MP_H
#define _MP_H

#include <string.h>

#define TRUE 1
#define FALSE 0
#define YES TRUE
#define NO FALSE

typedef signed char mp_ret_t;
typedef signed char mp_bool_t;
typedef struct mp_kernel_s mp_kernel_t;
typedef struct mp_options_s mp_options_t;

struct mp_options_s {
	char *key;
	char *value;
};

void test_printk(const char *fmt, ...);
#define mp_printk(a, args...) test_printk(a, ##args)

/* host stand-ins for the IRQ-safe section and the clock used by measure.c / input.c */
#define MP_INTERRUPT_SAFE_BEGIN
#define MP_INTERRUPT_SAFE_END

#ifdef MP_HOST_TASK
/* task tests step the clock themselves; device tests keep the increment below */
static unsigned long _mp_test_ticks;
static inline unsigned long mp_clock_ticks(void) {
	return(_mp_test_ticks);
}
static inline void mp_test_clock(unsigned long ticks) {
	_mp_test_ticks = ticks;
}
static inline unsigned long mp_clock_tick_hz(void) {
	return(1000);
}
#include <list.h>
#ifndef MP_TASK_MAX
#define MP_TASK_MAX 8
#endif
#include <task/task.h>
#else
static unsigned long _mp_test_ticks;
static inline unsigned long mp_clock_ticks(void) {
	return(_mp_test_ticks++);
}
static inline unsigned long mp_clock_tick_hz(void) {
	return(1000);
}
/* the input engine holds task pointers; the host tests never wake one */
struct mp_task_s;
static inline void mp_task_wake(struct mp_task_s *task) {
	(void)task;
}
typedef unsigned char mp_task_lock_t;
#define mp_task_lock(ceiling) ((mp_task_lock_t)0)
#define mp_task_unlock(key)   ((void)(key))
#define MP_TASK_CEILING_KERNEL ((unsigned char)0)
#include <list.h>
#endif

#ifdef SUPPORT_COMMON_DEVICE
#include <device/device.h>
struct mp_kernel_s {
	mp_list_t inputs;
	mp_list_t drivers;
	mp_list_t devices;
};
#else
struct mp_kernel_s {
	int unused;
};
#endif

#endif
