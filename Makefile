obj-m+=dmp.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

test: all
	sudo insmod dmp.ko
	@lsmod | grep dmp >/dev/null
	sudo dmsetup create zero1 --table "0 1024 zero"
	@ls /dev/mapper/zero1 >/dev/null
	sudo dmsetup create dmp1 --table "0 1024 dmp /dev/mapper/zero1"
	@ls /dev/mapper/dmp1 >/dev/null
	sudo dd if=/dev/random of=/dev/mapper/dmp1 bs=1024 count=1
	sudo dd of=/dev/null if=/dev/mapper/dmp1 bs=1024 count=1
	cat /sys/module/dmp/stat/volumes
