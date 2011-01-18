/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Sensors"

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>

#include <linux/input.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include "akm8973.h"

/*****************************************************************************/

#define LOGD

#define ZERO_TEMP			80.0f

#define AKM_DEVICE_NAME             "/dev/akm8973_aot"

// sensor IDs must be a power of two and
// must match values in SensorManager.java
#define EVENT_TYPE_ACCEL_X          ABS_Y
#define EVENT_TYPE_ACCEL_Y          ABS_X
#define EVENT_TYPE_ACCEL_Z          ABS_Z
#define EVENT_TYPE_ACCEL_STATUS     ABS_WHEEL

#define EVENT_TYPE_YAW              ABS_RX
#define EVENT_TYPE_PITCH            ABS_RY
#define EVENT_TYPE_ROLL             ABS_RZ
#define EVENT_TYPE_ORIENT_STATUS    ABS_RUDDER

/*** AKEMD ATTENTION! To adjust Android ***/
/*** cordination, SWAP X and Y axis.    ***/
#if 0 /*original*/
#define EVENT_TYPE_MAGV_X           ABS_HAT0X
#define EVENT_TYPE_MAGV_Y           ABS_HAT0Y
#define EVENT_TYPE_MAGV_Z           ABS_BRAKE
#else /*Changing for Google Sky Maps*/
#define EVENT_TYPE_MAGV_X           ABS_HAT0Y
#define EVENT_TYPE_MAGV_Y           ABS_HAT0X
#define EVENT_TYPE_MAGV_Z           ABS_BRAKE
#endif

#define EVENT_TYPE_PROXIMITY		ABS_DISTANCE

#define EVENT_TYPE_TEMPERATURE      ABS_THROTTLE
#define EVENT_TYPE_STEP_COUNT       ABS_GAS

// conversion of acceleration data to SI units (m/s^2)
#define CONVERT_A                   (GRAVITY_EARTH/128.0f)
#define CONVERT_A_X                 (-CONVERT_A)
#define CONVERT_A_Y                 (-CONVERT_A)
#define CONVERT_A_Z                 (CONVERT_A)

// conversion of magnetic data to uT units
#if 0 /*original*/
#define CONVERT_M                   (1.0f/16.0f)
#define CONVERT_M_X                 (-CONVERT_M)
#define CONVERT_M_Y                 (-CONVERT_M)
#define CONVERT_M_Z                 (CONVERT_M)
#else /*Changing for Google Sky Maps */
#define CONVERT_M                   (1.0f/16.0f)
#define CONVERT_M_X                 (-CONVERT_M)
#define CONVERT_M_Y                 (-CONVERT_M)
#define CONVERT_M_Z                 (-CONVERT_M)
#endif
#define CONVERT_O                   (1.0f/64.0f)
#define CONVERT_O_Y                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (-CONVERT_O)

enum {
    ID_O = 0,
    ID_A,
    ID_T,
    ID_M,
    ID_P,
    ID_OR,
    ID_L,
    MAX_NUM_SENSORS
};

#define SENSORS_AKM					(SENSORS_ORIENTATION_RAW | SENSORS_ORIENTATION | SENSORS_MAGNETIC_FIELD | SENSORS_TEMPERATURE)
#define SENSORS_BMA					(SENSORS_ORIENTATION_RAW | SENSORS_ORIENTATION | SENSORS_ACCELERATION)

#define SENSORS_ORIENTATION_RAW    (1<<ID_OR)
#define SENSORS_ORIENTATION        (1<<ID_O)
#define SENSORS_ACCELERATION       (1<<ID_A)
#define SENSORS_TEMPERATURE        (1<<ID_T)
#define SENSORS_MAGNETIC_FIELD     (1<<ID_M)
#define SENSORS_PROXIMITY		   (1<<ID_P)
#define SENSORS_LIGHT		       (1<<ID_L)

/*****************************************************************************/

static int sensors_control_device_close(struct hw_device_t *dev);
static native_handle_t* sensors_control_open_data_source(struct sensors_control_device_t *dev);
static int sensors_control_activate(struct sensors_control_device_t *dev,
		int handle, int enabled);
static int sensors_control_wake(struct sensors_control_device_t *dev);

static int sensors_data_device_close(struct hw_device_t *dev);
static int sensors_data_data_open(struct sensors_data_device_t *dev, native_handle_t* nh);
static int sensors_data_data_close(struct sensors_data_device_t *dev);
static int sensors_data_poll(struct sensors_data_device_t *dev, 
		sensors_data_t* data);

static int sensors_get_sensors_list(struct sensors_module_t* module,
		struct sensor_t const**);

static int sensors_device_open(const struct hw_module_t* module, const char* name,
		struct hw_device_t** device);

/*****************************************************************************/

static int sAkmFD = -1;
static uint32_t sActiveSensors = 0;

static int id_to_sensor[MAX_NUM_SENSORS] = {
	[ID_A] = SENSOR_TYPE_ACCELEROMETER,
	[ID_M] = SENSOR_TYPE_MAGNETIC_FIELD,
	[ID_O] = SENSOR_TYPE_ORIENTATION,
	[ID_T] = SENSOR_TYPE_TEMPERATURE,
	[ID_P] = SENSOR_TYPE_PROXIMITY,
	[ID_L] = SENSOR_TYPE_LIGHT,
};

static sensors_data_t sSensors[MAX_NUM_SENSORS];
static uint32_t sPendingSensors;
static volatile int wakeup;
static int isControlProcess;

#define event_time_to_sensor_time(event) (event.time.tv_sec*1000000000LL + event.time.tv_usec*1000)
void set_data_time(sensors_data_t* data) {
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	data->time = time.tv_sec*1000000000LL +
		time.tv_nsec;
}
/*****************************************************************************/

/*
 * We use a Least Mean Squares filter to smooth out the output of the yaw
 * sensor.
 *
 * The goal is to estimate the output of the sensor based on previous acquired
 * samples.
 *
 * We approximate the input by a line with the equation:
 *      Z(t) = a * t + b
 *
 * We use the Least Mean Squares method to calculate a and b so that the
 * distance between the line and the measured COUNT inputs Z(t) is minimal.
 *
 * In practice we only need to compute b, which is the value we're looking for
 * (it's the estimated Z at t=0). However, to improve the latency a little bit,
 * we're going to discard a certain number of samples that are too far from
 * the estimated line and compute b again with the new (trimmed down) samples.
 *
 * notes:
 * 'a' is the slope of the line, and physicaly represent how fast the input
 * is changing. In our case, how fast the yaw is changing, that is, how fast the
 * user is spinning the device (in degre / nanosecond). This value should be
 * zero when the device is not moving.
 *
 * The minimum distance between the line and the samples (which we are not
 * explicitely computing here), is an indication of how bad the samples are
 * and gives an idea of the "quality" of the estimation (well, really of the
 * sensor values).
 *
 */

/* sensor rate in me */
#define SENSORS_RATE_MS     20
/* timeout (constant value) in ms */
#define SENSORS_TIMEOUT_MS  100
/* # of samples to look at in the past for filtering */
#define COUNT               24
/* prediction ratio */
#define PREDICTION_RATIO    (1.0f/3.0f)
/* prediction time in seconds (>=0) */
#define PREDICTION_TIME     ((SENSORS_RATE_MS*COUNT/1000.0f)*PREDICTION_RATIO)

static float mV[COUNT*2];
static float mT[COUNT*2];
static int mIndex;

	static inline
float normalize(float x)
{
	x *= (1.0f / 360.0f);
	if (fabsf(x) >= 0.5f)
		x = x - ceilf(x + 0.5f) + 1.0f;
	if (x < 0)
		x += 1.0f;
	x *= 360.0f;
	return x;
}

static void LMSInit(void)
{
	memset(mV, 0, sizeof(mV));
	memset(mT, 0, sizeof(mT));
	mIndex = COUNT;
}

static float LMSFilter(int64_t time, int v)
{
	const float ns = 1.0f / 1000000000.0f;
	const float t = time*ns;
	float v1 = mV[mIndex];
	if ((v-v1) > 180) {
		v -= 360;
	} else if ((v1-v) > 180) {
		v += 360;
	}
	/* Manage the circular buffer, we write the data twice spaced by COUNT
	 * values, so that we don't have to memcpy() the array when it's full */
	mIndex++;
	if (mIndex >= COUNT*2)
		mIndex = COUNT;
	mV[mIndex] = v;
	mT[mIndex] = t;
	mV[mIndex-COUNT] = v;
	mT[mIndex-COUNT] = t;

	float A, B, C, D, E;
	float a, b;
	int i;

	A = B = C = D = E = 0;
	for (i=0 ; i<COUNT-1 ; i++) {
		const int j = mIndex - 1 - i;
		const float Z = mV[j];
		const float T = 0.5f*(mT[j] + mT[j+1]) - t;
		float dT = mT[j] - mT[j+1];
		dT *= dT;
		A += Z*dT;
		B += T*(T*dT);
		C +=   (T*dT);
		D += Z*(T*dT);
		E += dT;
	}
	b = (A*B + C*D) / (E*B + C*C);
	a = (E*b - A) / C;
	float f = b + PREDICTION_TIME*a;

	LOGD("A=%f, B=%f, C=%f, D=%f, E=%f", A,B,C,D,E);
	LOGD("%lld  %d  %f  %f", time, v, f, a);

	f = normalize(f);
	return f;
}

/*****************************************************************************/

static int open_input(char* iname)
{
	/* scan all input drivers and look for "compass" */
	int fd = -1;
	const char *dirname = "/dev/input";
	char devname[PATH_MAX];
	char *filename;
	DIR *dir;
	struct dirent *de;
	dir = opendir(dirname);
	if(dir == NULL)
		return -1;
	strcpy(devname, dirname);
	filename = devname + strlen(devname);
	*filename++ = '/';
	while((de = readdir(dir))) {
		if(de->d_name[0] == '.' &&
				(de->d_name[1] == '\0' ||
				 (de->d_name[1] == '.' && de->d_name[2] == '\0')))
			continue;
		strcpy(filename, de->d_name);
		fd = open(devname, O_RDONLY);
		if (fd>=0) {
			char name[PATH_MAX];
			if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
				name[0] = '\0';
			}
			if (!strcmp(name, iname)) {
				closedir(dir);
				return fd;
			}
			close(fd);
		} else {
			LOGE("Failed to open %d", devname);
		}
	}
	closedir(dir);
	return -1;
}

static int open_akm()
{
	LOGD("open_akm");

	if (sAkmFD <= 0) {
		sAkmFD = open(AKM_DEVICE_NAME, O_RDONLY);
		LOGD("%s, fd=%d", __PRETTY_FUNCTION__, sAkmFD);
		LOGE_IF(sAkmFD<0, "Couldn't open %s (%s)",
				AKM_DEVICE_NAME, strerror(errno));
		if (sAkmFD >= 0) {
			sActiveSensors = 0;
		}
	}
	return sAkmFD;
}

static void close_akm()
{
	LOGD("close_akm");

	if (sAkmFD > 0) {
		LOGE("%s, fd=%d", __PRETTY_FUNCTION__, sAkmFD);
		close(sAkmFD);
		sAkmFD = -1;
	}
}

static void enable_disable(int fd, uint32_t sensors, uint32_t mask)
{
	if (fd<0) return;
	short flags;
	LOGD("enable_disable");
	if (sensors & SENSORS_ORIENTATION_RAW) {
		sensors |= SENSORS_ORIENTATION;
		mask |= SENSORS_ORIENTATION;
	} else if (mask & SENSORS_ORIENTATION_RAW) {
		mask |= SENSORS_ORIENTATION;
	}

	if (mask & SENSORS_ORIENTATION) {
		flags = (sensors & SENSORS_ORIENTATION) ? 1 : 0;
		if (ioctl(fd, ECS_IOCTL_APP_SET_MFLAG, &flags) < 0) {
			LOGE("ECS_IOCTL_APP_SET_MFLAG error (%s)", strerror(errno));
		}
	}
	if (mask & SENSORS_ACCELERATION) {
		flags = (sensors & SENSORS_ACCELERATION) ? 1 : 0;
		if (ioctl(fd, ECS_IOCTL_APP_SET_AFLAG, &flags) < 0) {
			LOGE("ECS_IOCTL_APP_SET_AFLAG error (%s)", strerror(errno));
		}
	}
	if (mask & SENSORS_TEMPERATURE) {
		flags = (sensors & SENSORS_TEMPERATURE) ? 1 : 0;
		if (ioctl(fd, ECS_IOCTL_APP_SET_TFLAG, &flags) < 0) {
			LOGE("ECS_IOCTL_APP_SET_TFLAG error (%s)", strerror(errno));
		}
	}
#if 1 /*def ECS_IOCTL_APP_SET_MVFLAG	*/
	if (mask & SENSORS_PROXIMITY) {
		flags = (sensors & SENSORS_PROXIMITY) ? 1 : 0;
		if (ioctl(fd, ECS_IOCTL_APP_SET_PFLAG, &flags) < 0) {
			LOGE("ECS_IOCTL_APP_SET_TFLAG error (%s)", strerror(errno));
		}
	}
#endif	
#ifdef ECS_IOCTL_APP_SET_MVFLAG
	if (mask & SENSORS_MAGNETIC_FIELD) {
		flags = (sensors & SENSORS_MAGNETIC_FIELD) ? 1 : 0;
		if (ioctl(fd, ECS_IOCTL_APP_SET_MVFLAG, &flags) < 0) {
			LOGE("ECS_IOCTL_APP_SET_MVFLAG error (%s)", strerror(errno));
		}
	}
#endif
}

static uint32_t read_sensors_state(int fd)
{
	if (fd<0) return 0;
	short flags;
	uint32_t sensors = 0;
	LOGE("read_sensors_state");
	// read the actual value of all sensors
	if (!ioctl(fd, ECS_IOCTL_APP_GET_MFLAG, &flags)) {
		if (flags)  sensors |= SENSORS_ORIENTATION;
		else        sensors &= ~SENSORS_ORIENTATION;
	}
	if (!ioctl(fd, ECS_IOCTL_APP_GET_AFLAG, &flags)) {
		if (flags)  sensors |= SENSORS_ACCELERATION;
		else        sensors &= ~SENSORS_ACCELERATION;
	}
	if (!ioctl(fd, ECS_IOCTL_APP_GET_TFLAG, &flags)) {
		if (flags)  sensors |= SENSORS_TEMPERATURE;
		else        sensors &= ~SENSORS_TEMPERATURE;
	}
	/*
	   if (!ioctl(fd, ECS_IOCTL_APP_GET_PFLAG, &flags)) {
	   if (flags)  sensors |= SENSORS_PROXIMITY;
	   else        sensors &= ~SENSORS_PROXIMITY;
	   }*/
#ifdef ECS_IOCTL_APP_SET_MVFLAG
	if (!ioctl(fd, ECS_IOCTL_APP_GET_MVFLAG, &flags)) {
		if (flags)  sensors |= SENSORS_MAGNETIC_FIELD;
		else        sensors &= ~SENSORS_MAGNETIC_FIELD;
	}
#endif
	sensors |= SENSORS_PROXIMITY;

	return sensors;
}

/*****************************************************************************/
#define SUCCEED(...) if (! (__VA_ARGS__))  \
	LOGE("%s:%d expression '%s' failed: %s", __FILE__, __LINE__, #__VA_ARGS__, strerror(errno));
static int akm_fd = -1;
static int akm_input_fd = -1;
static int bma_input_fd = -1;
static int prox_input_fd = -1;

void calibrate_analog_apply()
{
	int i;
	char fixed_magnetometer_gain[] = { 7, 7, 7 };
	char akm_analog_offset[] = {0x07, 0x8e, 0x99};
	char params[6] = {
		akm_analog_offset[0], akm_analog_offset[1], akm_analog_offset[2],
		fixed_magnetometer_gain[0], fixed_magnetometer_gain[1], fixed_magnetometer_gain[2],
	};

	for (i = 0; i < 6; i ++) {
		char rwbuf[5] = { 2, 0xe1+i, params[i] };
		SUCCEED(ioctl(akm_fd, ECS_IOCTL_WRITE, &rwbuf) == 0);
	}

	//digital_gain = powf(10.0f, (magnetometer_gain - fixed_magnetometer_gain) * 0.4f / 20.0f) * 16.0f;

	struct timespec interval;
	interval.tv_sec = 0;
	interval.tv_nsec = 300000;
	SUCCEED(nanosleep(&interval, NULL) == 0);
}

static void akmd_open() {
	if( akm_fd != -1 )
		return;

	LOGI("opening akmd");
	akm_fd = open("/dev/akm8973_daemon", O_RDONLY);
	SUCCEED(akm_fd != -1);
	SUCCEED(ioctl(akm_fd, ECS_IOCTL_RESET, NULL) == 0);
	calibrate_analog_apply();
}

static int midx = 0;
static float m[6];
void akm_measure() {
	/* Measuring puts readable state to 0. It is going to take
	 * some time before the values are ready. Not using SET_MODE
	 * because it contains mdelay(1) which makes measurements spin CPU! */
	char akm_data[5] = { 2, AKECS_REG_MS1, AKECS_MODE_MEASURE };
	SUCCEED(ioctl(akm_fd, ECS_IOCTL_WRITE, &akm_data) == 0);
#if 0
	/* Sleep for 300 us, which is the measurement interval. */ 
	struct timespec interval;
	interval.tv_sec = 0;
	interval.tv_nsec = 300000;
	SUCCEED(nanosleep(&interval, NULL) == 0);

	/* Significance and range of values can be extracted from
	 * online AK 8973 manual. The kernel driver just passes the data on. */
	SUCCEED(ioctl(akm_fd, ECS_IOCTL_GETDATA, &akm_data) == 0);

	float temperature = (176.0f - (unsigned char)akm_data[1])/16.0f*10.0f ;
	m[midx] = 127 - (unsigned char) akm_data[2];
	m[midx+1] = 127 - (unsigned char) akm_data[3];
	m[midx+2] = 127 - (unsigned char) akm_data[4];
	float mag[3] = { (m[0] + m[3])/2, (m[1] + m[4])/2, (m[2] + m[5])/2 };
	LOGI("akm_measure: t: %f (%d), m: %f %f %f, mag %f %f %f", temperature, akm_data[1], m[midx], m[midx+1], m[midx+2], mag[0],mag[1],mag[2]);
	midx = (midx + 3) % 6;


	sPendingSensors |= SENSORS_TEMPERATURE;
	sSensors[ID_T].temperature = temperature;
	set_data_time(&sSensors[ID_T]);

	sPendingSensors |= SENSORS_MAGNETIC_FIELD;
	sSensors[ID_M].magnetic.x = mag[0] * CONVERT_M_X;
	sSensors[ID_M].magnetic.x = mag[1] * CONVERT_M_Y;
	sSensors[ID_M].magnetic.x = mag[2] * CONVERT_M_Z;
	set_data_time(&sSensors[ID_M]);

	//m = mbuf[0].add(mbuf[1]).multiply(0.5f);
	//calibrate_magnetometer_analog();
	//calibrate();
#endif
}

static native_handle_t* sensors_control_open_data_source(struct sensors_control_device_t *dev)
{
	LOGI("sensors_control_open_data_source");
	isControlProcess = 1;
	native_handle_t* nh = native_handle_create(3, 0);
	nh->data[0] = open_input("compass");
	nh->data[1] = open_input("bma150");
	nh->data[2] = open_input("gp2ap002");
	return nh;
}


static int sensors_control_activate(struct sensors_control_device_t *dev,
		int handle, int enable)
{
	uint32_t sensor = 1 << (handle - SENSORS_HANDLE_BASE);
	uint32_t new_sensors = enable ? (sActiveSensors | sensor) : (sActiveSensors & ~sensor);
	uint32_t changed = sActiveSensors ^ new_sensors;

	LOGI("%s old sensors=%08x sensor=%08x enable=%d changed=%d", __FUNCTION__,
			sActiveSensors, sensor, enable, !!changed);

	if(!changed) {
		return 0;
	}
	sActiveSensors = new_sensors;
#if 0 /* is done in data_device */
	if((sActiveSensors & SENSORS_AKM) && (akm_input_fd == -1)) {
		akmd_open();
		akm_input_fd = open_input("compass");
	} else if(!(sActiveSensors & SENSORS_AKM) && akm_input_fd != -1) {
		close(akm_fd);
		close(akm_input_fd);
		akm_fd = -1;
		akm_input_fd = -1;
	}

	if((sActiveSensors & SENSORS_BMA) && (bma_input_fd == -1))
		bma_input_fd = open_input("bma150");
	else if(!(sActiveSensors & SENSORS_BMA) && bma_input_fd != -1) {
		close(bma_input_fd);
		bma_input_fd = -1;
	}

	if((sActiveSensors & SENSORS_PROXIMITY) && (prox_input_fd == -1))
		prox_input_fd = open_input("gp2ap002");
	else if(!(sActiveSensors & SENSORS_PROXIMITY) && (prox_input_fd != -1)) {
		close(prox_input_fd);
		prox_input_fd = -1;
	}
#endif
	return 0;
}

static int sensors_control_delay(struct sensors_control_device_t *dev, int32_t ms)
{
	LOGD("sensors_control_delay");
	return 0;
}


/*****************************************************************************/

struct hw_module_methods_t sensors_module_methods = {
open: sensors_device_open
};

const struct sensors_module_t HAL_MODULE_INFO_SYM = {
common: {
tag: HARDWARE_MODULE_TAG,
	 version_major: 1,
	 version_minor: 0,
	 id: SENSORS_HARDWARE_MODULE_ID,
	 name : "AK8973 Compass module",
	 author : "Asahi Kasei Corp.",
	 methods: &sensors_module_methods,
		},
get_sensors_list: sensors_get_sensors_list,
};

struct sensor_t sensors_descs[] = {
	{
name : "AK8973 Magnetic Field",
	   vendor : "Asahi Kasei Corp.",
	   version : 1,
	   handle : SENSORS_HANDLE_BASE+ID_M,
	   type : SENSOR_TYPE_MAGNETIC_FIELD,
	   maxRange : 1.0,
	   resolution : 1,
	   power : 20,
	},
	{
name : "AK8973 Compass",
	   vendor : "Asahi Kasei Corp.",
	   version : 1,
	   handle : SENSORS_HANDLE_BASE+ID_O,
	   type : SENSOR_TYPE_ORIENTATION,
	   maxRange : 1.0,
	   resolution : 1,
	   power : 20,
	},
	{
name : "AK8973 Compass Raw",
	   vendor : "Asahi Kasei Corp.",
	   version : 1,
	   handle : SENSORS_HANDLE_BASE+ID_OR,
	   type : SENSOR_TYPE_ORIENTATION,
	   maxRange : 1.0,
	   resolution : 1,
	   power : 20,
	},
	{
name : "BMA150",
	   vendor : "bma150",
	   version : 1,
	   handle : SENSORS_HANDLE_BASE+ID_A,
	   type : SENSOR_TYPE_ACCELEROMETER,
	   maxRange : 1.0,
	   resolution : 1,
	   power : 20,
	},
	{
name : "GP2AP002",
	   vendor : "gp2ap002",
	   version : 1,
	   handle : SENSORS_HANDLE_BASE+ID_P,
	   type : SENSOR_TYPE_PROXIMITY,
	   maxRange : 1.0,
	   resolution : 1,
	   power : 20,
	},
	{
name : "AK8973 Temperature",
	   vendor : "Asahi Kasei Corp.",
	   version : 1,
	   handle : SENSORS_HANDLE_BASE+ID_T,
	   type : SENSOR_TYPE_TEMPERATURE,
	   maxRange : 1.0,
	   resolution : 1,
	   power : 20,
	},
	{
name : "Brightness",
	   vendor : "-",
	   version : 1,
	   handle : SENSORS_HANDLE_BASE+ID_L,
	   type : SENSOR_TYPE_LIGHT,
	   maxRange : 15.0,
	   resolution : 1,
	   power : 20,
	},
	0,
};

/*****************************************************************************/
struct sensors_control_context_t {
	struct sensors_control_device_t device;
};

struct sensors_data_context_t {
	struct sensors_data_device_t device;
};

static int sensors_get_sensors_list(struct sensors_module_t* module,
		struct sensor_t const** plist){
	*plist = sensors_descs;
	return 7;/*4;*/ // No need to return number of sensor list
}

static int read_brightness(void) {

	int val;
	FILE *f = fopen("/sys/class/backlight/adam-bl/alc_brightness","r");

	fscanf(f,"%d",&val);
	fclose(f);
	LOGI("read_brightness = %d", val);
	return val;
}

static int sensors_device_open(const struct hw_module_t* module, const char* name,
		struct hw_device_t** device)
{
	int status = -EINVAL;
	LOGD("sensors_device_open");
	if (!strcmp(name, SENSORS_HARDWARE_CONTROL)) {
		struct sensors_control_context_t *dev;
		dev = (struct sensors_control_context_t*)malloc(sizeof(*dev));

		/* initialize our state here */
		memset(dev, 0, sizeof(*dev));

		/* initialize the procs */
		dev->device.common.tag = HARDWARE_DEVICE_TAG;
		dev->device.common.version = 0;
		dev->device.common.module = module;
		dev->device.common.close = sensors_control_device_close;
		dev->device.open_data_source = sensors_control_open_data_source;
		dev->device.close_data_source= 0;
		dev->device.activate = sensors_control_activate;
		dev->device.set_delay = sensors_control_delay;
		dev->device.wake = sensors_control_wake;

		*device = &dev->device.common;
		status = 0;
	}
	if (!strcmp(name, SENSORS_HARDWARE_DATA)) {
		struct sensors_data_context_t *dev;
		dev = (struct sensors_data_context_t*)malloc(sizeof(*dev));

		/* initialize our state here */
		memset(dev, 0, sizeof(*dev));

		/* initialize the procs */
		dev->device.common.tag = HARDWARE_DEVICE_TAG;
		dev->device.common.version = 0;
		dev->device.common.module = module;
		dev->device.common.close = sensors_data_device_close;

		dev->device.data_open = sensors_data_data_open;
		dev->device.data_close = sensors_data_data_close;
		dev->device.poll = sensors_data_poll;

		*device = &dev->device.common;
		status = 0;
	}
	return status;
}

static int sensors_control_device_close(struct hw_device_t *dev)
{
	struct sensors_control_context_t* ctx = (struct sensors_control_device_t*)dev;
	LOGD("sensors_control_device_close");
#if 0 /* is done in data_device */
	if(akm_fd != -1) {
		close(akm_fd);
		akm_fd = -1;
	}
	if(bma_input_fd != -1) {
		close(bma_input_fd);
		bma_input_fd = -1;
	}
	if(akm_input_fd != -1) {
        close(akm_input_fd);
        akm_input_fd = -1;
    }
	if(prox_input_fd != -1) {
		close(prox_input_fd);
		prox_input_fd = -1;
	}
#endif
	free(ctx);
	return 0;
}

static int sensors_data_device_close(struct hw_device_t *dev)
{
	struct sensors_data_context_t* ctx = (struct sensors_data_device_t*)dev;
	LOGD("sensors_data_device_close");
	free(ctx);
	return 0;
}

static int sensors_control_wake(struct sensors_control_device_t *dev)
{
	LOGD("sensors_control_wake");
	wakeup = 1;
	return 0;
}

/*                                    sensors_data_* interface                 */

static int sensors_data_data_open(struct sensors_data_device_t *dev, native_handle_t* nh)
{
	int i;
	LMSInit();
	memset(&sSensors, 0, sizeof(sSensors));
	LOGD("sensors_data_data_open");
	for (i=0 ; i<MAX_NUM_SENSORS ; i++) {
		// by default all sensors have high accuracy
		// (we do this because we don't get an update if the value doesn't
		// change).
		sSensors[i].vector.status = SENSOR_STATUS_ACCURACY_HIGH;
		sSensors[i].sensor = id_to_sensor[i];
	}
	sPendingSensors = 0;
	akm_input_fd = dup(nh->data[0]);
	bma_input_fd = dup(nh->data[1]);
	prox_input_fd = dup(nh->data[2]);
	return 0;
}

static int sensors_data_data_close(struct sensors_data_device_t *dev)
{
	close(akm_input_fd);
	close(bma_input_fd);
	close(prox_input_fd);
	return 0;
}

static int pick_sensor(sensors_data_t* values)
{
	uint32_t mask = 0xff;
	while(mask) {
		uint32_t i = 31 - __builtin_clz(mask);
		mask &= ~(1<<i);
		if (sPendingSensors & (1<<i)) {
			sPendingSensors &= ~(1<<i);
			*values = sSensors[i];
			LOGI("returning sensor %d [%f, %f, %f]", (1<<i),
					values->vector.x,
					values->vector.y,
					values->vector.z);
			return SENSORS_HANDLE_BASE + i;
		}
	}
	LOGE("No sensor to return!!! sPendingSensors=%08x", sPendingSensors);
	// we may end-up in a busy loop, slow things down, just in case.
	usleep(100000);
	return 0;
}

static int sensors_data_poll(struct sensors_data_device_t *dev, sensors_data_t* data)
{
	static struct timespec last_time_poll;
	struct input_event event;
	int nread;

	LOGW("sensors_data_poll: entered");
	while(!wakeup) {
		if (sPendingSensors) {
			LOGD("sPending");
			return pick_sensor(data);
		}

		struct timespec time;
		time.tv_sec = time.tv_nsec = 0;
		clock_gettime(CLOCK_MONOTONIC, &time);

		if( ((time.tv_nsec - last_time_poll.tv_nsec)/1000000 + (time.tv_sec - last_time_poll.tv_sec)*1000) > 1000 ) {
			last_time_poll.tv_nsec = time.tv_nsec;
			last_time_poll.tv_sec = time.tv_sec;
			if (sActiveSensors & SENSORS_LIGHT) {
				sSensors[ID_L].light = read_brightness();
				set_data_time(&sSensors[ID_L]);
				sPendingSensors |= SENSORS_LIGHT;
			}
			if (sActiveSensors & SENSORS_AKM && isControlProcess)
				akm_measure();
		}

		if( bma_input_fd == -1
		 && akm_input_fd == -1 
		 && prox_input_fd != -1 ) {
			usleep(SENSORS_TIMEOUT_MS);
			continue;
		}
		struct pollfd fds[3];
		int nfd = 0;
		if( bma_input_fd != -1 ) {
			fds[nfd].fd = bma_input_fd;
			fds[nfd].events = POLLIN;
			fds[nfd].revents = 0;
			nfd++;
		}
		if( akm_input_fd != -1 ) {
			fds[nfd].fd = akm_input_fd;
			fds[nfd].events = POLLIN;
			fds[nfd].revents = 0;
			nfd++;
		}
		if( prox_input_fd != -1 ) {
            fds[nfd].fd = prox_input_fd;
            fds[nfd].events = POLLIN;
            fds[nfd].revents = 0;
            nfd++;
        }
		LOGI("polling for %d %d %d", bma_input_fd, akm_input_fd, prox_input_fd);
		int err = poll(fds, nfd, SENSORS_TIMEOUT_MS);
		if(err <= 0)
			continue;

		int i;
		int fd = -1;
		for(i=0; i<nfd; ++i) {
			if(fds[i].revents) {
				fd = fds[i].fd;
				break;
			}
		}
		nread = read(fd, &event, sizeof(event));

		if (nread != sizeof(event)) {
			LOGE("Invalid event size");
			continue;
		}

		if (event.type == EV_SYN)
			continue;

		if (event.type != EV_ABS) {
			LOGE("Invalid event type %d on %s", event.type, (fd == akm_input_fd) ? "akm" : "not akm");
			continue;
		}

		LOGE("type: %d code: %d value: %-5d time: %ds",
				event.type, event.code, event.value,
				(int)event.time.tv_sec);

		switch (event.code) {
			case EVENT_TYPE_ACCEL_X:
				LOGD("EVENT_TYPE_ACCEL_X");
				sSensors[ID_A].acceleration.y = event.value * CONVERT_A_X;
				break;
			case EVENT_TYPE_ACCEL_Y:
				LOGD("EVENT_TYPE_ACCEL_Y");
				sSensors[ID_A].acceleration.x = event.value * CONVERT_A_Y;
				break;
			case EVENT_TYPE_ACCEL_Z:
				LOGD("EVENT_TYPE_ACCEL_Z");
				sSensors[ID_A].acceleration.z = event.value * CONVERT_A_Z;
				sSensors[ID_A].time = event_time_to_sensor_time(event);
				sPendingSensors |= SENSORS_ACCELERATION;
				break;

			case EVENT_TYPE_MAGV_X:
				LOGD("EVENT_TYPE_MAGV_X");
				sSensors[ID_M].magnetic.x = event.value * CONVERT_M_X;
				break;
			case EVENT_TYPE_MAGV_Y:
				LOGD("EVENT_TYPE_MAGV_Y");
				sSensors[ID_M].magnetic.y = event.value * CONVERT_M_Y;
				break;
			case EVENT_TYPE_MAGV_Z:
				LOGD("EVENT_TYPE_MAGV_Z");
				sSensors[ID_M].magnetic.z = event.value * CONVERT_M_Z;
				sSensors[ID_M].time = event_time_to_sensor_time(event);
				sPendingSensors |= SENSORS_MAGNETIC_FIELD;
				break;

			case EVENT_TYPE_YAW:
				sPendingSensors |= SENSORS_ORIENTATION | SENSORS_ORIENTATION_RAW;
				LOGD("EVENT_TYPE_YAW");
				/*t = event.time.tv_sec*1000000000LL +
				  event.time.tv_usec*1000;
				  sSensors[ID_O].orientation.azimuth = 
				  (sActiveSensors & SENSORS_ORIENTATION) ?
				  LMSFilter(t, event.value * CONVERT_O) : event.value * CONVERT_O;*/
				sSensors[ID_O].orientation.azimuth = event.value * CONVERT_O_Y;
				sSensors[ID_OR].orientation.azimuth = event.value;
				break;
			case EVENT_TYPE_PITCH:
				sPendingSensors |= SENSORS_ORIENTATION | SENSORS_ORIENTATION_RAW;
				LOGD("EVENT_TYPE_PITCH");
				sSensors[ID_O].orientation.pitch = event.value * CONVERT_O_P;
				sSensors[ID_OR].orientation.pitch = event.value;
				break;
			case EVENT_TYPE_ROLL:
				sPendingSensors |= SENSORS_ORIENTATION | SENSORS_ORIENTATION_RAW;
				LOGD("EVENT_TYPE_ROLL");
				sSensors[ID_O].orientation.roll = event.value * CONVERT_O_R;
				sSensors[ID_OR].orientation.roll = event.value;
				break;

			case EVENT_TYPE_TEMPERATURE:
				LOGD("EVENT_TYPE_TEMPERATURE");
				sSensors[ID_T].temperature = (176.0f - event.value)/16.0f*10.0f;
				sSensors[ID_T].time = event_time_to_sensor_time(event);
				sPendingSensors |= SENSORS_TEMPERATURE;
				break;
			case EVENT_TYPE_PROXIMITY:
				LOGD("EVENT_TYPE_PROXIMITY");
				sSensors[ID_P].distance = event.value;
				sSensors[ID_P].time = event_time_to_sensor_time(event);
				sPendingSensors |= SENSORS_PROXIMITY;
				break;
		}
	}
	wakeup = 0;
	return 0x7FFFFFFF;
}

