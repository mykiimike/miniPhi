#include <mp.h>


mp_ret_t mp_machine_init(mp_kernel_t *kernel) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

#if defined(__msp430x54xA)
	if ((*((char *)0x1A04) == 0x05) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5438A";
	else if ((*((char *)0x1A04) == 0x04) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5437A";
	else if ((*((char *)0x1A04) == 0x03) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5436A";
	else if ((*((char *)0x1A04) == 0x02) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5435A";
	else if ((*((char *)0x1A04) == 0x01) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5419A";
	else if ((*((char *)0x1A04) == 0x00) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5418A";

#elif defined(__msp430x54x)
	if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x38))
		kernel->mcuName = "MSP430F5438";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x37))
		kernel->mcuName = "MSP430F5437";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x36))
		kernel->mcuName = "MSP430F5436";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x35))
		kernel->mcuName = "MSP430F5435";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x19))
		kernel->mcuName = "MSP430F5419";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x18))
			kernel->mcuName = "MSP430F5418";
#endif

	__disable_interrupt();

	/* initialize interrupts */
	mp_interrupt_init();

	/* initialize GPIO */
	mp_gpio_init();

	/* intialize clock */
	mp_clock_init(kernel);

	/* set high energy */
	mp_clock_high_energy(kernel);

	/* intialize UART */
	mp_uart_init();

	__enable_interrupt();

	return(TRUE);
}

mp_ret_t mp_machine_fini(mp_kernel_t *kernel) {

	/* intialize UART */
	mp_uart_fini();

	/* terminate clock */
	mp_clock_fini(kernel);

	/* terminate GPIO */
	mp_gpio_fini();

	/* initialize interrupts */
	mp_interrupt_fini();

	return(TRUE);
}


//mp_machine_stop
//mp_machine_
