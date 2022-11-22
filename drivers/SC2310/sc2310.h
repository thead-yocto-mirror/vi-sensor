/*
 * Support for OmniVision SC2310 5M camera sensor.
 *
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __SC2310_H__
#define __SC2310_H__

#define SC2310_NAME		"sc2310"

#define SC2310_POWER_UP_RETRY_NUM 5

/* Defines for register writes and register array processing */
#define I2C_MSG_LENGTH		0x2
#define I2C_RETRY_COUNT		5

#define SC2310_FOCAL_LENGTH_NUM	334	/*3.34mm*/
#define SC2310_FOCAL_LENGTH_DEM	100
#define SC2310_F_NUMBER_DEFAULT_NUM	24
#define SC2310_F_NUMBER_DEM	10

#define MAX_FMTS		1

/* sensor_mode_data read_mode adaptation */
#define SC2310_READ_MODE_BINNING_ON	0x0400
#define SC2310_READ_MODE_BINNING_OFF	0x00
#define SC2310_INTEGRATION_TIME_MARGIN	8

#define SC2310_MAX_EXPOSURE_VALUE	0xFFF1
#define SC2310_MAX_GAIN_VALUE		0xFF

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define SC2310_FOCAL_LENGTH_DEFAULT 0x1B70064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define SC2310_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define SC2310_F_NUMBER_RANGE 0x180a180a
#define SC2310_ID	0x5690

#define SC2310_FINE_INTG_TIME_MIN 0
#define SC2310_FINE_INTG_TIME_MAX_MARGIN 0
#define SC2310_COARSE_INTG_TIME_MIN 1
#define SC2310_COARSE_INTG_TIME_MAX_MARGIN 6

#define SC2310_BIN_FACTOR_MAX 4
/*
 * SC2310 System control registers
 */
#define SC2310_SW_SLEEP				0x0100
#define SC2310_SW_RESET				0x0103
#define SC2310_SW_STREAM			0x0100

#define SC2310_SC_CMMN_CHIP_ID_H		0x300A
#define SC2310_SC_CMMN_CHIP_ID_L		0x300B
#define SC2310_SC_CMMN_SCCB_ID			0x300C
#define SC2310_SC_CMMN_SUB_ID			0x302A /* process, version*/
/*Bit[7:4] Group control, Bit[3:0] Group ID*/
#define SC2310_GROUP_ACCESS			0x3208
/*
*Bit[3:0] Bit[19:16] of exposure,
*remaining 16 bits lies in Reg0x3501&Reg0x3502
*/
#define SC2310_EXPOSURE_H			0x3500
#define SC2310_EXPOSURE_M			0x3501
#define SC2310_EXPOSURE_L			0x3502
/*Bit[1:0] means Bit[9:8] of gain*/
#define SC2310_AGC_H				0x350A
#define SC2310_AGC_L				0x350B /*Bit[7:0] of gain*/

#define SC2310_HORIZONTAL_START_H		0x3800 /*Bit[11:8]*/
#define SC2310_HORIZONTAL_START_L		0x3801 /*Bit[7:0]*/
#define SC2310_VERTICAL_START_H			0x3802 /*Bit[11:8]*/
#define SC2310_VERTICAL_START_L			0x3803 /*Bit[7:0]*/
#define SC2310_HORIZONTAL_END_H			0x3804 /*Bit[11:8]*/
#define SC2310_HORIZONTAL_END_L			0x3805 /*Bit[7:0]*/
#define SC2310_VERTICAL_END_H			0x3806 /*Bit[11:8]*/
#define SC2310_VERTICAL_END_L			0x3807 /*Bit[7:0]*/
#define SC2310_HORIZONTAL_OUTPUT_SIZE_H		0x3808 /*Bit[3:0]*/
#define SC2310_HORIZONTAL_OUTPUT_SIZE_L		0x3809 /*Bit[7:0]*/
#define SC2310_VERTICAL_OUTPUT_SIZE_H		0x380a /*Bit[3:0]*/
#define SC2310_VERTICAL_OUTPUT_SIZE_L		0x380b /*Bit[7:0]*/
/*High 8-bit, and low 8-bit HTS address is 0x380d*/
#define SC2310_TIMING_HTS_H			0x380C
/*High 8-bit, and low 8-bit HTS address is 0x380d*/
#define SC2310_TIMING_HTS_L			0x380D
/*High 8-bit, and low 8-bit HTS address is 0x380f*/
#define SC2310_TIMING_VTS_H			0x380e
/*High 8-bit, and low 8-bit HTS address is 0x380f*/
#define SC2310_TIMING_VTS_L			0x380f

#define SC2310_MWB_RED_GAIN_H			0x3400
#define SC2310_MWB_GREEN_GAIN_H			0x3402
#define SC2310_MWB_BLUE_GAIN_H			0x3404
#define SC2310_MWB_GAIN_MAX			0x0fff

#define SC2310_START_STREAMING			0x01
#define SC2310_STOP_STREAMING			0x00

#define VCM_ADDR           0x0c
#define VCM_CODE_MSB       0x04

#define SC2310_INVALID_CONFIG	0xffffffff

#define SC2310_VCM_SLEW_STEP			0x30F0
#define SC2310_VCM_SLEW_STEP_MAX		0x7
#define SC2310_VCM_SLEW_STEP_MASK		0x7
#define SC2310_VCM_CODE				0x30F2
#define SC2310_VCM_SLEW_TIME			0x30F4
#define SC2310_VCM_SLEW_TIME_MAX		0xffff
#define SC2310_VCM_ENABLE			0x8000

#define SC2310_VCM_MAX_FOCUS_NEG       -1023
#define SC2310_VCM_MAX_FOCUS_POS       1023

#define DLC_ENABLE 1
#define DLC_DISABLE 0
#define VCM_PROTECTION_OFF     0xeca3
#define VCM_PROTECTION_ON      0xdc51
#define VCM_DEFAULT_S 0x0
#define vcm_step_s(a) (u8)(a & 0xf)
#define vcm_step_mclk(a) (u8)((a >> 4) & 0x3)
#define vcm_dlc_mclk(dlc, mclk) (u16)((dlc << 3) | mclk | 0xa104)
#define vcm_tsrc(tsrc) (u16)(tsrc << 3 | 0xf200)
#define vcm_val(data, s) (u16)(data << 4 | s)
#define DIRECT_VCM vcm_dlc_mclk(0, 0)

/* Defines for OTP Data Registers */
#define SC2310_FRAME_OFF_NUM		0x4202
#define SC2310_OTP_BYTE_MAX		32	//change to 32 as needed by otpdata
#define SC2310_OTP_SHORT_MAX		16
#define SC2310_OTP_START_ADDR		0x3D00
#define SC2310_OTP_END_ADDR		0x3D0F
#define SC2310_OTP_DATA_SIZE		320
#define SC2310_OTP_PROGRAM_REG      	0x3D80
#define SC2310_OTP_READ_REG		0x3D81	// 1:Enable 0:disable
#define SC2310_OTP_BANK_REG		0x3D84	//otp bank and mode
#define SC2310_OTP_READY_REG_DONE	1
#define SC2310_OTP_BANK_MAX		28
#define SC2310_OTP_BANK_SIZE		16	//16 bytes per bank
#define SC2310_OTP_READ_ONETIME		16
#define SC2310_OTP_MODE_READ		1

typedef enum SC2310_EXPOSURE_SETTING_e {
	SC2310_ANALOG_GAIN = 1 << 0,
	SC2310_INTEGRATION_TIME = 1 << 1,
	SC2310_DIGITAL_GAIN = 1 << 2,
}SC2310_EXPOSURE_SETTING_t;

#if 0
struct regval_list {
	u16 reg_num;
	u8 value;
};

struct sc2310_resolution {
	u8 *desc;
	const struct sc2310_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	int pix_clk_freq;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	u8 bin_mode;
	bool used;
};

struct sc2310_format {
	u8 *desc;
	u32 pixelformat;
	struct sc2310_reg *regs;
};

enum vcm_type {
	VCM_UNKNOWN,
	VCM_AD5823,
	VCM_DW9714,
};

/*
 * sc2310 device structure.
 */
struct sc2310_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct mutex input_lock;
	struct v4l2_ctrl_handler ctrl_handler;

	struct camera_sensor_platform_data *platform_data;
	struct timespec timestamp_t_focus_abs;
	int vt_pix_clk_freq_mhz;
	int fmt_idx;
	int run_mode;
	int otp_size;
	u8 *otp_data;
	u32 focus;
	s16 number_of_steps;
	u8 res;
	u8 type;
	bool vcm_update;
	enum vcm_type vcm;
};

enum sc2310_tok_type {
	SC2310_8BIT  = 0x0001,
	SC2310_16BIT = 0x0002,
	SC2310_32BIT = 0x0004,
	SC2310_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	SC2310_TOK_DELAY  = 0xfe00,	/* delay token for reg list */
	SC2310_TOK_MASK = 0xfff0
};

/**
 * struct sc2310_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct sc2310_reg {
	enum sc2310_tok_type type;
	u16 reg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

#define to_sc2310_sensor(x) container_of(x, struct sc2310_device, sd)

#define SC2310_MAX_WRITE_BUF_SIZE	30

struct sc2310_write_buffer {
	u16 addr;
	u8 data[SC2310_MAX_WRITE_BUF_SIZE];
};

struct sc2310_write_ctrl {
	int index;
	struct sc2310_write_buffer buffer;
};

static const struct i2c_device_id sc2310_id[] = {
	{SC2310_NAME, 0},
	{}
};

static struct sc2310_reg const sc2310_global_setting[] = {
	{SC2310_8BIT, 0x0103, 0x01},
	{SC2310_8BIT, 0x3001, 0x0a},
	{SC2310_8BIT, 0x3002, 0x80},
	{SC2310_8BIT, 0x3006, 0x00},
	{SC2310_8BIT, 0x3011, 0x21},
	{SC2310_8BIT, 0x3012, 0x09},
	{SC2310_8BIT, 0x3013, 0x10},
	{SC2310_8BIT, 0x3014, 0x00},
	{SC2310_8BIT, 0x3015, 0x08},
	{SC2310_8BIT, 0x3016, 0xf0},
	{SC2310_8BIT, 0x3017, 0xf0},
	{SC2310_8BIT, 0x3018, 0xf0},
	{SC2310_8BIT, 0x301b, 0xb4},
	{SC2310_8BIT, 0x301d, 0x02},
	{SC2310_8BIT, 0x3021, 0x00},
	{SC2310_8BIT, 0x3022, 0x01},
	{SC2310_8BIT, 0x3028, 0x44},
	{SC2310_8BIT, 0x3098, 0x02},
	{SC2310_8BIT, 0x3099, 0x19},
	{SC2310_8BIT, 0x309a, 0x02},
	{SC2310_8BIT, 0x309b, 0x01},
	{SC2310_8BIT, 0x309c, 0x00},
	{SC2310_8BIT, 0x30a0, 0xd2},
	{SC2310_8BIT, 0x30a2, 0x01},
	{SC2310_8BIT, 0x30b2, 0x00},
	{SC2310_8BIT, 0x30b3, 0x7d},
	{SC2310_8BIT, 0x30b4, 0x03},
	{SC2310_8BIT, 0x30b5, 0x04},
	{SC2310_8BIT, 0x30b6, 0x01},
	{SC2310_8BIT, 0x3104, 0x21},
	{SC2310_8BIT, 0x3106, 0x00},
	{SC2310_8BIT, 0x3400, 0x04},
	{SC2310_8BIT, 0x3401, 0x00},
	{SC2310_8BIT, 0x3402, 0x04},
	{SC2310_8BIT, 0x3403, 0x00},
	{SC2310_8BIT, 0x3404, 0x04},
	{SC2310_8BIT, 0x3405, 0x00},
	{SC2310_8BIT, 0x3406, 0x01},
	{SC2310_8BIT, 0x3500, 0x00},
	{SC2310_8BIT, 0x3503, 0x07},
	{SC2310_8BIT, 0x3504, 0x00},
	{SC2310_8BIT, 0x3505, 0x00},
	{SC2310_8BIT, 0x3506, 0x00},
	{SC2310_8BIT, 0x3507, 0x02},
	{SC2310_8BIT, 0x3508, 0x00},
	{SC2310_8BIT, 0x3509, 0x10},
	{SC2310_8BIT, 0x350a, 0x00},
	{SC2310_8BIT, 0x350b, 0x40},
	{SC2310_8BIT, 0x3601, 0x0a},
	{SC2310_8BIT, 0x3602, 0x38},
	{SC2310_8BIT, 0x3612, 0x80},
	{SC2310_8BIT, 0x3620, 0x54},
	{SC2310_8BIT, 0x3621, 0xc7},
	{SC2310_8BIT, 0x3622, 0x0f},
	{SC2310_8BIT, 0x3625, 0x10},
	{SC2310_8BIT, 0x3630, 0x55},
	{SC2310_8BIT, 0x3631, 0xf4},
	{SC2310_8BIT, 0x3632, 0x00},
	{SC2310_8BIT, 0x3633, 0x34},
	{SC2310_8BIT, 0x3634, 0x02},
	{SC2310_8BIT, 0x364d, 0x0d},
	{SC2310_8BIT, 0x364f, 0xdd},
	{SC2310_8BIT, 0x3660, 0x04},
	{SC2310_8BIT, 0x3662, 0x10},
	{SC2310_8BIT, 0x3663, 0xf1},
	{SC2310_8BIT, 0x3665, 0x00},
	{SC2310_8BIT, 0x3666, 0x20},
	{SC2310_8BIT, 0x3667, 0x00},
	{SC2310_8BIT, 0x366a, 0x80},
	{SC2310_8BIT, 0x3680, 0xe0},
	{SC2310_8BIT, 0x3681, 0x00},
	{SC2310_8BIT, 0x3700, 0x42},
	{SC2310_8BIT, 0x3701, 0x14},
	{SC2310_8BIT, 0x3702, 0xa0},
	{SC2310_8BIT, 0x3703, 0xd8},
	{SC2310_8BIT, 0x3704, 0x78},
	{SC2310_8BIT, 0x3705, 0x02},
	{SC2310_8BIT, 0x370a, 0x00},
	{SC2310_8BIT, 0x370b, 0x20},
	{SC2310_8BIT, 0x370c, 0x0c},
	{SC2310_8BIT, 0x370d, 0x11},
	{SC2310_8BIT, 0x370e, 0x00},
	{SC2310_8BIT, 0x370f, 0x40},
	{SC2310_8BIT, 0x3710, 0x00},
	{SC2310_8BIT, 0x371a, 0x1c},
	{SC2310_8BIT, 0x371b, 0x05},
	{SC2310_8BIT, 0x371c, 0x01},
	{SC2310_8BIT, 0x371e, 0xa1},
	{SC2310_8BIT, 0x371f, 0x0c},
	{SC2310_8BIT, 0x3721, 0x00},
	{SC2310_8BIT, 0x3724, 0x10},
	{SC2310_8BIT, 0x3726, 0x00},
	{SC2310_8BIT, 0x372a, 0x01},
	{SC2310_8BIT, 0x3730, 0x10},
	{SC2310_8BIT, 0x3738, 0x22},
	{SC2310_8BIT, 0x3739, 0xe5},
	{SC2310_8BIT, 0x373a, 0x50},
	{SC2310_8BIT, 0x373b, 0x02},
	{SC2310_8BIT, 0x373c, 0x41},
	{SC2310_8BIT, 0x373f, 0x02},
	{SC2310_8BIT, 0x3740, 0x42},
	{SC2310_8BIT, 0x3741, 0x02},
	{SC2310_8BIT, 0x3742, 0x18},
	{SC2310_8BIT, 0x3743, 0x01},
	{SC2310_8BIT, 0x3744, 0x02},
	{SC2310_8BIT, 0x3747, 0x10},
	{SC2310_8BIT, 0x374c, 0x04},
	{SC2310_8BIT, 0x3751, 0xf0},
	{SC2310_8BIT, 0x3752, 0x00},
	{SC2310_8BIT, 0x3753, 0x00},
	{SC2310_8BIT, 0x3754, 0xc0},
	{SC2310_8BIT, 0x3755, 0x00},
	{SC2310_8BIT, 0x3756, 0x1a},
	{SC2310_8BIT, 0x3758, 0x00},
	{SC2310_8BIT, 0x3759, 0x0f},
	{SC2310_8BIT, 0x376b, 0x44},
	{SC2310_8BIT, 0x375c, 0x04},
	{SC2310_8BIT, 0x3774, 0x10},
	{SC2310_8BIT, 0x3776, 0x00},
	{SC2310_8BIT, 0x377f, 0x08},
	{SC2310_8BIT, 0x3780, 0x22},
	{SC2310_8BIT, 0x3781, 0x0c},
	{SC2310_8BIT, 0x3784, 0x2c},
	{SC2310_8BIT, 0x3785, 0x1e},
	{SC2310_8BIT, 0x378f, 0xf5},
	{SC2310_8BIT, 0x3791, 0xb0},
	{SC2310_8BIT, 0x3795, 0x00},
	{SC2310_8BIT, 0x3796, 0x64},
	{SC2310_8BIT, 0x3797, 0x11},
	{SC2310_8BIT, 0x3798, 0x30},
	{SC2310_8BIT, 0x3799, 0x41},
	{SC2310_8BIT, 0x379a, 0x07},
	{SC2310_8BIT, 0x379b, 0xb0},
	{SC2310_8BIT, 0x379c, 0x0c},
	{SC2310_8BIT, 0x37c5, 0x00},
	{SC2310_8BIT, 0x37c6, 0x00},
	{SC2310_8BIT, 0x37c7, 0x00},
	{SC2310_8BIT, 0x37c9, 0x00},
	{SC2310_8BIT, 0x37ca, 0x00},
	{SC2310_8BIT, 0x37cb, 0x00},
	{SC2310_8BIT, 0x37de, 0x00},
	{SC2310_8BIT, 0x37df, 0x00},
	{SC2310_8BIT, 0x3800, 0x00},
	{SC2310_8BIT, 0x3801, 0x00},
	{SC2310_8BIT, 0x3802, 0x00},
	{SC2310_8BIT, 0x3804, 0x0a},
	{SC2310_8BIT, 0x3805, 0x3f},
	{SC2310_8BIT, 0x3810, 0x00},
	{SC2310_8BIT, 0x3812, 0x00},
	{SC2310_8BIT, 0x3823, 0x00},
	{SC2310_8BIT, 0x3824, 0x00},
	{SC2310_8BIT, 0x3825, 0x00},
	{SC2310_8BIT, 0x3826, 0x00},
	{SC2310_8BIT, 0x3827, 0x00},
	{SC2310_8BIT, 0x382a, 0x04},
	{SC2310_8BIT, 0x3a04, 0x06},
	{SC2310_8BIT, 0x3a05, 0x14},
	{SC2310_8BIT, 0x3a06, 0x00},
	{SC2310_8BIT, 0x3a07, 0xfe},
	{SC2310_8BIT, 0x3b00, 0x00},
	{SC2310_8BIT, 0x3b02, 0x00},
	{SC2310_8BIT, 0x3b03, 0x00},
	{SC2310_8BIT, 0x3b04, 0x00},
	{SC2310_8BIT, 0x3b05, 0x00},
	{SC2310_8BIT, 0x3e07, 0x20},
	{SC2310_8BIT, 0x4000, 0x08},
	{SC2310_8BIT, 0x4001, 0x04},
	{SC2310_8BIT, 0x4002, 0x45},
	{SC2310_8BIT, 0x4004, 0x08},
	{SC2310_8BIT, 0x4005, 0x18},
	{SC2310_8BIT, 0x4006, 0x20},
	{SC2310_8BIT, 0x4008, 0x24},
	{SC2310_8BIT, 0x4009, 0x10},
	{SC2310_8BIT, 0x400c, 0x00},
	{SC2310_8BIT, 0x400d, 0x00},
	{SC2310_8BIT, 0x4058, 0x00},
	{SC2310_8BIT, 0x404e, 0x37},
	{SC2310_8BIT, 0x404f, 0x8f},
	{SC2310_8BIT, 0x4058, 0x00},
	{SC2310_8BIT, 0x4101, 0xb2},
	{SC2310_8BIT, 0x4303, 0x00},
	{SC2310_8BIT, 0x4304, 0x08},
	{SC2310_8BIT, 0x4307, 0x31},
	{SC2310_8BIT, 0x4311, 0x04},
	{SC2310_8BIT, 0x4315, 0x01},
	{SC2310_8BIT, 0x4511, 0x05},
	{SC2310_8BIT, 0x4512, 0x01},
	{SC2310_8BIT, 0x4806, 0x00},
	{SC2310_8BIT, 0x4816, 0x52},
	{SC2310_8BIT, 0x481f, 0x30},
	{SC2310_8BIT, 0x4826, 0x2c},
	{SC2310_8BIT, 0x4831, 0x64},
	{SC2310_8BIT, 0x4d00, 0x04},
	{SC2310_8BIT, 0x4d01, 0x71},
	{SC2310_8BIT, 0x4d02, 0xfd},
	{SC2310_8BIT, 0x4d03, 0xf5},
	{SC2310_8BIT, 0x4d04, 0x0c},
	{SC2310_8BIT, 0x4d05, 0xcc},
	{SC2310_8BIT, 0x4837, 0x0a},
	{SC2310_8BIT, 0x5000, 0x06},
	{SC2310_8BIT, 0x5001, 0x01},
	{SC2310_8BIT, 0x5003, 0x20},
	{SC2310_8BIT, 0x5046, 0x0a},
	{SC2310_8BIT, 0x5013, 0x00},
	{SC2310_8BIT, 0x5046, 0x0a},
	{SC2310_8BIT, 0x5780, 0x1c},
	{SC2310_8BIT, 0x5786, 0x20},
	{SC2310_8BIT, 0x5787, 0x10},
	{SC2310_8BIT, 0x5788, 0x18},
	{SC2310_8BIT, 0x578a, 0x04},
	{SC2310_8BIT, 0x578b, 0x02},
	{SC2310_8BIT, 0x578c, 0x02},
	{SC2310_8BIT, 0x578e, 0x06},
	{SC2310_8BIT, 0x578f, 0x02},
	{SC2310_8BIT, 0x5790, 0x02},
	{SC2310_8BIT, 0x5791, 0xff},
	{SC2310_8BIT, 0x5842, 0x01},
	{SC2310_8BIT, 0x5843, 0x2b},
	{SC2310_8BIT, 0x5844, 0x01},
	{SC2310_8BIT, 0x5845, 0x92},
	{SC2310_8BIT, 0x5846, 0x01},
	{SC2310_8BIT, 0x5847, 0x8f},
	{SC2310_8BIT, 0x5848, 0x01},
	{SC2310_8BIT, 0x5849, 0x0c},
	{SC2310_8BIT, 0x5e00, 0x00},
	{SC2310_8BIT, 0x5e10, 0x0c},
	{SC2310_8BIT, 0x0100, 0x00},
	{SC2310_TOK_TERM, 0, 0}
};

/*
 * 654x496 30fps 17ms VBlanking 2lane 10Bit (Scaling)
 */
static struct sc2310_reg const sc2310_654x496[] = {
	{SC2310_8BIT, 0x3501, 0x3d},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe6},
	{SC2310_8BIT, 0x3709, 0xc7},
	{SC2310_8BIT, 0x3803, 0x00},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0xa3},
	{SC2310_8BIT, 0x3808, 0x02},
	{SC2310_8BIT, 0x3809, 0x90},
	{SC2310_8BIT, 0x380a, 0x01},
	{SC2310_8BIT, 0x380b, 0xf0},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x08},
	{SC2310_8BIT, 0x3813, 0x02},
	{SC2310_8BIT, 0x3814, 0x31},
	{SC2310_8BIT, 0x3815, 0x31},
	{SC2310_8BIT, 0x3820, 0x04},
	{SC2310_8BIT, 0x3821, 0x1f},
	{SC2310_8BIT, 0x5002, 0x80},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

/*
 * 1296x976 30fps 17ms VBlanking 2lane 10Bit (Scaling)
*DS from 2592x1952
*/
static struct sc2310_reg const sc2310_1296x976[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},

	{SC2310_8BIT, 0x3800, 0x00},
	{SC2310_8BIT, 0x3801, 0x00},
	{SC2310_8BIT, 0x3802, 0x00},
	{SC2310_8BIT, 0x3803, 0x00},

	{SC2310_8BIT, 0x3804, 0x0a},
	{SC2310_8BIT, 0x3805, 0x3f},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0xA3},

	{SC2310_8BIT, 0x3808, 0x05},
	{SC2310_8BIT, 0x3809, 0x10},
	{SC2310_8BIT, 0x380a, 0x03},
	{SC2310_8BIT, 0x380b, 0xD0},

	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},

	{SC2310_8BIT, 0x3810, 0x00},
	{SC2310_8BIT, 0x3811, 0x10},
	{SC2310_8BIT, 0x3812, 0x00},
	{SC2310_8BIT, 0x3813, 0x02},

	{SC2310_8BIT, 0x3814, 0x11},	/*X subsample control*/
	{SC2310_8BIT, 0x3815, 0x11},	/*Y subsample control*/
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_8BIT, 0x5041, 0x84}, /* scale is auto enabled */
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}

};


/*
 * 336x256 30fps 17ms VBlanking 2lane 10Bit (Scaling)
 DS from 2564x1956
 */
static struct sc2310_reg const sc2310_336x256[] = {
	{SC2310_8BIT, 0x3501, 0x3d},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe6},
	{SC2310_8BIT, 0x3709, 0xc7},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0xa3},
	{SC2310_8BIT, 0x3808, 0x01},
	{SC2310_8BIT, 0x3809, 0x50},
	{SC2310_8BIT, 0x380a, 0x01},
	{SC2310_8BIT, 0x380b, 0x00},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x1E},
	{SC2310_8BIT, 0x3814, 0x31},
	{SC2310_8BIT, 0x3815, 0x31},
	{SC2310_8BIT, 0x3820, 0x04},
	{SC2310_8BIT, 0x3821, 0x1f},
	{SC2310_8BIT, 0x5002, 0x80},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

/*
 * 336x256 30fps 17ms VBlanking 2lane 10Bit (Scaling)
 DS from 2368x1956
 */
static struct sc2310_reg const sc2310_368x304[] = {
	{SC2310_8BIT, 0x3501, 0x3d},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe6},
	{SC2310_8BIT, 0x3709, 0xc7},
	{SC2310_8BIT, 0x3808, 0x01},
	{SC2310_8BIT, 0x3809, 0x70},
	{SC2310_8BIT, 0x380a, 0x01},
	{SC2310_8BIT, 0x380b, 0x30},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x80},
	{SC2310_8BIT, 0x3814, 0x31},
	{SC2310_8BIT, 0x3815, 0x31},
	{SC2310_8BIT, 0x3820, 0x04},
	{SC2310_8BIT, 0x3821, 0x1f},
	{SC2310_8BIT, 0x5002, 0x80},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

/*
 * sc2310_192x160 30fps 17ms VBlanking 2lane 10Bit (Scaling)
 DS from 2460x1956
 */
static struct sc2310_reg const sc2310_192x160[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x80},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3804, 0x0a},
	{SC2310_8BIT, 0x3805, 0x3f},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0xA3},
	{SC2310_8BIT, 0x3808, 0x00},
	{SC2310_8BIT, 0x3809, 0xC0},
	{SC2310_8BIT, 0x380a, 0x00},
	{SC2310_8BIT, 0x380b, 0xA0},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x40},
	{SC2310_8BIT, 0x3813, 0x00},
	{SC2310_8BIT, 0x3814, 0x31},
	{SC2310_8BIT, 0x3815, 0x31},
	{SC2310_8BIT, 0x3820, 0x04},
	{SC2310_8BIT, 0x3821, 0x1f},
	{SC2310_8BIT, 0x5002, 0x80},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};


static struct sc2310_reg const sc2310_736x496[] = {
	{SC2310_8BIT, 0x3501, 0x3d},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe6},
	{SC2310_8BIT, 0x3709, 0xc7},
	{SC2310_8BIT, 0x3803, 0x68},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0x3b},
	{SC2310_8BIT, 0x3808, 0x02},
	{SC2310_8BIT, 0x3809, 0xe0},
	{SC2310_8BIT, 0x380a, 0x01},
	{SC2310_8BIT, 0x380b, 0xf0},
	{SC2310_8BIT, 0x380c, 0x0a}, /*hts*/
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07}, /*vts*/
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x08},
	{SC2310_8BIT, 0x3813, 0x02},
	{SC2310_8BIT, 0x3814, 0x31},
	{SC2310_8BIT, 0x3815, 0x31},
	{SC2310_8BIT, 0x3820, 0x04},
	{SC2310_8BIT, 0x3821, 0x1f},
	{SC2310_8BIT, 0x5002, 0x80},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

/*
static struct sc2310_reg const sc2310_736x496[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe6},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3803, 0x00},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0xa3},
	{SC2310_8BIT, 0x3808, 0x02},
	{SC2310_8BIT, 0x3809, 0xe0},
	{SC2310_8BIT, 0x380a, 0x01},
	{SC2310_8BIT, 0x380b, 0xf0},
	{SC2310_8BIT, 0x380c, 0x0d},
	{SC2310_8BIT, 0x380d, 0xb0},
	{SC2310_8BIT, 0x380e, 0x05},
	{SC2310_8BIT, 0x380f, 0xf2},
	{SC2310_8BIT, 0x3811, 0x08},
	{SC2310_8BIT, 0x3813, 0x02},
	{SC2310_8BIT, 0x3814, 0x31},
	{SC2310_8BIT, 0x3815, 0x31},
	{SC2310_8BIT, 0x3820, 0x01},
	{SC2310_8BIT, 0x3821, 0x1f},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};
*/
/*
 * 976x556 30fps 8.8ms VBlanking 2lane 10Bit (Scaling)
 */
static struct sc2310_reg const sc2310_976x556[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3803, 0xf0},
	{SC2310_8BIT, 0x3806, 0x06},
	{SC2310_8BIT, 0x3807, 0xa7},
	{SC2310_8BIT, 0x3808, 0x03},
	{SC2310_8BIT, 0x3809, 0xd0},
	{SC2310_8BIT, 0x380a, 0x02},
	{SC2310_8BIT, 0x380b, 0x2C},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x10},
	{SC2310_8BIT, 0x3813, 0x02},
	{SC2310_8BIT, 0x3814, 0x11},
	{SC2310_8BIT, 0x3815, 0x11},
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x80},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

/*DS from 2624x1492*/
static struct sc2310_reg const sc2310_1296x736[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},

	{SC2310_8BIT, 0x3800, 0x00},
	{SC2310_8BIT, 0x3801, 0x00},
	{SC2310_8BIT, 0x3802, 0x00},
	{SC2310_8BIT, 0x3803, 0x00},

	{SC2310_8BIT, 0x3804, 0x0a},
	{SC2310_8BIT, 0x3805, 0x3f},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0xA3},

	{SC2310_8BIT, 0x3808, 0x05},
	{SC2310_8BIT, 0x3809, 0x10},
	{SC2310_8BIT, 0x380a, 0x02},
	{SC2310_8BIT, 0x380b, 0xe0},

	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},

	{SC2310_8BIT, 0x3813, 0xE8},

	{SC2310_8BIT, 0x3814, 0x11},	/*X subsample control*/
	{SC2310_8BIT, 0x3815, 0x11},	/*Y subsample control*/
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_8BIT, 0x5041, 0x84}, /* scale is auto enabled */
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

static struct sc2310_reg const sc2310_1636p_30fps[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3803, 0xf0},
	{SC2310_8BIT, 0x3806, 0x06},
	{SC2310_8BIT, 0x3807, 0xa7},
	{SC2310_8BIT, 0x3808, 0x06},
	{SC2310_8BIT, 0x3809, 0x64},
	{SC2310_8BIT, 0x380a, 0x04},
	{SC2310_8BIT, 0x380b, 0x48},
	{SC2310_8BIT, 0x380c, 0x0a}, /*hts*/
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07}, /*vts*/
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x02},
	{SC2310_8BIT, 0x3813, 0x02},
	{SC2310_8BIT, 0x3814, 0x11},
	{SC2310_8BIT, 0x3815, 0x11},
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x80},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

static struct sc2310_reg const sc2310_1616x1216_30fps[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x80},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3800, 0x00},	/*{3800,3801} Array X start*/
	{SC2310_8BIT, 0x3801, 0x08},	/* 04 //{3800,3801} Array X start*/
	{SC2310_8BIT, 0x3802, 0x00},	/*{3802,3803} Array Y start*/
	{SC2310_8BIT, 0x3803, 0x04},	/* 00  //{3802,3803} Array Y start*/
	{SC2310_8BIT, 0x3804, 0x0a},	/*{3804,3805} Array X end*/
	{SC2310_8BIT, 0x3805, 0x37},	/* 3b  //{3804,3805} Array X end*/
	{SC2310_8BIT, 0x3806, 0x07},	/*{3806,3807} Array Y end*/
	{SC2310_8BIT, 0x3807, 0x9f},	/* a3  //{3806,3807} Array Y end*/
	{SC2310_8BIT, 0x3808, 0x06},	/*{3808,3809} Final output H size*/
	{SC2310_8BIT, 0x3809, 0x50},	/*{3808,3809} Final output H size*/
	{SC2310_8BIT, 0x380a, 0x04},	/*{380a,380b} Final output V size*/
	{SC2310_8BIT, 0x380b, 0xc0},	/*{380a,380b} Final output V size*/
	{SC2310_8BIT, 0x380c, 0x0a},	/*{380c,380d} HTS*/
	{SC2310_8BIT, 0x380d, 0x80},	/*{380c,380d} HTS*/
	{SC2310_8BIT, 0x380e, 0x07},	/*{380e,380f} VTS*/
	{SC2310_8BIT, 0x380f, 0xc0},	/* bc	//{380e,380f} VTS*/
	{SC2310_8BIT, 0x3810, 0x00},	/*{3810,3811} windowing X offset*/
	{SC2310_8BIT, 0x3811, 0x10},	/*{3810,3811} windowing X offset*/
	{SC2310_8BIT, 0x3812, 0x00},	/*{3812,3813} windowing Y offset*/
	{SC2310_8BIT, 0x3813, 0x06},	/*{3812,3813} windowing Y offset*/
	{SC2310_8BIT, 0x3814, 0x11},	/*X subsample control*/
	{SC2310_8BIT, 0x3815, 0x11},	/*Y subsample control*/
	{SC2310_8BIT, 0x3820, 0x00},	/*FLIP/Binnning control*/
	{SC2310_8BIT, 0x3821, 0x1e},	/*MIRROR control*/
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_8BIT, 0x5041, 0x84},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};


/*
 * 1940x1096 30fps 8.8ms VBlanking 2lane 10bit (Scaling)
 */
static struct sc2310_reg const sc2310_1940x1096[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3803, 0xf0},
	{SC2310_8BIT, 0x3806, 0x06},
	{SC2310_8BIT, 0x3807, 0xa7},
	{SC2310_8BIT, 0x3808, 0x07},
	{SC2310_8BIT, 0x3809, 0x94},
	{SC2310_8BIT, 0x380a, 0x04},
	{SC2310_8BIT, 0x380b, 0x48},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x02},
	{SC2310_8BIT, 0x3813, 0x02},
	{SC2310_8BIT, 0x3814, 0x11},
	{SC2310_8BIT, 0x3815, 0x11},
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x80},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

static struct sc2310_reg const sc2310_2592x1456_30fps[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3800, 0x00},
	{SC2310_8BIT, 0x3801, 0x00},
	{SC2310_8BIT, 0x3802, 0x00},
	{SC2310_8BIT, 0x3803, 0xf0},
	{SC2310_8BIT, 0x3804, 0x0a},
	{SC2310_8BIT, 0x3805, 0x3f},
	{SC2310_8BIT, 0x3806, 0x06},
	{SC2310_8BIT, 0x3807, 0xa4},
	{SC2310_8BIT, 0x3808, 0x0a},
	{SC2310_8BIT, 0x3809, 0x20},
	{SC2310_8BIT, 0x380a, 0x05},
	{SC2310_8BIT, 0x380b, 0xb0},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x10},
	{SC2310_8BIT, 0x3813, 0x00},
	{SC2310_8BIT, 0x3814, 0x11},
	{SC2310_8BIT, 0x3815, 0x11},
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_TOK_TERM, 0, 0}
};

static struct sc2310_reg const sc2310_2576x1456_30fps[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3800, 0x00},
	{SC2310_8BIT, 0x3801, 0x00},
	{SC2310_8BIT, 0x3802, 0x00},
	{SC2310_8BIT, 0x3803, 0xf0},
	{SC2310_8BIT, 0x3804, 0x0a},
	{SC2310_8BIT, 0x3805, 0x3f},
	{SC2310_8BIT, 0x3806, 0x06},
	{SC2310_8BIT, 0x3807, 0xa4},
	{SC2310_8BIT, 0x3808, 0x0a},
	{SC2310_8BIT, 0x3809, 0x10},
	{SC2310_8BIT, 0x380a, 0x05},
	{SC2310_8BIT, 0x380b, 0xb0},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x18},
	{SC2310_8BIT, 0x3813, 0x00},
	{SC2310_8BIT, 0x3814, 0x11},
	{SC2310_8BIT, 0x3815, 0x11},
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_TOK_TERM, 0, 0}
};

/*
 * 2592x1944 30fps 0.6ms VBlanking 2lane 10Bit
 */
static struct sc2310_reg const sc2310_2592x1944_30fps[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3803, 0x00},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0xa3},
	{SC2310_8BIT, 0x3808, 0x0a},
	{SC2310_8BIT, 0x3809, 0x20},
	{SC2310_8BIT, 0x380a, 0x07},
	{SC2310_8BIT, 0x380b, 0x98},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x10},
	{SC2310_8BIT, 0x3813, 0x00},
	{SC2310_8BIT, 0x3814, 0x11},
	{SC2310_8BIT, 0x3815, 0x11},
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

/*
 * 11:9 Full FOV Output, expected FOV Res: 2346x1920
 * ISP Effect Res: 1408x1152
 * Sensor out: 1424x1168, DS From: 2380x1952
 *
 * WA: Left Offset: 8, Hor scal: 64
 */
static struct sc2310_reg const sc2310_1424x1168_30fps[] = {
	{SC2310_8BIT, 0x3501, 0x3b}, /* long exposure[15:8] */
	{SC2310_8BIT, 0x3502, 0x80}, /* long exposure[7:0] */
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3800, 0x00}, /* TIMING_X_ADDR_START */
	{SC2310_8BIT, 0x3801, 0x50}, /* 80 */
	{SC2310_8BIT, 0x3802, 0x00}, /* TIMING_Y_ADDR_START */
	{SC2310_8BIT, 0x3803, 0x02}, /* 2 */
	{SC2310_8BIT, 0x3804, 0x09}, /* TIMING_X_ADDR_END */
	{SC2310_8BIT, 0x3805, 0xdd}, /* 2525 */
	{SC2310_8BIT, 0x3806, 0x07}, /* TIMING_Y_ADDR_END */
	{SC2310_8BIT, 0x3807, 0xa1}, /* 1953 */
	{SC2310_8BIT, 0x3808, 0x05}, /* TIMING_X_OUTPUT_SIZE */
	{SC2310_8BIT, 0x3809, 0x90}, /* 1424 */
	{SC2310_8BIT, 0x380a, 0x04}, /* TIMING_Y_OUTPUT_SIZE */
	{SC2310_8BIT, 0x380b, 0x90}, /* 1168 */
	{SC2310_8BIT, 0x380c, 0x0a}, /* TIMING_HTS */
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07}, /* TIMING_VTS */
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3810, 0x00}, /* TIMING_ISP_X_WIN */
	{SC2310_8BIT, 0x3811, 0x02}, /* 2 */
	{SC2310_8BIT, 0x3812, 0x00}, /* TIMING_ISP_Y_WIN */
	{SC2310_8BIT, 0x3813, 0x00}, /* 0 */
	{SC2310_8BIT, 0x3814, 0x11}, /* TIME_X_INC */
	{SC2310_8BIT, 0x3815, 0x11}, /* TIME_Y_INC */
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_8BIT, 0x5041, 0x84}, /* scale is auto enabled */
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

/*
 * 3:2 Full FOV Output, expected FOV Res: 2560x1706
 * ISP Effect Res: 720x480
 * Sensor out: 736x496, DS From 2616x1764
 */
static struct sc2310_reg const sc2310_736x496_30fps[] = {
	{SC2310_8BIT, 0x3501, 0x3b}, /* long exposure[15:8] */
	{SC2310_8BIT, 0x3502, 0x80}, /* long exposure[7:0] */
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3800, 0x00}, /* TIMING_X_ADDR_START */
	{SC2310_8BIT, 0x3801, 0x02}, /* 2 */
	{SC2310_8BIT, 0x3802, 0x00}, /* TIMING_Y_ADDR_START */
	{SC2310_8BIT, 0x3803, 0x62}, /* 98 */
	{SC2310_8BIT, 0x3804, 0x0a}, /* TIMING_X_ADDR_END */
	{SC2310_8BIT, 0x3805, 0x3b}, /* 2619 */
	{SC2310_8BIT, 0x3806, 0x07}, /* TIMING_Y_ADDR_END */
	{SC2310_8BIT, 0x3807, 0x43}, /* 1859 */
	{SC2310_8BIT, 0x3808, 0x02}, /* TIMING_X_OUTPUT_SIZE */
	{SC2310_8BIT, 0x3809, 0xe0}, /* 736 */
	{SC2310_8BIT, 0x380a, 0x01}, /* TIMING_Y_OUTPUT_SIZE */
	{SC2310_8BIT, 0x380b, 0xf0}, /* 496 */
	{SC2310_8BIT, 0x380c, 0x0a}, /* TIMING_HTS */
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07}, /* TIMING_VTS */
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3810, 0x00}, /* TIMING_ISP_X_WIN */
	{SC2310_8BIT, 0x3811, 0x02}, /* 2 */
	{SC2310_8BIT, 0x3812, 0x00}, /* TIMING_ISP_Y_WIN */
	{SC2310_8BIT, 0x3813, 0x00}, /* 0 */
	{SC2310_8BIT, 0x3814, 0x11}, /* TIME_X_INC */
	{SC2310_8BIT, 0x3815, 0x11}, /* TIME_Y_INC */
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_8BIT, 0x5041, 0x84}, /* scale is auto enabled */
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

static struct sc2310_reg const sc2310_2576x1936_30fps[] = {
	{SC2310_8BIT, 0x3501, 0x7b},
	{SC2310_8BIT, 0x3502, 0x00},
	{SC2310_8BIT, 0x3708, 0xe2},
	{SC2310_8BIT, 0x3709, 0xc3},
	{SC2310_8BIT, 0x3803, 0x00},
	{SC2310_8BIT, 0x3806, 0x07},
	{SC2310_8BIT, 0x3807, 0xa3},
	{SC2310_8BIT, 0x3808, 0x0a},
	{SC2310_8BIT, 0x3809, 0x10},
	{SC2310_8BIT, 0x380a, 0x07},
	{SC2310_8BIT, 0x380b, 0x90},
	{SC2310_8BIT, 0x380c, 0x0a},
	{SC2310_8BIT, 0x380d, 0x80},
	{SC2310_8BIT, 0x380e, 0x07},
	{SC2310_8BIT, 0x380f, 0xc0},
	{SC2310_8BIT, 0x3811, 0x18},
	{SC2310_8BIT, 0x3813, 0x00},
	{SC2310_8BIT, 0x3814, 0x11},
	{SC2310_8BIT, 0x3815, 0x11},
	{SC2310_8BIT, 0x3820, 0x00},
	{SC2310_8BIT, 0x3821, 0x1e},
	{SC2310_8BIT, 0x5002, 0x00},
	{SC2310_8BIT, 0x0100, 0x01},
	{SC2310_TOK_TERM, 0, 0}
};

struct sc2310_resolution sc2310_res_preview[] = {
	{
		.desc = "sc2310_736x496_30fps",
		.width = 736,
		.height = 496,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_736x496_30fps,
	},
	{
		.desc = "sc2310_1616x1216_30fps",
		.width = 1616,
		.height = 1216,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_1616x1216_30fps,
	},
	{
		.desc = "sc2310_5M_30fps",
		.width = 2576,
		.height = 1456,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_2576x1456_30fps,
	},
	{
		.desc = "sc2310_5M_30fps",
		.width = 2576,
		.height = 1936,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_2576x1936_30fps,
	},
};
#define N_RES_PREVIEW (ARRAY_SIZE(sc2310_res_preview))

struct sc2310_resolution sc2310_res_still[] = {
	{
		.desc = "sc2310_736x496_30fps",
		.width = 736,
		.height = 496,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_736x496_30fps,
	},
	{
		.desc = "sc2310_1424x1168_30fps",
		.width = 1424,
		.height = 1168,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_1424x1168_30fps,
	},
	{
		.desc = "sc2310_1616x1216_30fps",
		.width = 1616,
		.height = 1216,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_1616x1216_30fps,
	},
	{
		.desc = "sc2310_5M_30fps",
		.width = 2592,
		.height = 1456,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_2592x1456_30fps,
	},
	{
		.desc = "sc2310_5M_30fps",
		.width = 2592,
		.height = 1944,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_2592x1944_30fps,
	},
};
#define N_RES_STILL (ARRAY_SIZE(sc2310_res_still))

struct sc2310_resolution sc2310_res_video[] = {
	{
		.desc = "sc2310_736x496_30fps",
		.width = 736,
		.height = 496,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 1,
		.regs = sc2310_736x496,
	},
	{
		.desc = "sc2310_336x256_30fps",
		.width = 336,
		.height = 256,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 1,
		.regs = sc2310_336x256,
	},
	{
		.desc = "sc2310_368x304_30fps",
		.width = 368,
		.height = 304,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 1,
		.regs = sc2310_368x304,
	},
	{
		.desc = "sc2310_192x160_30fps",
		.width = 192,
		.height = 160,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 1,
		.regs = sc2310_192x160,
	},
	{
		.desc = "sc2310_1296x736_30fps",
		.width = 1296,
		.height = 736,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 0,
		.regs = sc2310_1296x736,
	},
	{
		.desc = "sc2310_1296x976_30fps",
		.width = 1296,
		.height = 976,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 0,
		.regs = sc2310_1296x976,
	},
	{
		.desc = "sc2310_1636P_30fps",
		.width = 1636,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_1636p_30fps,
	},
	{
		.desc = "sc2310_1080P_30fps",
		.width = 1940,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_1940x1096,
	},
	{
		.desc = "sc2310_5M_30fps",
		.width = 2592,
		.height = 1456,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_2592x1456_30fps,
	},
	{
		.desc = "sc2310_5M_30fps",
		.width = 2592,
		.height = 1944,
		.pix_clk_freq = 160,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = sc2310_2592x1944_30fps,
	},
};

#define N_RES_VIDEO (ARRAY_SIZE(sc2310_res_video))

static struct sc2310_resolution *sc2310_res = sc2310_res_preview;
static unsigned long N_RES = N_RES_PREVIEW;
#endif
#endif
