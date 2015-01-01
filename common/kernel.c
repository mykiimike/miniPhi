#include <mp.h>


static void _mp_kernel_state_boot_set(void *user);
static void _mp_kernel_state_boot_unset(void *user);
static void _mp_kernel_state_boot_tick(void *user);

static void _mp_kernel_state_kpanic_set(void *user);
static void _mp_kernel_state_kpanic_unset(void *user);
static void _mp_kernel_state_kpanic_tick(void *user);

void mp_kernel_init(mp_kernel_t *kernel, mp_kernel_onBoot_t onBoot, void *user) {

	/* initialize printk */
	mp_printk_unset();

	kernel->version = MP_KERNEL_VERSION;

	/* setup structure */
	kernel->onBoot = onBoot;
	kernel->onBootUser = user;

	/* initialize the machine */
	mp_machine_init(kernel);

	/* erase mem if necessary */
#ifdef SUPPORT_COMMON_MEM
	mp_mem_erase(kernel);
#endif

	/* init sensors */
#ifdef SUPPORT_COMMON_SENSOR
	mp_sensor_init(kernel);
#endif

	/* initialize tasks */
	mp_task_init(&kernel->tasks);

	/* initialize logical machine state */
	mp_state_init(&kernel->states);

	/* define KPANIC machine state */
	mp_state_define(
		&kernel->states,
		MP_KERNEL_KPANIC, "KPANIC", kernel,
		_mp_kernel_state_kpanic_set,
		_mp_kernel_state_kpanic_unset,
		_mp_kernel_state_kpanic_tick
	);

	/* define BOOT machine state */
	mp_state_define(
		&kernel->states,
		MP_KERNEL_BOOT, "BOOT", kernel,
		_mp_kernel_state_boot_set,
		_mp_kernel_state_boot_unset,
		_mp_kernel_state_boot_tick
	);

	mp_kernel_state(kernel, MP_KERNEL_BOOT);
}


void mp_kernel_fini(mp_kernel_t *kernel) {
	/* terminate logical machine state */
	mp_state_fini(&kernel->states);

	/* initialize tasks */
	mp_task_init(&kernel->tasks);

	/* terminate the machine */
	mp_machine_fini(kernel);
}


void mp_kernel_state(mp_kernel_t *kernel, char number) {
	mp_state_switch(&kernel->states, number);
}

void mp_kernel_loop(mp_kernel_t *kernel) {
	mp_task_tick_t taskRet;
	while(1) {
		/* execute tasks */
		taskRet = mp_task_tick(&kernel->tasks);
		if(taskRet == MP_TASK_WORKING) {
			/* execute machine state */
			mp_state_tick(&kernel->states);
		}
	}
}


void mp_kernel_panic(mp_kernel_t *kernel, int error) {


}

static void _mp_kernel_state_boot_set(void *user) { }

static void _mp_kernel_state_boot_unset(void *user) { }

static void _mp_kernel_state_boot_tick(void *user) {
	mp_kernel_t *kernel = user;
	if(kernel->onBoot)
		kernel->onBoot(kernel->onBootUser);
}


static void _mp_kernel_state_kpanic_set(void *user) {


}

static void _mp_kernel_state_kpanic_unset(void *user) {


}

static void _mp_kernel_state_kpanic_tick(void *user) {


}
