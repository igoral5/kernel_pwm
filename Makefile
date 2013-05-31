CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)
DEST = /lib/modules/$(CURRENT)/misc
TARGET = sysfsexample

obj-m      := $(TARGET).o

default:    
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:    
	cp -v $(TARGET).ko $(DEST)
	/sbin/depmod -a

uninstall:  
	/sbin/rmmod $(TARGET)
	rm -v $(DEST)/$(TARGET).ko
	/sbin/depmod

clean:  
	@rm -f *.o .*.cmd .*.flags *.mod.c *.order
	@rm -f .*.*.cmd *.symvers *~ *.*~
	@rm -fR .tmp*
	@rm -rf .tmp_versions