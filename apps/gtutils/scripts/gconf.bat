:: GT gconf.bat     Rev. Tue May  3 07:21:32 EDT 2005

@SETLOCAL
@echo off

:: Set default unit
SET /A unit = 0
IF NOT %1x==x SET /A unit=%1

:: Set nodeid, if specified
IF NOT %2x==x .\gtmon -u %unit% -n %2

:: Set other config settings
.\gtmon -u %unit% -i 1 --wlastoff --txon --rxon --pxon --ewrapoff --laseron -b 0xffffffff --uinton --sinton 

:: print the settings
.\gtmon -u %unit% -v --interface --wlast --tx --rx --px --ewrap --laser --bint --uint --sint --nodeid

:: print some info
.\gtmon -u %unit% -V | FIND "Driver"
.\gtmon -u %unit% | FIND /V "gtmon"

@echo on
@ENDLOCAL
