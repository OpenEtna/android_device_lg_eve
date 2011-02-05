LOCAL_PATH := $(call my-dir)

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

file := $(TARGET_ROOT_OUT)/ueventd.eve.rc
$(file) : $(LOCAL_PATH)/prebuilt/ueventd.eve.rc | $(ACP)
	$(transform-prebuilt-to-target)
ALL_PREBUILT += $(file)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := eve_qwerty.kcm
include $(BUILD_KEY_CHAR_MAP)

include $(CLEAR_VARS)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE := vold.fstab
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

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

ifeq ($(BOARD_CAMERA_BUILD_FROM_SOURCE),false)
include $(CLEAR_VARS)
LOCAL_MODULE := libcamera.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE)
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := libril.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := proprietary/$(LOCAL_MODULE) $(TARGET_OUT_INTERMEDIATE_LIBRARIES)/libhardware_legacy.so
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

#$(LOCAL_PATH)/proprietary/libGLES_qcom.so:system/lib/egl/libGLES_qcom.so
PRODUCT_COPY_FILES += \
    frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/base/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/base/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/base/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/base/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/base/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
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
