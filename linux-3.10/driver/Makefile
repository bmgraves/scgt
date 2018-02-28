#
# SCRAMNet GT driver Makefile for Linux 2.6 
#

#
# Usage:
#      make CONFIG=configuration
#      Setting CONFIG is optional (useful for cross-compiling).  
#      The driver module will end up in the directory named $(CONFIG).
#      This driver can be loaded using ./gtload <configName>
#

########## check for ENVIRONMENT file ############

# Build CONFIG  (see comment below):
MACHINE = $(shell uname -m)
KERNREV = $(shell uname -r)
CONFIG = $(KERNREV)-$(MACHINE)

# The configuration directory is passed in as CONFIG=configuration
# by the user or is set above.
# We will use GTCONFIGDIR from this point on to avoid confusion.

GTCONFIGDIR = $(CONFIG)

ifeq ($(wildcard $(GTCONFIGDIR)/ENVIRONMENT),$(GTCONFIGDIR)/ENVIRONMENT)

include $(GTCONFIGDIR)/ENVIRONMENT

## ENVIRONMENT file found ##

######### check for KERNELRELEASE ########

ifneq ($(KERNELRELEASE),)   ##### KERNELRELEASE is set... we can build the module #####

src ?= .
obj ?= .

# Directories 
GTCOREDIR = $(GTDVRDIR)../../gtcore
GTKSYSDIR = $(GTDVRDIR)../ksys
INCDIR = $(GTDVRDIR)../../inc

GTINCLUDE_FLAGS = -I$(GTCOREDIR) -I$(GTKSYSDIR) -I$(INCDIR)
EXTRA_CFLAGS += $(GTINCLUDE_FLAGS) -I$(GTDVRDIR)
EXTRA_LDFLAGS := -d

obj-m = scgt_module.o
scgt_module-objs = scgt.o ../ksys/ksys.o ../../gtcore/gtcore.o ../../gtcore/gtcorexfer.o


else  ################### KERNELRELEASE is not set.. must execute make ########################


PWD = $(shell pwd)

ifneq ($(KERN_BUILD_DIR), $(KERN_SRC_DIR))
BUILD_DIR_ARGS = "O=$(KERN_BUILD_DIR)"
endif

# Call kernel makefile to build module.
# Pass the full path to GTCONFIGDIR because we are changing directories.
# The ENVIRONMENT file is still included.
# GTDVRDIR is used to set full path to GTCOREDIR etc. and others above.

scgt_module.ko:
	@$(MAKE) -C $(KERN_SRC_DIR) $(BUILD_DIR_ARGS) SUBDIRS=$(PWD) GTCONFIGDIR=$(PWD)/$(GTCONFIGDIR) GTDVRDIR=$(PWD)/ modules
	@mv scgt_module.ko $(GTCONFIGDIR)
	@$(MAKE) clean

endif  # KERNELRELEASE

else  # ENVIRONMENT file doesn't exist

all: config
	@$(MAKE)

endif # ENVIRONMENT file.

config:
	@./makenv.sh $(GTCONFIGDIR)

clean:
	@rm -rf `find . -name ".*.cmd*"`
	@rm -rf *.mod.* 
	@rm -rf `find ../ksys -name ".*.cmd*"`
	@rm -rf `find ../../gtcore -name ".*.cmd*"`
	@rm -rf scgt.o scgt_module.o ../../gtcore/*.o ../ksys/*.o

