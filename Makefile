obj-m += panel-jdi-lt031mdz4000.o

BUILD_DIR ?= /lib/modules/$(shell uname -r)/build
DRIVER_PATH ?= /lib/modules/$(shell uname -r)/kernel/drivers/gpu/drm/panel/panel-jdi-lt031mdz4000.ko
OVERLAYS_DIR ?= /boot/overlays/

all:
	make -C $(BUILD_DIR) M=$(PWD) modules
	dtc -@ -I dts -O dtb -o vc4-kms-dsi-lt031mdz4000.dtbo vc4-kms-dsi-lt031mdz4000-overlay.dts

clean:
	rm *.dtbo
	make -C $(BUILD_DIR) M=$(PWD) clean

install:
	sudo cp -rf vc4-kms-dsi-lt031mdz4000.dtbo $(OVERLAYS_DIR)
	sudo cp -rf panel-jdi-lt031mdz4000.ko $(DRIVER_PATH)
	sudo depmod -A
