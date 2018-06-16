#include <string.h>
#include "W7500x_uart.h"
#include "W7500x_gpio.h"
#include "common.h"
#include "W7500x_board.h"
#include "ConfigData.h"
#include "uartHandler.h"
#include "seg.h"

#include <stdio.h> // for debugging

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private functions prototypes ----------------------------------------------*/
extern void delay(__IO uint32_t nCount);

/* Private functions ---------------------------------------------------------*/
int32_t uart_putc(uint8_t uartNum, uint8_t ch);
int32_t uart_puts(uint8_t uartNum, uint8_t* buf, uint16_t reqSize);
int32_t uart_getc(uint8_t uartNum);
int32_t uart_getc_nonblk(uint8_t uartNum);
int32_t uart_gets(uint8_t uartNum, uint8_t* buf, uint16_t reqSize);

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
uint8_t flag_ringbuf_full = 0;

// UART Ring buffer declaration
BUFFER_DEFINITION(data_rx, SEG_DATA_BUF_SIZE);

// UART structure declaration for switching between UART0 and UART1 
// UART selector [SEG_DATA_UART] and [SEG_DEBUG_UART] Defines are located at common.h file.

#if (SEG_DATA_UART == 0)
	UART_TypeDef * 		UART_data = UART0;
	IRQn_Type 			UART_data_irq = UART0_IRQn;
#else
	UART_TypeDef * 		UART_data = UART1;
	IRQn_Type 			UART_data_irq = UART1_IRQn;
#endif

uint8_t word_len_table[] = {7, 8, 9};
uint8_t * parity_table[] = {(uint8_t *)"N", (uint8_t *)"ODD", (uint8_t *)"EVEN"};
uint8_t stop_bit_table[] = {1, 2};
uint8_t * flow_ctrl_table[] = {(uint8_t *)"NONE", (uint8_t *)"XON/XOFF", (uint8_t *)"RTS/CTS", (uint8_t *)"RTS Only"};
uint8_t * uart_if_table[] = {(uint8_t *)UART_IF_STR_RS232_TTL, (uint8_t *)UART_IF_STR_RS422_485};

// XON/XOFF Status; 
static uint8_t xonoff_status = UART_XON;

// RTS Status; __USE_GPIO_HARDWARE_FLOWCONTROL__ defined
#ifdef __USE_GPIO_HARDWARE_FLOWCONTROL__
	static uint8_t rts_status = UART_RTS_LOW;
#endif

// UART Interface selecter; RS-422 or RS-485 use only
static uint8_t uart_if_mode = UART_IF_RS422;

/* Public functions ----------------------------------------------------------*/

////////////////////////////////////////////////////////////////////////////////
// UART Configuration & IRQ handler
// 		SEG_DATA_UART: 	UART0 or UART1
// 		SEG_DEBUG_UART:	UART2
////////////////////////////////////////////////////////////////////////////////

void S2E_UART_IRQ_Handler(UART_TypeDef * s2e_uart)
{
	uint8_t ch; // 1-byte character variable for UART Interrupt request handler
	struct __serial_info *serial = (struct __serial_info *)get_DevConfig_pointer()->serial_info;
	
	if(UART_GetITStatus(s2e_uart,  UART_IT_FLAG_RXI))
	{
		if(IS_BUFFER_FULL(data_rx))
		{
			//UartGetc(s2e_uart);
			UART_ReceiveData(s2e_uart);
			
			flag_ringbuf_full = 1;
			
			// buffer full => Serial data discard
			//BUFFER_CLEAR(data_rx); // Data-UART buffer flush -> Does not use
		}
		else
		{
#ifdef __USE_GPIO_HARDWARE_FLOWCONTROL__
			if(serial->flow_control == flow_rts_cts)
			{
				;
			}
#else
			if((serial->flow_control == flow_rts_cts) && (BUFFER_USED_SIZE(data_rx) > UART_OFF_THRESHOLD)) // CTS/RTS
			{
				; // Does nothing => RTS signal inactive
			}
			else
#endif
			{
				//ch = UartGetc(s2e_uart);
				ch = UART_ReceiveData(s2e_uart);
				
#ifdef _SEG_DEBUG_
				UART_SendData(s2e_uart, ch);	// ## UART echo; for debugging
#endif
				if(!(check_modeswitch_trigger(ch))) // ret: [0] data / [!0] trigger code
				{
					if(check_serial_store_permitted(ch)) // ret: [0] not permitted / [1] permitted
					{
						BUFFER_IN(data_rx) = ch;
						BUFFER_IN_MOVE(data_rx, 1);
					}
				}
			}
		}
		init_time_delimiter_timer();
		
		UART_ClearITPendingBit(s2e_uart, UART_IT_FLAG_RXI);
	}
/*
	// Does not use: UART Tx interrupt
	if(UART_GetITStatus(s2e_uart, UART_IT_FLAG_TXI)) 
	{
		UART_ClearITPendingBit(s2e_uart, UART_IT_FLAG_TXI);
	}
*/
}

void S2E_UART_Configuration(void)
{
	DevConfig *value = get_DevConfig_pointer();
	
	/* Configure the UARTx */
	serial_info_init(UART_data, &(value->serial_info[0])); // Load the UART_data Settings from Flash
	
	/* Configure UARTx Interrupt Enable */
	UART_ITConfig(UART_data, UART_IT_FLAG_RXI, ENABLE); // Enable the UARTx Rx interrupt only
	
	/* NVIC configuration */
	NVIC_ClearPendingIRQ(UART_data_irq);
	NVIC_SetPriority(UART_data_irq, 1);
	NVIC_EnableIRQ(UART_data_irq);
}

void UART2_Configuration(void)
{
	/* Configure UART2: Simple UART */
	/* UART2 configured as follow: 115200-8-N-1 */
	
	S_UART_Init(115200);
}

void serial_info_init(UART_TypeDef *pUART, struct __serial_info *serial)
{
	UART_InitTypeDef UART_InitStructure;
	uint32_t valid_arg = 0;
    uint32_t baud_table[] = {300, 600, 1200, 1800, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200, 230400, 460800};

	/* Set Baud Rate */
	if(serial->baud_rate < (sizeof(baud_table) / sizeof(baud_table[0])))
	{
		UART_InitStructure.UART_BaudRate = baud_table[serial->baud_rate];
		valid_arg = 1;
	}
	
	if(!valid_arg)
		UART_InitStructure.UART_BaudRate = baud_table[baud_115200];

	/* Set Data Bits */
	switch(serial->data_bits) {
		case word_len7:
			UART_InitStructure.UART_WordLength = UART_WordLength_7b;
			break;
		case word_len8:
			UART_InitStructure.UART_WordLength = UART_WordLength_8b;
			break;
		default:
			UART_InitStructure.UART_WordLength = UART_WordLength_8b;
			serial->data_bits = word_len8;
			break;
	}

	/* Set Stop Bits */
	switch(serial->stop_bits) {
		case stop_bit1:
			UART_InitStructure.UART_StopBits = UART_StopBits_1;
			break;
		case stop_bit2:
			UART_InitStructure.UART_StopBits = UART_StopBits_2;
			break;
		default:
			UART_InitStructure.UART_StopBits = UART_StopBits_1;
			serial->stop_bits = stop_bit1;
			break;
	}

	/* Set Parity Bits */
	switch(serial->parity) {
		case parity_none:
			UART_InitStructure.UART_Parity = UART_Parity_No;
			break;
		case parity_odd:
			UART_InitStructure.UART_Parity = UART_Parity_Odd;
			break;
		case parity_even:
			UART_InitStructure.UART_Parity = UART_Parity_Even;
			break;
		default:
			UART_InitStructure.UART_Parity = UART_Parity_No;
			serial->parity = parity_none;
			break;
	}
	
	/* Flow Control */
	if(serial->uart_interface == UART_IF_RS232_TTL)
	{
		// RS232 Hardware Flow Control
		//7		RTS		Request To Send		Output
		//8		CTS		Clear To Send		Input
		switch(serial->flow_control) {
			case flow_none:
				UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
				break;
			case flow_rts_cts:
#ifdef __USE_GPIO_HARDWARE_FLOWCONTROL__
				UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
				if(pUART == UART0)
				{
					GPIO_Configuration(UART0_RTS_PORT, UART0_RTS_PIN, GPIO_Mode_OUT, UART0_RTS_PAD_AF);
					GPIO_Configuration(UART0_CTS_PORT, UART0_CTS_PIN, GPIO_Mode_IN, UART0_CTS_PAD_AF);
					set_uart_rts_pin_low(0);
				}
				else if(pUART == UART1)
				{
					GPIO_Configuration(UART1_RTS_PORT, UART1_RTS_PIN, GPIO_Mode_OUT, UART1_RTS_PAD_AF);
					GPIO_Configuration(UART1_CTS_PORT, UART1_CTS_PIN, GPIO_Mode_IN, UART1_CTS_PAD_AF);
					set_uart_rts_pin_low(1);
				}
#else
				UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_RTS_CTS;
#endif
				break;
			case flow_xon_xoff:
				UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
				break;
			default:
				UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
				break;
		}
	}
	else // UART_IF_RS422_485
	{
		UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
		
		// GPIO configration (RTS pin -> GPIO: 485SEL)
		if((serial->flow_control != flow_rtsonly) && (serial->flow_control != flow_reverserts))
		{
			get_uart_rs485_sel(SEG_DATA_UART);
		}
		else
		{
			if(serial->flow_control == flow_rtsonly)
			{
				uart_if_mode = UART_IF_RS485;
			}else
			{
				uart_if_mode = UART_IF_RS485_REVERSE;
			}
		}
		uart_rs485_rs422_init(SEG_DATA_UART);
	}
	
	GPIO_Configuration(GPIOC, GPIO_Pin_14, GPIO_Mode_IN, PAD_AF1);

	/* Configure the UARTx */
	UART_InitStructure.UART_Mode = UART_Mode_Rx | UART_Mode_Tx;
	UART_Init(pUART, &UART_InitStructure);
}


void check_uart_flow_control(uint8_t flow_ctrl)
{
	if(flow_ctrl == flow_xon_xoff)
	{
		if((xonoff_status == UART_XON) && (BUFFER_USED_SIZE(data_rx) > UART_OFF_THRESHOLD)) // Send the transmit stop command to peer - go XOFF
		{
			UartPutc(UART_data, UART_XOFF);
			xonoff_status = UART_XOFF;
#ifdef _UART_DEBUG_
			printf(" >> SEND XOFF [%d / %d]\r\n", BUFFER_USED_SIZE(data_rx), SEG_DATA_BUF_SIZE);
#endif
		}
		else if((xonoff_status == UART_XOFF) && (BUFFER_USED_SIZE(data_rx) < UART_ON_THRESHOLD)) // Send the transmit start command to peer. -go XON
		{
			UartPutc(UART_data, UART_XON);
			xonoff_status = UART_XON;
#ifdef _UART_DEBUG_
			printf(" >> SEND XON [%d / %d]\r\n", BUFFER_USED_SIZE(data_rx), SEG_DATA_BUF_SIZE);
#endif
		}
	}
	else if(flow_ctrl == flow_rts_cts) // RTS pin control
	{
		// Buffer full occurred
		if((rts_status == UART_RTS_LOW) && (BUFFER_USED_SIZE(data_rx) > UART_OFF_THRESHOLD))
		{
			set_uart_rts_pin_high(SEG_DATA_UART);
			rts_status = UART_RTS_HIGH;
#ifdef _UART_DEBUG_
			printf(" >> UART_RTS_HIGH [%d / %d]\r\n", BUFFER_USED_SIZE(data_rx), SEG_DATA_BUF_SIZE);
#endif
		}
		
		// Clear the buffer full event
		if((rts_status == UART_RTS_HIGH) && (BUFFER_USED_SIZE(data_rx) <= UART_OFF_THRESHOLD))
		{
			set_uart_rts_pin_low(SEG_DATA_UART);
			rts_status = UART_RTS_LOW;
#ifdef _UART_DEBUG_
			printf(" >> UART_RTS_LOW [%d / %d]\r\n", BUFFER_USED_SIZE(data_rx), SEG_DATA_BUF_SIZE);
#endif
		}
	}
}

int32_t uart_putc(uint8_t uartNum, uint8_t ch)
{
	DevConfig *value = get_DevConfig_pointer();
	
	if(uartNum == SEG_DATA_UART)
	{
		UartPutc(UART_data, ch); 
	}
	else if(uartNum == SEG_DEBUG_UART)
	{
		S_UartPutc(ch);
	}
	return RET_OK;
}

int32_t uart_puts(uint8_t uartNum, uint8_t* buf, uint16_t reqSize)
{
	uint16_t lentot = 0;

	while(*buf != '\0' && lentot < reqSize)
	{
		if(uartNum == SEG_DATA_UART)
		{
			uart_putc(uartNum, *buf);
		}
		else if(uartNum == SEG_DEBUG_UART)
		{
			S_UartPutc(*buf);
		}
		else
			return 0;
		
		buf++;
		lentot++;
	}

	return lentot;
}

int32_t uart_getc(uint8_t uartNum)
{
	int32_t ch;

	if(uartNum == SEG_DATA_UART)
	{
		while(IS_BUFFER_EMPTY(data_rx));
		ch = (int32_t)BUFFER_OUT(data_rx);
		BUFFER_OUT_MOVE(data_rx, 1);
	}
	else if(uartNum == SEG_DEBUG_UART)
	{
		;//ch = (uint8_t)S_UartGetc();
	}
	else
		return RET_NOK;

	return ch;
}

int32_t uart_getc_nonblk(uint8_t uartNum)
{
	int32_t ch;

	if(uartNum == SEG_DATA_UART)
	{
		if(IS_BUFFER_EMPTY(data_rx)) return RET_NOK;
		ch = (int32_t)BUFFER_OUT(data_rx);
		BUFFER_OUT_MOVE(data_rx,1);
	}
	else if(uartNum == SEG_DEBUG_UART)
	{
		;
	}
	else
		return RET_NOK;

	return ch;
}

int32_t uart_gets(uint8_t uartNum, uint8_t* buf, uint16_t reqSize)
{
	uint16_t lentot = 0, len1st = 0;

	if(uartNum == SEG_DATA_UART)
	{
		lentot = reqSize = MIN(BUFFER_USED_SIZE(data_rx), reqSize);
		if(IS_BUFFER_OUT_SEPARATED(data_rx) && (len1st = BUFFER_OUT_1ST_SIZE(data_rx)) < reqSize) {
			memcpy(buf, &BUFFER_OUT(data_rx), len1st);
			BUFFER_OUT_MOVE(data_rx, len1st);
			reqSize -= len1st;
		}
		memcpy(buf+len1st, &BUFFER_OUT(data_rx), reqSize);
		BUFFER_OUT_MOVE(data_rx,reqSize);
	}
	else if(uartNum == SEG_DEBUG_UART)
	{
		;
	}
	else
		return RET_NOK;

	return lentot;
}

void uart_rx_flush(uint8_t uartNum)
{
	if(uartNum == SEG_DATA_UART)
	{
		BUFFER_CLEAR(data_rx);
	}
}

uint8_t get_uart_rs485_sel(uint8_t uartNum)
{
	if(uartNum == 0) // UART0
	{
		GPIO_Configuration(UART0_RTS_PORT, UART0_RTS_PIN, GPIO_Mode_IN, UART0_RTS_PAD_AF); // UART0 RTS pin: GPIO / Input
		GPIO_SetBits(UART0_RTS_PORT, UART0_RTS_PIN); 
		
		if(GPIO_ReadInputDataBit(UART0_RTS_PORT, UART0_RTS_PIN) == UART_IF_RS422)
		{
			uart_if_mode = UART_IF_RS422;
		}
		else
		{
			uart_if_mode = UART_IF_RS485;
		}
	}
	else
	{
		GPIO_Configuration(UART1_RTS_PORT, UART1_RTS_PIN, GPIO_Mode_IN, UART1_RTS_PAD_AF); // UART1 RTS pin: GPIO / Input
		GPIO_SetBits(UART1_RTS_PORT, UART1_RTS_PIN); 
		
		if(GPIO_ReadInputDataBit(UART1_RTS_PORT, UART1_RTS_PIN) == UART_IF_RS422) // RS-485
		{
			uart_if_mode = UART_IF_RS422;
		}
		else
		{
			uart_if_mode = UART_IF_RS485;
		}
	}
	
	return uart_if_mode;
}


void uart_rs485_rs422_init(uint8_t uartNum)
{
	if(uartNum == 0) // UART0
	{
		GPIO_Configuration(UART0_RTS_PORT, UART0_RTS_PIN, GPIO_Mode_OUT, UART0_RTS_PAD_AF); // UART0 RTS pin: GPIO / Output

		if(uart_if_mode == UART_IF_RS485)
		{
			GPIO_ResetBits(UART0_RTS_PORT, UART0_RTS_PIN); // UART0 RTS pin init, Set the signal low
		}
		else
		{
			GPIO_SetBits(UART0_RTS_PORT, UART0_RTS_PIN); // UART0 RTS pin init, Set the signal low
		}
	}
	else if(uartNum == 1) // UART1
	{
		GPIO_Configuration(UART1_RTS_PORT, UART1_RTS_PIN, GPIO_Mode_OUT, UART1_RTS_PAD_AF); // UART1 RTS pin: GPIO / Output

		if(uart_if_mode == UART_IF_RS485)
		{
			GPIO_ResetBits(UART1_RTS_PORT, UART1_RTS_PIN); // UART1 RTS pin init, Set the signal low
		}
		else
		{
			GPIO_SetBits(UART1_RTS_PORT, UART1_RTS_PIN); // UART1 RTS pin init, Set the signal low
		}
	}
}


void uart_rs485_enable(uint8_t uartNum)
{
	if(uart_if_mode == UART_IF_RS485)
	{
		// RTS pin -> High
		if(uartNum == 0) // UART0
		{
			GPIO_SetBits(UART0_RTS_PORT, UART0_RTS_PIN);
		}
		else if(uartNum == 1) // UART1
		{
			GPIO_SetBits(UART1_RTS_PORT, UART1_RTS_PIN);
		}
		delay(1);
	}
	else if(uart_if_mode == UART_IF_RS485_REVERSE)
	{
		// RTS pin -> High
		if(uartNum == 0) // UART0
		{
			GPIO_ResetBits(UART0_RTS_PORT, UART0_RTS_PIN);
		}
		else if(uartNum == 1) // UART1
		{
			GPIO_ResetBits(UART1_RTS_PORT, UART1_RTS_PIN);
		}
		delay(1);
	}
	
	//UART_IF_RS422: None
}


void uart_rs485_disable(uint8_t uartNum)
{
	if(uart_if_mode == UART_IF_RS485)
	{
		// RTS pin -> Low
		if(uartNum == 0) // UART0
		{
			GPIO_ResetBits(UART0_RTS_PORT, UART0_RTS_PIN);
		}
		else if(uartNum == 1) // UART1
		{
			GPIO_ResetBits(UART1_RTS_PORT, UART1_RTS_PIN);
		}
		delay(1);
	}
	else if(uart_if_mode == UART_IF_RS485_REVERSE)
	{
		// RTS pin -> High
		if(uartNum == 0) // UART0
		{
			GPIO_SetBits(UART0_RTS_PORT, UART0_RTS_PIN);
		}
		else if(uartNum == 1) // UART1
		{
			GPIO_SetBits(UART1_RTS_PORT, UART1_RTS_PIN);
		}
		delay(1);
	}
	
	//UART_IF_RS422: None
}

#ifdef __USE_GPIO_HARDWARE_FLOWCONTROL__
	
uint8_t get_uart_cts_pin(uint8_t uartNum)
{
	uint8_t cts_pin;

#ifdef _UART_DEBUG_
	static uint8_t prev_cts_pin;
#endif
	
	if(uartNum == 0) // UART0
	{
		cts_pin = GPIO_ReadInputDataBit(UART0_CTS_PORT, UART0_CTS_PIN); 
	}
	else if(uartNum == 1) // UART1
	{
		cts_pin = GPIO_ReadInputDataBit(UART1_CTS_PORT, UART1_CTS_PIN); 
	}
	
#ifdef _UART_DEBUG_
	if(cts_pin != prev_cts_pin)
	{
		printf(" >> UART_CTS_%s\r\n", cts_pin?"HIGH":"LOW");
		prev_cts_pin = cts_pin;
	}
#endif
	
	return cts_pin;
}

void set_uart_rts_pin_high(uint8_t uartNum)
{
	if(uartNum == 0) // UART0
	{
		GPIO_SetBits(UART0_RTS_PORT, UART0_RTS_PIN);
	}
	else if(uartNum == 1) // UART1
	{
		GPIO_SetBits(UART1_RTS_PORT, UART1_RTS_PIN);
	}
}

void set_uart_rts_pin_low(uint8_t uartNum)
{
	if(uartNum == 0) // UART0
	{
		GPIO_ResetBits(UART0_RTS_PORT, UART0_RTS_PIN);
	}
	else if(uartNum == 1) // UART1
	{
		GPIO_ResetBits(UART1_RTS_PORT, UART1_RTS_PIN);
	}
}

#endif


/**
  * @brief  None
  * @param  None
  * @retval None
  */
void check_n_clear_uart_recv_status(uint8_t channel)
{
	uint16_t dummy;
	
	UART_TypeDef* UARTx = (channel==0)?UART0:UART1;
	
	if(UARTx->STATUS.RSR != RESET)
	{
		if(UART_GetRecvStatus(UARTx, UART_RECV_STATUS_OE))
			dummy = UART_ReceiveData(UARTx);
		
		UARTx->STATUS.ECR = ~UARTx->STATUS.RSR;
	}
}
