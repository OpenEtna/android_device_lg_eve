#!/bin/bash

rm -Rf proprietary
mkdir -p proprietary

#Radio
FILES="lib/liblgdrmwbxml.so lib/liblgdrmxyssl.so lib/libdll.so lib/libril-qcril-hook-oem.so lib/libgsdi_exp.so lib/libgstk_exp.so lib/libwms.so"
FILES="$FILES lib/libnv.so lib/libwmsts.so lib/liblgeat.so lib/libril_log.so lib/liblgerft.so lib/libbcmwl.so lib/liblgdrm.so lib/libwmdrmpd.so"
FILES="$FILES lib/liboem_rapi.so lib/libmmgsdilib.so lib/libcm.so lib/liboncrpc.so lib/libdsm.so lib/libqueue.so"
FILES="$FILES lib/libdiag.so lib/libril-qc-1.so lib/libril.so"
FILES="$FILES lib/libdss.so lib/libqmi.so bin/qmuxd"

#Wifi
FILES="$FILES etc/wl/rtecdc.bin etc/wl/nvram.txt"

#Bluetooth
FILES="$FILES bin/BCM4325D0_004.001.007.0168.0169.hcd bin/btld"

#Camera
FILES="$FILES lib/libmm-qcamera-tgt.so lib/libmmjpeg.so lib/libcamera.so"

#Video
FILES="$FILES lib/libmm-adspsvc.so lib/libOmxH264Dec.so lib/libOmxMpeg4Dec.so lib/libOmxVidEnc.so lib/libOmxWmvDec.so"
FILES="$FILES lib/libomx_aacdec_sharedlibrary.so lib/libomx_amrdec_sharedlibrary.so lib/libomx_amrenc_sharedlibrary.so lib/libomx_avcdec_sharedlibrary.so"
FILES="$FILES lib/libomx_m4vdec_sharedlibrary.so lib/libomx_mp3dec_sharedlibrary.so"
FILES="$FILES lib/libaomx_mp3dec_sharedlibrary.so lib/libaomx_mp4dec_sharedlibrary.so lib/libaomx_wmadec_sharedlibrary.so lib/libaomx_wmvdec_sharedlibrary.so"

#Sensors
FILES="$FILES bin/akmd2"

#OpenGL
FILES="$FILES lib/egl/libGLES_qcom.so"

#GPS
FILES="$FILES lib/libloc.so lib/libloc-rpc.so lib/libcommondefs.so lib/libloc_api.so lib/libloc_ext.so lib/libgps.so"

SRC="../../../../lg2.2/system"

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

chmod 755 proprietary/akmd2
