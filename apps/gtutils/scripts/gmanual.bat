:: GT gmanual.bat   Rev. Tue Mar  3 11:24:51 EST 2004

@SETLOCAL
@echo off

:: Set default option to nothing
SET option=

IF %1x==x ECHO "Type gmanual -h for more help"

IF NOT %1x==-hx GOTO NEXT

SET option=-h

:: param 1 is -h, is param 2 a 1?

IF NOT %2x==1x ECHO "Type gmanual -h 1 for full help"

IF %2x==1x SET option=-h 1

:NEXT

echo "#########################################"
.\gtmon %option%
echo "#########################################"
.\gtmem %option%
echo "#########################################"
.\gttp %option%
echo "#########################################"
.\gtint %option%
echo "#########################################"
.\gtprog %option%
echo "#########################################"
.\gtnex %option%
echo "#########################################"
.\gtlat %option%
echo "#########################################"
.\gtbert %option%

@echo on
@ENDLOCAL
