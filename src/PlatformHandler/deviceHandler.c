
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "ConfigData.h"
#include "wizchip_conf.h"
#include "W7500x_wztoe.h"
#include "socket.h"
#include "seg.h"
#include "segcp.h"
#include "flashHandler.h"
#include "storageHandler.h"
#include "deviceHandler.h"
#include "uartHandler.h"
#include "timerHandler.h"
#include "util.h"

#include "dns.h"

uint16_t get_firmware_from_network(uint8_t sock, uint8_t * buf);
uint16_t get_firmware_from_server(uint8_t sock, uint8_t * server_ip, uint8_t * buf);
uint16_t gen_http_fw_request(uint8_t * buf);
int8_t process_dns_fw_server(uint8_t * domain_ip, uint8_t * buf);

void reset_fw_update_timer(void);
uint16_t get_any_port(void);

uint8_t enable_fw_update_timer = SEGCP_DISABLE;
volatile uint16_t fw_update_time = 0;
uint8_t flag_fw_update_timeout = SEGCP_DISABLE;
	
uint8_t enable_fw_from_network_timer = SEGCP_DISABLE;
volatile uint16_t fw_from_network_time = 0;
uint8_t flag_fw_from_network_timeout = SEGCP_DISABLE;

uint8_t flag_fw_from_server_failed = SEGCP_DISABLE;
static uint16_t any_port = 0;

extern uint8_t g_recv_buf[DATA_BUF_SIZE];


void device_set_factory_default(void)
{
	set_DevConfig_to_factory_value();
	save_DevConfig_to_storage();
}


void device_socket_termination(void)
{
	uint8_t i;
	for(i = 0; i < _WIZCHIP_SOCK_NUM_; i++)
	{
		process_socket_termination(i);
	}	
}


void device_reboot(void)
{
	device_socket_termination();
	
	clear_data_transfer_bytecount(SEG_ALL);
	
	NVIC_SystemReset();
	while(1);
}


#ifdef __USE_APPBACKUP_AREA__ // 50kB App + 50kB App-backup mode

	// if(stype == STORAGE_APP_BACKUP)	>>> Config tool -> STORAGE_APP_BACKUP (App, network to flash update)
	// if(stype == STORAGE_APP_MAIN) 	>>> STORAGE_APP_BACKUP -> STORAGE_APP_MAIN (Boot, flash to flash update)

uint8_t device_firmware_update(teDATASTORAGE stype)
{
	struct __firmware_update *fwupdate = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	
	struct __firmware_update_extend *fwupdate_server = (struct __firmware_update_extend *)&(get_DevConfig_pointer()->firmware_update_extend);
	uint8_t server_ip[4] = {0, };
	
	uint8_t ret = DEVICE_FWUP_RET_PROGRESS; // No Meaning, [Firmware update process] have to work as blocking function.
	uint16_t recv_len = 0;
	uint16_t write_len = 0;
	static uint32_t write_fw_len;
	
	if(stype != SERVER_APP_BACKUP) 
	{
		if(fwupdate->fwup_flag == SEGCP_DISABLE)	return DEVICE_FWUP_RET_FAILED;
		if((fwupdate->fwup_size == 0) || (fwupdate->fwup_size > DEVICE_FWUP_SIZE))
		{
			if(serial->serial_debug_en)
			{
				printf(" > SEGCP:FW_UPDATE:FAILED - Invalid firmware size: %d bytes (Firmware size must be within %d bytes)\r\n", fwupdate->fwup_size, DEVICE_FWUP_SIZE);
			}

			return DEVICE_FWUP_RET_FAILED;
		}
	}
	else // styep == SERVER_APP_BACKUP, HTTP server -> Flash
	{
		if(fwupdate->fwup_flag == SEGCP_DISABLE)	return DEVICE_FWUP_RET_FAILED;
		if(fwupdate_server->fwup_server_flag == SEGCP_DISABLE)	return DEVICE_FWUP_RET_FAILED;
		if(fwupdate_server->fwup_server_port == 0)				return DEVICE_FWUP_RET_FAILED;
		
		// DNS Query to Firmware update server
		if(process_dns_fw_server(server_ip, g_recv_buf) != SEGCP_ENABLE)
		{
			if(serial->serial_debug_en)
			{
				printf(" > SEGCP:FW_UPDATE:FAILED - DNS failed\r\n");
			}
			return DEVICE_FWUP_RET_FAILED;
		}
		
		// Update start
		fwupdate->fwup_size = DEVICE_APP_SIZE; // Update the Temporary firmware size: Firmware update by HTTP server, fw size can be get by HTTP response from the server
		if(serial->serial_debug_en)
		{
			printf(" > SEGCP:FW_UPDATE:UPDATE_SERVER - %s%s\r\n", FWUP_SERVER_DOMAIN, FWUP_SERVER_BINPATH);
			//printf(" > SEGCP:FW_UPDATE:UPDATE_SERVER - %s%s\r\n", fwupdate_server->fwup_server_domain, fwupdate_server->fwup_server_binpath);
		}
	}
		
		
	// App, FW update from Network (ethernet) to Flash memory (backup area)
	if((stype == NETWORK_APP_BACKUP) || (stype == SERVER_APP_BACKUP))
	{
		if(serial->serial_debug_en)
		{
			if(stype == NETWORK_APP_BACKUP) printf(" > SEGCP:FW_UPDATE:NETWORK - Firmware size: [%d] bytes\r\n", fwupdate->fwup_size);
		}

		write_fw_len = 0;
		erase_storage(STORAGE_APP_BACKUP); // Erase application backup blocks
		
		// init firmware update timer
		enable_fw_update_timer = SEGCP_ENABLE;
		
		do 
		{
/////////////////////////////////////////////////////////////////////////////////////////////////////////
			//recv_len = get_firmware_from_network(SOCK_FWUPDATE, (uint8_t *)&g_recv_buf);
			if(stype == NETWORK_APP_BACKUP) 		recv_len = get_firmware_from_network(SOCK_FWUPDATE, g_recv_buf);
			else if(stype == SERVER_APP_BACKUP)		recv_len = get_firmware_from_server(SOCK_FWUPDATE, server_ip, g_recv_buf);
			
			if(recv_len > 0)
			{
				write_len = write_storage(STORAGE_APP_BACKUP, (DEVICE_APP_BACKUP_ADDR + write_fw_len), g_recv_buf, recv_len);
				write_fw_len += write_len;
				fw_update_time = 0; // Reset fw update timeout counter
			}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
			
			// Firmware update failed: Timeout occurred
			if(flag_fw_update_timeout == SEGCP_ENABLE)
			{
				if(serial->serial_debug_en) printf(" > SEGCP:FW_UPDATE:FAILED - Firmware update timeout\r\n");
				ret = DEVICE_FWUP_RET_FAILED;
				break;
			}
			
			// Firmware update failed: timeout occurred at get_firmware_from_network() function
			if(flag_fw_from_network_timeout == SEGCP_ENABLE)
			{
				if(serial->serial_debug_en) printf(" > SEGCP:FW_UPDATE:FAILED - Network download timeout\r\n");
				ret = DEVICE_FWUP_RET_FAILED;
				break;
			}
			
			// Firmware update failed: invalid HTTP status code from server
			if(flag_fw_from_server_failed == SEGCP_ENABLE)
			{
				if(serial->serial_debug_en) printf(" > SEGCP:FW_UPDATE:FAILED - Invalid HTTP Status code response\r\n");
				ret = DEVICE_FWUP_RET_FAILED;
				break;
			}
		} while(write_fw_len < fwupdate->fwup_size);
		
		if(write_fw_len == fwupdate->fwup_size)
		{
			if(serial->serial_debug_en)
			{
				printf(" > SEGCP:FW_UPDATE:SUCCESS - %d / %d bytes\r\n", write_fw_len, fwupdate->fwup_size);
			}
			ret = DEVICE_FWUP_RET_SUCCESS;
		}
	}
	// Boot, FW update from Flash memory (backup area) to Flash memory (main application area)
	// Boot, FW update from Flash memory (main application area) to Flash memory (backup area)
	else if((stype == STORAGE_APP_MAIN) || (stype == STORAGE_APP_BACKUP)) // Run this code in the boot area only, stype means target storage
	{
		; // Operation code in BOOT field
	}
	else
	{
		ret = DEVICE_FWUP_RET_FAILED;
	}
	
	reset_fw_update_timer();
	
	return ret;
}

#else // 100kB App mode

uint8_t device_firmware_update(teDATASTORAGE stype)
{
	struct __firmware_update *fwupdate = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	
	uint8_t ret = DEVICE_FWUP_RET_PROGRESS; // No Meaning, [Firmware update process] have to work as blocking function.
	
	uint16_t recv_len = 0;
	uint16_t write_len = 0;
	static uint32_t write_fw_len;
	
	if(fwupdate->fwup_flag == SEGCP_DISABLE)	return DEVICE_FWUP_RET_FAILED;
	if((fwupdate->fwup_size == 0) || (fwupdate->fwup_size > DEVICE_FWUP_SIZE))
	{
		if(serial->serial_debug_en)
		{
			printf(" > SEGCP:FW_UPDATE:FAILED - Invalid firmware size: %d bytes (Firmware size must be within %d bytes)\r\n", fwupdate->fwup_size, DEVICE_FWUP_SIZE);
		}

		return DEVICE_FWUP_RET_FAILED;
	}
	
	// App, FW update from Network (ethernet) to Flash memory (backup area)
	if(stype == STORAGE_APP_MAIN)
	{
		if(serial->serial_debug_en)
		{
			printf(" > SEGCP:FW_UPDATE:NETWORK - Firmware size: [%d] bytes\r\n", fwupdate->fwup_size);
		}

		write_fw_len = 0;
		erase_storage(STORAGE_APP_MAIN); // Erase application blocks
		erase_storage(STORAGE_APP_BACKUP); // Erase application backup blocks
		
		// init firmware update timer
		enable_fw_update_timer = SEGCP_ENABLE;
		
		do 
		{
/////////////////////////////////////////////////////////////////////////////////////////////////////////
			recv_len = get_firmware_from_network(SOCK_FWUPDATE, g_recv_buf);
			
			if(recv_len > 0)
			{
				write_len = write_storage(STORAGE_APP_MAIN, (DEVICE_APP_MAIN_ADDR + write_fw_len), g_recv_buf, recv_len);
				write_fw_len += write_len;
				fw_update_time = 0; // Reset fw update timeout counter
			}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
			
			// Firmware update failed: Timeout occurred
			if(flag_fw_update_timeout == SEGCP_ENABLE)
			{
				if(serial->serial_debug_en) printf(" > SEGCP:FW_UPDATE:FAILED - Firmware update timeout\r\n");
				ret = DEVICE_FWUP_RET_FAILED;
				break;
			}
			
			// Firmware update failed: timeout occurred at get_firmware_from_network() function
			if(flag_fw_from_network_timeout == SEGCP_ENABLE)
			{
				if(serial->serial_debug_en) printf(" > SEGCP:FW_UPDATE:FAILED - Network download timeout\r\n");
				ret = DEVICE_FWUP_RET_FAILED;
				break;
			}
			
		} while(write_fw_len < fwupdate->fwup_size);
	}	
	else if((stype == NETWORK_APP_BACKUP) || stype == SERVER_APP_BACKUP) // Run this code in the boot area only
	{
		printf(" > SEGCP:FW_UPDATE:FAILED - Please try to FW Update at APPBOOT mode\r\n");
		
		return DEVICE_FWUP_RET_FAILED;
	}
	else
	{
		ret = DEVICE_FWUP_RET_FAILED;
	}
	
	// shared code
	if(write_fw_len == fwupdate->fwup_size)
	{
		if(serial->serial_debug_en)
		{
			printf(" > SEGCP:FW_UPDATE:SUCCESS - %d / %d bytes\r\n", write_fw_len, fwupdate->fwup_size);
		}
		ret = DEVICE_FWUP_RET_SUCCESS;
	}
	
	reset_fw_update_timer();
	
	return ret;
}

#endif


uint8_t device_appboot_update(void)
{
    struct __firmware_update *fwupdate = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
    struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);

    uint8_t ret = DEVICE_FWUP_RET_PROGRESS;
    uint16_t recv_len = 0;
    uint16_t write_len = 0;
    static uint32_t write_fw_len;
    
    uint16_t i;
    uint8_t vectortable[SECT_SIZE] = {0xff, };

    if((fwupdate->fwup_size == 0) || (fwupdate->fwup_size > DEVICE_BOOT_SIZE))
    {
        if(serial->serial_debug_en) printf(" > SEGCP:BU_UPDATE:FAILED - Invalid firmware size: %d bytes (Firmware size must be within %d bytes)\r\n", fwupdate->fwup_size, DEVICE_BOOT_SIZE);
        return DEVICE_FWUP_RET_FAILED;
    }

    if(serial->serial_debug_en) printf(" > SEGCP:BU_UPDATE:NETWORK - Firmware size: [%d] bytes\r\n", fwupdate->fwup_size);

    write_fw_len = 0;
    erase_storage(STORAGE_APPBOOT); // Erase flash blocks
    
    // init firmware update timer
    enable_fw_update_timer = SEGCP_ENABLE;
    
    do 
    {
        recv_len = get_firmware_from_network(SOCK_FWUPDATE, g_recv_buf);
        if(recv_len > 0)
        {
            if(write_fw_len == 0) // Update 'AppBoot' area except application's interrupt vector table
            {
                for(i = 0; i < SECT_SIZE; i++) {vectortable[i] = g_recv_buf[i];}
                write_len = write_storage(STORAGE_APPBOOT, (DEVICE_BOOT_ADDR + write_fw_len + SECT_SIZE), (g_recv_buf + SECT_SIZE), (recv_len - SECT_SIZE));
                write_len += SECT_SIZE;
            }
            else
            {
                write_len = write_storage(STORAGE_APPBOOT, (DEVICE_BOOT_ADDR + write_fw_len), g_recv_buf, recv_len);
            }
            
            write_fw_len += write_len;
            fw_update_time = 0; // Reset fw update timeout counter
        }
        
        // Firmware update failed: Timeout occurred
        if(flag_fw_update_timeout == SEGCP_ENABLE)
        {
            if(serial->serial_debug_en) printf(" > SEGCP:BU_UPDATE:FAILED - Firmware update timeout\r\n");
            ret = DEVICE_FWUP_RET_FAILED;
            break;
        }
        
        // Firmware update failed: timeout occurred at get_firmware_from_network() function
        if(flag_fw_from_network_timeout == SEGCP_ENABLE)
        {
            if(serial->serial_debug_en) printf(" > SEGCP:BU_UPDATE:FAILED - Network download timeout\r\n");
            ret = DEVICE_FWUP_RET_FAILED;
            break;
        }
    } while(write_fw_len < fwupdate->fwup_size);
    
    if(write_fw_len == fwupdate->fwup_size)
    {
        Copy_Interrupt_VectorTable(DEVICE_APP_MAIN_ADDR, vectortable);
        
        if(serial->serial_debug_en) printf(" > SEGCP:BU_UPDATE:SUCCESS - %d / %d bytes\r\n", write_fw_len, fwupdate->fwup_size);
        ret = DEVICE_FWUP_RET_SUCCESS;
    }

    reset_fw_update_timer();

    return ret;
}

uint16_t get_firmware_from_network(uint8_t sock, uint8_t * buf)
{
	struct __firmware_update *fwupdate = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
	uint8_t len_buf[2] = {0, };
	uint16_t len = 0;
	uint8_t state = getSn_SR(sock);
	
	static uint32_t recv_fwsize;
	
	switch(state)
	{
		case SOCK_INIT:
			//listen(sock);
			break;
		
		case SOCK_LISTEN:
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				// init network firmware update timer
				enable_fw_from_network_timer = SEGCP_ENABLE;
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Timeout occurred
			if(flag_fw_from_network_timeout == SEGCP_ENABLE)
			{
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:UPDATE:NET_TIMEOUT\r\n");
#endif
				//disconnect(sock);
				close(sock);
				return 0;
			}
			
			// DATA_BUF_SIZE
			if((len = getSn_RX_RSR(sock)) > 0)
			{
				if(len > DATA_BUF_SIZE) len = DATA_BUF_SIZE;
				if(recv_fwsize + len > fwupdate->fwup_size) len = fwupdate->fwup_size - recv_fwsize; // remain
				
				len = recv(sock, buf, len);
				recv_fwsize += len;
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:UPDATE:RECV_LEN - %d bytes | [%d] bytes\r\n", len, recv_fwsize);
#endif
				// Send ACK - receviced length - to configuration tool
				len_buf[0] = (uint8_t)((0xff00 & len) >> 8); // endian-independent code: Datatype translation, byte order regardless
				len_buf[1] = (uint8_t)(0x00ff & len);
				
				send(sock, len_buf, 2);
				
				fw_from_network_time = 0;
				
				if(recv_fwsize >= fwupdate->fwup_size)
				{
#ifdef _FWUP_DEBUG_
					printf(" > SEGCP:UPDATE:NETWORK - UPDATE END | [%d] bytes\r\n", recv_fwsize);
#endif
					// timer disable: network timeout
					reset_fw_update_timer();
					
					// socket close
					disconnect(sock);
				}
			}
			break;
			
		case SOCK_CLOSE_WAIT:
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			if(socket(sock, Sn_MR_TCP, DEVICE_FWUP_PORT, SF_TCP_NODELAY) == sock)
			{
				recv_fwsize = 0;
				listen(sock);
				
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:UPDATE:SOCKOPEN\r\n");
#endif
			}
			break;
			
		default:
			break;
	}
	
	return len;
}

uint16_t get_firmware_from_server(uint8_t sock, uint8_t * server_ip, uint8_t * buf)
{
	struct __firmware_update *fwupdate = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
	struct __firmware_update_extend *fwupdate_server = (struct __firmware_update_extend *)&(get_DevConfig_pointer()->firmware_update_extend);
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	
	uint16_t len = 0;
	uint8_t state = getSn_SR(sock);
	
	uint16_t src_port;
	uint8_t dest_ip[4] = {0, };
	uint16_t dest_port = 0;
	
	uint8_t tmp_buf[10]={0x00, };
	uint8_t * buf_ptr;
	uint8_t * data_ptr;
	uint32_t status_code, content_len = 0;
	uint16_t data_len, header_len = 0;
	
	uint16_t i;
	static uint8_t isHTTPHeaderParsed;
	
	static uint32_t recv_fwsize;
	
	switch(state)
	{
		case SOCK_INIT:
#ifdef _FWUP_DEBUG_
			printf(" > SEGCP:FW_UPDATE:CLIENT_CONNECTING to ");
			printf("%d.%d.%d.%d : %d\r\n", server_ip[0], server_ip[1], server_ip[2], server_ip[3], fwupdate_server->fwup_server_port);
#endif
			connect(sock, server_ip, fwupdate_server->fwup_server_port);
			break;

		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				if(serial->serial_debug_en)
				{
					 getSn_DIPR(sock, dest_ip);
					 dest_port = getSn_DPORT(sock);
					 printf(" > SEGCP:FW_UPDATE:UPDATE_SERVER_CONNECTED - %d.%d.%d.%d : %d\r\n", dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3], dest_port);
				}
				
				// Init network firmware update timer
				enable_fw_from_network_timer = SEGCP_ENABLE;
				
				// Send the HTTP Request
				isHTTPHeaderParsed = 0; // flag init
				len = gen_http_fw_request(buf);
				send(sock, buf, len);
				
				// Connection Interrupt Clear
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Timeout occurred
			if(flag_fw_from_network_timeout == SEGCP_ENABLE)
			{
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:FW_UPDATE:NET_TIMEOUT\r\n");
#endif
				close(sock);
				return 0;
			}
			
			// DATA_BUF_SIZE
			if((len = getSn_RX_RSR(sock)) > 0)
			{
				if(len > DATA_BUF_SIZE) len = DATA_BUF_SIZE;
				if(recv_fwsize + len > fwupdate->fwup_size) len = fwupdate->fwup_size - recv_fwsize; // remain
				
				len = recv(sock, buf, len);
				
				if(isHTTPHeaderParsed == 0) // HTTP header parsing
				{
					buf_ptr = buf;
					_mid((char *)buf, "HTTP/1.1 ", " ", (char *)tmp_buf);
					status_code = atoi((char *)tmp_buf);
					if(status_code != STATUS_HTTP_OK)
					{
						flag_fw_from_server_failed = SEGCP_ENABLE;
#ifdef _FWUP_DEBUG_
						printf(" > SEGCP:FW_UPDATE:FAILED - HTTP_STATUS_CODE [%d]\r\n", status_code);
#endif
						close(sock);
						return 0; // HTTP Request failed
					}
#ifdef _FWUP_DEBUG_
					else printf(" > SEGCP:FW_UPDATE:HTTP/1.1 200 OK\r\n");
#endif
					_mid((char *)buf, "Content-Length: ", "\r\n", (char *)tmp_buf);
					content_len = atol((char *)tmp_buf);
					
					// Updated firmware sizes need to be updated
					fwupdate->fwup_size = content_len;

#ifdef _FWUP_DEBUG_
					printf(" > SEGCP:FW_UPDATE:HTTP Content-Length: %d\r\n", content_len);
#endif
					
					data_ptr = (uint8_t *)strstr((char *)buf, "\r\n\r\n");
					data_ptr += 4;
					
					// Data length update: whole packet length - http header length = data length
					header_len = data_ptr - buf_ptr;
#ifdef _FWUP_DEBUG_
					printf("\r\n>> HTTP Response\r\n");
					printf("======================================================\r\n");
					for(i = 0; i < header_len; i++) printf("%c", buf[i]);
					printf("======================================================\r\n");
#endif
					data_len = len - header_len;
					len = data_len;
					for(i = 0; i < len; i++) buf[i] = buf[header_len+i];
					
					isHTTPHeaderParsed = 1;
				}
				
				// Update the received firmware size
				recv_fwsize += len;
				
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:FW_UPDATE:RECV_LEN - %d bytes | [%d] bytes\r\n", len, recv_fwsize);
#endif
				
				fw_from_network_time = 0;
				
				if(recv_fwsize >= fwupdate->fwup_size)
				{
#ifdef _FWUP_DEBUG_
					printf(" > SEGCP:FW_UPDATE:SERVER - UPDATE END | [%d] bytes\r\n", recv_fwsize);
#endif
					// timer disable: network timeout
					reset_fw_update_timer();
					
					// socket close
					close(sock);
				}
			}
			break;
			
		case SOCK_CLOSE_WAIT:
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			src_port = get_any_port();
			if(socket(sock, Sn_MR_TCP, src_port, SF_TCP_NODELAY) == sock)
			{
				recv_fwsize = 0;
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:FW_UPDATE:CLIENT_SOCKOPEN\r\n");
#endif
			}
			break;
			
		default:
			break;
	}
	
	return len;
}

int8_t process_dns_fw_server(uint8_t * fw_remote_ip, uint8_t * buf)
{
	struct __firmware_update_extend *fwupdate_server = (struct __firmware_update_extend *)&(get_DevConfig_pointer()->firmware_update_extend);
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	
	int8_t ret = 0;
	uint8_t dns_retry = 0;
	
#ifdef _FWUP_DEBUG_
	printf(" - DNS Client running: FW update server\r\n");
#endif
	
	DNS_init(SOCK_DNS, buf);
	
	while(1) 
	{
		if((ret = DNS_run(option->dns_server_ip, (uint8_t *)FWUP_SERVER_DOMAIN, (uint8_t *)fw_remote_ip)) == 1)
		{
#ifdef _FWUP_DEBUG_
			printf(" - DNS Success: Firmware Server IP is %d.%d.%d.%d\r\n", fw_remote_ip[0], fw_remote_ip[1], fw_remote_ip[2], fw_remote_ip[3]);
#endif
			break;
		}
		else
		{
			dns_retry++;
#ifdef _FWUP_DEBUG_
			if(dns_retry <= 2) printf(" - DNS Timeout occurred and retry [%d]\r\n", dns_retry);
#endif
		}

		if(dns_retry > 2) {
#ifdef _FWUP_DEBUG_
			printf(" - DNS Failed\r\n\r\n");
#endif
			break;
		}
	}
	
	return ret;
}

uint16_t gen_http_fw_request(uint8_t * buf)
{
	uint16_t len;
	
	// Make the HTTP message packet for Firmware request
	// HTTP Server will responded HTTP response message includes FW (octet-stream) when FW request packet received.
	memset(buf, 0x00, DATA_BUF_SIZE);
	len = sprintf((char *)buf, "GET %s HTTP/1.1\r\n", FWUP_SERVER_BINPATH);
	len += sprintf((char *)buf+len, "Host: %s\r\n", FWUP_SERVER_DOMAIN);
	len += sprintf((char *)buf+len, "Connection: keep-alive\r\n");
	len += sprintf((char *)buf+len, "\r\n");
	
#ifdef _FWUP_DEBUG_
	printf("\r\n>> HTTP Request Packet length: %d\r\n", len);
	printf("======================================================\r\n");
	printf("%s", buf);
	printf("======================================================\r\n");
#endif

	return len;
}

// function for timer
void device_timer_msec(void)
{
	if(enable_fw_update_timer == SEGCP_ENABLE)
	{
		if(fw_update_time < 0xFFFF) 	fw_update_time++;
		else							fw_update_time = 0;
		
		if(fw_update_time >= DEVICE_FWUP_TIMEOUT)
		{
			flag_fw_update_timeout = SEGCP_ENABLE;
			fw_update_time = 0;
		}
	}
	
	if(enable_fw_from_network_timer == SEGCP_ENABLE)
	{
		if(fw_from_network_time < 0xFFFF)	fw_from_network_time++;
		else								fw_from_network_time = 0;
		
		if(fw_from_network_time >= DEVICE_FWUP_TIMEOUT)
		{
			flag_fw_from_network_timeout = SEGCP_ENABLE;
			fw_from_network_time = 0;
		}
	}
}

void reset_fw_update_timer(void)
{
	// Enables
	enable_fw_update_timer = SEGCP_DISABLE;
	enable_fw_from_network_timer = SEGCP_DISABLE;
	
	// Counters
	fw_update_time = 0;
	fw_from_network_time = 0;
	
	// Flags
	flag_fw_update_timeout = SEGCP_DISABLE;
	flag_fw_from_network_timeout = SEGCP_DISABLE;
}

uint16_t get_any_port(void)
{
	if(any_port)
	{
		if(any_port < 0xffff) 	any_port++;
		else					any_port = 0;
	}
	
	if(any_port == 0) any_port = 50001;
	
	return any_port;
}

