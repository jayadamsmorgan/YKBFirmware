WB   = west build app --pristine
MENU = -t menuconfig
DBG  = -DEXTRA_CONF_FILE=conf/debug.conf

DACTYL_V1 = -b dactyl_v1/nrf5340/cpuapp/left
CHOCO_V1  = -b choco_v1/nrf5340/cpuapp/left

dactyl_v1:
	$(WB) $(DACTYL_V1)

dactyl_v1_menu:
	$(WB) $(DACTYL_V1) $(MENU)

dactyl_v1_dbg:
	$(WB) $(DACTYL_V1) -- $(DBG)

dactyl_v1_dbg_menu:
	$(WB) $(DACTYL_V1) $(MENU) -- $(DBG)

choco_v1:
	$(WB) $(CHOCO_V1)

choco_v1_menu:
	$(WB) $(CHOCO_V1) $(MENU)

choco_v1_dbg:
	$(WB) $(CHOCO_V1) -- $(DBG)

choco_v1_dbg_menu:
	$(WB) $(CHOCO_V1) $(MENU) -- $(DBG)

clean:
	rm -rf build
