#include <mp.h>

static unsigned char _inc_VCORE(unsigned char Level);
static unsigned char _dec_VCORE(unsigned char Level);
static void _conf_VCORE(unsigned char Level);
static mp_bool_t _processor_type(void);
static void _system_clock(mp_clock_t freq);
static void __start_crystal(void);
static void _set_timer(void);

static unsigned long __ticks;
static mp_clock_freq_settings_t _mp_clock_freq_settings[] = {
	{PMMCOREV_0, 244}, /* MHZ8_t. */
	{PMMCOREV_1, 488}, /* MHZ16_t. */
	{PMMCOREV_2, 610}, /* MHZ20_t. */
	{PMMCOREV_3, 762} /* MHZ25_t. */
};
static mp_clock_t __frequency;

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	/* start crystal oscillo */
	__start_crystal();

	/* CPU voltage and freq */
	_system_clock(MHZ16_t);

	/* tick timer */
	_set_timer();

	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {

	return(TRUE);
}

mp_ret_t mp_clock_low_energy() {
	//_system_clock(MHZ8_t);
	_BIS_SR(LPM3_bits + GIE);
	return(TRUE);
}

mp_ret_t mp_clock_high_energy() {
	//_system_clock(MHZ16_t);
	_BIS_SR(LPM0_bits + GIE);
	return(TRUE);
}

unsigned long mp_clock_ticks() {
	return(__ticks);
}


void mp_clock_delay(int delay) {
	unsigned long local = __ticks+delay;
	while(__ticks < local);
}

unsigned long mp_clock_get_speed() {
	mp_clock_freq_settings_t *cpu_settings;
	cpu_settings = &_mp_clock_freq_settings[__frequency - MHZ8_t];
	return(cpu_settings->DCO*32768L);
}

/* The following function is a utility function the is used to */
/* increment the VCore setting to the specified value. */
static unsigned char _inc_VCORE(unsigned char Level) {
	unsigned char Result;
	unsigned char PMMRIE_backup;
	unsigned char SVSMHCTL_backup;
	unsigned char SVSMLCTL_backup;

	/* The code flow for increasing the Vcore has been altered to work */
	/* around the erratum FLASH37. Please refer to the Errata sheet to */
	/* know if a specific device is affected DO NOT ALTER THIS FUNCTION */
	/* Open PMM registers for write access. */
	PMMCTL0_H = 0xA5;

	/* Disable dedicated Interrupts and backup all registers. */
	PMMRIE_backup = PMMRIE;
	PMMRIE &= ~(SVMHVLRPE | SVSHPE | SVMLVLRPE | SVSLPE | SVMHVLRIE | SVMHIE | SVSMHDLYIE | SVMLVLRIE | SVMLIE | SVSMLDLYIE );
	SVSMHCTL_backup = SVSMHCTL;
	SVSMLCTL_backup = SVSMLCTL;

	/* Clear flags. */
	PMMIFG = 0;

	/* Set SVM highside to new level and check if a VCore increase is */
	/* possible. */
	SVSMHCTL = SVMHE | SVSHE | (SVSMHRRL0 * Level);

	/* Wait until SVM highside is settled. */
	while ((PMMIFG & SVSMHDLYIFG) == 0);

	/* Clear flag. */
	PMMIFG &= ~SVSMHDLYIFG;

	/* Check if a VCore increase is possible. */
	if((PMMIFG & SVMHIFG) == SVMHIFG) {
		/* Vcc is too low for a Vcore increase so we will recover the */

		/* previous settings */
		PMMIFG &= ~SVSMHDLYIFG;
		SVSMHCTL = SVSMHCTL_backup;

		/* Wait until SVM highside is settled. */
		while((PMMIFG & SVSMHDLYIFG) == 0);

		/* Return that the value was not set. */
		Result = 1;
	}
	else {
		/* Set also SVS highside to new level Vcc is high enough for a */
		/* Vcore increase */
		SVSMHCTL |= (SVSHRVL0 * Level);

		/* Wait until SVM highside is settled. */
		while ((PMMIFG & SVSMHDLYIFG) == 0);

		/* Clear flags. */
		PMMIFG &= ~SVSMHDLYIFG;

		/* Set VCore to new level. */
		PMMCTL0_L = PMMCOREV0 * Level;

		/* Set SVM, SVS low side to new level. */
		SVSMLCTL = SVMLE | (SVSMLRRL0 * Level) | SVSLE | (SVSLRVL0 * Level);

		/* Wait until SVM, SVS low side is settled. */
		while ((PMMIFG & SVSMLDLYIFG) == 0);

		/* Clear flag. */
		PMMIFG &= ~SVSMLDLYIFG;

		/* SVS, SVM core and high side are now set to protect for the new */
		/* core level. Restore Low side settings Clear all other bits */
		/* _except_ level settings */
		SVSMLCTL &= (SVSLRVL0+SVSLRVL1+SVSMLRRL0+SVSMLRRL1+SVSMLRRL2);

		/* Clear level settings in the backup register,keep all other */
		/* bits. */
		SVSMLCTL_backup &= ~(SVSLRVL0+SVSLRVL1+SVSMLRRL0+SVSMLRRL1+SVSMLRRL2);

		/* Restore low-side SVS monitor settings. */
		SVSMLCTL |= SVSMLCTL_backup;

		/* Restore High side settings. Clear all other bits except level */
		/* settings */
		SVSMHCTL &= (SVSHRVL0+SVSHRVL1+SVSMHRRL0+SVSMHRRL1+SVSMHRRL2);

		/* Clear level settings in the backup register,keep all other */
		/* bits. */
		SVSMHCTL_backup &= ~(SVSHRVL0+SVSHRVL1+SVSMHRRL0+SVSMHRRL1+SVSMHRRL2);

		/* Restore backup. */
		SVSMHCTL |= SVSMHCTL_backup;

		/* Wait until high side, low side settled. */
		while(((PMMIFG & SVSMLDLYIFG) == 0) && ((PMMIFG & SVSMHDLYIFG) == 0));

		/* Return that the value was set. */
		Result = 0;
	}

	/* Clear all Flags. */
	PMMIFG &= ~(SVMHVLRIFG | SVMHIFG | SVSMHDLYIFG | SVMLVLRIFG | SVMLIFG | SVSMLDLYIFG);

	/* Restore PMM interrupt enable register. */
	PMMRIE = PMMRIE_backup;

	/* Lock PMM registers for write access. */
	PMMCTL0_H = 0x00;
	return(Result);
}
/* The following function is a utility function the is used to */
/* decrement the VCore setting to the specified value. */
static unsigned char _dec_VCORE(unsigned char Level) {
	unsigned char Result;
	unsigned char PMMRIE_backup;
	unsigned char SVSMHCTL_backup;
	unsigned char SVSMLCTL_backup;

	/* The code flow for decreasing the Vcore has been altered to work */
	/* around the erratum FLASH37. Please refer to the Errata sheet to */
	/* know if a specific device is affected DO NOT ALTER THIS FUNCTION */
	/* Open PMM registers for write access. */
	PMMCTL0_H = 0xA5;

	/* Disable dedicated Interrupts Backup all registers */
	PMMRIE_backup = PMMRIE;
	PMMRIE &= ~(SVMHVLRPE | SVSHPE | SVMLVLRPE | SVSLPE | SVMHVLRIE | SVMHIE | SVSMHDLYIE | SVMLVLRIE | SVMLIE | SVSMLDLYIE );
	SVSMHCTL_backup = SVSMHCTL;
	SVSMLCTL_backup = SVSMLCTL;

	/* Clear flags. */
	PMMIFG &= ~(SVMHIFG | SVSMHDLYIFG | SVMLIFG | SVSMLDLYIFG);

	/* Set SVM, SVS high & low side to new settings in normal mode. */
	SVSMHCTL = SVMHE | (SVSMHRRL0 * Level) | SVSHE | (SVSHRVL0 * Level);
	SVSMLCTL = SVMLE | (SVSMLRRL0 * Level) | SVSLE | (SVSLRVL0 * Level);

	/* Wait until SVM high side and SVM low side is settled. */
	while (((PMMIFG & SVSMHDLYIFG) == 0) || ((PMMIFG & SVSMLDLYIFG) == 0));

	/* Clear flags. */
	PMMIFG &= ~(SVSMHDLYIFG + SVSMLDLYIFG);

	/* SVS, SVM core and high side are now set to protect for the new */
	/* core level. */
	/* Set VCore to new level. */
	PMMCTL0_L = PMMCOREV0 * Level;

	/* Restore Low side settings Clear all other bits _except_ level */
	/* settings */
	SVSMLCTL &= (SVSLRVL0+SVSLRVL1+SVSMLRRL0+SVSMLRRL1+SVSMLRRL2);

	/* Clear level settings in the backup register,keep all other bits. */
	SVSMLCTL_backup &= ~(SVSLRVL0+SVSLRVL1+SVSMLRRL0+SVSMLRRL1+SVSMLRRL2);

	/* Restore low-side SVS monitor settings. */
	SVSMLCTL |= SVSMLCTL_backup;

	/* Restore High side settings Clear all other bits except level */
	/* settings */
	SVSMHCTL &= (SVSHRVL0+SVSHRVL1+SVSMHRRL0+SVSMHRRL1+SVSMHRRL2);

	/* Clear level settings in the backup register, keep all other bits. */
	SVSMHCTL_backup &= ~(SVSHRVL0+SVSHRVL1+SVSMHRRL0+SVSMHRRL1+SVSMHRRL2);

	/* Restore backup. */
	SVSMHCTL |= SVSMHCTL_backup;

	/* Wait until high side, low side settled. */
	while (((PMMIFG & SVSMLDLYIFG) == 0) && ((PMMIFG & SVSMHDLYIFG) == 0));

	/* Clear all Flags. */
	PMMIFG &= ~(SVMHVLRIFG | SVMHIFG | SVSMHDLYIFG | SVMLVLRIFG | SVMLIFG | SVSMLDLYIFG);

	/* Restore PMM interrupt enable register. */
	PMMRIE = PMMRIE_backup;

	/* Lock PMM registers for write access. */
	PMMCTL0_H = 0x00;

	/* Return success to the caller. */
	Result = 0;

	return(Result);
}
/* The following function is responsible for setting the PMM core */
/* voltage to the specified level. */
static void _conf_VCORE(unsigned char Level) {
	unsigned int ActualLevel;
	unsigned int Status;

	/* Set Mask for Max. level. */
	Level &= PMMCOREV_3;

	/* Get actual VCore. */
	ActualLevel = (PMMCTL0 & PMMCOREV_3);

	/* Step by step increase or decrease the VCore setting. */
	Status = 0;
	while (((Level != ActualLevel) && (Status == 0)) || (Level < ActualLevel)) {
	if (Level > ActualLevel)
		Status = _inc_VCORE(++ActualLevel);
	else
		Status = _dec_VCORE(--ActualLevel);
	}
}


static mp_bool_t _processor_type(void) {
	mp_bool_t ret_val = FALSE;
	/* Read the TLV descriptors to determine the device type. */
	if ((*((char *)0x1A04) == 0x05) && (*((char *)0x1A05) == 0x80))
		ret_val = TRUE;
	return(ret_val);
}

/* The following function is responsible for setting up the system */
/* clock at a specified freq. */
static void _system_clock(mp_clock_t freq) {
	int                       UseDCO;
	unsigned int                    Ratio;
	unsigned int                    DCODivBits;
	unsigned long                   SystemFrequency;
	volatile unsigned int           Counter;


	mp_clock_freq_settings_t *cpu_settings;

	/** \todo Check for erratum flash28 */

	/* Verify that the CPU freq enumerated type is valid, if it is */
	/* not then we will force it to a default. */
	if((freq != MHZ8_t) && (freq != MHZ16_t) && (freq != MHZ20_t) && (freq != MHZ25_t))
		freq = MHZ16_t;

	/* Do not allow improper settings (MSP430F5438 cannot run at 20MHz or*/
	/* 25 MHz). */
	if((!_processor_type()) && ((freq == MHZ20_t) || (freq == MHZ25_t)))
		freq = MHZ16_t;

	/* Get the CPU settings for the specified freq. */
	cpu_settings = &_mp_clock_freq_settings[freq - MHZ8_t];

	/* set global frequency */
	__frequency = freq;

	/* Configure the PMM core voltage. */
	//_conf_VCORE(cpu_settings->VCORE);

	/* Get the ratio of the system frequency to the source clock.        */
	Ratio           = cpu_settings->DCO;

	/* Use a divider of at least 2 in the FLL control loop.              */
	DCODivBits      = FLLD__2;

	/* Get the system frequency that is configured.                      */
	SystemFrequency  = mp_clock_get_speed();
	SystemFrequency /= 1000;

	/* If the requested frequency is above 16MHz we will use DCO as the  */
	/* source of the system clocks, otherwise we will use DCOCLKDIV.     */
	if(SystemFrequency > 16000)
	{
		Ratio  >>= 1;
		UseDCO   = TRUE;
	}
	else
	{
		SystemFrequency <<= 1;
		UseDCO            = FALSE;
	}

	/* While needed set next higher div level.                           */
	while (Ratio > 512)
	{
		DCODivBits   = DCODivBits + FLLD0;
		Ratio      >>= 1;
	}

	/* Disable the FLL.                                                  */
	__bis_SR_register(SCG0);

	/* Set DCO to lowest Tap.                                            */
	UCSCTL0 = 0x0000;

	/* Reset FN bits.                                                    */
	UCSCTL2 &= ~(0x03FF);
	UCSCTL2  = (DCODivBits | (Ratio - 1));

	UCSCTL1 = DCORSEL_5;

	/* Re-enable the FLL.                                                */
	__bic_SR_register(SCG0);

	/* Loop until the DCO is stabilized.                                 */
	while(UCSCTL7 & DCOFFG)
	{
		/* Clear DCO Fault Flag.                                         */
		UCSCTL7 &= ~DCOFFG;

		/* Clear OFIFG fault flag.                                       */
		SFRIFG1 &= ~OFIFG;
	}

	/* Enable the FLL control loop.                                      */
	__bic_SR_register(SCG0);

	/* Based on the frequency we will use either DCO or DCOCLKDIV as the */
	/* source of MCLK and SMCLK.                                         */
	if (UseDCO)
	{
		/* Select DCOCLK for MCLK and SMCLK.                              */
		UCSCTL4 &=  ~(SELM_7 | SELS_7);
		UCSCTL4 |= (SELM__DCOCLK | SELS__DCOCLK);
	}
	else
	{
		/* Select DCOCLKDIV for MCLK and SMCLK.                           */
		UCSCTL4 &=  ~(SELM_7 | SELS_7);
//		UCSCTL4 |= (SELM__DCOCLKDIV | SELS__DCOCLKDIV);
		UCSCTL4 = SELA__XT1CLK | SELS__DCOCLKDIV  |  SELM__DCOCLKDIV ;

	}

	/* Delay the appropriate amount of cycles for the clock to settle.   */
	Counter = Ratio * 32;
	while (Counter--)
		__delay_cycles(30);


	return;
}

/* The following function is responsible for starting XT1 in the */
/* MSP430 that is used to source the internal FLL that drives the */
/* MCLK and SMCLK. */
static void __start_crystal(void) {

	  // Set up XT1 Pins to analog function, and to lowest drive
	  P7SEL |= 0x03;
	  UCSCTL6 |= XCAP_3 ;                       // Set internal cap values

	  while(SFRIFG1 & OFIFG) {                  // Check OFIFG fault flag
	    while ( (SFRIFG1 & OFIFG))              // Check OFIFG fault flag
	    {
	      // Clear OSC fault flags
	      UCSCTL7 &= ~(DCOFFG + XT1LFOFFG + XT1HFOFFG + XT2OFFG);
	      SFRIFG1 &= ~OFIFG;                    // Clear OFIFG fault flag
	    }
	    UCSCTL6 &= ~(XT1DRIVE1_L+XT1DRIVE0);    // Reduce the drive strength
	  }

}


/* This function is called to configure the System Timer, i.e TA1. */
/* This timer is used for all system time scheduling. */
static void _set_timer(void) {
	/* Ensure the timer is stopped. */
	TA1CTL = 0;
	/* Run the timer off of the ACLK. */
	TA1CTL = TASSEL_1 | ID_0;
	/* Clear everything to start with. */
	TA1CTL |= TACLR;
	/* Set the compare match value according to the tick rate we want. */
	TA1CCR0 = ( ACLK_FREQ_HZ / MSP430_TICK_RATE_HZ ) + 1;
	/* Enable the interrupts. */
	TA1CCTL0 = CCIE;
	/* Start up clean. */
	TA1CTL |= TACLR;
	/* Up mode. */
	TA1CTL |= TASSEL_1 | MC_1 | ID_0;
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void TIMER1_A0(void) {
	__ticks++;
	LPM3_EXIT;
}


