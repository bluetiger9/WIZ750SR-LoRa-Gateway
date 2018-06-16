
#ifndef __EEPROMHANDLER_H__
#define __EEPROMHANDLER_H__

#include <stdint.h>
#include "W7500x_board.h"

//#define _EEPROM_DEBUG_

#define EE24AA01  128
#define EE24AA02  256
#define EE24AA04  512
#define EE24AA08  1024
#define EE24AA16  2048
#define EE24AA32  4096
#define EE24AA64  8192
#define EE24AA128 16384
#define EE24AA256 32768

#if (DEVICE_BOARD_NAME == WIZwiki_W7500ECO)
	#define EE_TYPE		EE24AA04
#else
	#define EE_TYPE		EE24AA16
#endif

#if (EE_TYPE > EE24AA16)
	#define EEPROM_BLOCK_SIZE		256
	#define EEPROM_PAGE_SIZE		32
#else
	#define EEPROM_BLOCK_SIZE		256
	#define EEPROM_PAGE_SIZE		16
	#define EEPROM_BLOCK_COUNT		(EE_TYPE / EEPROM_BLOCK_SIZE) // 2(24AA04), 4(24AA08), 8(24AA16)
#endif

void init_eeprom(void);
uint16_t read_eeprom(uint16_t addr, uint8_t * data, uint16_t data_len);
uint16_t write_eeprom(uint16_t addr, uint8_t * data, uint16_t data_len);
void erase_eeprom_block(uint16_t addr);

#ifdef _EEPROM_DEBUG_
	void dump_eeprom_block(uint16_t addr);
#endif

#endif
