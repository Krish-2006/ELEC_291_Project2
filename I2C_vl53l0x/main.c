#include <avr/io.h>
#include <stdio.h>
#include "usart.h"
#include <util/delay.h>
#include "vl53l0x.h"

#define BDIV (F_CPU / 400000 - 16) / 2 + 1    // Puts I2C rate just below 400kHz

#define LED_DDR   DDRD
#define LED_PORT  PORTD
#define LED_PIN   PD2

/* Pinout for DIP28 ATMega328P:

                           -------
     (PCINT14/RESET) PC6 -|1    28|- PC5 (ADC5/SCL/PCINT13)
       (PCINT16/RXD) PD0 -|2    27|- PC4 (ADC4/SDA/PCINT12)
       (PCINT17/TXD) PD1 -|3    26|- PC3 (ADC3/PCINT11)
      (PCINT18/INT0) PD2 -|4    25|- PC2 (ADC2/PCINT10)
 (PCINT19/OC2B/INT1) PD3 -|5    24|- PC1 (ADC1/PCINT9)
    (PCINT20/XCK/T0) PD4 -|6    23|- PC0 (ADC0/PCINT8)
                     VCC -|7    22|- GND
                     GND -|8    21|- AREF
(PCINT6/XTAL1/TOSC1) PB6 -|9    20|- AVCC
(PCINT7/XTAL2/TOSC2) PB7 -|10   19|- PB5 (SCK/PCINT5)
   (PCINT21/OC0B/T1) PD5 -|11   18|- PB4 (MISO/PCINT4)
 (PCINT22/OC0A/AIN0) PD6 -|12   17|- PB3 (MOSI/OC2A/PCINT3)
      (PCINT23/AIN1) PD7 -|13   16|- PB2 (SS/OC1B/PCINT2)
  (PCINT0/CLKO/ICP1) PB0 -|14   15|- PB1 (OC1A/PCINT1)
                           -------
*/

void I2C_init(char bitrate)
{
	TWSR = 0;
	TWBR = bitrate;
}

void I2C_start(void)
{
	TWCR = (1 << TWEN) | (1 << TWSTA) | (1 << TWINT);
	while (!(TWCR & (1 << TWINT)));
}

void I2C_stop(void)
{
	TWCR = (1 << TWEN) | (1 << TWSTO) | (1 << TWINT);
	while(TWCR & (1 << TWSTO));
}

void I2C_write(unsigned char byte)
{
	TWDR = byte;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

unsigned char I2C_read(unsigned char ack)
{
	TWCR = (1 << TWINT) | (1 << TWEN) | ((ack!=0)?(1 << TWEA):0x00);
	while (!(TWCR & (1 << TWINT)));
    return TWDR;
}

unsigned char i2c_read_addr8_data8(unsigned char address, unsigned char * value)
{
	I2C_start();
	I2C_write(0x52); // Write address
	I2C_write(address);
	I2C_stop();

	I2C_start();
	I2C_write(0x53); // Read address
	*value = I2C_read(0);
	I2C_stop();
	
	return 1;
}

unsigned char i2c_read_addr8_data16(unsigned char address, unsigned int * value)
{
	I2C_start();
	I2C_write(0x52); // Write address
	I2C_write(address);
	I2C_stop();

	I2C_start();
	I2C_write(0x53); // Read address
	*value = I2C_read(1) * 256;
	*value += I2C_read(0);
	I2C_stop();
	
	return 1;
}

unsigned char i2c_write_addr8_data8(unsigned char address, unsigned char value)
{
	I2C_start();
	I2C_write(0x52); // Write address
	I2C_write(address);
	I2C_write(value);
	I2C_stop();

	return 1;
}

// From the VL53L0X datasheet:
//
// The registers shown in the table below can be used to validate the user I2C interface.
// Address (After fresh reset, without API loaded)
//    0xC0 0xEE
//    0xC1 0xAA
//    0xC2 0x10
//    0x51 0x0099
//    0x61 0x0000
//
// Not needed, but it was useful to debug the I2C interface, so left here.
void validate_I2C_interface(void)
{
	unsigned char val8 = 0;
	unsigned int val16 = 0;
	
    printf("\n");   
    
    i2c_read_addr8_data8(0xc0, &val8);
    printf("Reg(0xc0): 0x%02x\n", val8);

    i2c_read_addr8_data8(0xc1, &val8);
    printf("Reg(0xc1): 0x%02x\n", val8);

    i2c_read_addr8_data8(0xc2, &val8);
    printf("Reg(0xc2): 0x%02x\n", val8);
    
    i2c_read_addr8_data16(0x51, &val16);
    printf("Reg(0x51): 0x%04x\n", val16);

    i2c_read_addr8_data16(0x61, &val16);
    printf("Reg(0x61): 0x%04x\n", val16);
    
    printf("\n");
}

int main(void)
{
	unsigned char success;
	unsigned int range = 0;
	
	usart_init();
	I2C_init(BDIV);

	LED_DDR |= (1 << LED_PIN);
	LED_PORT &= ~(1 << LED_PIN);

	_delay_ms(500);
	
	printf("\x1b[2J\x1b[1;1H");
	printf("ATMega328P vl53l0x test program\r\n"
	       "File: %s\r\n"
	       "Compiled: %s, %s\r\n\r\n",
	       __FILE__, __DATE__, __TIME__);
    	
    validate_I2C_interface();

	success = vl53l0x_init();
	if(success)
	{
		printf("VL53L0x initialization succeeded.\n");
	}
	else
	{
		printf("VL53L0x initialization failed.\n");
	}
	
    while(1)
    {
        success = vl53l0x_read_range_single(&range);
		if(success)
		{
	        printf("D: %4u (mm)\r", range);

			if(range < 100)
			{
				LED_PORT |= (1 << LED_PIN);
			}
			else
			{
				LED_PORT &= ~(1 << LED_PIN);
			}
		}
		else
		{
			LED_PORT &= ~(1 << LED_PIN);
		}

	    _delay_ms(100);
	}
}