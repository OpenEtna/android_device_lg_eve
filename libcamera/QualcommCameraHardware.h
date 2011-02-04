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

#ifndef ANDROID_HARDWARE_QUALCOMM_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_QUALCOMM_CAMERA_HARDWARE_H

#include <camera/CameraHardwareInterface.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <stdint.h>

//extern "C" {
#include <linux/android_pmem.h>
#include "msm_camera.h"
//}

#define MSM_CAMERA_CONTROL "/dev/msm_camera/msm_camera0"

#define JPEG_EVENT_DONE 0 /* guess */

#define CAM_CTRL_SUCCESS 1

#define CAMERA_SET_PARM_DIMENSION 1
#define CAMERA_STOP_PREVIEW 38
#define CAMERA_START_PREVIEW 39
#define CAMERA_EXIT 43


#define CAMERA_START_SNAPSHOT 41 /* orig 40 on donut kernel logs same to be 41 */
#define CAMERA_STOP_SNAPSHOT 42 /* guess, but likely based on previos ording */
#if 0
typedef enum
{
	CAMERA_RSP_CB_SUCCESS,    /* Function is accepted         */
	CAMERA_EXIT_CB_DONE,      /* Function is executed         */
	CAMERA_EXIT_CB_FAILED,    /* Execution failed or rejected */
	CAMERA_EXIT_CB_DSP_IDLE,  /* DSP is in idle state         */
	CAMERA_EXIT_CB_DSP_ABORT, /* Abort due to DSP failure     */
	CAMERA_EXIT_CB_ABORT,     /* Function aborted             */
	CAMERA_EXIT_CB_ERROR,     /* Failed due to resource       */
	CAMERA_EVT_CB_FRAME,      /* Preview or video frame ready */
	CAMERA_EVT_CB_PICTURE,    /* Picture frame ready for multi-shot */
	CAMERA_STATUS_CB,         /* Status updated               */
	CAMERA_EXIT_CB_FILE_SIZE_EXCEEDED, /* Specified file size not achieved,
										  encoded file written & returned anyway */
	CAMERA_EXIT_CB_BUFFER,    /* A buffer is returned         */
	CAMERA_EVT_CB_SNAPSHOT_DONE,/*  Snapshot updated               */
	CAMERA_CB_MAX
} camera_cb_type;
#endif
typedef struct
{
	uint32_t timestamp;  /* seconds since 1/6/1980          */
	double   latitude;   /* degrees, WGS ellipsoid */
	double   longitude;  /* degrees                */
	int16_t  altitude;   /* meters                          */
} camera_position_type;

typedef struct
{
	unsigned int in1_w;
	unsigned int in1_h;
	unsigned int out1_w;
	unsigned int out1_h;
	unsigned int in2_w;
	unsigned int in2_h;
	unsigned int out2_w;
	unsigned int out2_h;
	uint8_t update_flag; 
} common_crop_t;

typedef struct
{
	unsigned short picture_width;
	unsigned short picture_height;
	unsigned short display_width;
	unsigned short display_height;
	unsigned short filler;
	unsigned short filler2;
	unsigned short ui_thumbnail_height;
	unsigned short ui_thumbnail_width;
	unsigned short filler3;
	unsigned short filler4;
	unsigned short filler5;
	unsigned short filler6;
	unsigned short filler7;
	unsigned short filler8;
} cam_ctrl_dimension_t;

typedef uint8_t cam_ctrl_type;
typedef uint8_t jpeg_event_t;
typedef unsigned int isp3a_af_mode_t;

struct str_map {
	const char *const desc;
	int val;
};

namespace android {
	unsigned char m4mo_read_8bit( int fd, char category, char byte )  ;
	void m4mo_write_8bit( int fd, char category, char byte, char value ) ;

	class QualcommCameraHardware : public CameraHardwareInterface {
		public:

			virtual sp<IMemoryHeap> getPreviewHeap() const;
			virtual sp<IMemoryHeap> getPreviewHeap(int32_t i) const;
			virtual sp<IMemoryHeap> getRawHeap() const;
			virtual void setCallbacks(notify_callback notify_cb,
					data_callback data_cb,
					data_callback_timestamp data_cb_timestamp,
					void* user);
#ifdef USE_GETBUFFERINFO
			status_t getBufferInfo(sp<IMemory>& Frame, size_t *alignedSize);
#endif
			virtual void enableMsgType(int32_t msgType);
			virtual void disableMsgType(int32_t msgType);
			virtual bool msgTypeEnabled(int32_t msgType);

			virtual status_t dump(int fd, const Vector<String16>& args) const;
			virtual status_t startPreview();
			virtual void stopPreview();
			virtual bool previewEnabled();
			virtual status_t startRecording();
			virtual void stopRecording();
			virtual bool recordingEnabled();
			virtual void releaseRecordingFrame(const sp<IMemory>& mem);
			virtual status_t autoFocus();
			virtual status_t takePicture();
			virtual status_t cancelPicture();
			virtual status_t setParameters(const CameraParameters& params);
			virtual CameraParameters getParameters() const;
			virtual status_t sendCommand(int32_t command, int32_t arg1, int32_t arg2);
			virtual void release();
			virtual status_t cancelAutoFocus();

			static sp<CameraHardwareInterface> createInstance();
			static sp<QualcommCameraHardware> getInstance();

			void receivePreviewFrame(struct msm_frame_t *frame);
			void receiveJpegPicture(void);
			void jpeg_set_location();
			void receiveJpegPictureFragment(uint8_t *buf, uint32_t size);
			void notifyShutter();

		private:
			QualcommCameraHardware();
			virtual ~QualcommCameraHardware();
			status_t startPreviewInternal();
			void stopPreviewInternal();
			friend void *auto_focus_thread(void *user);
			void runAutoFocus();
			bool native_set_dimension (int camfd);
			bool native_jpeg_encode (void);
			int native_set_cfg(sensor_cfg_t cfgtype, int8_t data);
			bool native_set_parm(cam_ctrl_type type, uint16_t length, void *value);
			bool native_set_dimension(cam_ctrl_dimension_t *value);
			int getParm(const char *parm_str, const str_map *parm_map);

			static wp<QualcommCameraHardware> singleton;

			/* These constants reflect the number of buffers that libmmcamera requires
			   for preview and raw, and need to be updated when libmmcamera
			   changes.
			   */
			static const int kPreviewBufferCount = 4;
			static const int kRawBufferCount = 1;
			static const int kJpegBufferCount = 1;
			static const int kRawFrameHeaderSize = 0 ;

			//TODO: put the picture dimensions in the CameraParameters object;
			CameraParameters mParameters;
			int mPreviewHeight;
			int mPreviewWidth;
			int mRawHeight;
			int mRawWidth;
			int mRawHeightC;
			int mRawWidthC;
			unsigned int frame_size;
			bool mCameraRunning;
			bool mPreviewInitialized;
			bool mRawInitialized ;

			// This class represents a heap which maintains several contiguous
			// buffers.  The heap may be backed by pmem (when pmem_pool contains
			// the name of a /dev/pmem* file), or by ashmem (when pmem_pool == NULL).

			struct MemPool : public RefBase {
				MemPool(int buffer_size, int num_buffers,
						int frame_size,
						int frame_offset,
						const char *name);

				virtual ~MemPool() = 0;

				void completeInitialization();
				void completeInitializationnew();

				bool initialized() const {
					return ((mHeapnew[3] != NULL && mHeapnew[3]->base() != MAP_FAILED) ||
							(mHeap != NULL && mHeap->base() != MAP_FAILED));        

				}

				virtual status_t dump(int fd, const Vector<String16>& args) const;

				int mBufferSize;
				int mNumBuffers;
				int mFrameSize;
				int mFrameOffset;
				sp<MemoryHeapBase> mHeap;
				sp<MemoryHeapBase> mHeapnew[4];
				sp<MemoryBase> *mBuffers;

				const char *mName;
			};

			struct AshmemPool : public MemPool {
				AshmemPool(int buffer_size, int num_buffers,
						int frame_size,
						int frame_offset,
						const char *name);
			};

			struct PmemPool : public MemPool {
				PmemPool(const char *pmem_pool,
						int control_camera_fd, msm_pmem_t pmem_type,
						int buffer_size, int num_buffers,
						int frame_size, int frame_offset,
						const char *name);      
				PmemPool(const char *pmem_pool,
						int control_camera_fd, msm_pmem_t pmem_type,
						int buffer_size, int num_buffers,
						int frame_size, int frame_offset,
						const char *name,
						int flag);
				virtual ~PmemPool();
				int mFd;
				msm_pmem_t mPmemType;
				int mCameraControlFd;
				uint32_t mAlignedSize;
				struct pmem_region mSize;
				int ptypeflag ;
			};

			struct PreviewPmemPool : public PmemPool {
				virtual ~PreviewPmemPool();
				PreviewPmemPool(int control_fd, int buffer_size, int num_buffers,
						int frame_size,
						int frame_offset,
						const char *name);
				PreviewPmemPool(int control_fd, int buffer_size, int num_buffers,
						int frame_size,
						int frame_offset,
						const char *name,
						int flag);
			};

			sp<PreviewPmemPool> mPreviewHeap;
			sp<PmemPool> mThumbnailHeap;
			sp<PmemPool> mRawHeap;
			sp<AshmemPool> mJpegHeap;

			void startCamera();
			bool initPreview();
			void deinitPreview();
			bool initRaw(bool initJpegHeap);
			void deinitRaw();

			void setLensToBasePosition() ;

			void m4mo_write_8bit( char category, char byte, char value ) ;	    
			unsigned char m4mo_read_8bit( char category, char byte ) ;
			void m4mo_get_firmware_version() ;
			friend void *jpeg_encoder_thread( void *user ) ;
			void runJpegEncodeThread(void *data) ;

			bool mFrameThreadRunning;
			Mutex mFrameThreadWaitLock;
			Condition mFrameThreadWait;
			friend void *frame_thread(void *user);
			void runFrameThread(void *data);

			bool mShutterPending;
			Mutex mShutterLock;

			bool mSnapshotThreadRunning;
			Mutex mSnapshotThreadWaitLock;
			Condition mSnapshotThreadWait;
			friend void *snapshot_thread(void *user);
			void runSnapshotThread(void *data);

			void initDefaultParameters();

			void setAntibanding();
			void setEffect();
			void setWhiteBalance();
			void setSceneMode();
			void setFlashMode();

			Mutex mLock;
			bool mReleasedRecordingFrame;

			void receiveRawPicture(void);

			Mutex mAFLock;

			Mutex mRecordLock;
			Mutex mRecordFrameLock;
			Condition mRecordWait;
			Condition mStateWait;

			/* mJpegSize keeps track of the size of the accumulated JPEG.  We clear it
			   when we are about to take a picture, so at any time it contains either
			   zero, or the size of the last JPEG picture taken.
			   */
			uint32_t mJpegSize;

			notify_callback    mNotifyCb;
			data_callback      mDataCb;
			data_callback_timestamp mDataCbTimestamp;
			void               *mCallbackCookie;

			int32_t             mMsgEnabled;

			unsigned int        mPreviewFrameSize;
			int                 mRawSize;
			int                 mJpegMaxSize;

#if DLOPEN_LIBMMCAMERA
			//void *libmmcamera;
			void *libmmcamera_target;
#endif

			int mCameraControlFd;
			cam_ctrl_dimension_t mDimension;
			cam_ctrl_dimension_t mDimensionC;

			bool mAutoFocusThreadRunning;
			Mutex mAutoFocusThreadLock;

			pthread_t mCamConfigThread;
			pthread_t mFrameThread;
			pthread_t mSnapshotThread;

			common_crop_t mCrop;

			struct msm_frame_t frames[kPreviewBufferCount];
			bool mInPreviewCallback;
			bool mCameraRecording;
	};

}; // namespace android

#endif
