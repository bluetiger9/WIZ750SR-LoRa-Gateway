#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "W7500x_wztoe.h"
#include "W7500x_gpio.h"
#include "W7500x_board.h"
#include "socket.h"
#include "seg.h"
#include "timerHandler.h"
#include "uartHandler.h"
#include "gpioHandler.h"

/* Private define ------------------------------------------------------------*/
// Ring Buffer
BUFFER_DECLARATION(data_rx);

/* Private variables ---------------------------------------------------------*/
uint8_t flag_s2e_application_running = 0;

uint8_t opmode = DEVICE_GW_MODE;
static uint8_t mixed_state = MIXED_SERVER;
static uint8_t sw_modeswitch_at_mode_on = SEG_DISABLE;
static uint16_t client_any_port = 0;

// Timer Enable flags / Time
uint8_t enable_inactivity_timer = SEG_DISABLE;
volatile uint16_t inactivity_time = 0;

uint8_t enable_keepalive_timer = SEG_DISABLE;
volatile uint16_t keepalive_time = 0;

uint8_t enable_modeswitch_timer = SEG_DISABLE;
volatile uint16_t modeswitch_time = 0;
volatile uint16_t modeswitch_gap_time = DEFAULT_MODESWITCH_INTER_GAP;

uint8_t enable_reconnection_timer = SEG_DISABLE;
volatile uint16_t reconnection_time = 0;

uint8_t enable_serial_input_timer = SEG_DISABLE;
volatile uint16_t serial_input_time = 0;
uint8_t flag_serial_input_time_elapse = SEG_DISABLE; // for Time delimiter

// added for auth timeout
uint8_t enable_connection_auth_timer = SEG_DISABLE;
volatile uint16_t connection_auth_time = 0;

// flags
uint8_t flag_connect_pw_auth = SEG_DISABLE; // TCP_SERVER_MODE only
uint8_t flag_sent_keepalive = SEG_DISABLE;
uint8_t flag_sent_first_keepalive = SEG_DISABLE;

// static variables for function: check_modeswitch_trigger()
static uint8_t triggercode_idx;
static uint8_t ch_tmp[3];

// User's buffer / size idx
extern uint8_t g_send_buf[DATA_BUF_SIZE];
extern uint8_t g_recv_buf[DATA_BUF_SIZE];
uint16_t u2e_size = 0;
uint16_t e2u_size = 0;

// S2E Data byte count variables
volatile uint32_t s2e_uart_rx_bytecount = 0;
volatile uint32_t s2e_uart_tx_bytecount = 0;
volatile uint32_t s2e_ether_rx_bytecount = 0;
volatile uint32_t s2e_ether_tx_bytecount = 0;

// UDP: Peer netinfo
uint8_t peerip[4] = {0, };
uint8_t peerip_tmp[4] = {0xff, };
uint16_t peerport = 0;

// XON/XOFF (Software flow control) flag, Serial data can be transmitted to peer when XON enabled. 
uint8_t isXON = SEG_ENABLE;

char * str_working[] = {"TCP_CLIENT_MODE", "TCP_SERVER_MODE", "TCP_MIXED_MODE", "UDP_MODE"};

uint8_t flag_process_dhcp_success = OFF;
uint8_t flag_process_dns_success = OFF;

uint8_t isSocketOpen_TCPclient = OFF;

// ## timeflag for debugging
uint8_t tmp_timeflag_for_debug = 0;

/* Private functions prototypes ----------------------------------------------*/
void proc_SEG_tcp_client(uint8_t sock);
void proc_SEG_tcp_server(uint8_t sock);
void proc_SEG_tcp_mixed(uint8_t sock);
void proc_SEG_udp(uint8_t sock);

void uart_to_ether(uint8_t sock);
void ether_to_uart(uint8_t sock);
uint16_t get_serial_data(void);
void reset_SEG_timeflags(void);
uint8_t check_connect_pw_auth(uint8_t * buf, uint16_t len);
void restore_serial_data(uint8_t idx);

uint8_t check_tcp_connect_exception(void);

void set_device_status(teDEVSTATUS status);
uint16_t get_tcp_any_port(void);

// UART tx/rx and Ethernet tx/rx data transfer bytes counter
void add_data_transfer_bytecount(teDATADIR dir, uint16_t len);

// Serial debug messages for verifying data transfer
uint16_t debugSerial_dataTransfer(uint8_t * buf, uint16_t size, teDEBUGTYPE type);

extern void lora_write(uint8_t *buffer, size_t size);
extern size_t lora_read(uint8_t *buffer);

/* Public & Private functions ------------------------------------------------*/

void do_seg(uint8_t sock)
{
	//DevConfig *s2e = get_DevConfig_pointer();
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	struct __firmware_update *fwupdate = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
	
//#ifdef _SEG_DEBUG_
#if 1
	if(tmp_timeflag_for_debug) // every 1 sec
	{
		//if(opmode == DEVICE_GW_MODE) 	printf("working mode: %s, mixed: %s\r\n", str_working[net->working_mode], (net->working_mode == 2)?(mixed_state ? "CLIENT":"SERVER"):("NONE"));
		//else 							printf("opmode: DEVICE_AT_MODE\r\n");
		//printf("UART2Ether - ringbuf: %d, rd: %d, wr: %d, u2e_size: %d\r\n", BUFFER_USED_SIZE(data_rx), data_rx_rd, data_rx_wr, u2e_size);
		//printf("UART2Ether - ringbuf: %d, rd: %d, wr: %d, u2e_size: %d peer xon/off: %s\r\n", BUFFER_USED_SIZE(data_rx), data_rx_rd, data_rx_wr, u2e_size, isXON?"XON":"XOFF");
		//printf("modeswitch_time [%d] : modeswitch_gap_time [%d]\r\n", modeswitch_time, modeswitch_gap_time);
		//printf("[%d]: [%d] ", modeswitch_time, modeswitch_gap_time);
		//printf("idx = %d\r\n", triggercode_idx);
		//printf("opmode: %d\r\n", opmode);
		//printf("flag_connect_pw_auth: %d\r\n", flag_connect_pw_auth);
		//printf("UART2Ether - ringbuf: %d, u2e_size: %d\r\n", BUFFER_USED_SIZE(data_rx), u2e_size);
		//printf("Ether2UART - RX_RSR: %d, e2u_size: %d\r\n", getSn_RX_RSR(sock), e2u_size);
		//printf("sock_state: %x\r\n", getSn_SR(sock));
		//printf("\r\nringbuf_usedlen = %d\r\n", BUFFER_USED_SIZE(data_rx));
		//printf(" >> UART: [Rx] %u / [Tx] %u\r\n", get_data_transfer_bytecount(SEG_UART_RX), get_data_transfer_bytecount(SEG_UART_TX));
		//printf(" >> ETHER: [Rx] %u / [Tx] %u\r\n", get_data_transfer_bytecount(SEG_ETHER_RX), get_data_transfer_bytecount(SEG_ETHER_TX));
		//printf(" >> RINGBUFFER_USED_SIZE: [Rx] %d\r\n", BUFFER_USED_SIZE(data_rx));
		tmp_timeflag_for_debug = 0;
	}
#endif
	
	// Firmware update: Do not run SEG process
	if(fwupdate->fwup_flag == SEG_ENABLE) return;
	
	// Serial AT command mode enabled, initial settings
	if((opmode == DEVICE_GW_MODE) && (sw_modeswitch_at_mode_on == SEG_ENABLE))
	{
		// Socket disconnect (TCP only) / close
		process_socket_termination(sock);
		
		// Mode switch
		init_trigger_modeswitch(DEVICE_AT_MODE);
		
		// Mode switch flag disabled
		sw_modeswitch_at_mode_on = SEG_DISABLE;
	}
	
	if(opmode == DEVICE_GW_MODE) 
	{
		switch(net->working_mode)
		{
			case TCP_CLIENT_MODE:
				proc_SEG_tcp_client(sock);
				break;
			
			case TCP_SERVER_MODE:
				proc_SEG_tcp_server(sock);
				break;
			
			case TCP_MIXED_MODE:
				proc_SEG_tcp_mixed(sock);
				break;
			
			case UDP_MODE:
				proc_SEG_udp(sock);
				break;
			
			default:
				break;
		}
        
		//TODO: check_n_clear_uart_recv_status(SEG_DATA_UART);
        
		// XON/XOFF Software flow control: Check the Buffer usage and Send the start/stop commands
		// [WIZnet Device] -> [Peer]
		if((serial->flow_control == flow_xon_xoff) || serial->flow_control == flow_rts_cts) check_uart_flow_control(serial->flow_control);
	}
}

void set_device_status(teDEVSTATUS status)
{
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	
	switch(status)
	{
		case ST_OPEN:		// TCP connection: disconnected (or UDP mode)
			net->state = ST_OPEN;
			break;
		
		case ST_CONNECT:	// TCP connection: connected
			net->state = ST_CONNECT;
			break;
		
		case ST_UPGRADE:	// TCP connection: disconnected
			net->state = ST_UPGRADE;
			break;
		
		case ST_ATMODE:		// TCP connection: disconnected
			net->state = ST_ATMODE;
			break;
		
		case ST_UDP:		// UDP mode
			net->state = ST_UDP;
		default:
			break;
	}
	
	// Status indicator pins
	if(net->state == ST_CONNECT)
		set_connection_status_io(STATUS_TCPCONNECT_PIN, ON); // Status I/O pin to low
	else
		set_connection_status_io(STATUS_TCPCONNECT_PIN, OFF); // Status I/O pin to high
}

uint8_t get_device_status(void)
{
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	return net->state;
}


void proc_SEG_udp(uint8_t sock)
{
	DevConfig *s2e = get_DevConfig_pointer();
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	
	uint8_t state = getSn_SR(sock);
	switch(state)
	{
		case SOCK_UDP:
			/*if(BUFFER_USED_SIZE(data_rx) || u2e_size)*/
			uart_to_ether(sock);
			if(getSn_RX_RSR(sock) 	|| e2u_size)		ether_to_uart(sock);
			break;
			
		case SOCK_CLOSED:
			//reset_SEG_timeflags();
			BUFFER_CLEAR(data_rx);
		
			u2e_size = 0;
			e2u_size = 0;
			
			if(socket(sock, Sn_MR_UDP, net->local_port, 0) == sock)
			{
				set_device_status(ST_UDP);
				
				if(net->packing_time) modeswitch_gap_time = net->packing_time; // replace the GAP time (default: 500ms)
				
				if(serial->serial_debug_en)
				{
					printf(" > SEG:UDP_MODE:SOCKOPEN\r\n");
				}
			}
			break;
		default:
			break;
	}
}

void proc_SEG_tcp_client(uint8_t sock)
{
	DevConfig *s2e = get_DevConfig_pointer();
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	
	uint16_t source_port;
	uint8_t destip[4] = {0, };
	uint16_t destport = 0;
	
	uint8_t state = getSn_SR(sock);
	switch(state)
	{
		case SOCK_INIT:
			if(reconnection_time >= net->reconnection)
			{
				reconnection_time = 0; // reconnection time variable clear
				
				// TCP connect exception checker; e.g., dns failed / zero srcip ... and etc.
				if(check_tcp_connect_exception() == ON) return;
				
				// TCP connect
				connect(sock, net->remote_ip, net->remote_port);
#ifdef _SEG_DEBUG_
				printf(" > SEG:TCP_CLIENT_MODE:CLIENT_CONNECTION\r\n");
#endif
			}
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				///////////////////////////////////////////////////////////////////////////////////////////////////
				// S2E: TCP client mode initialize after connection established (only once)
				///////////////////////////////////////////////////////////////////////////////////////////////////
				//net->state = ST_CONNECT;
				set_device_status(ST_CONNECT);
				
				if(!inactivity_time && net->inactivity)		enable_inactivity_timer = SEG_ENABLE;
				if(!keepalive_time && net->keepalive_en)	enable_keepalive_timer = SEG_ENABLE;
				
				// TCP server mode only, This flag have to be enabled always at TCP client mode
				//if(option->pw_connect_en == SEG_ENABLE)		flag_connect_pw_auth = SEG_ENABLE;
				flag_connect_pw_auth = SEG_ENABLE;
				
				// Reconnection timer disable
				if(enable_reconnection_timer == SEG_ENABLE)
				{
					enable_reconnection_timer = SEG_DISABLE;
					reconnection_time = 0;
				}
				
				// Serial debug message printout
				if(serial->serial_debug_en)
				{
					getsockopt(sock, SO_DESTIP, &destip);
					getsockopt(sock, SO_DESTPORT, &destport);
					printf(" > SEG:CONNECTED TO - %d.%d.%d.%d : %d\r\n",destip[0], destip[1], destip[2], destip[3], destport);
				}
				
				// UART Ring buffer clear
				BUFFER_CLEAR(data_rx);
				
				// Debug message enable flag: TCP client sokect open 
				isSocketOpen_TCPclient = OFF;
				
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Serial to Ethernet process
			/*if(BUFFER_USED_SIZE(data_rx) || u2e_size)*/
			uart_to_ether(sock);
			if(getSn_RX_RSR(sock) 	|| e2u_size)		ether_to_uart(sock);
			
			// Check the inactivity timer
			if((enable_inactivity_timer == SEG_ENABLE) && (inactivity_time >= net->inactivity))
			{
				//disconnect(sock);
				process_socket_termination(sock);
				
				// Keep-alive timer disabled
				enable_keepalive_timer = DISABLE;
				keepalive_time = 0;
#ifdef _SEG_DEBUG_
				printf(" > INACTIVITY TIMER: TIMEOUT\r\n");
#endif
			}
			
			// Check the keee-alive timer
			if((net->keepalive_en == SEG_ENABLE) && (enable_keepalive_timer == SEG_ENABLE))
			{
				// Send the first keee-alive packet
				if((flag_sent_first_keepalive == SEG_DISABLE) && (keepalive_time >= net->keepalive_wait_time) && (net->keepalive_wait_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_first [%d]\r\n", keepalive_time);
#endif
					send_keepalive_packet_manual(sock); // <-> send_keepalive_packet_auto()
					keepalive_time = 0;
					
					flag_sent_first_keepalive = SEG_ENABLE;
				}
				// Send the keee-alive packet periodically
				if((flag_sent_first_keepalive == SEG_ENABLE) && (keepalive_time >= net->keepalive_retry_time) && (net->keepalive_retry_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_manual [%d]\r\n", keepalive_time);
#endif
					send_keepalive_packet_manual(sock);
					keepalive_time = 0;
				}
			}
			
			break;
		
		case SOCK_CLOSE_WAIT:
			while(getSn_RX_RSR(sock) || e2u_size) ether_to_uart(sock); // receive remaining packets
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			set_device_status(ST_OPEN);
			reset_SEG_timeflags();
			
			u2e_size = 0;
			e2u_size = 0;
			
			source_port = get_tcp_any_port();
#ifdef _SEG_DEBUG_
			printf(" > TCP CLIENT: client_any_port = %d\r\n", client_any_port);
#endif		
			// ## 20180208 Added by Eric, TCP Connect function in TCP client/mixed mode operates in non-block mode
			if(socket(sock, Sn_MR_TCP, source_port, (SF_TCP_NODELAY | SF_IO_NONBLOCK)) == sock)
			{
				// Replace the command mode switch code GAP time (default: 500ms)
				if((option->serial_command == SEG_ENABLE) && net->packing_time) modeswitch_gap_time = net->packing_time;
				
				// Enable the reconnection Timer
				if((enable_reconnection_timer == SEG_DISABLE) && net->reconnection) enable_reconnection_timer = SEG_ENABLE;
				
				if(serial->serial_debug_en)
				{
					if(isSocketOpen_TCPclient == OFF)
					{
						printf(" > SEG:TCP_CLIENT_MODE:SOCKOPEN\r\n");
						isSocketOpen_TCPclient = ON;
					}
				}
			}
		
			break;
			
		default:
			break;
	}
}

void proc_SEG_tcp_server(uint8_t sock)
{
	DevConfig *s2e = get_DevConfig_pointer();
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	
	uint8_t destip[4] = {0, };
	uint16_t destport = 0;
	
	uint8_t state = getSn_SR(sock);
	switch(state)
	{
		case SOCK_INIT:
			//listen(sock); Function call Immediately after socket open operation
			break;
		
		case SOCK_LISTEN:
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				///////////////////////////////////////////////////////////////////////////////////////////////////
				// S2E: TCP server mode initialize after connection established (only once)
				///////////////////////////////////////////////////////////////////////////////////////////////////
				//net->state = ST_CONNECT;
				set_device_status(ST_CONNECT);
				
				if(!inactivity_time && net->inactivity)		enable_inactivity_timer = SEG_ENABLE;
				//if(!keepalive_time && net->keepalive_en)	enable_keepalive_timer = SEG_ENABLE;
				
				if(option->pw_connect_en == SEG_DISABLE)	flag_connect_pw_auth = SEG_ENABLE;		// TCP server mode only (+ mixed_server)
				else
				{
					// Connection password auth timer initialize
					enable_connection_auth_timer = SEG_ENABLE;
					connection_auth_time  = 0;
				}
				
				// Serial debug message printout
				if(serial->serial_debug_en)
				{
					getsockopt(sock, SO_DESTIP, &destip);
					getsockopt(sock, SO_DESTPORT, &destport);
					printf(" > SEG:CONNECTED FROM - %d.%d.%d.%d : %d\r\n",destip[0], destip[1], destip[2], destip[3], destport);
				}
				
				// UART Ring buffer clear
				BUFFER_CLEAR(data_rx);
				
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Serial to Ethernet process
			/*if(BUFFER_USED_SIZE(data_rx) || u2e_size)*/
			uart_to_ether(sock);
			if(getSn_RX_RSR(sock) || e2u_size)	ether_to_uart(sock);
			
			// Check the inactivity timer
			if((enable_inactivity_timer == SEG_ENABLE) && (inactivity_time >= net->inactivity))
			{
				//disconnect(sock);
				process_socket_termination(sock);
				
				// Keep-alive timer disabled
				enable_keepalive_timer = DISABLE;
				keepalive_time = 0;
#ifdef _SEG_DEBUG_
				printf(" > INACTIVITY TIMER: TIMEOUT\r\n");
#endif
			}
			
			// Check the keee-alive timer
			if((net->keepalive_en == SEG_ENABLE) && (enable_keepalive_timer == SEG_ENABLE))
			{
				// Send the first keee-alive packet
				if((flag_sent_first_keepalive == SEG_DISABLE) && (keepalive_time >= net->keepalive_wait_time) && (net->keepalive_wait_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_first [%d]\r\n", keepalive_time);
#endif
					send_keepalive_packet_manual(sock); // <-> send_keepalive_packet_auto()
					keepalive_time = 0;
					
					flag_sent_first_keepalive = SEG_ENABLE;
				}
				// Send the keee-alive packet periodically
				if((flag_sent_first_keepalive == SEG_ENABLE) && (keepalive_time >= net->keepalive_retry_time) && (net->keepalive_retry_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_manual [%d]\r\n", keepalive_time);
#endif
					send_keepalive_packet_manual(sock);
					keepalive_time = 0;
				}
			}
			
			// Check the connection password auth timer
			if(option->pw_connect_en == SEG_ENABLE)
			{
				if((flag_connect_pw_auth == SEG_DISABLE) && (connection_auth_time >= MAX_CONNECTION_AUTH_TIME)) // timeout default: 5000ms (5 sec)
				{
					//disconnect(sock);
					process_socket_termination(sock);
					
					enable_connection_auth_timer = DISABLE;
					connection_auth_time = 0;
#ifdef _SEG_DEBUG_
					printf(" > CONNECTION PW: AUTH TIMEOUT\r\n");
#endif
				}
			}
			break;
		
		case SOCK_CLOSE_WAIT:
			while(getSn_RX_RSR(sock) || e2u_size) ether_to_uart(sock); // receive remaining packets
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			set_device_status(ST_OPEN);
			reset_SEG_timeflags();
			
			u2e_size = 0;
			e2u_size = 0;

			if(socket(sock, Sn_MR_TCP, net->local_port, SF_TCP_NODELAY) == sock)
			{
				// Replace the command mode switch code GAP time (default: 500ms)
				if((option->serial_command == SEG_ENABLE) && net->packing_time) modeswitch_gap_time = net->packing_time;
				
				// TCP Server listen
				listen(sock);
				
				if(serial->serial_debug_en)
				{
					printf(" > SEG:TCP_SERVER_MODE:SOCKOPEN\r\n");
				}
			}
			break;
			
		default:
			break;
	}
}


void proc_SEG_tcp_mixed(uint8_t sock)
{
	DevConfig *s2e = get_DevConfig_pointer();
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	
	uint16_t source_port = 0;
	uint8_t destip[4] = {0, };
	uint16_t destport = 0;
	
#ifdef MIXED_CLIENT_LIMITED_CONNECT
	static uint8_t reconnection_count;
#endif
	
	uint8_t state = getSn_SR(sock);
	switch(state)
	{
		case SOCK_INIT:
			if(mixed_state == MIXED_CLIENT)
			{
				if(reconnection_time >= net->reconnection)
				{
					reconnection_time = 0; // reconnection time variable clear
					
					// TCP connect exception checker; e.g., dns failed / zero srcip ... and etc.
					if(check_tcp_connect_exception() == ON)
					{
#ifdef MIXED_CLIENT_LIMITED_CONNECT
						process_socket_termination(sock);
						reconnection_count = 0;
						BUFFER_CLEAR(data_rx);
						mixed_state = MIXED_SERVER;
#endif
						return;
					}
					
					// TCP connect
					connect(sock, net->remote_ip, net->remote_port);
					
#ifdef MIXED_CLIENT_LIMITED_CONNECT
					reconnection_count++;
					if(reconnection_count >= MAX_RECONNECTION_COUNT)
					{
						process_socket_termination(sock);
						reconnection_count = 0;
						BUFFER_CLEAR(data_rx);
						mixed_state = MIXED_SERVER;
					}
	#ifdef _SEG_DEBUG_
					if(reconnection_count != 0)	printf(" > SEG:TCP_MIXED_MODE:CLIENT_CONNECTION [%d]\r\n", reconnection_count);
					else						printf(" > SEG:TCP_MIXED_MODE:CLIENT_CONNECTION_RETRY FAILED\r\n");
	#endif
#endif
				}
			}			
			break;
		
		case SOCK_LISTEN:
			// UART Rx interrupt detection in MIXED_SERVER mode
			// => Switch to MIXED_CLIENT mode
			if((mixed_state == MIXED_SERVER) && (BUFFER_USED_SIZE(data_rx)))
			{
				process_socket_termination(sock);
				mixed_state = MIXED_CLIENT;
				
				reconnection_time = net->reconnection; // rapid initial connection
			}
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				///////////////////////////////////////////////////////////////////////////////////////////////////
				// S2E: TCP mixed (server or client) mode initialize after connection established (only once)
				///////////////////////////////////////////////////////////////////////////////////////////////////
				//net->state = ST_CONNECT;
				set_device_status(ST_CONNECT);
				
				if(!inactivity_time && net->inactivity)		enable_inactivity_timer = SEG_ENABLE;
				if(!keepalive_time && net->keepalive_en)	enable_keepalive_timer = SEG_ENABLE;
				
				// Connection Password option: TCP server mode only (+ mixed_server)
				if((option->pw_connect_en == SEG_DISABLE) || (mixed_state == MIXED_CLIENT))
				{
					flag_connect_pw_auth = SEG_ENABLE;
				}
				else if((mixed_state == MIXED_SERVER) && (flag_connect_pw_auth == SEG_DISABLE))
				{
					// Connection password auth timer initialize
					enable_connection_auth_timer = SEG_ENABLE;
					connection_auth_time  = 0;
				}
				
				// Serial debug message printout
				if(serial->serial_debug_en)
				{
					getsockopt(sock, SO_DESTIP, &destip);
					getsockopt(sock, SO_DESTPORT, &destport);
					
					if(mixed_state == MIXED_SERVER)		printf(" > SEG:CONNECTED FROM - %d.%d.%d.%d : %d\r\n",destip[0], destip[1], destip[2], destip[3], destport);
					else								printf(" > SEG:CONNECTED TO - %d.%d.%d.%d : %d\r\n",destip[0], destip[1], destip[2], destip[3], destport);
				}
				
				
				// Mixed mode option init
				if(mixed_state == MIXED_SERVER)
				{
					// UART Ring buffer clear
					BUFFER_CLEAR(data_rx);
				}
				else if(mixed_state == MIXED_CLIENT)
				{
					// Mixed-mode flag switching in advance
					mixed_state = MIXED_SERVER;
				}
				
#ifdef MIXED_CLIENT_LIMITED_CONNECT
				reconnection_count = 0;
#endif
				
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Serial to Ethernet process
			/*if(BUFFER_USED_SIZE(data_rx) || u2e_size)*/
			uart_to_ether(sock);
			if(getSn_RX_RSR(sock) 	|| e2u_size)		ether_to_uart(sock);
			
			// Check the inactivity timer
			if((enable_inactivity_timer == SEG_ENABLE) && (inactivity_time >= net->inactivity))
			{
				//disconnect(sock);
				process_socket_termination(sock);
				
				// Keep-alive timer disabled
				enable_keepalive_timer = DISABLE;
				keepalive_time = 0;
#ifdef _SEG_DEBUG_
				printf(" > INACTIVITY TIMER: TIMEOUT\r\n");
#endif
				// TCP mixed mode state transition: initial state
				mixed_state = MIXED_SERVER;
			}
			
			// Check the keee-alive timer
			if((net->keepalive_en == SEG_ENABLE) && (enable_keepalive_timer == SEG_ENABLE))
			{
				// Send the first keee-alive packet
				if((flag_sent_first_keepalive == SEG_DISABLE) && (keepalive_time >= net->keepalive_wait_time) && (net->keepalive_wait_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_first [%d]\r\n", keepalive_time);
#endif
					send_keepalive_packet_manual(sock); // <-> send_keepalive_packet_auto()
					keepalive_time = 0;
					
					flag_sent_first_keepalive = SEG_ENABLE;
				}
				// Send the keee-alive packet periodically
				if((flag_sent_first_keepalive == SEG_ENABLE) && (keepalive_time >= net->keepalive_retry_time) && (net->keepalive_retry_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_manual [%d]\r\n", keepalive_time);
#endif
					send_keepalive_packet_manual(sock);
					keepalive_time = 0;
				}
			}
			
			// Check the connection password auth timer
			if((mixed_state == MIXED_SERVER) && (option->pw_connect_en == SEG_ENABLE))
			{
				if((flag_connect_pw_auth == SEG_DISABLE) && (connection_auth_time >= MAX_CONNECTION_AUTH_TIME)) // timeout default: 5000ms (5 sec)
				{
					//disconnect(sock);
					process_socket_termination(sock);
					
					enable_connection_auth_timer = DISABLE;
					connection_auth_time = 0;
#ifdef _SEG_DEBUG_
					printf(" > CONNECTION PW: AUTH TIMEOUT\r\n");
#endif
				}
			}
			break;
		
		case SOCK_CLOSE_WAIT:
			while(getSn_RX_RSR(sock) || e2u_size) ether_to_uart(sock); // receive remaining packets
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			set_device_status(ST_OPEN);

			if(mixed_state == MIXED_SERVER) // MIXED_SERVER
			{
				reset_SEG_timeflags();
				
				u2e_size = 0;
				e2u_size = 0;
				
				// ## 20180208 Added by Eric, TCP Connect function in TCP client/mixed mode operates in non-block mode
				if(socket(sock, Sn_MR_TCP, net->local_port, (SF_TCP_NODELAY | SF_IO_NONBLOCK)) == sock)
				{
					// Replace the command mode switch code GAP time (default: 500ms)
					if((option->serial_command == SEG_ENABLE) && net->packing_time) modeswitch_gap_time = net->packing_time;
					
					// TCP Server listen
					listen(sock);
					
					if(serial->serial_debug_en)
					{
						printf(" > SEG:TCP_MIXED_MODE:SERVER_SOCKOPEN\r\n");
					}
				}
			}
			else	// MIXED_CLIENT
			{
				e2u_size = 0;
				
				source_port = get_tcp_any_port();
#ifdef _SEG_DEBUG_
				printf(" > TCP CLIENT: any_port = %d\r\n", source_port);
#endif		
				if(socket(sock, Sn_MR_TCP, source_port, SF_TCP_NODELAY) == sock)
				{
					// Replace the command mode switch code GAP time (default: 500ms)
					if((option->serial_command == SEG_ENABLE) && net->packing_time) modeswitch_gap_time = net->packing_time;
					
					// Enable the reconnection Timer
					if((enable_reconnection_timer == SEG_DISABLE) && net->reconnection) enable_reconnection_timer = SEG_ENABLE;
					
					if(serial->serial_debug_en)
					{
						printf(" > SEG:TCP_MIXED_MODE:CLIENT_SOCKOPEN\r\n");
					}
				}
			}
			break;
			
		default:
			break;
	}
}

void uart_to_ether(uint8_t sock)
{
	struct __network_info *netinfo = (struct __network_info *)&(get_DevConfig_pointer()->network_info);
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	uint16_t len;
	int16_t sent_len;
	uint16_t i; // ## for debugging
	
#if ((DEVICE_BOARD_NAME == WIZ750SR) || (DEVICE_BOARD_NAME == WIZ750SR_1xx))
	if(get_phylink_in_pin() != 0) return; // PHY link down
#endif
	
	// LoRa read
	len = lora_read(g_send_buf + u2e_size);
	u2e_size += len;
	//len = get_serial_data();
	
	if(len > 0)
	{
		add_data_transfer_bytecount(SEG_UART_RX, len);
		if((serial->serial_debug_en == SEG_DEBUG_S2E) || (serial->serial_debug_en == SEG_DEBUG_ALL))
		{
			debugSerial_dataTransfer(g_send_buf, len, SEG_DEBUG_S2E);
		}
		
		switch(getSn_SR(sock))
		{
			case SOCK_UDP: // UDP_MODE
				if((netinfo->remote_ip[0] == 0x00) && (netinfo->remote_ip[1] == 0x00) && (netinfo->remote_ip[2] == 0x00) && (netinfo->remote_ip[3] == 0x00))
				{
					if((peerip[0] == 0x00) && (peerip[1] == 0x00) && (peerip[2] == 0x00) && (peerip[3] == 0x00))
					{
						if(serial->serial_debug_en) printf(" > SEG:UDP_MODE:DATA SEND FAILED - UDP Peer IP/Port required (0.0.0.0)\r\n");
					}
					else
					{
						// UDP 1:N mode
						sent_len = (int16_t)sendto(sock, g_send_buf, len, peerip, peerport);
					}
				}
				else
				{
					// UDP 1:1 mode
					sent_len = (int16_t)sendto(sock, g_send_buf, len, netinfo->remote_ip, netinfo->remote_port);
				}
				
				if(sent_len > 0) u2e_size-=sent_len;
				
				break;
			
			case SOCK_ESTABLISHED: // TCP_SERVER_MODE, TCP_CLIENT_MODE, TCP_MIXED_MODE
			case SOCK_CLOSE_WAIT:
				// Connection password is only checked in the TCP SERVER MODE / TCP MIXED MODE (MIXED_SERVER)
				if(flag_connect_pw_auth == SEG_ENABLE)
				{
					sent_len = (int16_t)send(sock, g_send_buf, len);
					if(sent_len > 0) u2e_size-=sent_len;
					
					add_data_transfer_bytecount(SEG_UART_TX, len);
					
					if(netinfo->keepalive_en == ENABLE)
					{
						if(flag_sent_first_keepalive == DISABLE)
						{
							enable_keepalive_timer = SEG_ENABLE;
						}
						else
						{
							flag_sent_first_keepalive = SEG_DISABLE;
						}
						keepalive_time = 0;
					}
				}
				break;
			
			case SOCK_LISTEN:
				u2e_size = 0;
				return;
			
			default:
				break;
		}
	}
	
	inactivity_time = 0;
	//flag_serial_input_time_elapse = SEG_DISABLE; // this flag is cleared in the 'Data packing delimiter:time' checker routine
}

uint16_t get_serial_data(void)
{
	struct __network_info *netinfo = (struct __network_info *)&(get_DevConfig_pointer()->network_info);
	uint16_t i;
	uint16_t len;
	
	len = BUFFER_USED_SIZE(data_rx);
	
	if((len + u2e_size) >= DATA_BUF_SIZE) // Avoiding u2e buffer (g_send_buf) overflow
	{
		/* Checking Data packing option: charactor delimiter */
		if((netinfo->packing_delimiter[0] != 0x00) && (len == 1))
		{
			//TODO: g_send_buf[u2e_size] = (uint8_t)uart_getc(SEG_DATA_UART);
			if(netinfo->packing_delimiter[0] == g_send_buf[u2e_size])
			{
				return u2e_size;
			}
		}
		
		// serial data length value update for avoiding u2e buffer overflow
		len = DATA_BUF_SIZE - u2e_size;
	}
	
	if((!netinfo->packing_time) && (!netinfo->packing_size) && (!netinfo->packing_delimiter[0])) // No Packing delimiters.
	{
		// ## 20150427 bugfix: Incorrect serial data storing (UART ring buffer to g_send_buf)
		for(i = 0; i < len; i++)
		{
			//TODO: g_send_buf[u2e_size++] = (uint8_t)uart_getc(SEG_DATA_UART);
		}
		
		return u2e_size;
	}
	else
	{
		/* Checking Data packing options */
		for(i = 0; i < len; i++)
		{
			//TODO: g_send_buf[u2e_size++] = (uint8_t)uart_getc(SEG_DATA_UART);
			
			// Packing delimiter: character option
			if((netinfo->packing_delimiter[0] != 0x00) && (netinfo->packing_delimiter[0] == g_send_buf[u2e_size - 1]))
			{
				return u2e_size;
			}
			
			// Packing delimiter: size option
			if((netinfo->packing_size != 0) && (netinfo->packing_size == u2e_size))
			{
				return u2e_size;
			}
		}
	}
	
	// Packing delimiter: time option
	if((netinfo->packing_time != 0) && (u2e_size != 0) && (flag_serial_input_time_elapse))
	{
		if(BUFFER_USED_SIZE(data_rx) == 0) flag_serial_input_time_elapse = SEG_DISABLE; // ##
		
		return u2e_size;
	}
	
	return 0;
}

void ether_to_uart(uint8_t sock)
{
	struct __network_info *netinfo = (struct __network_info *)&(get_DevConfig_pointer()->network_info);
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	uint16_t len;
	uint16_t i;

	if(serial->flow_control == flow_rts_cts)
	{
#ifdef __USE_GPIO_HARDWARE_FLOWCONTROL__
		//TODO: if(get_uart_cts_pin(SEG_DATA_UART) != UART_CTS_LOW) return;
#else
		; // check the CTS reg
#endif
	}

	// H/W Socket buffer -> User's buffer
	len = getSn_RX_RSR(sock);
	if(len > DATA_BUF_SIZE) len = DATA_BUF_SIZE; // avoiding buffer overflow
	
	if(len > 0)
	{
		switch(getSn_SR(sock))
		{
			case SOCK_UDP: // UDP_MODE
				e2u_size = recvfrom(sock, g_recv_buf, len, peerip, &peerport);
				
				if(memcmp(peerip_tmp, peerip, 4) !=  0)
				{
					memcpy(peerip_tmp, peerip, 4);
					if(serial->serial_debug_en) printf(" > UDP Peer IP/Port: %d.%d.%d.%d : %d\r\n", peerip[0], peerip[1], peerip[2], peerip[3], peerport);
				}
				break;
			
			case SOCK_ESTABLISHED: // TCP_SERVER_MODE, TCP_CLIENT_MODE, TCP_MIXED_MODE
			case SOCK_CLOSE_WAIT:
				e2u_size = recv(sock, g_recv_buf, len);
				break;
			
			default:
				break;
		}
		
		inactivity_time = 0;
		keepalive_time = 0;
		flag_sent_first_keepalive = DISABLE;
		
		add_data_transfer_bytecount(SEG_ETHER_RX, e2u_size);
	}
	
	if((netinfo->state == TCP_SERVER_MODE) || ((netinfo->state == TCP_MIXED_MODE) && (mixed_state == MIXED_SERVER)))
	{
		// Connection password authentication
		if((option->pw_connect_en == SEG_ENABLE) && (flag_connect_pw_auth == SEG_DISABLE))
		{
			if(check_connect_pw_auth(g_recv_buf, len) == SEG_ENABLE)
			{
				flag_connect_pw_auth = SEG_ENABLE;
			}
			else
			{
				flag_connect_pw_auth = SEG_DISABLE;
			}
			
			e2u_size = 0;
			
			if(flag_connect_pw_auth == SEG_DISABLE)
			{
				disconnect(sock);
				return;
			}
		}
	}
	
	// Ethernet data transfer to DATA UART
	if(e2u_size != 0)
	{
		lora_write(g_recv_buf, e2u_size);
		add_data_transfer_bytecount(SEG_ETHER_TX, e2u_size);
		e2u_size = 0;

//		if(serial->dsr_en == SEG_ENABLE) // DTR / DSR handshake (flowcontrol)
//		{
//			if(get_flowcontrol_dsr_pin() == 0) return;
//		}
////////////////////////////////////////////////////////////////////////
//		if(serial->uart_interface == UART_IF_RS422_485)
//		{
//			if((serial->serial_debug_en == SEG_DEBUG_E2S) || (serial->serial_debug_en == SEG_DEBUG_ALL))
//			{
//				debugSerial_dataTransfer(g_recv_buf, e2u_size, SEG_DEBUG_E2S);
//			}
//
//			//TODO: uart_rs485_enable(SEG_DATA_UART);
//			//TODO: for(i = 0; i < e2u_size; i++) uart_putc(SEG_DATA_UART, g_recv_buf[i]);
//			//TODO: uart_rs485_disable(SEG_DATA_UART);
//
//			add_data_transfer_bytecount(SEG_ETHER_TX, e2u_size);
//			e2u_size = 0;
//		}
////////////////////////////////////////////////////////////////////////
//		else if(serial->flow_control == flow_xon_xoff)
//		{
//			if(isXON == SEG_ENABLE)
//			{
//				if((serial->serial_debug_en == SEG_DEBUG_E2S) || (serial->serial_debug_en == SEG_DEBUG_ALL))
//				{
//					debugSerial_dataTransfer(g_recv_buf, e2u_size, SEG_DEBUG_E2S);
//				}
//
//				//TODO: for(i = 0; i < e2u_size; i++) uart_putc(SEG_DATA_UART, g_recv_buf[i]);
//				//TODO: add_data_transfer_bytecount(SEG_ETHER_TX, e2u_size);
//				//TODO: e2u_size = 0;
//			}
//			//else
//			//{
//			//	;//XOFF!!
//			//}
//		}
//		else
//		{
//			if((serial->serial_debug_en == SEG_DEBUG_E2S) || (serial->serial_debug_en == SEG_DEBUG_ALL))
//			{
//				debugSerial_dataTransfer(g_recv_buf, e2u_size, SEG_DEBUG_E2S);
//			}
//
//			for(i = 0; i < e2u_size; i++) uart_putc(SEG_DATA_UART, g_recv_buf[i]);
//
//			add_data_transfer_bytecount(SEG_ETHER_TX, e2u_size);
//			e2u_size = 0;
//		}
	}
}


uint16_t get_tcp_any_port(void)
{
	if(client_any_port)
	{
		if(client_any_port < 0xffff) 	client_any_port++;
		else							client_any_port = 0;
	}
	
	if(client_any_port == 0)
	{
		srand(get_phylink_downtime());
		client_any_port = (rand() % 10000) + 35000; // 35000 ~ 44999
	}
	
	return client_any_port;
}


void send_keepalive_packet_manual(uint8_t sock)
{
	setsockopt(sock, SO_KEEPALIVESEND, 0);
	
#ifdef _SEG_DEBUG_
	printf(" > SOCKET[%x]: SEND KEEP-ALIVE PACKET\r\n", sock);
#endif 
}


uint8_t process_socket_termination(uint8_t sock)
{
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	uint8_t sock_status = getSn_SR(sock);
	
	if(sock_status == SOCK_CLOSED) return sock;
	
	if(net->working_mode != UDP_MODE) // TCP_SERVER_MODE / TCP_CLIENT_MODE / TCP_MIXED_MODE
	{
		if((sock_status == SOCK_ESTABLISHED) || (sock_status == SOCK_CLOSE_WAIT))
		{
			disconnect(sock);
		}
	}
	
	close(sock);
	
	return sock;
}


uint8_t check_connect_pw_auth(uint8_t * buf, uint16_t len)
{
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	uint8_t ret = SEG_DISABLE;
	uint8_t pwbuf[11] = {0,};
	
	if(len >= sizeof(pwbuf)) len = sizeof(pwbuf) - 1;
	
	memcpy(pwbuf, buf, len);
	if((len == strlen(option->pw_connect)) && (memcmp(option->pw_connect, pwbuf, len) == 0))
	{
		ret = SEG_ENABLE; // Connection password auth success
	}
	
#ifdef _SEG_DEBUG_
	printf(" > Connection password: %s, len: %d\r\n", option->pw_connect, strlen(option->pw_connect));
	printf(" > Entered password: %s, len: %d\r\n", pwbuf, len);
	printf(" >> Auth %s\r\n", ret ? "success":"failed");
#endif
	
	return ret;
}


void init_trigger_modeswitch(uint8_t mode)
{
	struct __network_info *netinfo = (struct __network_info *)&(get_DevConfig_pointer()->network_info);
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	
	if(mode == DEVICE_AT_MODE)
	{
		opmode = DEVICE_AT_MODE;
		set_device_status(ST_ATMODE);
		
		if(serial->serial_debug_en)
		{
			printf(" > SEG:AT Mode\r\n");
			//TODO: uart_puts(SEG_DATA_UART, (uint8_t *)"SEG:AT Mode\r\n", sizeof("SEG:AT Mode\r\n"));
		}
	}
	else // DEVICE_GW_MODE
	{
		opmode = DEVICE_GW_MODE;
		set_device_status(ST_OPEN);
		if(netinfo->working_mode == TCP_MIXED_MODE) mixed_state = MIXED_SERVER;
				
		if(serial->serial_debug_en)
		{
			printf(" > SEG:GW Mode\r\n");
			//TODO: uart_puts(SEG_DATA_UART, (uint8_t *)"SEG:GW Mode\r\n", sizeof("SEG:GW Mode\r\n"));
		}
	}
	
	u2e_size = 0;
	BUFFER_CLEAR(data_rx);
	
	enable_inactivity_timer = SEG_DISABLE;
	enable_keepalive_timer = SEG_DISABLE;
	enable_serial_input_timer = SEG_DISABLE;
	enable_modeswitch_timer = SEG_DISABLE;
	
	inactivity_time = 0;
	keepalive_time = 0;
	serial_input_time = 0;
	modeswitch_time = 0;
	
	flag_serial_input_time_elapse = 0;
}

uint8_t check_modeswitch_trigger(uint8_t ch)
{
	struct __network_info *netinfo = (struct __network_info *)&(get_DevConfig_pointer()->network_info);
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	
	uint8_t modeswitch_failed = SEG_DISABLE;
	uint8_t ret = 0;
	
	if(opmode != DEVICE_GW_MODE) 				return 0;
	if(option->serial_command == SEG_DISABLE) 	return 0;
	
	switch(triggercode_idx)
	{
		case 0:
			if((ch == option->serial_trigger[triggercode_idx]) && (modeswitch_time == modeswitch_gap_time)) // comparision succeed
			{
				ch_tmp[triggercode_idx] = ch;
				triggercode_idx++;
				enable_modeswitch_timer = SEG_ENABLE;
			}
			break;
			
		case 1:
		case 2:
			if((ch == option->serial_trigger[triggercode_idx]) && (modeswitch_time < modeswitch_gap_time)) // comparision succeed
			{
				ch_tmp[triggercode_idx] = ch;
				triggercode_idx++;
			}
			else // comparision failed: invalid trigger code
			{
				modeswitch_failed = SEG_ENABLE; 
			}
			break;
		case 3:
			if(modeswitch_time < modeswitch_gap_time) // comparision failed: end gap
			{
				modeswitch_failed = SEG_ENABLE;
			}
			break;
	}
	
	if(modeswitch_failed == SEG_ENABLE)
	{
		restore_serial_data(triggercode_idx);
	}
	
	modeswitch_time = 0; // reset the inter gap time count for each trigger code recognition (Allowable interval)
	ret = triggercode_idx;
	
	return ret;
}

// when serial command mode trigger code comparision failed
void restore_serial_data(uint8_t idx)
{
	uint8_t i;
	
	for(i = 0; i < idx; i++)
	{
		BUFFER_IN(data_rx) = ch_tmp[i];
		BUFFER_IN_MOVE(data_rx, 1);
		ch_tmp[i] = 0x00;
	}
	
	enable_modeswitch_timer = SEG_DISABLE;
	triggercode_idx = 0;
}

uint8_t check_serial_store_permitted(uint8_t ch)
{
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	
	uint8_t ret = SEG_DISABLE; // SEG_DISABLE: Doesn't put the serial data in a ring buffer
	
	switch(net->state)
	{
		case ST_OPEN:
			if(net->working_mode != TCP_MIXED_MODE) break;
		case ST_CONNECT:
		case ST_UDP:
		case ST_ATMODE:
			ret = SEG_ENABLE;
			break;
		default:
			break;
	}
	
	// Software flow control: Check the XON/XOFF start/stop commands
	// [Peer] -> [WIZnet Device]
	if((ret == SEG_ENABLE) && (serial->flow_control == flow_xon_xoff))
	{
		if(ch == UART_XON)
		{
			isXON = SEG_ENABLE;
			ret = SEG_DISABLE; 
		}
		else if(ch == UART_XOFF)
		{
			isXON = SEG_DISABLE;
			ret = SEG_DISABLE;
		}
	}
	
	return ret;
}

void reset_SEG_timeflags(void)
{
	// Timer disable
	enable_inactivity_timer = SEG_DISABLE;
	enable_serial_input_timer = SEG_DISABLE;
	enable_keepalive_timer = SEG_DISABLE;
	enable_connection_auth_timer = SEG_DISABLE;
	
	// Flag clear
	flag_serial_input_time_elapse = SEG_DISABLE;
	flag_sent_keepalive = SEG_DISABLE;
	//flag_sent_keepalive_wait = SEG_DISABLE;
	flag_connect_pw_auth = SEG_DISABLE; // TCP_SERVER_MODE only (+ MIXED_SERVER)
	
	// Timer value clear
	inactivity_time = 0;
	serial_input_time = 0;
	keepalive_time = 0;
	connection_auth_time = 0;
}

void init_time_delimiter_timer(void)
{
	struct __network_info *netinfo = (struct __network_info *)&(get_DevConfig_pointer()->network_info);
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	
	if((option->serial_command == SEG_ENABLE) && (opmode == DEVICE_GW_MODE))
	{
		if(netinfo->packing_time != 0)
		{
			if(enable_serial_input_timer == SEG_DISABLE) enable_serial_input_timer = SEG_ENABLE;
			serial_input_time = 0;
		}
	}
}

uint8_t check_tcp_connect_exception(void)
{
	struct __network_info *net = (struct __network_info *)get_DevConfig_pointer()->network_info;
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	struct __options *option = (struct __options *)&(get_DevConfig_pointer()->options);
	
	uint8_t srcip[4] = {0, };
	uint8_t ret = OFF;
	
	getSIPR(srcip);
	
	// DNS failed
	if((option->dns_use == SEG_ENABLE) && (flag_process_dns_success != ON))
	{
		if(serial->serial_debug_en) printf(" > SEG:CONNECTION FAILED - DNS Failed\r\n");
		ret = ON;
	}	
	// if dhcp failed (0.0.0.0), this case do not connect to peer
	else if((srcip[0] == 0x00) && (srcip[1] == 0x00) && (srcip[2] == 0x00) && (srcip[3] == 0x00))
	{
		if(serial->serial_debug_en) printf(" > SEG:CONNECTION FAILED - Invalid IP address: Zero IP\r\n");
		ret = ON;
	}
	// Destination zero IP
	else if((net->remote_ip[0] == 0x00) && (net->remote_ip[1] == 0x00) && (net->remote_ip[2] == 0x00) && (net->remote_ip[3] == 0x00))
	{
		if(serial->serial_debug_en) printf(" > SEG:CONNECTION FAILED - Invalid Destination IP address: Zero IP\r\n");
		ret = ON;
	}
	 // Duplicate IP address
	else if((srcip[0] == net->remote_ip[0]) && (srcip[1] == net->remote_ip[1]) && (srcip[2] ==net->remote_ip[2]) && (srcip[3] == net->remote_ip[3]))
	{
		if(serial->serial_debug_en) printf(" > SEG:CONNECTION FAILED - Duplicate IP address\r\n");
		ret = ON;
	}
	else if((srcip[0] == 192) && (srcip[1] == 168)) // local IP address == Class C private IP
	{
		// Static IP address obtained
		if((option->dhcp_use == SEG_DISABLE) && ((net->remote_ip[0] == 192) && (net->remote_ip[1] == 168)))
		{
			if(srcip[2] != net->remote_ip[2]) // Class C Private IP network mismatch
			{
				if(serial->serial_debug_en) printf(" > SEG:CONNECTION FAILED - Invalid IP address range (%d.%d.[%d].%d)\r\n", net->remote_ip[0], net->remote_ip[1], net->remote_ip[2], net->remote_ip[3]);
				ret = ON; 
			}
		}
	}
	
	return ret;
}
	

void clear_data_transfer_bytecount(teDATADIR dir)
{
	switch(dir)
	{
		case SEG_ALL:
			s2e_uart_rx_bytecount = 0;
			s2e_uart_tx_bytecount = 0;
			s2e_ether_rx_bytecount = 0;
			s2e_ether_tx_bytecount = 0;
			break;
		
		case SEG_UART_RX:
			s2e_uart_rx_bytecount = 0;
			break;
		
		case SEG_UART_TX:
			s2e_uart_tx_bytecount = 0;
			break;
		
		case SEG_ETHER_RX:
			s2e_ether_rx_bytecount = 0;
			break;
		
		case SEG_ETHER_TX:
			s2e_ether_tx_bytecount = 0;
			break;
		
		default:
			break;
	}
}


void add_data_transfer_bytecount(teDATADIR dir, uint16_t len)
{
	if(len > 0)
	{
		switch(dir)
		{
			case SEG_UART_RX:
				if(s2e_uart_rx_bytecount < 0xffffffff)	s2e_uart_rx_bytecount += len;
				else 									s2e_uart_rx_bytecount = 0;
				break;
			
			case SEG_UART_TX:
				if(s2e_uart_rx_bytecount < 0xffffffff)	s2e_uart_tx_bytecount += len;
				else 									s2e_uart_tx_bytecount = 0;
				break;
			
			case SEG_ETHER_RX:
				if(s2e_uart_rx_bytecount < 0xffffffff)	s2e_ether_rx_bytecount += len;
				else 									s2e_ether_rx_bytecount = 0;
				break;
			
			case SEG_ETHER_TX:
				if(s2e_uart_rx_bytecount < 0xffffffff)	s2e_ether_tx_bytecount += len;
				else 									s2e_ether_tx_bytecount = 0;
				break;
			
			default:
				break;
		}
	}
}


uint32_t get_data_transfer_bytecount(teDATADIR dir)
{
	uint32_t ret = 0;
	
	switch(dir)
	{
		case SEG_UART_RX:
			ret = s2e_uart_rx_bytecount;
			break;
		
		case SEG_UART_TX:
			ret = s2e_uart_tx_bytecount;
			break;
		
		case SEG_ETHER_RX:
			ret = s2e_ether_rx_bytecount;
			break;
		
		case SEG_ETHER_TX:
			ret = s2e_ether_tx_bytecount;
			break;
		
		default:
			break;
	}
	return ret;
}

uint16_t debugSerial_dataTransfer(uint8_t * buf, uint16_t size, teDEBUGTYPE type)
{
    uint16_t bytecnt = 0;
    
//#ifdef __USE_DEBUG_UPTIME__
    if(getDeviceUptime_day() > 0)
        printf(" [%dd/%02d:%02d:%02d]", getDeviceUptime_day(), getDeviceUptime_hour(), getDeviceUptime_min(), getDeviceUptime_sec());
    else
        printf(" [%02d:%02d:%02d]", getDeviceUptime_hour(), getDeviceUptime_min(), getDeviceUptime_sec());
//#endif
    
    if((type == SEG_DEBUG_S2E) || (type == SEG_DEBUG_E2S))
    {
        printf("[%s][%04d] ", (type == SEG_DEBUG_S2E)?"S2E":"E2S", size);
        for(bytecnt = 0; bytecnt < size; bytecnt++) printf("%02X ", buf[bytecnt]);
        printf("\r\n");
    }
    
    return bytecnt;
}



// This function have to call every 1 millisecond by Timer IRQ handler routine.
void seg_timer_msec(void)
{
	struct __network_info *netinfo = (struct __network_info *)&(get_DevConfig_pointer()->network_info);
	
	// Firmware update timer for timeout
	// DHCP timer for timeout
	
	// SEGCP Keep-alive timer (for configuration tool, TCP mode)
	
	// Reconnection timer: Time count routine (msec)
	if(enable_reconnection_timer)
	{
		if(reconnection_time < 0xFFFF) 	reconnection_time++;
		else 							reconnection_time = 0;
	}
	
	// Keep-alive timer: Time count routine (msec)
	if(enable_keepalive_timer)
	{
		if(keepalive_time < 0xFFFF) 	keepalive_time++;
		else							keepalive_time = 0;
	}
	
	// Mode switch timer: Time count routine (msec) (GW mode <-> Serial command mode, for s/w mode switch trigger code)
	if(modeswitch_time < modeswitch_gap_time) modeswitch_time++;
	
	if((enable_modeswitch_timer) && (modeswitch_time == modeswitch_gap_time))
	{
		// result of command mode trigger code comparision
		if(triggercode_idx == 3) 	sw_modeswitch_at_mode_on = SEG_ENABLE; 	// success
		else						restore_serial_data(triggercode_idx);	// failed
		
		triggercode_idx = 0;
		enable_modeswitch_timer = SEG_DISABLE;
	}
	
	// Serial data packing time delimiter timer
	if(enable_serial_input_timer)
	{
		if(serial_input_time < netinfo->packing_time)
		{
			serial_input_time++;
		}
		else
		{
			serial_input_time = 0;
			enable_serial_input_timer = 0;
			flag_serial_input_time_elapse = 1;
		}
	}
	
	// Connection password auth timer
	if(enable_connection_auth_timer)
	{
		if(connection_auth_time < 0xffff) 	connection_auth_time++;
		else								connection_auth_time = 0;
	}
}

// This function have to call every 1 second by Timer IRQ handler routine.
void seg_timer_sec(void)
{
	// Inactivity timer: Time count routine (sec)
	if(enable_inactivity_timer)
	{
		if(inactivity_time < 0xFFFF) inactivity_time++;
	}

	tmp_timeflag_for_debug = 1;
}


