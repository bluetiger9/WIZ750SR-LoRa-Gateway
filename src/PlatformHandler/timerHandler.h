#ifndef TIMERHANDLER_H_
#define TIMERHANDLER_H_

#include <stdint.h>

void Timer_Configuration(void);
void Timer_IRQ_Handler(void);

////////////////////////////////////////
uint32_t getNow(void);
uint32_t getDevtime(void);
void setDevtime(uint32_t timeval_sec);
uint32_t millis(void);
////////////////////////////////////////

uint32_t getDeviceUptime_day(void);
uint32_t getDeviceUptime_hour(void);
uint8_t  getDeviceUptime_min(void);
uint8_t  getDeviceUptime_sec(void);
uint16_t getDeviceUptime_msec(void);

void set_phylink_time_check(uint8_t enable);
uint32_t get_phylink_downtime(void);

#endif /* TIMERHANDLER_H_ */
