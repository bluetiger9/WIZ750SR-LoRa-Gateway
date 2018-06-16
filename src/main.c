/**
  ******************************************************************************
  * @file    W7500x Serial to Ethernet Project - WIZ750SR App
  * @author  Eric Jung, Team CS
  * @version v1.2.3
  * @date    Apr-2018
  * @brief   Main program body
  ******************************************************************************
  * @attention
  * @par Revision history
  *    <2018/04/27> v1.2.3 Bugfix and Improvements by Eric Jung
  *    <2018/04/12> v1.2.2 Bugfix by Eric Jung
  *    <2018/04/11> v1.2.2 Bugfix and Improvements by Eric Jung(Pre-released Ver.)
  *    <2018/03/26> v1.2.1 Bugfix by Eric Jung
  *    <2018/03/16> v1.2.1 Bugfix by Eric Jung(Pre-released Ver.)
  *    <2018/03/12> v1.2.0 Bugfix and Improvements by Eric Jung
  *    <2018/02/08> v1.1.2 Bugfix by Eric Jung
  *    <2018/01/26> v1.1.2 Added WIZ750SR_1xx function by Edward Ahn
  *    <2017/12/13> v1.1.1 Develop by Eric Jung
  *    <2016/11/18> v1.1.0 Develop by Eric Jung
  *    <2016/03/29> v1.0.0 Develop by Eric Jung
  *    <2016/03/02> v0.8.0 Develop by Eric Jung
  *    <2015/11/24> v0.0.1 Develop by Eric Jung
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, WIZnet SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2016 WIZnet Co., Ltd.</center></h2>
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "common.h"
#include "W7500x_board.h"

#include "W7500x_gpio.h"
#include "W7500x_uart.h"
#include "W7500x_crg.h"
#include "W7500x_wztoe.h"
#include "W7500x_miim.h"

#include "dhcp.h"
#include "dhcp_cb.h"
#include "dns.h"

#include "seg.h"
#include "segcp.h"
#include "ConfigData.h"

#include "timerHandler.h"
#include "uartHandler.h"
#include "deviceHandler.h"
#include "flashHandler.h"
#include "gpioHandler.h"

// ## for debugging
//#include "loopback.h"


/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define _MAIN_DEBUG_	// debugging message enable

/* Private function prototypes -----------------------------------------------*/
static void W7500x_Init(void);
static void W7500x_WZTOE_Init(void);
static void set_WZTOE_NetTimeout(void);
int8_t process_dhcp(void);
int8_t process_dns(void);

// Debug messages
void display_Dev_Info_header(void);
void display_Dev_Info_main(void);
void display_Dev_Info_dhcp(void);
void display_Dev_Info_dns(void);

void delay(__IO uint32_t milliseconds); //Notice: used ioLibray
void TimingDelay_Decrement(void);

/* Private variables ---------------------------------------------------------*/
static __IO uint32_t TimingDelay;

/* Public variables ---------------------------------------------------------*/
// Shared buffer declaration
uint8_t g_send_buf[DATA_BUF_SIZE];
uint8_t g_recv_buf[DATA_BUF_SIZE];

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // W7500x Hardware Initialize
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    
    /* W7500x MCU Initialization */
    W7500x_Init(); // includes UART2 initialize code for print out debugging messages
    
    /* W7500x WZTOE (Hardwired TCP/IP stack) Initialization */
    W7500x_WZTOE_Init();
    
    /* W7500x Board Initialization */
    W7500x_Board_Init();
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // W7500x Application: Initialize
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    
    /* Load the Configuration data */
    load_DevConfig_from_storage();
    dev_config->network_info[0].state = ST_OPEN;
    
    /* Set the MAC address to WIZCHIP */
    Mac_Conf();
    
    /* Set the TCP retranssmission retry count to WIZCHIP */
    set_WZTOE_NetTimeout();
    
    /* UART Initialization */
    S2E_UART_Configuration();
    
    if(dev_config->serial_info[0].serial_debug_en)
    {
        // Debug UART: Device information print out
        display_Dev_Info_header();
        display_Dev_Info_main();
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // W7500x Application: DHCP client / DNS client handler
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    
    /* Network Configuration - DHCP client */
    // Initialize Network Information: DHCP or Static IP allocation
    if(dev_config->options.dhcp_use)
    {
        if(process_dhcp() == DHCP_IP_LEASED) // DHCP success
        {
            flag_process_dhcp_success = ON;
        }
        else // DHCP failed
        {
            Net_Conf(); // Set default static IP settings
        }
    }
    else
    {
        Net_Conf(); // Set default static IP settings
    }
    
    // Debug UART: Network information print out (includes DHCP IP allocation result)
    if(dev_config->serial_info[0].serial_debug_en)
    {
        display_Net_Info();
        display_Dev_Info_dhcp();
    }
    
    /* DNS client */
    //if((value->network_info[0].working_mode == TCP_CLIENT_MODE) || (value->network_info[0].working_mode == TCP_MIXED_MODE))
    if(dev_config->network_info[0].working_mode != TCP_SERVER_MODE)
    {
        if(dev_config->options.dns_use) 
        {
            if(process_dns()) // DNS success
            {
                flag_process_dns_success = ON;
            }
            
            // Debug UART: DNS results print out
            if(dev_config->serial_info[0].serial_debug_en)
            {
                display_Dev_Info_dns();
            }
        }
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // W7500x Application: Main Routine
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    
    flag_s2e_application_running = ON;
    
    // HW_TRIG switch ON
    if(flag_hw_trig_enable)
    {
        init_trigger_modeswitch(DEVICE_AT_MODE);
        flag_hw_trig_enable = 0;
    }
    
    /* GPIO Initialization*/
    IO_Configuration();

    while(1) // main loop
    {
        do_segcp();
        do_seg(SOCK_DATA);
        
        if(dev_config->options.dhcp_use) DHCP_run(); // DHCP client handler for IP renewal
        
        // ## debugging: Data echoback
        //loopback_tcps(6, g_recv_buf, 5001); // Loopback
        //loopback_iperf(6, g_recv_buf, 5001); // iperf: Ethernet performance test
        
        if(flag_check_phylink) // ## debugging: PHY link
        {
            // To-do: insert user's debugging code here
            //printf("PHY Link status: %x\r\n", GPIO_ReadInputDataBit(PHYLINK_IN_PORT, PHYLINK_IN_PIN));
            flag_check_phylink = 0; // flag clear
        }
        
        if(flag_ringbuf_full) // ## debugging: Ring buffer full
        {
            // To-do: insert user's debugging code here
            if(dev_config->serial_info[0].serial_debug_en) printf(" > UART Rx Ring buffer Full\r\n");
            flag_ringbuf_full = 0; // flag clear
        }
    } // End of application main loop
} // End of main

/*****************************************************************************
 * Private functions
 ****************************************************************************/
static void W7500x_Init(void)
{
    ////////////////////////////////////////////////////
    // W7500x MCU Initialize
    ////////////////////////////////////////////////////

    /* Reset supervisory IC Init */
    Supervisory_IC_Init();
    
    /* Set System init */
    SystemInit_User(DEVICE_CLOCK_SELECT, DEVICE_PLL_SOURCE_CLOCK, DEVICE_TARGET_SYSTEM_CLOCK);

    /* DualTimer Initialization */
    Timer_Configuration();
    
    /* Simple UART init for Debugging */
    UART2_Configuration();

    trace_printf("Hello!\n");

    /* SysTick_Config */
    SysTick_Config((GetSystemClock()/1000));
    

#ifdef _MAIN_DEBUG_
    trace_printf("\r\n >> W7500x MCU Clock Settings ===============\r\n");
    trace_printf(" - GetPLLSource: %s, %lu (Hz)\r\n", GetPLLSource()?"External":"Internal", DEVICE_PLL_SOURCE_CLOCK);
    trace_printf("\t+ CRG->PLL_FCR: 0x%.8x\r\n", CRG->PLL_FCR);
    trace_printf(" - SetSystemClock: %lu (Hz) \r\n", DEVICE_TARGET_SYSTEM_CLOCK);
    trace_printf(" - GetSystemClock: %d (Hz) \r\n", GetSystemClock());
#endif
}

static void W7500x_WZTOE_Init(void)
{
    ////////////////////////////////////////////////////
    // W7500x WZTOE (Hardwired TCP/IP core) Initialize
    ////////////////////////////////////////////////////
    
    /* Set Network Configuration: HW Socket Tx/Rx buffer size */
    uint8_t tx_size[8] = { 4, 2, 2, 2, 2, 2, 2, 0 }; // default: { 2, 2, 2, 2, 2, 2, 2, 2 }
    uint8_t rx_size[8] = { 4, 2, 2, 2, 2, 2, 2, 0 }; // default: { 2, 2, 2, 2, 2, 2, 2, 2 }
    
#ifdef _MAIN_DEBUG_
    uint8_t i;
#endif
    
    /* Software reset the WZTOE(Hardwired TCP/IP core) */
    wizchip_sw_reset();
    
#ifdef __W7500P__ // W7500P
    GPIO_Configuration(GPIOB, GPIO_Pin_5, GPIO_Mode_IN, PAD_AF1); // COL (PB_05)
    GPIO_Configuration(GPIOB, GPIO_Pin_6, GPIO_Mode_IN, PAD_AF1); // DUP (PB_06)
#endif
    
    /* Set WZ_100US Register */
    setTIC100US((GetSystemClock()/10000));
#ifdef _MAIN_DEBUG_
    printf("\r\n >> WZTOE Settings ==========================\r\n");
    printf(" - getTIC100US: %X, (%X) \r\n", getTIC100US(), *(uint32_t *)WZTOE_TIC100US); // for debugging
#endif
    
    /* Set Network Configuration */
    wizchip_init(tx_size, rx_size);
    
#ifdef _MAIN_DEBUG_
    printf(" - WZTOE H/W Socket Buffer Settings (kB)\r\n");
    printf("   [Tx] ");
    for(i = 0; i < _WIZCHIP_SOCK_NUM_; i++) printf("%d ", getSn_TXBUF_SIZE(i));
    printf("\r\n");
    printf("   [Rx] ");
    for(i = 0; i < _WIZCHIP_SOCK_NUM_; i++) printf("%d ", getSn_RXBUF_SIZE(i));
    printf("\r\n");
#endif
}

static void set_WZTOE_NetTimeout(void)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    /* Structure for TCP timeout control: RCR, RTR */
    wiz_NetTimeout net_timeout;
    
    /* Set TCP Timeout: retry count / timeout val */
    // W7500x Retransmission retry count default: [8], Timeout val default: [2000]
    net_timeout.retry_cnt = dev_config->options.tcp_rcr_val;
    net_timeout.time_100us = 2000;
    wizchip_settimeout(&net_timeout);
    
#ifdef _MAIN_DEBUG_
    wizchip_gettimeout(&net_timeout); // TCP timeout settings
    printf(" - Network Timeout Settings - RCR: %d, RTR: %d\r\n", net_timeout.retry_cnt, net_timeout.time_100us);
#endif
}

int8_t process_dhcp(void)
{
    uint8_t ret = 0;
    uint8_t dhcp_retry = 0;

#ifdef _MAIN_DEBUG_
    printf(" - DHCP Client running\r\n");
#endif
    DHCP_init(SOCK_DHCP, g_send_buf);
    reg_dhcp_cbfunc(w7500x_dhcp_assign, w7500x_dhcp_assign, w7500x_dhcp_conflict);
    
    set_device_status(ST_UPGRADE);
    
    while(1)
    {
        ret = DHCP_run();
        
        if(ret == DHCP_IP_LEASED)
        {
#ifdef _MAIN_DEBUG_
            printf(" - DHCP Success\r\n");
#endif
            break;
        }
        else if(ret == DHCP_FAILED)
        {
            dhcp_retry++;
#ifdef _MAIN_DEBUG_
            if(dhcp_retry <= 3) printf(" - DHCP Timeout occurred and retry [%d]\r\n", dhcp_retry);
#endif
        }

        if(dhcp_retry > 3)
        {
#ifdef _MAIN_DEBUG_
            printf(" - DHCP Failed\r\n\r\n");
#endif
            DHCP_stop();
            break;
        }

        do_segcp(); // Process the requests of configuration tool during the DHCP client run.
    }
    
    set_device_status(ST_OPEN);
    
    return ret;
}


int8_t process_dns(void)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    int8_t ret = 0;
    uint8_t dns_retry = 0;
    uint8_t dns_server_ip[4];
    
#ifdef _MAIN_DEBUG_
    printf(" - DNS Client running\r\n");
#endif
    
    DNS_init(SOCK_DNS, g_send_buf);
    
    dns_server_ip[0] = dev_config->options.dns_server_ip[0];
    dns_server_ip[1] = dev_config->options.dns_server_ip[1];
    dns_server_ip[2] = dev_config->options.dns_server_ip[2];
    dns_server_ip[3] = dev_config->options.dns_server_ip[3];
    
    set_device_status(ST_UPGRADE);
    
    while(1) 
    {
        if((ret = DNS_run(dns_server_ip, (uint8_t *)dev_config->options.dns_domain_name, dev_config->network_info[0].remote_ip)) == 1)
        {
#ifdef _MAIN_DEBUG_
            printf(" - DNS Success\r\n");
#endif
            break;
        }
        else
        {
            dns_retry++;
#ifdef _MAIN_DEBUG_
            if(dns_retry <= 2) printf(" - DNS Timeout occurred and retry [%d]\r\n", dns_retry);
#endif
        }

        if(dns_retry > 2) {
#ifdef _MAIN_DEBUG_
            printf(" - DNS Failed\r\n\r\n");
#endif
            break;
        }

        do_segcp(); // Process the requests of configuration tool during the DNS client run.

        if(dev_config->options.dhcp_use) DHCP_run();
    }
    
    set_device_status(ST_OPEN);
    return ret;
}

void display_Dev_Info_header(void)
{
    DevConfig *dev_config = get_DevConfig_pointer();

    printf("\r\n");
    printf("%s\r\n", STR_BAR);
    
#if ((DEVICE_BOARD_NAME == WIZ750SR) || (DEVICE_BOARD_NAME == W7500P_S2E) || (DEVICE_BOARD_NAME == WIZ750SR_1xx))
    printf(" %s \r\n", DEVICE_ID_DEFAULT);
    printf(" >> WIZnet Serial to Ethernet Device\r\n");
#else
    #ifndef __W7500P__
        printf(" [W7500] Serial to Ethernet Device\r\n");
    #else
        printf(" [W7500P] Serial to Ethernet Device\r\n");
    #endif
#endif
    
    printf(" >> Firmware version: %d.%d.%d %s\r\n", dev_config->fw_ver[0], dev_config->fw_ver[1], dev_config->fw_ver[2], STR_VERSION_STATUS);
    printf("%s\r\n", STR_BAR);
}


void display_Dev_Info_main(void)
{
    //uint8_t i;
    DevConfig *dev_config = get_DevConfig_pointer();
    uint32_t baud_table[] = {300, 600, 1200, 1800, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200, 230400, 460800};
    
    printf(" - Device name: %s\r\n", dev_config->module_name);
    printf(" - Device mode: %s\r\n", str_working[dev_config->network_info[0].working_mode]);
    
    printf(" - Network settings: \r\n");
        printf("\t- Obtaining IP settings: [%s]\r\n", (dev_config->options.dhcp_use == 1)?"Automatic - DHCP":"Static");
        printf("\t- TCP/UDP ports\r\n");
        printf("\t   + S2E data port: [%d]\r\n", dev_config->network_info[0].local_port);
        printf("\t   + TCP/UDP setting port: [%d]\r\n", DEVICE_SEGCP_PORT);
        printf("\t   + Firmware update port: [%d]\r\n", DEVICE_FWUP_PORT);
        printf("\t- TCP Retransmission retry: [%d]\r\n", getRCR());
    
    printf(" - Search ID code: \r\n");
        printf("\t- %s: [%s]\r\n", (dev_config->options.pw_search[0] != 0)?"Enabled":"Disabled", (dev_config->options.pw_search[0] != 0)?dev_config->options.pw_search:"None");
    
    printf(" - Ethernet connection password: \r\n");
        printf("\t- %s %s\r\n", (dev_config->options.pw_connect_en == 1)?"Enabled":"Disabled", "(TCP server / mixed mode only)");
    
    printf(" - Connection timer settings: \r\n");
        printf("\t- Inactivity timer: ");
            if(dev_config->network_info[0].inactivity) printf("[%d] (sec)\r\n", dev_config->network_info[0].inactivity);
            else printf("%s\r\n", STR_DISABLED);
        printf("\t- Reconnect interval: ");
            if(dev_config->network_info[0].reconnection) printf("[%d] (msec)\r\n", dev_config->network_info[0].reconnection);
            else printf("%s\r\n", STR_DISABLED);
    
    printf(" - Serial settings: \r\n");
        printf("\t- Data %s port:  [%s%d]\r\n", STR_UART, STR_UART, SEG_DATA_UART);
        printf("\t   + UART IF: [%s]\r\n", uart_if_table[dev_config->serial_info[0].uart_interface]);
        printf("\t   + %d-", baud_table[dev_config->serial_info[0].baud_rate]);
        printf("%d-", word_len_table[dev_config->serial_info[0].data_bits]);
        printf("%s-", parity_table[dev_config->serial_info[0].parity]);
        printf("%d / ", stop_bit_table[dev_config->serial_info[0].stop_bits]);
        if(dev_config->serial_info[0].uart_interface == UART_IF_RS232_TTL)
            printf("Flow control: %s\r\n", flow_ctrl_table[dev_config->serial_info[0].flow_control]);
        else
            printf("Flow control: %s\r\n", flow_ctrl_table[0]); // RS-422/485; flow control - NONE only
        
        printf("\t- Debug %s port: [%s%d]\r\n", STR_UART, STR_UART, SEG_DEBUG_UART);
        printf("\t   + %s / %s %s\r\n", "115200-8-N-1", "NONE", "(fixed)");
        
    printf(" - Serial data packing options:\r\n");
        printf("\t- Time: ");
            if(dev_config->network_info[0].packing_time) printf("[%d] (msec)\r\n", dev_config->network_info[0].packing_time);
            else printf("%s\r\n", STR_DISABLED);
        printf("\t- Size: ");
            if(dev_config->network_info[0].packing_size) printf("[%d] (bytes)\r\n", dev_config->network_info[0].packing_size);
            else printf("%s\r\n", STR_DISABLED);
        printf("\t- Char: ");
            if(dev_config->network_info[0].packing_delimiter_length == 1) printf("[%.2X] (hex only)\r\n", dev_config->network_info[0].packing_delimiter[0]);
            else printf("%s\r\n", STR_DISABLED);
        
        printf(" - Serial command mode swtich code:\r\n");
        printf("\t- %s\r\n", (dev_config->options.serial_command == 1)?STR_ENABLED:STR_DISABLED);
        printf("\t- [%.2X][%.2X][%.2X] (Hex only)\r\n", dev_config->options.serial_trigger[0], dev_config->options.serial_trigger[1], dev_config->options.serial_trigger[2]);
    
#if ((DEVICE_BOARD_NAME == WIZ750SR) || (DEVICE_BOARD_NAME == W7500P_S2E) || (DEVICE_BOARD_NAME == WIZ750SR_1xx))
    printf(" - Hardware information: Status pins\r\n");
        printf("\t- Status 1: [%s] - %s\r\n", "PA_10", dev_config->serial_info[0].dtr_en?"DTR":"PHY link");
        printf("\t- Status 2: [%s] - %s\r\n", "PA_01", dev_config->serial_info[0].dsr_en?"DSR":"TCP connection"); // shared pin; HW_TRIG (input) / TCP connection indicator (output)

#ifdef __USE_USERS_GPIO__
    printf(" - Hardware information: User I/O pins\r\n");
        printf("\t- UserIO A: [%s] - %s / %s\r\n", "PC_13", USER_IO_TYPE_STR[get_user_io_type(USER_IO_SEL[0])], USER_IO_DIR_STR[get_user_io_direction(USER_IO_SEL[0])]); 
        printf("\t- UserIO B: [%s] - %s / %s\r\n", "PC_12", USER_IO_TYPE_STR[get_user_io_type(USER_IO_SEL[1])], USER_IO_DIR_STR[get_user_io_direction(USER_IO_SEL[1])]); 
        printf("\t- UserIO C: [%s] - %s / %s\r\n", "PC_09", USER_IO_TYPE_STR[get_user_io_type(USER_IO_SEL[2])], USER_IO_DIR_STR[get_user_io_direction(USER_IO_SEL[2])]); 
        printf("\t- UserIO D: [%s] - %s / %s\r\n", "PC_08", USER_IO_TYPE_STR[get_user_io_type(USER_IO_SEL[3])], USER_IO_DIR_STR[get_user_io_direction(USER_IO_SEL[3])]); 
#endif

#endif
    
    printf("%s\r\n", STR_BAR);
}


void display_Dev_Info_dhcp(void)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    if(dev_config->options.dhcp_use) 
    {
        if(flag_process_dhcp_success == ON) printf(" # DHCP IP Leased time : %u seconds\r\n", getDHCPLeasetime());
        else printf(" # DHCP Failed\r\n");
    }
}


void display_Dev_Info_dns(void)
{
    DevConfig *dev_config = get_DevConfig_pointer();
    
    if(dev_config->options.dns_use) 
    {
        if(flag_process_dns_success == ON)
        {
            printf(" # DNS: %s => %d.%d.%d.%d : %d\r\n", dev_config->options.dns_domain_name, 
                                                        dev_config->network_info[0].remote_ip[0],
                                                        dev_config->network_info[0].remote_ip[1],
                                                        dev_config->network_info[0].remote_ip[2],
                                                        dev_config->network_info[0].remote_ip[3],
                                                        dev_config->network_info[0].remote_port);
        }
        else printf(" # DNS Failed\r\n");
    }
    
    printf("\r\n");
}


/**
  * @brief  Inserts a delay time.
  * @param  nTime: specifies the delay time length, in milliseconds.
  * @retval None
  */
void delay(__IO uint32_t milliseconds)
{
    TimingDelay = milliseconds;
    while(TimingDelay != 0);
}


/**
  * @brief  Decrements the TimingDelay variable.
  * @param  None
  * @retval None
  */
void TimingDelay_Decrement(void)
{
    if(TimingDelay != 0x00)
    {
        TimingDelay--;
    }
}

