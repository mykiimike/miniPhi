/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
 * Copyright (C) 2014  VERMAN                                              *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software Foundation, *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

static unsigned char _inc_VCORE(unsigned char level);
static unsigned char _dec_VCORE(unsigned char level);
static void _conf_VCORE(unsigned char level);
static mp_bool_t _processor_type(void);
static void _system_clock(mp_clock_t freq);
static void __start_crystal(void);
static void _set_timer(void);

/* erratum FLASH28 */
#ifdef __msp430x54x
	#define _VCORE_1MHZ   PMMCOREV_2
	#define _VCORE_4MHZ   PMMCOREV_2
	#define _VCORE_8MHZ   PMMCOREV_2
	#define _VCORE_12MHZ  PMMCOREV_2
	#define _VCORE_16MHZ  PMMCOREV_2
	#define _VCORE_20MHZ  PMMCOREV_2
	#define _VCORE_25MHZ  PMMCOREV_2
#else
	#define _VCORE_1MHZ    PMMCOREV_0
	#define _VCORE_4MHZ    PMMCOREV_0
	#define _VCORE_8MHZ    PMMCOREV_0
	#define _VCORE_12MHZ   PMMCOREV_0
	#define _VCORE_16MHZ   PMMCOREV_1
	#define _VCORE_20MHZ   PMMCOREV_2
	#define _VCORE_25MHZ   PMMCOREV_3
#endif

#pragma DATA_SECTION(__ticks, ".sysmem")
static unsigned long __ticks;

static const mp_clock_freq_settings_t _mp_clock_freq_settings[] = {
	{MHZ1_t, DCORSEL_2, _VCORE_1MHZ, 30},  /* MHZ1_t. */
	{MHZ4_t, DCORSEL_4, _VCORE_4MHZ, 122}, /* MHZ4_t. */
	{MHZ8_t, DCORSEL_4, _VCORE_8MHZ, 244}, /* MHZ8_t. */
	{MHZ12_t, DCORSEL_5, _VCORE_12MHZ, 366}, /* MHZ12_t. */
	{MHZ16_t, DCORSEL_5, _VCORE_16MHZ, 488}, /* MHZ16_t. */
#if !defined(__msp430x54x) && !defined(__msp430x54xA)
	{MHZ20_t, DCORSEL_6, _VCORE_20MHZ, 610}, /* MHZ20_t. */
	{MHZ25_t, DCORSEL_7, _VCORE_25MHZ, 763}  /* MHZ25_t. */
#endif

};

static const char *_mp_clock_freq_name[] = {
	{"1Mhz"},
	{"4Mhz"},
	{"8Mhz"},
	{"12Mhz"},
	{"16Mhz"},
#if !defined(__msp430x54x) && !defined(__msp430x54xA)
	{"20Mhz"},
	{"25Mhz"},
#endif
};


static mp_clock_t __frequency;

/**
@defgroup mpArchTiMSP430F5xx6xxClock The clock system

@ingroup mpArchTiMSP430

@brief The clock system for Ti MSP430 F5xx/F6xx

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

The clock system for Ti MSP430 F5xx/F6xx is based on Ti UCS (Unified Clock System).

The UCS module supports low system cost and ultralow power consumption. Using three internal clock
signals, the user can select the best balance of performance and low power consumption. The UCS
module can be configured to operate without any external components, with one or two external crystals,
or with resonators, under full software control.

The UCS module includes up to five clock sources:
@li XT1CLK: Low-frequency or high-frequency oscillator that can be used either with low-frequency 32768
Hz watch crystals, standard crystals, resonators, or external clock sources in the 4 MHz to 32 MHz
range. XT1CLK can be used as a clock reference into the FLL. Some devices only support the low
frequency oscillator for XT1CLK. See the device-specific data sheet for supported functions.
@li VLOCLK: Internal very low power, low frequency oscillator with 10 kHz typical frequency
@li REFOCLK: Internal, trimmed, low-frequency oscillator with 32768 Hz typical frequency, with the ability
to be used as a clock reference into the FLL
@li DCOCLK: Internal digitally-controlled oscillator (DCO) that can be stabilized by the FLL
@li XT2CLK: Optional high-frequency oscillator that can be used with standard crystals, resonators, or
external clock sources in the 4 MHz to 32 MHz range. XT2CLK can be used as a clock reference into
the FLL.

Three clock signals are available from the UCS module:
@li ACLK: Auxiliary clock. The ACLK is software selectable as XT1CLK, REFOCLK, VLOCLK, DCOCLK,
DCOCLKDIV, and when available, XT2CLK. DCOCLKDIV is the DCOCLK frequency divided by 1, 2, 4,
8, 16, or 32 within the FLL block. ACLK can be divided by 1, 2, 4, 8, 16, or 32. ACLK/n is ACLK
divided by 1, 2, 4, 8, 16, or 32 and is available externally at a pin. ACLK is software selectable by
individual peripheral modules.
@li MCLK: Master clock. MCLK is software selectable as XT1CLK, REFOCLK, VLOCLK, DCOCLK,
DCOCLKDIV, and when available, XT2CLK. DCOCLKDIV is the DCOCLK frequency divided by 1, 2, 4,
8, 16, or 32 within the FLL block. MCLK can be divided by 1, 2, 4, 8, 16, or 32. MCLK is used by the
CPU and system.
@li SMCLK: Subsystem master clock. SMCLK is software selectable as XT1CLK, REFOCLK, VLOCLK,
DCOCLK, DCOCLKDIV, and when available, XT2CLK. DCOCLKDIV is the DCOCLK frequency divided
by 1, 2, 4, 8, 16, or 32 within the FLL block. SMCLK can be divided by 1, 2, 4, 8, 16, or 32. SMCLK is
software selectable by individual peripheral modules.

Depending the maximum CPU frequency miniPhi will choose the best way to drive clocks on the device.

@li ACLK is driven by XT1 at anytime fixed to 32Khz
@li MCLK could be driven by XT1 or XT2 depending if MP_CLOCK_XT2_DRIVE is set and the kernel is running
in high power mode. At full speed the MCLK is driven by XT2 is possible.
@li SCLK could be driven by XT1 or XT2

You can specify the frequency of each oscillator by setting MP_CLOCK_XT1_FREQ and MP_CLOCK_XT2_FREQ

@{

*/
mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	/* start crystal oscillo */
	__start_crystal();

	/* CPU voltage and freq */
	_system_clock(MP_CLOCK_LE_FREQ);

	/* tick timer */
	_set_timer();

	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {

	return(TRUE);
}

mp_ret_t mp_clock_low_energy() {
	_system_clock(MP_CLOCK_LE_FREQ);
	_BIS_SR(LPM3_bits + GIE);
	return(TRUE);
}

mp_ret_t mp_clock_high_energy() {
	_system_clock(MP_CLOCK_HE_FREQ);
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
	const mp_clock_freq_settings_t *cpu_settings;
	cpu_settings = &_mp_clock_freq_settings[__frequency - MHZ8_t];
	return(cpu_settings->DCO*32768L);
}

const char *mp_clock_name(mp_clock_t clock) {
	return(_mp_clock_freq_name[clock]);
}

/** @} */

/* The following function is a utility function the is used to */
/* increment the VCore setting to the specified value. */
static unsigned char _inc_VCORE(unsigned char level) {
	  // Open PMM module registers for write access
	  PMMCTL0_H = 0xA5;

	  // Set SVS/M high side to new level
	  SVSMHCTL = (SVSMHCTL & ~(SVSHRVL0*3 + SVSMHRRL0)) | \
	             (SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level);

	  // Set SVM new Level
	  SVSMLCTL = SVSLE + SVMLE + SVSMLRRL0 * level;
	  // Set SVS/M low side to new level
	  SVSMLCTL = (SVSMLCTL & ~(SVSMLRRL_3)) | (SVMLE + SVSMLRRL0 * level);

	  while ((PMMIFG & SVSMLDLYIFG) == 0);      // Wait till SVM is settled (Delay)
	  PMMCTL0_L = PMMCOREV0 * level;            // Set VCore to x
	  PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);        // Clear already set flags

	  if ((PMMIFG & SVMLIFG))
	    while ((PMMIFG & SVMLVLRIFG) == 0);     // Wait till level is reached

	  // Set SVS/M Low side to new level
	  SVSMLCTL = (SVSMLCTL & ~(SVSLRVL0*3 + SVSMLRRL_3)) | \
	             (SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level);

	  // Lock PMM module registers from write access
	  PMMCTL0_H = 0x00;

	  return(0);
}

/* The following function is a utility function the is used to */
/* decrement the VCore setting to the specified value. */
static unsigned char _dec_VCORE(unsigned char level) {
	  // Open PMM module registers for write access
	  PMMCTL0_H = 0xA5;

	  // Set SVS/M low side to new level
	  SVSMLCTL = (SVSMLCTL & ~(SVSLRVL0*3 + SVSMLRRL_3)) | \
	             (SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level);

	  while ((PMMIFG & SVSMLDLYIFG) == 0);      // Wait till SVM is settled (Delay)
	  PMMCTL0_L = (level * PMMCOREV0);          // Set VCore to new level
	  // Lock PMM module registers for write access

	  PMMCTL0_H = 0x00;

	  return(0);
}

/* The following function is responsible for setting the PMM core */
/* voltage to the specified level. */
static void _conf_VCORE(unsigned char level) {
	unsigned int current;

	current = PMMCTL0 & PMMCOREV_3;

	while (level != current) {
		if (level > current)
			_inc_VCORE(++current);
		else
			_dec_VCORE(--current);
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
	int UseDCO;
	unsigned int Ratio;
	unsigned int DCODivBits;
	unsigned long SystemFrequency;
	volatile unsigned int Counter;
	const mp_clock_freq_settings_t *cpu_settings;

	/* Verify that the CPU freq enumerated type is valid, if it is */
	/* not then we will force it to a default. */
	if((freq != MHZ8_t) && (freq != MHZ16_t) && (freq != MHZ20_t) && (freq != MHZ25_t))
		freq = MHZ16_t;

	/* Do not allow improper settings (MSP430F5438 cannot run at 20MHz or*/
	/* 25 MHz). */
	if((!_processor_type()) && ((freq == MHZ20_t) || (freq == MHZ25_t)))
		freq = MHZ16_t;

	/** Check for erratum flash28 */
#ifdef __msp430x54x
	freq = MHZ16_t;
#endif

	/* Get the CPU settings for the specified freq. */
	cpu_settings = &_mp_clock_freq_settings[freq];

	/* set global frequency */
	__frequency = freq;

	/* Configure the PMM core voltage. */
	_conf_VCORE(cpu_settings->VCORE);

	/* Get the ratio of the system frequency to the source clock.        */
	Ratio = cpu_settings->DCO;

	/* Use a divider of at least 2 in the FLL control loop.              */
	DCODivBits = FLLD__2;

	/* Get the system frequency that is configured.                      */
	SystemFrequency  = mp_clock_get_speed();
	SystemFrequency /= 1000;

	/* If the requested frequency is above 16MHz we will use DCO as the  */
	/* source of the system clocks, otherwise we will use DCOCLKDIV.     */
	if(SystemFrequency > 16000) {
		Ratio >>= 1;
		UseDCO = TRUE;
	}
	else {
		SystemFrequency <<= 1;
		UseDCO = FALSE;
	}

	/* While needed set next higher div level.                           */
	while (Ratio > 512) {
		DCODivBits = DCODivBits + FLLD0;
		Ratio >>= 1;
	}

	/* Disable the FLL.                                                  */
	__bis_SR_register(SCG0);

	/* Set DCO to lowest Tap.                                            */
	UCSCTL0 = 0x0000;

	/* Reset FN bits.                                                    */
	UCSCTL2 &= ~(0x03FF);

	/* Set ratio */
	UCSCTL2  = (DCODivBits | (Ratio - 1));

	/* Select DCO */
	UCSCTL1 = cpu_settings->DCORSEL;

	/* Re-enable the FLL.                                                */
	__bic_SR_register(SCG0);

	/* Loop until the DCO is stabilized.                                 */
	while(UCSCTL7 & DCOFFG) {
		/* Clear DCO Fault Flag.                                         */
		UCSCTL7 &= ~DCOFFG;

		/* Clear OFIFG fault flag.                                       */
		SFRIFG1 &= ~OFIFG;
	}

	/* Enable the FLL control loop.                                      */
	__bic_SR_register(SCG0);

	/* Based on the frequency we will use either DCO or DCOCLKDIV as the */
	/* source of MCLK and SMCLK.                                         */
	if (UseDCO) {
		/* Select DCOCLK for MCLK and SMCLK.                              */
		UCSCTL4 &=  ~(SELM_7 | SELS_7);
		UCSCTL4 |= (SELM__DCOCLK | SELS__DCOCLK);
	}
	else {
		/* Select DCOCLKDIV for MCLK and SMCLK.                           */
		UCSCTL4 &=  ~(SELM_7 | SELS_7);
		UCSCTL4 |= (SELM__DCOCLKDIV | SELS__DCOCLKDIV);
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

	/* Set up XT1 Pins to analog function, and to lowest drive           */
	P7SEL   |= (BIT1 | BIT0);

	/* Set internal cap values.                                          */
	UCSCTL6 |= XCAP_3;

	/* Loop while the Oscillator Fault bit is set.                       */
	while(SFRIFG1 & OFIFG) {
		while (SFRIFG1 & OFIFG) {
			/* Clear OSC fault flags.                                       */
			UCSCTL7 &= ~(DCOFFG + XT1LFOFFG + XT1HFOFFG + XT2OFFG);
			SFRIFG1 &= ~OFIFG;
		}

		/* Reduce the drive strength.                                      */
		UCSCTL6 &= ~(XT1DRIVE1_L + XT1DRIVE0);
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

/* disable Watchdog at pre init in order to use correctly eabi */
int _system_pre_init(void) {
	WDTCTL = WDTPW + WDTHOLD;
	return(1);
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void TIMER1_A0(void) {
	__ticks++;
	LPM3_EXIT;
}


