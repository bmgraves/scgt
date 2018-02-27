#!/bin/sh

#auto.sh  Rev. Mon Dec  8 10:08:02 EST 2003

cmd="unset"
n="-1"
s="1"

printUsage()
{
    echo "USAGE:"
    echo "    $0 [-s sleepTime] [-n numTimes] cmd"
    echo "OPTIONS:"
    echo "    -s sleepTime:  "
    echo "         sleepTime is passed to sleep between each command execution."
    echo "         On Linux this can be in the form: NUMBER[SUFFIX]"
    echo "         Where number is an integer or floating number."
    echo "         SUFFIX can be 's' for seconds (default), 'm' for minutes,"
    echo "         or 'd' for days."
    echo "    -n numTimes: "
    echo "         numTimes is the number of times to run the command"
    echo "         -1 to run forever (default)" 
}


if [ $# -lt "1" ]
then
    printUsage;
    exit 1;
fi


while [ $# -gt 0 ];
do
    case $1 in
        -s)
            shift;
            s=$1
            ;;
        -n)
            shift;
            n=$1
            ;;
        -*)
            echo ERROR: unknown flag: $1
            printUsage;
            exit 2;
            ;;
        *)
            cmd=$*
            break;
            ;;
    esac
    shift
done


if [ "$cmd" = "unset" ];
then
    echo "ERROR: No command specified"
    printUsage;
    exit 3;
fi

echo running: $0 -s $s -n $n $cmd

i=0
while [ $i -ne $n ]
do
    echo "[----   "`date +"%D   %T"` " dt: $SECONDS  i: $i""   ----]"
    echo "$cmd"
    sh -c "$cmd"
    sleep $s
    echo
    i=`expr $i + 1`
done

