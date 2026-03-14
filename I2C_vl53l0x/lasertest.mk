SHELL=cmd
PORTN=$(shell type COMPORT.inc)
CC=avr-gcc
CPU=-mmcu=atmega328p
COPT=-g -Os -Wall $(CPU)
OBJS=main.o usart.o vl53l0x.o

main.elf: $(OBJS)
	avr-gcc $(CPU) -Wl,-Map,main.map $(OBJS) -o main.elf
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	@echo done!

main.o: main.c usart.h vl53l0x.h
	avr-gcc $(COPT) -c main.c

usart.o: usart.c usart.h
	avr-gcc $(COPT) -c usart.c

vl53l0x.o: vl53l0x.c vl53l0x.h
	avr-gcc $(COPT) -c vl53l0x.c

clean:
	@del *.hex *.elf *.map *.o 2>nul

LoadFlash:
	@taskkill /f /im putty.exe /t /fi "status eq running" > NUL
	spi_atmega -p -v -crystal main.hex
	@cmd /c start putty.exe -serial $(PORTN) -sercfg 115200,8,n,1,N

putty:
	@taskkill /f /im putty.exe /t /fi "status eq running" > NUL
	@cmd /c start putty.exe -serial $(PORTN) -sercfg 115200,8,n,1,N

dummy: main.hex main.map Atmega328_vl53l0x.jpg start_putty.bat
	@echo Hello dummy!

Picture:
	@cmd /c start Atmega328_vl53l0x.jpg

explorer:
	cmd /c start explorer .