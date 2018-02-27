#!/bin/sh
#GT gclock.sh   Rev. Tue Jul 13 15:01:58 EDT 2004 - Linux only
#Function: checks clock accuracy (ppm) on all GT nodes on a ring

restoreid()
{
    ./gtmon -n $MYID 
    exit
}


MYID=`exec ./gtmon --nodeid`
echo GT clock reference node id = $MYID
trap restoreid 2     # trap Control-C (2) to ensure setConfig gets called

ALLID=`./gtmon -N|grep -|cut -b 6-8`
echo Found GTs with id $ALLID
for ID in $ALLID; do
    ./gtmon -n $ID
    PPM=`exec ./gtmon -v -p 1000 -s 5|grep Lat|grep ppm|cut -b 56-`
    echo node $ID --- ppm $PPM
done

restoreid

