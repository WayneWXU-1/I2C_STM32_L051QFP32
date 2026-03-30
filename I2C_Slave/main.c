#include <stdio.h>
#include "../Common/Include/stm32l051xx.h"
#include "../Common/Include/serial.h"
#include "lcd.h"

#define F_CPU 32000000L
#define MY_ADDRESS 0x42

volatile uint8_t r_data[2] = {0};
volatile uint8_t r_counter = 0;
volatile uint8_t r_data_ready = 0;

void wait_1ms(void)
{
    SysTick->LOAD = (F_CPU/1000L) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while((SysTick->CTRL & BIT16)==0);
    SysTick->CTRL = 0x00;
}

void waitms(int len)
{
    while(len--) wait_1ms();
}

// ── custom character bitmaps ──────────────────────────────────────────────────
// slot 0: unused (avoids \x00 null terminator problem in LCDprint)
// slots 1-4: normal eye (TL, TR, BL, BR)
// slots 5-7: wide eye (TL, TR, BL) — BR mirrors BL, only 8 slots total

uint8_t cgram_chars[8][8] = {
    {0,0,0,0,0,0,0,0},                                                           // 0: unused
    {0b00111,0b01111,0b11111,0b11111,0b11111,0b11111,0b01111,0b00111}, // 1: norm TL
    {0b11100,0b11110,0b11111,0b11111,0b11111,0b11111,0b11110,0b11100}, // 2: norm TR
    {0b00111,0b01111,0b11111,0b11111,0b11111,0b11111,0b01111,0b00111}, // 3: norm BL
    {0b11100,0b11110,0b11111,0b11111,0b11111,0b11111,0b11110,0b11100}, // 4: norm BR
    {0b00011,0b01111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111}, // 5: wide TL
    {0b11000,0b11110,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111}, // 6: wide TR
    {0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b01111,0b00011}, // 7: wide BL
};

void load_custom_chars(void)
{
    for (int slot = 0; slot < 8; slot++)
    {
        WriteCommand(0x40 + slot * 8);
        for (int row = 0; row < 8; row++)
            WriteData(cgram_chars[slot][row]);
    }
    WriteCommand(0x80); // back to DDRAM, required after CGRAM writes
}

// ── eye expressions ───────────────────────────────────────────────────────────

void eyes_normal(void)
{
    // \x01\x02 = norm TL/TR, \x03\x04 = norm BL/BR
    LCDprint("   \x01\x02      \x01\x02   ", 1, 0);
    LCDprint("   \x03\x04      \x03\x04   ", 2, 0);
}

void eyes_wide(void)
{
    // \x05\x06 = wide TL/TR, \x07\x07 = wide BL mirrored for BR
    LCDprint("  \x05\x06        \x05\x06  ", 1, 0);
    LCDprint("  \x07\x07        \x07\x07  ", 2, 0);
}

void eyes_panic(void)
{
    LCDprint("  >.<       >.<  ", 1, 0);
    LCDprint("  >.<       >.<  ", 2, 0);
}

void eyes_blink(void)
{
    LCDprint("                ", 1, 0);
    LCDprint("   __      __   ", 2, 0);
    waitms(120);
    eyes_normal();
}

// ── I2C ──────────────────────────────────────────────────────────────────────

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

void LCD_pins_init(void)
{
    RCC->IOPENR |= BIT0; // port A clock (already enabled but safe to repeat)

    // PA0 to PA5 as push-pull outputs (RS, E, D4, D5, D6, D7)
    GPIOA->MODER = (GPIOA->MODER & ~(BIT0|BIT1))   | BIT0;  // PA0
    GPIOA->MODER = (GPIOA->MODER & ~(BIT2|BIT3))   | BIT2;  // PA1
    GPIOA->MODER = (GPIOA->MODER & ~(BIT4|BIT5))   | BIT4;  // PA2
    GPIOA->MODER = (GPIOA->MODER & ~(BIT6|BIT7))   | BIT6;  // PA3
    GPIOA->MODER = (GPIOA->MODER & ~(BIT8|BIT9))   | BIT8;  // PA4
    GPIOA->MODER = (GPIOA->MODER & ~(BIT10|BIT11)) | BIT10; // PA5

    // all push-pull (OTYPER default is 0 = push-pull, but set explicitly)
    GPIOA->OTYPER &= ~(BIT0|BIT1|BIT2|BIT3|BIT4|BIT5);
}

void I2C1_Handler(void)
{
    if (I2C1->ISR & I2C_ISR_ADDR)
    {
        I2C1->ICR |= I2C_ICR_ADDRCF;
        r_counter = 0;
    }
    if (I2C1->ISR & I2C_ISR_RXNE)
    {
        if (r_counter < 2)
            r_data[r_counter] = I2C1->RXDR;
        r_counter++;
    }
    if (I2C1->ISR & I2C_ISR_STOPF)
    {
        I2C1->ICR |= I2C_ICR_STOPCF;
        r_data_ready = 1;
    }
}

// ── main ─────────────────────────────────────────────────────────────────────

void main(void)
{
    I2C_Init();
    __enable_irq();

    // PA8 = LED output
    RCC->IOPENR |= BIT0;
    GPIOA->MODER = (GPIOA->MODER & ~(BIT16|BIT17)) | BIT16;

    // boot blink — confirms power on before LCD init
    GPIOA->ODR |= BIT8;  waitms(100);
    GPIOA->ODR &= ~BIT8; waitms(100);
    GPIOA->ODR |= BIT8;  waitms(100);
    GPIOA->ODR &= ~BIT8; waitms(100);
    GPIOA->ODR |= BIT8;  waitms(100);
    GPIOA->ODR &= ~BIT8; waitms(100);

    LCD_pins_init();
	LCD_4BIT();
	load_custom_chars();
	eyes_normal();

    int blink_counter = 0;

    while (1)
    {
        if (r_data_ready)
        {
            r_data_ready = 0;
            uint16_t packet  = ((uint16_t)r_data[0] << 8) | r_data[1];
            uint8_t  command = (packet >> 12) & 0xF;
            uint16_t dist    = packet & 0x0FFF;

            if (command == 0x1)
            {
                // LED
                if (dist < 200)
                    GPIOA->ODR |= BIT8;
                else
                    GPIOA->ODR &= ~BIT8;

                // eyes
                if (dist < 200)
                {
                    eyes_panic();
                    blink_counter = 0;
                }
                else if (dist < 400)
                {
                    eyes_wide();
                    blink_counter = 0;
                }
                else
                {
                    blink_counter++;
                    if (blink_counter >= 25) // ~2.5s at 100ms/packet
                    {
                        eyes_blink();
                        blink_counter = 0;
                    }
                    else
                        eyes_normal();
                }
            }
        }
    }
}