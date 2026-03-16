#include <avr/io.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "usart.h"
#include <util/delay.h>

#define BDIV ((F_CPU / 400000UL - 16) / 2 + 1)

#define LED1_DDR   DDRD
#define LED1_PORT  PORTD
#define LED1_PIN   PD2

#define LED2_DDR   DDRD
#define LED2_PORT  PORTD
#define LED2_PIN   PD7

#define XSHUT1_DDR   DDRB
#define XSHUT1_PORT  PORTB
#define XSHUT1_PIN   PB0

#define XSHUT2_DDR   DDRB
#define XSHUT2_PORT  PORTB
#define XSHUT2_PIN   PB1

#define TOF1_ADDR_DEFAULT  0x29
#define TOF1_ADDR_NEW      0x2A
#define TOF2_ADDR_NEW      0x2B

#define TOF1_LED_THRESHOLD_MM  100
#define TOF2_LED_THRESHOLD_MM  100

#define VL53L0X_OUT_OF_RANGE 8190

#define REG_SYSRANGE_START                             0x00
#define REG_SYSTEM_SEQUENCE_CONFIG                     0x01
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO               0x0A
#define REG_SYSTEM_INTERRUPT_CLEAR                     0x0B
#define REG_RESULT_INTERRUPT_STATUS                    0x13
#define REG_RESULT_RANGE_STATUS                        0x14
#define REG_GPIO_HV_MUX_ACTIVE_HIGH                    0x84
#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV          0x89
#define REG_I2C_SLAVE_DEVICE_ADDRESS                   0x8A
#define REG_IDENTIFICATION_MODEL_ID                    0xC0

#define VL53L0X_EXPECTED_DEVICE_ID                     0xEE

#define RANGE_SEQUENCE_STEP_DSS                        0x28
#define RANGE_SEQUENCE_STEP_PRE_RANGE                  0x40
#define RANGE_SEQUENCE_STEP_FINAL_RANGE                0x80

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

static void I2C_init(uint8_t bitrate)
{
    TWSR = 0;
    TWBR = bitrate;
}

static void I2C_start(void)
{
    TWCR = (1 << TWEN) | (1 << TWSTA) | (1 << TWINT);
    while (!(TWCR & (1 << TWINT)));
}

static void I2C_stop(void)
{
    TWCR = (1 << TWEN) | (1 << TWSTO) | (1 << TWINT);
    while (TWCR & (1 << TWSTO));
}

static void I2C_write(uint8_t byte)
{
    TWDR = byte;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

static uint8_t I2C_read(uint8_t ack)
{
    TWCR = (1 << TWINT) | (1 << TWEN) | ((ack != 0) ? (1 << TWEA) : 0x00);
    while (!(TWCR & (1 << TWINT)));
    return TWDR;
}

static uint8_t i2c_write_reg8(uint8_t addr7, uint8_t reg, uint8_t value)
{
    I2C_start();
    I2C_write((uint8_t)(addr7 << 1));
    I2C_write(reg);
    I2C_write(value);
    I2C_stop();
    return 1;
}

static uint8_t i2c_read_reg8(uint8_t addr7, uint8_t reg, uint8_t *value)
{
    I2C_start();
    I2C_write((uint8_t)(addr7 << 1));
    I2C_write(reg);
    I2C_stop();

    I2C_start();
    I2C_write((uint8_t)((addr7 << 1) | 1));
    *value = I2C_read(0);
    I2C_stop();
    return 1;
}

static uint8_t i2c_read_reg16(uint8_t addr7, uint8_t reg, uint16_t *value)
{
    I2C_start();
    I2C_write((uint8_t)(addr7 << 1));
    I2C_write(reg);
    I2C_stop();

    I2C_start();
    I2C_write((uint8_t)((addr7 << 1) | 1));
    *value = ((uint16_t)I2C_read(1)) << 8;
    *value |= I2C_read(0);
    I2C_stop();
    return 1;
}

static void xshut1_low(void)
{
    XSHUT1_PORT &= ~(1 << XSHUT1_PIN);
    XSHUT1_DDR |= (1 << XSHUT1_PIN);
}

static void xshut2_low(void)
{
    XSHUT2_PORT &= ~(1 << XSHUT2_PIN);
    XSHUT2_DDR |= (1 << XSHUT2_PIN);
}

static void xshut1_release(void)
{
    XSHUT1_PORT &= ~(1 << XSHUT1_PIN);
    XSHUT1_DDR &= ~(1 << XSHUT1_PIN);
}

static void xshut2_release(void)
{
    XSHUT2_PORT &= ~(1 << XSHUT2_PIN);
    XSHUT2_DDR &= ~(1 << XSHUT2_PIN);
}

static bool vl53l0x_device_is_booted(VL53L0X_Dev *dev)
{
    uint8_t device_id = 0;

    if (!i2c_read_reg8(dev->addr7, REG_IDENTIFICATION_MODEL_ID, &device_id))
    {
        return false;
    }

    return device_id == VL53L0X_EXPECTED_DEVICE_ID;
}

static bool vl53l0x_set_address(VL53L0X_Dev *dev, uint8_t new_addr7)
{
    if (!i2c_write_reg8(dev->addr7, REG_I2C_SLAVE_DEVICE_ADDRESS, (uint8_t)(new_addr7 & 0x7F)))
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

    if ((*range == 8190) || (*range == 8191))
    {
        *range = VL53L0X_OUT_OF_RANGE;
    }

    return true;
}

static void print_ref_registers(VL53L0X_Dev *dev, const char *name)
{
    uint8_t val8 = 0;
    uint16_t val16 = 0;

    printf("%s @ 0x%02X\n", name, dev->addr7);

    i2c_read_reg8(dev->addr7, 0xC0, &val8);
    printf("  Reg(0xC0): 0x%02x\n", val8);

    i2c_read_reg8(dev->addr7, 0xC1, &val8);
    printf("  Reg(0xC1): 0x%02x\n", val8);

    i2c_read_reg8(dev->addr7, 0xC2, &val8);
    printf("  Reg(0xC2): 0x%02x\n", val8);

    i2c_read_reg16(dev->addr7, 0x51, &val16);
    printf("  Reg(0x51): 0x%04x\n", val16);

    i2c_read_reg16(dev->addr7, 0x61, &val16);
    printf("  Reg(0x61): 0x%04x\n\n", val16);
}

int main(void)
{
    VL53L0X_Dev tof1;
    VL53L0X_Dev tof2;
    bool tof1_ok;
    bool tof2_ok;
    bool read1_ok;
    bool read2_ok;
    uint16_t range1;
    uint16_t range2;

    tof1.addr7 = TOF1_ADDR_DEFAULT;
    tof1.stop_variable = 0;
    tof2.addr7 = TOF1_ADDR_DEFAULT;
    tof2.stop_variable = 0;
    tof1_ok = false;
    tof2_ok = false;
    read1_ok = false;
    read2_ok = false;
    range1 = 0;
    range2 = 0;

    usart_init();
    I2C_init((uint8_t)BDIV);

    LED1_DDR |= (1 << LED1_PIN);
    LED1_PORT &= ~(1 << LED1_PIN);
    LED2_DDR |= (1 << LED2_PIN);
    LED2_PORT &= ~(1 << LED2_PIN);

    xshut1_low();
    xshut2_low();

    _delay_ms(500);

    printf("\x1b[2J\x1b[1;1H");
    printf("ATMega328P dual VL53L0X test\r\n");
    printf("TOF1 XSHUT=PB0, TOF2 XSHUT=PB1\r\n");
    printf("LED1=PD2 (<100 mm), LED2=PD7 (>10 mm)\r\n");
    printf("File: %s\r\n", __FILE__);
    printf("Compiled: %s, %s\r\n\r\n", __DATE__, __TIME__);

    _delay_ms(10);

    xshut1_release();
    _delay_ms(10);

    if (vl53l0x_device_is_booted(&tof1))
    {
        if (vl53l0x_set_address(&tof1, TOF1_ADDR_NEW))
        {
            tof1_ok = vl53l0x_init_device(&tof1);
        }
    }

    if (tof1_ok)
    {
        printf("TOF1 init OK at 0x%02X\n", tof1.addr7);
        print_ref_registers(&tof1, "TOF1");
    }
    else
    {
        printf("TOF1 init FAILED\n");
    }

    xshut2_release();
    _delay_ms(10);

    if (vl53l0x_device_is_booted(&tof2))
    {
        if (vl53l0x_set_address(&tof2, TOF2_ADDR_NEW))
        {
            tof2_ok = vl53l0x_init_device(&tof2);
        }
    }

    if (tof2_ok)
    {
        printf("TOF2 init OK at 0x%02X\n", tof2.addr7);
        print_ref_registers(&tof2, "TOF2");
    }
    else
    {
        printf("TOF2 init FAILED\n");
    }

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

        if (read1_ok)
        {
            if (range1 < TOF1_LED_THRESHOLD_MM)
            {
                LED1_PORT |= (1 << LED1_PIN);
            }
            else
            {
                LED1_PORT &= ~(1 << LED1_PIN);
            }
        }
        else
        {
            LED1_PORT &= ~(1 << LED1_PIN);
        }

        if (read2_ok)
        {
            if (range2 > TOF2_LED_THRESHOLD_MM)
            {
                LED2_PORT |= (1 << LED2_PIN);
            }
            else
            {
                LED2_PORT &= ~(1 << LED2_PIN);
            }
        }
        else
        {
            LED2_PORT &= ~(1 << LED2_PIN);
        }

        if (read1_ok && read2_ok)
        {
            printf("D1:%4u mm  D2:%4u mm\r", range1, range2);
        }
        else if (read1_ok)
        {
            printf("D1:%4u mm  D2:ERR \r", range1);
        }
        else if (read2_ok)
        {
            printf("D1:ERR   D2:%4u mm\r", range2);
        }
        else
        {
            printf("D1:ERR   D2:ERR \r");
        }

        _delay_ms(100);
    }
}