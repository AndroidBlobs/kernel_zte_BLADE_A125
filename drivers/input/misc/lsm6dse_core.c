/*
 * STMicroelectronics lsm6dse driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 *
 * Giuseppe Barba <giuseppe.barba@st.com>
 * v 1.1.0
 * Licensed under the GPL-2.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#include	"lsm6dse.h"
#include	"lsm6dse_core.h"

#if defined(CONFIG_LSM6DSE_POLLING_MODE)
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#endif

/* [3405350]-Add-BEGIN by T2M.zhuli 20161109 */
#define ST_SOLUTION_FOR_CTS
/* [3405350]-Add-END by T2M.zhuli */

/* COMMON VALUES FOR ACCEL-GYRO SENSORS */
#define LSM6DSE_WHO_AM_I			0x0f
#define LSM6DSE_WHO_AM_I_DEF			0x6a
#define LSM6DSE_AXIS_EN_MASK			0x38
#define LSM6DSE_INT1_CTRL_ADDR			0x0d
#define LSM6DSE_INT2_CTRL_ADDR			0x0e
#define LSM6DSE_INT1_FULL			0x20
#define LSM6DSE_INT1_FTH			0x08
#define LSM6DSE_MD1_ADDR			0x5e
#define LSM6DSE_ODR_LIST_NUM			5
#define LSM6DSE_ODR_POWER_OFF_VAL		0x00
#define LSM6DSE_ODR_26HZ_VAL			0x02
#define LSM6DSE_ODR_52HZ_VAL			0x03
#define LSM6DSE_ODR_104HZ_VAL			0x04
#define LSM6DSE_ODR_208HZ_VAL			0x05
#define LSM6DSE_ODR_416HZ_VAL			0x06
#define LSM6DSE_FS_LIST_NUM			4
#define LSM6DSE_BDU_ADDR			0x12
#define LSM6DSE_BDU_MASK			0x40
#define LSM6DSE_EN_BIT				0x01
#define LSM6DSE_DIS_BIT				0x00
#define LSM6DSE_FUNC_EN_ADDR			0x19
#define LSM6DSE_FUNC_EN_MASK			0x04
#define LSM6DSE_FUNC_CFG_ACCESS_ADDR		0x01
#define LSM6DSE_FUNC_CFG_ACCESS_MASK		0x01
#define LSM6DSE_FUNC_CFG_ACCESS_MASK2		0x04
#define LSM6DSE_FUNC_CFG_REG2_MASK		0x80
#define LSM6DSE_FUNC_CFG_START1_ADDR		0x62
#define LSM6DSE_FUNC_CFG_START2_ADDR		0x63
#define LSM6DSE_SELFTEST_ADDR			0x14
#define LSM6DSE_SELFTEST_ACCEL_MASK		0x03
#define LSM6DSE_SELFTEST_GYRO_MASK		0x0c
#define LSM6DSE_SELF_TEST_DISABLED_VAL		0x00
#define LSM6DSE_SELF_TEST_POS_SIGN_VAL		0x01
#define LSM6DSE_SELF_TEST_NEG_ACCEL_SIGN_VAL	0x02
#define LSM6DSE_SELF_TEST_NEG_GYRO_SIGN_VAL	0x03
#define LSM6DSE_LIR_ADDR			0x58
#define LSM6DSE_LIR_MASK			0x01
#define LSM6DSE_TIMER_EN_ADDR			0x58
#define LSM6DSE_TIMER_EN_MASK			0x80
#define LSM6DSE_PEDOMETER_EN_ADDR		0x58
#define LSM6DSE_PEDOMETER_EN_MASK		0x40
#define LSM6DSE_INT2_ON_INT1_ADDR		0x13
#define LSM6DSE_INT2_ON_INT1_MASK		0x20
#define LSM6DSE_MIN_DURATION_MS			1638
#define LSM6DSE_ROUNDING_ADDR			0x16
#define LSM6DSE_ROUNDING_MASK			0x04
#define LSM6DSE_FIFO_MODE_ADDR			0x0a
#define LSM6DSE_FIFO_MODE_MASK			0x07
#define LSM6DSE_FIFO_MODE_BYPASS		0x00
#define LSM6DSE_FIFO_MODE_CONTINUOS		0x06
#define LSM6DSE_FIFO_THRESHOLD_IRQ_MASK		0x08
#define LSM6DSE_FIFO_ODR_ADDR			0x0a
#define LSM6DSE_FIFO_ODR_MASK			0x78
#define LSM6DSE_FIFO_ODR_MAX			0x07
#define LSM6DSE_FIFO_ODR_MAX_HZ			800
#define LSM6DSE_FIFO_ODR_OFF			0x00
#define LSM6DSE_FIFO_CTRL3_ADDR			0x08
#define LSM6DSE_FIFO_ACCEL_DECIMATOR_MASK	0x07
#define LSM6DSE_FIFO_GYRO_DECIMATOR_MASK	0x38
#define LSM6DSE_FIFO_CTRL4_ADDR			0x09
#define LSM6DSE_FIFO_STEP_C_DECIMATOR_MASK	0x38
#define LSM6DSE_FIFO_THR_L_ADDR			0x06
#define LSM6DSE_FIFO_THR_H_ADDR			0x07
#define LSM6DSE_FIFO_THR_H_MASK			0x0f
#define LSM6DSE_FIFO_THR_IRQ_MASK		0x08
#define LSM6DSE_FIFO_PEDO_E_ADDR		0x07
#define LSM6DSE_FIFO_PEDO_E_MASK		0x80
#define LSM6DSE_FIFO_STEP_C_FREQ		25

/* CUSTOM VALUES FOR ACCEL SENSOR */
#define LSM6DSE_ACCEL_ODR_ADDR			0x10
#define LSM6DSE_ACCEL_ODR_MASK			0xf0
#define LSM6DSE_ACCEL_FS_ADDR			0x10
#define LSM6DSE_ACCEL_FS_MASK			0x0c
#define LSM6DSE_ACCEL_FS_2G_VAL			0x00
#define LSM6DSE_ACCEL_FS_4G_VAL			0x02
#define LSM6DSE_ACCEL_FS_8G_VAL			0x03
#define LSM6DSE_ACCEL_FS_16G_VAL		0x01
#define LSM6DSE_ACCEL_FS_2G_GAIN		61
#define LSM6DSE_ACCEL_FS_4G_GAIN		122
#define LSM6DSE_ACCEL_FS_8G_GAIN		244
#define LSM6DSE_ACCEL_FS_16G_GAIN		488
#define LSM6DSE_ACCEL_OUT_X_L_ADDR		0x28
#define LSM6DSE_ACCEL_OUT_Y_L_ADDR		0x2a
#define LSM6DSE_ACCEL_OUT_Z_L_ADDR		0x2c
#define LSM6DSE_ACCEL_AXIS_EN_ADDR		0x18
#define LSM6DSE_ACCEL_DRDY_IRQ_MASK		0x01
#define LSM6DSE_ACCEL_STD			1
#define LSM6DSE_ACCEL_STD_FROM_PD		2

/* CUSTOM VALUES FOR GYRO SENSOR */
#define LSM6DSE_GYRO_ODR_ADDR			0x11
#define LSM6DSE_GYRO_ODR_MASK			0xf0
#define LSM6DSE_GYRO_FS_ADDR			0x11
#define LSM6DSE_GYRO_FS_MASK			0x0c
#define LSM6DSE_GYRO_FS_245_VAL			0x00
#define LSM6DSE_GYRO_FS_500_VAL			0x01
#define LSM6DSE_GYRO_FS_1000_VAL		0x02
#define LSM6DSE_GYRO_FS_2000_VAL		0x03
#define LSM6DSE_GYRO_FS_245_GAIN		875
#define LSM6DSE_GYRO_FS_500_GAIN		1750
#define LSM6DSE_GYRO_FS_1000_GAIN		3500
#define LSM6DSE_GYRO_FS_2000_GAIN		7000
#define LSM6DSE_GYRO_OUT_X_L_ADDR		0x22
#define LSM6DSE_GYRO_OUT_Y_L_ADDR		0x24
#define LSM6DSE_GYRO_OUT_Z_L_ADDR		0x26
#define LSM6DSE_GYRO_AXIS_EN_ADDR		0x19
#define LSM6DSE_GYRO_DRDY_IRQ_MASK		0x02
#define LSM6DSE_GYRO_STD			6
#define LSM6DSE_GYRO_STD_FROM_PD		2

#define LSM6DSE_OUT_XYZ_SIZE			8

/* CUSTOM VALUES FOR SIGNIFICANT MOTION SENSOR */
#define LSM6DSE_SIGN_MOTION_EN_ADDR		0x19
#define LSM6DSE_SIGN_MOTION_EN_MASK		0x01
#define LSM6DSE_SIGN_MOTION_DRDY_IRQ_MASK	0x40

/* CUSTOM VALUES FOR STEP DETECTOR SENSOR */
#define LSM6DSE_STEP_DETECTOR_DRDY_IRQ_MASK	0x80

/* CUSTOM VALUES FOR STEP COUNTER SENSOR */
#define LSM6DSE_STEP_COUNTER_DRDY_IRQ_MASK	0x80
#define LSM6DSE_STEP_COUNTER_OUT_L_ADDR		0x4b
#define LSM6DSE_STEP_COUNTER_OUT_SIZE		2
#define LSM6DSE_STEP_COUNTER_RES_ADDR		0x19
#define LSM6DSE_STEP_COUNTER_RES_MASK		0x06
#define LSM6DSE_STEP_COUNTER_RES_ALL_EN		0x03
#define LSM6DSE_STEP_COUNTER_RES_FUNC_EN	0x02
#define LSM6DSE_STEP_COUNTER_DURATION_ADDR	0x15

/* CUSTOM VALUES FOR TILT SENSOR */
#define LSM6DSE_TILT_EN_ADDR			0x58
#define LSM6DSE_TILT_EN_MASK			0x20
#define LSM6DSE_TILT_DRDY_IRQ_MASK		0x02

#define LSM6DSE_ENABLE_AXIS			0x07
#define LSM6DSE_FIFO_DIFF_L			0x3a
#define LSM6DSE_FIFO_DIFF_MASK			0x0fff
#define LSM6DSE_FIFO_DATA_OUT_L			0x3e
#define LSM6DSE_FIFO_ELEMENT_LEN_BYTE		6
#define LSM6DSE_FIFO_BYTE_FOR_CHANNEL		2
#define LSM6DSE_FIFO_DATA_OVR_2REGS		0x4000
#define LSM6DSE_FIFO_DATA_OVR			0x40

#define LSM6DSE_SRC_FUNC_ADDR			0x53
#define LSM6DSE_FIFO_DATA_AVL_ADDR		0x3b

#define LSM6DSE_SRC_SIGN_MOTION_DATA_AVL	0x40
#define LSM6DSE_SRC_STEP_DETECTOR_DATA_AVL	0x10
#define LSM6DSE_SRC_TILT_DATA_AVL		0x20
#define LSM6DSE_SRC_STEP_COUNTER_DATA_AVL	0x80
#define LSM6DSE_FIFO_DATA_AVL			0x80
#define LSM6DSE_RESET_ADDR			0x12
#define LSM6DSE_RESET_MASK			0x01
#define LSM6DSE_MAX_FIFO_SIZE			(8 * 1024)
#define LSM6DSE_MAX_FIFO_LENGTH			(LSM6DSE_MAX_FIFO_SIZE / \
						LSM6DSE_FIFO_ELEMENT_LEN_BYTE)
#ifndef MAX
#define MAX(a, b)				(((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b)				(((a) < (b)) ? (a) : (b))
#endif

#define ACCEL_G_MAX			8096
#define ACCEL_FUZZ			3
#define ACCEL_FLAT			3
#define BMG_VALUE_MAX (32767)
#define BMG_VALUE_MIN (-32768)
#define DEBUG_QUE           0


static struct sensors_classdev acc_sensors_cdev = {
	.name = "lsm6dse-accel",
	.vendor = "lsm",
	.version = 1,
	.handle = SENSORS_ACCELERATION_HANDLE,
	.type = SENSOR_TYPE_ACCELEROMETER,
	.max_range = "156.8",	/* 16g */
	.resolution = "0.156",	/* 15.63mg */
	.sensor_power = "0.13",	/* typical value */
#ifndef ST_SOLUTION_FOR_CTS
	.min_delay = 10,	/* in microseconds */
	.max_delay = 1000,
#else
	.min_delay = 10000,	/* in microseconds */
	.max_delay = 100,
#endif
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,	/* in millisecond */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev gyro_sensors_cdev = {
	.name = "lsm6dse-gyro",
	.vendor = "lsm",
	.version = 1,
	.handle = SENSORS_GYROSCOPE_HANDLE,
	.type = SENSOR_TYPE_GYROSCOPE,
	.max_range = "35.0",
	.resolution = "1.0",
	.sensor_power = "0.2",
#ifndef ST_SOLUTION_FOR_CTS
	.min_delay = 10,
	.max_delay = 1000,
#else
	.min_delay = 10000,
	.max_delay = 100,
#endif
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,

};


static const struct lsm6dse_sensor_name {
	const char *name;
	const char *description;
} lsm6dse_sensor_name[LSM6DSE_SENSORS_NUMB] = {
	[LSM6DSE_ACCEL] = {
		.name = "lsm6dse-accel",
		.description = "lsm6dse-accel",
	},
	[LSM6DSE_GYRO] = {
			.name = "lsm6dse-gyro",
			.description = "lsm6dse-gyro",
	},
	[LSM6DSE_SIGN_MOTION] = {
		.name = "sign_m",
		.description = "ST LSM6DSE Significant Motion Sensor",
	},
	[LSM6DSE_STEP_COUNTER] = {
		.name = "step_c",
		.description = "lsm6dse-stepcounter",
	},
	[LSM6DSE_STEP_DETECTOR] = {
			.name = "step_d",
			.description = "ST LSM6DSE Step Detector Sensor",
	},
	[LSM6DSE_TILT] = {
			.name = "tilt",
			.description = "ST LSM6DSE Tilt Sensor",
	},
};

struct lsm6dse_odr_reg {
	u32 hz;
	u8 value;
};

static const struct lsm6dse_odr_table {
	u8 addr[2];
	u8 mask[2];
	struct lsm6dse_odr_reg odr_avl[6];
} lsm6dse_odr_table = {
	.addr[LSM6DSE_ACCEL] = LSM6DSE_ACC_ODR_ADDR,
	.mask[LSM6DSE_ACCEL] = LSM6DSE_ACC_ODR_MASK,
	.addr[LSM6DSE_GYRO] = LSM6DSE_GYR_ODR_ADDR,
	.mask[LSM6DSE_GYRO] = LSM6DSE_GYR_ODR_MASK,
	.odr_avl[0] = { .hz = 26, .value = LSM6DSE_ODR_26HZ_VAL },
	.odr_avl[1] = { .hz = 52, .value = LSM6DSE_ODR_52HZ_VAL },
	.odr_avl[2] = { .hz = 104, .value = LSM6DSE_ODR_104HZ_VAL },
	.odr_avl[3] = { .hz = 208, .value = LSM6DSE_ODR_208HZ_VAL },
	.odr_avl[4] = { .hz = 416, .value = LSM6DSE_ODR_416HZ_VAL },
};

struct lsm6dse_fs_reg {
	unsigned int gain;
	u8 value;
	int urv;
};

static struct lsm6dse_fs_table {
	u8 addr;
	u8 mask;
	struct lsm6dse_fs_reg fs_avl[LSM6DSE_FS_LIST_NUM];
} lsm6dse_fs_table[LSM6DSE_SENSORS_NUMB] = {
	[LSM6DSE_ACCEL] = {
		.addr = LSM6DSE_ACCEL_FS_ADDR,
		.mask = LSM6DSE_ACCEL_FS_MASK,
		.fs_avl[0] = { .gain = LSM6DSE_ACCEL_FS_2G_GAIN,
					.value = LSM6DSE_ACCEL_FS_2G_VAL,
					.urv = 2, },
		.fs_avl[1] = { .gain = LSM6DSE_ACCEL_FS_4G_GAIN,
					.value = LSM6DSE_ACCEL_FS_4G_VAL,
					.urv = 4, },
		.fs_avl[2] = { .gain = LSM6DSE_ACCEL_FS_8G_GAIN,
					.value = LSM6DSE_ACCEL_FS_8G_VAL,
					.urv = 8, },
		.fs_avl[3] = { .gain = LSM6DSE_ACCEL_FS_16G_GAIN,
					.value = LSM6DSE_ACCEL_FS_16G_VAL,
					.urv = 16, },
	},
	[LSM6DSE_GYRO] = {
		.addr = LSM6DSE_GYRO_FS_ADDR,
		.mask = LSM6DSE_GYRO_FS_MASK,
		.fs_avl[0] = { .gain = LSM6DSE_GYRO_FS_245_GAIN,
					.value = LSM6DSE_GYRO_FS_245_VAL,
					.urv = 245, },
		.fs_avl[1] = { .gain = LSM6DSE_GYRO_FS_500_GAIN,
					.value = LSM6DSE_GYRO_FS_500_VAL,
					.urv = 500, },
		.fs_avl[2] = { .gain = LSM6DSE_GYRO_FS_1000_GAIN,
					.value = LSM6DSE_GYRO_FS_1000_VAL,
					.urv = 1000, },
		.fs_avl[3] = { .gain = LSM6DSE_GYRO_FS_2000_GAIN,
					.value = LSM6DSE_GYRO_FS_2000_VAL,
					.urv = 2000, },
	}
};

static struct workqueue_struct *lsm6dse_workqueue = 0;

static inline void lsm6dse_flush_works(void)
{
	flush_workqueue(lsm6dse_workqueue);
}

static inline s64 lsm6dse_get_time_ns(void)
{
	struct timespec ts;
	/*
	 * calls getnstimeofday.
	 * If hrtimers then up to ns accurate, if not microsecond.
	 */
	get_monotonic_boottime(&ts);

	return timespec_to_ns(&ts);
}

static int lsm6dse_write_data_with_mask(struct lsm6dse_data *cdata,
				u8 reg_addr, u8 mask, u8 data, bool b_lock)
{
	int err;
	u8 new_data = 0x00, old_data = 0x00;

	err = cdata->tf->read(cdata, reg_addr, 1, &old_data, b_lock);
	if (err < 0)
		return err;

	new_data = ((old_data & (~mask)) | ((data << __ffs(mask)) & mask));

	if (new_data == old_data)
		return 1;

	return cdata->tf->write(cdata, reg_addr, 1, &new_data, b_lock);
}

static int lsm6dse_input_init(struct lsm6dse_sensor_data *sdata, u16 bustype,
							const char *description)
{
	int err = 0;

	sdata->input_dev = input_allocate_device();
	if (!sdata->input_dev) {
		dev_err(sdata->cdata->dev, "failed to allocate input device");
		return -ENOMEM;
	}

	sdata->input_dev->name = lsm6dse_sensor_name[sdata->sindex].name;

	sdata->input_dev->id.bustype = bustype;
	sdata->input_dev->dev.parent = sdata->cdata->dev;
	/*sdata->input_dev->name = description;*/
	input_set_drvdata(sdata->input_dev, sdata);

	if (sdata->sindex == LSM6DSE_ACCEL) {
		__set_bit(EV_ABS, sdata->input_dev->evbit);
		input_set_abs_params(sdata->input_dev, ABS_X, -ACCEL_G_MAX, ACCEL_G_MAX,
					ACCEL_FUZZ, ACCEL_FLAT);
		input_set_abs_params(sdata->input_dev, ABS_Y, -ACCEL_G_MAX, ACCEL_G_MAX,
					ACCEL_FUZZ, ACCEL_FLAT);
		input_set_abs_params(sdata->input_dev, ABS_Z, -ACCEL_G_MAX, ACCEL_G_MAX,
					ACCEL_FUZZ, ACCEL_FLAT);
	} else if (sdata->sindex == LSM6DSE_GYRO) {
		input_set_capability(sdata->input_dev, EV_ABS, ABS_MISC);
		input_set_abs_params(sdata->input_dev, ABS_RX, BMG_VALUE_MIN, BMG_VALUE_MAX, 0, 0);
		input_set_abs_params(sdata->input_dev, ABS_RY, BMG_VALUE_MIN, BMG_VALUE_MAX, 0, 0);
		input_set_abs_params(sdata->input_dev, ABS_RZ, BMG_VALUE_MIN, BMG_VALUE_MAX, 0, 0);
	}
#if 0
	__set_bit(INPUT_EVENT_TYPE, sdata->input_dev->evbit);
	__set_bit(INPUT_EVENT_TIME_MSB, sdata->input_dev->mscbit);
	__set_bit(INPUT_EVENT_TIME_LSB, sdata->input_dev->mscbit);
	__set_bit(INPUT_EVENT_X, sdata->input_dev->mscbit);

	if ((sdata->sindex == LSM6DSE_ACCEL) ||
					(sdata->sindex == LSM6DSE_GYRO)) {
		__set_bit(INPUT_EVENT_Y, sdata->input_dev->mscbit);
		__set_bit(INPUT_EVENT_Z, sdata->input_dev->mscbit);
	}
#endif
	err = input_register_device(sdata->input_dev);
	if (err) {
		dev_err(sdata->cdata->dev, "unable to register sensor %s\n",
								sdata->name);
		input_free_device(sdata->input_dev);
	}

	return err;
}

static void lsm6dse_input_cleanup(struct lsm6dse_sensor_data *sdata)
{
	input_unregister_device(sdata->input_dev);
	input_free_device(sdata->input_dev);
}

static void lsm6dse_report_3axes_event(struct lsm6dse_sensor_data *sdata,
							s32 *xyz, s64 timestamp)
{
	struct input_dev  *input	= sdata->input_dev;
#ifdef ST_SOLUTION_FOR_CTS
	struct timespec input_tpc;
#endif

	if (!sdata->enabled)
		return;
#if 0
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_X, xyz[0]);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_Y, xyz[1]);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_Z, xyz[2]);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_MSB,
							timestamp >> 32);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_LSB,
							timestamp & 0xffffffff);
#endif

#ifdef ST_SOLUTION_FOR_CTS
	input_tpc = ns_to_timespec(timestamp);
#endif
	if (sdata->sindex == LSM6DSE_ACCEL) {
		input_report_abs(input, ABS_X, xyz[0]);
		input_report_abs(input, ABS_Y, -xyz[1]);
		input_report_abs(input, ABS_Z, -xyz[2]);
	} else if (sdata->sindex == LSM6DSE_GYRO) {
		input_report_abs(input, ABS_RX, -xyz[1]);
		input_report_abs(input, ABS_RY, -xyz[0]);
		input_report_abs(input, ABS_RZ, -xyz[2]);
	}

#ifdef ST_SOLUTION_FOR_CTS
	input_event(input, EV_SYN, SYN_TIME_SEC, input_tpc.tv_sec);
	input_event(input, EV_SYN, SYN_TIME_NSEC, input_tpc.tv_nsec);
#endif

	input_sync(input);
}

static void lsm6dse_report_single_event(struct lsm6dse_sensor_data *sdata,
							s32 data, s64 timestamp)
{
	struct input_dev  *input	= sdata->input_dev;

	if (!sdata->enabled)
		return;

	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_X, data);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_MSB,
							timestamp >> 32);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_LSB,
							timestamp & 0xffffffff);
	input_sync(input);
}

#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
static void lsm6dse_push_data_with_timestamp(struct lsm6dse_sensor_data *sdata,
							u16 offset, s64 timestamp)
{
	s32 data[3];

	data[0] = (s32)((s16)(sdata->cdata->fifo_data_buffer[offset] |
			(sdata->cdata->fifo_data_buffer[offset + 1] << 8)));
	data[1] = (s32)((s16)(sdata->cdata->fifo_data_buffer[offset + 2] |
			(sdata->cdata->fifo_data_buffer[offset + 3] << 8)));
	data[2] = (s32)((s16)(sdata->cdata->fifo_data_buffer[offset + 4] |
			(sdata->cdata->fifo_data_buffer[offset + 5] << 8)));

	/*data[0] *= sdata->c_gain;
	data[1] *= sdata->c_gain;
	data[2] *= sdata->c_gain;*/

	lsm6dse_report_3axes_event(sdata, data, timestamp);
}
#else
enum hrtimer_restart lsm6dse_poll_function_read(struct hrtimer *timer)
{
	struct lsm6dse_sensor_data *sdata;

#ifdef ST_SOLUTION_FOR_CTS
	s64 nowtime;
	s64 timegap;
	s64 ktime_ns = 0;
#endif

	sdata = container_of((struct hrtimer *)timer, struct lsm6dse_sensor_data,
							hr_timer);

#ifdef ST_SOLUTION_FOR_CTS
	nowtime = lsm6dse_get_time_ns();
	sdata->timestamp = nowtime;

	if (sdata->previoustime == 0)
		hrtimer_start(&sdata->hr_timer, sdata->ktime, HRTIMER_MODE_REL);
	else {
		timegap = nowtime - sdata->previoustime;
		if (timegap > MS_TO_NS(sdata->report_interval)) {
			if (timegap < 2 * MS_TO_NS(sdata->report_interval)) {
				ktime_ns = MS_TO_NS(sdata->poll_interval) -
					(timegap - MS_TO_NS(sdata->report_interval));
				sdata->ktime = ktime_set(0, ktime_ns);
				/*printk("== kt %lld pl %lld prt %lld nt %lld\n", ktime_ns,
				(s64)(MS_TO_NS(sdata->poll_interval)),previoustime,nowtime);*/
			} else {
				dev_err(sdata->cdata->dev, "== error 2x timer exceed 2 poll_interval.\n");
			}
		}
		hrtimer_start(&sdata->hr_timer, sdata->ktime, HRTIMER_MODE_REL);
	}

	/*printk("== kt %lld  nt %lld\n", sdata->ktime.tv64, nowtime);*/
	sdata->previoustime = nowtime;
#endif
	queue_work(lsm6dse_workqueue, &sdata->input_work);

	hrtimer_start(&sdata->hr_timer, sdata->ktime, HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static int lsm6dse_get_step_c_data(struct lsm6dse_sensor_data *sdata, u16 *steps)
{
	u8 data[2];
	int err = 0;

	err = sdata->cdata->tf->read(sdata->cdata,
					LSM6DSE_STEP_COUNTER_OUT_L_ADDR,
					LSM6DSE_STEP_COUNTER_OUT_SIZE,
					data, true);
	if (err < 0)
		return err;

	*steps = data[0] | (data[1] << 8);

	return 0;
}

static int lsm6dse_get_poll_data(struct lsm6dse_sensor_data *sdata, u8 *data)
{
	int err = 0;
	u8 reg_addr;

	switch (sdata->sindex) {
	case LSM6DSE_ACCEL:
		reg_addr = LSM6DSE_ACCEL_OUT_X_L_ADDR;

		break;
	case LSM6DSE_GYRO:
		reg_addr = LSM6DSE_GYRO_OUT_X_L_ADDR;

		break;
	default:
		dev_err(sdata->cdata->dev, "invalid polling mode for sensor %s\n",
								sdata->name);
		return -ENOMEM;
	}

	err = sdata->cdata->tf->read(sdata->cdata, reg_addr, LSM6DSE_OUT_XYZ_SIZE,
								data, true);

	return err;
}

static void poll_function_work(struct work_struct *input_work)
{
	struct lsm6dse_sensor_data *sdata;
	int xyz[3] = { 0 };
	u8 data[6];
	int err;
#if DEBUG_QUE
	s64 queue_time;
	s64 queue_gap;
#endif

	sdata = container_of((struct work_struct *)input_work,
			struct lsm6dse_sensor_data, input_work);
#if DEBUG_QUE
	queue_time = lsm6dse_get_time_ns();
	queue_gap  = queue_time - sdata->previoustime;
	dev_info(sdata->cdata->dev, "== qu-g %lld , re-t %lld\n", queue_gap, sdata->timestamp);
#endif

	err = lsm6dse_get_poll_data(sdata, data);
#ifdef ST_SOLUTION_FOR_CTS
	if (sdata->previous_timestampe >=  sdata->timestamp) {
		dev_err(sdata->cdata->dev, "== error timestamp  pt %lld, nt %lld\n",
			sdata->previous_timestampe, sdata->timestamp);
		return;
	}
#endif
	if (err < 0)
		dev_err(sdata->cdata->dev, "get %s data failed %d\n",
								sdata->name, err);
	else {
		xyz[0] = (s32)((s16)(data[0] | (data[1] << 8)));
		xyz[1] = (s32)((s16)(data[2] | (data[3] << 8)));
		xyz[2] = (s32)((s16)(data[4] | (data[5] << 8)));

		sdata->timestamp = ktime_get_boottime();
		lsm6dse_report_3axes_event(sdata, xyz, sdata->timestamp);
	}

#ifdef ST_SOLUTION_FOR_CTS
	sdata->previous_timestampe = sdata->timestamp;
#endif
}
#endif

#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
static void lsm6dse_parse_fifo_data(struct lsm6dse_data *cdata, u16 read_len)
{
	u16 fifo_offset = 0, steps_c = 0;
	u8 gyro_sip, accel_sip, step_c_sip;

	while (fifo_offset < read_len) {
		gyro_sip = cdata->sensors[LSM6DSE_GYRO].sample_in_pattern;
		accel_sip = cdata->sensors[LSM6DSE_ACCEL].sample_in_pattern;
		step_c_sip =
			cdata->sensors[LSM6DSE_STEP_COUNTER].sample_in_pattern;

		do {
			if (gyro_sip > 0) {
				if (cdata->sensors[LSM6DSE_GYRO].sample_to_discard > 0)
					cdata->sensors[LSM6DSE_GYRO].sample_to_discard--;
				else
					lsm6dse_push_data_with_timestamp(
						&cdata->sensors[LSM6DSE_GYRO],
						fifo_offset,
						cdata->sensors[LSM6DSE_GYRO].timestamp);

				cdata->sensors[LSM6DSE_GYRO].timestamp +=
					cdata->sensors[LSM6DSE_GYRO].deltatime;
				fifo_offset += LSM6DSE_FIFO_ELEMENT_LEN_BYTE;
				gyro_sip--;
			}

			if (accel_sip > 0) {
				if (cdata->sensors[LSM6DSE_ACCEL].sample_to_discard > 0)
					cdata->sensors[LSM6DSE_ACCEL].sample_to_discard--;
				else
					lsm6dse_push_data_with_timestamp(
						&cdata->sensors[LSM6DSE_ACCEL],
						fifo_offset,
						cdata->sensors[LSM6DSE_ACCEL].timestamp);

				cdata->sensors[LSM6DSE_ACCEL].timestamp +=
					cdata->sensors[LSM6DSE_ACCEL].deltatime;
				fifo_offset += LSM6DSE_FIFO_ELEMENT_LEN_BYTE;
				accel_sip--;
			}

			if (step_c_sip > 0) {
				steps_c = cdata->fifo_data_buffer[fifo_offset + 4] |
					(cdata->fifo_data_buffer[fifo_offset + 5] << 8);
				if (cdata->steps_c != steps_c) {
					lsm6dse_report_single_event(
						&cdata->sensors[LSM6DSE_STEP_COUNTER],
						steps_c,
						cdata->sensors[LSM6DSE_STEP_COUNTER].timestamp);
					cdata->steps_c = steps_c;
				}
				cdata->sensors[LSM6DSE_STEP_COUNTER].timestamp +=
						cdata->sensors[LSM6DSE_STEP_COUNTER].deltatime;
				fifo_offset += LSM6DSE_FIFO_ELEMENT_LEN_BYTE;
				step_c_sip--;
			}
		} while ((accel_sip > 0) || (gyro_sip > 0) || (step_c_sip > 0));
	}
}

void lsm6dse_read_fifo(struct lsm6dse_data *cdata, bool check_fifo_len)
{
	int err;
	u16 read_len = cdata->fifo_threshold;

	if (!cdata->fifo_data_buffer)
		return;

	if (check_fifo_len) {
		err = cdata->tf->read(cdata, LSM6DSE_FIFO_DIFF_L, 2, (u8 *)&read_len,
									true);
		if (err < 0)
			return;

		if (read_len & LSM6DSE_FIFO_DATA_OVR_2REGS) {
			dev_err(cdata->dev,
				"data fifo overrun, failed to read it.\n");

			return;
		}

		read_len &= LSM6DSE_FIFO_DIFF_MASK;
		read_len *= LSM6DSE_FIFO_BYTE_FOR_CHANNEL;

		if (read_len > cdata->fifo_threshold)
			read_len = cdata->fifo_threshold;
	}

	if (read_len == 0)
		return;

	err = cdata->tf->read(cdata, LSM6DSE_FIFO_DATA_OUT_L, read_len,
						cdata->fifo_data_buffer, true);
	if (err < 0)
		return;

	lsm6dse_parse_fifo_data(cdata, read_len);
}

static int lsm6dse_set_fifo_enable(struct lsm6dse_data *cdata, bool status)
{
	int err;
	u8 reg_value;

	if (status)
		reg_value = LSM6DSE_FIFO_ODR_MAX;
	else
		reg_value = LSM6DSE_FIFO_ODR_OFF;

	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_FIFO_ODR_ADDR,
					LSM6DSE_FIFO_ODR_MASK,
					reg_value, true);
	if (err < 0)
		return err;

	cdata->sensors[LSM6DSE_ACCEL].timestamp = lsm6dse_get_time_ns();
	cdata->sensors[LSM6DSE_GYRO].timestamp =
					cdata->sensors[LSM6DSE_ACCEL].timestamp;
	cdata->sensors[LSM6DSE_STEP_COUNTER].timestamp =
					cdata->sensors[LSM6DSE_ACCEL].timestamp;

	return 0;
}

int lsm6dse_set_fifo_mode(struct lsm6dse_data *cdata, enum fifo_mode fm)
{
	int err;
	u8 reg_value;
	bool enable_fifo;

	switch (fm) {
	case BYPASS:
		reg_value = LSM6DSE_FIFO_MODE_BYPASS;
		enable_fifo = false;
		break;
	case CONTINUOS:
		reg_value = LSM6DSE_FIFO_MODE_CONTINUOS;
		enable_fifo = true;
		break;
	default:
		return -EINVAL;
	}

	err = lsm6dse_set_fifo_enable(cdata, enable_fifo);
	if (err < 0)
		return err;

	return lsm6dse_write_data_with_mask(cdata, LSM6DSE_FIFO_MODE_ADDR,
				LSM6DSE_FIFO_MODE_MASK, reg_value, true);
}

int lsm6dse_set_fifo_decimators_and_threshold(struct lsm6dse_data *cdata)
{
	int err;
	unsigned int min_odr = 416, max_odr = 0;
	u8 decimator = 0;
	struct lsm6dse_sensor_data *sdata_accel, *sdata_gyro, *sdata_step_c;
	u16 fifo_len = 0, fifo_threshold;
	u16 min_num_pattern, num_pattern;

	min_num_pattern = LSM6DSE_MAX_FIFO_SIZE / LSM6DSE_FIFO_ELEMENT_LEN_BYTE;
	sdata_accel = &cdata->sensors[LSM6DSE_ACCEL];
	if (sdata_accel->enabled) {
		min_odr = MIN(min_odr, sdata_accel->c_odr);
		max_odr = MAX(max_odr, sdata_accel->c_odr);
	}

	sdata_gyro = &cdata->sensors[LSM6DSE_GYRO];
	if (sdata_gyro->enabled) {
		min_odr = MIN(min_odr, sdata_gyro->c_odr);
		max_odr = MAX(max_odr, sdata_gyro->c_odr);
	}

	sdata_step_c = &cdata->sensors[LSM6DSE_STEP_COUNTER];
	if (sdata_step_c->enabled) {
		min_odr = MIN(min_odr, sdata_step_c->c_odr);
	}

	if (sdata_accel->enabled) {
		sdata_accel->sample_in_pattern = (sdata_accel->c_odr / min_odr);
		fifo_len += sdata_accel->sample_in_pattern;
		num_pattern = MAX(sdata_accel->fifo_length /
					sdata_accel->sample_in_pattern, 1);
		min_num_pattern = MIN(min_num_pattern, num_pattern);
		sdata_accel->deltatime = (1000000000ULL / sdata_accel->c_odr);
		decimator = max_odr / sdata_accel->c_odr;
	} else {
		sdata_accel->sample_in_pattern = 0;
		decimator = 0;
	}

	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_FIFO_CTRL3_ADDR,
					LSM6DSE_FIFO_ACCEL_DECIMATOR_MASK,
					decimator, true);
	if (err < 0)
		return err;

	if (sdata_gyro->enabled) {
		sdata_gyro->sample_in_pattern = (sdata_gyro->c_odr / min_odr);
		fifo_len += sdata_gyro->sample_in_pattern;
		num_pattern = MAX(sdata_gyro->fifo_length /
					sdata_gyro->sample_in_pattern, 1);
		min_num_pattern = MIN(min_num_pattern, num_pattern);
		sdata_gyro->deltatime = (1000000000ULL / sdata_gyro->c_odr);
		decimator = max_odr / sdata_gyro->c_odr;
	} else {
		sdata_gyro->sample_in_pattern = 0;
		decimator = 0;
	}

	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_FIFO_CTRL3_ADDR,
					LSM6DSE_FIFO_GYRO_DECIMATOR_MASK,
					decimator, true);
	if (err < 0)
		return err;

	if (sdata_step_c->enabled) {
		sdata_step_c->sample_in_pattern = (sdata_step_c->c_odr / min_odr);
		fifo_len += sdata_step_c->sample_in_pattern;
		num_pattern = MAX(sdata_step_c->fifo_length /
					sdata_step_c->sample_in_pattern, 1);
		min_num_pattern = MIN(min_num_pattern, num_pattern);
		sdata_step_c->deltatime = (1000000000ULL / sdata_step_c->c_odr);
		decimator = MAX(max_odr / sdata_step_c->c_odr, 1);
	} else {
		sdata_step_c->sample_in_pattern = 0;
		decimator = 0;
	}

	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_FIFO_CTRL4_ADDR,
					LSM6DSE_FIFO_STEP_C_DECIMATOR_MASK,
					decimator, true);
	if (err < 0)
		return err;

	fifo_len *= (min_num_pattern * LSM6DSE_FIFO_ELEMENT_LEN_BYTE);

	if (fifo_len > 0) {
		fifo_threshold = fifo_len;

		err = cdata->tf->write(cdata,
					LSM6DSE_FIFO_THR_L_ADDR,
					1,
					(u8 *)&fifo_threshold, true);
		if (err < 0)
			return err;

		err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_FIFO_THR_H_ADDR,
					LSM6DSE_FIFO_THR_H_MASK,
					*(((u8 *)&fifo_threshold) + 1), true);
		if (err < 0)
			return err;

		cdata->fifo_threshold = fifo_len;
	}
	if (cdata->fifo_data_buffer != NULL) {
		kfree(cdata->fifo_data_buffer);
		cdata->fifo_data_buffer = 0;
	}

	if (fifo_len > 0) {
		cdata->fifo_data_buffer = kmalloc(cdata->fifo_threshold, GFP_KERNEL);
		if (!cdata->fifo_data_buffer)
			return -ENOMEM;
	}

	return fifo_len;
}

int lsm6dse_reconfigure_fifo(struct lsm6dse_data *cdata,
						bool disable_irq_and_flush)
{
	int err, fifo_len;

	if (disable_irq_and_flush) {
		disable_irq(cdata->irq);
		lsm6dse_flush_works();
	}

	mutex_lock(&cdata->fifo_lock);

	lsm6dse_read_fifo(cdata, true);

	err = lsm6dse_set_fifo_mode(cdata, BYPASS);
	if (err < 0)
		goto reconfigure_fifo_irq_restore;

	fifo_len = lsm6dse_set_fifo_decimators_and_threshold(cdata);
	if (fifo_len < 0) {
		err = fifo_len;
		goto reconfigure_fifo_irq_restore;
	}

	if (fifo_len > 0) {
		err = lsm6dse_set_fifo_mode(cdata, CONTINUOS);
		if (err < 0)
			goto reconfigure_fifo_irq_restore;
	}

reconfigure_fifo_irq_restore:
	mutex_unlock(&cdata->fifo_lock);

	if (disable_irq_and_flush)
		enable_irq(cdata->irq);

	return err;
}
#endif

int lsm6dse_set_drdy_irq(struct lsm6dse_sensor_data *sdata, bool state)
{
	u8 reg_addr, mask, value;

	if (state)
		value = LSM6DSE_EN_BIT;
	else
		value = LSM6DSE_DIS_BIT;

	switch (sdata->sindex) {
	case LSM6DSE_ACCEL:
	case LSM6DSE_GYRO:
	case LSM6DSE_STEP_COUNTER:
#if defined(CONFIG_LSM6DSE_POLLING_MODE)
		return 0;
#else
		if ((sdata->cdata->sensors[LSM6DSE_GYRO].enabled) ||
				(sdata->cdata->sensors[LSM6DSE_ACCEL].enabled) ||
				(sdata->cdata->sensors[LSM6DSE_STEP_COUNTER].enabled))
			return 0;

		reg_addr = LSM6DSE_INT1_CTRL_ADDR;
		mask = LSM6DSE_FIFO_THR_IRQ_MASK;
		break;
#endif
	case LSM6DSE_SIGN_MOTION:
	case LSM6DSE_STEP_DETECTOR:
		if ((sdata->cdata->sensors[LSM6DSE_STEP_DETECTOR].enabled) ||
				(sdata->cdata->sensors[LSM6DSE_SIGN_MOTION].enabled))
			return 0;

		reg_addr = LSM6DSE_INT1_CTRL_ADDR;
		mask = LSM6DSE_STEP_DETECTOR_DRDY_IRQ_MASK;
		break;
	case LSM6DSE_TILT:
		reg_addr = LSM6DSE_MD1_ADDR;
		mask = LSM6DSE_TILT_DRDY_IRQ_MASK;
		break;
	default:
		return -EINVAL;
	}

	return lsm6dse_write_data_with_mask(sdata->cdata, reg_addr, mask, value,
									true);
}

static int lsm6dse_set_fs(struct lsm6dse_sensor_data *sdata, u32 gain)
{
	int err, i;

	for (i = 0; i < LSM6DSE_FS_LIST_NUM; i++) {
		if (lsm6dse_fs_table[sdata->sindex].fs_avl[i].gain == gain)
			break;
	}

	if (i == LSM6DSE_FS_LIST_NUM)
		return -EINVAL;

	err = lsm6dse_write_data_with_mask(sdata->cdata,
				lsm6dse_fs_table[sdata->sindex].addr,
				lsm6dse_fs_table[sdata->sindex].mask,
				lsm6dse_fs_table[sdata->sindex].fs_avl[i].value,
				true);
	if (err < 0)
		return err;

	dev_info(sdata->cdata->dev, "c_gain %d register value %x, i %d.\n",
		gain, lsm6dse_fs_table[sdata->sindex].fs_avl[i].value, i);
	sdata->c_gain = gain;

	return 0;
}

irqreturn_t lsm6dse_save_timestamp(int irq, void *private)
{
	struct lsm6dse_data *cdata = private;

	cdata->timestamp = lsm6dse_get_time_ns();
	queue_work(lsm6dse_workqueue, &cdata->input_work);

	disable_irq_nosync(irq);

	return IRQ_HANDLED;
}

static int lsm6dse_disable_sensors(struct lsm6dse_sensor_data *sdata);

static void lsm6dse_irq_management(struct work_struct *input_work)
{
	struct lsm6dse_data *cdata;
	u8 src_value = 0x00, src_fifo = 0x00;
	struct lsm6dse_sensor_data *sdata;
#ifdef CONFIG_LSM6DSE_POLLING_MODE
	u16 steps_c;
	int err;
#endif

	cdata = container_of((struct work_struct *)input_work,
						struct lsm6dse_data, input_work);

	cdata->tf->read(cdata, LSM6DSE_SRC_FUNC_ADDR, 1, &src_value, true);
	cdata->tf->read(cdata, LSM6DSE_FIFO_DATA_AVL_ADDR, 1, &src_fifo, true);

#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
	if (src_fifo & LSM6DSE_FIFO_DATA_AVL) {
		if (src_fifo & LSM6DSE_FIFO_DATA_OVR) {
			lsm6dse_set_fifo_mode(cdata, BYPASS);
			lsm6dse_set_fifo_mode(cdata, CONTINUOS);
			dev_err(cdata->dev,
				"data fifo overrun, reduce fifo size.\n");
		} else
			lsm6dse_read_fifo(cdata, false);
	}
#else
	if (src_value & LSM6DSE_SRC_STEP_COUNTER_DATA_AVL) {
		sdata = &cdata->sensors[LSM6DSE_STEP_COUNTER];
		sdata->timestamp = cdata->timestamp;
		err = lsm6dse_get_step_c_data(sdata, &steps_c);
		if (err < 0) {
			dev_err(cdata->dev,
				"error while reading step counter data\n");
			enable_irq(cdata->irq);

			return;
		}

		lsm6dse_report_single_event(&cdata->sensors[LSM6DSE_STEP_COUNTER],
						steps_c,
						cdata->sensors[LSM6DSE_STEP_COUNTER].timestamp);
		cdata->steps_c = steps_c;
	}
#endif

	if (src_value & LSM6DSE_SRC_STEP_DETECTOR_DATA_AVL) {
		sdata = &cdata->sensors[LSM6DSE_STEP_DETECTOR];
		sdata->timestamp = cdata->timestamp;
		lsm6dse_report_single_event(sdata, 1, sdata->timestamp);

		if (cdata->sign_motion_event_ready) {
			sdata = &cdata->sensors[LSM6DSE_SIGN_MOTION];
			sdata->timestamp = cdata->timestamp;
			lsm6dse_report_single_event(sdata, 1, sdata->timestamp);
			cdata->sign_motion_event_ready = false;
			lsm6dse_disable_sensors(sdata);
		}
	}

	if (src_value & LSM6DSE_SRC_TILT_DATA_AVL) {
		sdata = &cdata->sensors[LSM6DSE_TILT];
		sdata->timestamp = cdata->timestamp;
		lsm6dse_report_single_event(sdata, 1, sdata->timestamp);
	}

	enable_irq(cdata->irq);
}

int lsm6dse_allocate_workqueue(struct lsm6dse_data *cdata)
{
	int err;

	if (!lsm6dse_workqueue)
		lsm6dse_workqueue = create_workqueue(cdata->name);

	if (!lsm6dse_workqueue)
		return -EINVAL;

	INIT_WORK(&cdata->input_work, lsm6dse_irq_management);

	err = request_threaded_irq(cdata->irq, lsm6dse_save_timestamp, NULL,
			IRQF_TRIGGER_HIGH, cdata->name, cdata);
	if (err)
		return err;

	return 0;
}

static int lsm6dse_set_extra_dependency(struct lsm6dse_sensor_data *sdata,
								bool enable)
{
	int err;

	if (!(sdata->cdata->sensors[LSM6DSE_SIGN_MOTION].enabled |
				sdata->cdata->sensors[LSM6DSE_STEP_COUNTER].enabled |
				sdata->cdata->sensors[LSM6DSE_STEP_DETECTOR].enabled |
				sdata->cdata->sensors[LSM6DSE_TILT].enabled)) {
		if (enable) {
			err = lsm6dse_write_data_with_mask(sdata->cdata,
						LSM6DSE_FUNC_EN_ADDR,
						LSM6DSE_FUNC_EN_MASK,
						LSM6DSE_EN_BIT, true);
			if (err < 0)
				return err;
		} else {
			err = lsm6dse_write_data_with_mask(sdata->cdata,
						LSM6DSE_FUNC_EN_ADDR,
						LSM6DSE_FUNC_EN_MASK,
						LSM6DSE_DIS_BIT, true);
			if (err < 0)
				return err;
		}
	}

	if (!sdata->cdata->sensors[LSM6DSE_ACCEL].enabled) {
		if (enable) {
			err = lsm6dse_write_data_with_mask(sdata->cdata,
						lsm6dse_odr_table.addr[LSM6DSE_ACCEL],
						lsm6dse_odr_table.mask[LSM6DSE_ACCEL],
						lsm6dse_odr_table.odr_avl[0].value, true);
			if (err < 0)
				return err;
		} else {
			err = lsm6dse_write_data_with_mask(sdata->cdata,
						lsm6dse_odr_table.addr[LSM6DSE_ACCEL],
						lsm6dse_odr_table.mask[LSM6DSE_ACCEL],
						LSM6DSE_ODR_POWER_OFF_VAL, true);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

static int lsm6dse_enable_pedometer(struct lsm6dse_sensor_data *sdata,
								bool enable)
{
	int err = 0;
	u8 value = LSM6DSE_DIS_BIT;

	if (sdata->cdata->sensors[LSM6DSE_STEP_COUNTER].enabled &&
			sdata->cdata->sensors[LSM6DSE_STEP_DETECTOR].enabled)
		return 0;

	if (enable)
		value = LSM6DSE_EN_BIT;

	err = lsm6dse_write_data_with_mask(sdata->cdata,
						LSM6DSE_FIFO_PEDO_E_ADDR,
						LSM6DSE_FIFO_PEDO_E_MASK,
						value, true);
	if (err < 0)
		return err;

	return lsm6dse_write_data_with_mask(sdata->cdata,
						LSM6DSE_PEDOMETER_EN_ADDR,
						LSM6DSE_PEDOMETER_EN_MASK,
						value, true);

}

static int lsm6dse_enable_sensors(struct lsm6dse_sensor_data *sdata)
{
	int err, i;

	if (sdata->enabled)
		return 0;

	switch (sdata->sindex) {
	case LSM6DSE_ACCEL:
	case LSM6DSE_GYRO:
		for (i = 0; i < LSM6DSE_ODR_LIST_NUM; i++) {
			if (lsm6dse_odr_table.odr_avl[i].hz == sdata->c_odr)
				break;
		}
		if (i == LSM6DSE_ODR_LIST_NUM)
			return -EINVAL;

		if (sdata->sindex == LSM6DSE_ACCEL)
			sdata->sample_to_discard = LSM6DSE_ACCEL_STD +
							LSM6DSE_ACCEL_STD_FROM_PD;

		sdata->cdata->sensors[LSM6DSE_GYRO].sample_to_discard =
							LSM6DSE_GYRO_STD +
							LSM6DSE_GYRO_STD_FROM_PD;
		err = lsm6dse_write_data_with_mask(sdata->cdata,
				lsm6dse_odr_table.addr[sdata->sindex],
				lsm6dse_odr_table.mask[sdata->sindex],
				lsm6dse_odr_table.odr_avl[i].value, true);
		if (err < 0)
			return err;

#if defined(CONFIG_LSM6DSE_POLLING_MODE)
		hrtimer_start(&sdata->hr_timer, sdata->ktime, HRTIMER_MODE_REL);
#ifdef ST_SOLUTION_FOR_CTS
		sdata->previous_timestampe = 0;    /* if enable , then clear timestamp*/
		sdata->previoustime = 0 ;       /* if enable , then clear previoustime*/
		/*printk("== clear prvious&timestamp lsm6dse_enable_sensors\n");*/
#endif
#endif

		sdata->c_odr = lsm6dse_odr_table.odr_avl[i].hz;

		break;
	case LSM6DSE_SIGN_MOTION:
		err = lsm6dse_write_data_with_mask(sdata->cdata,
						LSM6DSE_SIGN_MOTION_EN_ADDR,
						LSM6DSE_SIGN_MOTION_EN_MASK,
						LSM6DSE_EN_BIT, true);
		if (err < 0)
			return err;

		if ((sdata->cdata->sensors[LSM6DSE_STEP_COUNTER].enabled) ||
				(sdata->cdata->sensors[LSM6DSE_STEP_DETECTOR].enabled)) {
			err = lsm6dse_write_data_with_mask(sdata->cdata,
						LSM6DSE_PEDOMETER_EN_ADDR,
						LSM6DSE_PEDOMETER_EN_MASK,
						LSM6DSE_DIS_BIT, true);
			if (err < 0)
				return err;

			err = lsm6dse_write_data_with_mask(sdata->cdata,
						LSM6DSE_PEDOMETER_EN_ADDR,
						LSM6DSE_PEDOMETER_EN_MASK,
						LSM6DSE_EN_BIT, true);
			if (err < 0)
				return err;
		} else {
			err = lsm6dse_enable_pedometer(sdata, true);
			if (err < 0)
				return err;
		}

		sdata->cdata->sign_motion_event_ready = true;

		break;
	case LSM6DSE_STEP_COUNTER:
	case LSM6DSE_STEP_DETECTOR:
		err = lsm6dse_enable_pedometer(sdata, true);
		if (err < 0)
			return err;

		break;
	case LSM6DSE_TILT:
		err = lsm6dse_write_data_with_mask(sdata->cdata,
					LSM6DSE_TILT_EN_ADDR,
					LSM6DSE_TILT_EN_MASK,
					LSM6DSE_EN_BIT, true);
		if (err < 0)
			return err;

		break;
	default:
		return -EINVAL;
	}

	err = lsm6dse_set_extra_dependency(sdata, true);
	if (err < 0)
		return err;


	err = lsm6dse_set_drdy_irq(sdata, true);
	if (err < 0)
		return err;

	sdata->enabled = true;

#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
	err = lsm6dse_reconfigure_fifo(sdata->cdata, true);
	if (err < 0)
		return err;
#endif

	return 0;
}

static int lsm6dse_disable_sensors(struct lsm6dse_sensor_data *sdata)
{
	int err;

	if (!sdata->enabled)
		return 0;

	switch (sdata->sindex) {
	case LSM6DSE_ACCEL:
		if (sdata->cdata->sensors[LSM6DSE_SIGN_MOTION].enabled |
			sdata->cdata->sensors[LSM6DSE_STEP_COUNTER].enabled |
			sdata->cdata->sensors[LSM6DSE_STEP_DETECTOR].enabled |
			sdata->cdata->sensors[LSM6DSE_TILT].enabled) {

			err = lsm6dse_write_data_with_mask(sdata->cdata,
					lsm6dse_odr_table.addr[LSM6DSE_ACCEL],
					lsm6dse_odr_table.mask[LSM6DSE_ACCEL],
					lsm6dse_odr_table.odr_avl[0].value, true);
		} else {
			err = lsm6dse_write_data_with_mask(sdata->cdata,
					lsm6dse_odr_table.addr[LSM6DSE_ACCEL],
					lsm6dse_odr_table.mask[LSM6DSE_ACCEL],
					LSM6DSE_ODR_POWER_OFF_VAL, true);
		}
		if (err < 0)
			return err;

#if defined(CONFIG_LSM6DSE_POLLING_MODE)
		cancel_work_sync(&sdata->input_work);
		hrtimer_cancel(&sdata->hr_timer);
#endif

		break;
	case LSM6DSE_GYRO:
		err = lsm6dse_write_data_with_mask(sdata->cdata,
					lsm6dse_odr_table.addr[LSM6DSE_GYRO],
					lsm6dse_odr_table.mask[LSM6DSE_GYRO],
					LSM6DSE_ODR_POWER_OFF_VAL, true);
		if (err < 0)
			return err;

#if defined(CONFIG_LSM6DSE_POLLING_MODE)
		cancel_work_sync(&sdata->input_work);
		hrtimer_cancel(&sdata->hr_timer);
#endif

		break;
	case LSM6DSE_SIGN_MOTION:
		err = lsm6dse_write_data_with_mask(sdata->cdata,
					LSM6DSE_SIGN_MOTION_EN_ADDR,
					LSM6DSE_SIGN_MOTION_EN_MASK,
					LSM6DSE_DIS_BIT, true);
		if (err < 0)
			return err;

		err = lsm6dse_enable_pedometer(sdata, false);
		if (err < 0)
			return err;

		sdata->cdata->sign_motion_event_ready = false;

		break;
	case LSM6DSE_STEP_COUNTER:
	case LSM6DSE_STEP_DETECTOR:
		err = lsm6dse_enable_pedometer(sdata, false);
		if (err < 0)
			return err;

		break;
	case LSM6DSE_TILT:
		err = lsm6dse_write_data_with_mask(sdata->cdata,
					LSM6DSE_TILT_EN_ADDR,
					LSM6DSE_TILT_EN_MASK,
					LSM6DSE_DIS_BIT, true);
		if (err < 0)
			return err;

		break;
	default:
		return -EINVAL;
	}

	err = lsm6dse_set_extra_dependency(sdata, false);
	if (err < 0)
		return err;

	err = lsm6dse_set_drdy_irq(sdata, false);
	if (err < 0)
		return err;

	sdata->enabled = false;

#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
	err = lsm6dse_reconfigure_fifo(sdata->cdata, true);
	if (err < 0)
		return err;
#endif

	return 0;
}

static int lsm6dse_reset_steps(struct lsm6dse_data *cdata)
{
	int err;
	u8 reg_value = 0x00;

	err = cdata->tf->read(cdata,
			LSM6DSE_STEP_COUNTER_RES_ADDR, 1, &reg_value, true);
	if (err < 0)
		return err;

	if (reg_value & LSM6DSE_FUNC_EN_MASK)
		reg_value = LSM6DSE_STEP_COUNTER_RES_FUNC_EN;
	else
		reg_value = LSM6DSE_DIS_BIT;

	err = lsm6dse_write_data_with_mask(cdata,
				LSM6DSE_STEP_COUNTER_RES_ADDR,
				LSM6DSE_STEP_COUNTER_RES_MASK,
				LSM6DSE_STEP_COUNTER_RES_ALL_EN, true);
	if (err < 0)
		return err;

	err = lsm6dse_write_data_with_mask(cdata,
				LSM6DSE_STEP_COUNTER_RES_ADDR,
				LSM6DSE_STEP_COUNTER_RES_MASK,
				reg_value, true);
	if (err < 0)
		return err;

	cdata->reset_steps = true;

	return 0;
}

static int lsm6dse_init_sensors(struct lsm6dse_data *cdata)
{
	int err, i;
	u8 default_reg_value = 0;
	struct lsm6dse_sensor_data *sdata;

	mutex_init(&cdata->tb.buf_lock);

	for (i = 0; i < LSM6DSE_SENSORS_NUMB; i++) {
		sdata = &cdata->sensors[i];

		err = lsm6dse_disable_sensors(sdata);
		if (err < 0)
			return err;

		if ((sdata->sindex == LSM6DSE_ACCEL) ||
				(sdata->sindex == LSM6DSE_GYRO)) {
			err = lsm6dse_set_fs(sdata, sdata->c_gain);
			if (err < 0)
				return err;
		}
	}

#if defined(CONFIG_LSM6DSE_POLLING_MODE)
	hrtimer_init(&cdata->sensors[LSM6DSE_ACCEL].hr_timer, CLOCK_BOOTTIME,
						HRTIMER_MODE_REL);
	hrtimer_init(&cdata->sensors[LSM6DSE_GYRO].hr_timer, CLOCK_BOOTTIME,
						HRTIMER_MODE_REL);
	cdata->sensors[LSM6DSE_ACCEL].hr_timer.function =
						&lsm6dse_poll_function_read;
	cdata->sensors[LSM6DSE_GYRO].hr_timer.function =
						&lsm6dse_poll_function_read;
#endif

	cdata->gyro_selftest_status = 0;
	cdata->accel_selftest_status = 0;
	cdata->steps_c = 0;
	cdata->reset_steps = false;

	err = lsm6dse_write_data_with_mask(cdata, LSM6DSE_RESET_ADDR,
				LSM6DSE_RESET_MASK, LSM6DSE_EN_BIT, true);
	if (err < 0)
		return err;

	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_LIR_ADDR,
					LSM6DSE_LIR_MASK,
					LSM6DSE_EN_BIT, true);
	if (err < 0)
		return err;

	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_TIMER_EN_ADDR,
					LSM6DSE_TIMER_EN_MASK,
					LSM6DSE_EN_BIT, true);
		if (err < 0)
			return err;

	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_BDU_ADDR,
					LSM6DSE_BDU_MASK,
					LSM6DSE_EN_BIT, true);
	if (err < 0)
		return err;
#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
	err = lsm6dse_set_fifo_enable(sdata->cdata, false);
	if (err < 0)
		return err;
#endif
	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_ROUNDING_ADDR,
					LSM6DSE_ROUNDING_MASK,
					LSM6DSE_EN_BIT, true);
	if (err < 0)
		return err;

	err = lsm6dse_write_data_with_mask(cdata,
					LSM6DSE_INT2_ON_INT1_ADDR,
					LSM6DSE_INT2_ON_INT1_MASK,
					LSM6DSE_EN_BIT, true);
	if (err < 0)
		return err;

	err = lsm6dse_reset_steps(sdata->cdata);
	if (err < 0)
		return err;

	mutex_lock(&cdata->bank_registers_lock);
	err = lsm6dse_write_data_with_mask(sdata->cdata,
					LSM6DSE_FUNC_CFG_ACCESS_ADDR,
					LSM6DSE_FUNC_CFG_REG2_MASK,
					LSM6DSE_EN_BIT, false);
	if (err < 0)
		goto lsm6dse_init_sensor_mutex_unlock;

	err = sdata->cdata->tf->write(sdata->cdata,
					LSM6DSE_STEP_COUNTER_DURATION_ADDR,
					1,
					&default_reg_value, false);
	if (err < 0)
		goto lsm6dse_init_sensor_mutex_unlock;

	err = lsm6dse_write_data_with_mask(sdata->cdata,
					LSM6DSE_FUNC_CFG_ACCESS_ADDR,
					LSM6DSE_FUNC_CFG_REG2_MASK,
					LSM6DSE_DIS_BIT, false);
	if (err < 0)
		goto lsm6dse_init_sensor_mutex_unlock;
	mutex_unlock(&cdata->bank_registers_lock);

#if defined(CONFIG_LSM6DSE_POLLING_MODE)
	cdata->sensors[LSM6DSE_ACCEL].ktime = ktime_set(0,
			MS_TO_NS(cdata->sensors[LSM6DSE_ACCEL].poll_interval));
	cdata->sensors[LSM6DSE_GYRO].ktime = ktime_set(0,
			MS_TO_NS(cdata->sensors[LSM6DSE_GYRO].poll_interval));
	INIT_WORK(&cdata->sensors[LSM6DSE_ACCEL].input_work, poll_function_work);
	INIT_WORK(&cdata->sensors[LSM6DSE_GYRO].input_work, poll_function_work);
#else
	err = lsm6dse_reconfigure_fifo(cdata, false);
	if (err < 0)
		return err;
#endif

	return 0;

lsm6dse_init_sensor_mutex_unlock:
	mutex_unlock(&cdata->bank_registers_lock);
	return err;
}

static int lsm6dse_set_odr(struct lsm6dse_sensor_data *sdata, u32 odr)
{
	int err = 0, i;

	for (i = 0; i < LSM6DSE_ODR_LIST_NUM; i++) {
		if (lsm6dse_odr_table.odr_avl[i].hz >= odr)
			break;
	}
	if (i == LSM6DSE_ODR_LIST_NUM)
		return -EINVAL;

#ifndef ST_SOLUTION_FOR_CTS
	if (sdata->c_odr == lsm6dse_odr_table.odr_avl[i].hz)
		return 0;
#endif

	if (sdata->enabled) {
		disable_irq(sdata->cdata->irq);
		lsm6dse_flush_works();

		if (sdata->sindex == LSM6DSE_ACCEL)
			sdata->cdata->sensors[LSM6DSE_ACCEL].sample_to_discard +=
							LSM6DSE_ACCEL_STD;

		if (sdata->cdata->sensors[LSM6DSE_GYRO].enabled)
			sdata->cdata->sensors[LSM6DSE_GYRO].sample_to_discard +=
							LSM6DSE_GYRO_STD;

		err = lsm6dse_write_data_with_mask(sdata->cdata,
					lsm6dse_odr_table.addr[sdata->sindex],
					lsm6dse_odr_table.mask[sdata->sindex],
					lsm6dse_odr_table.odr_avl[i].value, true);
		if (err < 0) {
			enable_irq(sdata->cdata->irq);

			return err;
		}

		sdata->c_odr = lsm6dse_odr_table.odr_avl[i].hz;
#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
		err = lsm6dse_reconfigure_fifo(sdata->cdata, false);
		if (err < 0)
			return err;
#endif
		enable_irq(sdata->cdata->irq);
	} else
		sdata->c_odr = lsm6dse_odr_table.odr_avl[i].hz;

	return err;
}

static ssize_t get_enable(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", sdata->enabled);
}

static ssize_t set_enable(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	int err;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);
	unsigned long enable;

	if (kstrtoul(buf, 10, &enable))
		return -EINVAL;

	if (enable)
		err = lsm6dse_enable_sensors(sdata);
	else
		err = lsm6dse_disable_sensors(sdata);

	return count;
}

#if defined(CONFIG_LSM6DSE_POLLING_MODE)
static ssize_t get_polling_rate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", sdata->poll_interval);
}

static ssize_t set_polling_rate(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int err;
	unsigned int polling_rate;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &polling_rate);
	if (err < 0)
		return err;

	mutex_lock(&sdata->input_dev->mutex);
	/*
	 * Polling interval is in msec, then we have to convert it in Hz to
	 * configure ODR through lsm6dse_set_odr
	 */
	err = lsm6dse_set_odr(sdata, 1000 / polling_rate);
	if (!(err < 0)) {
		sdata->poll_interval = polling_rate;
		sdata->ktime = ktime_set(0, MS_TO_NS(polling_rate));
	}
	mutex_unlock(&sdata->input_dev->mutex);

	return (err < 0 ? err : count);
}
#endif

static ssize_t get_sampling_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", sdata->c_odr);
}

static ssize_t set_sampling_freq(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int err;
	unsigned int odr;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &odr);
	if (err < 0)
		return err;

	mutex_lock(&sdata->input_dev->mutex);
	err = lsm6dse_set_odr(sdata, odr);
	mutex_unlock(&sdata->input_dev->mutex);

	return (err < 0 ? err : count);
}

#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
static ssize_t get_fifo_length(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", sdata->fifo_length);
}

static ssize_t set_fifo_length(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int err;
	unsigned int fifo_length;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &fifo_length);
	if (err < 0)
		return err;

	mutex_lock(&sdata->input_dev->mutex);
	sdata->fifo_length = fifo_length;
	mutex_unlock(&sdata->input_dev->mutex);

	lsm6dse_reconfigure_fifo(sdata->cdata, true);

	return count;
}

static ssize_t get_hw_fifo_length(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", LSM6DSE_MAX_FIFO_LENGTH);
}

static ssize_t flush_fifo(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	disable_irq(sdata->cdata->irq);
	lsm6dse_flush_works();

	mutex_lock(&sdata->cdata->fifo_lock);
	lsm6dse_read_fifo(sdata->cdata, true);
	mutex_unlock(&sdata->cdata->fifo_lock);

	enable_irq(sdata->cdata->irq);

	return size;
}
#endif

static ssize_t reset_steps(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int err;
	unsigned int reset;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &reset);
	if (err < 0)
		return err;

	lsm6dse_reset_steps(sdata->cdata);

	return count;
}

static ssize_t set_max_delivery_rate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 duration;
	int err, err2;
	unsigned int max_delivery_rate;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	err = kstrtouint(buf, 10, &max_delivery_rate);
	if (err < 0)
		return -EINVAL;

	if (max_delivery_rate == sdata->c_odr)
		return size;

	duration = max_delivery_rate / LSM6DSE_MIN_DURATION_MS;

	mutex_lock(&sdata->cdata->bank_registers_lock);

	err = lsm6dse_write_data_with_mask(sdata->cdata,
					LSM6DSE_FUNC_CFG_ACCESS_ADDR,
					LSM6DSE_FUNC_CFG_REG2_MASK,
					LSM6DSE_EN_BIT, false);
	if (err < 0)
		goto lsm6dse_set_max_delivery_rate_mutex_unlock;

	err = sdata->cdata->tf->write(sdata->cdata,
					LSM6DSE_STEP_COUNTER_DURATION_ADDR,
					1, &duration, false);
	if (err < 0)
		goto lsm6dse_set_max_delivery_rate_restore_bank;

	err = lsm6dse_write_data_with_mask(sdata->cdata,
					LSM6DSE_FUNC_CFG_ACCESS_ADDR,
					LSM6DSE_FUNC_CFG_REG2_MASK,
					LSM6DSE_DIS_BIT, false);
	if (err < 0)
		goto lsm6dse_set_max_delivery_rate_restore_bank;

	mutex_unlock(&sdata->cdata->bank_registers_lock);

	sdata->c_odr = max_delivery_rate;

	return size;

lsm6dse_set_max_delivery_rate_restore_bank:
	do {
		err2 = lsm6dse_write_data_with_mask(sdata->cdata,
					LSM6DSE_FUNC_CFG_ACCESS_ADDR,
					LSM6DSE_FUNC_CFG_REG2_MASK,
					LSM6DSE_DIS_BIT, false);

		msleep(500);
	} while (err2 < 0);

lsm6dse_set_max_delivery_rate_mutex_unlock:
	mutex_unlock(&sdata->cdata->bank_registers_lock);
	return err;
}

static ssize_t get_max_delivery_rate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", sdata->c_odr);
}

static ssize_t get_sampling_frequency_avail(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int i, len = 0;

	for (i = 0; i < LSM6DSE_ODR_LIST_NUM; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
					lsm6dse_odr_table.odr_avl[i].hz);
	}
	buf[len - 1] = '\n';

	return len;
}

static ssize_t get_scale_avail(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, len = 0;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	for (i = 0; i < LSM6DSE_FS_LIST_NUM; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
			lsm6dse_fs_table[sdata->sindex].fs_avl[i].urv);

	buf[len - 1] = '\n';

	return len;
}

static ssize_t get_cur_scale(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	for (i = 0; i < LSM6DSE_FS_LIST_NUM; i++)
		if (sdata->c_gain == lsm6dse_fs_table[sdata->sindex].fs_avl[i].gain)
			break;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			lsm6dse_fs_table[sdata->sindex].fs_avl[i].urv);
}

static ssize_t set_cur_scale(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	int i, urv, err;
	struct lsm6dse_sensor_data *sdata = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &urv);
	if (err < 0)
		return err;

	for (i = 0; i < LSM6DSE_FS_LIST_NUM; i++)
		if (urv == lsm6dse_fs_table[sdata->sindex].fs_avl[i].urv)
			break;

	if (i == LSM6DSE_FS_LIST_NUM)
		return -EINVAL;

	err = lsm6dse_set_fs(sdata,
				lsm6dse_fs_table[sdata->sindex].fs_avl[i].gain);
	if (err < 0)
		return err;

	return count;
}

static int sensor_cdev_enable(struct sensors_classdev *sensors_cdev,
				    unsigned int enable)
{
	struct lsm6dse_sensor_data *sdata = container_of
				(sensors_cdev, struct lsm6dse_sensor_data, cdev);
	int err = 0;

	dev_info(sdata->cdata->dev, "lsm sdata->sindex = %d, enable = %d\n", sdata->sindex, enable);
	mutex_lock(&sdata->enable_lock);
	if (enable)
		err = lsm6dse_enable_sensors(sdata);
	else
		err = lsm6dse_disable_sensors(sdata);

	sensors_cdev->enabled = enable;
	mutex_unlock(&sdata->enable_lock);

	return err;
}

static int sensor_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
					unsigned int delay_ms)
{
	struct lsm6dse_sensor_data *sdata = container_of
					(sensors_cdev, struct lsm6dse_sensor_data, cdev);
	int err = 0;

	mutex_lock(&sdata->poll_lock);
	if (delay_ms > sensors_cdev->max_delay)
		delay_ms = sensors_cdev->max_delay;
#ifndef ST_SOLUTION_FOR_CTS
	else if (delay_ms < sensors_cdev->min_delay)
		delay_ms = sensors_cdev->min_delay;
#else
	else if (delay_ms < (sensors_cdev->min_delay / 1000))
		delay_ms = sensors_cdev->min_delay / 1000;
#endif

	err = lsm6dse_set_odr(sdata, 1000 / delay_ms);

	if (!(err < 0)) {
		sdata->poll_interval = delay_ms;
#ifdef ST_SOLUTION_FOR_CTS
		sdata->report_interval = delay_ms;
		sdata->previous_timestampe = 0;    /* if change odr  , then clear timestamp*/
		sdata->previoustime = 0;
		/*printk("== clear prvious&timestamp sensor_cdev_poll_delay\n");*/
#endif
		sdata->ktime = ktime_set(0, MS_TO_NS(delay_ms));
	}
	/*printk("=== delay_ms  %d, sdata->poll_interval %d report_interval %d.\n",
		delay_ms, sdata->poll_interval,sdata->report_interval );*/
	mutex_unlock(&sdata->poll_lock);

	return 0;
}


static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO, get_enable, set_enable);
static DEVICE_ATTR(sampling_freq, S_IWUSR | S_IRUGO, get_sampling_freq,
							set_sampling_freq);
#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
static DEVICE_ATTR(fifo_length, S_IWUSR | S_IRUGO, get_fifo_length,
							set_fifo_length);
static DEVICE_ATTR(get_hw_fifo_length, S_IRUGO, get_hw_fifo_length, NULL);
static DEVICE_ATTR(flush_fifo, S_IWUSR, NULL, flush_fifo);
#else
static DEVICE_ATTR(polling_rate, S_IWUSR | S_IRUGO, get_polling_rate,
							set_polling_rate);
#endif
static DEVICE_ATTR(reset_steps, S_IWUSR, NULL, reset_steps);
static DEVICE_ATTR(max_delivery_rate, S_IWUSR | S_IRUGO, get_max_delivery_rate,
							set_max_delivery_rate);
static DEVICE_ATTR(sampling_freq_avail, S_IRUGO, get_sampling_frequency_avail, NULL);
static DEVICE_ATTR(scale_avail, S_IRUGO, get_scale_avail, NULL);
static DEVICE_ATTR(scale, S_IWUSR | S_IRUGO, get_cur_scale, set_cur_scale);

static struct attribute *lsm6dse_accel_attribute[] = {
	&dev_attr_enable.attr,
	&dev_attr_sampling_freq.attr,
#if defined(CONFIG_LSM6DSE_POLLING_MODE)
	&dev_attr_polling_rate.attr,
#else
	&dev_attr_fifo_length.attr,
	&dev_attr_get_hw_fifo_length.attr,
	&dev_attr_flush_fifo.attr,
#endif
	&dev_attr_sampling_freq_avail.attr,
	&dev_attr_scale_avail.attr,
	&dev_attr_scale.attr,
	NULL,
};

static struct attribute *lsm6dse_gyro_attribute[] = {
	&dev_attr_enable.attr,
	&dev_attr_sampling_freq.attr,
#if defined(CONFIG_LSM6DSE_POLLING_MODE)
	&dev_attr_polling_rate.attr,
#else
	&dev_attr_fifo_length.attr,
	&dev_attr_get_hw_fifo_length.attr,
	&dev_attr_flush_fifo.attr,
#endif
	&dev_attr_sampling_freq_avail.attr,
	&dev_attr_scale_avail.attr,
	&dev_attr_scale.attr,
	NULL,
};

static struct attribute *lsm6dse_sign_m_attribute[] = {
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute *lsm6dse_step_c_attribute[] = {
	&dev_attr_enable.attr,
#if !defined(CONFIG_LSM6DSE_POLLING_MODE)
	&dev_attr_fifo_length.attr,
	&dev_attr_get_hw_fifo_length.attr,
	&dev_attr_flush_fifo.attr,
#endif
	&dev_attr_reset_steps.attr,
	&dev_attr_max_delivery_rate.attr,
	NULL,
};

static struct attribute *lsm6dse_step_d_attribute[] = {
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute *lsm6dse_tilt_attribute[] = {
	&dev_attr_enable.attr,
	NULL,
};

static const struct attribute_group lsm6dse_attribute_groups[] = {
	[LSM6DSE_ACCEL] = {
		.attrs = lsm6dse_accel_attribute,
		.name = "accel",
	},
	[LSM6DSE_GYRO] = {
		.attrs = lsm6dse_gyro_attribute,
		.name = "gyro",
	},
	[LSM6DSE_SIGN_MOTION] = {
		.attrs = lsm6dse_sign_m_attribute,
		.name = "sign_m",
	},
	[LSM6DSE_STEP_COUNTER] = {
		.attrs = lsm6dse_step_c_attribute,
		.name = "step_c",
	},
	[LSM6DSE_STEP_DETECTOR] = {
		.attrs = lsm6dse_step_d_attribute,
		.name = "step_d",
	},
	[LSM6DSE_TILT] = {
		.attrs = lsm6dse_tilt_attribute,
		.name = "tilt",
	},
};

#ifdef CONFIG_OF
static const struct of_device_id lsm6dse_dt_id[] = {
	{.compatible = "st,lsm6dse",},
	{},
};
MODULE_DEVICE_TABLE(of, lsm6dse_dt_id);

static u32 lsm6dse_parse_dt(struct lsm6dse_data *cdata)
{
	u32 val;
	struct device_node *np;

	np = cdata->dev->of_node;
	if (!np)
		return -EINVAL;

	if (!of_property_read_u32(np, "st,drdy-int-pin", &val) &&
							(val <= 2) && (val > 0))
		cdata->drdy_int_pin = (u8)val;
	else
		cdata->drdy_int_pin = 1;

	return 0;
}
#endif

int lsm6dse_common_probe(struct lsm6dse_data *cdata, int irq, u16 bustype)
{
	/* TODO: add errors management */
	int32_t err, i;
	u8 wai = 0x00;
	struct lsm6dse_sensor_data *sdata;

	mutex_init(&cdata->bank_registers_lock);
	mutex_init(&cdata->fifo_lock);
	mutex_init(&cdata->sensors[LSM6DSE_ACCEL].enable_lock);
	mutex_init(&cdata->sensors[LSM6DSE_GYRO].enable_lock);
	mutex_init(&cdata->sensors[LSM6DSE_ACCEL].poll_lock);
	mutex_init(&cdata->sensors[LSM6DSE_GYRO].poll_lock);

	cdata->fifo_data_buffer = 0;

	err = cdata->tf->read(cdata, LSM6DSE_WHO_AM_I, 1, &wai, true);
	if (err < 0) {
		dev_err(cdata->dev, "failed to read Who-Am-I register.\n");
		return err;
	}
	if (wai != LSM6DSE_WHO_AM_I_DEF) {
		dev_err(cdata->dev, "Who-Am-I value not valid.\n");
		return -ENODEV;
	}

	dev_info(cdata->dev, "lsm read chipid = 0x%x\n", wai);

	mutex_init(&cdata->lock);

	if (irq > 0) {
#ifdef CONFIG_OF
		err = lsm6dse_parse_dt(cdata);
		if (err < 0)
			return err;
#else /* CONFIG_OF */
		if (cdata->dev->platform_data) {
			cdata->drdy_int_pin = ((struct lsm6dse_platform_data *)
					cdata->dev->platform_data)->drdy_int_pin;

			if ((cdata->drdy_int_pin > 2) || (cdata->drdy_int_pin < 1))
				cdata->drdy_int_pin = 1;
		} else
			cdata->drdy_int_pin = 1;
#endif /* CONFIG_OF */

		dev_info(cdata->dev, "driver use DRDY int pin %d\n",
							cdata->drdy_int_pin);
	}

	for (i = 0; i < LSM6DSE_SENSORS_NUMB; i++) {
		sdata = &cdata->sensors[i];
		sdata->enabled = false;
		sdata->cdata = cdata;
		sdata->sindex = i;
		sdata->name = lsm6dse_sensor_name[i].name;
		sdata->fifo_length = 1;
		if ((i == LSM6DSE_ACCEL) || (i == LSM6DSE_GYRO)) {
			sdata->c_odr = lsm6dse_odr_table.odr_avl[1].hz;
			sdata->c_gain = lsm6dse_fs_table[i].fs_avl[1].gain;
#if defined(CONFIG_LSM6DSE_POLLING_MODE)
			sdata->poll_interval = 1000 / sdata->c_odr;
#endif
		}
		if (i == LSM6DSE_STEP_COUNTER) {
			sdata->c_odr = LSM6DSE_MIN_DURATION_MS;
		}

		lsm6dse_input_init(sdata, bustype, lsm6dse_sensor_name[i].description);

		if (sysfs_create_group(&sdata->input_dev->dev.kobj,
						&lsm6dse_attribute_groups[i])) {
			dev_err(cdata->dev, "failed to create sysfs group for sensor %s",
								sdata->name);
			input_unregister_device(sdata->input_dev);
			sdata->input_dev = NULL;
		}
	}

	if (lsm6dse_workqueue == 0)
		lsm6dse_workqueue =
			create_workqueue("lsm6dse_workqueue");

	err = lsm6dse_init_sensors(cdata);
	if (err < 0)
		return err;
	if (irq > 0)
		cdata->irq = irq;

	if (irq > 0) {
		err = lsm6dse_allocate_workqueue(cdata);
		if (err < 0)
			return err;
	}

	cdata->fifo_data_buffer = kmalloc(LSM6DSE_MAX_FIFO_SIZE, GFP_KERNEL);
	if (!cdata->fifo_data_buffer)
			return -ENOMEM;

	cdata->fifo_data_size = LSM6DSE_MAX_FIFO_SIZE;

	/* Register to sensors class */
	sdata = &cdata->sensors[LSM6DSE_ACCEL];
	sdata->cdev = acc_sensors_cdev;
	sdata->cdev.sensors_enable = sensor_cdev_enable;
	sdata->cdev.sensors_poll_delay = sensor_cdev_poll_delay;
	err = sensors_classdev_register(cdata->acc_dev, &sdata->cdev);
	if (err) {
		kfree(cdata->fifo_data_buffer);
		dev_err(cdata->acc_dev, "create class device file failed\n");
		return err;
	}

	sdata = &cdata->sensors[LSM6DSE_GYRO];
	sdata->cdev = gyro_sensors_cdev;
	sdata->cdev.sensors_enable = sensor_cdev_enable;
	sdata->cdev.sensors_poll_delay = sensor_cdev_poll_delay;
	err = sensors_classdev_register(cdata->gyro_dev, &sdata->cdev);
	if (err) {
		kfree(cdata->fifo_data_buffer);
		dev_err(cdata->gyro_dev, "sensors class register failed.\n");
		return err;
	}

	dev_info(cdata->dev, "%s: probed\n", LSM6DSE_ACC_GYR_DEV_NAME);

#ifdef FEATURE_PM_SOLUTION
	SetDeviceType(DEVICETYPE_Accelerometer, SENSOR_TYPE_LSM6DSE, __func__);
	SetDeviceType(DEVICETYPE_Gyroscope, SENSOR_TYPE_LSM6DSE, __func__);
#endif

	return 0;
}
EXPORT_SYMBOL(lsm6dse_common_probe);

void lsm6dse_common_remove(struct lsm6dse_data *cdata, int irq)
{
	u8 i;

	for (i = 0; i < LSM6DSE_SENSORS_NUMB; i++) {
		lsm6dse_disable_sensors(&cdata->sensors[i]);
		lsm6dse_input_cleanup(&cdata->sensors[i]);
	}

	/*remove_sysfs_interfaces(cdata->dev);*/

	if (!lsm6dse_workqueue) {
		flush_workqueue(lsm6dse_workqueue);
		destroy_workqueue(lsm6dse_workqueue);
	}

	kfree(cdata->fifo_data_buffer);
}
EXPORT_SYMBOL(lsm6dse_common_remove);

#ifdef CONFIG_PM
int lsm6dse_common_suspend(struct lsm6dse_data *cdata)
{
	int ret = 0;

	ret = regulator_disable(cdata->vdd);
	if (ret) {
		dev_err(cdata->dev, "%s: vdd suspend fail\n", __func__);
		}
	ret = regulator_disable(cdata->vio);
	if (ret) {
		dev_err(cdata->dev, "%s: viosuspend fail\n", __func__);
		}
	return 0;
}
EXPORT_SYMBOL(lsm6dse_common_suspend);

int lsm6dse_common_resume(struct lsm6dse_data *cdata)
{
	int ret = 0;

	ret = regulator_enable(cdata->vdd);
	if (ret) {
		dev_err(cdata->dev, "%s: vdd resume fail\n", __func__);
		}
	ret = regulator_enable(cdata->vio);
	if (ret) {
		dev_err(cdata->dev, "%s: vio resume fail\n", __func__);
	}
	return 0;
}
EXPORT_SYMBOL(lsm6dse_common_resume);
#endif /* CONFIG_PM */

