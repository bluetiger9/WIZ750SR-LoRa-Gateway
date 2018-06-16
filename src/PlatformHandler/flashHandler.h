#ifndef __FLASHHANDLER_H__
#define __FLASHHANDLER_H__

#include <stdint.h>
//#define _FLASH_DEBUG_

/* W7500 IAP IDs */
#define IAP_ENTRY			(0x1FFF1001) // Because Thum code
#define IAP_ERAS			(0x010)
#define IAP_ERAS_DAT0		(IAP_ERAS + 0)
#define IAP_ERAS_DAT1		(IAP_ERAS + 1)
#define IAP_ERAS_SECT		(IAP_ERAS + 2)
#define IAP_ERAS_BLCK		(IAP_ERAS + 3)
#define IAP_ERAS_CHIP		(IAP_ERAS + 4)
#define IAP_ERAS_MASS		(IAP_ERAS + 5)
#define IAP_PROG			(0x022)
#define SECT_SIZE			(0x0100) // 256 bytes
#define BLOCK_SIZE			(0x1000) // 4096 bytes

/* W7500 IAP: Flash memory map */
// Main flash size: 128kB (512 Sectors == 32 Blocks)
#define FLASH_START_ADDR	0x00000000
#define FLASH_END_ADDR		0x0001FFFF

// Data flash 0 size: 256bytes (1 Sector)
#define DAT0_START_ADDR		0x0003FE00
#define DAT0_END_ADDR		0x0003FEFF

// Data flash 1 size: 256bytes (1 Sector)
#define DAT1_START_ADDR		0x0003FF00
#define DAT1_END_ADDR		0x0003FFFF


void erase_flash_sector(uint32_t sector_addr);
void erase_flash_block(uint32_t block_addr);
uint32_t write_flash(uint32_t addr, uint8_t *data, uint32_t data_len);
uint32_t read_flash(uint32_t addr, uint8_t *data, uint32_t data_len);
void Copy_Interrupt_VectorTable(uint32_t start_addr, uint8_t * vectortable);

void DO_IAP( uint32_t id, uint32_t dst_addr, uint8_t* src_addr, uint32_t size);

#endif
