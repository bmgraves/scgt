#!/bin/sh
#GT gmanual.sh  Rev. Mon Dec  8 16:26:58 EST 2003



if [ "$1" = "" ]; then
    option=""
    echo "type $0 -h for more help"
elif [ "$1" = "-h" ]; then
    if [ "$2" = "1" ]; then
        option="-h 1"
    else
        echo "type $0 -h 1 for full help"
        option="-h"
    fi
fi

echo "#########################################"
./gtmon $option
echo "#########################################"
./gtmem $option
echo "#########################################"
./gttp $option
echo "#########################################"
./gtint $option
echo "#########################################"
./gtprog $option
echo "#########################################"
./gtnex $option
echo "#########################################"
./gtlat $option
echo "#########################################"
./gtbert $option
