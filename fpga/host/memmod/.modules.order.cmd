cmd_/mnt/sda1/projs/memtest/memmod/modules.order := {   echo /mnt/sda1/projs/memtest/memmod/memtest_module.ko; :; } | awk '!x[$$0]++' - > /mnt/sda1/projs/memtest/memmod/modules.order
