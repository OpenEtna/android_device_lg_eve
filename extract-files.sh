#!/bin/sh

mkdir -p proprietary

#Radio
FILES="lib/liblgdrmwbxml.so lib/liblgdrmxyssl.so lib/libdll.so lib/libril-qcril-hook-oem.so lib/libgsdi_exp.so lib/libgstk_exp.so lib/libwms.so"
FILES="$FILES lib/libnv.so lib/libwmsts.so lib/liblgeat.so lib/libril_log.so lib/liblgerft.so lib/libbcmwl.so lib/liblgdrm.so lib/libwmdrmpd.so"
FILES="$FILES lib/liboem_rapi.so lib/libdss.so lib/libqmi.so lib/libmmgsdilib.so lib/libcm.so lib/liboncrpc.so lib/libdsm.so lib/libqueue.so"
FILES="$FILES lib/libdiag.so lib/libril-qc-1.so lib/libril.so"
#Wifi
FILES="$FILES etc/wl/rtecdc.bin etc/wl/nvram.txt"
#Keyboard
FILES="$FILES usr/keychars/eve_qwerty.kcm.bin"
#GPS
FILES="$FILES lib/libcommondefs.so lib/libgps.so lib/libloc_api-rpc.so lib/libloc_api.so"
#Compass
FILES="$FILES bin/akmd2"
#Camera
FILES="$FILES lib/libmm-qcamera-tgt.so lib/libmmcamera.so lib/libmmjpeg.so"
#Video
FILES="$FILES lib/libmm-adspsvc.so lib/libOmxH264Dec.so lib/libOmxMpeg4Dec.so lib/libOmxVidEnc.so"
FILES="$FILES lib/libomx_wmadec_sharedlibrary.so lib/libomx_wmvdec_sharedlibrary.so"
#FILES="$FILES lib/libpvasfcommon.so lib/libpvasflocalpbreg.so lib/libpvasflocalpb.so"
#etc/pvasflocal.cfg

SRC="../../../../my-system/korean-v10t"

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
