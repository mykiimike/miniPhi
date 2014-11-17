#include <mp.h>

static void __dummy_int();

/* optimization in order to reduce ram usage */
//#if defined(__msp430x54x)
//	#define _MAX_INTERRUPTS 25
//	#define _FACTOR 40
//#else
	#define _MAX_INTERRUPTS 100
	#define _FACTOR 0
//#endif

/* 100 shall be enougth for all msp430 series */
mp_interrupt_t __interrupts[_MAX_INTERRUPTS];

mp_ret_t mp_interrupt_init() {
	mp_interrupt_t *inter;
	int a;
	for(a=0; a<_MAX_INTERRUPTS; a++) {
		inter = &__interrupts[a];
		inter->callback = __dummy_int;
	}
	return(TRUE);
}

mp_ret_t mp_interrupt_fini() {
	/* nothing */
	return(TRUE);
}

mp_interrupt_t *mp_interrupt_set(int inVector, mp_interrupt_cb_t in, void *user, char *who) {
	mp_interrupt_t *inter;
	volatile int vector;

	/* apply factor */
	vector = inVector-_FACTOR;

	__disable_interrupt();

	inter = &__interrupts[vector];

	if(inter->callback != __dummy_int) {
		__enable_interrupt();
		return(NULL); /* already used */
	}

	inter->callback = in;
	inter->user = user;
	inter->who = who;

	__enable_interrupt();

	/* nothing */
	return(inter);
}

mp_ret_t mp_interrupt_unset(int vector) {
	__disable_interrupt();
	__interrupts[vector].callback = __dummy_int;
	__interrupts[vector].user = NULL;
	__interrupts[vector].who = NULL;
	__enable_interrupt();
	return(TRUE);
}

static void __dummy_int() { }

#define _INSIDE_ISR(V) \
	__interrupt void V##_isr(void) { \
		mp_interrupt_t *__i = &__interrupts[V-_FACTOR]; \
		__i->callback(__i->user); \
	}

#ifdef USCI_B0_BASE
#pragma vector=USCI_B0_VECTOR
_INSIDE_ISR(USCI_B0_VECTOR);
#endif

#ifdef USCI_B1_BASE
#pragma vector=USCI_B1_VECTOR
_INSIDE_ISR(USCI_B1_VECTOR);
#endif

#ifdef USCI_B2_BASE
#pragma vector=USCI_B2_VECTOR
_INSIDE_ISR(USCI_B2_VECTOR);
#endif

#ifdef USCI_B3_BASE
#pragma vector=USCI_B3_VECTOR
_INSIDE_ISR(USCI_B3_VECTOR);
#endif

#ifdef USCI_A0_BASE
#pragma vector=USCI_A0_VECTOR
_INSIDE_ISR(USCI_A0_VECTOR);
#endif

#ifdef USCI_A1_BASE
#pragma vector=USCI_A1_VECTOR
_INSIDE_ISR(USCI_A1_VECTOR);
#endif

#ifdef USCI_A2_BASE
#pragma vector=USCI_A2_VECTOR
_INSIDE_ISR(USCI_A2_VECTOR);
#endif

//_INSIDE_ISR(USCI_A3_VECTOR);
//#ifdef USCI_A3_BASE
#pragma vector=USCI_A3_VECTOR
__interrupt void test_isr(void) {
	P10OUT ^= 0x40;
	UCA3RXBUF = 0;
}
//#endif

#ifdef PORT1_VECTOR
#pragma vector=PORT1_VECTOR
_INSIDE_ISR(PORT1_VECTOR);
#endif

#ifdef PORT2_VECTOR
#pragma vector=PORT2_VECTOR
_INSIDE_ISR(PORT2_VECTOR);
#endif

