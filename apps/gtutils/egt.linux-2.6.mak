#################### GT applications makefile ###################
####################        OS - Linux        ###################

# MAKEFILE VERSION 1.4

##### Applications: ###########
APPS = gttp gtmem gtnex gtmon gtprog gtint gtbert gtlat

# Additional test applications
TESTAPPS = gtspeed gtrand gtniv

HDWARE_PLAT = $(shell uname -m)
PLAT_DIR = ../../linux-2.6
TARGET_DIR = $(PLAT_DIR)/bin
COPY_DIR   = $(TARGET_DIR)_$(HDWARE_PLAT)
USYS_DIR   = .
LOCAL_INCS = .
INC_DIR    = ../../inc
GTLIB_DIR = $(PLAT_DIR)/lib
GTLIB = -lscgtapi_$(HDWARE_PLAT)

CC           = gcc
CP           = cp
LD           = $(CC)

CC_DEFINES  = -O2 -fomit-frame-pointer -DPLATFORM_UNIX \
              -DOS_VER_STRING="\"Linux\"" -Wall 
CC_INCLUDE  = -I$(LOCAL_INCS) -I$(INC_DIR)
CFLAGS      = $(CC_INCLUDE) $(CC_DEFINES)

COMMON_OBJS = $(USYS_DIR)/usys.o
INCS        = $(USYS_DIR)/usys.h

LIBPATH    = -L. -L$(GTLIB_DIR)
LIBS       = -lpthread -lm 
GTLIBS     = $(LIBS) $(GTLIB)


.PHONY: clean mkdirs scripts all sl240 sl_warn scramnet scram_warn 

################
all: mkdirs $(APPS) copyapps scripts
	rm -f *.o $(COMMON_OBJS)

################
test: $(TESTAPPS)

################
mkdirs:
	@if [ ! -d $(TARGET_DIR) ]; then mkdir $(TARGET_DIR); fi;
	@if [ ! -d $(COPY_DIR) ]; then mkdir $(COPY_DIR); fi

copyapps:
	cd $(TARGET_DIR); $(CP) $(APPS) $(COPY_DIR)

################
scripts: mkdirs
	$(CP) scripts/*.sh $(TARGET_DIR)/
	chmod 755 $(TARGET_DIR)/*.sh
	$(CP) scripts/*.sh $(COPY_DIR)/
	chmod 755 $(COPY_DIR)/*.sh

################
pscripts: mkdirs
	$(CP) pscripts/*.sh $(TARGET_DIR)/
	chmod 755 $(TARGET_DIR)/*.sh
	$(CP) pscripts/*.sh $(COPY_DIR)/
	chmod 755 $(COPY_DIR)/*.sh

###################
gttp.o: gttp.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gttp : mkdirs $(COMMON_OBJS) gttp.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtnex.o: gtnex.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtnex : mkdirs $(COMMON_OBJS) gtnex.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtmem.o: gtmem.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtmem : mkdirs $(COMMON_OBJS) gtmem.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtmon.o: gtmon.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtmon : mkdirs $(COMMON_OBJS) gtmon.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtprog.o: gtprog.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtprog : mkdirs $(COMMON_OBJS) gtprog.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtlat.o: gtlat.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtlat : mkdirs $(COMMON_OBJS) gtlat.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtbert.o: gtbert.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtbert : mkdirs $(COMMON_OBJS) gtbert.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtint.o: gtint.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtint : mkdirs $(COMMON_OBJS) gtint.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtspeed.o: gtspeed.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtspeed : mkdirs $(COMMON_OBJS) gtspeed.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtrand.o: gtrand.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtrand : mkdirs $(COMMON_OBJS) gtrand.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtniv.o: gtniv.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtniv : mkdirs $(COMMON_OBJS) gtniv.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

###################
gtsem.o: gtsem.c
	$(CC) $(CFLAGS) -o $@ -c $(LIBPATH) $?
gtsem : mkdirs $(COMMON_OBJS) gtsem.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(GTLIBS)

################
$(COMMON_OBJS): $(INCS)

#Define the implicit rule for building .o files from .c files
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f $(COMMON_OBJS) 
	-rm -f *.o

#####################################################
# THE FOLLOWING CAN BE REMOVED WHEN DONE WITH SL240 #
#####################################################

sl240: sl_warn xgttp xgtbert xgtprog

# Must set up SL240BASE
SL240BASE=/borg1a/ex240
SL240_DEFINES = -DHW_SL240 -I$(SL240BASE)/inc -I$(SL240BASE)/lib
SL240_LIBS = -L$(SL240BASE)/bin -lfxsl

sl_warn:
	@echo "VARIABLE \"SL240BASE\" MUST POINT TO THE ROOT"
	@echo "OF YOUR SL240 DIRECTORY TREE"

xgttp.o: gttp.c
	$(CC) $(CFLAGS) $(SL240_DEFINES) -o $@ -c $(LIBPATH) $?
xgttp : mkdirs $(COMMON_OBJS) xgttp.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(LIBS) $(SL240_LIBS)

xgtbert.o: gtbert.c
	$(CC) $(CFLAGS) $(SL240_DEFINES) -o $@ -c $(LIBPATH) $?
xgtbert : mkdirs $(COMMON_OBJS) xgtbert.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(LIBS) $(SL240_LIBS)

xgtmem.o: gtmem.c
	$(CC) $(CFLAGS) $(SL240_DEFINES) -o $@ -c $(LIBPATH) $?
xgtmem : mkdirs $(COMMON_OBJS) xgtmem.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(LIBS) $(SL240_LIBS)

xgtprog.o: gtprog.c
	$(CC) $(CFLAGS) $(SL240_DEFINES) -o $@ -c $(LIBPATH) $?
xgtprog : mkdirs $(COMMON_OBJS) xgtprog.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(LIBS) $(SL240_LIBS)

########################################################
# THE FOLLOWING CAN BE REMOVED WHEN DONE WITH SCRAMNET #
########################################################

scramnet: scram_warn sgttp sgtmem sgtnex sgtbert

# Must set up SCRAMBASE
SCRAMBASE=/borg1a/scramnet
SCR_DEFINES = -DHW_SCFF -I$(SCRAMBASE)/inc 
SCR_LIBS = -L$(SCRAMBASE)/lib -lhwd -lplus

scram_warn:
	@echo "VARIABLE \"SCRAMBASE\" MUST POINT TO THE ROOT"
	@echo "OF YOUR SCRAMNET DIRECTORY TREE"

sgttp.o: gttp.c
	$(CC) $(CFLAGS) $(SCR_DEFINES) -o $@ -c $(LIBPATH) $?
sgttp : mkdirs $(COMMON_OBJS) sgttp.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(LIBS) $(SCR_LIBS)

sgtmem.o: gtmem.c
	$(CC) $(CFLAGS) $(SCR_DEFINES) -o $@ -c $(LIBPATH) $?
sgtmem : mkdirs $(COMMON_OBJS) sgtmem.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(LIBS) $(SCR_LIBS)

sgtnex.o: gtnex.c
	$(CC) $(CFLAGS) $(SCR_DEFINES) -o $@ -c $(LIBPATH) $?
sgtnex : mkdirs $(COMMON_OBJS) sgtnex.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(LIBS) $(SCR_LIBS)

sgtbert.o: gtbert.c
	$(CC) $(CFLAGS) $(SCR_DEFINES) -o $@ -c $(LIBPATH) $?
sgtbert : mkdirs $(COMMON_OBJS) sgtbert.o
	$(LD) -o $(TARGET_DIR)/$@ $(LIBPATH) $(COMMON_OBJS) $@.o $(LIBS) $(SCR_LIBS)
