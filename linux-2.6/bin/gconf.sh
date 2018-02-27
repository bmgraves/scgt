#!/bin/sh
#GT gconf.sh    Rev. Mon May  2 16:27:12 EDT 2005
#This script is called from gdriver.sh or it can be called directly.
#This script serves as an example of setting GT hardware to some
#well defined configuration including automatic node Id allocation.
#If you customize this script make sure that you keep a copy
#as GT software update may overwrite it.

if [ "$1" = "" ];then units=0
elif [ "$1" = "-a" ]; then units=`./gtmon -A | cut -f2 -d " "`
else units=$1
fi

node=`exec uname -n`
if   [ "$node" = "bg1" ]; then nId=1
elif [ "$node" = "bg2" ]; then nId=2
elif [ "$node" = "bg3" ]; then nId=3
elif [ "$node" = "bg4" ]; then nId=4
elif [ "$node" = "bg5" ]; then nId=5
elif [ "$node" = "bg6" ]; then nId=6
elif [ "$node" = "bg7" ]; then nId=7
elif [ "$node" = "bg8" ]; then nId=8
elif [ "$node" = "bg9" ]; then nId=9
else

# Picking nId from ip address handles multiple iterfaces appropriately
# Either of the next two lines works correctly on Linux, Solaris, and Irix.
# The first requires fewer steps, so we'll use it for now.

if [ `uname` = "HP-UX" ];
then
    IFCONFIG="ifconfig lan0"
else
    IFCONFIG="ifconfig -a"
fi

#grab last part of IP address (e.g., the nnn in 192.0.0.nnn)
nId=`echo \`$IFCONFIG | grep inet | grep -v 127.0.0.1\` | cut -d "." -f 4 | cut -d " " -f 1`

#older and less portable nId methods
#network IP(s) from which to pick node ID
#NETWORKS="19[28]"
#nId=`echo \`ifconfig -a|grep $NETWORKS|sed "s/[^0-9]/ /g"\` | cut -d " " -f 4`
#nId=`ifconfig -a|grep "19[28]"|sed "s/[^0-9]/ /g"|tr "[:space:]" " "|tr -s " "|cut -d " " -f 5`
fi

for unit in $units;
do
    #Making up node Id for multiple interface case
    hId=`expr 10 \* $unit + $nId`
    #Options to set and setting them
    options="--wlastoff --txon --rxon --pxon --ewrapoff --laser0on --laser1on -b 0xffffffff --uinton --sinton"
    ./gtmon -u $unit -n $hId $options
    #Selecting active interface (interface 0 for old gt100 and 1 for gt200 is preffered)
    firmware=`./gtmon -u $unit -v|grep Firmware|cut -d 'F' -f2|cut -d ' ' -f2|cut -d '.' -f1`
    Iuse=0;
    if [ $firmware -lt 2 ]; then
        Iuse=0;
        if   [ "`./gtmon -u $unit --signal0`" = "1" ]; then Iuse=0;
        elif [ "`./gtmon -u $unit --signal1`" = "1" ]; then Iuse=1;
        fi
    else
        Iuse=1;
        if   [ "`./gtmon -u $unit --signal1`" = "1" ]; then Iuse=1;
        elif [ "`./gtmon -u $unit --signal0`" = "1" ]; then Iuse=0;
        fi
    fi

    ./gtmon -u $unit -i $Iuse

    ./gtmon -u $unit -V |grep "compat"
    ./gtmon -u $unit -V |grep "Driver"
    state=`./gtmon -v -u $unit $options --bint --d64 --nodeid --interface`
    echo $state
    ./gtmon -u $unit |grep -v "gtmon"

    #./gtmem -R -u $unit -o 0x14 -w 0x0 -m 0xf0000 #this is for testing only
done;

