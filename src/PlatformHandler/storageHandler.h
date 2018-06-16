#ifndef STORAGEHANDLER_H_
#define STORAGEHANDLER_H_

#include <stdint.h>

//#define _STORAGE_DEBUG_

typedef enum{STORAGE_MAC, STORAGE_CONFIG, STORAGE_APP_MAIN, STORAGE_APP_BACKUP, NETWORK_APP_BACKUP, SERVER_APP_BACKUP, STORAGE_APPBOOT} teDATASTORAGE;

uint32_t read_storage(teDATASTORAGE stype, uint32_t addr, void *data, uint16_t size);
uint32_t write_storage(teDATASTORAGE stype, uint32_t addr, void *data, uint16_t size);
void erase_storage(teDATASTORAGE stype);

#endif /* STORAGEHANDLER_H_ */
