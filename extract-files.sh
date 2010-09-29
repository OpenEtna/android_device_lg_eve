#!/bin/sh

mkdir -p proprietary

#GPS
FILES="lib/libcommondefs.so lib/libgps.so lib/libloc_api-rpc.so lib/libloc_api.so"
#Compass
FILES="$FILES bin/akmd2"
#Camera
FILES="lib/libmm-qcamera-tgt.so lib/libmmcamera.so lib/libmmjpeg.so"
#Video
FILES="$FILES lib/libmm-adspsvc.so lib/libOmxH264Dec.so lib/libOmxMpeg4Dec.so lib/libOmxVidEnc.so lib/libspeech.so"
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
