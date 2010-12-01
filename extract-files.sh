#!/bin/bash

mkdir -p proprietary

#Radio
FILES="lib/liblgdrmwbxml.so lib/liblgdrmxyssl.so lib/libdll.so lib/libril-qcril-hook-oem.so lib/libgsdi_exp.so lib/libgstk_exp.so lib/libwms.so"
FILES="$FILES lib/libnv.so lib/libwmsts.so lib/liblgeat.so lib/libril_log.so lib/liblgerft.so lib/libbcmwl.so lib/liblgdrm.so lib/libwmdrmpd.so"
FILES="$FILES lib/liboem_rapi.so lib/libdss.so lib/libqmi.so lib/libmmgsdilib.so lib/libcm.so lib/liboncrpc.so lib/libdsm.so lib/libqueue.so"
FILES="$FILES lib/libdiag.so lib/libril-qc-1.so lib/libril.so"
#Wifi
FILES="$FILES etc/wl/rtecdc.bin etc/wl/nvram.txt"
#Bluetooth
FILES="$FILES bin/BCM4325D0.hcd"
#Compass
FILES="$FILES bin/akmd2"
#Camera
FILES="$FILES lib/libmm-qcamera-tgt.so lib/libmmcamera.so lib/libmmjpeg.so"
#Video
CY_FILES="system/lib/libmm-adspsvc.so system/lib/libOmxH264Dec.so system/lib/libOmxMpeg4Dec.so system/lib/libOmxVidEnc.so"
CY_FILES="$CY_FILES system/lib/libomx_wmadec_sharedlibrary.so system/lib/libomx_wmvdec_sharedlibrary.so"
CY_FILES="$CY_FILES system/lib/libpvasfcommon.so system/lib/libpvasflocalpbreg.so system/lib/libpvasflocalpb.so system/etc/pvasflocal.cfg"

#OpenGL
CY_FILES="$CY_FILES system/lib/egl/libGLES_qcom.so"

SRC="../../../../korean-v10t"
CY_SRC="../../../../cm_dream_sapphire_full-263.zip"
CY_URL="http://mirror.teamdouche.net/get/dream_sapphire/cm_dream_sapphire_full-263.zip"

if [[ ! -e $SRC ]]; then
  echo "ERROR: Could not find $SRC"
  exit 1
fi

for i in $FILES
do
  #if [[ -e $SRC ]]; then
    cp -a "$SRC/$i" proprietary/ || exit 1
  #else
  #  adb pull /system/$i proprietary/ || exit 1
  #fi
done

if [[ ! -e $CY_SRC ]]; then
  wget "$CY_URL" -O $CY_SRC
fi

unzip -o -j $CY_SRC $CY_FILES -d proprietary

chmod 755 proprietary/akmd2
