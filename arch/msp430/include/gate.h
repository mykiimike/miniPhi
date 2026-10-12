#ifndef _HAVE_MSP430_GATE_H
	#define _HAVE_MSP430_GATE_H

	typedef struct mp_gate_s mp_gate_t;

	struct mp_gate_s {
		/** Gate name */
		char *portDevice;

		/** gate user flags */
		unsigned char gateFlags;

		/** is uart gate busy ? */
		mp_bool_t isBusy;

		/** busy by who ? */
		char *byWho;

		/** internal: register control */
		unsigned int _baseAddress;

		/** internal: ISR vector */
		unsigned int _ISRVector;

		/** internal: register displacement */
		unsigned char _registersB;
	};

	void mp_gate_init(mp_kernel_t *kernel);
	void mp_gate_fini(mp_kernel_t *kernel);
	mp_gate_t *mp_gate_handle(char *id, char *who);
	void mp_gate_release(mp_gate_t *gate);
#endif
