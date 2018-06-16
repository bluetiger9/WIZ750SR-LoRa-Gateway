#include "W7500x.h"
#include "W7500x_gpio.h"

#include "common.h"
#include "W7500x_board.h"
#include "ConfigData.h"
#include "gpioHandler.h"

#ifdef _GPIO_DEBUG_
	#include <stdio.h>
#endif

#ifdef __USE_USERS_GPIO__
	const uint16_t USER_IO_PIN[USER_IOn] =     {USER_IO_A_PIN, USER_IO_B_PIN, USER_IO_C_PIN, USER_IO_D_PIN};
	GPIO_TypeDef* USER_IO_PORT[USER_IOn] =     {USER_IO_A_PORT, USER_IO_B_PORT, USER_IO_C_PORT, USER_IO_D_PORT};
	uint8_t     USER_IO_ADC_CH[USER_IOn] =     {USER_IO_A_ADC_CH, USER_IO_B_ADC_CH, USER_IO_C_ADC_CH, USER_IO_D_ADC_CH};

	uint8_t        USER_IO_SEL[USER_IOn] =     {USER_IO_A, USER_IO_B, USER_IO_C, USER_IO_D};
	const char*    USER_IO_STR[USER_IOn] =     {"a", "b", "c", "d"};
	#if (DEVICE_BOARD_NAME == WIZ750SR_1xx)
		const char*    USER_IO_PIN_STR[USER_IOn] = {"PC15\0", "PC14\0", "PC13\0", "PC12\0"}; // WIZ750SR_1xx
	#else
		const char*    USER_IO_PIN_STR[USER_IOn] = {"PC28\0", "PC27\0", "PC26\0", "PC25\0",}; // W(IZ750SR
	#endif
	const char*    USER_IO_TYPE_STR[] =        {"Digital", "Analog"};
	const char*    USER_IO_DIR_STR[] =         {"Input", "Output"};
#endif

/**
  * @brief  I/O Intialize Function
  */
void IO_Configuration(void)
{
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	//uint16_t val;
	
	/* GPIOs Initialization */
	
	// Status I/O - Shared pin init: Connection status pins or DTR/DSR pins
	init_connection_status_io();
	
	// Set the DTR pin to high when the DTR signal enabled (== PHY link status disabled)
	if(serial->dtr_en == 1) set_flowcontrol_dtr_pin(ON);
	
	/* GPIOs Initialization */
	// Expansion GPIOs (4-pins, A to D)
	// Available to the ANALOG input pins or DIGITAL input/output pins
#ifdef __USE_USERS_GPIO__
	if(get_user_io_enabled(USER_IO_A)) init_user_io(USER_IO_A);
	if(get_user_io_enabled(USER_IO_B)) init_user_io(USER_IO_B);
	if(get_user_io_enabled(USER_IO_C)) init_user_io(USER_IO_C);
	if(get_user_io_enabled(USER_IO_D)) init_user_io(USER_IO_D);
#endif
	
	// ## debugging: io functions test
	/*
	get_user_io_val(USER_IO_A, &val);
	printf("USER_IO_A: [val] %d, [enable] %s, [type] %s, [direction] %s\r\n", val, get_user_io_enabled(USER_IO_A)?"enabled":"disabled", get_user_io_type(USER_IO_A)?"analog":"digital", get_user_io_direction(USER_IO_A)?"output":"input");
	get_user_io_val(USER_IO_B, &val);
	printf("USER_IO_B: [val] %d, [enable] %s, [type] %s, [direction] %s\r\n", val, get_user_io_enabled(USER_IO_B)?"enabled":"disabled", get_user_io_type(USER_IO_B)?"analog":"digital", get_user_io_direction(USER_IO_B)?"output":"input");
	get_user_io_val(USER_IO_C, &val);
	printf("USER_IO_C: [val] %d, [enable] %s, [type] %s, [direction] %s\r\n", val, get_user_io_enabled(USER_IO_C)?"enabled":"disabled", get_user_io_type(USER_IO_C)?"analog":"digital", get_user_io_direction(USER_IO_C)?"output":"input");
	get_user_io_val(USER_IO_D, &val);
	printf("USER_IO_D: [val] %d, [enable] %s, [type] %s, [direction] %s\r\n", val, get_user_io_enabled(USER_IO_D)?"enabled":"disabled", get_user_io_type(USER_IO_D)?"analog":"digital", get_user_io_direction(USER_IO_D)?"output":"input");
	*/
}

#ifdef __USE_USERS_GPIO__

void init_user_io(uint8_t io_sel)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	uint8_t idx = 0;
	
	uint8_t io_status = 0;
	
	if((user_io_info->user_io_enable & io_sel) == io_sel)
	{
		idx = get_user_io_bitorder(io_sel);
		
		if((user_io_info->user_io_type & io_sel) == io_sel) // IO_ANALOG_IN == 1 
		{
			if(USER_IO_ADC_CH[idx] != USER_IO_NO_ADC)
			{
				// Analog Input
				ADC_Init();
				GPIO_Configuration(USER_IO_PORT[idx], USER_IO_PIN[idx], GPIO_Mode_IN, USER_IO_AIN_PAD_AF);
			}
			//else // IO init falied
		}
		else
		{
			// Digital Input / Output
			if((user_io_info->user_io_direction & io_sel) == io_sel) // IO_OUTPUT == 1
			{
				GPIO_Configuration(USER_IO_PORT[idx], USER_IO_PIN[idx], GPIO_Mode_OUT, USER_IO_DEFAULT_PAD_AF);
				io_status = ((user_io_info->user_io_status & (uint16_t)io_sel) == (uint16_t)io_sel)?IO_HIGH:IO_LOW;
				
				if(io_status == IO_HIGH) {
					GPIO_SetBits(USER_IO_PORT[idx], USER_IO_PIN[idx]);
				} else {
					GPIO_ResetBits(USER_IO_PORT[idx], USER_IO_PIN[idx]);
				}
			}
			else
			{
				GPIO_Configuration(USER_IO_PORT[idx], USER_IO_PIN[idx], GPIO_Mode_IN, USER_IO_DEFAULT_PAD_AF);
				USER_IO_PORT[idx]->OUTENCLR = USER_IO_PIN[idx];
			}
		}
	}
}


uint8_t set_user_io_enable(uint8_t io_sel, uint8_t enable)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	uint8_t ret = 1;
	
	
	if(enable == IO_ENABLE)
	{
		// IO enables
		user_io_info->user_io_enable |= io_sel;
	}
	else if(enable == IO_DISABLE)
	{
		// IO disables
		user_io_info->user_io_enable &= ~(io_sel);
	}
	else
		ret = 0;
	
	return ret;
}


uint8_t set_user_io_type(uint8_t io_sel, uint8_t type)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	uint8_t ret = 1;
	uint8_t idx;
	
	if(type == IO_ANALOG_IN)
	{
		idx = get_user_io_bitorder(io_sel);
		
		if(USER_IO_ADC_CH[idx] != USER_IO_NO_ADC)
		{
			set_user_io_direction(io_sel, IO_INPUT); // Analog: input only
			user_io_info->user_io_type |= io_sel;
			init_user_io(io_sel); // IO reinitialize
		}
		else
		{
			// IO setting failed
			ret = 0;
		}
	}
	else if(type == IO_DIGITAL)
	{
		user_io_info->user_io_type &= ~(io_sel);
		init_user_io(io_sel); // IO reinitialize
	}
	else
		ret = 0;
	
	init_user_io(io_sel);
	
	return ret;
}


uint8_t set_user_io_direction(uint8_t io_sel, uint8_t dir)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	uint8_t ret = 1;
	
	
	if(dir == IO_OUTPUT)
	{
		user_io_info->user_io_direction |= io_sel;
		init_user_io(io_sel); // IO reinitialize
	}
	else if(dir == IO_INPUT)
	{
		user_io_info->user_io_direction &= ~(io_sel);
		init_user_io(io_sel); // IO reinitialize
	}
	else
		ret = 0;
	
	init_user_io(io_sel);
	
	return ret;
}


uint8_t get_user_io_enabled(uint8_t io_sel)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	uint8_t ret;
	
	if((user_io_info->user_io_enable & io_sel) == io_sel)
	{
		ret = IO_ENABLE;
	}
	else
	{
		ret = IO_DISABLE;
	}
	
	return ret;
}


uint8_t get_user_io_type(uint8_t io_sel)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	uint8_t ret;
	
	if((user_io_info->user_io_type & io_sel) == io_sel)
	{
		ret = IO_ANALOG_IN;
	}
	else
	{
		ret = IO_DIGITAL;
	}
	
	return ret;
}


uint8_t get_user_io_direction(uint8_t io_sel)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	uint8_t ret;
	
	if((user_io_info->user_io_direction & io_sel) == io_sel)
	{
		ret = IO_OUTPUT;
	}
	else
	{
		ret = IO_INPUT;
	}
	
	return ret;
}


// Analog input: 	Returns ADC value (12-bit resolution)
// Digital in/out: 	Returns I/O status to match the bit ordering
uint8_t get_user_io_val(uint16_t io_sel, uint16_t * val)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	
	uint8_t ret = 0; // I/O Read failed
	uint8_t idx = 0;
	uint8_t status = 0;
	
	*val = 0;
	
	if((user_io_info->user_io_enable & io_sel) == io_sel) // Enable
	{
		idx = get_user_io_bitorder(io_sel);
		
		if((user_io_info->user_io_type & io_sel) == io_sel) // IO_ANALOG == 1
		{
			if(USER_IO_ADC_CH[idx] != USER_IO_NO_ADC)
			{
				// Analog Input: value
				*val = read_ADC((ADC_CH)USER_IO_ADC_CH[idx]);
			}
			else
			{
				// IO value get failed
				*val = 0;
			}
		}
		else // IO_DIGITAL == 0
		{
			// Digital Input / Output
			if((user_io_info->user_io_direction & io_sel) == io_sel) // IO_OUTPUT == 1
			{
				// Digital Output: status
				status = (uint16_t)GPIO_ReadOutputDataBit(USER_IO_PORT[idx], USER_IO_PIN[idx]);
			}
			else // IO_INPUT == 0
			{
				// Digital Input: status
				status = (uint16_t)GPIO_ReadInputDataBit(USER_IO_PORT[idx], USER_IO_PIN[idx]);
			}
			
			//*val |= (status << i);
			*val = status;
		}
		
		ret = 1; // I/O Read success
	}
	
	return ret;
}


uint8_t set_user_io_val(uint16_t io_sel, uint16_t * val)
{
	struct __user_io_info *user_io_info = (struct __user_io_info *)&(get_DevConfig_pointer()->user_io_info);
	uint8_t ret = 0; // I/O Read failed
	uint8_t idx = 0;
	
	if((user_io_info->user_io_enable & io_sel) == io_sel) // Enable
	{
		// Digital output only (type == 0 && direction == 1)
		if(((user_io_info->user_io_type & io_sel) == IO_DIGITAL) && ((user_io_info->user_io_direction & io_sel) == io_sel))
		{
			idx = get_user_io_bitorder(io_sel);
			
			if(*val == 0)
			{
				user_io_info->user_io_status &= ~(io_sel);
				GPIO_ResetBits(USER_IO_PORT[idx], USER_IO_PIN[idx]);
			}
			else if(*val == 1)
			{
				user_io_info->user_io_status |= io_sel;
				GPIO_SetBits(USER_IO_PORT[idx], USER_IO_PIN[idx]);
			}
			
			ret = 1;
		}
	}
	
	return ret;
}


uint8_t get_user_io_bitorder(uint16_t io_sel)
{
	uint8_t i;
	uint8_t ret = 0;
	
	for(i = 0; i < USER_IOn; i++)
	{
		if((io_sel >> i) == 1)
		{
			ret = i;
			break;
		}
	}
		
	return ret;
}

// Read ADC val: 12-bit ADC resolution
uint16_t read_ADC(ADC_CH ch)
{
	ADC_ChannelSelect(ch);				///< Select ADC channel to CH0
	ADC_Start();						///< Start ADC
	while(ADC_IsEOC());					///< Wait until End of Conversion
	
	return ((uint16_t)ADC_ReadData());	///< read ADC Data
}

#endif

void init_connection_status_io(void)
{
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	
	if(serial->dtr_en == 0)	init_phylink_status_pin();
	else					init_flowcontrol_dtr_pin();

	if(serial->dsr_en == 0)	init_tcpconnection_status_pin();
	else					init_flowcontrol_dsr_pin();

#if (DEVICE_BOARD_NAME == WIZ750SR_1xx)
    #ifdef __USE_TCPCONNECT_STATUS_PIN__
        // TCP connection status pin for WIZ750SR-10x series only
        init_status_pin();
    #endif
#endif
}

// This function is intended only for output connection status pins; PHYlink, TCPconnection
void set_connection_status_io(uint16_t pin, uint8_t set)
{
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	
	if(pin == STATUS_PHYLINK_PIN)
	{
		if(set == ON)
		{
			
			if(serial->dtr_en == 0) GPIO_ResetBits(STATUS_PHYLINK_PORT, STATUS_PHYLINK_PIN); 
			LED_On(LED1);
		}
		else // OFF
		{
			if(serial->dtr_en == 0) GPIO_SetBits(STATUS_PHYLINK_PORT, STATUS_PHYLINK_PIN); 
			LED_Off(LED1);
		}
	}
	else if(pin == STATUS_TCPCONNECT_PIN)
	{
		if(set == ON)
		{
			if(serial->dsr_en == 0) GPIO_ResetBits(STATUS_TCPCONNECT_PORT, STATUS_TCPCONNECT_PIN);
#if (DEVICE_BOARD_NAME == WIZ750SR_1xx)
    #ifdef __USE_TCPCONNECT_STATUS_PIN__
            GPIO_ResetBits(STATUS_PORT, STATUS_PIN);
    #endif
#endif
			LED_On(LED2);
		}
		else // OFF
		{
			
			if(serial->dsr_en == 0) GPIO_SetBits(STATUS_TCPCONNECT_PORT, STATUS_TCPCONNECT_PIN);
#if (DEVICE_BOARD_NAME == WIZ750SR_1xx)
    #ifdef __USE_TCPCONNECT_STATUS_PIN__
            GPIO_SetBits(STATUS_PORT, STATUS_PIN);
    #endif
#endif
			LED_Off(LED2);
		}
	}
}

uint8_t get_connection_status_io(uint16_t pin)
{
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	uint8_t status;
	
	if(pin == STATUS_PHYLINK_PIN)
	{
			if(serial->dtr_en == 0)
				status = GPIO_ReadInputDataBit(STATUS_PHYLINK_PORT, STATUS_PHYLINK_PIN);
			else
				status = GPIO_ReadOutputDataBit(DTR_PORT, DTR_PIN);
	}
	else if(pin == STATUS_TCPCONNECT_PIN)
	{
			if(serial->dsr_en == 0) 
				status = GPIO_ReadInputDataBit(STATUS_TCPCONNECT_PORT, STATUS_TCPCONNECT_PIN); 
			else 
				status = GPIO_ReadInputDataBit(DSR_PORT, DSR_PIN);
	}
	
	return status;
}

// PHY link status pin
void init_phylink_status_pin(void)
{
	GPIO_Configuration(STATUS_PHYLINK_PORT, STATUS_PHYLINK_PIN, GPIO_Mode_OUT, STATUS_PHYLINK_PAD_AF);
	
	// Pin initial state; High
	GPIO_SetBits(STATUS_PHYLINK_PORT, STATUS_PHYLINK_PIN); 
}


// TCP connection status pin
void init_tcpconnection_status_pin(void)
{
	GPIO_Configuration(STATUS_TCPCONNECT_PORT, STATUS_TCPCONNECT_PIN, GPIO_Mode_OUT, STATUS_TCPCONNECT_PAD_AF);
	
	// Pin initial state; High
	GPIO_SetBits(STATUS_TCPCONNECT_PORT, STATUS_TCPCONNECT_PIN); 

#if (DEVICE_BOARD_NAME == WIZ750SR_1xx)
    #ifdef __USE_TCPCONNECT_STATUS_PIN__
        GPIO_Configuration(STATUS_PORT, STATUS_PIN, GPIO_Mode_OUT, STATUS_PAD_AF);
        GPIO_SetBits(STATUS_PORT, STATUS_PIN);
    #endif
#endif
}

#if (DEVICE_BOARD_NAME == WIZ750SR_1xx)
// TCP connection status pin for WIZ750SR-10x series only
void init_status_pin(void)
{
	GPIO_Configuration(STATUS_PORT, STATUS_PIN, GPIO_Mode_OUT, STATUS_PAD_AF);
	
	// Pin initial state; High
	GPIO_SetBits(STATUS_PORT, STATUS_PIN);
}
#endif

// DTR pin
// output (shared pin with PHY link status)
void init_flowcontrol_dtr_pin(void)
{
	GPIO_Configuration(DTR_PORT, DTR_PIN, GPIO_Mode_OUT, DTR_PAD_AF);
	
	// Pin initial state; High
	GPIO_ResetBits(DTR_PORT, DTR_PIN); 
}

void set_flowcontrol_dtr_pin(uint8_t set)
{
	if(set == ON)	GPIO_SetBits(DTR_PORT, DTR_PIN);
	else			GPIO_ResetBits(DTR_PORT, DTR_PIN);
}


// DSR pin
// input, active high (shared pin with TCP connection status)
void init_flowcontrol_dsr_pin(void)
{
	GPIO_Configuration(DSR_PORT, DSR_PIN, GPIO_Mode_IN, DSR_PAD_AF);
}


uint8_t get_flowcontrol_dsr_pin(void)
{
	return GPIO_ReadInputDataBit(DSR_PORT, DSR_PIN);
}


// Check the PHY link status
void check_phylink_status(void)
{
	static uint8_t prev_link_status = 1;
	uint8_t link_status;
	
#if ((DEVICE_BOARD_NAME == WIZ750SR) || (DEVICE_BOARD_NAME == W7500P_S2E) || (DEVICE_BOARD_NAME == WIZ750SR_1xx))
	link_status = get_phylink_in_pin();
#else
	link_status = 0;
#endif
	
	if(prev_link_status != link_status)
	{
		if(link_status == 0x00)
			set_connection_status_io(STATUS_PHYLINK_PIN, ON); 	// PHY Link up
		else
			set_connection_status_io(STATUS_PHYLINK_PIN, OFF); 	// PHY Link down
		
		prev_link_status = link_status;
	}
}

// This function have to call every 1 millisecond by Timer IRQ handler routine.
void gpio_handler_timer_msec(void)
{
	// PHY link check
	if(++phylink_check_time_msec >= PHYLINK_CHECK_CYCLE_MSEC)
	{
		phylink_check_time_msec = 0;
		check_phylink_status();
		
		flag_check_phylink = 1;
	}
}
