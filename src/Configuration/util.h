#ifndef __UTILS_H
#define __UTILS_H

#include <stdint.h>

#define htons(X)  (X)
#define ntohs(X)  (X)
#define htonl(X)  (X)
#define ntohl(X)  (X)

uint8_t is_macaddr(uint8_t* macstr, uint8_t* digitstr, uint8_t* mac);
uint8_t is_ipaddr(uint8_t * ipaddr, uint8_t * ret_ip);
uint8_t is_hexstr(uint8_t* hexstr);
uint8_t str_to_hex(uint8_t * str, uint8_t * hex);
uint8_t is_hex(uint8_t hex);
void _mid(char* src, char* s1, char* s2, char* sub);

#endif
