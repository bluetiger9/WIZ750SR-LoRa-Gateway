#include <string.h>
#include "W7500x.h"
#include "common.h"
#include "flashHandler.h"

#ifdef _FLASH_DEBUG_
	#include <stdio.h>
#endif

void erase_flash_sector(uint32_t sector_addr)
{
	uint16_t erase_id = 0;
	uint32_t addr = 0;
	
	if(sector_addr == (sector_addr & FLASH_END_ADDR)) 
	{
		erase_id = IAP_ERAS_SECT;
		addr = sector_addr;
	}
	else if((sector_addr >= DAT0_START_ADDR) && (sector_addr == (sector_addr & DAT0_END_ADDR))) 
	{
		erase_id = IAP_ERAS_DAT0;
		addr = 0;
	}
	else if((sector_addr >= DAT1_START_ADDR) && (sector_addr == (sector_addr & DAT1_END_ADDR))) 
	{
		erase_id = IAP_ERAS_DAT1;
		addr = 0;
	}
	else
	{
#ifdef _FLASH_DEBUG_
		printf(" > FLASH:SECTOR_ERASE_FAILED: [0x%.8x]\r\n", sector_addr);
#endif

		return;
	}
	
	__disable_irq();
	
	DO_IAP(erase_id, addr, 0, 0);
	
	__enable_irq();
	
}

void erase_flash_block(uint32_t block_addr)
{
	if(block_addr != (block_addr & FLASH_END_ADDR))
	{
#ifdef _FLASH_DEBUG_
		printf(" > FLASH:BLOCK_ERASE_FAILED: [0x%.8x]\r\n", block_addr);
#endif
		return; // masking;
	}
	__disable_irq();
	
	 DO_IAP(IAP_ERAS_BLCK, block_addr, 0, 0);
	
	__enable_irq();
	
}

uint32_t write_flash(uint32_t addr, uint8_t * data, uint32_t data_len)
{
	// Invalid memory address range: Data blocks
	if((addr >= DAT0_START_ADDR) && (addr == (addr & DAT1_END_ADDR)))
	{
		if((data_len > SECT_SIZE) || (DAT0_START_ADDR + data_len > DAT1_END_ADDR)) return 0;
	}
	
	// Invalid data_len
	if((data_len == 0) && (addr + data_len > DAT1_END_ADDR)) return 0; // (Max ~ 0x0003FFFF)
	
	 // Invalid memory address range: Information block, Do not access
	if((addr > FLASH_END_ADDR) && (addr < DAT0_START_ADDR)) return 0;
	
	__disable_irq();
	
	DO_IAP(IAP_PROG, addr, data, data_len);
	
	__enable_irq();
	
	return data_len;
}

uint32_t read_flash(uint32_t addr, uint8_t *data, uint32_t data_len)
{
	int32_t i;

	for(i = 0; i < data_len; i++)
	{
		data[i] = *(uint8_t *) (addr + i);
	}

	return i;
}


void Copy_Interrupt_VectorTable(uint32_t start_addr, uint8_t * vectortable)
{
    uint32_t i;
    uint8_t flash_vector_area[SECT_SIZE];

    if(vectortable != NULL)
    {
        for (i = 0x00; i < 0x08; i++)			flash_vector_area[i] = vectortable[i];
        for (i = 0x08; i < 0xA8; i++) 			flash_vector_area[i] = *(volatile uint8_t *)(start_addr+i); // Actual address range; Interrupt vector table is located here
        for (i = 0xA8; i < SECT_SIZE; i++)	flash_vector_area[i] = vectortable[i];
    }
    else
    {
        for (i = 0x00; i < 0x08; i++)			flash_vector_area[i] = *(volatile uint8_t *)(0x00000000+i);
        for (i = 0x08; i < 0xA8; i++) 			flash_vector_area[i] = *(volatile uint8_t *)(start_addr+i); // Actual address range; Interrupt vector table is located here
        for (i = 0xA8; i < SECT_SIZE; i++)	flash_vector_area[i] = *(volatile uint8_t *)(0x00000000+i);
    }


    __disable_irq();

    DO_IAP(IAP_ERAS_SECT, 0x00000000, 0, 0); 						// Erase the interrupt vector table area : Sector 0
    DO_IAP(IAP_PROG, 0x00000000, flash_vector_area , SECT_SIZE);	// Write the applicaion vector table to 0x00000000

    __enable_irq(); 
}


/**
  * @brief  DO IAP Function
  */
void DO_IAP( uint32_t id, uint32_t dst_addr, uint8_t* src_addr, uint32_t size)
{
	uint32_t temp_interrupt;

	// Backup Interrupt Set Pending Register
	temp_interrupt = (NVIC->ISPR[0]);
	(NVIC->ISPR[0]) = (uint32_t)0xFFFFFFFF;

	// Call IAP Function
	((void(*)(uint32_t,uint32_t,uint8_t*,uint32_t))IAP_ENTRY)( id,dst_addr,src_addr,size);

	// Restore Interrupt Set Pending Register
	(NVIC->ISPR[0]) = temp_interrupt;
}

