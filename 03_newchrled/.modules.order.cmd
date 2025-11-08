cmd_/home/chen/Linux_Drivers/03_newchrled/modules.order := {   echo /home/chen/Linux_Drivers/03_newchrled/newchrled.ko; :; } | awk '!x[$$0]++' - > /home/chen/Linux_Drivers/03_newchrled/modules.order
