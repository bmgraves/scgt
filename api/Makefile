# SCRAMNet GT API makefile

PLAT_DIR    = ../linux-3.10
HDWARE_PLAT = $(shell uname -m)
DEST_DIR    = $(PLAT_DIR)/lib
COPY_DIR    = $(DEST_DIR)
TARGET      = $(DEST_DIR)/libscgtapi.a
COPY_TARGET = $(COPY_DIR)/libscgtapi_$(HDWARE_PLAT).a
API_INC     = ../inc
INC_FLAGS   = -I$(API_INC) -I$(PLAT_DIR)/driver -I../gtcore

CC        = gcc

AR           = ar
LD           = ld
CP           = cp
CFLAGS       = $(INC_FLAGS) $(CC_DEFINES)
CC_DEFINES   = -O2 -Wall -DPLATFORM_UNIX -DPLATFORM_USES_MMAP

LINKFLAGS = -r -o $(APP)
LPTH      = -L.

INCDEPS = $(API_INC)/scgtapi.h \
          $(PLAT_DIR)/driver/scgtdrv.h

$(TARGET): $(INCDEPS) scgtapi.o
	@if [ ! -d $(DEST_DIR) ]; then mkdir $(DEST_DIR); fi
	@if [ ! -d $(COPY_DIR) ]; then mkdir $(COPY_DIR); fi
	$(AR) rcs $@ scgtapi.o
	rm -f scgtapi.o
	cp $(TARGET) $(COPY_TARGET)
clean:
	rm -f $(TARGET)

