#
# Product-specific compile-time definitions.
#

TARGET_BOARD_PLATFORM := msm7k
TARGET_BOARD_PLATFORM_GPU := qcom
TARGET_CPU_ABI := armeabi
TARGET_ARCH_VARIANT := armv6j

TARGET_NO_BOOTLOADER := true
TARGET_NO_RECOVERY := true

# Wifi related defines
BOARD_WPA_SUPPLICANT_DRIVER := WEXT
WPA_SUPPLICANT_VERSION      := VER_0_6_X
WIFI_DRIVER_MODULE_PATH     := "/system/lib/modules/wireless.ko"
WIFI_DRIVER_MODULE_ARG      := "firmware_path=/system/etc/wl/rtecdc.bin nvram_path=/system/etc/wl/nvram.txt config_path=/data/misc/wifi/config"
WIFI_DRIVER_MODULE_NAME     := "wireless"

#This would pull in the wrong wpa_supplicant.conf and dhcpcd.conf
#BOARD_WLAN_DEVICE           := bcm4329

BOARD_USES_GENERIC_AUDIO := false
BOARD_USE_HTC_LIBSENSORS := false
BOARD_USES_OLD_CAMERA_HACK := true
BOARD_USES_QCOM_LIBS := true
BOARD_GL_TEX_POW2_DIMENSION_REQUIRED := true
BOARD_NO_GL2 := true
BOARD_KERNEL_CMDLINE := no_console_suspend=1

BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true
BOARD_HAVE_FM_RADIO := true

TARGET_HARDWARE_3D := false

BOARD_GPS_LIBRARIES :=  libloc_api

USE_PV_WINDOWS_MEDIA := false

USE_CAMERA_STUB := false
BOARD_EGL_CFG := device/lg/eve/egl.cfg

