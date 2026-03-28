
#include <stdio.h>
#include "../Common/Include/stm32l051xx.h"
#include "../Common/Include/serial.h"

// LQFP32 pinout
//             ----------
//       VDD -|1       32|- VSS
//      PC14 -|2       31|- BOOT0
//      PC15 -|3       30|- PB7 (I2C1_SDA)
//      NRST -|4       29|- PB6 (I2C1_SCL)
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
#define SLAVE_ADDRESS 0x42

void wait_1ms(void)
{
    SysTick->LOAD = (F_CPU/1000L) - 1;  // set reload register, counter rolls over from zero, hence -1
    SysTick->VAL = 0; // load the SysTick counter
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while((SysTick->CTRL & BIT16)==0); // Bit 16 is the COUNTFLAG. True when counter rolls over from zero.
    SysTick->CTRL = 0x00; // Disable Systick counter
}

void waitms(int len)
{
    while(len--) wait_1ms();
}

// RCC is the Reset and Clock Control, which controls the clock for the various peripherals. Check page 177 of RM0451 Reference manual for more info.
void I2C_Init(void)
{
    RCC->IOPENR  |= BIT1;  // Enable clock for port B (I2C pins are in port B)
    RCC->APB1ENR |= BIT21; // Enable clock for I2C1 (page 177 of RM0451 Reference manual)

    // Configure PB6 for I2C1_SCL, pin 29 in LQFP32 package
    // Default GPIOB MODER is 11 (analog), change to 10 (AF mode). Check page 200 of RM0451
    GPIOB->MODER = (GPIOB->MODER & ~(BIT12|BIT13)) | BIT13; // PB6 AF-Mode
    // AFR[0] covers PB0-PB7. PB6 is bits 27:24 → AF1 = 0001
    GPIOB->AFR[0] |= BIT24; // AF1 selected for PB6 (I2C1_SCL)

    // Configure PB7 for I2C1_SDA, pin 30 in LQFP32 package
    GPIOB->MODER = (GPIOB->MODER & ~(BIT14|BIT15)) | BIT15; // PB7 AF-Mode
    // AFR[0] covers PB0-PB7. PB7 is bits 31:28 → AF1 = 0001
    GPIOB->AFR[0] |= BIT28; // AF1 selected for PB7 (I2C1_SDA)

    // I2C pins must be open drain (0 = push pull, 1 = open drain)
    GPIOB->OTYPER |= BIT6 | BIT7;
    GPIOB->OSPEEDR |= BIT12 | BIT14; // Medium speed for PB6 and PB7

    // I2C1 peripheral config (Check page 564 of RM0451). SCLK must be 100kHz or less.
    I2C1->TIMINGR = (uint32_t)0x70420f13; // 100kHz
    I2C1->CR1 = I2C_CR1_PE; // Enable I2C1 peripheral
}

void I2C_Send(uint8_t address, uint8_t data)
{
    I2C1->CR2 = (address << 1) | (1 << 16); // address, write direction, (1<<16) = 1 byte to send
    I2C1->CR2 |= I2C_CR2_START; // start
    while(!(I2C1->ISR & I2C_ISR_TXIS)); // wait for TXIS=1, ready to transmit
    I2C1->TXDR = data; // send data, TXDR = Transmit Data Register
    while(!(I2C1->ISR & I2C_ISR_TC)); // wait for TC=1, all data sent and acknowledged by slave
    I2C1->CR2 |= I2C_CR2_STOP; // stop must be set after TC=1, otherwise slave thinks bus is busy
}

void main(void)
{
    I2C_Init();

    // PA8 LED setup (master side indicator)
    RCC->IOPENR |= BIT0;                                      // port A clock
    GPIOA->MODER = (GPIOA->MODER & ~(BIT16|BIT17)) | BIT16;  // PA8 output

    while(1)
    {
        GPIOA->ODR |= BIT8;        // master LED on
        I2C_Send(SLAVE_ADDRESS, 0x01); // tell slave: LED on
        waitms(500);

        GPIOA->ODR &= ~BIT8;       // master LED off
        I2C_Send(SLAVE_ADDRESS, 0x00); // tell slave: LED off
        waitms(500);
    }
}