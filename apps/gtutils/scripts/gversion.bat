:: GT gversion.bat  Rev. Tue Mar  3 11:24:51 EST 2004
@SETLOCAL
@echo off

.\gtmon |find "rev"
.\gtmem |find "rev"
.\gttp |find "rev"
.\gtprog |find "rev"
.\gtint |find "rev"
.\gtnex |find "rev"
.\gtlat |find "rev"
.\gtbert |find "rev"
:: extract revision line from all .bat files, then filter the line doing the
:: search along with the filename line and blank lines output by find when
:: parsing multiple files
find "Rev." g*.bat | find /V "----"  | find "Rev."

@echo on
@ENDLOCAL
