#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "W7500x_gpio.h"

#include "common.h"
#include "W7500x_board.h"
#include "W7500x_wztoe.h"
#include "socket.h"
#include "ConfigData.h"
#include "storageHandler.h"
#include "deviceHandler.h"

#include "seg.h"
#include "segcp.h"
#include "util.h"
#include "uartHandler.h"
#include "gpioHandler.h"
#include "timerHandler.h"

/* Private define ------------------------------------------------------------*/
// Ring Buffer declaration
BUFFER_DECLARATION(data_rx);

/* Private functions ---------------------------------------------------------*/
uint16_t uart_get_commandline(uint8_t uartNum, uint8_t* buf, uint16_t maxSize);

/* Private variables ---------------------------------------------------------*/
static uint8_t gSEGCPREQ[CONFIG_BUF_SIZE];
static uint8_t gSEGCPREP[CONFIG_BUF_SIZE];

uint8_t * strDEVSTATUS[]  = {"BOOT", "OPEN", "CONNECT", "UPGRADE", "ATMODE", "UDP", 0};

// [K!]: Hidden command, Erase the MAC address and configuration data
uint8_t * tbSEGCPCMD[] = {"MC", "VR", "MN", "IM", "OP", "DD", "CP", "PO", "DG", "KA", 
                          "KI", "KE", "RI", "LI", "SM", "GW", "DS", "PI", "PP", "DX",
                          "DP", "DI", "DW", "DH", "LP", "RP", "RH", "BR", "DB", "PR",
                          "SB", "FL", "IT", "PT", "PS", "PD", "TE", "SS", "NP", "SP",
                          "LG", "ER", "FW", "MA", "PW", "SV", "EX", "RT", "UN", "ST",
                          "FR", "EC", "K!", "UE", "GA", "GB", "GC", "GD", "CA", "CB", 
                          "CC", "CD", "SC", "S0", "S1", "RX", "FS", "FC", "FP", "FD",
                          "FH", "UI", "AB", "TR", "BU", 0};
                            
uint8_t * tbSEGCPERR[] = {"ERNULL", "ERNOTAVAIL", "ERNOPARAM", "ERIGNORED", "ERNOCOMMAND", "ERINVALIDPARAM", "ERNOPRIVILEGE"};

uint8_t gSEGCPPRIVILEGE = SEGCP_PRIVILEGE_CLR;

// Keep-alive timer values for TCP unicast search function
uint8_t enable_configtool_keepalive_timer = SEGCP_DISABLE;
volatile uint16_t configtool_keepalive_time = 0;
uint8_t flag_send_configtool_keepalive = SEGCP_DISABLE;

extern uint8_t tmp_timeflag_for_debug;

static uint32_t temp_interrupt;
static void flash_update_start(void);
static void flash_update_end(void);

void do_segcp(void)
{
    DevConfig *dev_config = get_DevConfig_pointer();

    uint8_t ret = 0;
    uint16_t segcp_ret = 0;
    
    teDEVSTATUS status_bak;

    // Process the serial AT command mode
    if(opmode == DEVICE_AT_MODE)
    {
        segcp_ret = proc_SEGCP_uart(gSEGCPREP);
        if(segcp_ret & SEGCP_RET_ERR)
        {
            if(dev_config->serial_info[0].serial_debug_en) printf(" > SEGCP:ERROR:%04X\r\n", segcp_ret);
        }
    }
    
    segcp_ret |= proc_SEGCP_udp(gSEGCPREQ, gSEGCPREP);
    segcp_ret |= proc_SEGCP_tcp(gSEGCPREQ, gSEGCPREP);
    
    if(segcp_ret && ((segcp_ret & SEGCP_RET_ERR) != SEGCP_RET_ERR)) // Command parsing success
    {
        if(segcp_ret & SEGCP_RET_SWITCH)
        {
            if(opmode == DEVICE_GW_MODE) 		init_trigger_modeswitch(DEVICE_AT_MODE); // DEVICE_GW_MODE -> DEVICE_AT_MODE
            else 								init_trigger_modeswitch(DEVICE_GW_MODE); // DEVICE_AT_MODE -> DEVICE_GW_MODE
        }
    
        if(segcp_ret & SEGCP_RET_FACTORY)
        {
            flash_update_start();
            device_set_factory_default();
            flash_update_end();
        }
        else if(segcp_ret & SEGCP_RET_SAVE)
        {
            flash_update_start();
            save_DevConfig_to_storage();
            flash_update_end();
        }
        else if(segcp_ret & SEGCP_RET_ERASE_EEPROM)
        {
#ifdef _SEGCP_DEBUG_
                printf(">> Erase success\r\n");
                printf("\r\n");
#endif
                erase_storage(STORAGE_MAC);
                erase_storage(STORAGE_CONFIG);
        }
    
        if(segcp_ret & SEGCP_RET_FWUP)
        {
            flash_update_start();
            
            /* DualTimer Re-Initialization */
            //Timer_Configuration();
            
            status_bak = (teDEVSTATUS)get_device_status();
            set_device_status(ST_UPGRADE);
            
            if((segcp_ret & SEGCP_RET_FWUP) == segcp_ret)               ret = device_firmware_update(NETWORK_APP_BACKUP); // Firmware update by Configuration tool
#ifdef __USE_APPBACKUP_AREA__
            // 'Firmware update via Web Server' function supports '__USE_APPBACKUP_AREA__' mode only
            else if((segcp_ret & SEGCP_RET_FWUP_SERVER) == segcp_ret)   ret = device_firmware_update(SERVER_APP_BACKUP); // or Firmware update by HTTP Server
#endif
            else if((segcp_ret & SEGCP_RET_FWUP_APPBOOT) == segcp_ret)
            {
                ret = device_appboot_update(); // AppBoot Firmware update by Configuration tool
            }
            else ret = DEVICE_FWUP_RET_FAILED;
            
            if(ret == DEVICE_FWUP_RET_SUCCESS)
            {
                /*
                if((opmode == DEVICE_AT_MODE) && ((segcp_ret & SEGCP_RET_FWUP_SERVER) == segcp_ret))
                {
                    // for AT mode
                    uart_puts(SEG_DATA_UART, "FS:UPDATE_SUCCESS\r\n", 19);
                    uart_puts(SEG_DATA_UART, "REBOOT\r\n", 8);
                }
                */
                set_device_status(ST_OPEN);
                save_DevConfig_to_storage();
                
                device_reboot();
            }
            else
            {
                // Clear the firmware update flags and size
                dev_config->firmware_update.fwup_size = 0;
                dev_config->firmware_update.fwup_flag = SEGCP_DISABLE;
                dev_config->firmware_update_extend.fwup_server_flag = SEGCP_DISABLE;
                /*
                if((opmode == DEVICE_AT_MODE) && ((segcp_ret & SEGCP_RET_FWUP_SERVER) == segcp_ret))
                {
                    uart_puts(SEG_DATA_UART, "FS:UPDATE_FAILED\r\n", 18);
                }
                */
                set_device_status(status_bak);
                
                // ## 20180208 Added by Eric, Force socket close when fw update fails occurred
                close(SOCK_FWUPDATE);

                flash_update_end();
                
                /* Simple UART init for Debugging */
                UART2_Configuration();
                
                /* DualTimer Re-Initialization */
                Timer_Configuration();
                
                if(dev_config->serial_info[0].serial_debug_en) printf(" > SEGCP:UPDATE:FAILED\r\n");
            }
            
            // If this device worked unstable after fw update failed problem occurred, users can add the device_reboot() function at below.
            //device_reboot();
        }
        else if (segcp_ret & SEGCP_RET_REBOOT)
        {
            if(opmode == DEVICE_AT_MODE) 
            {
                if(dev_config->serial_info[0].serial_debug_en) uart_puts(SEG_DATA_UART, "REBOOT\r\n", 8);
            }
            
            device_reboot();
        }
    }
}


uint8_t parse_SEGCP(uint8_t * pmsg, uint8_t * param)
{
    uint8_t** pcmd;
    uint8_t cmdnum = 0;
    uint8_t i;
    
    *param = 0;

    for(pcmd = tbSEGCPCMD; *pcmd != 0; pcmd++)
    {
        if(!strncmp((char *)pmsg, *pcmd, strlen(*pcmd))) break;
    }

    if(*pcmd == 0) 
    {
#ifdef _SEGCP_DEBUG_   
        printf("SEGCP[UNKNOWN]:%s\r\n", pmsg);
#endif
        return SEGCP_UNKNOWN;
    }
    
    cmdnum = (uint8_t)(pcmd - tbSEGCPCMD);
    
    if(cmdnum == (uint8_t)SEGCP_MA) 
    {
        if((pmsg[8] == '\r') && (pmsg[9] == '\n'))
        {
            memcpy(param, (uint8_t*)&pmsg[2], 6);
        }
        else
        {
            return SEGCP_UNKNOWN;
        }
    }
    else if(cmdnum == (uint8_t)SEGCP_PW)
    {
        for(i = 0; pmsg[2+i] != '\r'; i++)
        {
            param[i] = pmsg[2+i];
        }
        
        if(pmsg[2+i+1] == '\n')
        {
            param[i] = 0; param[i+1] = 0;
        }
        else
        {
            return SEGCP_UNKNOWN;
        }
    }
    else
    {
        strcpy(param, (uint8_t*)&pmsg[2]);
    }

#ifdef _SEGCP_DEBUG_
    printf("SEGCP[%d:%s:", cmdnum, *pcmd);
    if(cmdnum == SEGCP_MA)
    {
        for(i = 0; i < 6; i++) printf("%.2x", param[i]);
    }
    else
    {
        printf("%s",param);
    }
    printf("]\r\n");
#endif
    
    return cmdnum;
}

uint16_t proc_SEGCP(uint8_t* segcp_req, uint8_t* segcp_rep)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    //uint8_t  i = 0;
    uint16_t ret = 0;
    uint8_t  cmdnum = 0;
    uint8_t* treq;
    
    char * trep = segcp_rep;
    uint16_t param_len = 0;
    
    uint8_t  io_num = 0;
    uint8_t  io_type = 0;
    uint8_t  io_dir = 0;
    
    uint8_t  tmp_byte = 0;
    uint16_t tmp_int = 0;
    uint32_t tmp_long = 0;
    
    uint8_t tmp_ip[4];
    
    uint8_t param[SEGCP_PARAM_MAX*2];
    
#ifdef _SEGCP_DEBUG_
    printf("SEGCP_REQ : %s\r\n",segcp_req);
#endif
    memset(trep, 0, sizeof(trep));
    treq = strtok(segcp_req, SEGCP_DELIMETER);
    
    while(treq)
    {
#ifdef _SEGCP_DEBUG_   
        printf("SEGCP_REQ_TOK : %s\r\n",treq);
#endif
        if((cmdnum = parse_SEGCP(treq, param)) != SEGCP_UNKNOWN)
        {
            param_len = strlen((const char *)param);
            
            if(*param == 0)
            {
                memcpy(trep, tbSEGCPCMD[cmdnum], SEGCP_CMD_MAX);
                trep+=SEGCP_CMD_MAX;
                
                switch((teSEGCPCMDNUM)cmdnum)
                {
                    case SEGCP_MC: sprintf(trep,"%02X:%02X:%02X:%02X:%02X:%02X", 
                                            dev_config->network_info_common.mac[0], dev_config->network_info_common.mac[1], dev_config->network_info_common.mac[2],
                                            dev_config->network_info_common.mac[3], dev_config->network_info_common.mac[4], dev_config->network_info_common.mac[5]);
                        break;
                    case SEGCP_VR: 
                        if(strcmp(STR_VERSION_STATUS, "Develop") == 0)
                        {
                            // Develop version 
                            sprintf(trep,"%d.%d.%ddev", dev_config->fw_ver[0], dev_config->fw_ver[1], dev_config->fw_ver[2]);
                        }
                        else
                        {
                            sprintf(trep,"%d.%d.%d", dev_config->fw_ver[0], dev_config->fw_ver[1], dev_config->fw_ver[2]);
                        }
                        break;
                    case SEGCP_MN: sprintf(trep,"%s", dev_config->module_name);
                        break;
                    case SEGCP_IM: sprintf(trep,"%d", dev_config->options.dhcp_use);	// 0:STATIC, 1:DHCP (PPPoE X)
                        break;
                    case SEGCP_OP: sprintf(trep,"%d", dev_config->network_info[0].working_mode); // opmode
                        break;
                    case SEGCP_DD: sprintf(trep,"%d", 0); // Not used
                        break;
                    case SEGCP_PO: sprintf(trep,"%d", 0); // Not used
                        break;
                    case SEGCP_CP: sprintf(trep,"%d", dev_config->options.pw_connect_en);
                        break;
                    case SEGCP_DG: sprintf(trep,"%d", dev_config->serial_info[0].serial_debug_en);
                        break;
                    case SEGCP_KA: sprintf(trep,"%d", dev_config->network_info[0].keepalive_en);
                        break;
                    case SEGCP_KI: sprintf(trep,"%d", dev_config->network_info[0].keepalive_wait_time);
                        break;
                    case SEGCP_KE: sprintf(trep,"%d", dev_config->network_info[0].keepalive_retry_time);
                        break;
                    case SEGCP_RI: sprintf(trep,"%d", dev_config->network_info[0].reconnection);
                        break;
                    case SEGCP_LI:
                        sprintf(trep,"%d.%d.%d.%d", dev_config->network_info_common.local_ip[0], dev_config->network_info_common.local_ip[1],
                                                    dev_config->network_info_common.local_ip[2], dev_config->network_info_common.local_ip[3]);
                        break;
                    case SEGCP_SM: 
                        sprintf(trep,"%d.%d.%d.%d", dev_config->network_info_common.subnet[0], dev_config->network_info_common.subnet[1],
                                                    dev_config->network_info_common.subnet[2], dev_config->network_info_common.subnet[3]);
                        break;
                    case SEGCP_GW: 
                        sprintf(trep,"%d.%d.%d.%d", dev_config->network_info_common.gateway[0], dev_config->network_info_common.gateway[1],
                                                    dev_config->network_info_common.gateway[2], dev_config->network_info_common.gateway[3]);
                        break;
                    case SEGCP_DS:
                        sprintf(trep,"%d.%d.%d.%d", dev_config->options.dns_server_ip[0], dev_config->options.dns_server_ip[1],
                                                    dev_config->options.dns_server_ip[2], dev_config->options.dns_server_ip[3]);
                        break;
                    case SEGCP_PI:
                        //if(tsvDEVCONFnew.pppoe_id[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
                        //else sprintf(trep,"%s",tsvDEVCONFnew.pppoe_id);
                        sprintf(trep,"%c", SEGCP_NULL); // Not used
                        break;
                    case SEGCP_PP: 
                        //if(tsvDEVCONFnew.pppoe_pass[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
                        //sprintf(trep,"%s",tsvDEVCONFnew.pppoe_pass);
                        sprintf(trep,"%c",SEGCP_NULL); // Not used
                        break;
                    //case SEGCP_DX: sprintf(trep,"%d",tsvDEVCONFnew.ddns_index);
                    case SEGCP_DX: sprintf(trep,"%d", 0); // Not used
                        break;
                    //case SEGCP_DP: sprintf(trep,"%d",tsvDEVCONFnew.ddns_port);
                    case SEGCP_DP: sprintf(trep,"%d", DEVICE_DDNS_PORT); // Not used
                        break;
                    //case SEGCP_DI: sprintf(trep,"%s",tsvDEVCONFnew.ddns_id);
                    case SEGCP_DI: sprintf(trep,"%c", SEGCP_NULL); // Not used
                        break;
                    case SEGCP_DW: 
                        //if(tsvDEVCONFnew.ddns_pass[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
                        //else sprintf(trep,"%s",tsvDEVCONFnew.ddns_pass);
                        sprintf(trep, "%c", SEGCP_NULL); // Not used
                        break;
                    case SEGCP_DH:
                        if(dev_config->module_name[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
                        else sprintf(trep, "%s-%02X%02X%02X", dev_config->module_name, dev_config->network_info_common.mac[3], dev_config->network_info_common.mac[4], dev_config->network_info_common.mac[5]);
                        break;
                    case SEGCP_LP: sprintf(trep, "%d", dev_config->network_info[0].local_port);
                        break;
                    case SEGCP_RP: sprintf(trep, "%d", dev_config->network_info[0].remote_port);
                        break;
                    case SEGCP_RH: 
                        if(dev_config->options.dns_use == SEGCP_DISABLE)
                        {
                            sprintf(trep, "%d.%d.%d.%d", dev_config->network_info[0].remote_ip[0], dev_config->network_info[0].remote_ip[1],
                                                        dev_config->network_info[0].remote_ip[2], dev_config->network_info[0].remote_ip[3]);
                        }
                        else
                        {
                            if(dev_config->options.dns_domain_name[0] == 0) sprintf(trep, "%c", SEGCP_NULL);
                            else sprintf(trep, "%s", dev_config->options.dns_domain_name);
                        }
                        break;
                    case SEGCP_BR: sprintf(trep, "%d", dev_config->serial_info[0].baud_rate);
                        break;
                    case SEGCP_DB: sprintf(trep, "%d", dev_config->serial_info[0].data_bits);
                        break;
                    case SEGCP_PR: sprintf(trep, "%d", dev_config->serial_info[0].parity);
                        break;
                    case SEGCP_SB: sprintf(trep, "%d", dev_config->serial_info[0].stop_bits);
                        break;
                    case SEGCP_FL: sprintf(trep, "%d", dev_config->serial_info[0].flow_control);
                        break;
                    case SEGCP_IT: sprintf(trep, "%d", dev_config->network_info[0].inactivity);
                        break;
                    case SEGCP_PT: sprintf(trep, "%d", dev_config->network_info[0].packing_time);
                        break;
                    case SEGCP_PS: sprintf(trep, "%d", dev_config->network_info[0].packing_size);
                        break;
                    case SEGCP_PD: sprintf(trep, "%02X", dev_config->network_info[0].packing_delimiter[0]);
                        break;
                    case SEGCP_TE: sprintf(trep, "%d", dev_config->options.serial_command);
                        break;
                    case SEGCP_SS: sprintf(trep, "%02X%02X%02X", dev_config->options.serial_trigger[0], dev_config->options.serial_trigger[1], dev_config->options.serial_trigger[2]);
                        break;
                    case SEGCP_NP:
                        if(dev_config->options.pw_connect[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
                        else sprintf(trep, "%s", dev_config->options.pw_connect);
                        break;
                    case SEGCP_SP:
                        if(dev_config->options.pw_search[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
                        else sprintf(trep, "%s", dev_config->options.pw_search);
                        break;
                    case SEGCP_LG: 
                    case SEGCP_ER: 
                    case SEGCP_MA:
                    case SEGCP_PW: ret |= SEGCP_RET_ERR_NOTAVAIL;
                        break;
                    case SEGCP_FW: ret |= SEGCP_RET_ERR_NOPARAM;
                        break;
#ifdef __USE_USERS_GPIO__
                    // GET GPIOs Status / Value
                    case SEGCP_GA:
                    case SEGCP_GB:
                    case SEGCP_GC:
                    case SEGCP_GD:
                        io_num = (teSEGCPCMDNUM)cmdnum - SEGCP_GA;
                        if(get_user_io_val(USER_IO_SEL[io_num], &tmp_int) != 0) sprintf(trep, "%d", tmp_int);
                        else ret |= SEGCP_RET_ERR_NOTAVAIL;
                        break;
                    
                    // GET GPIOs settings; Type and Direction
                    case SEGCP_CA:
                    case SEGCP_CB:
                    case SEGCP_CC:
                    case SEGCP_CD:
                        io_num = (teSEGCPCMDNUM)cmdnum - SEGCP_CA;
                        io_type = get_user_io_type(USER_IO_SEL[io_num]);
                        io_dir = get_user_io_direction(USER_IO_SEL[io_num]);
                        sprintf(trep, "%d", (((io_type & 0x01) << 1) | io_dir));
                        break;
#endif
                    // GET Status pin's setting and status
                    case SEGCP_SC: // mode select
                        sprintf(trep, "%d%d", dev_config->serial_info[0].dtr_en, dev_config->serial_info[0].dsr_en);
                        break;
                    case SEGCP_S0:
                        sprintf(trep, "%d", get_connection_status_io(STATUS_PHYLINK_PIN)); // STATUS_PHYLINK_PIN (in) == DTR_PIN (out)
                        break;
                    case SEGCP_S1:
                        sprintf(trep, "%d", get_connection_status_io(STATUS_TCPCONNECT_PIN)); // STATUS_TCPCONNECT_PIN (in) == DSR_PIN (in)
                        break;
                    case SEGCP_RX:
                        uart_rx_flush(SEG_DATA_UART);
                        sprintf(trep, "%s", "FLUSH");
                        //ret |= SEGCP_RET_ERR_NOTAVAIL;
                        break;
                    // Firmware Update via HTTP server (Under Development)
                    case SEGCP_FS: // Firmware update by HTTP Server
                        dev_config->firmware_update.fwup_flag = SEGCP_ENABLE;
                        dev_config->firmware_update_extend.fwup_server_flag = SEGCP_ENABLE;
                        process_socket_termination(SEG_SOCK);
                        
                        sprintf(trep, "%s%s", FWUP_SERVER_DOMAIN, FWUP_SERVER_BINPATH);
                        ret |= SEGCP_RET_FWUP_SERVER;
#ifdef _SEGCP_DEBUG_
                        printf("SEGCP_FS:OK\r\n");
#endif
                        break;
                    
                    case SEGCP_FC: // Firmware update by HTTP Server using default server info enable / disable
                        sprintf(trep, "%d", dev_config->firmware_update_extend.fwup_server_use_default);
                        break;
                    
                    case SEGCP_FP: // Firmware update HTTP Server Port
                        sprintf(trep, "%d", dev_config->firmware_update_extend.fwup_server_port);
                        break;

                    case SEGCP_FD: // HTTP Server domain for Firmware update
#ifdef FWUP_SERVER_DOMAIN
                        sprintf(trep, "%s", FWUP_SERVER_DOMAIN);
#else
                        sprintf(trep,"%c",SEGCP_NULL);
#endif
                        // Planned to change
                        //if(dev_config->dev_config->firmware_update_extend.fwup_server_domain[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
                        //else sprintf(trep, "%s", dev_config->firmware_update_extend.fwup_server_domain);
                        break;
                    
                    case SEGCP_FH: // Firmware file path in HTTP server for Firmware update
#ifdef FWUP_SERVER_BINPATH
                        sprintf(trep, "%s", FWUP_SERVER_BINPATH);
#else
                        sprintf(trep,"%c",SEGCP_NULL);
#endif
                        // Planned to change
                        //if(dev_config->dev_config->firmware_update_extend.fwup_server_binpath[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
                        //else sprintf(trep, "%s", dev_config->firmware_update_extend.fwup_server_binpath);
                        break;
///////////////////////////////////////////////////////////////////////////////////////////////
                    case SEGCP_SV:
                        if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) ret |= SEGCP_RET_SAVE;
                        else ret |= SEGCP_RET_ERR_NOPRIVILEGE;
                        break;
                    case SEGCP_EX:
                        if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) ret |= SEGCP_RET_SWITCH;
                        else ret |= SEGCP_RET_ERR_NOPRIVILEGE;
                        break;
                    case SEGCP_RT:
                        if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) ret |= SEGCP_RET_REBOOT;
                        else ret |= SEGCP_RET_ERR_NOPRIVILEGE;
                        break;
                    case SEGCP_UN:
                        // NEW: UART Interface String - TTL/RS-232 or RS-422/485
                        sprintf(trep, "%s", uart_if_table[dev_config->serial_info[0].uart_interface]);
                        
                        // OLD: UART COUNT
                        //sprintf(trep, "%d", DEVICE_UART_CNT); 
                        break;
                    case SEGCP_UI: 
                        // NEW: UART Interface Number- [0] TTL/RS-232 or [1] RS-422/485
                        sprintf(trep, "%d", dev_config->serial_info[0].uart_interface);
                        break;
                    case SEGCP_ST: sprintf(trep, "%s", strDEVSTATUS[dev_config->network_info[0].state]);
                        break;
                    case SEGCP_FR: 
                        if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) 
                        {
                            // #20161110 Hidden option, Local port number [1] + FR cmd => K! (EEPROM Erase)
                            if(dev_config->network_info[0].local_port == 1)
                            {
                                ret |= SEGCP_RET_ERASE_EEPROM | SEGCP_RET_REBOOT; // EEPROM Erase
                            }
                            else
                            {
                                ret |= SEGCP_RET_FACTORY | SEGCP_RET_REBOOT; // Factory Reset
                            }
                        }
                        else
                        {
                            ret |= SEGCP_RET_ERR_NOPRIVILEGE;
                        }
                        break;
                    case SEGCP_EC:
                        sprintf(trep,"%d",dev_config->options.serial_command_echo);
                        break;
                    case SEGCP_K1:
                        ret |= SEGCP_RET_ERASE_EEPROM | SEGCP_RET_REBOOT;
                        break;
                    case SEGCP_UE: // User echo, not used.
                        sprintf(trep, "%d", 0);
                        break;
                    case SEGCP_AB: // Added command to enter AppBoot mode: AB
                        if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) {
                            dev_config->network_info[0].state = ST_BOOT;
                            ret |= SEGCP_RET_SAVE | SEGCP_RET_REBOOT;
                        }
                        else ret |= SEGCP_RET_ERR_NOPRIVILEGE;
                        break;
                    case SEGCP_TR: sprintf(trep, "%d", dev_config->options.tcp_rcr_val);
                        break;
                    case SEGCP_BU: ret |= SEGCP_RET_ERR_NOPARAM;
                        break;
                    default:
                        ret |= SEGCP_RET_ERR_NOCOMMAND;
                        sprintf(trep,"%s", strDEVSTATUS[dev_config->network_info[0].state]);
                        break;
                }
                
                if(ret & (SEGCP_RET_ERR | SEGCP_RET_REBOOT | SEGCP_RET_SWITCH | SEGCP_RET_SAVE))
                {
                    trep -= SEGCP_CMD_MAX;
                    *trep = 0;
                }
                else
                {
                    strcat(trep, SEGCP_DELIMETER);
                    trep += strlen(trep);
                }
            }
            else if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE))
            {
                switch((teSEGCPCMDNUM)cmdnum)
                {
                    case SEGCP_MC:
                        if((dev_config->network_info_common.mac[0] == 0x00) && (dev_config->network_info_common.mac[1] == 0x08) && (dev_config->network_info_common.mac[2] == 0xDC)) ret |= SEGCP_RET_ERR_IGNORED;
                        else if(!is_macaddr(param, ".:-", dev_config->network_info_common.mac)) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;
                    case SEGCP_VR: 
                    case SEGCP_MN:
                        ret |= SEGCP_RET_ERR_IGNORED;
                        break;
                    case SEGCP_IM:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_DHCP) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->options.dhcp_use = tmp_byte;
                        break;
                    case SEGCP_OP: 
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > UDP_MODE)
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        else
                        {
                            process_socket_termination(SEG_SOCK);
                            dev_config->network_info[0].working_mode = tmp_byte;
                        }
                        break;
                   case SEGCP_DD: // ## Does nothing
                        //if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //else tsvDEVCONFnew.ddns_en = tmp_byte;
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;               
                    case SEGCP_PO: // ## Does nothing
                        //if(param_len != 1 || tmp_byte > SEGCP_TELNET) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //else tsvDEVCONFnew.telnet_en[0] = tmp_byte;
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_TELNET) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;
                    case SEGCP_CP:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->options.pw_connect_en = tmp_byte;
                        break;
                    case SEGCP_DG:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEG_DEBUG_ALL) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->serial_info[0].serial_debug_en = tmp_byte;
                        break;
                    case SEGCP_KA:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].keepalive_en = tmp_byte;
                        break;
                    case SEGCP_KI:
                        tmp_long = atol(param);
                        if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].keepalive_wait_time = (uint16_t) tmp_long;
                        break;
                    case SEGCP_KE:
                        tmp_long = atol(param);
                        if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].keepalive_retry_time = (uint16_t) tmp_long;
                        break;
                    case SEGCP_RI:
                        tmp_long = atol(param);
                        if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].reconnection = (uint16_t) tmp_long;
                        break;
                    case SEGCP_LI:
                        if(is_ipaddr(param, tmp_ip))
                        {
                            dev_config->network_info_common.local_ip[0] = tmp_ip[0];
                            dev_config->network_info_common.local_ip[1] = tmp_ip[1];
                            dev_config->network_info_common.local_ip[2] = tmp_ip[2];
                            dev_config->network_info_common.local_ip[3] = tmp_ip[3];
                        }
                        else
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        break;
                    case SEGCP_SM:
                        if(is_ipaddr(param, tmp_ip))
                        {
                            dev_config->network_info_common.subnet[0] = tmp_ip[0];
                            dev_config->network_info_common.subnet[1] = tmp_ip[1];
                            dev_config->network_info_common.subnet[2] = tmp_ip[2];
                            dev_config->network_info_common.subnet[3] = tmp_ip[3];
                        }
                        else ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;
                    case SEGCP_GW:
                        if(is_ipaddr(param, tmp_ip))
                        {
                            dev_config->network_info_common.gateway[0] = tmp_ip[0];
                            dev_config->network_info_common.gateway[1] = tmp_ip[1];
                            dev_config->network_info_common.gateway[2] = tmp_ip[2];
                            dev_config->network_info_common.gateway[3] = tmp_ip[3];
                        }
                        else ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;
                    case SEGCP_DS:
                        if(is_ipaddr(param, tmp_ip))
                        {
                            dev_config->options.dns_server_ip[0] = tmp_ip[0];
                            dev_config->options.dns_server_ip[1] = tmp_ip[1];
                            dev_config->options.dns_server_ip[2] = tmp_ip[2];
                            dev_config->options.dns_server_ip[3] = tmp_ip[3];
                        }
                        else ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;
                    case SEGCP_PI: // ## Does nothing
                        //if(param_len > sizeof(tsvDEVCONFnew.pppoe_id)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //else
                        //{
                            //if(param[0] == SEGCP_NULL) tsvDEVCONFnew.pppoe_id[0]= 0;
                            //else  sscanf(param,"%s",&tsvDEVCONFnew.pppoe_id);
                        //}
                        ;
                        break;
                    case SEGCP_PP: // ## Does nothing
                        //if(param_len > sizeof(tsvDEVCONFnew.pppoe_pass)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //else 
                        //{
                            //if(param[0] == SEGCP_NULL) tsvDEVCONFnew.pppoe_pass[0]= 0;
                            //else sscanf(param,"%s",&tsvDEVCONFnew.pppoe_pass);
                        //}
                        ;
                        break;
                    case SEGCP_DX: // ## Does nothing
                        //if(param_len != 1 || tmp_byte > (DEVICE_DDNS_CNT-1)) ret |= SEGCP_RET_ERR_INVALIDPARAM;                   
                        //else tsvDEVCONFnew.ddns_index = tmp_byte;
                        ;
                        break;
                    case SEGCP_DP: // ## Does nothing
                        //if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //else tsvDEVCONFnew.ddns_port = (uint16_t)tmp_long;
                        ;
                        break;
                    case SEGCP_DI: // ## Does nothing
                        //if(param_len > sizeof(tsvDEVCONFnew.ddns_id)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //else 
                        //{
                            //if(param[0] == SEGCP_NULL) tsvDEVCONFnew.ddns_id[0] = 0;
                            //else sscanf(param,"%s",&tsvDEVCONFnew.ddns_id);
                        //}
                        ;
                        break;	 
                    case SEGCP_DW: // ## Does nothing
                        //if(param_len > sizeof(tsvDEVCONFnew.ddns_pass)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //else 
                        //{
                            //if(param[0] == SEGCP_NULL) tsvDEVCONFnew.ddns_pass[0] = 0;
                            //else sscanf(param,"%s",&tsvDEVCONFnew.ddns_pass);
                        //}
                        ;
                        break;
                    case SEGCP_DH:
                        if(param_len > sizeof(dev_config->module_name)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else 
                        {
                            if(param[0] == SEGCP_NULL)
                            {
                                dev_config->module_name[0] = 0;
                            }
                            else
                            {
                                sprintf(dev_config->module_name, "%s", param);
                            }
                        }
                        break;
                    case SEGCP_LP:
                        tmp_long = atol(param);
                        if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].local_port = (uint16_t)tmp_long;
                        break;
                    case SEGCP_RP:
                        tmp_long = atol(param);
                        if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].remote_port = (uint16_t)tmp_long;
                        break;
                    case SEGCP_RH:
                        if(is_ipaddr(param, tmp_ip))
                        {
                            dev_config->options.dns_use = SEGCP_DISABLE;
                            dev_config->network_info[0].remote_ip[0] = tmp_ip[0];
                            dev_config->network_info[0].remote_ip[1] = tmp_ip[1];
                            dev_config->network_info[0].remote_ip[2] = tmp_ip[2];
                            dev_config->network_info[0].remote_ip[3] = tmp_ip[3];
                        }
                        else
                        {
                            dev_config->options.dns_use = SEGCP_ENABLE;
                            if(param[0] == SEGCP_NULL) dev_config->options.dns_domain_name[0] = 0;
                            else strcpy(dev_config->options.dns_domain_name, param);
                        }
                        
                        break;
                    case SEGCP_BR:
                        tmp_int = atoi(param);
                        if(param_len > 2 || tmp_int > baud_460800) ret |= SEGCP_RET_ERR_INVALIDPARAM; // ## 20180208 Added by Eric, Supports baudrate up to 460.8kbps 
                        else dev_config->serial_info[0].baud_rate = (uint8_t)tmp_int;
                        break;
                    case SEGCP_DB:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > word_len8) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->serial_info[0].data_bits = tmp_byte;
                        break;
                    case SEGCP_PR:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > parity_even) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->serial_info[0].parity = tmp_byte;
                        break;
                    case SEGCP_SB:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > stop_bit2) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->serial_info[0].stop_bits = tmp_byte;
                        break;
                    case SEGCP_FL:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > flow_reverserts)
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        else
                        {
                            if(dev_config->serial_info[0].uart_interface == UART_IF_RS422_485)
                            {
                                if((tmp_byte != flow_rtsonly) && (tmp_byte != flow_reverserts))
                                {
                                    dev_config->serial_info[0].flow_control = flow_none;
                                }
                                else
                                {
                                    dev_config->serial_info[0].flow_control = tmp_byte;
                                }
                            }
                            else
                            {
                                dev_config->serial_info[0].flow_control = tmp_byte;
                            }
                        }
                        break;
                    case SEGCP_IT:
                        tmp_long = atol(param);
                        if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].inactivity = (uint16_t)tmp_long;
                        break;
                    case SEGCP_PT:
                        tmp_long = atol(param);
                        if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].packing_time = (uint16_t)tmp_long;
                        break;
                    case SEGCP_PS:
                        tmp_int = atoi(param);
                        if(param_len > 3 || tmp_int > 0xFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->network_info[0].packing_size = (uint8_t)tmp_int;
                        break;
                    case SEGCP_PD:
                        if(param_len != 2 || !is_hexstr(param))
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        else
                        {
                            str_to_hex(param, &tmp_byte);
                            dev_config->network_info[0].packing_delimiter[0] = tmp_byte;
                            
                            if(dev_config->network_info[0].packing_delimiter[0] == 0x00) 
                                dev_config->network_info[0].packing_delimiter_length = 0;
                            else 
                                dev_config->network_info[0].packing_delimiter_length = 1;
                        }
                        
                        break;
                    case SEGCP_TE:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->options.serial_command = tmp_byte;
                        break;
                    case SEGCP_SS:
                        if(param_len != 6 || !is_hexstr(param) || !str_to_hex(param, dev_config->options.serial_trigger))
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        break;
                    case SEGCP_NP:
                        if(param_len > sizeof(dev_config->options.pw_connect)-1)
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        else
                        {
                            if(param[0] == SEGCP_NULL) dev_config->options.pw_connect[0] = 0;
                            else sprintf(dev_config->options.pw_connect, "%s", param);
                        }
                        break;
                    case SEGCP_SP:
                        if(param_len > sizeof(dev_config->options.pw_search)-1)
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        else
                        {
                            if(param[0] == SEGCP_NULL) dev_config->options.pw_search[0] = 0;
                            else sprintf(dev_config->options.pw_search, "%s", param);
                        }
                        break;
                    case SEGCP_FW:
                        tmp_long = atol(param);
#ifdef __USE_APPBACKUP_AREA__
                        if(tmp_long > (((uint32_t)DEVICE_FWUP_SIZE) & 0x0FFFF)) // 64KByte
#else
                        if(tmp_long > (((uint32_t)DEVICE_FWUP_SIZE) & 0x19000)) // 100KByte
#endif
                        {
                            dev_config->firmware_update.fwup_size = 0;
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
#ifdef _SEGCP_DEBUG_
                            printf("SEGCP_FW:ERROR:TOOBIG\r\n");
#endif
                        }
                        else
                        {
#ifdef __USE_APPBACKUP_AREA__
                            dev_config->firmware_update.fwup_size = (uint16_t)tmp_long;
#else
                            dev_config->firmware_update.fwup_size = tmp_long;
#endif
                            dev_config->firmware_update.fwup_flag = SEGCP_ENABLE;
                            ret |= SEGCP_RET_FWUP;

                            sprintf(trep,"FW%d.%d.%d.%d:%d\r\n", dev_config->network_info_common.local_ip[0], dev_config->network_info_common.local_ip[1]
                            ,dev_config->network_info_common.local_ip[2] , dev_config->network_info_common.local_ip[3], (uint16_t)DEVICE_FWUP_PORT);
                            
                            process_socket_termination(SEG_SOCK);
#ifdef _SEGCP_DEBUG_
                            printf("SEGCP_FW:OK\r\n");
#endif
                        }
                        break;
///////////////////////////////////////////////////////////////////////////////////////////////
// User GPIOs
#ifdef __USE_USERS_GPIO__
                    // SET GPIOs Status / Value (Digital output only)
                    case SEGCP_GA:
                    case SEGCP_GB:
                    case SEGCP_GC:
                    case SEGCP_GD:
                        io_num = (teSEGCPCMDNUM)cmdnum - SEGCP_GA;
                        tmp_int = is_hex(*param);
                        if(param_len != 1 || tmp_int > IO_HIGH) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else
                        {
                            if(set_user_io_val(USER_IO_SEL[io_num], &tmp_int) == 0) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        break;
                    
                    // SET GPIOs settings; Type and Direction ('Analog output' mode is not allowed)
                    case SEGCP_CA:
                    case SEGCP_CB:
                    case SEGCP_CC:
                    case SEGCP_CD:
                        io_num = (teSEGCPCMDNUM)cmdnum - SEGCP_CA;
                        tmp_int = atoi(param);
                        
                        io_type = (uint8_t)(tmp_int >> 1);
                        io_dir = (uint8_t)(tmp_int & 0x01);
                        
                        if((param_len > 2) || (io_type > IO_ANALOG_IN) || (io_dir > IO_OUTPUT)) // Invalid parameters
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        else
                        {
                            if((io_type == IO_ANALOG_IN) && (io_dir == IO_OUTPUT)) // This case not allowed. (Analog output)
                            {
                                ret |= SEGCP_RET_ERR_INVALIDPARAM;
                            }
                            else
                            {
                                // IO type and Direction settings
                                set_user_io_type(USER_IO_SEL[io_num], io_type);
                                set_user_io_direction(USER_IO_SEL[io_num], io_dir);
                                init_user_io(USER_IO_SEL[io_num]);
                            }
                        }
                        break;
#endif
///////////////////////////////////////////////////////////////////////////////////////////////
// Status Pins
                    // SET status pin mode selector
                    case SEGCP_SC:
                        str_to_hex(param, &tmp_byte);
                        
                        tmp_int = (tmp_byte & 0xF0) >> 4; // [0] PHY link / [1] DTR
                        tmp_byte = (tmp_byte & 0x0F); // [0] TCP connection / [1] DSR
                        
                        if((param_len > 2) || (tmp_byte > IO_HIGH) || (tmp_int > IO_HIGH)) // Invalid parameters
                        {
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        }
                        else
                        {
                            dev_config->serial_info[0].dtr_en = (uint8_t)tmp_int;
                            dev_config->serial_info[0].dsr_en = tmp_byte;
                            
                            // Status I/O - Shared pin init: Connection status pins or DTR/DSR pins
                            init_connection_status_io(); 
                            
                            // Set the DTR pin to high when the DTR signal enabled (== PHY link status disabled)
                            if(dev_config->serial_info[0].dtr_en == SEGCP_ENABLE) set_flowcontrol_dtr_pin(ON);
                        }
                        break;
                    case SEGCP_S0:
                    case SEGCP_S1:
                        ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;
///////////////////////////////////////////////////////////////////////////////////////////////
// UART Rx flush
                    case SEGCP_RX:
                        ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;
///////////////////////////////////////////////////////////////////////////////////////////////
// Firmware Update via HTTP server (Under Development)
                    case SEGCP_FS: // Firmware update by HTTP Server
                        ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        break;
                    
                    case SEGCP_FC: // Firmware update by HTTP Server using default server info enable / disable
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->firmware_update_extend.fwup_server_use_default = tmp_byte;
                        break;
                    
                    case SEGCP_FP: // Firmware update HTTP Server Port
                        tmp_int = atoi(param);
                        if(tmp_int > 0xffff) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->firmware_update_extend.fwup_server_port = tmp_int;
                        break;
                    
                    // Planned to apply
                    case SEGCP_FD: // HTTP Server domain for Firmware update
                        ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //if(param[0] == SEGCP_NULL) dev_config->dev_config->firmware_update_extend.fwup_server_domain[0] = 0;
                        //else strcpy(dev_config->firmware_update_extend.fwup_server_domain, param);
                        break;
                    // Planned to apply
                    case SEGCP_FH: // Firmware file path in HTTP server for Firmware update
                        ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        //if(param[0] == SEGCP_NULL) dev_config->firmware_update_extend.fwup_server_binpath[0] = 0;
                        //else strcpy(dev_config->firmware_update_extend.fwup_server_binpath, param);
                        break;
///////////////////////////////////////////////////////////////////////////////////////////////
                    
                    case SEGCP_EC:
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->options.serial_command_echo = tmp_byte;
                        break;
                    case SEGCP_UE: // User echo, Not used
                        tmp_byte = is_hex(*param);
                        if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else ; 
                        break;
                    
                    case SEGCP_TR: // TCP Retransmission retry count
                        tmp_int = atoi(param);
                        if((param_len > 3) || (tmp_int < 1) || (tmp_int > 0xFF)) ret |= SEGCP_RET_ERR_INVALIDPARAM;
                        else dev_config->options.tcp_rcr_val = (uint8_t)tmp_int;
                        break;
                    
                    case SEGCP_BU:
                        tmp_long = atol(param);
                        if(tmp_long > DEVICE_BOOT_SIZE) // AppBoot Size, 28KByte
                        {
                            dev_config->firmware_update.fwup_size = 0;
                            ret |= SEGCP_RET_ERR_INVALIDPARAM;
#ifdef _SEGCP_DEBUG_
                            printf("SEGCP_BU:ERROR:TOOBIG\r\n");
#endif
                        }
                        else
                        {
                            dev_config->firmware_update.fwup_size = (uint16_t)tmp_long;
                            ret |= SEGCP_RET_FWUP_APPBOOT;

                            sprintf(trep,"BU%d.%d.%d.%d:%d\r\n", dev_config->network_info_common.local_ip[0], dev_config->network_info_common.local_ip[1]
                            ,dev_config->network_info_common.local_ip[2] , dev_config->network_info_common.local_ip[3], (uint16_t)DEVICE_FWUP_PORT);
                            
                            process_socket_termination(SEG_SOCK);
#ifdef _SEGCP_DEBUG_
                            printf("SEGCP_BU:OK\r\n");
#endif
                        }
                        break;
                        
                    case SEGCP_UN:
                    case SEGCP_ST:
                    case SEGCP_LG:
                    case SEGCP_ER: 
                    case SEGCP_MA:
                    case SEGCP_EX: 
                    case SEGCP_SV:
                    case SEGCP_RT:
                    case SEGCP_FR:
                    case SEGCP_PW:
                    case SEGCP_UI:
                    case SEGCP_AB:
                        ret |= SEGCP_RET_ERR_NOTAVAIL;
                        break;
                    default:
                        ret |= SEGCP_RET_ERR_NOCOMMAND;
                        break;
                }
            }
            else
            {
                ret |= SEGCP_RET_ERR_NOPRIVILEGE;
            }
        }
        else
        {
            ret |= SEGCP_RET_ERR_NOCOMMAND;
        }
        
        // Process the serial command mode
        if(opmode == DEVICE_AT_MODE)
        {
            if(ret & SEGCP_RET_ERR)
            {
                treq[2] = 0;
                sprintf(trep,"%s:%s\r\n",tbSEGCPERR[((ret-SEGCP_RET_ERR) >> 8)],(cmdnum!=SEGCP_UNKNOWN)? tbSEGCPCMD[cmdnum] : treq);
#ifdef _SEGCP_DEBUG_
                printf("ERROR : %s\r\n",trep);
#endif
                uart_rx_flush(SEG_DATA_UART);
                return ret;
            }
        }
        treq = strtok(NULL, SEGCP_DELIMETER);
#ifdef _SEGCP_DEBUG_
        //printf(">> strtok: %s\r\n", treq);
#endif
    }
    
#ifdef _SEGCP_DEBUG_
    printf("\r\nEND of [proc_SEGCP] function - RET[0x%.4x]\r\n\r\n", ret);
#endif
    
    return ret;
}

uint16_t proc_SEGCP_udp(uint8_t* segcp_req, uint8_t* segcp_rep)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    uint16_t ret = 0;
    uint16_t len = 0;
    
    uint8_t destip[4];
    uint16_t destport;
    
    uint8_t tpar[SEGCP_PARAM_MAX*2];
    uint8_t* treq;
    uint8_t* trep;
    
    gSEGCPPRIVILEGE = SEGCP_PRIVILEGE_CLR;
    switch(getSn_SR(SEGCP_UDP_SOCK))
    {
        case SOCK_UDP:
            if((len = getSn_RX_RSR(SEGCP_UDP_SOCK)) > 0)
            {
                treq = segcp_req;
                trep = segcp_rep;
                len = recvfrom(SEGCP_UDP_SOCK, treq, len, destip, &destport);
                treq[len-1] = 0;
                if(SEGCP_MA == parse_SEGCP(treq, tpar))
                {
                    if(!memcmp(tpar,"\xFF\xFF\xFF\xFF\xFF\xFF", 6)) gSEGCPPRIVILEGE |= (SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_READ);
                    else if(!memcmp(tpar, dev_config->network_info_common.mac, sizeof(dev_config->network_info_common.mac))) gSEGCPPRIVILEGE |= (SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_WRITE);
                    else break;
                    
                    if(gSEGCPPRIVILEGE & SEGCP_PRIVILEGE_SET)
                    {
                        sprintf(trep,"%s%c%c%c%c%c%c\r\n",tbSEGCPCMD[SEGCP_MA],
                            dev_config->network_info_common.mac[0], dev_config->network_info_common.mac[1], dev_config->network_info_common.mac[2],
                            dev_config->network_info_common.mac[3], dev_config->network_info_common.mac[4], dev_config->network_info_common.mac[5]);
                        
                        treq += 10;
                        trep += 10;
                        
                        if(SEGCP_PW == parse_SEGCP(treq, tpar))
                        {
                            if((tpar[0] == SEGCP_NULL && dev_config->options.pw_search[0] == 0) || !strcmp(tpar, dev_config->options.pw_search))
                            {
                                memcpy(trep,treq, strlen(tpar)+4);  // "PWxxxx\r\n"
                                treq += (strlen(tpar) + 4);
                                trep += (strlen(tpar) + 4);
                                ret = proc_SEGCP(treq,trep);
                                
                                sendto(SEGCP_UDP_SOCK, segcp_rep, 14+strlen(tpar)+strlen(trep), "\xFF\xFF\xFF\xFF", destport);
                            }
                        }
                    }
                    else
                    {
                        return 0;
                    }
                }
                else
                {
                    return 0;
                }
            }
            break;
        case SOCK_CLOSED:
            if(socket(SEGCP_UDP_SOCK, Sn_MR_UDP, DEVICE_SEGCP_PORT, 0x00) == SEGCP_UDP_SOCK)
            {
                ;//if(dev_config->serial_info[0].serial_debug_en) printf(" > SEGCP:UDP:STARTED\r\n");
            }
            break;
    }
    return ret;
}


uint16_t proc_SEGCP_tcp(uint8_t* segcp_req, uint8_t* segcp_rep)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    uint16_t ret = 0;
    uint16_t len = 0;
    
    uint8_t tpar[SEGCP_PARAM_MAX+1];
    uint8_t * treq;
    uint8_t * trep;
    
    uint8_t state = getSn_SR(SEGCP_TCP_SOCK);
    gSEGCPPRIVILEGE = SEGCP_PRIVILEGE_CLR;
    
    switch(state)
    {
        case SOCK_INIT:
            //listen(SEGCP_TCP_SOCK); Function call Immediately after socket open operation
            break;
        
        case SOCK_LISTEN:
            break;
        
        case SOCK_ESTABLISHED:
            if(getSn_IR(SEGCP_TCP_SOCK) & Sn_IR_CON)
            {
                // TCP unicast search: Keep-alive timer enable 
                enable_configtool_keepalive_timer = ENABLE;
                configtool_keepalive_time = 0;
                
                setSn_IR(SEGCP_TCP_SOCK, Sn_IR_CON); // TCP connection interrupt clear
            }
            
            if(flag_send_configtool_keepalive == SEGCP_ENABLE) // default: 15sec
            {
                send_keepalive_packet_configtool(SEGCP_TCP_SOCK);
                flag_send_configtool_keepalive = SEGCP_DISABLE; // flag clear
            }

            if((len = getSn_RX_RSR(SEGCP_TCP_SOCK)) > 0)
            {
                treq = segcp_req;
                trep = segcp_rep;
                len = recv(SEGCP_TCP_SOCK,treq,len);
                treq[len-1] = 0x00;

                if(SEGCP_MA == parse_SEGCP(treq,tpar))
                {
                    if(!memcmp(tpar, "\xFF\xFF\xFF\xFF\xFF\xFF", 6)) gSEGCPPRIVILEGE |= (SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_READ);
                    else if(!memcmp(tpar, dev_config->network_info_common.mac, sizeof(dev_config->network_info_common.mac))) gSEGCPPRIVILEGE |= (SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_WRITE);
                    else break;
                    
                    if(gSEGCPPRIVILEGE & SEGCP_PRIVILEGE_SET)
                    {
                        sprintf(trep,"%s%c%c%c%c%c%c\r\n",tbSEGCPCMD[SEGCP_MA],
                            dev_config->network_info_common.mac[0], dev_config->network_info_common.mac[1], dev_config->network_info_common.mac[2],
                            dev_config->network_info_common.mac[3], dev_config->network_info_common.mac[4], dev_config->network_info_common.mac[5]);
                        
                        treq += 10;
                        trep += 10;
                        
                        if(SEGCP_PW == parse_SEGCP(treq,tpar))
                        {
                            if((tpar[0] == SEGCP_NULL && dev_config->options.pw_search[0] == 0) || !strcmp(tpar, dev_config->options.pw_search))
                            {
                                memcpy(trep,treq, strlen(tpar)+4);  // "PWxxxx\r\n"
                                treq += (strlen(tpar) + 4);
                                trep += (strlen(tpar) + 4);
                                ret = proc_SEGCP(treq,trep);
                                send(SEGCP_TCP_SOCK, segcp_rep, 14+strlen(tpar)+strlen(trep));
                            }
                        }
                    }
                    else
                    {
                        return 0;
                    }
                }
                else
                {
                    return 0;
                }
            }
            break;
            
        case SOCK_CLOSE_WAIT:
            disconnect(SEGCP_TCP_SOCK);
        
        case SOCK_CLOSED:
        case SOCK_FIN_WAIT:
            close(SEGCP_TCP_SOCK);
            
            if(socket(SEGCP_TCP_SOCK, Sn_MR_TCP, DEVICE_SEGCP_PORT, SF_TCP_NODELAY) == SEGCP_TCP_SOCK)
            {
                //if(dev_config->serial_info[0].serial_debug_en) printf(" > SEGCP:TCP:STARTED\r\n");
                
                //Keep-alive timer keep disabled until TCP connection established.
                enable_configtool_keepalive_timer = DISABLE;
                
                listen(SEGCP_TCP_SOCK);
            }
            break;
    }
    return ret;
}


uint16_t proc_SEGCP_uart(uint8_t * segcp_rep)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    uint16_t len = 0;
    uint16_t ret = 0;
    uint8_t segcp_req[SEGCP_PARAM_MAX*2];
    
    if(BUFFER_USED_SIZE(data_rx))
    {
        len = uart_get_commandline(SEG_DATA_UART, segcp_req, (sizeof(segcp_req) - 1));
        
        if(len != 0)
        {
            gSEGCPPRIVILEGE = SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_WRITE;
            ret = proc_SEGCP(segcp_req, segcp_rep);
            if(segcp_rep[0])
            {
                if(dev_config->serial_info[0].serial_debug_en)
                {
                    printf("%s",segcp_rep);
                }
                
                uart_puts(SEG_DATA_UART, segcp_rep, strlen(segcp_rep));
                
            }
        }
    }
    return ret;
}

uint16_t uart_get_commandline(uint8_t uartNum, uint8_t* buf, uint16_t maxSize)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    uint16_t i;
    //uint16_t j;
    uint16_t len = BUFFER_USED_SIZE(data_rx);
    
    if(len >= 4) // Minimum of command: 4-bytes, e.g., MC\r\n (MC$0d$0a)
    {
        for(i = 0; i < maxSize; i++)
        {
            buf[i] = uart_getc(uartNum);
            //if(buf[i] == '\n') break;	// [\n]: end of command
            if(buf[i] == 0x0a) break;	// [0x0a]: end of command (Line feed)
        }
        buf[i+1] = 0x00; // end of string
        
        if(dev_config->options.serial_command_echo == SEGCP_ENABLE)
        {
            //for(j = 0; j < i; j++) uart_putc(uartNum, buf[j]);
            uart_puts(uartNum, buf, i);
        }
    }
    else
    {
        return 0;
    }
    
    return strlen(buf);
}

void send_keepalive_packet_configtool(uint8_t sock)
{
    setsockopt(sock, SO_KEEPALIVESEND, 0);
#ifdef _SEGCP_DEBUG_
    printf(" > SOCKET[%x]: SEND KEEP-ALIVE PACKET\r\n", sock);
#endif 
}

// Function for Timer
void segcp_timer_msec(void)
{
    if(enable_configtool_keepalive_timer)
    {
        if(configtool_keepalive_time < 0xFFFF) 	configtool_keepalive_time++;
        else									configtool_keepalive_time = 0;
        
        if(configtool_keepalive_time >= CONFIGTOOL_KEEPALIVE_TIME_MS)
        {
            flag_send_configtool_keepalive = SEGCP_ENABLE;
            configtool_keepalive_time = 0;
        }
    }
}


/* System Core Clock Update for improved stability */
static void flash_update_start(void)
{
    /* System Core Clock Update */
    SystemCoreClockUpdate_User(CLOCK_SOURCE_INTERNAL, PLL_SOURCE_8MHz, SYSTEM_CLOCK_8MHz);
    
    /* SysTick_Config */
    SysTick_Config((GetSystemClock()/1000));
    
    /* Simple UART re-init by MCU clock update */
    UART2_Configuration();
    
    // Backup Interrupt Set Pending Register
    temp_interrupt = (NVIC->ISPR[0]);
    (NVIC->ISPR[0]) = (uint32_t)0xFFFFFFFF;
}

/* System Core Clock Update - Restore */
static void flash_update_end(void)
{
    /* System Core Clock Update */
    SystemCoreClockUpdate_User(DEVICE_CLOCK_SELECT, DEVICE_PLL_SOURCE_CLOCK, DEVICE_TARGET_SYSTEM_CLOCK);
    
    /* SysTick_Config */
    SysTick_Config((GetSystemClock()/1000));
    
    /* Simple UART re-init by MCU clock update */
    UART2_Configuration();
    
    // Restore Interrupt Set Pending Register
    (NVIC->ISPR[0]) = temp_interrupt;
}
