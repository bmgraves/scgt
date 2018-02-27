#!/bin/bash

#GT gfwtest.sh  Rev. Mon Feb  2 09:54:43 EST 2004 - test, private, Linux only

#================================================
# usage - prints script usage                   =
#================================================
usage()
{
    echo
    echo "Usage:     $0  [-u unit] [-d delay] [-l] [-x] [-h]"
    echo "Defaults:  $0 -u 0"
    echo
    if [ $# -gt 0 ]
    then
        echo "Options:"
        echo "   -u # - the unit to test"
        echo "   -d # - ms to delay after gtmon config change (default 100)"
        echo "   -l   - loop, run test infinitely"
        echo "   -x   - try to fix link-up failures by sending data"
        echo
        echo "NOTES: The unit under test must be on a loop by itself.";
        echo "       For test summaries only, apply filter \"grep __\".";
        echo
    fi
    return 0
}

#################################################################
# FUNCTIONS FOR STORING AND RESTORING CONFIGURATION             #
#################################################################

CARRAY=( --nodeid --interface --spyid --bint --rx --tx --px --ewrap --wlast
         --laser0 --laser1 --sint --uint )
declare -a CVAL

#================================================
# getConfig - stores device configuration       =
#================================================
getConfig()
{
    local unit=${1:-$UNIT}
    local i

    CVAL[0]=`./gtmon -u $unit ${CARRAY[0]}`
    CVAL[1]=`./gtmon -u $unit ${CARRAY[1]}`
    CVAL[2]=`./gtmon -u $unit ${CARRAY[2]}`
    CVAL[3]=`./gtmon -u $unit ${CARRAY[3]}`

    # for the rest of the elements in CARRAY, generate restoration string
    for (( i = 4; i < ${#CARRAY[@]}; i++ ))
    do
        if [ `./gtmon -u $unit ${CARRAY[$i]}` == "1" ]
        then
            CVAL[$i]=${CARRAY[$i]}on
        else
            CVAL[$i]=${CARRAY[$i]}off
        fi
    done
}

#================================================
# setConfig - restores device configuration     =
# pre: getConfig has been called, unit is valid =
#================================================
setConfig()
{
    local unit=${1:-$UNIT}
#   echo "./gtmon -u $unit -n ${CVAL[0]} -i ${CVAL[1]} -y ${CVAL[2]} -b ${CVAL[3]} ${CVAL[@]:4:${#CVAL[@]}}"
    ./gtmon -u $unit -n ${CVAL[0]} -i ${CVAL[1]} -y ${CVAL[2]} -b ${CVAL[3]} ${CVAL[@]:4:${#CVAL[@]}}
}

#################################################################
# The following functions do not affect the setup of the device #
#################################################################

hexStringToInt(){  ((x=$1));  echo $x;  }

#================================================
# try_write - tests write functionality         =
#================================================

MASK=0xffffffff

try_write()
{
    #Parm 1 is value to write, default 0xa5a5a5a5

    RVAL=0
    WVAL1=${1:-0xa5a5a5a5}
    ((WVAL2=~$WVAL1 & $MASK))

    ./gtmem -u $UNIT -w $WVAL1 -o 0 -c 1 -b 4 -P > /dev/null
    ./gtmem -u $UNIT -w $WVAL2 -o 4 -c 1 -b 4 -P > /dev/null
    RVAL=`./gtmem -u $UNIT -o 0 -c 1 -b 4 -P`

    if [ $RVAL != $WVAL1 ]; then  return 1;  fi
    return 0
}

#================================================
# try_read - tests read functionality           =
#================================================
try_read()
{
    RVAL1=`./gtmem -u $UNIT -o 0 -c 1000 -b 4 -P`
    RVAL2=`./gtmem -u $UNIT -o 0 -c 1000 -b 4 -P`
    if [ "$RVAL1" != "$RVAL2" ]; then  return 1;  fi
    return 0
}

#================================================
# test_linkup - checks if link is up            =
#================================================
test_linkup()
{
   RET=`./gtmon -u $UNIT --linkup`
   if [ $RET -eq 0 ]; then  return 1;  fi
   return 0
}

###############################################################
# The following functions may affect the setup of the device  #
###############################################################

#===================================================
# clear_mem - writes 0 to first 10 words of memory =
#===================================================
clear_mem()
{
    setMode "--wlastoff"

    ./gtmem -u $UNIT -o 0 -c 10 -w 0 -b 4 -P > /dev/null
    if [ $? != 0 ]; then  return 1;  fi
    return 0
}

#================================================
# test_laser - tests laser on/off functionality =
#================================================
test_laser()
{
    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron"
    test_linkup
    if [ $? != 0 ]; then  return 1;  fi  #Should have passed

    setMode "--laseroff"
    sleep 1
    test_linkup
    if [ $? == 0 ]; then return 2;  fi  #Should have failed

    setMode "--laseron"
    sleep 1
    test_linkup
    if [ $? != 0 ]; then  return 3;  fi  #Should have passed

    return 0
}

#================================================
# test_reads - tests read functionality         =
# pre - link must be up                         =
#================================================
test_reads()
{
    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron"
    try_read
    if [ $? != 0 ]; then  return 1;  fi

    return 0
}

#================================================
# test_writes  - tests write functionality      =
# precondition - link must be up                =
#================================================
test_writes()
{
    clear_mem

    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron"

    fixLinkUp

    try_write
    if [ $? != 0 ]; then  return 1;  fi

    return 0
}

#================================================
# test_wlast_path - tests write-last path       =
# pre - laser, tx path, or rx path must work    =
#================================================
test_wlast_path()
{
    clear_mem

    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron"
    fixLinkUp

    try_write
    if [ $? != 0 ]; then  return 1;  fi  #Should have passed

    setMode "--txoff --laseroff --rxoff"
    sleep 1

    try_write 0xb4b4b4b4
    if [ $? == 0 ]; then  return 2;  fi  #Should have failed

    setMode "--wlastoff"

    try_write 0xc3c3c3c3
    if [ $? != 0 ]; then  echo;  return 3;  fi  #Should have passed

    return 0
}

#================================================
# test_tx_path - tests transmit path            =
#================================================
test_tx_path()
{
    clear_mem

    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron"
    fixLinkUp

    try_write
    if [ $? != 0 ]; then  return 1;  fi  #Should have passed

    setMode "--txoff"
    try_write 0xb4b4b4b4
    if [ $? == 0 ]; then  return 2;  fi  #Should have failed

    setMode "--wlastoff"
    try_write 0xc3c3c3c3
    if [ $? != 0 ]; then  return 3;  fi  #Should have passed

    return 0
}

#================================================
# test_rx_path - tests receive path             =
#================================================
test_rx_path()
{
    clear_mem

    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron"
    fixLinkUp

    try_write
    if [ $? != 0 ]; then  return 1;  fi  #Should have passed

    setMode "--rxoff"
    try_write 0xb4b4b4b4
    if [ $? == 0 ]; then  return 2;  fi  #Should have failed

    setMode "--wlastoff"
    try_write 0xc3c3c3c3
    if [ $? != 0 ]; then  return 3;  fi  #Should have passed

    return 0
}

#================================================
# test_rt_path - tests retransmit path          =
#================================================
test_rt_path()
{
# No way to test this with one device
#    if [ $? != 0 ]; then  return 1;  fi

    return 0
}

#================================================
# test_ewrap_path - tests ewrap path            =
# pre - laser on/off must work                  =
#================================================
test_ewrap_path()
{
    clear_mem

    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron"

    fixLinkUp

    try_write
    if [ $? != 0 ]; then  return 1;  fi  #Should have passed

    setMode "--laseroff"

    sleep 1

    try_write 0xb4b4b4b4
    if [ $? == 0 ]; then  return 2;  fi  #Should have failed

    setMode "--ewrapon"

    fixLinkUp

    try_write 0xc3c3c3c3
    if [ $? != 0 ]; then  return 3;  fi  #Should have passed

    return 0
}

#==================================================================
# gtnex_Q_wlaston - performs gtnex quick test with write-last on  =
#==================================================================
gtnex_Q_wlaston()
{
    setMode "--txon --rxon --wlastoff --ewrapoff --pxon --laseron"
    ./gtmem -u $UNIT -Aw 1 > /dev/null
    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron"
    fixLinkUp
    gtnex_quicktest
    return $?;
}

#===================================================================
# gtnex_Q_wlaston - performs gtnex quick test with write-last off  =
#===================================================================
gtnex_Q_wlastoff()
{
    setMode "--txon --rxon --wlastoff --ewrapoff --pxon --laseron"
    ./gtmem -u $UNIT -Aw 0 > /dev/null

    fixLinkUp
    gtnex_quicktest
    return $?;
}

#==============================================
# gtnex_quicktest - performs gtnex quick test =
#==============================================
gtnex_quicktest()
{
    RET=`./gtnex -u $UNIT -d 4 -PQ | grep -i Total | sed s/[^0-9]//g`

    if [ $RET != 0 ]
    then
        echo -n $RET errors...
        return 1;
    else
        return 0;
    fi
}

#================================================
# test_hw_intr - verifies HW interrupt support  =
#================================================
test_hw_intr()
{
    local NET_INTR_CNTR_OFFSET=0x58

    setMode "--txon --rxon --wlaston --ewrapoff --pxon --laseron --sinton --uinton -b 0xffffffff"

    fixLinkUp

    START_CNT=`./gtmem -u $UNIT -R -o $NET_INTR_CNTR_OFFSET`
    START_CNT=`hexStringToInt $START_CNT`

    #NODEID is set up in "main", only once

    NUM_SENT=5
    ./gtint -u $UNIT -U -i $NODEID -n $NUM_SENT  #send unicast interrupts to me
    ./gtint -u $UNIT -BA  # will send each of 32 broacast interrupts
    ((NUM_SENT+=32))

    END_CNT=`./gtmem -u $UNIT -R -o $NET_INTR_CNTR_OFFSET`
    END_CNT=`hexStringToInt $END_CNT`

    if [ $END_CNT -lt $START_CNT ]
    then
        ((DIFF=END_CNT + 1 + ((0xffffffff - START_CNT))))
    else
        ((DIFF=END_CNT - START_CNT))
    fi

    if [ $DIFF != $NUM_SENT ]
    then
        return 1;
    fi

    return 0;
}

#================================================
# cleanup - cleans up mess, and prints stats    =
#================================================
cleanup()
{
    local i=${2:-1}; #Show stats flag is parm 2. Default is true.

    setConfig ${1:-$UNIT} # restore original configuration

    if [ $i -ne 0 ]; then printStats; fi;

    exit
}

#================================================
# printStats - displays statistics              =
#================================================
printStats()
{
    local i;
    local TPASS=0;
    local TFAIL=0;

    printf "\n__TEST              ITERS      PASSCNT    FAILCNT    FAIL%%\n"
    for (( i = 0; i < ${#TEST_ARRAY[@]}; i++ ))
    do
        ((TPASS+=${PASS_ARRAY[$i]}))
        ((TFAIL+=${FAIL_ARRAY[$i]}))
        ((total=${PASS_ARRAY[$i]}+${FAIL_ARRAY[$i]}))

        if [ $total -ne 0 ];
        then
            ((intgr=((${FAIL_ARRAY[$i]}*10000))/$total));
            ((fract=intgr%100))
            ((intgr=intgr/100))
        else
            intgr=0
            fract=0
        fi;

        #printf statement must all be on one line, use '\' for multi-line
        printf "__%-16s  %-9i  %-9i  %-9i  %3i.%.2i\n" ${TEST_ARRAY[$i]} \
               $total ${PASS_ARRAY[$i]} ${FAIL_ARRAY[$i]} $intgr $fract;
    done

    ((total=$TPASS+$TFAIL))

    if [ $total -ne 0 ];
    then
        ((intgr=(($TFAIL*10000))/$total));
        ((fract=intgr%100))
        ((intgr=intgr/100))
    else
        intgr=0
        fract=0
    fi;

    printf "__TOTALS            %-9i  %-9i  %-9i  %3i.%.2i\n\n" \
            $total $TPASS $TFAIL $intgr $fract

    if [ $FIX_LINKUP -ne 1 ]; then return 0; fi;  # fixLinkUp is turned off

    ((i=$FIX_LU_TRIES-$FIX_LU_FAILURES))
    printf "__LINKUP_FIXES     %-9i  %-9i  %-9i\n\n" \
            $FIX_LU_TRIES $i $FIX_LU_FAILURES
}

#==================================================
# setMode - set the gt device mode and optionally =
#           adds delay after the mode change      =
#==================================================
setMode()
{
    if [ $DEBUG -eq 1 ]; then  echo ./gtmon -u $UNIT $1; fi;

    ./gtmon -u $UNIT $1 > /dev/null

    if [ $ADD_DELAY -eq 0 ]; then return 0; fi;  #adding of delay is turned off

    if [ $DEBUG -eq 1 ]; then  echo "Delaying for $ADD_DELAY us."; fi;
    usleep $ADD_DELAY;
}

#==================================================
# fixLinkUp - Accounts for and tries to correct   =
# FW bug when link is supposed to be up but isn't.=
#==================================================
fixLinkUp()
{
    if [ $FIX_LINKUP -ne 1 ]; then return 0; fi;  # fixLinkUp is turned off

    local unit=${1:-$UNIT}
    linkup=`./gtmon -u $unit --linkup`
    if [ $linkup -eq 1 ]; then return 0; fi;  # nothing to fix

    ((FIX_LU_TRIES++))

    try_write 0x12345678    # writes to the network usually fix linkup problem

    linkup=`./gtmon -u $unit --linkup`
    if [ $linkup -eq 1 ]; then return 0; fi;  # fixed
    ((FIX_LU_FAILURES++))
}

#================================================
# unitExists - tests that unit is present       =
#================================================
unitExists()
{
    local unit=${1:-$UNIT}
    echo $unit | grep "[^0-9a-fA-F]" > /dev/null  # check for non-hex digit
    if [ $? -eq 0 ]; then echo "__Invalid unit number ($unit)"; exit; fi;
    ./gtmon -u $unit > /dev/null
    if [ $? -ne 0 ]; then echo "__Invalid unit number ($unit)"; exit; fi;
}

#======================================================
# isInLoopback - tests that unit is on loop by itself =
#======================================================
isInLoopback()
{
    setMode "--laseron --ewrapoff --txon --rxon --pxon --wlaston"
    sleep 1

    NODEID=`./gtmon --nodeid -u $UNIT`      #store original node ID

    for nID in $NODEID 1 2
    do
        ./gtmon -u $UNIT -n $nID
        RINGSIZE=`./gtmon -u $UNIT --ringsize`

        if [ $RINGSIZE -ne 1 ];
        then
        echo; echo "__Device not on a loop by itself. Ring size = $RINGSIZE";
        echo;
        ./gtmon -u $UNIT -n $NODEID
        cleanup $UNIT 0
        fi;
    done;

    ./gtmon -n $NODEID -u $UNIT       #restore original node ID
}

#########################################################################
# SCRIPT EXECUTION STARTS HERE                                          #
#########################################################################

if [ $# -lt 1 ]
then
    usage
fi

#default options
UNIT=0
LOOP=-1
ADD_DELAY=100000
DEBUG=0
FIX_LINKUP=0  # all things related are temporary

FIX_LU_TRIES=0
FIX_LU_FAILURES=0

#parse options
while getopts ":u:d:lxhz" Option
do
  case $Option in
    u ) UNIT=$OPTARG;;
    d ) ADD_DELAY=$OPTARG; ((ADD_DELAY=ADD_DELAY*1000));;
    l ) LOOP=1;;
    x ) FIX_LINKUP=1;;
    z ) DEBUG=1;;
    h ) usage 1; exit;;
    * ) echo "__Invalid or incorrectly used option \"-$OPTARG\""; exit;;
  esac
done

((x=$OPTIND-1))
if [ $# -ne $x ]; then echo "Invalid option string"; exit; fi;
# done parsing options

# TEST_ARRAY holds the function names of the tests to perform
TEST_ARRAY=( test_laser       test_reads     test_writes       test_wlast_path
             test_tx_path     test_rx_path   test_ewrap_path   gtnex_Q_wlaston
             gtnex_Q_wlastoff test_hw_intr )

#TEST_ARRAY=( test_hw_intr )

PASS_ARRAY=( 0 0 0 0 0 0 0 0 0 0 )
FAIL_ARRAY=( 0 0 0 0 0 0 0 0 0 0 )

UNIX_APPS="grep sed sleep usleep"
EXTERNAL_APPS="./gtmon ./gtmem ./gtnex ./gtint"
RET=0

for app in $UNIX_APPS
do
    type $app 2>&1 &> /dev/null # redirect stdout and stderr
    if [ $? != 0 ]
    then
        echo "__ERROR: Unix application \"$app\" doesn't exist!!!!"
        RET=1
    fi
done

for app in $EXTERNAL_APPS
do
    if [ ! -e $app ] || [ ! -x $app ]
    then
        echo "__ERROR: External GT application $app doesn't exist!!!!"
        RET=1
    fi
done

if [ $RET -eq "1" ]
then
    exit
fi

unitExists $UNIT   # verify unit number is valid
getConfig  $UNIT   # store original configuration
trap cleanup 2     # trap Control-C (2) to ensure setConfig gets called
isInLoopback       # verify that device is on loop by itself

NODEID=`./gtmon --nodeid -u $UNIT`      #store node ID

while [ $LOOP -ne 0 ];
do
    for (( i = 0; i < ${#TEST_ARRAY[@]}; i++ ))
    do
        echo -n "${TEST_ARRAY[$i]}..."
        ${TEST_ARRAY[$i]}
        RET=$?
        if [ $RET != 0 ];
        then
            ((FAIL_ARRAY[$i]++))
            echo "FAILURE code $RET"
            echo STATE AFTER FAILURE is
            ./gtmon -u $UNIT | grep -v gtmon
        else
            ((PASS_ARRAY[$i]++))
            echo "SUCCESS"
        fi
    done

    printStats
    ((LOOP=LOOP+1))
done

cleanup $UNIT
