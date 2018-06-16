#include "W7500x.h"
#include "W7500x_miim.h"
#include "W7500x_gpio.h"

#include "common.h"
#include "W7500x_board.h"

#include "timerHandler.h"
#include "uartHandler.h"

#ifdef __USE_EXT_EEPROM__
	#include "eepromHandler.h"
#endif

#include <stdio.h>

extern void delay(__IO uint32_t nCount);

static void PHY_Init(void);
//static void delay_ms(uint32_t ms);

GPIO_TypeDef* LED_PORT[LEDn] = {LED1_GPIO_PORT, LED2_GPIO_PORT};
const uint16_t LED_PIN[LEDn] = {LED1_PIN, LED2_PIN};
PAD_Type LED_PAD[LEDn] = {LED1_GPIO_PAD, LED2_GPIO_PAD};
PAD_AF_TypeDef LED_PAD_AF[LEDn] = {LED1_GPIO_PAD_AF, LED2_GPIO_PAD_AF};

volatile uint16_t phylink_check_time_msec = 0;
uint8_t flag_check_phylink = 0;
uint8_t flag_hw_trig_enable = 0;

/* W7500x Board Initialization */
void W7500x_Board_Init(void)
{
	/* PHY Initialization */
	PHY_Init();
	
	/* HW_TRIG input pin - Check this Pin only once at boot (switch) */
	init_hw_trig_pin();
	flag_hw_trig_enable = (get_hw_trig_pin()?0:1);
	
	/* PHY link input pin */
#if ((DEVICE_BOARD_NAME == WIZ750SR) || (DEVICE_BOARD_NAME == W7500P_S2E) || (DEVICE_BOARD_NAME == WIZ750SR_1xx))
	init_phylink_in_pin();
#endif
	
#ifdef __USE_EXT_EEPROM__
	init_eeprom();
#endif
	
#ifdef __USE_BOOT_ENTRY__
	init_boot_entry_pin();
#endif
	// STATUS #1 : PHY link status (LED1)
	// STATUS #2 : TCP connection status (LED2)
	LED_Init(LED1);
	LED_Init(LED2);
}

void Supervisory_IC_Init(void)
{
	*(volatile uint32_t *)(0x41001170) = 0x0002; // 8MHz output (125n)
	*(volatile uint32_t *)(0x41002008) = 0x0002; // PAD PA_02 use as CLKOUT
}

static void PHY_Init(void)
{
	set_phylink_time_check(1); // start PHY link time checker
	
#ifdef __DEF_USED_IC101AG__ // For using W7500 + (IC+101AG Phy)
	*(volatile uint32_t *)(0x41003068) = 0x64; //TXD0 - set PAD strengh and pull-up
	*(volatile uint32_t *)(0x4100306C) = 0x64; //TXD1 - set PAD strengh and pull-up
	*(volatile uint32_t *)(0x41003070) = 0x64; //TXD2 - set PAD strengh and pull-up
	*(volatile uint32_t *)(0x41003074) = 0x64; //TXD3 - set PAD strengh and pull-up
	*(volatile uint32_t *)(0x41003050) = 0x64; //TXE  - set PAD strengh and pull-up
	
	//printf("\r\n[MCU: W7500]\r\n");
#endif

#ifdef __W7500P__ // W7500P
	// PB_05, PB_12 pull down
	*(volatile uint32_t *)(0x41003070) = 0x61; // RXDV - set pull down (PB_12)
	*(volatile uint32_t *)(0x41003054) = 0x61; // COL  - set pull down (PB_05)
#endif

#ifdef __DEF_USED_MDIO__ 
	/* mdio Init */
	mdio_init(GPIOB, W7500x_MDC, W7500x_MDIO);
	//mdio_write(GPIOB, PHYREG_CONTROL, CNTL_RESET); // PHY Reset
	
	#ifdef __W7500P__ // W7500P
		//set_link(FullDuplex10);
		//set_link(HalfDuplex10);
		//set_link(FullDuplex100);
		//set_link(HalfDuplex100);
		//set_link(AUTONEGO);
	#endif
	
	// ## for debugging
	//printf("\r\nPHYADDR = %.3x, PHYREGADDR = %x, VAL = 0x%.4x\r\n", PHY_ADDR, 0, mdio_read(GPIOB, 0)); // [RW] Control, default: 0x3100 / 0011 0001 0000 0000b
	//printf("PHYADDR = %.3x, PHYREGADDR = %x, VAL = 0x%.4x\r\n", PHY_ADDR, 1, mdio_read(GPIOB, 1)); // [RO] Status,  default: 0x786d / 0111 1000 0110 1101b (link up)
	//printf("PHYADDR = %.3x, PHYREGADDR = %x, VAL = 0x%.4x\r\n", PHY_ADDR, 2, mdio_read(GPIOB, 2)); // [RO] OUI,     default: 0x001c
#endif

	set_phylink_time_check(0); // start PHY link time checker
}


// Status pins, active low
void init_phylink_in_pin(void)
{
	GPIO_Configuration(PHYLINK_IN_PORT, PHYLINK_IN_PIN, GPIO_Mode_IN, PHYLINK_IN_PAD_AF);
}


uint8_t get_phylink_in_pin(void)
{
	// PHYlink input; Active low
	return GPIO_ReadInputDataBit(PHYLINK_IN_PORT, PHYLINK_IN_PIN);
}

// Hardware mode switch pin, active low
void init_hw_trig_pin(void)
{
	GPIO_Configuration(HW_TRIG_PORT, HW_TRIG_PIN, GPIO_Mode_IN, HW_TRIG_PAD_AF);
	HW_TRIG_PORT->OUTENCLR = HW_TRIG_PIN;
}


uint8_t get_hw_trig_pin(void)
{
	// HW_TRIG input; Active low
	uint8_t hw_trig, i;
	for(i = 0; i < 5; i++)
	{
		hw_trig = GPIO_ReadInputDataBit(HW_TRIG_PORT, HW_TRIG_PIN);
		if(hw_trig != 0) return 1; // High
		delay(5);
	}
	return 0; // Low
}


void init_uart_if_sel_pin(void)
{
	#ifdef __USE_UART_IF_SELECTOR__
		GPIO_Configuration(UART_IF_SEL_PORT, UART_IF_SEL_PIN, GPIO_Mode_IN, UART_IF_SEL_PAD_AF);
	#else
		;
	#endif
}

uint8_t get_uart_if_sel_pin(void)
{
	// Status of UART interface selector pin input; [0] RS-232/TTL mode, [1] RS-422/485 mode
	#ifdef __USE_UART_IF_SELECTOR__
		return GPIO_ReadInputDataBit(UART_IF_SEL_PORT, UART_IF_SEL_PIN);
	#else
		return UART_IF_DEFAULT;
	#endif
}


#ifdef __USE_BOOT_ENTRY__
// Application boot mode entry pin, active low
void init_boot_entry_pin(void)
{
	GPIO_Configuration(BOOT_ENTRY_PORT, BOOT_ENTRY_PIN, GPIO_Mode_IN, BOOT_ENTRY_PAD_AF);
	BOOT_ENTRY_PORT->OUTENCLR = BOOT_ENTRY_PIN; // set to high
}

uint8_t get_boot_entry_pin(void)
{
	// Get the status of application boot mode entry pin, active low
	return GPIO_ReadInputDataBit(BOOT_ENTRY_PORT, BOOT_ENTRY_PIN);
}
#endif


/**
  * @brief  Configures LED GPIO.
  * @param  Led: Specifies the Led to be configured.
  *   This parameter can be one of following parameters:
  *     @arg WIZWIKI-W7500 board: LED1(LED_R) / LED2(LED_G) / Not used(LED_B)
  *     @arg WIZWIKI-W7500ECO board: LED1 / LED2
  * @retval None
  */
void LED_Init(Led_TypeDef Led)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	if(Led >= LEDn) return;
	
	/* Configure the GPIO_LED pin */
	GPIO_InitStructure.GPIO_Pin = LED_PIN[Led];
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_Init(LED_PORT[Led], &GPIO_InitStructure);
	PAD_AFConfig(LED_PAD[Led], LED_PIN[Led], LED_PAD_AF[Led]);
	
	/* LED off */
	GPIO_SetBits(LED_PORT[Led], LED_PIN[Led]);
}

/**
  * @brief  Turns selected LED On.
  * @param  Led: Specifies the Led to be set on.
  *   This parameter can be one of following parameters:
  *     @arg WIZWIKI-W7500 board: LED_R / LED_G / LED_B
  *     @arg WIZWIKI-W7500ECO board: LED1 / LED2
  * @retval None
  */
void LED_On(Led_TypeDef Led)
{
	if(Led >= LEDn) return;
	GPIO_ResetBits(LED_PORT[Led], LED_PIN[Led]);
}

/**
  * @brief  Turns selected LED Off.
  * @param  Led: Specifies the Led to be set off.
  *   This parameter can be one of following parameters:
  *     @arg WIZWIKI-W7500 board: LED_R / LED_G / LED_B
  *     @arg WIZWIKI-W7500ECO board: LED1 / LED2
  * @retval None
  */
void LED_Off(Led_TypeDef Led)
{
	if(Led >= LEDn) return;
	GPIO_SetBits(LED_PORT[Led], LED_PIN[Led]);
}

/**
  * @brief  Toggles the selected LED.
  * @param  Led: Specifies the Led to be toggled.
  *   This parameter can be one of following parameters:
  *     @arg WIZWIKI-W7500 board: LED_R / LED_G / LED_B
  *     @arg WIZWIKI-W7500ECO board: LED1 / LED2
  * @retval None
  */
void LED_Toggle(Led_TypeDef Led)
{
	if(Led >= LEDn) return;
	LED_PORT[Led]->DATAOUT ^= LED_PIN[Led];
}

/**
  * @brief  Get the selected LED status
  * @param  Led: Specifies the Led.
  *   This parameter can be one of following parameters:
  *     @arg WIZWIKI-W7500 board: LED_R / LED_G / LED_B
  *     @arg WIZWIKI-W7500ECO board: LED1 / LED2
  * @retval Status of LED (on / off)
  */
uint8_t get_LED_Status(Led_TypeDef Led)
{
	return GPIO_ReadOutputDataBit(LED_PORT[Led], LED_PIN[Led]);
}

/**
  * @brief  Inserts a delay time when the situation cannot use the timer interrupt.
  * @param  ms: specifies the delay time length, in milliseconds.
  * @retval None
  */
/*
void delay(__IO uint32_t milliseconds)
{
	volatile uint32_t nCount;
	
	nCount=(GetSystemClock()/10000)*milliseconds;
	for (; nCount!=0; nCount--);
}
*/

/*
static void delay_ms(uint32_t ms)
{
	volatile uint32_t nCount;
	
	nCount=(GetSystemClock()/10000)*ms;
	for (; nCount!=0; nCount--);
}
*/
