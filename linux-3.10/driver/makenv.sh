#!/bin/bash
#
# Configure environment for building driver by dumping vars to a file
# to be included into the driver makefile.
# Nothing GT specific.
# 

# usage:
#     ./makenv.sh [MODDIR]
#         creates build configuration in MODDIR
#         if MODDIR is not specified (usual situation) MODDIR is
#         set to `uname -r`-`uname -m`
#         Usually, the makefile sets up MODDIR and calls this.

if [ "$#" = "0" ];
then
    MACHINE=`uname -m`
    KERNEL=`uname -r`
    MODDIR=$KERNEL-$MACHINE 
else
    MODDIR=$1
fi


if [ -d $MODDIR ]; then
    echo $MODDIR exists.
else
    mkdir $MODDIR
fi

f=$MODDIR/ENVIRONMENT

cat <<EOM
***
*** Configuring the SCRAMNet GT Linux driver.
***
EOM

############### Get kernel build directory ###################
DEFAULT_BUILD_DIR="/lib/modules/`uname -r`/build/"

if [ ! -f $DEFAULT_BUILD_DIR/.config ]
then
    # let's guess again
    DEFAULT_BUILD_DIR="/usr/src/linux"
fi

echo ""
echo "Please enter the path to kernel build directory."
echo "The build directory contains the .config configuration file for your kernel."
echo "On some systems this is the kernel source directory."
echo "On Fedora and SUSE, /lib/modules/`uname -r`/build/ is the build directory."
echo -n "[$DEFAULT_BUILD_DIR]: "
read BUILD_DIR
if [ "$BUILD_DIR" == "" ]; then
    BUILD_DIR=$DEFAULT_BUILD_DIR
fi

while [ ! -d $BUILD_DIR -o ! -f $BUILD_DIR/.config ]; do

    if [ ! -d $BUILD_DIR ]; then
        echo "$BUILD_DIR does not exist."
    elif [ ! -f $BUILD_DIR/.config ]; then
        echo "$BUILD_DIR is not a valid build directory."
        echo ".config does not exist in $BUILD_DIR"
    fi
    
    echo ""
    echo "Please try again."
    echo -n "[$DEFAULT_BUILD_DIR]: "
    read BUILD_DIR

    if [ "$BUILD_DIR" == "" ]; then
      BUILD_DIR=$DEFAULT_BUILD_DIR
    fi 
done


############### Get kernel source directory ###################
if [ -f $BUILD_DIR/Makefile ];
then
    SRC_DIR=$BUILD_DIR
else
    DEFAULT_SRC_DIR="/usr/src/linux/"

    echo ""
    echo "Please enter the path to kernel source directory."
    echo -n "[$DEFAULT_SRC_DIR]: "
    read SRC_DIR
    if [ "$SRC_DIR" == "" ]; then
        SRC_DIR=$DEFAULT_BUILD_DIR
    fi
    while [ ! -d $SRC_DIR -o ! -f $SRC_DIR/Makefile ]; do

        if [ ! -d $SRC_DIR ]; then
            echo "$SRC_DIR does not exist."
        elif [ ! -f $SRC_DIR/Makefile ]; then
            echo "$SRC_DIR is not a valid kernel source directory."
            echo "Makefile does not exist in $SRC_DIR"
        fi

        echo ""
        echo "Please try again."
        echo -n "[$DEFAULT_SRC_DIR]: "
        read SRC_DIR

        if [ "$SRC_DIR" == "" ]; then
          SRC_DIR=$DEFAULT_SRC_DIR
        fi 
    done

fi


cat >$f <<EOM
#
# This file is automatically generated by 'make config'.
#
EOM

echo "KERN_BUILD_DIR := $BUILD_DIR" >> $f
echo "KERN_SRC_DIR := $SRC_DIR" >> $f


echo "" >>$f

echo ""
echo "*** Configuration complete."

