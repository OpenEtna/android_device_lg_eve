LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_KERNEL_CONFIG),)
ifeq ($(TARGET_PREBUILT_KERNEL),)
TARGET_PREBUILT_KERNEL := $(LOCAL_PATH)/kernel
endif # TARGET_PREBUILT_KERNEL
endif # TARGET_KERNEL_CONFIG

file := $(INSTALLED_KERNEL_TARGET)
ALL_PREBUILT += $(file)
$(file): $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT)/lib/libhtc_ril.so
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/proprietary/libhtc_ril.so | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT)/lib/libhtc_acoustic.so
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/proprietary/libhtc_acoustic.so | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT)/lib/libaudioeq.so
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/proprietary/libaudioeq.so | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT_KEYLAYOUT)/h2w_headset.kl
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/h2w_headset.kl | $(ACP)
#	$(transform-prebuilt-to-target)
    
#file := $(TARGET_OUT_KEYLAYOUT)/trout-keypad.kl
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/trout-keypad.kl | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT_KEYLAYOUT)/trout-keypad-v2.kl
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/trout-keypad-v2.kl | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT_KEYLAYOUT)/trout-keypad-v3.kl
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/trout-keypad-v3.kl | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT_KEYLAYOUT)/trout-keypad-qwertz.kl
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/trout-keypad-qwertz.kl | $(ACP)
#	$(transform-prebuilt-to-target)

file := $(TARGET_ROOT_OUT)/init.eve.rc
$(file) : $(LOCAL_PATH)/prebuilt/init.eve.rc | $(ACP)
	$(transform-prebuilt-to-target)
ALL_PREBUILT += $(file)

file := $(TARGET_ROOT_OUT_SBIN)/choosesystem
$(file) : $(LOCAL_PATH)/prebuilt/choosesystem | $(ACP)
	$(transform-prebuilt-to-target)
ALL_PREBUILT += $(file)

file := $(TARGET_ROOT_OUT)/initlogo.rle
$(file) : $(LOCAL_PATH)/prebuilt/initlogo.rle | $(ACP)
	$(transform-prebuilt-to-target)
ALL_PREBUILT += $(file)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := eve_qwerty.kcm
include $(BUILD_KEY_CHAR_MAP)

#include $(CLEAR_VARS)
#LOCAL_SRC_FILES := trout-keypad-v2.kcm
#include $(BUILD_KEY_CHAR_MAP)

#include $(CLEAR_VARS)
#LOCAL_SRC_FILES := trout-keypad-v3.kcm
#include $(BUILD_KEY_CHAR_MAP)

#include $(CLEAR_VARS)
#LOCAL_SRC_FILES := trout-keypad-qwertz.kcm
#include $(BUILD_KEY_CHAR_MAP)

#file := $(TARGET_OUT)/build.trout.prop
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/build.trout.prop | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT_KEYLAYOUT)/sapphire-keypad.kl
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/sapphire-keypad.kl | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_ROOT_OUT)/init.sapphire.rc
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/init.sapphire.rc | $(ACP)
#	$(transform-prebuilt-to-target)

#include $(CLEAR_VARS)
#LOCAL_SRC_FILES := sapphire-keypad.kcm
#include $(BUILD_KEY_CHAR_MAP)

#file := $(TARGET_OUT)/build.sapphire.prop
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/build.sapphire.prop | $(ACP)
#	$(transform-prebuilt-to-target)

#file := $(TARGET_OUT)/etc/firmware/brf6300.bin
#ALL_PREBUILT += $(file)
#$(file) : $(LOCAL_PATH)/proprietary/brf6300.bin | $(ACP)
#	$(transform-prebuilt-to-target)


# This will install the file in /system/etc
#
#include $(CLEAR_VARS)
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE := AudioFilter.csv
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#include $(BUILD_PREBUILT)

#include $(CLEAR_VARS)
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE := AudioPreProcess.csv
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#include $(BUILD_PREBUILT)

# Installer will setup a symlink based on board type if set
#
#ifeq ($(WITH_HTCACOUSTIC_HACK),)
#include $(CLEAR_VARS)
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE := AudioPara4.csv
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#include $(BUILD_PREBUILT)
#else
#include $(CLEAR_VARS)
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE := AudioPara_sapphire.csv
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#include $(BUILD_PREBUILT)

#include $(CLEAR_VARS)
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE := AudioPara_dream.csv
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#include $(BUILD_PREBUILT)
#endif # WITH_HTCACOUSTIC_HACK

include $(CLEAR_VARS)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE := vold.fstab
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

# WiFi driver and firmware
#local_target_dir := $(TARGET_OUT_ETC)/firmware

#include $(CLEAR_VARS)
#LOCAL_MODULE := Fw1251r1c.bin
#LOCAL_MODULE_TAGS := user
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE_PATH := $(local_target_dir)
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#include $(BUILD_PREBUILT)

#include $(CLEAR_VARS)
#LOCAL_MODULE := wireless.ko
#LOCAL_MODULE_TAGS := user
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE_PATH := $(TARGET_OUT)/lib/modules
#LOCAL_SRC_FILES := $(LOCAL_MODULE)
#include $(BUILD_PREBUILT)

#include $(CLEAR_VARS)
#LOCAL_MODULE := libloc_api.so
#LOCAL_MODULE_CLASS := SHARED_LIBRARIES
#LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
#include $(BUILD_PREBUILT)

#include $(CLEAR_VARS)
#LOCAL_MODULE := libloc_api-rpc.so
#LOCAL_MODULE_CLASS := SHARED_LIBRARIES
#LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
#include $(BUILD_PREBUILT)

#include $(CLEAR_VARS)
#LOCAL_MODULE := libcommondefs.so
#LOCAL_MODULE_CLASS := SHARED_LIBRARIES
#LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
#LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
#OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
#include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libmm-adspsvc.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libOmxH264Dec.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libOmxMpeg4Dec.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libOmxVidEnc.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcommondefs.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libloc-rpc.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE) $(TARGET_OUT_INTERMEDIATE_LIBRARIES)/libcommondefs.so
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libloc.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
#Only the first file is copied, the other are deps
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE) $(TARGET_OUT_INTERMEDIATE_LIBRARIES)/libloc-rpc.so
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcamera.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libril.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE) $(TARGET_OUT_INTERMEDIATE_LIBRARIES)/libhardware_legacy.so
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

#$(LOCAL_PATH)/proprietary/libGLES_qcom.so:system/lib/egl/libGLES_qcom.so
PRODUCT_COPY_FILES += \
    frameworks/base/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml \
    frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/base/data/etc/android.hardware.touchscreen.multitouch.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.xml \
	packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:/system/etc/permissions/android.software.live_wallpaper.xml \
    device/lg/eve/prebuilt/15checkgapps:system/etc/init.d/15checkgapps \
	device/lg/eve/prebuilt/eve_qwerty-qwertz.kl:system/usr/keylayout/eve_qwerty-qwertz.kl \
	device/lg/eve/prebuilt/eve_qwerty-azerty.kl:system/usr/keylayout/eve_qwerty-azerty.kl \
	device/lg/eve/prebuilt/eve_qwerty.kl:system/usr/keylayout/eve_qwerty.kl \
	device/lg/eve/prebuilt/qwerty.kl:system/usr/keylayout/qwerty.kl \
    device/lg/eve/prebuilt/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
	device/lg/eve/prebuilt/dhcpcd.conf:system/etc/dhcpcd/dhcpcd.conf \
	device/lg/eve/prebuilt/gps.conf:system/etc/gps.conf

#Radio
PROPRIETARY := lib/liblgdrmwbxml.so lib/liblgdrmxyssl.so lib/libdll.so lib/libril-qcril-hook-oem.so lib/libgsdi_exp.so lib/libgstk_exp.so lib/libwms.so \
               lib/libnv.so lib/libwmsts.so lib/liblgeat.so lib/libril_log.so lib/liblgerft.so lib/libbcmwl.so lib/liblgdrm.so lib/libwmdrmpd.so \
               lib/liboem_rapi.so lib/libdss.so lib/libqmi.so lib/libmmgsdilib.so lib/libcm.so lib/liboncrpc.so lib/libdsm.so lib/libqueue.so \
			   lib/libril-qc-1.so lib/libdiag.so bin/qmuxd
#Wifi
PROPRIETARY += etc/wl/rtecdc.bin etc/wl/nvram.txt

#Bluetooth
PROPRIETARY += bin/BCM4325D0_004.001.007.0168.0169.hcd bin/btld

#OpenGL
PROPRIETARY += lib/egl/libGLES_qcom.so

#Video
PROPRIETARY += lib/libOmxWmvDec.so \
			   lib/libomx_aacdec_sharedlibrary.so lib/libomx_amrdec_sharedlibrary.so lib/libomx_amrenc_sharedlibrary.so lib/libomx_avcdec_sharedlibrary.so \
			   lib/libomx_m4vdec_sharedlibrary.so lib/libomx_mp3dec_sharedlibrary.so \
			   lib/libaomx_mp3dec_sharedlibrary.so lib/libaomx_mp4dec_sharedlibrary.so lib/libaomx_wmadec_sharedlibrary.so lib/libaomx_wmvdec_sharedlibrary.so

#Sensors
PROPRIETARY += bin/akmd2

#Camera
PROPRIETARY += lib/libmm-qcamera-tgt.so lib/libmmjpeg.so

#GPS
PROPRIETARY += lib/libloc_api.so lib/libloc_ext.so lib/libgps.so

PRODUCT_COPY_FILES += $(foreach i,$(PROPRIETARY),$(LOCAL_PATH)/proprietary/$(notdir $i):system/$i)

#PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/proprietary/pvasflocal.cfg:system/etc/pvasflocal.cfg \
    $(LOCAL_PATH)/proprietary/libomx_wmadec_sharedlibrary.so:system/lib/libomx_wmadec_sharedlibrary.so\
    $(LOCAL_PATH)/proprietary/libomx_wmvdec_sharedlibrary.so:system/lib/libomx_wmvdec_sharedlibrary.so\
    $(LOCAL_PATH)/proprietary/libpvasfcommon.so:system/lib/libpvasfcommon.so \
    $(LOCAL_PATH)/proprietary/libpvasflocalpbreg.so:system/lib/libpvasflocalpbreg.so \
    $(LOCAL_PATH)/proprietary/libpvasflocalpb.so:system/lib/libpvasflocalpb.so
