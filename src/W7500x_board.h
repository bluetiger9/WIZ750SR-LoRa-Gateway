/*
*
@file		W7500x_board.h
@brief
*/

#ifndef __W7500X_BOARD_H__ 
#define __W7500X_BOARD_H__ 

#include <stdint.h>
#include "common.h"

////////////////////////////////
// Product Configurations     //
////////////////////////////////

/* Target Board Selector */
//#define DEVICE_BOARD_NAME	WIZwiki_W7500ECO
//#define DEVICE_BOARD_NAME	W7500P_S2E
#define DEVICE_BOARD_NAME	WIZ750SR
//#define DEVICE_BOARD_NAME	WIZ750SR_1xx
//#define DEVICE_BOARD_NAME	W7500_S2E

#ifdef DEVICE_BOARD_NAME
	#if (DEVICE_BOARD_NAME == WIZ750SR)
		#define __W7500P__
		#define __USE_UART_IF_SELECTOR__	// RS-232/TTL or RS-422/485 selector using UART IF selector pin
		//#define __USE_EXT_EEPROM__			// External EEPROM or Internal Data flash (DAT0/1)
		#define __USE_BOOT_ENTRY__			// Application boot mode entry pin activated
		//#define __USE_APPBACKUP_AREA__		// If this option activated, Application firmware area is consists of App (50kB) and App backup (50kB). If not, user's application can be 100kB size. (Does not use the backup area)
		#define __USE_GPIO_HARDWARE_FLOWCONTROL__
		#define __USE_USERS_GPIO__
		//#define __USE_DSR_DTR_DEFAULT__
        #define __USE_SAFE_SAVE__           // DevConfig Save verify function added
        #define __USE_APPBOOT_TCP_SEARCH__
		#define DEVICE_CLOCK_SELECT	         CLOCK_SOURCE_EXTERNAL
		#define DEVICE_PLL_SOURCE_CLOCK      PLL_SOURCE_12MHz
		#define DEVICE_TARGET_SYSTEM_CLOCK   SYSTEM_CLOCK_48MHz
		#define DEVICE_ID_DEFAULT            "WIZ750SR" // Device name
	#elif (DEVICE_BOARD_NAME == W7500P_S2E) // Chip product
		#define __W7500P__
		#define __USE_UART_IF_SELECTOR__	// RS-232/TTL or RS-422/485 selector using UART IF selector pin
		#define __USE_BOOT_ENTRY__			// Application boot mode entry pin activated
		//#define __USE_APPBACKUP_AREA__
		#define __USE_GPIO_HARDWARE_FLOWCONTROL__
		#define __USE_USERS_GPIO__
		//#define __USE_DSR_DTR_DEFAULT__
        #define __USE_SAFE_SAVE__           // DevConfig Save verify function added
        #define __USE_APPBOOT_TCP_SEARCH__
		#define DEVICE_CLOCK_SELECT	         CLOCK_SOURCE_EXTERNAL
		#define DEVICE_PLL_SOURCE_CLOCK      PLL_SOURCE_12MHz
		#define DEVICE_TARGET_SYSTEM_CLOCK   SYSTEM_CLOCK_48MHz
		#define DEVICE_ID_DEFAULT            "W7500P-S2E"
	#elif (DEVICE_BOARD_NAME == WIZ750SR_1xx)
		//#define __USE_UART_IF_SELECTOR__	// RS-232/TTL or RS-422/485 selector using UART IF selector pin
		//#define __USE_EXT_EEPROM__			// External EEPROM or Internal Data flash (DAT0/1)
		#define __USE_BOOT_ENTRY__			// Application boot mode entry pin activated
		//#define __USE_APPBACKUP_AREA__		// If this option activated, Application firmware area is consists of App (50kB) and App backup (50kB). If not, user's application can be 100kB size. (Does not use the backup area)
		#define __USE_GPIO_HARDWARE_FLOWCONTROL__
		#define __USE_USERS_GPIO__
		#define __USE_DSR_DTR_DEFAULT__
        #define __USE_SAFE_SAVE__             // DevConfig Save verify function added
        #define __USE_APPBOOT_TCP_SEARCH__
        //#define __USE_TCPCONNECT_STATUS_PIN__ // When this option is enabled, the STATUS pin acts as a pin to indicate the TCP connection status of socket. (WIZ750SR-1xx series only)
		#define DEVICE_CLOCK_SELECT	         CLOCK_SOURCE_EXTERNAL
		#define DEVICE_PLL_SOURCE_CLOCK      PLL_SOURCE_12MHz
		#define DEVICE_TARGET_SYSTEM_CLOCK   SYSTEM_CLOCK_48MHz
		#define DEVICE_ID_DEFAULT            "WIZ750SR-1xx" // Device name
	#elif (DEVICE_BOARD_NAME == W7500_S2E) // Chip product
		//#define __USE_UART_IF_SELECTOR__	// RS-232/TTL or RS-422/485 selector using UART IF selector pin
		#define __USE_BOOT_ENTRY__			// Application boot mode entry pin activated
		//#define __USE_APPBACKUP_AREA__
		#define __USE_GPIO_HARDWARE_FLOWCONTROL__
		#define __USE_USERS_GPIO__
		//#define __USE_DSR_DTR_DEFAULT__
        #define __USE_SAFE_SAVE__           // DevConfig Save verify function added
        #define __USE_APPBOOT_TCP_SEARCH__
		#define DEVICE_CLOCK_SELECT	         CLOCK_SOURCE_EXTERNAL
		#define DEVICE_PLL_SOURCE_CLOCK      PLL_SOURCE_12MHz
		#define DEVICE_TARGET_SYSTEM_CLOCK   SYSTEM_CLOCK_48MHz
		#define DEVICE_ID_DEFAULT            "W7500-S2E"
	#else
		//#define __USE_UART_IF_SELECTOR__
		//#define __USE_EXT_EEPROM__
		//#define __USE_APPBACKUP_AREA__
		#define __USE_BOOT_ENTRY__			// Application boot mode entry pin activated
		#define __USE_GPIO_HARDWARE_FLOWCONTROL__
        #define __USE_SAFE_SAVE__           // DevConfig Save verify function added
        #define __USE_APPBOOT_TCP_SEARCH__
		#define DEVICE_CLOCK_SELECT	         CLOCK_SOURCE_EXTERNAL
		#define DEVICE_PLL_SOURCE_CLOCK      PLL_SOURCE_8MHz
		#define DEVICE_TARGET_SYSTEM_CLOCK   SYSTEM_CLOCK_48MHz
		#define DEVICE_ID_DEFAULT            "W7500-S2E" // Device name: WIZwiki_W7500 or WIZwiki_W7500ECO Board
	#endif
#else
	#define __USE_APPBACKUP_AREA__
    #define __USE_SAFE_SAVE__
	#define DEVICE_CLOCK_SELECT	             CLOCK_SOURCE_INTERNAL
	#define DEVICE_PLL_SOURCE_CLOCK          PLL_SOURCE_8MHz
	#define DEVICE_TARGET_SYSTEM_CLOCK       SYSTEM_CLOCK_48MHz
	#define DEVICE_BOARD_NAME                UNKNOWN_DEVICE
	#define DEVICE_ID_DEFAULT                "UNKNOWN"
#endif

#define DEVICE_UART_CNT		1 // Not used

// PHY init defines: USE MDC/MDIO
#define __DEF_USED_MDIO__ 

#ifdef __DEF_USED_MDIO__ // MDC/MDIO defines
	#ifndef __W7500P__ // W7500
		//#define __DEF_USED_IC101AG__ // PHY initialize for WIZwiki-W7500 Board
		#define W7500x_MDIO    GPIO_Pin_14
		#define W7500x_MDC     GPIO_Pin_15
	#else // W7500P
		#define W7500x_MDIO    GPIO_Pin_15
		#define W7500x_MDC     GPIO_Pin_14
	#endif
#endif

/* PHY Link check  */
#define PHYLINK_CHECK_CYCLE_MSEC	1000

////////////////////////////////
// Pin definitions			  //
////////////////////////////////
#if (DEVICE_BOARD_NAME == WIZ750SR_1xx) // ##20161031 WIZ750SR-1xx

	#define PHYLINK_IN_PIN				GPIO_Pin_0
	#define PHYLINK_IN_PORT				GPIOA
	#define PHYLINK_IN_PAD_AF			PAD_AF1 // PAD Config - LED used 2nd Function
	
	// Connection status indicator pins
	// Direction: Output
	#define STATUS_PHYLINK_PIN			GPIO_Pin_10
	#define STATUS_PHYLINK_PORT			GPIOA
	#define STATUS_PHYLINK_PAD_AF		PAD_AF1

	#define STATUS_TCPCONNECT_PIN		GPIO_Pin_9
	#define STATUS_TCPCONNECT_PORT		STATUS_PHYLINK_PORT
	#define STATUS_TCPCONNECT_PAD_AF	STATUS_PHYLINK_PAD_AF

	#define DTR_PIN						STATUS_PHYLINK_PIN
	#define DTR_PORT					STATUS_PHYLINK_PORT
	#define DTR_PAD_AF					STATUS_PHYLINK_PAD_AF

	#define DSR_PIN						STATUS_TCPCONNECT_PIN
	#define DSR_PORT					STATUS_TCPCONNECT_PORT
	#define DSR_PAD_AF					STATUS_TCPCONNECT_PAD_AF

	// HW_TRIG - Command mode switch enable pin
	// Direction: Input (Shared pin with TCP connection status pin)
	#define HW_TRIG_PIN					GPIO_Pin_7
	#define HW_TRIG_PORT				GPIOA
	#define HW_TRIG_PAD_AF				PAD_AF1

	// TCP Connection status indicator pin (WIZ750SR-10x series only)
	// Direction: Output
	#define STATUS_PIN			GPIO_Pin_7
	#define STATUS_PORT			GPIOA
	#define STATUS_PAD_AF		PAD_AF1

#else // Original pins
	// PHY link status pin: Input (PHYLINK_IN_PIN -> STATUS_PHYLINK_PIN)
	#define PHYLINK_IN_PIN				GPIO_Pin_9
	#define PHYLINK_IN_PORT				GPIOA
	#define PHYLINK_IN_PAD_AF			PAD_AF1 // PAD Config - LED used 2nd Function

	// Connection status indicator pins
	// Direction: Output
	#define STATUS_PHYLINK_PIN			GPIO_Pin_10
	#define STATUS_PHYLINK_PORT			GPIOA
	#define STATUS_PHYLINK_PAD_AF		PAD_AF1

	#define STATUS_TCPCONNECT_PIN		GPIO_Pin_1
	#define STATUS_TCPCONNECT_PORT		STATUS_PHYLINK_PORT
	#define STATUS_TCPCONNECT_PAD_AF	STATUS_PHYLINK_PAD_AF

	// DTR / DSR - Handshaking signals, Shared with PHYLINK_PIN and TCPCONNECT_PIN (selectable)
	// > DTR - Data Terminal Ready, Direction: Output (= PHYLINK_PIN)
	// 		>> This signal pin asserted when the device could be possible to transmit the UART inputs
	// 		>> [O], After boot and initialize
	// 		>> [X], nope (E.g., TCP connected (Server / client mode) or TCP mixed mode or UDP mode)
	// > DSR - Data Set Ready, Direction: Input (= TCPCONNECT_PIN)
	// 		>> [O] Ethet_to_UART() function control

	#define DTR_PIN						STATUS_PHYLINK_PIN
	#define DTR_PORT					STATUS_PHYLINK_PORT
	#define DTR_PAD_AF					STATUS_PHYLINK_PAD_AF

	#define DSR_PIN						STATUS_TCPCONNECT_PIN
	#define DSR_PORT					STATUS_TCPCONNECT_PORT
	#define DSR_PAD_AF					STATUS_TCPCONNECT_PAD_AF
	
	// HW_TRIG - Command mode switch enable pin
	// Direction: Input (Shared pin with TCP connection status pin)
	#define HW_TRIG_PIN					STATUS_TCPCONNECT_PIN
	#define HW_TRIG_PORT				STATUS_TCPCONNECT_PORT
	#define HW_TRIG_PAD_AF				STATUS_TCPCONNECT_PAD_AF

#endif

// UART Interface Selector Pin (temporary pin)
// [Pull-down] RS-232/TTL mode
// [Pull-up  ] RS-422/485 mode
// if does not use this pin, UART interface is fixed RS-232 / TTL mode
#ifdef __USE_UART_IF_SELECTOR__
	#define UART_IF_SEL_PIN			GPIO_Pin_6
	#define UART_IF_SEL_PORT		GPIOC
	#define UART_IF_SEL_PAD_AF		PAD_AF1 // Used 2nd Function, GPIO
#endif

// Configuration Storage Selector
// Internal data flash (DAT0/DAT1) or External EEPROM (I2C bus interface via GPIO pins)
#ifdef __USE_EXT_EEPROM__
	#include "i2cHandler.h"
	#define EEPROM_I2C_SCL_PIN		I2C_SCL_PIN
	#define EEPROM_I2C_SCL_PORT		I2C_SCL_PORT
	#define EEPROM_I2C_SDA_PIN		I2C_SDA_PIN
	#define EEPROM_I2C_SDA_PORT		I2C_SDA_PORT
	#define EEPROM_I2C_PAD_AF		I2C_PAD_AF // GPIO
#endif

#ifdef __USE_BOOT_ENTRY__
	#if (DEVICE_BOARD_NAME == WIZ750SR_1xx) // ##20170215 WIZ750SR_1xx
		#define BOOT_ENTRY_PIN			GPIO_Pin_8
		#define BOOT_ENTRY_PORT			GPIOA
		#define BOOT_ENTRY_PAD_AF		PAD_AF1
	#else
		#define BOOT_ENTRY_PIN			GPIO_Pin_14
		#define BOOT_ENTRY_PORT			GPIOC
		#define BOOT_ENTRY_PAD_AF		PAD_AF1
	#endif
#endif

// Expansion GPIOs (4-Pins, GPIO A / B / C / D)

#ifdef __USE_USERS_GPIO__

	//#define MAX_USER_IOn    16
	#define USER_IOn       4
	#define USER_IO_A      (uint16_t)(0x01 <<  0)     // USER's I/O A
	#define USER_IO_B      (uint16_t)(0x01 <<  1)     // USER's I/O B
	#define USER_IO_C      (uint16_t)(0x01 <<  2)     // USER's I/O C
	#define USER_IO_D      (uint16_t)(0x01 <<  3)     // USER's I/O D

	#define USER_IO_DEFAULT_PAD_AF		PAD_AF1 // [2nd] GPIO
	#define USER_IO_AIN_PAD_AF			PAD_AF3 // [4th] AIN

	#define USER_IO_NO_ADC				0xff

	#if (DEVICE_BOARD_NAME == WIZ750SR_1xx) // ##20170215 WIZ750SR_1xx
		// USER IO pins for WIZ750SR_1xx
		#define USER_IO_A_PIN				GPIO_Pin_15
		#define USER_IO_A_PORT				GPIOC
		#define USER_IO_A_ADC_CH			ADC_CH0

		#define USER_IO_B_PIN				GPIO_Pin_14
		#define USER_IO_B_PORT				GPIOC
		#define USER_IO_B_ADC_CH			ADC_CH1

		#define USER_IO_C_PIN				GPIO_Pin_13
		#define USER_IO_C_PORT				GPIOC
		#define USER_IO_C_ADC_CH			ADC_CH2

		#define USER_IO_D_PIN				GPIO_Pin_12
		#define USER_IO_D_PORT				GPIOC
		#define USER_IO_D_ADC_CH			ADC_CH3
	#else
		// USER IO pins for WIZ750SR / WIZwiki-W7500ECO
		#define USER_IO_A_PIN				GPIO_Pin_13 // ECO: P28, AIN2
		#define USER_IO_A_PORT				GPIOC
		#define USER_IO_A_ADC_CH			ADC_CH2

		#define USER_IO_B_PIN				GPIO_Pin_12 // ECO: P27, AIN3
		#define USER_IO_B_PORT				GPIOC
		#define USER_IO_B_ADC_CH			ADC_CH3

		#define USER_IO_C_PIN				GPIO_Pin_9 // ECO: P26, AIN6
		#define USER_IO_C_PORT				GPIOC
		#define USER_IO_C_ADC_CH			ADC_CH6

		#define USER_IO_D_PIN				GPIO_Pin_8 // ECO: P25, AIN7
		#define USER_IO_D_PORT				GPIOC
		#define USER_IO_D_ADC_CH			ADC_CH7
	#endif

#endif


// Status LEDs define
#if ((DEVICE_BOARD_NAME == WIZ750SR) || (DEVICE_BOARD_NAME == W7500P_S2E))

	#define LED1_PIN			GPIO_Pin_2
	#define LED1_GPIO_PORT		GPIOB
	#define LED1_GPIO_PAD		PAD_PB
	#define LED1_GPIO_PAD_AF	PAD_AF1		// PAD Config - LED used 2nd Function

	#define LED2_PIN			GPIO_Pin_3
	#define LED2_GPIO_PORT		GPIOB
	#define LED2_GPIO_PAD		PAD_PB
	#define LED2_GPIO_PAD_AF	PAD_AF1

#elif (DEVICE_BOARD_NAME == WIZ750SR_1xx) // ##20161031 WIZ750SR-1xx
	
	#define LED1_PIN			GPIO_Pin_8
	#define LED1_GPIO_PORT		GPIOC
	#define LED1_GPIO_PAD		PAD_PC
	#define LED1_GPIO_PAD_AF	PAD_AF1		// PAD Config - LED used 2nd Function

	#define LED2_PIN			GPIO_Pin_9
	#define LED2_GPIO_PORT		GPIOC
	#define LED2_GPIO_PAD		PAD_PC
	#define LED2_GPIO_PAD_AF	PAD_AF1
	
#elif (DEVICE_BOARD_NAME == WIZwiki_W7500ECO)

	#define LED1_PIN			GPIO_Pin_1
	#define LED1_GPIO_PORT		GPIOA
	#define LED1_GPIO_PAD		PAD_PA
	#define LED1_GPIO_PAD_AF	PAD_AF1		// PAD Config - LED used 2nd Function

	#define LED2_PIN			GPIO_Pin_2
	#define LED2_GPIO_PORT		GPIOA
	#define LED2_GPIO_PAD		PAD_PA
	#define LED2_GPIO_PAD_AF	PAD_AF1

#else // WIZwiki-W7500 board

	// [RGB LED] R: PC_08, G: PC_09, B: PC_05
	#define LED1_PIN			GPIO_Pin_8 // RGB LED: RED
	#define LED1_GPIO_PORT		GPIOC
	#define LED1_GPIO_PAD		PAD_PC
	#define LED1_GPIO_PAD_AF	PAD_AF1

	#define LED2_PIN			GPIO_Pin_9 // RGB LED: GREEN
	#define LED2_GPIO_PORT		GPIOC
	#define LED2_GPIO_PAD		PAD_PC
	#define LED2_GPIO_PAD_AF	PAD_AF1
	
/*	
	#define LED_R_PIN			GPIO_Pin_8
	#define LED_R_GPIO_PORT		GPIOC
	#define LED_R_GPIO_PAD		PAD_PC
	#define LED_R_GPIO_PAD_AF	PAD_AF1

	#define LED_G_PIN			GPIO_Pin_9
	#define LED_G_GPIO_PORT		GPIOC
	#define LED_G_GPIO_PAD		PAD_PC
	#define LED_G_GPIO_PAD_AF	PAD_AF1

	#define LED_B_PIN			GPIO_Pin_5
	#define LED_B_GPIO_PORT		GPIOC
	#define LED_B_GPIO_PAD		PAD_PC
	#define LED_B_GPIO_PAD_AF	PAD_AF1

	// LED
	#define LEDn		3
	typedef enum
	{
	  LED_R = 0,
	  LED_G = 1,
	  LED_B = 2  
	} Led_TypeDef;
*/
#endif
	
	// LED
	#define LEDn		2
	typedef enum
	{
	  LED1 = 0,	// PHY link status
	  LED2 = 1	// TCP connection status
	} Led_TypeDef;

	extern volatile uint16_t phylink_check_time_msec;
	extern uint8_t flag_check_phylink;
	extern uint8_t flag_hw_trig_enable;
	
#ifdef __cplusplus
extern "C" {
#endif
	void W7500x_Board_Init(void);
	void Supervisory_IC_Init(void);
	
	void init_hw_trig_pin(void);
	uint8_t get_hw_trig_pin(void);
	
	void init_phylink_in_pin(void);
	uint8_t get_phylink_in_pin(void);
	
	void init_uart_if_sel_pin(void);
	uint8_t get_uart_if_sel_pin(void);
	
#ifdef __USE_BOOT_ENTRY__
	void init_boot_entry_pin(void);
	uint8_t get_boot_entry_pin(void);
#endif
	
	void LED_Init(Led_TypeDef Led);
	void LED_On(Led_TypeDef Led);
	void LED_Off(Led_TypeDef Led);
	void LED_Toggle(Led_TypeDef Led);
	uint8_t get_LED_Status(Led_TypeDef Led);
	
#ifdef __cplusplus
}
#endif

#endif

