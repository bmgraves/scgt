#!/bin/bash

#GT gbytes.sh   Rev. Thu Jan 22 15:14:01 EST 2004 - test, private, Linux only
#Tests byte accounting in the GT FW

#================================================
# usage - prints script usage                   =
#================================================
usage()
{
    echo
    echo "Usage:     $0 [unit | -h] [seconds] [write_size] [simult_read_flag]"
    echo "Defaults:  $0 0 10 4 1"
    echo
    if [ $# -gt 0 ]
    then
        echo "NOTE: The unit under test must be on a closed loop";
        echo "      with no network write activity.";
        echo
    fi
    return 0
}

#================================================
# ctrl_C_trap - kills background tasks          =
#================================================
ctrl_C_trap()
{
    echo "Iterations $passCnt - Seconds $SECONDS - Failures $failed"
    echo "Byte accounting test completed."

    ps -p $pid > /dev/null
    if [ $? == 0 ];
    then
        echo "Killed gttp background task.";
        kill -9 $pid;
    fi;

    trap - 2    ### Remove signal handler
    exit
}

#================================================
# getBytes - stores byte counter in return_val  =
#================================================
getBytes()
{
    return_val=`./gtmem -u $UNIT -R -o 0x54 -c 1`
    ((return_val=return_val*4))

#    return_val=`./gtmon -u $UNIT -v | grep Bytes | sed s/^.*://`
#    ((return_val=return_val)) #make it an integer, remove whitespaces
}

#================================================
# testByteAccounting - need I say more          =
#================================================
testByteAccounting()
{
    iter=${iter:-4000}
    getBytes
    bytes=$return_val

    ./gttp -u $UNIT -Pw -t 1 -l $SIZE -n $iter > /dev/null
    usleep 100 #allow time for byte propagation
    getBytes
    (( wrote=bytes+((SIZE*iter)) ))

    ((lost=return_val-wrote))
    printf "%-4i : %-11i : %-11i : %i\n" $passCnt $wrote $return_val $lost;

    if [ $wrote -ne $return_val ]
    then
        return 1
    fi

    return 0
}

printHeader()
{
    echo "---------------------------------------------"
    echo "Iter | Expected    | Found       | Difference"
    echo "---------------------------------------------"
}

#########################################################################
# SCRIPT EXECUTION STARTS HERE                                          #
#########################################################################

if [ "$1" == "-h" ]
then
    usage 1
    exit
fi

if [ $# -lt 1 ]
then
    usage
fi

UNIT=${1:-0}
SEC=${2:-10}
SIZE=${3:-4}
DOREADS=${4:-1}

pid=0                 ## set to invalid pid of 0
((SIZE=((SIZE*4))/4)) ## ensure size is a multiple of four

## determine max iteration per second for this packet size
iterPerSec=`./gttp -u $UNIT -PwS -t 1 -l $SIZE | grep _w_ | cut -c13-24`
((iter=(iterPerSec/24)+1)) ## set num of iters to run per test iteration

if [ $DOREADS -eq 1 ]
then
    ### trap keyboard interrupt to insure gttp gets killed
    trap ctrl_C_trap 2

    echo "Running gttp for $SEC seconds in the background."
    ./gttp -u $UNIT -PS -t 1 -s $SEC -l 4 -n 1 > /dev/null &
    pid=$!
fi

echo "Performing byte accounting test..."

passCnt=0
failed=0
#SECONDS is a built-in bash shell variable that counts elapsed seconds
SECONDS=0
((SEC=SEC+1))  ## to give gttp a chance to exit

# set "spyid"
./gtmon -y `./gtmon --nodeid`

printHeader

while [ $SECONDS -le $SEC ]; # successful
do
    ((passCnt=passCnt+1))
    testByteAccounting
    ((failed=failed+$?))  ## perform byte accounting test

    ((x=passCnt%20))

    if [ $x -eq 0 ]
    then
        #echo
        echo "Iterations $passCnt - Seconds $SECONDS - Failures $failed"
        printHeader
    fi
done;

ctrl_C_trap
