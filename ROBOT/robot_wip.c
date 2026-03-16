//  Author: Krish Vashist
//  Date: 2026-03-16
//  Description: This file contains the working in progress code for the entire ROBOT setup

#include <stdio.h>
#include <stdlib.h>
#include "../Common/Include/stm32l051xx.h"



// PIN MAPPING FOR MOSFET DRIVER (configure these to MODER register)
// PB4 -> LM1 (Pin 27) - left motor forward
// PB5 -> LM2 (Pin 28) - left motor backward
// PB6 -> RM1 (Pin 29) - right motor forward
// PB7 -> RM2 (Pin 30) - right motor backward

// -- MOTOR PIN DEFINITIONS --
#define LM1 BIT4
#define LM2 BIT5
#define RM1 BIT6
#define RM2 BIT7


#define SYSCLK 32000000L
#define TICK_FREQ 1000L


volatile uint32_t StartTime = 0; 
volatile uint32_t MeasuredWidth = 0; 
volatile int STATE = 0;        // 0-> IDLE, 1-> RECEIVING BITS
volatile int BitCount = 0; 
volatile int command = 0; 
volatile int NewData_flag = 0; 

void wait(int ms) {
    for (volatile int i = 0; i < ms * (SYSCLK / TICK_FREQ) / 5; i++);
}

void Hardware_Init(void) {
    RCC->IOPENR |= BIT0;   // GPIOA Clock
    RCC->IOPENR |= BIT1;   // GPIOB Clock
    RCC->APB1ENR |= BIT0;  // Timer 2 Clock


    // Configure PA1-PA4 as Outputs for LEDs
    GPIOA->MODER = (GPIOA->MODER & ~(0x3FF)) | 0x155;

    // Configure PB4-PB7 as Outputs for Motors
    GPIOB->MODER = (GPIOB->MODER & ~(0xFF00)) | 0x5500; // 0xFF00 = 1111 1111 0000 0000 -> 8 bits set to 1 (clear the bits 8 through 15)
    GPIOB->ODR &= ~(LM1 | LM2 | RM1 | RM2); // clear all motor bits to stop the car -> intially the car must be in stopped state


    // Set up PA15 for Timer 2 Input Capture (AF2)
    GPIOA->MODER = (GPIOA->MODER & ~(BIT30)) | (BIT31); 
    GPIOA->AFR[1] |= (BIT30 | BIT28); 

    // Timer speed: 1 tick = 1 us
    TIM2->PSC = 31; 
    TIM2->ARR = 0xFFFF; 

    // Input Capture on Channel 1 (Both Edges)
    TIM2->CCMR1 |= BIT0; 
    TIM2->CCER |= (BIT1 | BIT0 | BIT3); 
    TIM2->DIER |= BIT1; 

    NVIC->ISER[0] |= BIT15; 
    TIM2->CR1 |= BIT0; 
    __enable_irq();
}

void TIM2_Handler(void) {
    if (TIM2->SR & BIT1) { 
        uint32_t currentCapture = TIM2->CCR1; 

        if (!(GPIOA->IDR & BIT15)) {
            StartTime = currentCapture;
        }
        else {
            MeasuredWidth = currentCapture - StartTime; 


            if (MeasuredWidth > 300 && MeasuredWidth < 600) {
                STATE = 1;
                BitCount = 0;
                command = 0;
            }


            else if (STATE == 1) {

                if (MeasuredWidth > 100 && MeasuredWidth < 350) { 
                    command = (command << 1);
                    BitCount++; 
                }

                else if (MeasuredWidth > 650 && MeasuredWidth < 1100) { 
                    command = (command << 1) | 1;
                    BitCount++;
                }
                else {
                    STATE = 0;
                }

                if (BitCount == 4) {
                    NewData_flag = 1; 
                    STATE = 0; 
                }
            }
        }
        TIM2->SR &= ~BIT1; 
    }
}

int main(void) {
    Hardware_Init();

    while(1) {    
        if (NewData_flag == 1) {

            // stop all the motors before giving a new command
            GPIOB->ODR &= ~(LM1 | LM2 | RM1 | RM2); 
            wait(500); 
            
            switch (command) {
                case 0b1001: // FORWARD
                    GPIOB->ODR |= LM1 | RM1; 
                    GPIOA->ODR |= BIT4; 
                    wait(5000); 
                    GPIOB->ODR &= ~(LM1 | RM1); 
                    GPIOA->ODR &= ~BIT4; 
                    break;
                case 0b0001: // LEFT
                    GPIOB->ODR |= LM2 | RM1; 
                    wait(5000); 
                    GPIOB->ODR &= ~(LM2 | RM1); 
                    break;
                case 0b1000: // RIGHT
                    GPIOB->ODR |= LM1 | RM2; 
                    wait(5000); 
                    GPIOB->ODR &= ~(LM1 | RM2); 
                    break;
                case 0b0000: // STOP
                    GPIOB->ODR &= ~(LM1 | LM2 | RM1 | RM2); 
                    wait(5000);
                    break;
            }
            NewData_flag = 0; 
        }
    }
}
