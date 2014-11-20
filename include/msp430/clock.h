#ifndef _HAVE_MSP430_CLOCK_H
	#define _HAVE_MSP430_CLOCK_H

	#define MSP430_TICK_RATE_HZ ((unsigned int)1000)

	/* Auxilary clock */
	#define ACLK_FREQ_HZ ((unsigned int)32768)

	typedef struct mp_clock_freq_settings_s {
		unsigned char VCORE;
		unsigned int DCO;
	} mp_clock_freq_settings_t;

	typedef enum {
		MHZ8_t,
		MHZ16_t,
		MHZ20_t,
		MHZ25_t
	} mp_clock_t;

	mp_ret_t mp_clock_init(mp_kernel_t *kernel);
	mp_ret_t mp_clock_fini(mp_kernel_t *kernel);
	mp_ret_t mp_clock_low_energy();
	mp_ret_t mp_clock_high_energy();
	unsigned long mp_clock_ticks();
	unsigned long mp_clock_get_speed();
	static inline void mp_clock_delay(int delay) {
		while(delay--)
			__no_operation();
	}
#endif
