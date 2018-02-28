#!/bin/sh
#GT gdriver.sh  Rev. Thu Feb 12 15:18:39 EST 2004

########### get script location ###########
FNAME=`which $0`
NSLASHES=`echo $FNAME | tr -d -c "/" | wc -c`
NSLASHES=`echo $NSLASHES`   # get rid of leading white space
SCRIPT_DIR_NAME=`echo $FNAME | cut -d/ -f "1-$NSLASHES"`
cd $SCRIPT_DIR_NAME
SCRIPT_DIR_NAME=`pwd`
###########################################

cd ../driver

if [ "$1" = "start" ]
then
	./gtload
        cd $SCRIPT_DIR_NAME
        if [ "$2" != "" ]; then
            ./gconf.sh -a
        else
            sleep 0
        fi
        ./gtmon -N |grep -v gtmon
        ./gtmon -A     
elif [ "$1" = "stop" ]
then
	./gtunload
elif [ "$1" = "restart" ]
then
    ./gtunload
    ./gtload
else
	echo GT driver start/stop script
	echo "./gdriver.sh start   - should start the driver"
	echo "./gdriver.sh start r - starts the driver, resets all GT, and sets nId"
	echo "./gdriver.sh stop    - should stop it"
fi

