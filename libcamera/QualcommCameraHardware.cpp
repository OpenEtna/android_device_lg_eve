/*
 ** Copyright 2008, Google Inc.
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "QualcommCameraHardware"
#define CONFIG_SENSOR_M4MO
#include <utils/Log.h>

#include "QualcommCameraHardware.h"

#include <utils/threads.h>
#include <binder/MemoryHeapPmem.h>
#include <utils/String16.h>
#include <sys/types.h>
#include <sys/stat.h>


#include <unistd.h>
#if HAVE_ANDROID_OS
#include <linux/android_pmem.h>
#endif
#include <linux/ioctl.h>

#define LIKELY(exp)   __builtin_expect(!!(exp), 1)
#define UNLIKELY(exp) __builtin_expect(!!(exp), 0)

//extern "C" {

#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdlib.h>

#define THUMBNAIL_WIDTH        192 //512
#define THUMBNAIL_HEIGHT       144 //384
#define THUMBNAIL_WIDTH_STR    "192" //"512"
#define THUMBNAIL_HEIGHT_STR  "144" //"384"
#define DEFAULT_PICTURE_WIDTH  1280 // 1280
#define DEFAULT_PICTURE_HEIGHT 960 // 768
#define THUMBNAIL_BUFFER_SIZE (THUMBNAIL_WIDTH * THUMBNAIL_HEIGHT * 3/2)

#define DEFAULT_PREVIEW_SETTING 3 // HVGA
#define PREVIEW_SIZE_COUNT (sizeof(preview_sizes)/sizeof(preview_size_type))

#define NOT_FOUND -1

#if DLOPEN_LIBMMCAMERA
#include <dlfcn.h>

void* (*LINK_cam_conf)(void *data);
void* (*LINK_cam_frame)(void *data);
bool  (*LINK_jpeg_encoder_init)();
void  (*LINK_jpeg_encoder_join)();
/*bool  (*LINK_jpeg_encoder_encode)(const cam_ctrl_dimension_t *dimen,
  const uint8_t *thumbnailbuf, int thumbnailfd,
  const uint8_t *snapshotbuf, int snapshotfd,
  common_crop_t *scaling_parms);
  */
unsigned char (*LINK_jpeg_encoder_encode)(const char* file_name, const cam_ctrl_dimension_t *dimen,
		const unsigned char* thumbnailbuf, int thumbnailfd,
		const unsigned char* snapshotbuf, int snapshotfd, common_crop_t *cropInfo) ;

int  (*LINK_camframe_terminate)(void);
int8_t (*LINK_jpeg_encoder_setMainImageQuality)(uint32_t quality);
int8_t (*LINK_jpeg_encoder_setThumbnailQuality)(uint32_t quality);
int8_t (*LINK_jpeg_encoder_setRotation)(uint32_t rotation);
int8_t (*LINK_jpeg_encoder_setLocation)(const camera_position_type *location);
// callbacks
void  (**LINK_mmcamera_camframe_callback)(struct msm_frame_t *frame);
void  (**LINK_mmcamera_jpegfragment_callback)(uint8_t *buff_ptr,
		uint32_t buff_size);
void  (**LINK_mmcamera_jpeg_callback)(jpeg_event_t status);
#else
#define LINK_cam_conf cam_conf
#define LINK_cam_frame cam_frame
#define LINK_jpeg_encoder_init jpeg_encoder_init
#define LINK_jpeg_encoder_join jpeg_encoder_join
#define LINK_jpeg_encoder_encode jpeg_encoder_encode
#define LINK_camframe_terminate camframe_terminate
#define LINK_jpeg_encoder_setMainImageQuality jpeg_encoder_setMainImageQuality
#define LINK_jpeg_encoder_setThumbnailQuality jpeg_encoder_setThumbnailQuality
#define LINK_jpeg_encoder_setRotation jpeg_encoder_setRotation
#define LINK_jpeg_encoder_setLocation jpeg_encoder_setLocation
extern void (*mmcamera_camframe_callback)(struct msm_frame_t *frame);
extern void (*mmcamera_jpegfragment_callback)(uint8_t *buff_ptr,
		uint32_t buff_size);
extern void (*mmcamera_jpeg_callback)(jpeg_event_t status);
#endif

//} // extern "C"




struct preview_size_type {
	int width;
	int height;
};

static preview_size_type preview_sizes[] = {
	{ 800, 480 }, // WVGA
	{ 640, 480 }, // VGA
	{ 480, 320 }, // HVGA
	{ 432, 320 },
	{ 384, 288 },
	{ 352, 288 }, // CIF
	{ 320, 240 }, // QVGA
	{ 240, 160 }, // SQVGA
	{ 176, 144 }, // QCIF
};

static int attr_lookup(const struct str_map *const arr, const char *name)
{
	if (name) {
		const struct str_map *trav = arr;
		while (trav->desc) {
			if (!strcmp(trav->desc, name))
				return trav->val;
			trav++;
		}
	}
	return NOT_FOUND;
}

#define INIT_VALUES_FOR(parm)                                    \
	static char* parm##_values = 0;                              \
if (!parm##_values) {                                        \
	parm##_values = (char *)malloc(sizeof(parm)/             \
			sizeof(parm[0])*30);      \
	char *ptr = parm##_values;                               \
	const str_map *trav;                                     \
	for (trav = parm; trav->desc; trav++) {                  \
		int len = strlen(trav->desc);                        \
		strcpy(ptr, trav->desc);                             \
		ptr += len;                                          \
		*ptr++ = ',';                                        \
	}                                                        \
	*--ptr = 0;                                              \
}                                                            \

namespace android {

	static const str_map whitebalance[] = {
		{ CameraParameters::WHITE_BALANCE_AUTO,         CAMERA_WB_AUTO },
		{ CameraParameters::WHITE_BALANCE_INCANDESCENT, CAMERA_WB_INCANDESCENT },
		{ CameraParameters::WHITE_BALANCE_FLUORESCENT,   CAMERA_WB_FLUORESCENT },
		{ CameraParameters::WHITE_BALANCE_DAYLIGHT,     CAMERA_WB_DAYLIGHT },
		{ CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT,       CAMERA_WB_CLOUDY_DAYLIGHT },
		{ NULL, 0 }
	};

	/* camera also has:
	   CAMERA_EFFECT_SKETCH,
	   CAMERA_EFFECT_EMBOSS,
	   CAMERA_EFFECT_RED,
	   CAMERA_EFFECT_GREEN,
	   android also knows:
	   static const char EFFECT_POSTERIZE[];
	   static const char EFFECT_WHITEBOARD[];
	   static const char EFFECT_BLACKBOARD[];
	   */
	static const str_map effect[] = {
		{ CameraParameters::EFFECT_NONE,       CAMERA_EFFECT_OFF },  /* This list must match aeecamera.h */
		{ CameraParameters::EFFECT_MONO,       CAMERA_EFFECT_MONO },
		{ CameraParameters::EFFECT_NEGATIVE,   CAMERA_EFFECT_NEGATIVE },
		{ CameraParameters::EFFECT_SOLARIZE,   CAMERA_EFFECT_SOLARIZE },
		{ CameraParameters::EFFECT_SEPIA,      CAMERA_EFFECT_SEPIA },
		{ CameraParameters::EFFECT_AQUA,       CAMERA_EFFECT_AQUA },
		{ NULL, 0 }
	};

	// from qcamera/common/camera.h
	static const str_map antibanding[] = {
		{ CameraParameters::ANTIBANDING_50HZ,  50 },
		{ CameraParameters::ANTIBANDING_60HZ, 60},
		{ NULL, 0 }
	};

	static const str_map auto_focus[] = {
		{ CameraParameters::FOCUS_MODE_AUTO, 0 /* normal focus */ },
		{ CameraParameters::FOCUS_MODE_MACRO, 1 /* macro */},
		{ NULL, 0}
	};
	/*
Missing: android knows:
static const char SCENE_MODE_ACTION[];
static const char SCENE_MODE_THEATRE[];
static const char SCENE_MODE_SNOW[];
static const char SCENE_MODE_SUNSET[];
static const char SCENE_MODE_STEADYPHOTO[];
static const char SCENE_MODE_FIREWORKS[];
static const char SCENE_MODE_CANDLELIGHT[];

our camera supports:
CAMERA_SCENE_BACKLIGHT,

*/
	static const str_map scene_modes[] = {
		{ CameraParameters::SCENE_MODE_AUTO, CAMERA_SCENE_NORMAL},
		{ CameraParameters::SCENE_MODE_NIGHT, CAMERA_SCENE_NIGHT },
		{ CameraParameters::SCENE_MODE_LANDSCAPE, CAMERA_SCENE_LANDSCAPE },
		{ CameraParameters::SCENE_MODE_PORTRAIT, CAMERA_SCENE_PORTRAIT },
		{ CameraParameters::SCENE_MODE_NIGHT_PORTRAIT, CAMERA_SCENE_NIGHT_PORTRAIT },
		{ CameraParameters::SCENE_MODE_BEACH, CAMERA_SCENE_BEACH },
		{ CameraParameters::SCENE_MODE_PARTY, CAMERA_SCENE_PARTY },
		{ CameraParameters::SCENE_MODE_SPORTS, CAMERA_SCENE_SPORT },
		{ NULL, 0}
	};

	static const str_map flash_mode[] = {
		{ CameraParameters::FLASH_MODE_AUTO, CAMERA_FLASH_AUTO },
		{ CameraParameters::FLASH_MODE_ON, CAMERA_FLASH_ON },
		{ CameraParameters::FLASH_MODE_OFF, CAMERA_FLASH_OFF },
		{ CameraParameters::FLASH_MODE_RED_EYE, CAMERA_FLASH_REDEYE_REMOVAL },
		{ NULL, 0}
	};

	// round to the next power of two
	static inline unsigned clp2(unsigned x)
	{
		x = x - 1;
		x = x | (x >> 1);
		x = x | (x >> 2);
		x = x | (x >> 4);
		x = x | (x >> 8);
		x = x | (x >>16);
		return x + 1;
	}

	static void dump_to_file(const char *fname,
			uint8_t *buf, uint32_t size) ;


	static Mutex singleton_lock;
	static bool singleton_releasing;
	static Condition singleton_wait;

	static void receive_camframe_callback(struct msm_frame_t *frame);
	static void receive_jpeg_fragment_callback(uint8_t *buff_ptr, uint32_t buff_size);
	static void receive_jpeg_callback(jpeg_event_t status);
	//static void receive_shutter_callback();

	int cam_conf_sync[2];

	static unsigned char *hal_mmap (uint32_t size, int *pmemFd)
	{
		void     *ret; /* returned virtual address */
		int    pmem_fd   = 0;

		pmem_fd = open("/dev/pmem_adsp", O_RDWR);

		if (pmem_fd < 0) {
			LOGI("do_mmap: Open device /dev/pmem_adsp failed!\n");
			return NULL;
		}

		/* to make it page size aligned */
		size = (size + 4095) & (~4095);
		size=clp2(size);
		LOGV("do_mmap: pmem mmap size:%ld\n",size);

		ret = mmap(NULL,
				size,
				PROT_READ      | PROT_WRITE,
				MAP_SHARED,
				pmem_fd,
				0);

		if (ret == MAP_FAILED) {
			LOGI("do_mmap: pmem mmap() failed: %s (%d)\n", strerror(errno), errno);
			return NULL;
		}

		*pmemFd = pmem_fd;
		return (unsigned char *)ret;
	}

	static int hal_munmap (int pmem_fd, void *addr, size_t size)
	{
		int rc;

		size = (size + 4095) & (~4095);
		size = clp2(size);
		LOGV("munmapped size = %d, virt_addr = 0x%x\n",
				size, (uint32_t)addr);

		rc = (munmap(addr, size));

		close(pmem_fd);
		pmem_fd = -1;

		return rc;
	}

	QualcommCameraHardware::QualcommCameraHardware()
		: mParameters(),
		mPreviewHeight(-1),
		mPreviewWidth(-1),
		mRawHeight(-1),
		mRawWidth(-1),
		mCameraRunning(false),
		mPreviewInitialized(false),
		mRawInitialized(false),
		mFrameThreadRunning(false),
		mSnapshotThreadRunning(false),
		mReleasedRecordingFrame(false),
		mNotifyCb(0),
		mDataCb(0),
		mDataCbTimestamp(0),
		mCallbackCookie(0),
		mMsgEnabled(0),
		mPreviewFrameSize(0),
		mRawSize(0),
		mCameraControlFd(-1),
		mAutoFocusThreadRunning(false),
		mInPreviewCallback(false),
		mCameraRecording(false)
	{
		memset(&mDimension, 0, sizeof(mDimension));
		memset(&mCrop, 0, sizeof(mCrop));
		LOGD("constructor EX");
	}

	void QualcommCameraHardware::initDefaultParameters()
	{
		CameraParameters p;

		LOGD("initDefaultParameters E");

		preview_size_type *ps = &preview_sizes[DEFAULT_PREVIEW_SETTING];
		p.setPreviewSize(ps->width, ps->height);
		p.setPreviewFrameRate(30);
		p.setPreviewFormat("yuv420sp"); // informative
		p.setPictureFormat("jpeg"); // informative

		p.set("jpeg-quality", "100"); // maximum quality
		p.set("jpeg-thumbnail-width", THUMBNAIL_WIDTH_STR); // informative
		p.set("jpeg-thumbnail-height", THUMBNAIL_HEIGHT_STR); // informative
		p.set("jpeg-thumbnail-quality", "85");

		p.setPictureSize(DEFAULT_PICTURE_WIDTH, DEFAULT_PICTURE_HEIGHT);

#if 0
		p.set("gps-timestamp", "1199145600"); // Jan 1, 2008, 00:00:00
		p.set("gps-latitude", "37.736071"); // A little house in San Francisco
		p.set("gps-longitude", "-122.441983");
		p.set("gps-altitude", "21"); // meters
#endif

		// This will happen only one in the lifetime of the mediaserver process.
		// We do not free the *_values arrays when we destroy the camera object.
		INIT_VALUES_FOR(scene_modes);
		p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, scene_modes_values);
		p.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_AUTO);

		INIT_VALUES_FOR(antibanding);
		p.set("antibanding-values", antibanding_values);
		p.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_50HZ);

		INIT_VALUES_FOR(effect);
		p.set("effect-values", effect_values);
		p.set("effect", "none");

		INIT_VALUES_FOR(whitebalance);
		p.set("whitebalance-values", whitebalance_values);
		p.set("whitebalance", "auto");

		INIT_VALUES_FOR(auto_focus);
		p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, auto_focus_values);
		p.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_AUTO);

		INIT_VALUES_FOR(flash_mode);
		p.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, flash_mode_values);
		p.set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_AUTO);

		p.set("picture-size-values", "2560x1920,2048x1536,1600x1200,1024x768");
		p.set("preview-size-values", "432x320");

		if (setParameters(p) != NO_ERROR) {
			LOGE("Failed to set default parameters?!");
		}

		LOGD("initDefaultParameters X");
	}

	void QualcommCameraHardware::setCallbacks(notify_callback notify_cb,
			data_callback data_cb,
			data_callback_timestamp data_cb_timestamp,
			void* user)
	{
		Mutex::Autolock lock(mLock);
		mNotifyCb = notify_cb;
		mDataCb = data_cb;
		mDataCbTimestamp = data_cb_timestamp;
		mCallbackCookie = user;
	}

	void QualcommCameraHardware::enableMsgType(int32_t msgType)
	{
		Mutex::Autolock lock(mLock);
		LOGD("enableMsgType( %d )", msgType ) ;
		mMsgEnabled |= msgType;
	}

	void QualcommCameraHardware::disableMsgType(int32_t msgType)
	{
		Mutex::Autolock lock(mLock);
		mMsgEnabled &= ~msgType;
	}

	bool QualcommCameraHardware::msgTypeEnabled(int32_t msgType)
	{
		Mutex::Autolock lock(mLock);
		return (mMsgEnabled & msgType);
	}

#ifdef USE_GETBUFFERINFO
status_t QualcommCameraHardware::getBufferInfo(sp<IMemory>& Frame, size_t *alignedSize) {
    /* No Support for this API in STUB Camera. Just return NULL */
    Frame = NULL;
    if( alignedSize != NULL)
        *alignedSize = 0;

    return UNKNOWN_ERROR;
}
#endif


#define ROUND_TO_PAGE(x)  (((x)+0xfff)&~0xfff)

	void QualcommCameraHardware::startCamera()
	{
		unsigned char sync_value;
		LOGD("startCamera E");
		libmmcamera_target = ::dlopen("libmm-qcamera-tgt.so", RTLD_NOW) ;
		LOGD("loading libmm-qcamera-tgt.so %p", libmmcamera_target) ;
		if (!libmmcamera_target) {
			LOGE("FATAL ERROR: could not dlopen libmm-qcamera-tgt.so: %s", dlerror());
			return;
		}


		*(void **)&LINK_cam_frame =
			::dlsym(libmmcamera_target, "cam_frame");
		*(void **)&LINK_camframe_terminate =
			::dlsym(libmmcamera_target, "camframe_terminate");

		*(void **)&LINK_jpeg_encoder_init =
			::dlsym(libmmcamera_target, "jpeg_encoder_init");

		*(void **)&LINK_jpeg_encoder_encode =
			::dlsym(libmmcamera_target, "jpeg_encoder_encode");

		*(void **)&LINK_jpeg_encoder_join =
			::dlsym(libmmcamera_target, "jpeg_encoder_join");

		*(void **)&LINK_mmcamera_camframe_callback =
			::dlsym(libmmcamera_target, "camframe_callback");

		*LINK_mmcamera_camframe_callback = receive_camframe_callback;

		*(void **)&LINK_mmcamera_jpegfragment_callback =
			::dlsym(libmmcamera_target, "jpegfragment_callback");

		*LINK_mmcamera_jpegfragment_callback = receive_jpeg_fragment_callback;

		*(void **)&LINK_mmcamera_jpeg_callback =
			::dlsym(libmmcamera_target, "jpeg_callback");

		*LINK_mmcamera_jpeg_callback = receive_jpeg_callback;

		*(void**)&LINK_jpeg_encoder_setMainImageQuality =
			::dlsym(libmmcamera_target, "jpeg_encoder_setMainImageQuality");

		*(void**)&LINK_jpeg_encoder_setThumbnailQuality =
			::dlsym(libmmcamera_target, "jpeg_encoder_setThumbnailQuality");

		*(void**)&LINK_jpeg_encoder_setRotation =
			::dlsym(libmmcamera_target, "jpeg_encoder_setRotation");

		*(void**)&LINK_jpeg_encoder_setLocation =
			::dlsym(libmmcamera_target, "jpeg_encoder_setLocation");

		*(void **)&LINK_cam_conf =
			::dlsym(libmmcamera_target, "cam_conf");

		/* The control thread is in libcamera itself. */
		mCameraControlFd = open(MSM_CAMERA_CONTROL, O_RDWR);
		LOGD ("mCameraControlFd == %d", mCameraControlFd ) ;

		LOGD("before LINK_jpeg_encoder_init") ;
		if (!LINK_jpeg_encoder_init()) {
			LOGE("jpeg_encoding_init failed.\n") ;
		}
		LOGD("LINK_jpeg_encoder_init done") ;


		if (pipe(cam_conf_sync) < 0) {
			LOGE("cam_conf_sync pipe create failed");
			return;
		}
		LOGD("pipe done") ;


		if (mCameraControlFd < 0) {
			LOGE("startCamera X: %s open failed: %s!",
					MSM_CAMERA_CONTROL,
					strerror(errno));
			//         return;d
		}


		LOGD("before cam_conf thread") ;
		if( (pthread_create(&mCamConfigThread, NULL,
						LINK_cam_conf, (void*)&(cam_conf_sync[1]) )) != 0 ) { //(void*)&(cam_conf_sync[1])
							LOGE("Config thread creation failed !") ;
						}


		usleep(500*1000); // use sleep value found in old qualcomm code

		//    m4mo_get_firmware_version() ;

		/*close(cam_conf_sync[1]);

		  if (read(cam_conf_sync[0], &sync_value, sizeof(sync_value)) < 0) {
		  LOGE("thread sync failed");
		  close(cam_conf_sync[0]);
		  return ;
		  }
		  close(cam_conf_sync[0]);
		  close(cam_conf_sync[1]);
		  if (sync_value) {
		  LOGE("error : sync_value is true") ;
		  return;
		  }*/

		LOGD("startCamera X");
	}

	status_t QualcommCameraHardware::dump(int fd,
			const Vector<String16>& args) const
	{
		const size_t SIZE = 256;
		char buffer[SIZE];
		String8 result;

		// Dump internal primitives.
		result.append("QualcommCameraHardware::dump");
		snprintf(buffer, 255, "preview width(%d) x height (%d)\n",
				mPreviewWidth, mPreviewHeight);
		result.append(buffer);
		snprintf(buffer, 255, "raw width(%d) x height (%d)\n",
				mRawWidth, mRawHeight);
		result.append(buffer);
		snprintf(buffer, 255,
				"preview frame size(%d), raw size (%d), jpeg size (%d) "
				"and jpeg max size (%d)\n", mPreviewFrameSize, mRawSize,
				mJpegSize, mJpegMaxSize);
		result.append(buffer);
		write(fd, result.string(), result.size());

		// Dump internal objects.
		if (mPreviewHeap != 0) {
			mPreviewHeap->dump(fd, args);
		}
		if (mRawHeap != 0) {
			mRawHeap->dump(fd, args);
		}
		if (mJpegHeap != 0) {
			mJpegHeap->dump(fd, args);
		}
		mParameters.dump(fd, args);
		return NO_ERROR;
	}

	static bool native_start_preview(int camfd)
	{
		int ioctlRetVal = 1 ;

		struct msm_ctrl_cmd_t ctrlCmd;

		ctrlCmd.timeout_ms = 5000;
		ctrlCmd.type       = CAMERA_START_PREVIEW;
		ctrlCmd.length     = 0;
		ctrlCmd.value = NULL ;
		//ctrlCmd.resp_fd    = camfd; // FIXME: this will be put in by the kernel

		ioctlRetVal = ioctl(camfd, MSM_CAM_IOCTL_CTRL_COMMAND, &ctrlCmd) ;
		if ( ioctlRetVal < 0) {
			LOGE("native_start_preview: MSM_CAM_IOCTL_CTRL_COMMAND fd %d error %s",
					camfd,
					strerror(errno));
			LOGE("ioctlRetVal %d", ioctlRetVal ) ;
			return false;
		}
		LOGD("native_start_preview status after ioctl == %d" ,ctrlCmd.status ) ;

		return true;
	}

	static bool native_get_picture (int camfd, common_crop_t *crop)
	{
		struct msm_ctrl_cmd_t ctrlCmd;

		ctrlCmd.timeout_ms = 5000;
		ctrlCmd.length     = sizeof(common_crop_t);
		ctrlCmd.value      = crop;

		if(ioctl(camfd, MSM_CAM_IOCTL_GET_PICTURE, &ctrlCmd) < 0) {
			LOGE("native_get_picture: MSM_CAM_IOCTL_GET_PICTURE fd %d error %s",
					camfd,
					strerror(errno));
			return false;
		}

		LOGD("crop: in1_w %d", crop->in1_w);
		LOGD("crop: in1_h %d", crop->in1_h);
		LOGD("crop: out1_w %d", crop->out1_w);
		LOGD("crop: out1_h %d", crop->out1_h);

		LOGD("crop: in2_w %d", crop->in2_w);
		LOGD("crop: in2_h %d", crop->in2_h);
		LOGD("crop: out2_w %d", crop->out2_w);
		LOGD("crop: out2_h %d", crop->out2_h);

		LOGD("crop: update %d", crop->update_flag);

		LOGD("native_get_picture status after ioctl == %d" ,ctrlCmd.status ) ;

		return true;
	}

	static bool native_stop_preview(int camfd)
	{
		struct msm_ctrl_cmd_t ctrlCmd;
		ctrlCmd.timeout_ms = 5000;
		ctrlCmd.type       = CAMERA_STOP_PREVIEW;
		ctrlCmd.length     = 0;
		//ctrlCmd.resp_fd    = camfd; // FIXME: this will be put in by the kernel

		if(ioctl(camfd, MSM_CAM_IOCTL_CTRL_COMMAND, &ctrlCmd) < 0) {
			LOGE("native_stop_preview: ioctl fd %d error %s",
					camfd,
					strerror(errno));
			return false;
		}
		LOGD("native_stop_preview status after ioctl == %d" ,ctrlCmd.status ) ;
		return true;
	}

	static bool native_start_snapshot(int camfd)
	{
		struct msm_ctrl_cmd_t ctrlCmd;

		ctrlCmd.timeout_ms = 5000;
		ctrlCmd.type       = CAMERA_START_SNAPSHOT;
		ctrlCmd.length     = 0;
		//ctrlCmd.resp_fd    = camfd; // FIXME: this will be put in by the kernel

		if(ioctl(camfd, MSM_CAM_IOCTL_CTRL_COMMAND, &ctrlCmd) < 0) {
			LOGE("native_start_snapshot: ioctl fd %d error %s",
					camfd,
					strerror(errno));
			return false;
		}
		LOGD("native_start_snapshot status after ioctl == %d" ,ctrlCmd.status ) ;
		return true;
	}

	static bool native_stop_snapshot (int camfd)
	{
		struct msm_ctrl_cmd_t ctrlCmd;

		ctrlCmd.timeout_ms = 5000;
		ctrlCmd.type       = CAMERA_STOP_SNAPSHOT;
		ctrlCmd.length     = 0;
		//ctrlCmd.resp_fd    = camfd; // FIXME: this will be put in by the kernel

		if (ioctl(camfd, MSM_CAM_IOCTL_CTRL_COMMAND, &ctrlCmd) < 0) {
			LOGE("native_stop_snapshot: ioctl fd %d error %s",
					camfd,
					strerror(errno));
			return false;
		}
		LOGD("native_stop_snapshot status after ioctl == %d" ,ctrlCmd.status ) ;
		return true;
	}


	void *jpeg_encoder_thread( void *user )
	{
		LOGD("jpeg_encoder_thread E");
		sp<QualcommCameraHardware> obj = QualcommCameraHardware::getInstance();
		if (obj != 0) {
			obj->runJpegEncodeThread(user);
		}
		else LOGW("not starting frame thread: the object went away!");
		LOGD("jpeg_encoder_thread X");
		return NULL;
	}

	bool QualcommCameraHardware::native_jpeg_encode(void)
	{
		int jpeg_quality = mParameters.getInt("jpeg-quality");
		if (jpeg_quality >= 0) {
			LOGD("native_jpeg_encode, current jpeg main img quality =%d",
					jpeg_quality);
			if(!LINK_jpeg_encoder_setMainImageQuality(jpeg_quality)) {
				LOGE("native_jpeg_encode set jpeg-quality failed");
				return false;
			}
			LOGD("jpeg main img quality done ") ;
		}
		//
		int thumbnail_quality = mParameters.getInt("jpeg-thumbnail-quality");
		if (thumbnail_quality >= 0) {

			if( !LINK_jpeg_encoder_setThumbnailQuality ) {
				LOGE("cannot set thumbnail-quality - not supported by libmmcamera");
			} else {
				LOGD("native_jpeg_encode, current jpeg thumbnail quality =%d",
						thumbnail_quality);
				if(!LINK_jpeg_encoder_setThumbnailQuality(thumbnail_quality)) {
					LOGE("native_jpeg_encode set thumbnail-quality failed");
					return false;
				}
			}
		}
		int rotation = mParameters.getInt("rotation");
		if (rotation >= 0) {
			if( !LINK_jpeg_encoder_setRotation ) {
				LOGE("cannot set jpeg rotation - not supported by libmmcamera");
			} else {
				LOGD("native_jpeg_encode, rotation = %d", rotation);
				if(!LINK_jpeg_encoder_setRotation(rotation)) {
					LOGE("native_jpeg_encode set rotation failed");
					return false;
				}
			}
		}
		jpeg_set_location();
		char jpegFileName[256] = {0};
		static int snapshotCntr = 0;

		mDimension.filler7 = 2560 ;
		mDimension.filler8 = 1920 ;



		LOGD("picture_width %d, picture_height = %d, display_width = %d, display_height = %d, filler = %d, filler2 = %d, ui_thumbnail_height = %d , ui_thumbnail_width = %d, filler3 = %d, filler4 = %d, filler5 = %d, filler6 = %d, filler7 = %d, filler8 = %d\n" ,
				mDimension.picture_width,mDimension.picture_height,
				mDimension.display_width,mDimension.display_height,
				mDimension.filler, mDimension.filler2,
				mDimension.ui_thumbnail_height, mDimension.ui_thumbnail_width,
				mDimension.filler3, mDimension.filler4, mDimension.filler5, mDimension.filler6,
				mDimension.filler7, mDimension.filler8 );


		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		/*struct sigaction actions ;
		  memset( &actions, 0, sizeof(actions));
		  sigemptyset(&actions.sa_mask);
		  actions.sa_flags = 0 ;
		  actions.sa_handler = exit_camframe_handler ;
		  sigaction( SIGUSR1, &actions, NULL ) ;*/

		pthread_t jpegThread ;

		int ret = !pthread_create(&jpegThread,
				&attr, //NULL,
				jpeg_encoder_thread,
				NULL);

		/*sprintf(jpegFileName, "snapshot_%d.jpg", ++snapshotCntr);
		  if ( !LINK_jpeg_encoder_encode(jpegFileName, &mDimension,
		  (uint8_t *)mThumbnailHeap->mHeap->base(), mThumbnailHeap->mHeap->getHeapID(),
		  (uint8_t *)mRawHeap->mHeap->base(), mRawHeap->mHeap->getHeapID(),
		  &mCrop)) {
		  LOGV("native_jpeg_encode:%d@%s: jpeg_encoder_encode failed.\n", __LINE__, __FILE__);
		  return false;
		  }*/

		/*
		   if (!LINK_jpeg_encoder_encode(&mDimension,
		   mThumbnailHeap->mHeap->getHeapID(),
		   mRawHeap->mHeap->getHeapID(),
		   (uint8_t *)mThumbnailHeap->mHeap->base(),
		   (uint8_t *)mRawHeap->mHeap->base(),
		   &mCrop)) {
		   LOGE("native_jpeg_encode: jpeg_encoder_encode failed.");
		   return false;
		   }*/
		return true;
	}

	bool QualcommCameraHardware::native_set_dimension(cam_ctrl_dimension_t *value)
	{
		LOGE("native_set_dimension: length: %d.", sizeof(cam_ctrl_dimension_t));
		LOGD("  mDimension.picture_width %d, mDimension.picture_height = %d" , value->picture_width,value->picture_height );
		return native_set_parm(CAMERA_SET_PARM_DIMENSION,
				sizeof(cam_ctrl_dimension_t), value);
	}


	bool QualcommCameraHardware::native_set_parm(
			cam_ctrl_type type, uint16_t length, void *value)
	{
		int rc = 0;
		struct msm_ctrl_cmd_t ctrlCmd;

		LOGD("native_set_parm length == %d", length ) ;

		ctrlCmd.timeout_ms = 5000;
		ctrlCmd.type       = (uint16_t)type;
		ctrlCmd.length     = length;
		// FIXME: this will be put in by the kernel
		//ctrlCmd.resp_fd    = mCameraControlFd;
		ctrlCmd.value = value;

		LOGD("native_set_parm. camfd=%d, type=%d, length=%d",
				mCameraControlFd, type, length);
		rc = ioctl(mCameraControlFd, MSM_CAM_IOCTL_CTRL_COMMAND, &ctrlCmd);
		if(rc < 0 || ctrlCmd.status != CAM_CTRL_SUCCESS) {
			LOGE("ioctl error. camfd=%d, type=%d, length=%d, rc=%d, ctrlCmd.status=%d, %s",
					mCameraControlFd, type, length, rc, ctrlCmd.status, strerror(errno));
			return false;
		}
		LOGD("native_set_parm status after ioctl == %d" ,ctrlCmd.status ) ;
		return true;
	}

	int QualcommCameraHardware::native_set_cfg(sensor_cfg_t cfgtype, int8_t data) {

		struct sensor_cfg_data_t scfg;

		scfg.cfgtype = cfgtype;
		scfg.cfg.effect = data;

		LOGD("native_set_cfg type %d, value %d\n", cfgtype, data);
		/* This will block until either AF completes or is cancelled. */
		int status = ioctl(mCameraControlFd, MSM_CAM_IOCTL_SENSOR_IO_CFG, &scfg);
		if( status < 0 )
			LOGE("native_set_parm2: ioctl failed");
		return status;
	}

	void QualcommCameraHardware::jpeg_set_location()
	{
		bool encode_location = true;
		camera_position_type pt;

#define PARSE_LOCATION(what,type,fmt,desc) do {                                \
	pt.what = 0;                                                           \
	const char *what##_str = mParameters.get("gps-"#what);                 \
	LOGD("GPS PARM %s --> [%s]", "gps-"#what, what##_str);                 \
	if (what##_str) {                                                      \
		type what = 0;                                                     \
		if (sscanf(what##_str, fmt, &what) == 1)                           \
		pt.what = what;                                                \
		else {                                                             \
			LOGE("GPS " #what " %s could not"                              \
					" be parsed as a " #desc, what##_str);                    \
			encode_location = false;                                       \
		}                                                                  \
	}                                                                      \
	else {                                                                 \
		LOGD("GPS " #what " not specified: "                               \
				"defaulting to zero in EXIF header.");                        \
		encode_location = false;                                           \
	}                                                                       \
} while(0)

		PARSE_LOCATION(timestamp, long, "%ld", "long");
		if (!pt.timestamp) pt.timestamp = time(NULL);
		PARSE_LOCATION(altitude, short, "%hd", "short");
		PARSE_LOCATION(latitude, double, "%lf", "double float");
		PARSE_LOCATION(longitude, double, "%lf", "double float");

#undef PARSE_LOCATION

		if (encode_location) {
			if( !LINK_jpeg_encoder_setLocation ) {
				LOGE("cannot set gps location in jpeg - not supported by libmmcamera");
			} else {
				LOGD("setting image location ALT %d LAT %lf LON %lf",
						pt.altitude, pt.latitude, pt.longitude);
				if (!LINK_jpeg_encoder_setLocation(&pt)) {
					LOGE("jpeg_set_location: LINK_jpeg_encoder_setLocation failed.");
				}
			}
		}
else LOGD("not setting image location");
}

void QualcommCameraHardware::runFrameThread(void *data)
{
	LOGD("runFrameThread E");

	int cnt;

		LOGD("Before LINK_cam_frame") ;
		LINK_cam_frame(data);
		LOGD("After LINK_cam_frame") ;

	mFrameThreadWaitLock.lock();
	mFrameThreadRunning = false;
	mFrameThreadWait.signal();
	mFrameThreadWaitLock.unlock();

	LOGD("runFrameThread X");
}

void QualcommCameraHardware::runJpegEncodeThread(void *data)
{
	// unsigned char *buffer ;

	// readFromMemory( (unsigned char *)mRawHeap->mHeap->base(), 2097152, buffer ) ;
	// writeToMemory( buffer, 2560, 1920, (char *)mJpegHeap->mHeap->base(), (int *)&mJpegSize ) ;

	memcpy( mJpegHeap->mHeap->base(), mRawHeap->mHeap->base(),  2097152) ;
	mJpegSize = 2097152 ;
	receiveJpegPicture();

}

void *frame_thread(void *user)
{
	LOGD("frame_thread E");
	sp<QualcommCameraHardware> obj = QualcommCameraHardware::getInstance();
	if (obj != 0) {
		obj->runFrameThread(user);
	}
	else LOGW("not starting frame thread: the object went away!");
	LOGD("frame_thread X");
	return NULL;
}


static bool register_buf(int camfd,
		int size,
		int pmempreviewfd,
		uint32_t offset,
		uint8_t *buf,
		msm_pmem_t mem_type,
		bool active,
		bool register_buffer = true);
bool QualcommCameraHardware::initPreview()
{
	// See comments in deinitPreview() for why we have to wait for the frame
	// thread here, and why we can't use pthread_join().
	LOGI("initPreview E: preview size=%dx%d", mPreviewWidth, mPreviewHeight);
	LOGD("running custom built libcamera.so for samsung galaxy by npinot based on NCommander for dream");
	mFrameThreadWaitLock.lock();
	while (mFrameThreadRunning) {
		LOGD("initPreview: waiting for old frame thread to complete.");
		mFrameThreadWait.wait(mFrameThreadWaitLock);
		LOGD("initPreview: old frame thread completed.");
	}
	mFrameThreadWaitLock.unlock();

	mSnapshotThreadWaitLock.lock();
	while (mSnapshotThreadRunning) {
		LOGD("initPreview: waiting for old snapshot thread to complete.");
		mSnapshotThreadWait.wait(mSnapshotThreadWaitLock);
		LOGD("initPreview: old snapshot thread completed.");
	}
	mSnapshotThreadWaitLock.unlock();



	mDimension.picture_width  = DEFAULT_PICTURE_WIDTH;
	mDimension.picture_height = DEFAULT_PICTURE_HEIGHT;
	LOGD ("mCameraControlFd == %d", mCameraControlFd ) ;
	bool ret = native_set_dimension(&mDimension);

	if (ret) {

		int cnt = 0;
		mPreviewFrameSize = mPreviewWidth * mPreviewHeight * 3/2;
		LOGD("mPreviewFrameSize = %d", mPreviewFrameSize ) ;
		mPreviewHeap = new PreviewPmemPool(
				mCameraControlFd,
				kRawFrameHeaderSize + mPreviewFrameSize,
				kPreviewBufferCount,
				mPreviewFrameSize,kRawFrameHeaderSize,
				"preview");

		if (!mPreviewHeap->initialized()) {
			mPreviewHeap.clear();
			LOGE("initPreview X: could not initialize preview heap.");
			return false;
		}


		frame_size= (clp2(mDimension.display_width * mDimension.display_height *3/2));
		unsigned char activeBuffer;
		LOGI("hal display_width = %d height = %d frame_size = %d\n",
				(int)mDimension.display_width, (int)mDimension.display_height, frame_size);

		//int frame_size= (clp2(mDimension.display_width * mDimension.display_height *3/2));
		for (cnt = 0; cnt < kPreviewBufferCount; cnt++) {
			frames[cnt].fd = mPreviewHeap->mHeapnew[cnt]->heapID();
			frames[cnt].buffer = (unsigned long)mPreviewHeap->mHeapnew[cnt]->base();
			LOGE("hal_mmap #%d start = %x end = %x", (int)cnt, (int)frames[cnt].buffer,
					(int)(frames[cnt].buffer + frame_size - 1));

			frames[cnt].y_off = 0;
			frames[cnt].cbcr_off = mDimension.display_width * mDimension.display_height;

			if (frames[cnt].buffer == 0) {
				LOGV("main: malloc failed!\n");
				return 0;
			}
			frames[cnt].path = MSM_FRAME_ENC;

			register_buf(mCameraControlFd,
					mDimension.display_width * mDimension.display_height * 3/2,
					frames[cnt].fd ,
					0,
					(uint8_t *)mPreviewHeap->mHeapnew[cnt]->base() ,
					MSM_PMEM_OUTPUT2, cnt == ( kPreviewBufferCount - 1 ) ? false : true, true );
		}

		mFrameThreadWaitLock.lock();
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		/*struct sigaction actions ;
		  memset( &actions, 0, sizeof(actions));
		  sigemptyset(&actions.sa_mask);
		  actions.sa_flags = 0 ;
		  actions.sa_handler = exit_camframe_handler ;
		  sigaction( SIGUSR1, &actions, NULL ) ;*/

		mFrameThreadRunning = !pthread_create(&mFrameThread,
				&attr, //NULL,
				frame_thread,
				&frames[kPreviewBufferCount-1]);
		ret = mFrameThreadRunning;
		mFrameThreadWaitLock.unlock();
	}

	LOGD("initPreview X: %d", ret);
	return ret;
}


void QualcommCameraHardware::deinitPreview(void)
{
	LOGI("deinitPreview E");

	// When we call deinitPreview(), we signal to the frame thread that it
	// needs to exit, but we DO NOT WAIT for it to complete here.  The problem
	// is that deinitPreview is sometimes called from the frame-thread's
	// callback, when the refcount on the Camera client reaches zero.  If we
	// called pthread_join(), we would deadlock.  So, we just call
	// LINK_camframe_terminate() in deinitPreview(), which makes sure that
	// after the preview callback returns, the camframe thread will exit.  We
	// could call pthread_join() in initPreview() to join the last frame
	// thread.  However, we would also have to call pthread_join() in release
	// as well, shortly before we destoy the object; this would cause the same
	// deadlock, since release(), like deinitPreview(), may also be called from
	// the frame-thread's callback.  This we have to make the frame thread
	// detached, and use a separate mechanism to wait for it to complete.

	//    if (LINK_camframe_terminate() < 0)
	//       LOGE("failed to stop the camframe thread: %s",
	//             strerror(errno));


	LOGD("before camframe_terminate ")  ;
	int rc = LINK_camframe_terminate() ;

	LOGD("camframe terminate rc = %d", rc ) ;

	LOGD("Unregister buffers") ;
	for (int cnt = 0; cnt < kPreviewBufferCount; cnt++) {
		register_buf(mCameraControlFd,
				mDimension.display_width * mDimension.display_height * 3/2,
				mPreviewHeap->mHeapnew[cnt]->heapID() ,
				0,
				(uint8_t *)mPreviewHeap->mHeapnew[cnt]->base() ,
				MSM_PMEM_OUTPUT2, cnt == ( kPreviewBufferCount - 1 ) ? false : true, false );

	}

	mPreviewHeap.clear();

	/*

*/
	LOGI("deinitPreview X");
}

bool QualcommCameraHardware::initRaw(bool initJpegHeap)
{
	mRawWidth = 2048 ;
	mRawHeight = 930 ;
	LOGD("initRaw E: picture size=%dx%d",
			mRawWidth, mRawHeight);


	mDimension.picture_width   = mRawWidth;
	mDimension.picture_height  = mRawHeight;
	mRawSize = mRawWidth * mRawHeight * 3 / 2;
	mJpegMaxSize = mRawWidth * mRawHeight * 3 / 2;

	mJpegMaxSize = 1904640 ;

	if(!native_set_dimension(&mDimension)) {
		LOGE("initRaw X: failed to set dimension");
		return false;
	}

	if (mJpegHeap != NULL) {
		LOGD("initRaw: clearing old mJpegHeap.");
		mJpegHeap.clear();
	}

	// Snapshot

	LOGD("initRaw: initializing mRawHeap.");
	mRawHeap =
		new PmemPool("/dev/pmem_camera",
				mCameraControlFd,
				MSM_PMEM_MAINIMG,
				mJpegMaxSize,
				kRawBufferCount,
				mRawSize,
				0,
				"snapshot camera");

	if (!mRawHeap->initialized()) {
		LOGE("initRaw X failed with pmem_camera, trying with pmem_adsp");
		mRawHeap =
			new PmemPool("/dev/pmem_adsp",
					mCameraControlFd,
					MSM_PMEM_MAINIMG,
					mJpegMaxSize,
					kRawBufferCount,
					mRawSize,
					0,
					"snapshot camera" );
		if (!mRawHeap->initialized()) {
			mRawHeap.clear();
			LOGE("initRaw X: error initializing mRawHeap");
			return false;
		}
	}

	// Thumbnails

	mThumbnailHeap =
		new PmemPool("/dev/pmem_adsp",
				mCameraControlFd,
				MSM_PMEM_THUMBAIL,
				262144, //THUMBNAIL_BUFFER_SIZE,
				1,
				262144, //THUMBNAIL_BUFFER_SIZE,
				0,
				"thumbnail");
	//  thumbnail_buf = hal_mmap(THUMBNAIL_BUFFER_SIZE,
	//                                &pmemThumbnailfd);

	if (!mThumbnailHeap->initialized()) {
		mThumbnailHeap.clear();
		mRawHeap.clear();
		LOGE("initRaw X failed: error initializing mThumbnailHeap.");
		return false;
	}




	/*
	   static bool register_buf(int camfd,
	   int size,
	   int pmempreviewfd,
	   uint32_t offset,
	   uint8_t *buf,
	   int pmem_type,
	   bool active ,
	   bool register_buffer)
	   */

	LOGD("do_mmap snapshot pbuf = %p, pmem_fd = %d",
			(uint8_t *)mRawHeap->mHeap->base(), mRawHeap->mHeap->getHeapID());

	// Jpeg

	if (initJpegHeap) {
		LOGD("initRaw: initializing mJpegHeap.");
		mJpegHeap =
			new AshmemPool(2097152,
					kJpegBufferCount,
					0, // we do not know how big the picture wil be
					0,
					"jpeg");

		if (!mJpegHeap->initialized()) {
			mJpegHeap.clear();
			mRawHeap.clear();
			LOGE("initRaw X failed: error initializing mJpegHeap.");
			return false;
		}


	}

	register_buf( mCameraControlFd,
			262144,mThumbnailHeap->mHeap->getHeapID(), 0,
			(uint8_t *)mThumbnailHeap->mHeap->base(), MSM_PMEM_THUMBAIL, true, true ) ;
	//TODO why 2097152 and not mJpegMaxSize
	register_buf( mCameraControlFd,
			2097152,mRawHeap->mHeap->getHeapID(), 0,
			(uint8_t *)mRawHeap->mHeap->base(), MSM_PMEM_RAW_MAINIMG, true, true ) ;
	mRawInitialized = true ;
	LOGD("initRaw X");
	return true;
}

void QualcommCameraHardware::deinitRaw()
{
	LOGD("deinitRaw E");
	register_buf( mCameraControlFd,
			262144,mThumbnailHeap->mHeap->getHeapID(), 0,
			(uint8_t *)mThumbnailHeap->mHeap->base(), MSM_PMEM_THUMBAIL, true, false ) ;

	register_buf( mCameraControlFd,
			2097152,mRawHeap->mHeap->getHeapID(), 0,
			(uint8_t *)mRawHeap->mHeap->base(), MSM_PMEM_RAW_MAINIMG, true, false ) ;

	mThumbnailHeap.clear();
	mJpegHeap.clear();
	mRawHeap.clear();
	mRawInitialized = false ;
	LOGD("deinitRaw X");
}

void QualcommCameraHardware::release()
{
	LOGD("release E");
	Mutex::Autolock l(&mLock);

	int cnt, rc;
	struct msm_ctrl_cmd_t ctrlCmd;

	if (mCameraRunning) {
		if(mMsgEnabled & CAMERA_MSG_VIDEO_FRAME) {
			mRecordFrameLock.lock();
			mReleasedRecordingFrame = true;
			mRecordWait.signal();
			mRecordFrameLock.unlock();
		}
		stopPreviewInternal();
	}

	LINK_jpeg_encoder_join();
	if( mRawInitialized ) {
		deinitRaw();
	}

	ctrlCmd.timeout_ms = 5000;
	ctrlCmd.length = 0;
	ctrlCmd.type = (uint16_t)CAMERA_EXIT;
	//    ctrlCmd.resp_fd = mCameraControlFd; // FIXME: this will be put in by the kernel
	if (ioctl(mCameraControlFd, MSM_CAM_IOCTL_CTRL_COMMAND, &ctrlCmd) < 0)
		LOGE("ioctl CAMERA_EXIT fd %d error %s",
				mCameraControlFd, strerror(errno));
	rc = pthread_join(mCamConfigThread, NULL);
	if (rc)
		LOGE("config_thread exit failure: %s", strerror(errno));
	else
		LOGD("pthread_join succeeded on config_thread");

	close(mCameraControlFd);
	mCameraControlFd = -1;

	if (libmmcamera_target) {
		::dlclose(libmmcamera_target);
		LOGD("dlclose(libmmcamera_target)");
		libmmcamera_target = NULL ;
	}

	Mutex::Autolock lock(&singleton_lock);
	singleton_releasing = true;

	LOGD("release X");
}

QualcommCameraHardware::~QualcommCameraHardware()
{
	LOGD("~QualcommCameraHardware E");
	Mutex::Autolock lock(&singleton_lock);
	singleton.clear();
	singleton_releasing = false;
	singleton_wait.signal();
	LOGD("~QualcommCameraHardware X");
}

sp<IMemoryHeap> QualcommCameraHardware::getRawHeap() const
{
	LOGD("getRawHeap");
	return mRawHeap != NULL ? mRawHeap->mHeap : NULL;
}

sp<IMemoryHeap> QualcommCameraHardware::getPreviewHeap() const
{
	LOGD("interface: getPreviewHeap");
	if( mPreviewHeap != NULL ) {
		LOGD("previewHeap not null") ;
	}
	return mPreviewHeap != NULL ? mPreviewHeap->mHeapnew[3] : NULL;
}

sp<IMemoryHeap> QualcommCameraHardware::getPreviewHeap(int32_t i) const
{
	LOGD("interface: getPreviewHeap");
	if( mPreviewHeap != NULL ) {
		LOGD("previewHeap not null") ;
	}
	return mPreviewHeap != NULL ? mPreviewHeap->mHeapnew[i] : NULL;
}

status_t QualcommCameraHardware::startPreviewInternal()
{
	if(mCameraRunning) {
		LOGD("startPreview X: preview already running.");
		return NO_ERROR;
	}

	if (!mPreviewInitialized) {
		mPreviewInitialized = initPreview();
		if (!mPreviewInitialized) {
			LOGE("startPreview X initPreview failed.  Not starting preview.");
			return UNKNOWN_ERROR;
		}
	}

	mCameraRunning = native_start_preview(mCameraControlFd);
	if(!mCameraRunning) {
		deinitPreview();
		mPreviewInitialized = false;
		LOGE("startPreview X: native_start_preview failed!");
		return UNKNOWN_ERROR;
	}


	LOGD("startPreview X");
	return NO_ERROR;
}

status_t QualcommCameraHardware::startPreview()
{
	LOGD("startPreview E");
	Mutex::Autolock l(&mLock);

	return startPreviewInternal();
}

void QualcommCameraHardware::stopPreviewInternal()
{
	LOGD("stopPreviewInternal E: %d", mCameraRunning);
	if (mCameraRunning) {
		// Cancel auto focus.
		if (mMsgEnabled & CAMERA_MSG_FOCUS) {
			LOGD("canceling autofocus");
		}

		LOGD("Stopping preview");

		mCameraRunning = !native_stop_preview(mCameraControlFd);
		if (!mCameraRunning && mPreviewInitialized) {
			deinitPreview();
			mPreviewInitialized = false;
		}
		else LOGE("stopPreviewInternal: failed to stop preview");
	}
	LOGD("stopPreviewInternal X: %d", mCameraRunning);
}

void QualcommCameraHardware::stopPreview()
{
	LOGD("stopPreview: E");
	Mutex::Autolock l(&mLock);

	if(mMsgEnabled & CAMERA_MSG_VIDEO_FRAME)
		return;

	stopPreviewInternal();

	LOGD("stopPreview: X");
}

void QualcommCameraHardware::runAutoFocus()
{
	/* TODO: do we need this thread? the ioctl blocks and can't be cancled by any means
	 * so we could just execute that without wrapping it inside this auto-focus thread */
	mAutoFocusThreadLock.lock();

	int32_t useMacro = getParm(CameraParameters::KEY_FOCUS_MODE, auto_focus);
	if (useMacro == NOT_FOUND) {
		useMacro = 0;
	}

	struct sensor_cfg_data_t scfg;

	scfg.cfgtype = CFG_SET_DEFAULT_FOCUS;
	scfg.cfg.focus.steps = 0;
	scfg.cfg.focus.dir = (sensor_move_focus_t)useMacro;

	/* This will block until either AF completes or is cancelled. */
	int status = ioctl(mCameraControlFd, MSM_CAM_IOCTL_SENSOR_IO_CFG, &scfg);
	bool gotFocus = (scfg.cfg.focus.steps==0);

	LOGI("runAutoFocus: %s",gotFocus?"good":"failed");

	mAutoFocusThreadRunning = false;
	mAutoFocusThreadLock.unlock();

	if (mMsgEnabled & CAMERA_MSG_FOCUS) {
		LOGD("mNotifyCb(CAMERA_MSG_FOCUS, gotFocus = %d)", gotFocus);
		mNotifyCb(CAMERA_MSG_FOCUS, gotFocus, 0, mCallbackCookie);
	}
}

status_t QualcommCameraHardware::cancelAutoFocus()
{
	return NO_ERROR;
}

void *auto_focus_thread(void *user)
{
	LOGD("auto_focus_thread E");
	sp<QualcommCameraHardware> obj = QualcommCameraHardware::getInstance();
	if (obj != 0) {
		obj->runAutoFocus();
	}
	else LOGW("not starting autofocus: the object went away!");
	LOGD("auto_focus_thread X");
	return NULL;
}

status_t QualcommCameraHardware::autoFocus()
{
	LOGD("autoFocus E");
	Mutex::Autolock l(&mLock);

	if (mCameraControlFd < 0) {
		LOGE("not starting autofocus: main control fd %d", mCameraControlFd);
		return UNKNOWN_ERROR;
	}

	/* Not sure this is still needed with new APIs ..
	   if (mMsgEnabled & CAMERA_MSG_FOCUS) {
	   LOGW("Auto focus is already in progress");
	   return NO_ERROR;
	// No idea how to rewrite this
	//return mAutoFocusCallback == af_cb ? NO_ERROR : INVALID_OPERATION;
	}*/

	{
		mAutoFocusThreadLock.lock();
		if (!mAutoFocusThreadRunning) {
			// Create a detatched thread here so that we don't have to wait
			// for it when we cancel AF.
			pthread_t thr;
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			mAutoFocusThreadRunning =
				!pthread_create(&thr, &attr,
						auto_focus_thread, NULL);
			if (!mAutoFocusThreadRunning) {
				LOGE("failed to start autofocus thread");
				mAutoFocusThreadLock.unlock();
				return UNKNOWN_ERROR;
			}
		}
		mAutoFocusThreadLock.unlock();
	}

	LOGD("autoFocus X");
	return NO_ERROR;
}

void QualcommCameraHardware::runSnapshotThread(void *data)
{
	LOGD("runSnapshotThread E");

	/*	m4mo_write_8bit( 0x0c, 0x00, 0x00 ) ;
		m4mo_write_8bit( 0x02, 0x00, 0x08 ) ;
		m4mo_write_8bit( 0x02, 0x04, 0x06 ) ;
		m4mo_write_8bit( 0x02, 0x25, 0x01 ) ;
		m4mo_write_8bit( 0x00, 0x11, 0x08 ) ;
		m4mo_write_8bit( 0x00, 0x12, 0x01 ) ;*/
	usleep( 20000 ) ;

	//	   m4mo_write_8bit( 0x00, 0x0b, 0x03 );

	if (native_start_snapshot(mCameraControlFd)) {
		// guessed from logs
		notifyShutter() ;


		receiveRawPicture();
	}
	else {
		LOGE("main: native_start_snapshot failed!");
	}

	mSnapshotThreadWaitLock.lock();
	mSnapshotThreadRunning = false;
	mSnapshotThreadWait.signal();
	mSnapshotThreadWaitLock.unlock();

	LOGD("runSnapshotThread X");
}

void *snapshot_thread(void *user)
{
	LOGD("snapshot_thread E");
	sp<QualcommCameraHardware> obj = QualcommCameraHardware::getInstance();
	if (obj != 0) {
		obj->runSnapshotThread(user);
	}
	else LOGW("not starting snapshot thread: the object went away!");
	LOGD("snapshot_thread X");
	return NULL;
}

status_t QualcommCameraHardware::takePicture()
{
	LOGD("takePicture: E");
	Mutex::Autolock l(&mLock);

	// Wait for old snapshot thread to complete.
	mSnapshotThreadWaitLock.lock();
	while (mSnapshotThreadRunning) {
		LOGD("takePicture: waiting for old snapshot thread to complete.");
		mSnapshotThreadWait.wait(mSnapshotThreadWaitLock);
		LOGD("takePicture: old snapshot thread completed.");
	}

	stopPreviewInternal();

	if (!initRaw(mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)) { /* not sure if this is right */
		LOGE("initRaw failed.  Not taking picture.");
		return UNKNOWN_ERROR;
	}



	mShutterLock.lock();
	mShutterPending = true;
	mShutterLock.unlock();

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	mSnapshotThreadRunning = !pthread_create(&mSnapshotThread,
			&attr,
			snapshot_thread,
			NULL);
	mSnapshotThreadWaitLock.unlock();

	LOGD("takePicture: X");
	return mSnapshotThreadRunning ? NO_ERROR : UNKNOWN_ERROR;
}

status_t QualcommCameraHardware::cancelPicture()
{
	LOGD("cancelPicture: EX");

	return NO_ERROR;
}

status_t QualcommCameraHardware::setParameters(
		const CameraParameters& params)
{
	LOGD("setParameters: E params = %p", &params);

	//LOGD("dump param ") ;
	//params.dump() ;
	//LOGD("dump done") ;

	Mutex::Autolock l(&mLock);

	// Set preview size.
	preview_size_type *ps = preview_sizes;
	{
		int width, height;
		params.getPreviewSize(&width, &height);
		LOGD("requested size %d x %d", width, height);
		// Validate the preview size
		size_t i;
		for (i = 0; i < PREVIEW_SIZE_COUNT; ++i, ++ps) {
			if (width == ps->width && height == ps->height)
				break;
		}
		if (i == PREVIEW_SIZE_COUNT) {
			LOGE("Invalid preview size requested: %dx%d",
					width, height);
			return BAD_VALUE;
		}
	}
	mPreviewWidth = mDimension.display_width = ps->width;
	mPreviewHeight = mDimension.display_height = ps->height;

	LOGD("mPreviewWidth %d, mPreviewHeight %d", mPreviewWidth, mPreviewHeight ) ;

	// FIXME: validate snapshot sizes,
	params.getPictureSize(&mRawWidth, &mRawHeight);

	mRawWidth = (mRawWidth+1) & ~1 ;
	mRawHeight = (mRawHeight+1) & ~1 ;


	mDimension.picture_width = mRawWidth;
	mDimension.picture_height = mRawHeight;

	// Set up the jpeg-thumbnail-size parameters.
	{
		int val;

		val = params.getInt("jpeg-thumbnail-width");
		if (val < 0) {
			mDimension.ui_thumbnail_width= THUMBNAIL_WIDTH;
			LOGW("jpeg-thumbnail-width is not specified: defaulting to %d",
					THUMBNAIL_WIDTH);
		}
		else mDimension.ui_thumbnail_width = val;
		LOGD("jpeg-thumbnail-width %d", val ) ;

		val = params.getInt("jpeg-thumbnail-height");
		if (val < 0) {
			mDimension.ui_thumbnail_height= THUMBNAIL_HEIGHT;
			LOGW("jpeg-thumbnail-height is not specified: defaulting to %d",
					THUMBNAIL_HEIGHT);
		}
		else mDimension.ui_thumbnail_height = val;
		LOGD("jpeg-thumbnail-height %d", val ) ;
	}

	mParameters = params;

	setAntibanding();
	setEffect();
	setWhiteBalance();
	setSceneMode();
	setFlashMode();
	// FIXME: set nightshot and luma adaptatiom

	LOGD("setParameters: X");
	return NO_ERROR ;
}

CameraParameters QualcommCameraHardware::getParameters() const
{
	LOGD("getParameters: EX");
	return mParameters;
}

extern "C" sp<CameraHardwareInterface> openCameraHardware()
{
	LOGD("openCameraHardware: call createInstance");
	return QualcommCameraHardware::createInstance();
}

wp<QualcommCameraHardware> QualcommCameraHardware::singleton;

// If the hardware already exists, return a strong pointer to the current
// object. If not, create a new hardware object, put it in the singleton,
// and return it.
sp<CameraHardwareInterface> QualcommCameraHardware::createInstance()
{
	LOGD("createInstance: E");

	Mutex::Autolock lock(&singleton_lock);

	// Wait until the previous release is done.
	while (singleton_releasing) {
		LOGD("Wait for previous release.");
		singleton_wait.wait(singleton_lock);
	}

	if (singleton != 0) {
		sp<CameraHardwareInterface> hardware = singleton.promote();
		if (hardware != 0) {
			LOGD("createInstance: X return existing hardware=%p", &(*hardware));
			return hardware;
		}
	}

	{
		struct stat st;
		int rc = stat("/dev/oncrpc", &st);
		if (rc < 0) {
			LOGD("createInstance: X failed to create hardware: %s", strerror(errno));
			return NULL;
		}
	}

	QualcommCameraHardware *cam = new QualcommCameraHardware();
	sp<QualcommCameraHardware> hardware(cam);
	singleton = hardware;

	cam->initDefaultParameters();
	cam->startCamera();

	LOGD("createInstance: X created hardware=%p", &(*hardware));
	return hardware;
}

// For internal use only, hence the strong pointer to the derived type.
sp<QualcommCameraHardware> QualcommCameraHardware::getInstance()
{
	sp<CameraHardwareInterface> hardware = singleton.promote();
	if (hardware != 0) {
		//    LOGD("getInstance: X old instance of hardware");
		return sp<QualcommCameraHardware>(static_cast<QualcommCameraHardware*>(hardware.get()));
	} else {
		LOGD("getInstance: X new instance of hardware");
		return sp<QualcommCameraHardware>();
	}
}

static ssize_t previewframe_offset = 3;

void QualcommCameraHardware::receivePreviewFrame(struct msm_frame_t *frame)
{
	//    LOGD("receivePreviewFrame E");

	if (!mCameraRunning) {
		LOGE("ignoring preview callback--camera has been stopped");
		return;
	}

	// Why is this here?
	/*mCallbackLock.lock();
	  preview_callback pcb = mPreviewCallback;
	  void *pdata = mPreviewCallbackCookie;
	  recording_callback rcb = mRecordingCallback;
	  void *rdata = mRecordingCallbackCookie;
	  mCallbackLock.unlock(); */

	// Find the offset within the heap of the current buffer.
	//ssize_t offset =
	/////    (ssize_t)frame->buffer - (ssize_t)mPreviewHeap->mHeap->base();
	//offset /= mPreviewFrameSize;

	if ((unsigned int)mPreviewHeap->mHeapnew[previewframe_offset]->base() !=
			(unsigned int)frame->buffer)
		for (previewframe_offset = 0; previewframe_offset < 4; previewframe_offset++) {
			if ((unsigned int)mPreviewHeap->mHeapnew[previewframe_offset]->base() ==
					(unsigned int)frame->buffer)
				break;
		}


	mInPreviewCallback = true;
	if (mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) {
		LOGD("mDataCb(CAMERA_MSG_PREVIEW_FRAME)");
		mDataCb(CAMERA_MSG_PREVIEW_FRAME, mPreviewHeap->mBuffers[previewframe_offset], mCallbackCookie);
	}

	if (mMsgEnabled & CAMERA_MSG_VIDEO_FRAME) {
		Mutex::Autolock rLock(&mRecordFrameLock);
		LOGD("mDataCbTimestamp");
		mDataCbTimestamp(systemTime(), CAMERA_MSG_VIDEO_FRAME, mPreviewHeap->mBuffers[previewframe_offset], mCallbackCookie); /* guess? */
		//mDataCb(CAMERA_MSG_VIDEO_FRAME, mPreviewHeap->mBuffers[previewframe_offset], mCallbackCookie);

		if (mReleasedRecordingFrame != true) {
			LOGV("block for release frame request/command");
			mRecordWait.wait(mRecordFrameLock);
		}
		mReleasedRecordingFrame = false;
	}

	/*if(mMsgEnabled & CAMERA_MSG_VIDEO_IMAGE) {
	  Mutex::Autolock rLock(&mRecordFrameLock);
	  rcb(systemTime(), mPreviewHeap->mBuffers[offset], rdata);
	  if (mReleasedRecordingFrame != true) {
	  LOGD("block for release frame request/command");
	  mRecordWait.wait(mRecordFrameLock);
	  }
	  mReleasedRecordingFrame = false;
	  }*/
	mInPreviewCallback = false;
	previewframe_offset--;
	previewframe_offset &= 3;
	//    LOGD("receivePreviewFrame X");
}

status_t QualcommCameraHardware::startRecording()
{
	LOGD("startRecording E");
	Mutex::Autolock l(&mLock);

	mReleasedRecordingFrame = false;
	mCameraRecording = true;

	return startPreviewInternal();
}

void QualcommCameraHardware::stopRecording()
{
	LOGD("stopRecording: E");
	Mutex::Autolock l(&mLock);

	{
		mRecordFrameLock.lock();
		mReleasedRecordingFrame = true;
		mRecordWait.signal();
		mRecordFrameLock.unlock();

		mCameraRecording = false;

		if(mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) {
			LOGD("stopRecording: X, preview still in progress");
			return;
		}
	}

	stopPreviewInternal();
	LOGD("stopRecording: X");
}

	void QualcommCameraHardware::releaseRecordingFrame(
			const sp<IMemory>& mem __attribute__((unused)))
{
	LOGV("releaseRecordingFrame E");
	Mutex::Autolock l(&mLock);
	Mutex::Autolock rLock(&mRecordFrameLock);
	mReleasedRecordingFrame = true;
	mRecordWait.signal();
	LOGV("releaseRecordingFrame X");
}

bool QualcommCameraHardware::recordingEnabled()
{
	return (mCameraRunning && mCameraRecording);
}

void QualcommCameraHardware::notifyShutter()
{
	mShutterLock.lock();
	if (mShutterPending && (mMsgEnabled & CAMERA_MSG_SHUTTER)) {
		LOGD("mNotifyCb(CAMERA_MSG_SHUTTER)");
		mNotifyCb(CAMERA_MSG_SHUTTER, 0, 1, mCallbackCookie);
		mShutterPending = false;
	}
	mShutterLock.unlock();
}
/*
   static void receive_shutter_callback()
   {
   LOGD("receive_shutter_callback: E");
   sp<QualcommCameraHardware> obj = QualcommCameraHardware::getInstance();
   if (obj != 0) {
   obj->notifyShutter();
   }
   LOGD("receive_shutter_callback: X");
   }
   */

void QualcommCameraHardware::receiveRawPicture()
{
	LOGD("receiveRawPicture: E");



	if (mMsgEnabled & CAMERA_MSG_RAW_IMAGE) {
		int nbtry = 0 ;
		char ret = 0 ;
		/*    do {
			  ret = m4mo_read_8bit( 0x00, 0x0c ) ;
			  nbtry++ ;
			  usleep( 30000 ) ;
			  } while( ret != 7 && nbtry < 50 ) ;

			  char r = m4mo_read_8bit( 0x00, 0x10 ) ;
			  m4mo_write_8bit( 0x0c, 0x04, 0x01 ) ;
			  m4mo_write_8bit( 0x0c, 0x05, 0x01 ) ;
			  */
		LOGD("before native_get_picture") ;
		if(native_get_picture(mCameraControlFd, &mCrop) == false) {
			LOGE("getPicture failed!");
			return;
		}
		/*
		   char r1  = m4mo_read_8usleepbit( 0x0c, 0x10 ) ;
		   char r2  = m4mo_read_8bit( 0x0c, 0x11 ) ;
		   char r3  = m4mo_read_8bit( 0x0c, 0x12 ) ;
		   char r4  = m4mo_read_8bit( 0x0c, 0x13 ) ;

		   char r5  = m4mo_read_8bit( 0x00, 0x01 ) ;
		   char r6  = m4mo_read_8bit( 0x00, 0x02 ) ;

		   char r7  = m4mo_read_8bit( 0x07, 0x00 ) ;
		   char r8  = m4mo_read_8bit( 0x07, 0x01 ) ;
		   char r9  = m4mo_read_8bit( 0x07, 0x02 ) ;
		   char r10 = m4mo_read_8bit( 0x07, 0x03 ) ;
		   char r11 = m4mo_read_8bit( 0x07, 0x04 ) ;
		   char r12 = m4mo_read_8bit( 0x07, 0x05 ) ;
		   char r13 = m4mo_read_8bit( 0x07, 0x06 ) ;
		   char r14 = m4mo_read_8bit( 0x07, 0x07 ) ;

		   char r15 = m4mo_read_8bit( 0x07, 0x28 ) ;
		   char r16 = m4mo_read_8bit( 0x07, 0x29 ) ;

		   char r17 = m4mo_read_8bit( 0x03, 0x01 ) ;

		   char r18 = m4mo_read_8bit( 0x01, 0x04 ) ;

		   char r19 = m4mo_read_8bit( 0x02, 0x0f ) ;
		   char r20 = m4mo_read_8bit( 0x02, 0x11 ) ;

		   char r21 = m4mo_read_8bit( 0x06, 0x02 ) ;
		   */
		LOGD("native_get_picture done") ;
		// By the time native_get_picture returns, picture is taken. Call
		// shutter callback if cam config thread has not done that.
		LOGD("mDataCb(CAMERA_MSG_RAW_IMAGE)");
		mDataCb(CAMERA_MSG_RAW_IMAGE, mRawHeap->mBuffers[0], mCallbackCookie);
	}
	else LOGD("Raw-picture callback was canceled--skipping.");

	if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE) {
		mJpegSize = 0;
		if (LINK_jpeg_encoder_init()) {
			if(native_jpeg_encode()) {
				LOGD("receiveRawPicture: X (success)");
				return;
			}
			LOGE("jpeg encoding failed");
		}
		else LOGE("receiveRawPicture X: jpeg_encoder_init failed.");
	}
	else LOGD("JPEG callback is NULL, not encoding image.");
	deinitRaw();
	LOGD("receiveRawPicture: X");
}

void QualcommCameraHardware::receiveJpegPictureFragment(
		uint8_t *buff_ptr, uint32_t buff_size)
{
	uint32_t remaining = mJpegHeap->mHeap->virtualSize();
	remaining -= mJpegSize;
	uint8_t *base = (uint8_t *)mJpegHeap->mHeap->base();

	LOGD("receiveJpegPictureFragment size %d", buff_size);
	if (buff_size > remaining) {
		LOGE("receiveJpegPictureFragment: size %d exceeds what "
				"remains in JPEG heap (%d), truncating",
				buff_size,
				remaining);
		buff_size = remaining;
	}
	memcpy(base + mJpegSize, buff_ptr, buff_size);
	mJpegSize += buff_size;
}

void QualcommCameraHardware::receiveJpegPicture(void)
{
	LOGD("receiveJpegPicture: E image (%d uint8_ts out of %d)",
			mJpegSize, mJpegHeap->mBufferSize);

	LOGD("mJpegHeap->mFrameOffset %d", mJpegHeap->mFrameOffset ) ;

	int index = 0, rc;

	if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE) {
		// The reason we do not allocate into mJpegHeap->mBuffers[offset] is
		// that the JPEG image's size will probably change from one snapshot
		// to the next, so we cannot reuse the MemoryBase object.
		sp<MemoryBase> buffer = new
			MemoryBase(mJpegHeap->mHeap,
					index * mJpegHeap->mBufferSize +
					mJpegHeap->mFrameOffset,
					mJpegSize);

		LOGD("mDataCb(CAMERA_MSG_COMPRESSED_IMAGE)");
		mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, buffer, mCallbackCookie);
		buffer = NULL;
	}
	else LOGD("JPEG callback was cancelled--not delivering image.");

	LINK_jpeg_encoder_join();
	deinitRaw();

	LOGD("receiveJpegPicture: X callback done.");
}

bool QualcommCameraHardware::previewEnabled()
{
	Mutex::Autolock l(&mLock);
	return (mCameraRunning && (mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME));
}

int QualcommCameraHardware::getParm(
		const char *parm_str, const struct str_map *const parm_map)
{
	// Check if the parameter exists.
	const char *str = mParameters.get(parm_str);
	if (str == NULL) return NOT_FOUND;

	// Look up the parameter value.
	return attr_lookup(parm_map, str);
}

void QualcommCameraHardware::setEffect()
{
	int8_t value = getParm(CameraParameters::KEY_EFFECT, effect);
	if (value != NOT_FOUND) {
		native_set_cfg(CFG_SET_EFFECT, value);
	}
}

void QualcommCameraHardware::setWhiteBalance()
{
	int8_t value = getParm(CameraParameters::KEY_WHITE_BALANCE, whitebalance);
	if (value != NOT_FOUND) {
		native_set_cfg(CFG_SET_WB, value);
	}
}

void QualcommCameraHardware::setAntibanding()
{
	int8_t value = getParm(CameraParameters::KEY_ANTIBANDING, antibanding);
	if(value != NOT_FOUND)
		native_set_cfg(CFG_SET_FLICKER, value);
}

void QualcommCameraHardware::setSceneMode()
{
	int8_t value = getParm(CameraParameters::KEY_SCENE_MODE, scene_modes);
	if(value != NOT_FOUND)
		native_set_cfg(CFG_SET_SCENE_MODE, value);
}

void QualcommCameraHardware::setFlashMode()
{
	int8_t value = getParm(CameraParameters::KEY_FLASH_MODE, flash_mode);
	if(value != NOT_FOUND)
		native_set_cfg(CFG_SET_FLASH_MODE, value);
}


QualcommCameraHardware::MemPool::MemPool(int buffer_size, int num_buffers,
		int frame_size,
		int frame_offset,
		const char *name) :
	mBufferSize(buffer_size),
	mNumBuffers(num_buffers),
	mFrameSize(frame_size),
	mFrameOffset(frame_offset),
	mBuffers(NULL), mName(name)
{
	// empty
}

void QualcommCameraHardware::MemPool::completeInitialization()
{
	// If we do not know how big the frame will be, we wait to allocate
	// the buffers describing the individual frames until we do know their
	// size.
	LOGD("mBufferSize = %d, mFrameOffset = %d, mFrameSize = %d", mBufferSize, mFrameOffset, mFrameSize ) ;
	if (mFrameSize > 0) {
		mBuffers = new sp<MemoryBase>[mNumBuffers];
		for (int i = 0; i < mNumBuffers; i++) {
			mBuffers[i] = new
				MemoryBase(mHeap,
						i * mBufferSize + mFrameOffset,
						mFrameSize);
		}
	}
}

void QualcommCameraHardware::MemPool::completeInitializationnew()
{
	LOGD("QualcommCameraHardware::MemPool::completeInitializationnew");

	if (mFrameSize > 0) {
		mBuffers = new sp<MemoryBase>[mNumBuffers];
		for (int i = 0; i < mNumBuffers; i++) {
			LOGI("SFbufs: i = %d mBufferSize = %d mFrameOffset = %d mFrameSize = %d\n", i, mBufferSize, mFrameOffset, mFrameSize);
			mBuffers[i] = new
				MemoryBase(mHeapnew[i], 0, mFrameSize);
		}
	}
}

QualcommCameraHardware::AshmemPool::AshmemPool(int buffer_size, int num_buffers,
		int frame_size,
		int frame_offset,
		const char *name) :
	QualcommCameraHardware::MemPool(buffer_size,
			num_buffers,
			frame_size,
			frame_offset,
			name)
{
	LOGD("constructing MemPool %s backed by ashmem: "
			"%d frames @ %d uint8_ts, offset %d, "
			"buffer size %d",
			mName,
			num_buffers, frame_size, frame_offset, buffer_size);

	int page_mask = getpagesize() - 1;
	int ashmem_size = buffer_size * num_buffers;
	ashmem_size += page_mask;
	ashmem_size &= ~page_mask;

	mHeap = new MemoryHeapBase(ashmem_size);

	completeInitialization();
}


QualcommCameraHardware::PmemPool::PmemPool(const char *pmem_pool,
		int camera_control_fd,
		msm_pmem_t pmem_type,
		int buffer_size, int num_buffers,
		int frame_size,
		int frame_offset,
		const char *name) :
	QualcommCameraHardware::MemPool(buffer_size,
			num_buffers,
			frame_size,
			frame_offset,
			name),
	mPmemType(pmem_type),
	mCameraControlFd(camera_control_fd)
{
	LOGV("constructing MemPool %s backed by pmem pool %s: "
			"%d frames @ %d bytes, offset %d, buffer size %d",
			mName,
			pmem_pool, num_buffers, frame_size, frame_offset,
			buffer_size);

	ptypeflag = 0;

	// Make a new mmap'ed heap that can be shared across processes.

	mAlignedSize = clp2(buffer_size * num_buffers);

	sp<MemoryHeapBase> masterHeap =
		new MemoryHeapBase(pmem_pool, mAlignedSize, 0);
	sp<MemoryHeapPmem> pmemHeap = new MemoryHeapPmem(masterHeap, 0);
	if (pmemHeap->getHeapID() >= 0) {
		pmemHeap->slap();
		masterHeap.clear();
		mHeap = pmemHeap;
		pmemHeap.clear();

		mFd = mHeap->getHeapID();
		if (::ioctl(mFd, PMEM_GET_SIZE, &mSize)) {
			LOGE("pmem pool %s ioctl(PMEM_GET_SIZE) error %s (%d)",
					pmem_pool,
					::strerror(errno), errno);
			mHeap.clear();
			return;
		}

		LOGE("pmem pool %s ioctl(PMEM_GET_SIZE) is %ld",
				pmem_pool,
				mSize.len);

		completeInitialization();
	}
}


QualcommCameraHardware::PmemPool::PmemPool(const char *pmem_pool,
		int camera_control_fd,
		msm_pmem_t pmem_type,
		int buffer_size, int num_buffers,
		int frame_size,
		int frame_offset,
		const char *name, int flag) :
	QualcommCameraHardware::MemPool(buffer_size,
			num_buffers,
			frame_size,
			frame_offset,
			name),
	mPmemType(pmem_type),
	mCameraControlFd(camera_control_fd)
{
	LOGD("constructing MemPool %s backed by pmem pool %s: "
			"%d frames @ %d bytes, offset %d, buffer size %d",
			mName,
			pmem_pool, num_buffers, frame_size, frame_offset,
			buffer_size);

	/*
	   if( mCameraControlFd == 0 ) {
	   LOGD("duped FD is 0 , dup again ") ;
	   mCameraControlFd = dup(camera_control_fd);
	   }
	   LOGD("%s: duplicating control fd %d --> %d",
	   __FUNCTION__,
	   camera_control_fd, mCameraControlFd);
	   */
	// Make a new mmap'ed heap that can be shared across processes.

	sp<MemoryHeapBase> masterHeap;
	sp<MemoryHeapPmem> pmemHeap;

	ptypeflag = 0;

	buffer_size = clp2(buffer_size);
	for (int i = 0; i < num_buffers; i++) {
		masterHeap = new MemoryHeapBase(pmem_pool, buffer_size, 0);
		pmemHeap = new MemoryHeapPmem(masterHeap, 0);
		LOGE("pmemheap: id = %d base = %x", (int)pmemHeap->getHeapID(), (unsigned int)pmemHeap->base());
		if (pmemHeap->getHeapID() >= 0) {
			pmemHeap->slap();
			masterHeap.clear();
			mHeapnew[i] = pmemHeap;
			pmemHeap.clear();

			mFd = mHeapnew[i]->getHeapID();
			if (::ioctl(mFd, PMEM_GET_SIZE, &mSize)) {
				LOGE("pmem pool %s ioctl(PMEM_GET_SIZE) error %s (%d)",
						pmem_pool,
						::strerror(errno), errno);
				mHeapnew[i].clear();
				return;
			}

			LOGE("pmem pool %s ioctl(PMEM_GET_SIZE) is %ld",
					pmem_pool,
					mSize.len);
		}
		else {
			LOGE("pmem pool %s error: could not create master heap!", pmem_pool);
		}
	}
	completeInitializationnew();

	// Register preview buffers with the camera drivers.
	/*for (int cnt = 0; cnt < num_buffers; ++cnt) {
	  register_buf(mCameraControlFd,
	  buffer_size,
	  mHeap->getHeapID(),
	  buffer_size * cnt,
	  (uint8_t *)mHeap->base() + buffer_size * cnt,
	  pmem_type, cnt == ( num_buffers - 1 ) ? false : true );
	  }*/
}

QualcommCameraHardware::PmemPool::~PmemPool()
{
	LOGD("%s: %s E", __FUNCTION__, mName);
	// Unregister preview buffers with the camera drivers.
	/*for (int cnt = 0; cnt < mNumBuffers; ++cnt) {
	  register_buf(mCameraControlFd,
	  mBufferSize,
	  mHeap->getHeapID(),
	  mBufferSize * cnt,
	  (uint8_t *)mHeap->base() + mBufferSize * cnt,
	  mPmemType,
	  true, false);
	  }*/
	LOGD("destroying PmemPool %s: ",
			mName);
	//close(mCameraControlFd);
	LOGD("%s: %s X", __FUNCTION__, mName);
}

QualcommCameraHardware::MemPool::~MemPool()
{
	LOGD("destroying MemPool %s", mName);
	if (mFrameSize > 0)
		delete [] mBuffers;
	mHeap.clear();
	LOGD("destroying MemPool %s completed", mName);
}

QualcommCameraHardware::PreviewPmemPool::PreviewPmemPool(
		int control_fd,
		int buffer_size, int num_buffers,
		int frame_size,
		int frame_offset,
		const char *name,
		int flag) :
	QualcommCameraHardware::PmemPool("/dev/pmem_adsp",
			control_fd,MSM_PMEM_OUTPUT2,
			buffer_size,
			num_buffers,
			frame_size,
			frame_offset,
			name, 1)
{
	LOGV("constructing PreviewPmemPool");
	if (initialized()) {
		//NOTE : SOME PREVIEWPMEMPOOL SPECIFIC CODE MAY BE ADDED
	}
}
QualcommCameraHardware::PreviewPmemPool::PreviewPmemPool(
		int control_fd,
		int buffer_size, int num_buffers,
		int frame_size,
		int frame_offset,
		const char *name) :
	QualcommCameraHardware::PmemPool("/dev/pmem_adsp",control_fd,MSM_PMEM_OUTPUT2,
			buffer_size,
			num_buffers,
			frame_size,
			frame_offset,
			name,1)
{
	LOGD("QualcommCameraHardware::PreviewPmemPool::PreviewPmemPool");
	LOGV("constructing PreviewPmemPool");
	if (initialized()) {
		//NOTE : SOME PREVIEWPMEMPOOL SPECIFIC CODE MAY BE ADDED
	}
}

QualcommCameraHardware::PreviewPmemPool::~PreviewPmemPool()
{
	LOGV("destroying PreviewPmemPool");
	if(initialized()) {
		LOGV("destroying PreviewPmemPool");
	}
}

static bool register_buf(int camfd,
		int size,
		int pmempreviewfd,
		uint32_t offset,
		uint8_t *buf,
		msm_pmem_t pmem_type,
		bool active ,
		bool register_buffer)
{
	struct msm_pmem_info_t pmemBuf;

	LOGD("register_buf E" ) ;
	pmemBuf.type     = pmem_type;
	pmemBuf.fd       = pmempreviewfd;
	//pmemBuf.offset   = offset;
	//pmemBuf.len      = size;
	pmemBuf.vaddr    = buf ;
	pmemBuf.y_off    = 0;
	//pmemBuf.cbcr_off = size * 2 / 3; //PAD_TO_WORD(size * 2 / 3);
	//pmemBuf.vfe_can_write   = true;
	pmemBuf.active   = active ;

	if( pmem_type == MSM_PMEM_RAW_MAINIMG )
		pmemBuf.cbcr_off = 0;
	else
		pmemBuf.cbcr_off = ((size * 2 / 3)+1) & ~1 ;


	/*LOGD("register_buf: camfd = %d, pmemfd = %d, reg = %d buffer = %p offset = %d, size = %d, cbrc_off = %d",
	  camfd, pmempreviewfd, !register_buffer, buf, offset, size, pmemBuf.cbcr_off );*/
	if (ioctl(camfd,
				register_buffer ?
				MSM_CAM_IOCTL_REGISTER_PMEM :
				MSM_CAM_IOCTL_UNREGISTER_PMEM,
				&pmemBuf) < 0) {
		LOGE("register_buf: MSM_CAM_IOCTL_(UN)REGISTER_PMEM fd %d error %s",
				camfd,
				strerror(errno));
		return false;
	}
	LOGD("register_buf X") ;
	return true;
}

status_t QualcommCameraHardware::MemPool::dump(int fd, const Vector<String16>& args) const
{
	const size_t SIZE = 256;
	char buffer[SIZE];
	String8 result;
	snprintf(buffer, 255, "QualcommCameraHardware::AshmemPool::dump\n");
	result.append(buffer);
	if (mName) {
		snprintf(buffer, 255, "mem pool name (%s)\n", mName);
		result.append(buffer);
	}
	if (mHeap != 0) {
		snprintf(buffer, 255, "heap base(%p), size(%d), flags(%d), device(%s)\n",
				mHeap->getBase(), mHeap->getSize(),
				mHeap->getFlags(), mHeap->getDevice());
		result.append(buffer);
	}
	snprintf(buffer, 255, "buffer size (%d), number of buffers (%d),"
			" frame size(%d), and frame offset(%d)\n",
			mBufferSize, mNumBuffers, mFrameSize, mFrameOffset);
	result.append(buffer);
	write(fd, result.string(), result.size());
	return NO_ERROR;
}

static void receive_camframe_callback(struct msm_frame_t *frame)
{
	sp<QualcommCameraHardware> obj = QualcommCameraHardware::getInstance();
	if (obj != 0) {
		obj->receivePreviewFrame(frame);
	}
}

static void receive_jpeg_fragment_callback(uint8_t *buff_ptr, uint32_t buff_size)
{
	LOGD("receive_jpeg_fragment_callback E");
	sp<QualcommCameraHardware> obj = QualcommCameraHardware::getInstance();
	if (obj != 0) {
		obj->receiveJpegPictureFragment(buff_ptr, buff_size);
	}
	LOGD("receive_jpeg_fragment_callback X");
}

static void receive_jpeg_callback(jpeg_event_t status)
{
	LOGD("receive_jpeg_callback E (completion status %d)", status);
	if (status == JPEG_EVENT_DONE) {
		sp<QualcommCameraHardware> obj = QualcommCameraHardware::getInstance();
		if (obj != 0) {
			obj->receiveJpegPicture();
		}
	}
	LOGD("receive_jpeg_callback X");
}

status_t QualcommCameraHardware::sendCommand(int32_t command, int32_t arg1,
		int32_t arg2)
{
	LOGD("sendCommand: %d %d %d (stub)", command, arg1, arg2);
	return BAD_VALUE;
}




static void dump_to_file(const char *fname,
		uint8_t *buf, uint32_t size)
{
	int nw, cnt = 0;
	uint32_t written = 0;

	LOGD("opening file [%s]\n", fname);
	int fd = open(fname, O_RDWR | O_CREAT);
	if (fd < 0) {
		LOGE("failed to create file [%s]: %s", fname, strerror(errno));
		return;
	}

	LOGD("writing %d bytes to file [%s]\n", size, fname);
	while (written < size) {
		nw = ::write(fd,
				buf + written,
				size - written);
		if (nw < 0) {
			LOGE("failed to write to file [%s]: %s",
					fname, strerror(errno));
			break;
		}
		written += nw;
		cnt++;
	}
	LOGD("done writing %d bytes to file [%s] in %d passes\n",
			size, fname, cnt);
	::close(fd);
}
}; // namespace android
