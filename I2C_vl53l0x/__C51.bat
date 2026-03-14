@echo off
::This file was created automatically by CrossIDE to compile with C51.
C:
cd "\Users\jonat\Downloads\I2C_vl53l0x_ATMega328_Example\I2C_vl53l0x\"
"C:\CrossIDE\Call51\Bin\c51.exe" --use-stdout  "C:\Users\jonat\Downloads\I2C_vl53l0x_ATMega328_Example\I2C_vl53l0x\main.c"
if not exist hex2mif.exe goto done
if exist main.ihx hex2mif main.ihx
if exist main.hex hex2mif main.hex
:done
echo done
echo Crosside_Action Set_Hex_File C:\Users\jonat\Downloads\I2C_vl53l0x_ATMega328_Example\I2C_vl53l0x\main.hex
