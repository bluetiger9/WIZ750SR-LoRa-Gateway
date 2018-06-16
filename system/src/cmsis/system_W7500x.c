/*******************************************************************************************************************************************************
 * Copyright ¡§I 2016 <WIZnet Co.,Ltd.>
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the ¢®¡ÆSoftware¢®¡Ÿ),
 * to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED ¢®¡ÆAS IS¢®¡Ÿ, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************************************************************************************************/
/**************************************************************************/
/**
 * @file    system_W7500x.c
 * @author  IOP Team
 * @version V1.0.0
 * @date    25-AUG-2015
 * @brief   CMSIS Cortex-M0 Core Peripheral Access Layer Source File for
 *          Device W7500x
 ******************************************************************************
 *
 * @attention
 * @par Revision history
 *    <2015/05/01> 1st Release
 *
 * <h2><center>&copy; COPYRIGHT 2015 WIZnet Co.,Ltd.</center></h2>
 ******************************************************************************
 */
#include "system_W7500x.h"


/*----------------------------------------------------------------------------
  DEFINES
 *----------------------------------------------------------------------------*/
//#define SYSCLK_EXTERN_OSC


/*----------------------------------------------------------------------------
  Clock Variable definitions
 *----------------------------------------------------------------------------*/
uint32_t SystemFrequency = 0;    /*!< System Clock Frequency (Core Clock)  */
uint32_t SourceFrequency = 0;    /*!< PLL Clock Source Frequency           */

/*----------------------------------------------------------------------------
  Clock functions
 *----------------------------------------------------------------------------*/

/* Get Core Clock Frequency       */
uint32_t GetSystemClock()
{
    return SystemFrequency;
}

/* Get PLL Source Clock Frequency */
uint32_t GetSourceClock()
{
    return SourceFrequency;
}

/*!< Get PLL Source Input; Internal or External */
uint32_t GetPLLSource(void)
{
    return CRG->PLL_IFSR;
}

/**
 * Initialize the system
 *
 * @param  none
 * @return none
 *
 * @brief  Setup the microcontroller system.
 *         Initialize the System.
 */
void SystemInit (void)
{
    uint8_t M,N,OD;

    (*((volatile uint32_t *)(W7500x_TRIM_BGT))) = (*((volatile uint32_t *)(W7500x_INFO_BGT)));
    (*((volatile uint32_t *)(W7500x_TRIM_OSC))) = (*((volatile uint32_t *)(W7500x_INFO_OSC)));

    // Set PLL input frequency
#ifdef SYSCLK_EXTERN_OSC
    CRG->PLL_IFSR = CRG_PLL_IFSR_OCLK;
#else
    CRG->PLL_IFSR = CRG_PLL_IFSR_RCLK;
#endif
    OD = (1 << (CRG->PLL_FCR & 0x01)) * (1 << ((CRG->PLL_FCR & 0x02) >> 1));
    N = (CRG->PLL_FCR >>  8 ) & 0x3F;
    M = (CRG->PLL_FCR >> 16) & 0x3F;

#ifdef SYSCLK_EXTERN_OSC
    SystemFrequency = EXTERN_XTAL * M / N * 1 / OD;
#else
    SystemFrequency = INTERN_XTAL * M / N * 1 / OD;
#endif
}

/**
 * Initialize the system for users custom
 *
 * @param  none
 * @return uint8_t OSC input selector, Internal or external
 *         uint32_t PLL clock source frequency (8MHz ~ 24MHz)
 *                  Internal 8MHz RC oscillator(RCLK) or External oscillator clock (OCLK, 8MHz ~ 24MHz)
 *         uint32_t Target System clock frequency (8MHz ~ 48MHz)
 *
 * @brief  Setup the microcontroller system.
 *         Initialize the System using parameters
 *         !! This function do not support clock divide !!
 */
//void SystemInit_User(uint32_t pll_ifsr_val, uint32_t xtal_clock, uint32_t pll_fcr_val)
void SystemInit_User(uint8_t osc_in_sel, uint32_t pll_src_clock, uint32_t system_clock)
{
	(*((volatile uint32_t *)(W7500x_TRIM_BGT))) = (*((volatile uint32_t *)(W7500x_INFO_BGT)));
	(*((volatile uint32_t *)(W7500x_TRIM_OSC))) = (*((volatile uint32_t *)(W7500x_INFO_OSC)));

	SystemCoreClockUpdate_User(osc_in_sel, pll_src_clock, system_clock);
}

/**
 * Re-Initialize the system clock for users custom
 *
 * @param  none
 * @return uint8_t OSC input selector, Internal or external
 *         uint32_t PLL clock source frequency (8MHz ~ 24MHz)
 *                  Internal 8MHz RC oscillator(RCLK) or External oscillator clock (OCLK, 8MHz ~ 24MHz)
 *         uint32_t Target System clock frequency (8MHz ~ 48MHz)
 *
 * @brief  Setup the microcontroller system.
 *         Re-Initialize the System using parameters
 *         !! This function do not support clock divide !!
 */
void SystemCoreClockUpdate_User(uint8_t osc_in_sel, uint32_t pll_src_clock, uint32_t system_clock)
{
	uint8_t M,N,OD;
	uint8_t mul;
	uint32_t PLL_FCR_DEFAULT = 0x00020100; // Initial CRG->PLL_FCR,
	uint32_t determined_pll_src;

	/* CRG Registers Setting */
	// Set PLL input frequency; Interal OSC (8MHz) or External OSC (8MHz ~ 24MHz)
	if(osc_in_sel == 1)
	{
		CRG->PLL_IFSR = CRG_PLL_IFSR_OCLK; // CLOCK_SOURCE_EXTERNAL
		determined_pll_src = pll_src_clock;
	}
	else
	{
		CRG->PLL_IFSR = CRG_PLL_IFSR_RCLK; // CLOCK_SOURCE_INTERNAL
		determined_pll_src = INTERN_XTAL;
	}

	// Calculate SystemFrequency
	mul = system_clock / determined_pll_src;

	if(mul <= 1) // Clock source bypass
	{
		CRG->PLL_BPR = CRG_PLL_BPR_EN; // PLL clock source bypass register:  Enabled
		CRG->PLL_FCR = PLL_FCR_DEFAULT;
		M = N = OD = 1;
	}
	else
	{
		CRG->PLL_BPR = CRG_PLL_BPR_DIS; // PLL clock source bypass register: Disabled

		PLL_FCR_DEFAULT &= (0xFFC0FFFF); // 'M' value clear
		PLL_FCR_DEFAULT |= ((mul & 0x3F) << 16);
		CRG->PLL_FCR = PLL_FCR_DEFAULT;

		OD = (1 << (CRG->PLL_FCR & 0x01)) * (1 << ((CRG->PLL_FCR & 0x02) >> 1));
		N = (CRG->PLL_FCR >>  8 ) & 0x3F;
		M = (CRG->PLL_FCR >> 16) & 0x3F;
	}

	// Update SystemFrequency
	SourceFrequency = determined_pll_src;
	SystemFrequency = SourceFrequency * M / N * 1 / OD;
}
