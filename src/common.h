#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>

//////////////////////////////////
// Product Version              //
//////////////////////////////////
/* Application Firmware Version */
#define MAJOR_VER               1
#define MINOR_VER               2
#define MAINTENANCE_VER         3

//#define STR_VERSION_STATUS      "Develop" // or "Stable"
#define STR_VERSION_STATUS      "Stable"

//////////////////////////////////
// W7500x HW Socket Definition  //
//////////////////////////////////
// 0 ~ 6, Changed the S2E data socket(0)'s buffer to double
#define SOCK_MAX_USED           7

#define SOCK_DATA               0
#define SOCK_CONFIG_UDP         1
#define SOCK_CONFIG_TCP         2

#define SEG_SOCK                SOCK_DATA
#define SEGCP_UDP_SOCK          SOCK_CONFIG_UDP
#define SEGCP_TCP_SOCK          SOCK_CONFIG_TCP

#define SOCK_DHCP               3
#define SOCK_DNS                4
#define SOCK_FWUPDATE           4

//////////////////////////////////
// In/External Clock Setting    //
//////////////////////////////////
/* W7500x Board Internal / External OSC clock Settings */
#define CLOCK_SOURCE_INTERNAL   (0x0UL)
#define CLOCK_SOURCE_EXTERNAL   (0x1UL)

// Source clock frequency
#define PLL_SOURCE_8MHz         (8000000UL)     /* 8MHz Internal / External Oscillator Frequency   */
#define PLL_SOURCE_12MHz        (12000000UL)    /* 12MHz External Oscillator Frequency             */
#define PLL_SOURCE_24MHz        (24000000UL)    /* 24MHz External Oscillator Frequency             */

// Targer system clock frequency
#define SYSTEM_CLOCK_8MHz       (8000000UL)
#define SYSTEM_CLOCK_12MHz      (12000000UL)
#define SYSTEM_CLOCK_16MHz      (16000000UL)
#define SYSTEM_CLOCK_24MHz      (24000000UL)
#define SYSTEM_CLOCK_32MHz      (32000000UL)
#define SYSTEM_CLOCK_36MHz      (36000000UL)
#define SYSTEM_CLOCK_48MHz      (48000000UL)    // W7500x maximum clock frequency

//////////////////////////////////
// Ethernet                     //
//////////////////////////////////
/* Buffer size */
#define DATA_BUF_SIZE           2048
#define CONFIG_BUF_SIZE         512

//////////////////////////////////
// Available board list         //
//////////////////////////////////
#define WIZwiki_W7500           0
#define WIZwiki_W7500ECO        1
#define WIZ750SR                2
#define W7500P_S2E              3
#define WIZ750SR_1xx            5
#define UNKNOWN_DEVICE          0xff

//////////////////////////////////
// Defines                      //
//////////////////////////////////
// Defines for S2E Status
typedef enum{ST_BOOT, ST_OPEN, ST_CONNECT, ST_UPGRADE, ST_ATMODE, ST_UDP} teDEVSTATUS;   // for Device status

#define DEVICE_AT_MODE          0
#define DEVICE_GW_MODE          1

#define TCP_CLIENT_MODE         0
#define TCP_SERVER_MODE         1
#define TCP_MIXED_MODE          2
#define UDP_MODE                3

#define MIXED_SERVER            0
#define MIXED_CLIENT            1

// On/Off Status
typedef enum
{
    OFF = 0,
    ON  = 1
} OnOff_State_Type;

#define OP_COMMAND              0
#define OP_DATA                 1

#define RET_OK                  0
#define RET_NOK                 -1
#define RET_TIMEOUT             -2

#define STR_UART                "UART"
#define STR_ENABLED             "Enabled"
#define STR_DISABLED            "Disabled"
#define STR_BAR                 "=================================================="

#endif //_COMMON_H
