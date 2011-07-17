#Android makefile to build kernel as a part of Android Build

ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT)/.config
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage
#LGE_CHANGE_S, [jisung.yang@lge.com], 2010-04-24, <cp wireless.ko to system/lib/modules>
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules
#LGE_CHANGE_E, [jisung.yang@lge.com], 2010-04-24, <cp wireless.ko to system/lib/modules>
TARGET_PREBUILT_INT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage
ifeq ($(TARGET_USES_UNCOMPRESSED_KERNEL),true)
$(info Using uncompressed kernel)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/piggy
else
TARGET_PREBUILT_KERNEL := $(TARGET_PREBUILT_INT_KERNEL)
endif

# LGE_CHANGE [dojip.kim@lge.com] 2010-06-25, temporarily blocked for froyo
# LGE_CHANGE_S [kaisr.shin@lge.com] 2010.07.14 : Merge Sprint Extension ( Copy minigzip to kernel/zlib for compile )
HOST_BIN_OUT := $(TARGET_OUT)/../../../../host/linux-x86/bin
FSUA_BIN := $(TARGET_OUT)/../../../../../device/lge/cappuccino/packages/apps/omadm/fsua
FSUA_OUT := $(TARGET_OUT)/../root
# LGE_CHANGE_E [kaisr.shin@lge.com] 2010.05.12

$(KERNEL_OUT):
	mkdir -p $(KERNEL_OUT)
	mkdir -p $(HOST_BIN_OUT)
	cp -f kernel/scripts/minigzip $(HOST_BIN_OUT)
	
$(KERNEL_MODULES_OUT):
	mkdir -p $(KERNEL_MODULES_OUT)

$(KERNEL_CONFIG): $(KERNEL_OUT)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- $(KERNEL_DEFCONFIG)

$(KERNEL_OUT)/piggy : $(TARGET_PREBUILT_INT_KERNEL)
	$(hide) gunzip -c $(KERNEL_OUT)/arch/arm/boot/compressed/piggy > $(KERNEL_OUT)/piggy

$(TARGET_PREBUILT_INT_KERNEL): $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi-
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- modules
#LGE_CHANGE_S, [jisung.yang@lge.com], 2010-04-24, <cp wireless.ko to system/lib/modules>
	mkdir -p $(TARGET_OUT)/lib
	mkdir -p $(KERNEL_MODULES_OUT) 
	-cp  -f $(KERNEL_OUT)/drivers/net/wireless/bcm4325/wireless.ko $(KERNEL_MODULES_OUT)
#LGE_CHANGE_E, [jisung.yang@lge.com], 2010-04-24, <cp wireless.ko to system/lib/modules>
# LGE_CHANGE_S [kaisr.shin@lge.com] 2010.07.14 : Merge Sprint Extension
	mkdir -p $(FSUA_OUT)
	mkdir -p $(FSUA_OUT)/sbin
	mkdir -p $(FSUA_OUT)/res
	mkdir -p $(FSUA_OUT)/res/images
	cp -f $(FSUA_BIN)/hpfsfota $(FSUA_OUT)/sbin
	cp -f $(FSUA_BIN)/res/images/* $(FSUA_OUT)/res/images
# LGE_CHANGE_E [kaisr.shin@lge.com] 2010.07.14	

kerneltags: $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- tags

kernelconfig: $(KERNEL_OUT) $(KERNEL_CONFIG)
	env KCONFIG_NOTIMESTAMP=true \
	     $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- menuconfig
	cp $(KERNEL_OUT)/.config kernel/arch/arm/configs/$(KERNEL_DEFCONFIG)

endif
