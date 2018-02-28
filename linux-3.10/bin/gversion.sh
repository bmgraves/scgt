#!/bin/sh
#GT gversion.sh Rev. Thu Jan 22 15:30:48 EST 2004

./gtmon |grep rev
./gtmem |grep rev
./gttp |grep rev
./gtprog |grep rev
./gtint |grep rev
./gtnex |grep rev
./gtlat |grep rev
./gtbert |grep rev
cat g*.sh |grep Rev.|grep -v cat
