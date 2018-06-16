
/* Includes ------------------------------------------------------------------*/
#include "i2cHandler.h"
#include "eepromHandler.h"

/* Private functions prototype -----------------------------------------------*/
void I2C_delay_us(uint16_t num);

/* Private functions ---------------------------------------------------------*/
void I2C_SCL_HIGH(void)
{
	GPIO_SetBits(I2C_SCL_PORT, I2C_SCL_PIN);
}

void I2C_SCL_LOW(void)
{
	GPIO_ResetBits(I2C_SCL_PORT, I2C_SCL_PIN);
}

void I2C_SDA_HIGH(void)
{
	I2C_SDA_PORT->OUTENCLR = I2C_SDA_PIN;
}

void I2C_SDA_LOW(void)
{
	I2C_SDA_PORT->OUTENSET = I2C_SDA_PIN;
}

/* Public functions ----------------------------------------------------------*/
void I2C_Init(void)
{
	// SCL
	GPIO_Configuration(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_Mode_OUT, I2C_PAD_AF);
	GPIO_SetBits(I2C_SCL_PORT, I2C_SCL_PIN);
	
	// SDA
	GPIO_Configuration(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_Mode_IN, I2C_PAD_AF);
	GPIO_ResetBits(I2C_SCL_PORT, I2C_SCL_PIN);
}


void I2C_Start(void)
{
	I2C_SDA_HIGH();
	I2C_SCL_HIGH();
	I2C_delay_us(4);
	I2C_SDA_LOW();
	I2C_delay_us(4);
	I2C_SCL_LOW();
}

void I2C_Stop(void)
{
	I2C_SCL_LOW();
	I2C_SDA_LOW();
	I2C_delay_us(4);
	I2C_SCL_HIGH();
	I2C_SDA_HIGH();
	I2C_delay_us(4);
}

uint8_t I2C_Wait_Ack(void)
{
	uint16_t ucErrTime=0;

	I2C_SDA_HIGH();
	I2C_delay_us(1);
	I2C_SCL_HIGH();
	I2C_delay_us(1);

	while(I2C_READ_DATA)
	{
		ucErrTime++;
		if(ucErrTime > 2500)
		{
			I2C_Stop();
			return 1;
		}
	}

	I2C_SCL_LOW();

	return 0;
}

void I2C_Ack(void)
{
	I2C_SCL_LOW();
	I2C_SDA_LOW();
	I2C_delay_us(2);
	I2C_SCL_HIGH();
	I2C_delay_us(2);
	I2C_SCL_LOW();
}

void I2C_NAck(void)
{
	I2C_SCL_LOW();
	I2C_SDA_HIGH();
	I2C_delay_us(2);
	I2C_SCL_HIGH();
	I2C_delay_us(2);
	I2C_SCL_LOW();
}

void I2C_Send_Byte(uint8_t txd)
{
	uint8_t t;

	I2C_SCL_LOW();

	for(t = 0; t < 8; t++)
	{
		if(txd & 0x80)
			I2C_SDA_HIGH();
		else
			I2C_SDA_LOW();
		
		txd<<=1;
		
		I2C_delay_us(2);
		I2C_SCL_HIGH();
		I2C_delay_us(2);
		I2C_SCL_LOW();
		I2C_delay_us(2);
	}
}

uint8_t I2C_Read_Byte(unsigned char ack)
{
	unsigned char i,receive = 0;
	
	I2C_SDA_HIGH();
	
	for(i = 0; i < 8; i++)
	{
		I2C_SCL_LOW();
		I2C_delay_us(2);
		I2C_SCL_HIGH();
		receive<<=1;

		if(I2C_READ_DATA) receive++;

		I2C_delay_us(1);
	}

	if(!ack)
		I2C_NAck();
	else
		I2C_Ack();

	return receive;
}

void I2C_delay_us(uint16_t num)
{
	uint8_t i,j;

	for(i=0;i<num;i++)
		for(j=0;j<20;j++);
}

