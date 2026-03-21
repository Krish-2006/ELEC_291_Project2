#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "../Common/Include/stm32l051xx.h"
#include "../Common/Include/serial.h"

// LQFP32 pinout
//              ----------
//        VDD -|1       32|- VSS
//       PC14 -|2       31|- BOOT0
//       PC15 -|3       30|- PB7 (I2C1_SDA)
//       NRST -|4       29|- PB6 (I2C1_SCL)
//       VDDA -|5       28|- PB5
//        PA0 -|6       27|- PB4
//        PA1 -|7       26|- PB3
//        PA2 -|8       25|- PA15 (IR input / TIM2_CH1)
//        PA3 -|9       24|- PA14 (RM2)
//        PA4 -|10      23|- PA13 (RM1)
//        PA5 -|11      22|- PA12 (LM2)
//        PA6 -|12      21|- PA11 (LM1)
//        PA7 -|13      20|- PA10 (USART1 RX)
//        PB0 -|14      19|- PA9  (USART1 TX)
//        PB1 -|15      18|- PA8
//        VSS -|16      17|- VDD
//              ----------

/******************************************************************************
 * CURRENT PIN / PERIPHERAL / REGISTER USAGE SUMMARY
 *
 * -------------------------
 * GPIO PINS USED IN CODE
 * -------------------------
 * PA0   -> LED1 output
 *         Used for ToF1 indicator
 *
 * PA1   -> LED2 output
 *         Used for ToF2 indicator
 *
 * PA9   -> USART1 TX
 *         Used by serial printf output through initUART(115200)
 *
 * PA10  -> USART1 RX
 *         Reserved by serial library
 *
 * PA11  -> LM1 motor output
 *         Left motor forward
 *
 * PA12  -> LM2 motor output
 *         Left motor backward
 *
 * PA13  -> RM1 motor output
 *         Right motor forward
 *
 * PA14  -> RM2 motor output
 *         Right motor backward
 *
 * PA15  -> IR receiver input
 *         Configured as TIM2_CH1 alternate function for input capture
 *
 * PB0   -> XSHUT1 output
 *         Shutdown / enable control for VL53L0X sensor #1
 *
 * PB1   -> XSHUT2 output
 *         Shutdown / enable control for VL53L0X sensor #2
 *
 * PB6   -> I2C1_SCL
 *         Shared clock line for both VL53L0X sensors
 *
 * PB7   -> I2C1_SDA
 *         Shared data line for both VL53L0X sensors
 *
 *
 * -------------------------
 * DEVICES / FUNCTIONS USING THOSE PINS
 * -------------------------
 * VL53L0X sensor #1:
 *   - SDA   -> PB7
 *   - SCL   -> PB6
 *   - XSHUT -> PB0
 *   - I2C address changed from 0x29 to 0x2A
 *
 * VL53L0X sensor #2:
 *   - SDA   -> PB7
 *   - SCL   -> PB6
 *   - XSHUT -> PB1
 *   - I2C address changed from 0x29 to 0x2B
 *
 * LEDs:
 *   - LED1 -> PA0
 *   - LED2 -> PA1
 *
 * Motors:
 *   - LM1 -> PA11
 *   - LM2 -> PA12
 *   - RM1 -> PA13
 *   - RM2 -> PA14
 *
 * IR receiver:
 *   - Input on PA15
 *   - Decoded using TIM2 input capture interrupt
 *
 * UART / serial terminal:
 *   - USART1 on PA9 / PA10
 *
 *
 * -------------------------
 * STM32 PERIPHERALS USED
 * -------------------------
 * GPIOA   -> LEDs, motor outputs, IR input pin, USART pins
 * GPIOB   -> XSHUT pins, I2C pins
 * I2C1    -> VL53L0X communication
 * TIM2    -> IR pulse width capture / decode
 * USART1  -> serial terminal output
 * SysTick -> millisecond delay routine
 * NVIC    -> TIM2 interrupt enable
 * RCC     -> peripheral clock enables
 *
 *
 * -------------------------
 * MAJOR REGISTERS TO WATCH FOR CONFLICTS
 * -------------------------
 * RCC:
 *   RCC->IOPENR
 *   RCC->APB1ENR
 *
 * GPIOA:
 *   GPIOA->MODER
 *   GPIOA->OTYPER
 *   GPIOA->PUPDR
 *   GPIOA->AFR[1]
 *   GPIOA->IDR
 *   GPIOA->BSRR
 *   GPIOA->BRR
 *
 * GPIOB:
 *   GPIOB->MODER
 *   GPIOB->OTYPER
 *   GPIOB->PUPDR
 *   GPIOB->AFR[0]
 *   GPIOB->OSPEEDR
 *   GPIOB->BSRR
 *   GPIOB->BRR
 *
 * I2C1:
 *   I2C1->CR1
 *   I2C1->CR2
 *   I2C1->ISR
 *   I2C1->TXDR
 *   I2C1->RXDR
 *   I2C1->TIMINGR
 *
 * TIM2:
 *   TIM2->CR1
 *   TIM2->PSC
 *   TIM2->ARR
 *   TIM2->CNT
 *   TIM2->CCMR1
 *   TIM2->CCER
 *   TIM2->DIER
 *   TIM2->CCR1
 *   TIM2->SR
 *
 * SysTick:
 *   SysTick->LOAD
 *   SysTick->VAL
 *   SysTick->CTRL
 *
 * NVIC:
 *   NVIC->ISER[0]
 *
 *
 * -------------------------
 * IMPORTANT CONFLICT NOTES
 * -------------------------
 * 1) Do not reuse PB6/PB7 for anything else unless you are changing the I2C bus.
 * 2) Do not reuse PB0/PB1 unless you are changing VL53L0X XSHUT wiring.
 * 3) Do not reuse PA11-PA14 unless you are changing motor wiring.
 * 4) Do not reuse PA15 unless you are changing the IR input / TIM2 capture logic.
 * 5) Do not reuse PA9/PA10 unless you are removing serial terminal support.
 * 6) Any new code that writes whole-port values to GPIOA->MODER / GPIOB->MODER /
 *    AFR / PUPDR can accidentally overwrite existing pin setup.
 * 7) Any new code using TIM2, I2C1, USART1, or SysTick may conflict with current
 *    functionality unless merged carefully.
 ******************************************************************************/

#define F_CPU 32000000L

#define TOF1_ADDR_DEFAULT          0x29
#define TOF1_ADDR_NEW              0x2A
#define TOF2_ADDR_NEW              0x2B

#define TOF1_LED_THRESHOLD_MM      100u
#define TOF2_LED_THRESHOLD_MM      100u
#define VL53L0X_OUT_OF_RANGE       8190u

#define REG_SYSRANGE_START                             0x00
#define REG_SYSTEM_SEQUENCE_CONFIG                     0x01
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO               0x0A
#define REG_SYSTEM_INTERRUPT_CLEAR                     0x0B
#define REG_RESULT_INTERRUPT_STATUS                    0x13
#define REG_RESULT_RANGE_STATUS                        0x14
#define REG_GPIO_HV_MUX_ACTIVE_HIGH                    0x84
#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV           0x89
#define REG_I2C_SLAVE_DEVICE_ADDRESS                   0x8A
#define REG_IDENTIFICATION_MODEL_ID                    0xC0

#define VL53L0X_EXPECTED_DEVICE_ID                     0xEE

#define RANGE_SEQUENCE_STEP_DSS                        0x28
#define RANGE_SEQUENCE_STEP_PRE_RANGE                  0x40
#define RANGE_SEQUENCE_STEP_FINAL_RANGE                0x80

#define LED1_PIN       0u   // PA0
#define LED2_PIN       1u   // PA1
#define XSHUT1_PIN     0u   // PB0
#define XSHUT2_PIN     1u   // PB1

#define IR_PIN         15u  // PA15 (TIM2_CH1)
#define LM1_PIN        11u  // PA11
#define LM2_PIN        12u  // PA12
#define RM1_PIN        13u  // PA13
#define RM2_PIN        14u  // PA14

#define IR_CMD_STOP        0x0u
#define IR_CMD_LEFT        0x1u
#define IR_CMD_RIGHT       0x8u
#define IR_CMD_FORWARD     0x9u

#define PINMASK(pin) (1u << (pin))

typedef enum
{
    CALIBRATION_TYPE_VHV,
    CALIBRATION_TYPE_PHASE
} calibration_type_t;

typedef struct
{
    uint8_t addr7;
    uint8_t stop_variable;
} VL53L0X_Dev;

static volatile uint16_t ir_start_time = 0;
static volatile uint16_t ir_measured_width = 0;
static volatile uint8_t ir_state = 0;
static volatile uint8_t ir_bit_count = 0;
static volatile uint8_t ir_command = 0;
static volatile uint8_t ir_new_data_flag = 0;

static void wait_1ms(void)
{
    SysTick->LOAD = (F_CPU / 1000L) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while ((SysTick->CTRL & BIT16) == 0)
    {
    }
    SysTick->CTRL = 0;
}

static void waitms(int len)
{
    while (len-- > 0)
    {
        wait_1ms();
    }
}

static void gpio_pin_mode_output(GPIO_TypeDef *port, uint8_t pin)
{
    port->MODER = (port->MODER & ~(3u << (pin * 2u))) | (1u << (pin * 2u));
}

static void gpio_pin_mode_alternate(GPIO_TypeDef *port, uint8_t pin)
{
    port->MODER = (port->MODER & ~(3u << (pin * 2u))) | (2u << (pin * 2u));
}

static void gpio_write_low(GPIO_TypeDef *port, uint8_t pin)
{
    port->BRR = PINMASK(pin);
}

static void gpio_write_high(GPIO_TypeDef *port, uint8_t pin)
{
    port->BSRR = PINMASK(pin);
}

static void gpio_write_port_mask_low(GPIO_TypeDef *port, uint32_t mask)
{
    port->BRR = mask;
}

static void gpio_write_port_mask_high(GPIO_TypeDef *port, uint32_t mask)
{
    port->BSRR = mask;
}

static void leds_init(void)
{
    RCC->IOPENR |= BIT0; // GPIOA clock

    gpio_pin_mode_output(GPIOA, LED1_PIN);
    gpio_pin_mode_output(GPIOA, LED2_PIN);

    GPIOA->OTYPER &= ~(PINMASK(LED1_PIN) | PINMASK(LED2_PIN));
    GPIOA->PUPDR &= ~((3u << (LED1_PIN * 2u)) | (3u << (LED2_PIN * 2u)));

    gpio_write_low(GPIOA, LED1_PIN);
    gpio_write_low(GPIOA, LED2_PIN);
}

static void led1_on(void)  { gpio_write_high(GPIOA, LED1_PIN); }
static void led1_off(void) { gpio_write_low(GPIOA, LED1_PIN); }
static void led2_on(void)  { gpio_write_high(GPIOA, LED2_PIN); }
static void led2_off(void) { gpio_write_low(GPIOA, LED2_PIN); }

static void motor_stop(void)
{
    gpio_write_port_mask_low(GPIOA,
        PINMASK(LM1_PIN) | PINMASK(LM2_PIN) | PINMASK(RM1_PIN) | PINMASK(RM2_PIN));
}

static void motor_forward(void)
{
    motor_stop();
    gpio_write_port_mask_high(GPIOA, PINMASK(LM1_PIN) | PINMASK(RM1_PIN));
}

static void motor_left(void)
{
    motor_stop();
    gpio_write_port_mask_high(GPIOA, PINMASK(LM2_PIN) | PINMASK(RM1_PIN));
}

static void motor_right(void)
{
    motor_stop();
    gpio_write_port_mask_high(GPIOA, PINMASK(LM1_PIN) | PINMASK(RM2_PIN));
}

static void motor_ir_init(void)
{
    RCC->IOPENR |= BIT0;   // GPIOA clock
    RCC->APB1ENR |= BIT0;  // TIM2 clock

    gpio_pin_mode_output(GPIOA, LM1_PIN);
    gpio_pin_mode_output(GPIOA, LM2_PIN);
    gpio_pin_mode_output(GPIOA, RM1_PIN);
    gpio_pin_mode_output(GPIOA, RM2_PIN);

    GPIOA->OTYPER &= ~(PINMASK(LM1_PIN) | PINMASK(LM2_PIN) | PINMASK(RM1_PIN) | PINMASK(RM2_PIN));
    GPIOA->PUPDR &= ~((3u << (LM1_PIN * 2u)) |
                      (3u << (LM2_PIN * 2u)) |
                      (3u << (RM1_PIN * 2u)) |
                      (3u << (RM2_PIN * 2u)));

    motor_stop();

    // PA15 as TIM2_CH1 alternate function AF5
    gpio_pin_mode_alternate(GPIOA, IR_PIN);
    GPIOA->PUPDR &= ~(3u << (IR_PIN * 2u));
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~(0xFu << ((IR_PIN - 8u) * 4u))) | (5u << ((IR_PIN - 8u) * 4u));

    // Timer 2 at 1 MHz => 1 tick = 1 us
    TIM2->CR1 = 0;
    TIM2->PSC = 31;
    TIM2->ARR = 0xFFFF;
    TIM2->CNT = 0;

    // CH1 input capture on TI1, both edges
    TIM2->CCMR1 &= ~(3u << 0);
    TIM2->CCMR1 |= BIT0;
    TIM2->CCER &= ~(BIT1 | BIT3);
    TIM2->CCER |= BIT0 | BIT1 | BIT3;
    TIM2->DIER |= BIT1;
    TIM2->CR1 |= BIT0;

    NVIC->ISER[0] |= BIT15;
    __enable_irq();
}

void TIM2_Handler(void)
{
    if (TIM2->SR & BIT1)
    {
        uint16_t current_capture = (uint16_t)TIM2->CCR1;

        if ((GPIOA->IDR & PINMASK(IR_PIN)) == 0u)
        {
            ir_start_time = current_capture;
        }
        else
        {
            ir_measured_width = (uint16_t)(current_capture - ir_start_time);

            if ((ir_measured_width > 300u) && (ir_measured_width < 600u))
            {
                ir_state = 1u;
                ir_bit_count = 0u;
                ir_command = 0u;
            }
            else if (ir_state == 1u)
            {
                if ((ir_measured_width > 100u) && (ir_measured_width < 350u))
                {
                    ir_command = (uint8_t)(ir_command << 1);
                    ir_bit_count++;
                }
                else if ((ir_measured_width > 650u) && (ir_measured_width < 1100u))
                {
                    ir_command = (uint8_t)((ir_command << 1) | 1u);
                    ir_bit_count++;
                }
                else
                {
                    ir_state = 0u;
                    ir_bit_count = 0u;
                }

                if (ir_bit_count >= 4u)
                {
                    ir_new_data_flag = 1u;
                    ir_state = 0u;
                }
            }
        }

        TIM2->SR &= ~BIT1;
    }
}

static void xshut_init(void)
{
    RCC->IOPENR |= BIT1; // GPIOB clock

    gpio_pin_mode_output(GPIOB, XSHUT1_PIN);
    gpio_pin_mode_output(GPIOB, XSHUT2_PIN);
    GPIOB->OTYPER &= ~(PINMASK(XSHUT1_PIN) | PINMASK(XSHUT2_PIN));
    GPIOB->PUPDR &= ~((3u << (XSHUT1_PIN * 2u)) | (3u << (XSHUT2_PIN * 2u)));

    gpio_write_low(GPIOB, XSHUT1_PIN);
    gpio_write_low(GPIOB, XSHUT2_PIN);
}

static void xshut1_low(void)
{
    gpio_pin_mode_output(GPIOB, XSHUT1_PIN);
    gpio_write_low(GPIOB, XSHUT1_PIN);
}

static void xshut2_low(void)
{
    gpio_pin_mode_output(GPIOB, XSHUT2_PIN);
    gpio_write_low(GPIOB, XSHUT2_PIN);
}

static void xshut1_release(void)
{
    gpio_pin_mode_output(GPIOB, XSHUT1_PIN);
    gpio_write_high(GPIOB, XSHUT1_PIN);
}

static void xshut2_release(void)
{
    gpio_pin_mode_output(GPIOB, XSHUT2_PIN);
    gpio_write_high(GPIOB, XSHUT2_PIN);
}

static void I2C_init(void)
{
    RCC->IOPENR |= BIT1;   // GPIOB clock
    RCC->APB1ENR |= (1u << 21); // I2C1 clock

    // PB6 -> I2C1_SCL AF1
    GPIOB->MODER = (GPIOB->MODER & ~(3u << (6u * 2u))) | (2u << (6u * 2u));
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(0xFu << (6u * 4u))) | (1u << (6u * 4u));

    // PB7 -> I2C1_SDA AF1
    GPIOB->MODER = (GPIOB->MODER & ~(3u << (7u * 2u))) | (2u << (7u * 2u));
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(0xFu << (7u * 4u))) | (1u << (7u * 4u));

    GPIOB->OTYPER |= PINMASK(6u) | PINMASK(7u);
    GPIOB->OSPEEDR |= (1u << (6u * 2u)) | (1u << (7u * 2u));

    // 100 kHz-ish timing from original example
    I2C1->TIMINGR = (uint32_t)0x70420f13;
}

static bool i2c_write_reg8(uint8_t addr7, uint8_t reg, uint8_t value)
{
    I2C1->CR1 = I2C_CR1_PE;
    I2C1->CR2 = I2C_CR2_AUTOEND | (2u << 16) | ((uint32_t)addr7 << 1);
    I2C1->CR2 |= I2C_CR2_START;

    while ((I2C1->ISR & I2C_ISR_TXE) != I2C_ISR_TXE)
    {
    }

    I2C1->TXDR = reg;
    while ((I2C1->ISR & I2C_ISR_TXE) != I2C_ISR_TXE)
    {
    }

    I2C1->TXDR = value;
    while ((I2C1->ISR & I2C_ISR_TXE) != I2C_ISR_TXE)
    {
    }

    waitms(1);
    return true;
}

static bool i2c_read_reg8(uint8_t addr7, uint8_t reg, uint8_t *value)
{
    I2C1->CR1 = I2C_CR1_PE;
    I2C1->CR2 = I2C_CR2_AUTOEND | (1u << 16) | ((uint32_t)addr7 << 1);
    I2C1->CR2 |= I2C_CR2_START;

    while ((I2C1->ISR & I2C_ISR_TXE) != I2C_ISR_TXE)
    {
    }

    I2C1->TXDR = reg;
    while ((I2C1->ISR & I2C_ISR_TXE) != I2C_ISR_TXE)
    {
    }

    waitms(1);

    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_RXIE;
    I2C1->CR2 = I2C_CR2_AUTOEND | (1u << 16) | I2C_CR2_RD_WRN | ((uint32_t)addr7 << 1);
    I2C1->CR2 |= I2C_CR2_START;

    while ((I2C1->ISR & I2C_ISR_RXNE) != I2C_ISR_RXNE)
    {
    }

    *value = (uint8_t)I2C1->RXDR;
    waitms(1);
    return true;
}

static bool i2c_read_reg16(uint8_t addr7, uint8_t reg, uint16_t *value)
{
    I2C1->CR1 = I2C_CR1_PE;
    I2C1->CR2 = I2C_CR2_AUTOEND | (1u << 16) | ((uint32_t)addr7 << 1);
    I2C1->CR2 |= I2C_CR2_START;

    while ((I2C1->ISR & I2C_ISR_TXE) != I2C_ISR_TXE)
    {
    }

    I2C1->TXDR = reg;
    while ((I2C1->ISR & I2C_ISR_TXE) != I2C_ISR_TXE)
    {
    }

    waitms(1);

    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_RXIE;
    I2C1->CR2 = I2C_CR2_AUTOEND | (2u << 16) | I2C_CR2_RD_WRN | ((uint32_t)addr7 << 1);
    I2C1->CR2 |= I2C_CR2_START;

    while ((I2C1->ISR & I2C_ISR_RXNE) != I2C_ISR_RXNE)
    {
    }
    *value = ((uint16_t)I2C1->RXDR) << 8;

    while ((I2C1->ISR & I2C_ISR_RXNE) != I2C_ISR_RXNE)
    {
    }
    *value |= (uint16_t)I2C1->RXDR;

    waitms(1);
    return true;
}

static bool vl53l0x_device_is_booted(VL53L0X_Dev *dev)
{
    uint8_t device_id = 0;

    if (!i2c_read_reg8(dev->addr7, REG_IDENTIFICATION_MODEL_ID, &device_id))
    {
        return false;
    }

    return (device_id == VL53L0X_EXPECTED_DEVICE_ID);
}

static bool vl53l0x_set_address(VL53L0X_Dev *dev, uint8_t new_addr7)
{
    if (!i2c_write_reg8(dev->addr7, REG_I2C_SLAVE_DEVICE_ADDRESS, (uint8_t)(new_addr7 & 0x7Fu)))
    {
        return false;
    }

    dev->addr7 = new_addr7;
    return true;
}

static bool vl53l0x_data_init(VL53L0X_Dev *dev)
{
    bool success = false;
    uint8_t vhv_config_scl_sda = 0;

    if (!i2c_read_reg8(dev->addr7, REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, &vhv_config_scl_sda))
    {
        return false;
    }

    vhv_config_scl_sda |= 0x01;
    if (!i2c_write_reg8(dev->addr7, REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, vhv_config_scl_sda))
    {
        return false;
    }

    success = i2c_write_reg8(dev->addr7, 0x88, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x80, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x00, 0x00);
    success &= i2c_read_reg8(dev->addr7, 0x91, &dev->stop_variable);
    success &= i2c_write_reg8(dev->addr7, 0x00, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x80, 0x00);

    return success;
}

static bool vl53l0x_load_default_tuning_settings(VL53L0X_Dev *dev)
{
    bool success;

    success = i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x00, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x09, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x10, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x11, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x24, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x25, 0xFF);
    success &= i2c_write_reg8(dev->addr7, 0x75, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x4E, 0x2C);
    success &= i2c_write_reg8(dev->addr7, 0x48, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x30, 0x20);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x30, 0x09);
    success &= i2c_write_reg8(dev->addr7, 0x54, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x31, 0x04);
    success &= i2c_write_reg8(dev->addr7, 0x32, 0x03);
    success &= i2c_write_reg8(dev->addr7, 0x40, 0x83);
    success &= i2c_write_reg8(dev->addr7, 0x46, 0x25);
    success &= i2c_write_reg8(dev->addr7, 0x60, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x27, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x50, 0x06);
    success &= i2c_write_reg8(dev->addr7, 0x51, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x52, 0x96);
    success &= i2c_write_reg8(dev->addr7, 0x56, 0x08);
    success &= i2c_write_reg8(dev->addr7, 0x57, 0x30);
    success &= i2c_write_reg8(dev->addr7, 0x61, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x62, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x64, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x65, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x66, 0xA0);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x22, 0x32);
    success &= i2c_write_reg8(dev->addr7, 0x47, 0x14);
    success &= i2c_write_reg8(dev->addr7, 0x49, 0xFF);
    success &= i2c_write_reg8(dev->addr7, 0x4A, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x7A, 0x0A);
    success &= i2c_write_reg8(dev->addr7, 0x7B, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x78, 0x21);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x23, 0x34);
    success &= i2c_write_reg8(dev->addr7, 0x42, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x44, 0xFF);
    success &= i2c_write_reg8(dev->addr7, 0x45, 0x26);
    success &= i2c_write_reg8(dev->addr7, 0x46, 0x05);
    success &= i2c_write_reg8(dev->addr7, 0x40, 0x40);
    success &= i2c_write_reg8(dev->addr7, 0x0E, 0x06);
    success &= i2c_write_reg8(dev->addr7, 0x20, 0x1A);
    success &= i2c_write_reg8(dev->addr7, 0x43, 0x40);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x34, 0x03);
    success &= i2c_write_reg8(dev->addr7, 0x35, 0x44);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x31, 0x04);
    success &= i2c_write_reg8(dev->addr7, 0x4B, 0x09);
    success &= i2c_write_reg8(dev->addr7, 0x4C, 0x05);
    success &= i2c_write_reg8(dev->addr7, 0x4D, 0x04);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x44, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x45, 0x20);
    success &= i2c_write_reg8(dev->addr7, 0x47, 0x08);
    success &= i2c_write_reg8(dev->addr7, 0x48, 0x28);
    success &= i2c_write_reg8(dev->addr7, 0x67, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x70, 0x04);
    success &= i2c_write_reg8(dev->addr7, 0x71, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x72, 0xFE);
    success &= i2c_write_reg8(dev->addr7, 0x76, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x77, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x0D, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x80, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x01, 0xF8);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x8E, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x00, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x80, 0x00);

    return success;
}

static bool vl53l0x_configure_interrupt(VL53L0X_Dev *dev)
{
    uint8_t gpio_hv_mux_active_high = 0;

    if (!i2c_write_reg8(dev->addr7, REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04))
    {
        return false;
    }

    if (!i2c_read_reg8(dev->addr7, REG_GPIO_HV_MUX_ACTIVE_HIGH, &gpio_hv_mux_active_high))
    {
        return false;
    }

    gpio_hv_mux_active_high &= (uint8_t)~0x10;

    if (!i2c_write_reg8(dev->addr7, REG_GPIO_HV_MUX_ACTIVE_HIGH, gpio_hv_mux_active_high))
    {
        return false;
    }

    if (!i2c_write_reg8(dev->addr7, REG_SYSTEM_INTERRUPT_CLEAR, 0x01))
    {
        return false;
    }

    return true;
}

static bool vl53l0x_set_sequence_steps_enabled(VL53L0X_Dev *dev, uint8_t sequence_step)
{
    return i2c_write_reg8(dev->addr7, REG_SYSTEM_SEQUENCE_CONFIG, sequence_step);
}

static bool vl53l0x_static_init(VL53L0X_Dev *dev)
{
    if (!vl53l0x_load_default_tuning_settings(dev))
    {
        return false;
    }

    if (!vl53l0x_configure_interrupt(dev))
    {
        return false;
    }

    if (!vl53l0x_set_sequence_steps_enabled(dev,
        RANGE_SEQUENCE_STEP_DSS + RANGE_SEQUENCE_STEP_PRE_RANGE + RANGE_SEQUENCE_STEP_FINAL_RANGE))
    {
        return false;
    }

    return true;
}

static bool vl53l0x_perform_single_ref_calibration(VL53L0X_Dev *dev, calibration_type_t calib_type)
{
    uint8_t sysrange_start = 0;
    uint8_t sequence_config = 0;
    uint8_t interrupt_status = 0;
    bool success = false;

    switch (calib_type)
    {
        case CALIBRATION_TYPE_VHV:
            sequence_config = 0x01;
            sysrange_start = 0x01 | 0x40;
            break;

        case CALIBRATION_TYPE_PHASE:
            sequence_config = 0x02;
            sysrange_start = 0x01 | 0x00;
            break;
    }

    if (!i2c_write_reg8(dev->addr7, REG_SYSTEM_SEQUENCE_CONFIG, sequence_config))
    {
        return false;
    }

    if (!i2c_write_reg8(dev->addr7, REG_SYSRANGE_START, sysrange_start))
    {
        return false;
    }

    do
    {
        success = i2c_read_reg8(dev->addr7, REG_RESULT_INTERRUPT_STATUS, &interrupt_status);
    }
    while (success && ((interrupt_status & 0x07) == 0));

    if (!success)
    {
        return false;
    }

    if (!i2c_write_reg8(dev->addr7, REG_SYSTEM_INTERRUPT_CLEAR, 0x01))
    {
        return false;
    }

    if (!i2c_write_reg8(dev->addr7, REG_SYSRANGE_START, 0x00))
    {
        return false;
    }

    return true;
}

static bool vl53l0x_perform_ref_calibration(VL53L0X_Dev *dev)
{
    if (!vl53l0x_perform_single_ref_calibration(dev, CALIBRATION_TYPE_VHV))
    {
        return false;
    }

    if (!vl53l0x_perform_single_ref_calibration(dev, CALIBRATION_TYPE_PHASE))
    {
        return false;
    }

    if (!vl53l0x_set_sequence_steps_enabled(dev,
        RANGE_SEQUENCE_STEP_DSS + RANGE_SEQUENCE_STEP_PRE_RANGE + RANGE_SEQUENCE_STEP_FINAL_RANGE))
    {
        return false;
    }

    return true;
}

static bool vl53l0x_init_device(VL53L0X_Dev *dev)
{
    if (!vl53l0x_device_is_booted(dev))
    {
        return false;
    }

    if (!vl53l0x_data_init(dev))
    {
        return false;
    }

    if (!vl53l0x_static_init(dev))
    {
        return false;
    }

    if (!vl53l0x_perform_ref_calibration(dev))
    {
        return false;
    }

    return true;
}

static bool vl53l0x_read_range_single_device(VL53L0X_Dev *dev, uint16_t *range)
{
    uint8_t sysrange_start = 0;
    uint8_t interrupt_status = 0;
    bool success;

    success = i2c_write_reg8(dev->addr7, 0x80, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0x00, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x91, dev->stop_variable);
    success &= i2c_write_reg8(dev->addr7, 0x00, 0x01);
    success &= i2c_write_reg8(dev->addr7, 0xFF, 0x00);
    success &= i2c_write_reg8(dev->addr7, 0x80, 0x00);

    if (!success)
    {
        return false;
    }

    if (!i2c_write_reg8(dev->addr7, REG_SYSRANGE_START, 0x01))
    {
        return false;
    }

    do
    {
        success = i2c_read_reg8(dev->addr7, REG_SYSRANGE_START, &sysrange_start);
    }
    while (success && (sysrange_start & 0x01));

    if (!success)
    {
        return false;
    }

    do
    {
        success = i2c_read_reg8(dev->addr7, REG_RESULT_INTERRUPT_STATUS, &interrupt_status);
    }
    while (success && ((interrupt_status & 0x07) == 0));

    if (!success)
    {
        return false;
    }

    if (!i2c_read_reg16(dev->addr7, (uint8_t)(REG_RESULT_RANGE_STATUS + 10), range))
    {
        return false;
    }

    if (!i2c_write_reg8(dev->addr7, REG_SYSTEM_INTERRUPT_CLEAR, 0x01))
    {
        return false;
    }

    if ((*range == 8190u) || (*range == 8191u))
    {
        *range = VL53L0X_OUT_OF_RANGE;
    }

    return true;
}

static void print_ref_registers(VL53L0X_Dev *dev, const char *name)
{
    uint8_t val8 = 0;
    uint16_t val16 = 0;

    printf("%s @ 0x%02X\r\n", name, dev->addr7);

    i2c_read_reg8(dev->addr7, 0xC0, &val8);
    printf("  Reg(0xC0): 0x%02x\r\n", val8);

    i2c_read_reg8(dev->addr7, 0xC1, &val8);
    printf("  Reg(0xC1): 0x%02x\r\n", val8);

    i2c_read_reg8(dev->addr7, 0xC2, &val8);
    printf("  Reg(0xC2): 0x%02x\r\n", val8);

    i2c_read_reg16(dev->addr7, 0x51, &val16);
    printf("  Reg(0x51): 0x%04x\r\n", val16);

    i2c_read_reg16(dev->addr7, 0x61, &val16);
    printf("  Reg(0x61): 0x%04x\r\n\r\n", val16);
}

int main(void)
{
    VL53L0X_Dev tof1;
    VL53L0X_Dev tof2;
    bool tof1_ok = false;
    bool tof2_ok = false;
    bool read1_ok = false;
    bool read2_ok = false;
    uint16_t range1 = 0;
    uint16_t range2 = 0;
    uint8_t last_ir_command = IR_CMD_STOP;

    initUART(115200);
    waitms(500);

    printf("\x1b[2J\x1b[1;1H");
    printf("STM32L051 dual VL53L0X + IR robot\r\n");
    printf("TOF1 XSHUT=PB0, TOF2 XSHUT=PB1\r\n");
    printf("TOF I2C: PB6=SCL, PB7=SDA\r\n");
    printf("LED1=PA0 (<100 mm), LED2=PA1 (>100 mm)\r\n");
    printf("Motors: PA11/PA12 left, PA13/PA14 right, IR=PA15\r\n");
    printf("IR commands: STOP=0x0 LEFT=0x1 RIGHT=0x8 FORWARD=0x9\r\n");
    printf("File: %s\r\n", __FILE__);
    printf("Compiled: %s, %s\r\n\r\n", __DATE__, __TIME__);
    fflush(stdout);

    tof1.addr7 = TOF1_ADDR_DEFAULT;
    tof1.stop_variable = 0;
    tof2.addr7 = TOF1_ADDR_DEFAULT;
    tof2.stop_variable = 0;

    leds_init();
    motor_ir_init();
    xshut_init();
    I2C_init();

    xshut1_low();
    xshut2_low();
    waitms(50);

    xshut1_release();
    waitms(50);

    if (vl53l0x_device_is_booted(&tof1))
    {
        if (vl53l0x_set_address(&tof1, TOF1_ADDR_NEW))
        {
            tof1_ok = vl53l0x_init_device(&tof1);
        }
    }

    if (tof1_ok)
    {
        printf("TOF1 init OK at 0x%02X\r\n", tof1.addr7);
        print_ref_registers(&tof1, "TOF1");
    }
    else
    {
        printf("TOF1 init FAILED\r\n");
    }
    fflush(stdout);

    xshut2_release();
    waitms(50);

    if (vl53l0x_device_is_booted(&tof2))
    {
        if (vl53l0x_set_address(&tof2, TOF2_ADDR_NEW))
        {
            tof2_ok = vl53l0x_init_device(&tof2);
        }
    }

    if (tof2_ok)
    {
        printf("TOF2 init OK at 0x%02X\r\n", tof2.addr7);
        print_ref_registers(&tof2, "TOF2");
    }
    else
    {
        printf("TOF2 init FAILED\r\n");
    }
    fflush(stdout);

    while (1)
    {
        if (tof1_ok)
        {
            read1_ok = vl53l0x_read_range_single_device(&tof1, &range1);
        }
        else
        {
            read1_ok = false;
        }

        if (tof2_ok)
        {
            read2_ok = vl53l0x_read_range_single_device(&tof2, &range2);
        }
        else
        {
            read2_ok = false;
        }

        if (read1_ok && (range1 < TOF1_LED_THRESHOLD_MM))
            led1_on();
        else
            led1_off();

        if (read2_ok && (range2 > TOF2_LED_THRESHOLD_MM))
            led2_on();
        else
            led2_off();

        if (ir_new_data_flag != 0u)
        {
            last_ir_command = ir_command;
            ir_new_data_flag = 0u;

            switch (last_ir_command)
            {
                case IR_CMD_FORWARD:
                    motor_forward();
                    break;

                case IR_CMD_LEFT:
                    motor_left();
                    break;

                case IR_CMD_RIGHT:
                    motor_right();
                    break;

                case IR_CMD_STOP:
                default:
                    motor_stop();
                    break;
            }
        }

        if (read1_ok && read2_ok)
        {
            printf("D1:%4u mm  D2:%4u mm  IR:0x%01X\r", range1, range2, last_ir_command);
        }
        else if (read1_ok)
        {
            printf("D1:%4u mm  D2:ERR   IR:0x%01X\r", range1, last_ir_command);
        }
        else if (read2_ok)
        {
            printf("D1:ERR   D2:%4u mm  IR:0x%01X\r", range2, last_ir_command);
        }
        else
        {
            printf("D1:ERR   D2:ERR   IR:0x%01X\r", last_ir_command);
        }

        fflush(stdout);
        waitms(100);
    }
}