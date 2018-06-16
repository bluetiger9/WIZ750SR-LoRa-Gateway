/*******************************************************************************************************************************************************
 * Copyright �� 2016 <WIZnet Co.,Ltd.> 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the ��Software��), 
 * to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED ��AS IS��, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************************************************************************************************/
#include <stdio.h>
#include "W7500x_miim.h"

#ifdef __W7500P__
	// PB_05, PB_12 pull down
	*(volatile uint32_t *)(0x41003070) = 0x61;
	*(volatile uint32_t *)(0x41003054) = 0x61;
#endif

#define MAX_PHYID_CHECK_CNT     (2)

#define __DEF_DBG_LEVEL1__

extern void delay(__IO uint32_t nCount);

// Default MDC/MDIO Pin settings for W7500P
// It can be changed by W7500x_MDC / W7500x_MDIO defines
static uint16_t MDIO = GPIO_Pin_15;
static uint16_t MDC  = GPIO_Pin_14;

uint32_t PHY_ADDR_IP101G; //(phy_id())
uint32_t PHY_ADDR;// PHY_ADDR_IP101G

uint32_t link(void)
{
    uint32_t phy_status = mdio_read(GPIOB, PHYREG_STATUS);  
    uint32_t phy_status_link;
    
    phy_status_link = (phy_status>>2)&0x01;
    
    return phy_status_link; 
}

void set_link(SetLink_Type mode)
{
   uint32_t val=0;
   assert_param(IS_SETLINK_TYPE(mode));
   
   if( mode == CNTL_AUTONEGO)
   {    
      val = CNTL_AUTONEGO; 
   }
   else
   {
        val = (mode & (CNTL_SPEED|CNTL_DUPLEX)); 
   }

    mdio_write(GPIOB, PHYREG_CONTROL, val);
}


void mdio_init(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin_MDC, uint16_t GPIO_Pin_MDIO)
{
    /* Set GPIOs for MDIO and MDC */
    GPIO_InitTypeDef GPIO_InitDef;  
    GPIO_StructInit(&GPIO_InitDef); // init the structure
    GPIO_InitDef.GPIO_Pin = GPIO_Pin_MDC | GPIO_Pin_MDIO;
    GPIO_InitDef.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(GPIOx, &GPIO_InitDef);

    PAD_AFConfig(PAD_PB, GPIO_Pin_MDIO, PAD_AF1);  
    PAD_AFConfig(PAD_PB, GPIO_Pin_MDC, PAD_AF1);  

    MDC = GPIO_Pin_MDC;
    MDIO = GPIO_Pin_MDIO;

    PHY_ADDR = (phy_id());
}


void output_MDIO(GPIO_TypeDef* GPIOx, uint32_t val, uint32_t n)
{
    for(val <<= (32-n); n; val<<=1, n--)
    {
        if(val & 0x80000000)
            GPIO_SetBits(GPIOx, MDIO); 
        else
            GPIO_ResetBits(GPIOx, MDIO);

        delay(1);
        GPIO_SetBits(GPIOx, MDC); 
        delay(1);
        GPIO_ResetBits(GPIOx, MDC);
    }
}

uint32_t input_MDIO(GPIO_TypeDef* GPIOx)
{
    uint32_t i, val=0; 
    for(i=0; i<16; i++)
    {
        val <<=1;
        GPIO_SetBits(GPIOx, MDC); 
        delay(1);
        GPIO_ResetBits(GPIOx, MDC);
        delay(1);
        val |= GPIO_ReadInputDataBit(GPIOx, MDIO);
    }
    return (val);
}

void turnaround_MDIO(GPIO_TypeDef* GPIOx)
{

    GPIOx->OUTENCLR = MDIO ;

    delay(1);
    GPIO_SetBits(GPIOx, MDC); 
    delay(1);
    GPIO_ResetBits(GPIOx, MDC);
    delay(1);
}

void idle_MDIO(GPIO_TypeDef* GPIOx)
{

    GPIOx->OUTENSET = MDIO ;

    GPIO_SetBits(GPIOx,MDC); 
    delay(1);
    GPIO_ResetBits(GPIOx, MDC);
    delay(1);
}

uint32_t mdio_read(GPIO_TypeDef* GPIOx, uint32_t PhyRegAddr)
{
    uint32_t val =0;

    /* 32 Consecutive ones on MDO to establish sync */
    //printf("mdio read - sync \r\n");
    output_MDIO(GPIOx, 0xFFFFFFFF, 32);

    /* start code 01, read command (10) */
    //printf("mdio read - start \r\n");
    output_MDIO(GPIOx, 0x06, 4);

    /* write PHY address */
    //printf("mdio write- PHY address \r\n");
    output_MDIO(GPIOx, PHY_ADDR, 5);

    //printf("mdio read - PHY REG address \r\n");
    output_MDIO(GPIOx, PhyRegAddr, 5);

    /* turnaround MDO is tristated */
    //printf("mdio read - turnaround \r\n");
    turnaround_MDIO(GPIOx);

    /* Read the data value */
    //printf("mdio read - read the data value \r\n");
    val = input_MDIO(GPIOx);
    //printf("mdio read - val : %X\r\n", val );

    /* turnaround MDO is tristated */
    //printf("mdio read - idle \r\n");
    idle_MDIO(GPIOx);

    return val;
}

void mdio_write(GPIO_TypeDef* GPIOx, uint32_t PhyRegAddr, uint32_t val)
{
    /* 32 Consecutive ones on MDO to establish sync */
    //printf("mdio write- sync \r\n");
    output_MDIO(GPIOx, 0xFFFFFFFF, 32);

    /* start code 01, write command (01) */
    //printf("mdio write- start \r\n");
    output_MDIO(GPIOx, 0x05, 4);

    /* write PHY address */
    //printf("mdio write- PHY address \r\n");
    output_MDIO(GPIOx, PHY_ADDR, 5);
    
    //printf("mdio read - PHY REG address \r\n");
    output_MDIO(GPIOx, PhyRegAddr, 5);

    /* turnaround MDO */
    //printf("mdio write- turnaround (1,0)\r\n");
    output_MDIO(GPIOx, 0x02, 2);

    /* Write the data value */
    //printf("mdio writeread - read the data value \r\n");
    output_MDIO(GPIOx, val, 16);

    /* turnaround MDO is tristated */
    //printf("mdio write- idle \r\n");
    idle_MDIO(GPIOx);
}

int32_t phy_id(void)
{
    int32_t data;
    int i = 0;
    uint8_t cnt = 0;
    
    while(cnt < MAX_PHYID_CHECK_CNT)
    {
        for(i=0; i<=8; i+=1)
        {
            /* 32 Consecutive ones on MDO to establish sync */
            //printf("mdio read - sync \r\n");
            output_MDIO(GPIOB, 0xFFFFFFFF, 32);

            /* start code 01, read command (10) */
            //printf("mdio read - start \r\n");
            output_MDIO(GPIOB, 0x06, 4);

            /* write PHY address */
            //printf("mdio read - PHY address \r\n");
            output_MDIO(GPIOB, i, 5);

            //printf("mdio read - PHY REG address \r\n");
            output_MDIO(GPIOB, PHYREG_STATUS, 5);

            /* turnaround MDO is tristated */
            //printf("mdio read - turnaround \r\n");
            turnaround_MDIO(GPIOB);

            /* Read the data value */
            //printf("mdio read - read the data value \r\n");
            data = input_MDIO(GPIOB);
            //printf("mdio read - val : %X\r\n", val );

            /* turnaround MDO is tristated */
            //printf("mdio read - idle \r\n");
            idle_MDIO(GPIOB);
            
            //printf("\r\nPHY_ID = 0x%.4x , STATUS = %x, MDIO = 0x%.4x, MDC = 0x%.4x", i, data, MDIO, MDC);  //right : 0x786d // ## for debugging        
            if((data != 0x0) && (data != 0xFFFF)) return i;
            
            
        }
        cnt++;
        printf("\r\nphy id detect error!!\r\n");
    }
    return i;
}

