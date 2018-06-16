
/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "eepromHandler.h"
#include "i2cHandler.h"

#ifdef _EEPROM_DEBUG_
	#include <stdio.h>
	#include <ctype.h>
#endif


/* Private functions prototypes ----------------------------------------------*/
void EE24AAXX_Init(void);
uint16_t EE24AAXX_Read(uint16_t ReadAddr,uint8_t *pBuffer,uint16_t NumToRead);
uint16_t EE24AAXX_Write(uint16_t WriteAddr,uint8_t *pBuffer,uint16_t NumToWrite);
uint8_t EE24AAXX_WritePage(uint16_t WriteAddr,uint8_t *pBuffer,uint8_t len);

void delay(__IO uint32_t milliseconds);

/* Public functions ----------------------------------------------------------*/
void init_eeprom(void)
{
	// Initialize the GPIO pins for I2C
	EE24AAXX_Init();
}


uint16_t read_eeprom(uint16_t addr, uint8_t * data, uint16_t data_len)
{
	return EE24AAXX_Read(addr, data, data_len);
}


uint16_t write_eeprom(uint16_t addr, uint8_t * data, uint16_t data_len)
{
	return EE24AAXX_Write(addr, data, data_len);
}


void erase_eeprom_block(uint16_t addr)
{
	uint8_t block, page;
	uint16_t i;
	uint32_t start_addr;
	uint8_t buf[EEPROM_PAGE_SIZE];
	
	block = addr / EEPROM_BLOCK_SIZE;
	if(block >= EEPROM_BLOCK_COUNT) return; // failed
	
	memset(buf, 0xff, EEPROM_PAGE_SIZE);
	page = EEPROM_BLOCK_SIZE / EEPROM_PAGE_SIZE;
	
	block = addr / EEPROM_BLOCK_SIZE;
	start_addr = block * EEPROM_BLOCK_SIZE;
	
	for(i = 0; i < page; i++)
	{
		EE24AAXX_WritePage(start_addr + (i * EEPROM_PAGE_SIZE), buf, EEPROM_PAGE_SIZE);
	}
	
#ifdef _EEPROM_DEBUG_
	printf(" > EEPROM:ERASE: [0x%.4x ~ 0x%.4x]\r\n", start_addr, (start_addr + (i * EEPROM_PAGE_SIZE) - 1));
#endif
}


#ifdef _EEPROM_DEBUG_
void dump_eeprom_block(uint16_t addr)
{
	uint16_t i, j;
	uint16_t len, spaces;
	uint8_t buf[EEPROM_BLOCK_SIZE];
	uint8_t block;
	uint16_t start_addr;
	
	block = addr / EEPROM_BLOCK_SIZE;
	start_addr = block * EEPROM_BLOCK_SIZE;
	
	if(block < EEPROM_BLOCK_COUNT)
	{
		len = EE24AAXX_Read(start_addr, buf, EEPROM_BLOCK_SIZE);
		printf("\r\n > EEPROM:DUMP: [0x%.4x ~ 0x%.4x]\r\n", start_addr, (start_addr + EEPROM_BLOCK_SIZE - 1));
	}
	else
	{
		printf("\r\n > EEPROM:DUMP: Failed [0x%.4x ~ 0x%.4x]\r\n", start_addr, (start_addr + EEPROM_BLOCK_SIZE - 1));
		return;
	}
	
	for(i = 0; i < len; i++)
	{
		if((i%16) == 0)	printf("0x%.04x : ", i + (block * EEPROM_BLOCK_SIZE));
		printf("%.02x ", buf[i]);

		if((i%16-15) == 0)
		{
			printf("  ");
			for (j = i-15; j <= i; j++)
			{
				if(isprint(buf[j]))	printf("%c", buf[j]);
				else				printf(".");
			}
			printf("\r\n");
		}
	}
	
	if((i%16) != 0)
	{
		spaces = (len-i+16-i%16)*3 + 2;
		for(j=0; j<spaces; j++)	printf(" ");
		for(j=i-i%16; j<len; j++)
		{
			if(isprint(buf[j]))	printf("%c", buf[j]);
			else				printf(".");
		}
	}
	printf("\r\n");
}
#endif



/* Private functions ---------------------------------------------------------*/

void EE24AAXX_Init(void)
{
	I2C_Init();
}

uint16_t EE24AAXX_Read(uint16_t addr, uint8_t *buf, uint16_t len)
{
	uint8_t bsb;
	uint8_t block;
	uint16_t i;
	uint16_t ret_len = 0;
	
	block = addr / EEPROM_BLOCK_SIZE;
	if(block >= EEPROM_BLOCK_COUNT) return 0; // failed
	
	bsb = (addr / EEPROM_BLOCK_SIZE) << 1;
	
	__disable_irq(); // Global interrupt disabled
	
	I2C_Start();
	
	if(EE_TYPE > EE24AA16)
	{
		I2C_Send_Byte(0xA0);
		I2C_Wait_Ack();
		I2C_Send_Byte((uint8_t)((addr & 0xFF00) >> 8));
		I2C_Wait_Ack();
		I2C_Send_Byte((uint8_t)(addr & 0x00FF));
		I2C_Wait_Ack();
	}
	else
	{
		I2C_Send_Byte((uint8_t)(0xA0 + bsb));
		I2C_Wait_Ack();
		I2C_Send_Byte((uint8_t)(addr % EEPROM_BLOCK_SIZE));
		I2C_Wait_Ack();
	}
	
	I2C_Start();
	
	if(EE_TYPE>EE24AA16)
	{
		I2C_Send_Byte(0xA1);
	}
	else
	{
		I2C_Send_Byte((uint8_t)(0xA1 + bsb));
	}
	I2C_Wait_Ack();
	
	for(i = 0; i < len; i++)
	{
		ret_len++;
		if(len == ret_len) 
			buf[i] = I2C_Read_Byte(0); // I2C NACK
		else 
			buf[i] = I2C_Read_Byte(1); // I2C ACK
		
	}
	
	I2C_Stop();
	
	__enable_irq(); // Global interrupt enabled
	
	delay(10);
	
	return ret_len;
}

uint16_t EE24AAXX_Write(uint16_t addr, uint8_t *buf, uint16_t len)
{
	uint8_t page, remainder;
	uint8_t block;
	uint16_t write_ptr;
	uint16_t i;
	uint16_t ret_len = 0;
	
	block = addr / EEPROM_BLOCK_SIZE;
	if(block >= EEPROM_BLOCK_COUNT) return 0; // failed
	
	page = len / EEPROM_PAGE_SIZE;
	remainder = len - (page * EEPROM_PAGE_SIZE);
	
#ifdef _EEPROM_DEBUG_
	printf(" > EEPROM:WRITE: [0x%.4x]\r\n", addr);
	printf(" > EEPROM:WRITE: data_len = %d, pages = %d, remainder = %d\r\n", len, page, remainder);
#endif
	
	if(len < EEPROM_PAGE_SIZE) // data length < EEPROM_PAGE_SIZE (16-bytes)
	{
		ret_len += EE24AAXX_WritePage(addr, buf, len);
	}
	else
	{
		for(i = 0; i < page; i++)
		{
			write_ptr = i * EEPROM_PAGE_SIZE;
			ret_len += EE24AAXX_WritePage(addr + write_ptr, buf + write_ptr, EEPROM_PAGE_SIZE);
		}
		
		if(remainder > 0)
		{
			write_ptr = page * EEPROM_PAGE_SIZE;
			ret_len += EE24AAXX_WritePage(addr + write_ptr, buf + write_ptr, remainder);
		}
	}
	
	return ret_len;
}

uint8_t EE24AAXX_WritePage(uint16_t addr, uint8_t *buf, uint8_t len)
{
	uint8_t bsb;
	uint8_t block;
	uint8_t i;
	uint8_t ret_len = 0;
	
	block = addr / EEPROM_BLOCK_SIZE;
	if(block >= EEPROM_BLOCK_COUNT) return 0; // failed
	
	bsb = (addr / EEPROM_BLOCK_SIZE) << 1;
	
	if(len > EEPROM_PAGE_SIZE) len = EEPROM_PAGE_SIZE;
	
	__disable_irq(); // Global interrupt disabled
	
	I2C_Start();
	
	if(EE_TYPE > EE24AA16)
	{
		I2C_Send_Byte(0xA0);
		I2C_Wait_Ack();
		I2C_Send_Byte((uint8_t)((addr & 0xFF00) >> 8));
		I2C_Wait_Ack();
		I2C_Send_Byte((uint8_t)(addr & 0x00FF));
		I2C_Wait_Ack();
	}
	else
	{
		I2C_Send_Byte((uint8_t)(0xA0 + bsb));
		I2C_Wait_Ack();
		I2C_Send_Byte((uint8_t)(addr % EEPROM_BLOCK_SIZE));
		I2C_Wait_Ack();
	}

	for(i = 0; i < len; i++)
	{
		I2C_Send_Byte(buf[i]);
		I2C_Wait_Ack();
		ret_len++;
	}
	
	I2C_Stop();
	
	__enable_irq(); // Global interrupt enabled
	
	delay(10);
	
	return ret_len;
}
