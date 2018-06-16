#ifndef DEVICEHANDLER_H_
#define DEVICEHANDLER_H_

#include <stdint.h>
#include "W7500x_board.h"
#include "storageHandler.h"

/* Debug message enable */
//#define _FWUP_DEBUG_

/* Application Port */
#define DEVICE_SEGCP_PORT			50001	// Search / Setting Port (UDP Broadcast / TCP unicast)
#define DEVICE_FWUP_PORT			50002	// Firmware Update Port
#define DEVICE_DDNS_PORT			3030	// Not 	used

// HTTP Response: Status code
#define STATUS_HTTP_OK				200

/* W7500S2E Application flash memory map */
#define DEVICE_BOOT_SIZE					(28*1024)
#ifdef __USE_APPBACKUP_AREA__
	#define DEVICE_APP_SIZE						(50*1024)
#else
	#define DEVICE_APP_SIZE						(100*1024)
#endif

#define DEVICE_BOOT_ADDR					(FLASH_START_ADDR) // Boot: 28kB
#define	DEVICE_BOOT_BLOCKS					(7)

//#define DEVICE_APP_MAIN_ADDR				(DEVICE_BOOT_ADDR + (BLOCK_SIZE * DEVICE_BOOT_BLOCKS)) // App main: 50kB
// #define DEVICE_APP_MAIN_ADDR				(0x7000)
#define DEVICE_APP_MAIN_ADDR				(DEVICE_BOOT_ADDR + DEVICE_BOOT_SIZE)
#define	DEVICE_APP_MAIN_BLOCKS				(12)
#define	DEVICE_APP_MAIN_REMAIN_SECTORS		(8)

//#define DEVICE_APP_BACKUP_ADDR				(DEVICE_APP_MAIN_ADDR + (BLOCK_SIZE * DEVICE_APP_MAIN_BLOCKS) + (SECT_SIZE * DEVICE_APP_MAIN_REMAIN_SECTORS)) // App backup: 50kB
// #define DEVICE_APP_BACKUP_ADDR			(0x13800)
#define DEVICE_APP_BACKUP_ADDR				(DEVICE_APP_MAIN_ADDR + DEVICE_APP_SIZE)
#define	DEVICE_APP_BACKUP_BLOCKS			(12)
#define	DEVICE_APP_BACKUP_REMAIN_SECTORS	(8)

#define DEVICE_MAC_ADDR						(DAT0_START_ADDR)
#define DEVICE_CONFIG_ADDR					(DAT1_START_ADDR)


/* Defines for firmware update */
#define DEVICE_FWUP_SIZE			DEVICE_APP_SIZE // Firmware size - 50kB MAX
#define DEVICE_FWUP_TIMEOUT			5000 // 5 secs.

#define DEVICE_FWUP_RET_SUCCESS		0x80
#define DEVICE_FWUP_RET_FAILED		0x40
#define DEVICE_FWUP_RET_PROGRESS	0x20
#define DEVICE_FWUP_RET_NONE		0x00


void device_set_factory_default(void);
void device_socket_termination(void);
void device_reboot(void);

uint8_t device_firmware_update(teDATASTORAGE stype); // Firmware update by Configuration tool / Flash to Flash
uint8_t device_appboot_update(void);

// function for timer
void device_timer_msec(void);

#endif /* DEVICEHANDLER_H_ */
