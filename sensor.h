#ifndef SENSOR_H
#define SENSOR_H

/* Module to define sensor functions and interrupt 
handling. Uses the LSM6DS3

Edited by: Sebastian Russo <sebrusso@stanford.edu>, 
         Nick Reisner <nreisner@stanford.edu>,
         Devon Smith <devon2@stanford.edu>.

Last Update: 12/15/2022

Original Author: Whoever created LSM6DS3 library in lectures/sensors/accel

*/


/*
* Sensor address for all functions on the sensor.
*/

#define LSM6DS3_ADDRESS	0x6B
// this was previously 0x6A, check this again

/*
     LSM6DS33-AN4682.pdf
		p8
 */

// register addresses
enum regAddr
{
      FUNC_CFG_ACCESS   = 0x01,

      FIFO_CTRL1        = 0x06,
      FIFO_CTRL2        = 0x07,
      FIFO_CTRL3        = 0x08,
      FIFO_CTRL4        = 0x09,
      FIFO_CTRL5        = 0x0A,
      ORIENT_CFG_G      = 0x0B,

      INT1_CTRL         = 0x0D,
      INT2_CTRL         = 0x0E,
      WHO_AM_I          = 0x0F,
      CTRL1_XL          = 0x10,
      CTRL2_G           = 0x11,
      CTRL3_C           = 0x12,
      CTRL4_C           = 0x13,
      CTRL5_C           = 0x14,
      CTRL6_C           = 0x15,
      CTRL7_G           = 0x16,
      CTRL8_XL          = 0x17,
      CTRL9_XL          = 0x18,
      CTRL10_C          = 0x19,

      WAKE_UP_SRC       = 0x1B,
      TAP_SRC           = 0x1C,
      D6D_SRC           = 0x1D,
      STATUS_REG        = 0x1E,

      OUT_TEMP_L        = 0x20,
      OUT_TEMP_H        = 0x21,
      OUTX_L_G          = 0x22,
      OUTX_H_G          = 0x23,
      OUTY_L_G          = 0x24,
      OUTY_H_G          = 0x25,
      OUTZ_L_G          = 0x26,
      OUTZ_H_G          = 0x27,
      OUTX_L_XL         = 0x28,
      OUTX_H_XL         = 0x29,
      OUTY_L_XL         = 0x2A,
      OUTY_H_XL         = 0x2B,
      OUTZ_L_XL         = 0x2C,
      OUTZ_H_XL         = 0x2D,

      FIFO_STATUS1      = 0x3A,
      FIFO_STATUS2      = 0x3B,
      FIFO_STATUS3      = 0x3C,
      FIFO_STATUS4      = 0x3D,
      FIFO_DATA_OUT_L   = 0x3E,
      FIFO_DATA_OUT_H   = 0x3F,
      TIMESTAMP0_REG    = 0x40,
      TIMESTAMP1_REG    = 0x41,
      TIMESTAMP2_REG    = 0x42,

      STEP_TIMESTAMP_L  = 0x49,
      STEP_TIMESTAMP_H  = 0x4A,
      STEP_COUNTER_L    = 0x4B,
      STEP_COUNTER_H    = 0x4C,

      FUNC_SRC          = 0x53,

      TAP_CFG           = 0x58,
      TAP_THS_6D        = 0x59,
      INT_DUR2          = 0x5A,
      WAKE_UP_THS       = 0x5B,
      WAKE_UP_DUR       = 0x5C,
      FREE_FALL         = 0x5D,
      MD1_CFG           = 0x5E,
      MD2_CFG           = 0x5F,
};

void lsm6ds33_init();

void lsm6ds33_write_reg(unsigned char reg, unsigned char v);
unsigned lsm6ds33_read_reg(unsigned char reg);

unsigned lsm6ds33_get_whoami(); 

void lsm6ds33_enable_gyroscope();
void lsm6ds33_read_gyroscope(short *x, short *y, short *z);

void lsm6ds33_enable_accelerometer();
void lsm6ds33_read_accelerometer(short *x, short *y, short *z);

/* New functions for Tetris hand glove implementation. */

// struct to hold all sensor info
typedef struct sensor_info sensor_info_t;

// init functions for sesnor: both required 
void sensor_init(sensor_info_t *sensor);
sensor_info_t *sensor_new(void);

// internal function to get the avg of all elems in current rb and dequeue them
short rb_get_average(void *rb);
short sensor_get_xAccel_Avg(sensor_info_t *sensor);
short sensor_get_yAccel_Avg(sensor_info_t *sensor);
short sensor_get_zAccel_Avg(sensor_info_t *sensor);

// calibrate sensor
void sensor_calibrate(sensor_info_t *sensor);
void sensor_recalibrate(sensor_info_t *sensor);

// printing calibration values 
void sensor_print_calibration(sensor_info_t *sensor);
void sensor_print_calibration_y_acc(sensor_info_t *sensor);
void sensor_print_calibration_x_acc(sensor_info_t *sensor);
void sensor_print_calibration_z_acc(sensor_info_t *sensor);


// read sensor vals
void sensor_read(sensor_info_t *sensor);

// check if sensor is in motion
bool sensor_right(short x_accel, sensor_info_t *sensor);
bool sensor_left(short x_accel, sensor_info_t *sensor);
bool sensor_up(short y_accel, sensor_info_t *sensor);
bool sensor_down(short y_accel, sensor_info_t *sensor);

#endif
