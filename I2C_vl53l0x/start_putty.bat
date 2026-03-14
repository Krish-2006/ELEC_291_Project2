@echo off
setlocal enabledelayedexpansion
if exist COMPORT.inc (
	:: If we know from the flash loader the COM port number use it
	set line=1
	for /f "delims=" %%L in (COMPORT.inc) do (
	    if "!line!"=="1" set comport=%%L
	    set /a line+=1
	)
) else (
	:: Try with the first COM port available
	set line=1
	for /f "tokens=4" %%L in ('mode^|findstr "COM[0-9]*"') do (
		if "!line!"=="1" set stringout=%%L
	    set /a line+=1
	)
	for /F %%G IN ("!stringout!") do set "comport=%%G"
	:: Get rid of the ':' in comport
	set comport=!comport::=!
)
start putty.exe -serial !comport! -sercfg 115200,8,n,1,N
