#!/bin/bash
#GT gspeed.sh   Rev. Tue Sep  7 16:59:31 EDT 2004 - test, private, Linux only, sl240 OK

#================================================
# usage - prints script usage                   =
#================================================
usage()
{
    echo
    echo "Usage:     $0  [-u unit] [-t time_interval] [-l] [-p] [-h] [-x]"
    echo "Defaults:  $0 -u 0 -t 2 -lp"
    echo
    if [ $# -gt 0 ]
    then
        echo "Options:"
        echo "   -u # - the unit to test"
        echo "   -t # - time interval for calculation (seconds)"
        echo "   -l   - calculate link speed"
        echo "   -p   - calculate pci bus speed"
        echo "   -q   - almost quiet"
        echo "   -x   - hardware is an Xtreme rather then GT board"
        echo
    fi
    return 0
}

#================================================
# unitExists - tests that unit is present       =
#================================================

unitExists()
{
    local unit=${1:-$UNIT}
    # check for non-hex digit
    if  echo $unit | grep -q "[^0-9a-fA-F]" || ! $MONAPP -u $unit > /dev/null
    then
        echo "Invalid unit number ($unit)";
        exit;
    fi;
}

#================================================
# diffCounter - Calculates difference between   =
#   current counter value and previous counter  =
#   value, accounting for (at most 1) roll-over =
# parameters: currCnt, lastCnt, maxCnt, retVar  =
# postcondition: variable name given by the 4th =
#   positional parameter, "retVar", holds the   =
#   return value                                =
#================================================
diffCounter()
{
    local currCnt=$1
    local lastCnt=$2
    local maxCnt=$3
    local retVar=$4

    if [ $currCnt -ge $lastCnt ]
    then
        (($retVar = currCnt - lastCnt))
    else # overflow
        (($retVar = currCnt + ((maxCnt - lastCnt)) + 1 ))
    fi
}

#================================================
# hexStringToInt - Converts a variable from hex =
#   storage to decimal storage.                 =
# Usage:                                        =
#   VALUE=0x10; hexStringToInt VALUE;           =
#   echo $VALUE   # prints "16"                 =
#================================================
hexStringToInt()
{
    (($1=${!1}));
}

#########################################################################
# SCRIPT EXECUTION STARTS HERE                                          #
#########################################################################

#default options
UNIT=0; SEC=2; LINK=0; PCI=0; QUIET=0; HW="GT"

#parse options
while getopts ":u:t:hlpqx" Option
do
  case $Option in
    u ) UNIT=$OPTARG;;
    t ) SEC=$OPTARG;;
    p ) PCI=1;;
    l ) LINK=1;;
    q ) QUIET=1;;
    x ) HW="SL240";;
    h ) usage 1; exit;;
    * ) echo "__Invalid or incorrectly used option \"-$OPTARG\""; exit;;
  esac
done

((x=$OPTIND-1))
if [ $# -ne $x ]; then echo "Invalid option string"; exit; fi;
# done parsing options


if [ "$HW" == "SL240" ]
then # sl240 version invoked
    MEMAPP=./xgtmem;
    MONAPP=./sl_mon
    RX_LINK_CNT_REG=0x74; LINK_CNT_REG=0x44; PCI_CNT_REG=0x1c
#    RX_LINK_CNT_REG=0x44; LINK_CNT_REG=0x44; PCI_CNT_REG=0x1c

else # assume gt version
    MEMAPP=./gtmem
    MONAPP=./gtmon
    RX_LINK_CNT_REG=0x40; LINK_CNT_REG=0x40; PCI_CNT_REG=0x44
fi

for name in $MEMAPP $MONAPP;
do
    if ! type $name &> /dev/null
    then
        echo "Can't find $name"
        echo "Make sure that you are in GT or SL240 bin directory as required."
        exit 1
    fi
done

if [ $PCI == 0 ] && [ $LINK == 0 ]; then  PCI=1;  LINK=1;  fi;

unitExists $UNIT  # verify that unit is valid

# read counter values
LINKCYCLES1=`$MEMAPP -u $UNIT -R -o $LINK_CNT_REG`
RXLINKCYCLES1=`$MEMAPP -u $UNIT -R -o $RX_LINK_CNT_REG`
PCICYCLES1=`$MEMAPP -u $UNIT -R -o $PCI_CNT_REG`

# delay
sleep $SEC

# read counter values again
LINKCYCLES2=`$MEMAPP -u $UNIT -R -o $LINK_CNT_REG`
RXLINKCYCLES2=`$MEMAPP -u $UNIT -R -o $RX_LINK_CNT_REG`
PCICYCLES2=`$MEMAPP -u $UNIT -R -o $PCI_CNT_REG`

#convert to decimal integers from hex strings
for name in LINKCYCLES1 LINKCYCLES2 RXLINKCYCLES1 RXLINKCYCLES2 PCICYCLES1 PCICYCLES2
do
    hexStringToInt $name
done

# calculate and display results

if [ $LINK == 1 ]
then
    diffCounter $LINKCYCLES2 $LINKCYCLES1 0xffffffff CYCLES
    ((CYCLES*=20))
    ((FREQ=($CYCLES/$SEC)/1000000))
    ((FRACT=(($CYCLES/$SEC)/100000)%10))

    echo -n "$FREQ.$FRACT "

    #an attempt to calculate ppm for sl240 hardware -it will have to change
    #as it is far from being of any accuracy

    diffCounter $LINKCYCLES2 $LINKCYCLES1 0xffffffff TXDIF
    diffCounter $RXLINKCYCLES2 $RXLINKCYCLES1 0xffffffff RXDIF
    ((TXRXDIF=$TXDIF - $RXDIF))

#echo $RXDIF $TXDIF $TXRXDIF
    ((PPM=($TXRXDIF*1000000)/($TXDIF+1)))

    if [ "$QUIET" == "0" ]
    then
        echo -n "Mbps Link   "
        echo -n "$PPM ppm   "        
    fi
fi

if [ $PCI == 1 ]
then
    diffCounter $PCICYCLES2 $PCICYCLES1 0xffffffff CYCLES
    ((FREQ=($CYCLES/$SEC)/1000000))
    ((FRACT=(($CYCLES/$SEC)/100000)%10))
    echo -n "$FREQ.$FRACT "

    if [ "$QUIET" == "0" ]
    then
        echo -n "MHz PCI"
    fi
fi

echo
