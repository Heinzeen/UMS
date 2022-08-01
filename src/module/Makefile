KDIR = /lib/modules/$(shell uname -r)/build
obj-m += UMS.o
UMS-objs := UMSmain.o UMSProcManager.o

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean