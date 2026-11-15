#ifndef _HAVE_MP_TASK_CPUS_H
	#define _HAVE_MP_TASK_CPUS_H

	/* Arch code that cannot include mp.h includes this after config.h.
	 * task.h includes it too, so the rule lives here. */

	#ifndef MP_TASK_SCHED_PREEMPT
		#define MP_TASK_SCHED_COOP     0
		#define MP_TASK_SCHED_PRIORITY 1
		#define MP_TASK_SCHED_PREEMPT  2
	#endif
	#ifndef MP_TASK_SCHEDULER
		#define MP_TASK_SCHEDULER 0
	#endif

	/* Same-priority sharing needs a line under the tick and a stack switch.
	 * The arch sets MP_ARCH_TASK_SHARE when it has that line. Priority 0
	 * stays on the caller stack. A task above that gets its own stack and
	 * yields every tick. */
	#ifndef MP_ARCH_TASK_SHARE
		#define MP_ARCH_TASK_SHARE 0
	#endif
	#ifndef MP_TASK_SHARE
		#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT && MP_ARCH_TASK_SHARE
			#define MP_TASK_SHARE 1
		#else
			#define MP_TASK_SHARE 0
		#endif
	#endif

	/* Cores sharing one task list. The arch sets MP_ARCH_CPUS. Under the
	 * preemptive scheduler those cores share it: core 0 keeps the main
	 * loop, the tick, the power governor and the private stacks. Every
	 * other core runs callbacks to completion. -DMP_CPUS overrides this. */
	#ifndef MP_ARCH_CPUS
		#define MP_ARCH_CPUS 1
	#endif
	#ifndef MP_CPUS
		#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
			#define MP_CPUS MP_ARCH_CPUS
		#else
			#define MP_CPUS 1
		#endif
	#endif

#endif
