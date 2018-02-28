# Linux GT Makefile
PLAT_DIR=linux-3.10

all: driver libscgt appli

driver:
	@echo "############### BUILDING DRIVER ###############"
	make -C $(PLAT_DIR)/driver/

libscgt: 
	@echo "################ BUILDING API #################"
	make -C api/ -f Makefile

config:
	make -C $(PLAT_DIR)/driver/ config

appli: 
	@echo "################ BUILDING APPS #################"  
	make -C apps/gtutils -f egt.linux-3.10.mak
clean:
	make -C api/ -f Makefile clean
	make -C $(PLAT_DIR)/driver/ clean

realclean: clean
	rm $(PLAT_DIR)/driver/*.o

