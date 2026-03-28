#include <stdio.h>
#include "../Common/Include/stm32l051xx.h"
#include "../Common/Include/serial.h"

// LQFP32 pinout
//             ----------
//       VDD -|1       32|- VSS
//      PC14 -|2       31|- BOOT0
//      PC15 -|3       30|- PB7
//      NRST -|4       29|- PB6
//      VDDA -|5       28|- PB5
//       PA0 -|6       27|- PB4
//       PA1 -|7       26|- PB3
//       PA2 -|8       25|- PA15
//       PA3 -|9       24|- PA14
//       PA4 -|10      23|- PA13
//       PA5 -|11      22|- PA12
//       PA6 -|12      21|- PA11
//       PA7 -|13      20|- PA10 (Reserved for RXD)
//       PB0 -|14      19|- PA9  (Reserved for TXD)
//       PB1 -|15      18|- PA8  (LED+1k)
//       VSS -|16      17|- VDD
//             ----------

#define F_CPU 32000000L
#define MY_ADDRESS 0x42

/*
 * This course's ../Common/Source/startup.c installs I2C1_Handler in the vector
 * table (not CMSIS's I2C1_IRQHandler). A function named I2C1_IRQHandler is never
 * called by hardware — use I2C1_Handler for the I2C1 ISR.
 */
volatile uint8_t g_command = 0; // command from master
volatile uint8_t g_command_ready = 0; // flag to indicate command is ready

void wait_1ms(void)
{
	SysTick->LOAD = (F_CPU/1000L) -1;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
	while((SysTick->CTRL & BIT16)==0);
	SysTick->CTRL = 0x00;
}

void waitms(int len)
{
	while(len--) wait_1ms();
}

void I2C_Init(void)
{
	RCC->IOPENR |= BIT1;
	RCC->APB1ENR |= BIT21;

	GPIOB->MODER = (GPIOB->MODER & ~(BIT12|BIT13)) | BIT13;
	GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(0xF << 24)) | (0x1 << 24);

	GPIOB->MODER = (GPIOB->MODER & ~(BIT14|BIT15)) | BIT15;
	GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(0xF << 28)) | (0x1 << 28);

	GPIOB->OTYPER |= BIT6 | BIT7;
	GPIOB->OSPEEDR |= BIT12 | BIT14;

	I2C1->TIMINGR = (uint32_t)0x70420f13;
	I2C1->OAR1 = (MY_ADDRESS << 1) | I2C_OAR1_OA1EN;

	I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ADDRIE | I2C_CR1_RXIE | I2C_CR1_STOPIE;

	NVIC_SetPriority(I2C1_IRQn, 0);
	NVIC_EnableIRQ(I2C1_IRQn);
}

void I2C1_Handler(void)
{
	if (I2C1->ISR & I2C_ISR_ADDR)
		I2C1->ICR |= I2C_ICR_ADDRCF;

	if (I2C1->ISR & I2C_ISR_RXNE)
		g_command = (uint8_t)I2C1->RXDR;

	if (I2C1->ISR & I2C_ISR_STOPF)
	{
		I2C1->ICR |= I2C_ICR_STOPCF;
		g_command_ready = 1;
	}
}

void main(void)
{
	I2C_Init();
	__enable_irq();
	RCC->IOPENR |= BIT0;
	GPIOA->MODER = (GPIOA->MODER & ~(BIT16|BIT17)) | BIT16;
	GPIOA->MODER = (GPIOA->MODER & ~(BIT10|BIT11)) | BIT10; // PA5 output



	GPIOA->ODR |= BIT8;  waitms(100);
	GPIOA->ODR &= ~BIT8; waitms(100);
	GPIOA->ODR |= BIT8;  waitms(100);
	GPIOA->ODR &= ~BIT8; waitms(100);
	GPIOA->ODR |= BIT8;  waitms(100);
	GPIOA->ODR &= ~BIT8; waitms(100);

	while(1)
	{
		if (g_command_ready)
		{
			g_command_ready = 0;
			if (g_command == 0x01)
				GPIOA->ODR |= BIT8;
			else if (g_command == 0x00)
				GPIOA->ODR &= ~BIT8;

			else if (g_command == 0x02)
			{
				GPIOA->ODR |= BIT5;
				waitms(500);
				GPIOA->ODR &= ~BIT5;
			}
		}

			
	}
}
