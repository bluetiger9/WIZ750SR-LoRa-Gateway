#ifndef __GPIOHANDLER_H__
#define __GPIOHANDLER_H__

#include <stdint.h>
#include "W7500x_adc.h"

#define _GPIO_DEBUG_

typedef enum
{
	IO_DIGITAL_INPUT  = 0,
	IO_DIGITAL_OUTPUT = 1,
	IO_ANALOG_INPUT   = 2,
	IO_ANALOG_OUTPUT  = 3,
	IO_UNKNOWN        = 0xff
} USER_IO_Define;

typedef enum
{
	IO_INPUT  = 0,
	IO_OUTPUT = 1
} USER_IO_Direction;

typedef enum
{
	IO_DIGITAL   = 0,
	IO_ANALOG_IN = 1
} USER_IO_Type;

typedef enum
{
	IO_LOW  = 0,
	IO_HIGH = 1
} USER_IO_Status;

typedef enum
{
	IO_DISABLE = 0,
	IO_ENABLE  = 1
} USER_IO_Enable;

extern uint8_t        USER_IO_SEL[];
extern const char*    USER_IO_STR[];
extern const char*    USER_IO_PIN_STR[];
extern const char*    USER_IO_TYPE_STR[];
extern const char*    USER_IO_DIR_STR[];

void IO_Configuration(void);

// User IO
void init_user_io(uint8_t io_sel);

uint8_t get_user_io_enabled(uint8_t io_sel);
uint8_t get_user_io_type(uint8_t io_sel);
uint8_t get_user_io_direction(uint8_t io_sel);
uint8_t set_user_io_enable(uint8_t io_sel, uint8_t enable);
uint8_t set_user_io_type(uint8_t io_sel, uint8_t type);
uint8_t set_user_io_direction(uint8_t io_sel, uint8_t dir);

uint8_t get_user_io_val(uint16_t io_sel, uint16_t * val); // get the I/O status or value
uint8_t set_user_io_val(uint16_t io_sel, uint16_t * val); // set the I/O status, digital output only

uint8_t get_user_io_bitorder(uint16_t io_sel);
uint16_t read_ADC(ADC_CH ch);

// Status IO
void init_connection_status_io(void);

void init_phylink_status_pin(void);
void init_tcpconnection_status_pin(void);
void init_flowcontrol_dtr_pin(void);
void init_flowcontrol_dsr_pin(void);
void init_status_pin(void);

void set_connection_status_io(uint16_t pin, uint8_t set);
uint8_t get_connection_status_io(uint16_t pin);
void set_flowcontrol_dtr_pin(uint8_t set);
uint8_t get_flowcontrol_dsr_pin(void);


// Check the PHY link status 
void check_phylink_status(void);
void gpio_handler_timer_msec(void); // This function have to call every 1 millisecond by Timer IRQ handler routine.

#endif

