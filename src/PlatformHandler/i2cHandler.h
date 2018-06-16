
#ifndef __I2CHANDLER_H__
#define __I2CHANDLER_H__

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "W7500x_gpio.h"

// Define the GPIO pins for EEPROM access via I2C bus interface 
#define I2C_SCL_PIN     GPIO_Pin_4
#define I2C_SCL_PORT    GPIOC
#define I2C_SDA_PIN     GPIO_Pin_5
#define I2C_SDA_PORT    GPIOC
#define I2C_PAD_AF      PAD_AF1 // GPIO

#define I2C_READ_DATA   GPIO_ReadInputDataBit(I2C_SDA_PORT, I2C_SDA_PIN)

void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
uint8_t I2C_Wait_Ack(void);
void I2C_Ack(void);
void I2C_NAck(void);
void I2C_Send_Byte(uint8_t txd);
uint8_t I2C_Read_Byte(unsigned char ack);

#endif
