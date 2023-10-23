#include "i2c.c"
#include "uart.h"
#include "mymodule.h"
#include "gl.h"
#include "strings.h"
#include "gpio.h"
#include "timer.h"
#include "printf.h"
#include "sensor.h"
#include "malloc.h"
#include "ringbuffer.h"
#include <assert.h>


/* Levers for fine-tuning sensitivy and performance */
#define RIGHT_MOVE_MULTIPLIER 100
#define LEFT_MOVE_MULTIPLIER 100
#define UP_MOVE_MULTIPLIER 100 
#define DOWN_MOVE_MULTIPLIER 100
#define COOLDOWN_TIME 25


/* The following functions are taken from lectures/sensors/accel:
We attribute all of the following code to that author! */

const unsigned lsm6ds33_address = 0b1101011; // this is for the gyro/accel


void lsm6ds33_write_reg(unsigned char reg, unsigned char v) {
	char data[2] = {reg, v};
	i2c_write(lsm6ds33_address, data, 2);
}

unsigned lsm6ds33_read_reg(unsigned char reg) {
	i2c_write(lsm6ds33_address, (void*) &reg, 1);
	unsigned char uc = 0;
	i2c_read(lsm6ds33_address, (void*) &uc, 1);
	return uc;
}

void lsm6ds33_init() {
	lsm6ds33_write_reg(CTRL2_G, 0x80);   // 1600Hz (high perf mode)
	lsm6ds33_write_reg(CTRL1_XL, 0x80);  // 1600Hz (high perf mode)
}

unsigned lsm6ds33_get_whoami() {
	// should return 0x69 ...
    return lsm6ds33_read_reg(WHO_AM_I);
}

// default is enabled
void lsm6ds33_enable_gyroscope() {
	// gyroscope _G registers
	lsm6ds33_write_reg(CTRL10_C, 0x38);  // GYRO: x,y,z enabled (bits 4-6)
}

// default is enabled
void lsm6ds33_enable_accelerometer() {
    // accelerator _XL registers
	lsm6ds33_write_reg(CTRL9_XL, 0x38);  // ACCEL: x,y,z enabled (bits 4-6)
}

void lsm6ds33_read_gyroscope(short *x, short *y, short *z) {
    *x =  lsm6ds33_read_reg(OUTX_L_G);
    *x |= lsm6ds33_read_reg(OUTX_H_G) << 8;

    *y =  lsm6ds33_read_reg(OUTY_L_G);
    *y |= lsm6ds33_read_reg(OUTY_H_G) << 8;

    *z =  lsm6ds33_read_reg(OUTZ_L_G);
    *z |= lsm6ds33_read_reg(OUTZ_H_G) << 8;
}

void lsm6ds33_read_accelerometer(short *x, short *y, short *z) {
    *x =  lsm6ds33_read_reg(OUTX_L_XL);
    *x |= lsm6ds33_read_reg(OUTX_H_XL) << 8;

    *y =  lsm6ds33_read_reg(OUTY_L_XL);
    *y |= lsm6ds33_read_reg(OUTY_H_XL) << 8;

    *z =  lsm6ds33_read_reg(OUTZ_L_XL);
    *z |= lsm6ds33_read_reg(OUTZ_H_XL) << 8;
}

/* The following functions are written as part of an original contribution
for the Tetris glove implementation. */

/* This struct will hold the sensor information! 
Void pointers will eventually point to ringbuffers, and 
the calibration levels are going to act as the basis for firing
a command on the tetris game. */
struct sensor_info { // sensor_info_t
    bool isOn;
    bool isCalibrated;
    short calibratedLevels[6]; // 0-2: x, y, z accel; -5: x, y, z gyro

    // These will point to ring buffers for current readings
    // These didn't have to be ring buffers, but I wanted to use 
    // because of their FIFO nature. 
    rb_t *x_accel;
    rb_t *y_accel;
    rb_t *z_accel;
    rb_t *x_gyro; 
    rb_t *y_gyro;
    rb_t *z_gyro;

    // These will point to the ring buffer for average readings. 
    // These rb_t's are necesasary for keeping track of the running 
    // average, where window size is 500. (number of elems in rb_t)
    rb_t *x_accel_avg;
    rb_t *y_accel_avg;
    rb_t *z_accel_avg;

    short running_xAccel_Sum; // running sum for the average
    short running_yAccel_Sum; // running sum for the average
    short running_zAccel_Sum; // running sum for the average

    int numElemsX; // number of elements in the average x_accel
    int numElemsY; // number of elements in the average z_accel
    int numElemsZ; // number of elements in the average y_accel
};

/* Initializes the sensor by preparing
the ring buffers and the sensor itself */
void sensor_init(sensor_info_t *sensor){
    
    lsm6ds33_init();

    sensor->x_accel = rb_new();
    sensor->y_accel = rb_new();
    sensor->z_accel = rb_new();
    sensor->x_gyro = rb_new();
    sensor->y_gyro = rb_new();
    sensor->z_gyro = rb_new();

    sensor->x_accel_avg = rb_new();
    sensor->y_accel_avg = rb_new();
    sensor->z_accel_avg = rb_new();
}

/* Build a sensor_t type and initialize its data */
sensor_info_t *sensor_new(void) {
    sensor_info_t *sensor = malloc(sizeof(struct sensor_info));
    sensor->isOn = false;
    sensor->isCalibrated = false;


    sensor->running_xAccel_Sum = 0;
    sensor->numElemsX = 0;

    sensor->running_yAccel_Sum = 0;
    sensor->numElemsY = 0;

    sensor->running_zAccel_Sum = 0;
    sensor->numElemsZ = 0;

    // assert we recieve the correct whoami value
    assert(lsm6ds33_get_whoami() == 0x69);

    return sensor;
}

/* This function dequeues elems from the rb until it is empty. 
Then, it goes over the number of elems in the array 
and gets the average value */
short rb_get_average(void *rb) {
    
    int sum = 0; // running sum value
    int elem = 0; // elem
    int *p_elem = &elem; // pointer to elem
    rb_t *ringb = (rb_t *)rb;  // cast to rb_t type

    int i = 0; // number of elems in the rb
    while (!rb_empty(ringb)) {
        rb_dequeue(ringb, p_elem);
        sum += *p_elem;
        i++;
    }

    return sum / i;
}

/* Convenience function to get the average of x_acc 
rb, takes the sensor and returns a short value of the 
average */
short sensor_get_xAccel_Avg(sensor_info_t *sensor) {
    return rb_get_average(sensor->x_accel);
}

/* Convenience function to get the average of y_acc 
rb, takes the sensor and returns a short value of the 
average */
short sensor_get_yAccel_Avg(sensor_info_t *sensor) {
    return rb_get_average(sensor->y_accel);
}

/* Convenience function to get the average of z_acc 
rb, takes the sensor and returns a short value of the 
average */
short sensor_get_zAccel_Avg(sensor_info_t *sensor) {
    return rb_get_average(sensor->z_accel);
}


/* This function calibrates the sensor by reading the current values 
for accelerometer and gyroscope and storing them in the sensor struct. 
It does this by enqueing values in each ringbuffer until they have 100 values, 
and then taking the average of those values in order to have a calibrated
threshold. */
void sensor_calibrate(sensor_info_t *sensor) {
    short x_accel, y_accel, z_accel, x_gyro, y_gyro, z_gyro;
    int i = 0;

    while (i < 100) {
        lsm6ds33_read_accelerometer(&x_accel, &y_accel, &z_accel);
        lsm6ds33_read_gyroscope(&x_gyro, &y_gyro, &z_gyro);
        rb_enqueue((rb_t *)sensor->x_accel, x_accel / 16);
        rb_enqueue((rb_t *)sensor->y_accel, y_accel / 16);
        rb_enqueue((rb_t *)sensor->z_accel, z_accel / 16);
        rb_enqueue((rb_t *)sensor->x_gyro, x_gyro);
        rb_enqueue((rb_t *)sensor->y_gyro, y_gyro);
        rb_enqueue((rb_t *)sensor->z_gyro, z_gyro);
        i++;
    }

    sensor->calibratedLevels[0] = rb_get_average(sensor->x_accel);
    sensor->calibratedLevels[1] = rb_get_average(sensor->y_accel);
    sensor->calibratedLevels[2] = rb_get_average(sensor->z_accel);
    sensor->calibratedLevels[3] = rb_get_average(sensor->x_gyro);
    sensor->calibratedLevels[4] = rb_get_average(sensor->y_gyro);
    sensor->calibratedLevels[5] = rb_get_average(sensor->z_gyro);

    sensor->isCalibrated = true;
}

/* This function reads 5 current values for accelerometer and gyroscope
and enqueues them in the ringbuffers. */
void sensor_read(sensor_info_t *sensor) {
    short x_accel, y_accel, z_accel, x_gyro, y_gyro, z_gyro;
    int i = 0;

    while (i < 2) {
        lsm6ds33_read_accelerometer(&x_accel, &y_accel, &z_accel);
        lsm6ds33_read_gyroscope(&x_gyro, &y_gyro, &z_gyro);

        // convert from LSB vals to mg, where 16384 is 1g, and 1g == 100mg
        // so, dividing by 16 approximates the mg value
        rb_enqueue(sensor->x_accel, x_accel / 16);
        rb_enqueue(sensor->y_accel, y_accel / 16);
        rb_enqueue(sensor->z_accel, z_accel / 16);

        // not going to do any conversion for gyro vals
        rb_enqueue(sensor->x_gyro, x_gyro);
        rb_enqueue(sensor->y_gyro, y_gyro);
        rb_enqueue(sensor->z_gyro, z_gyro);
        i++;
    }
}


/* This function continuously recalibrates the 
calibrated x_acc and y_acc for the sensor by enqueuing 5 values,
adding those values to the running sum, and then dividing
by the number of values in the ringbuffer. */
void sensor_recalibrate(sensor_info_t *sensor) {
    short x_accel, y_accel, z_accel;

    int elem = 0; // for dequeueing
    int *p_elem = &elem; // for dequeueing
    int i = 0;

    while (i < 5) {
        lsm6ds33_read_accelerometer(&x_accel, &y_accel, &z_accel); // we only care about x_accel

        // x_acc_avg
        if (rb_full(sensor->x_accel_avg)) { // if the rb is full, dequeue an elem
            rb_dequeue(sensor->x_accel_avg, p_elem);
            sensor->running_xAccel_Sum -= *p_elem; // subtract the dequeued elem from the running sum
        }
        else {
            sensor->numElemsX++; // if the rb is not full, increment the number of elems
        }
        sensor->running_xAccel_Sum += (x_accel / 16); // add the new elem to the running sum
        rb_enqueue(sensor->x_accel_avg, x_accel / 16); 

        // y_acc_avg
        if (rb_full(sensor->y_accel_avg)) { // if the rb is full, dequeue an elem
            rb_dequeue(sensor->y_accel_avg, p_elem);
            sensor->running_yAccel_Sum -= *p_elem; // subtract the dequeued elem from the running sum
        }
        else {
            sensor->numElemsY++; // if the rb is not full, increment the number of elems
        }
        sensor->running_yAccel_Sum += (y_accel / 16); // add the new elem to the running sum
        rb_enqueue(sensor->y_accel_avg, y_accel / 16);

        // z_acc_avg
         if (rb_full(sensor->z_accel_avg)) { // if the rb is full, dequeue an elem
             rb_dequeue(sensor->z_accel_avg, p_elem);
            sensor->running_zAccel_Sum -= *p_elem; // subtract the dequeued elem from the running sum
         }
         else {
             sensor->numElemsZ++; // if the rb is not full, increment the number of elems
         }
        sensor->running_zAccel_Sum += (z_accel / 16); // add the new elem to the running sum
        rb_enqueue(sensor->z_accel_avg, z_accel / 16);

        i++; // increment the counter
    }

    // after the loop, the running sum has 5 new elems, so we can take the average
    sensor->calibratedLevels[0] = (sensor->running_xAccel_Sum / sensor->numElemsX);
    sensor->calibratedLevels[1] = (sensor->running_yAccel_Sum / sensor->numElemsY);
    sensor->calibratedLevels[2] = (sensor->running_zAccel_Sum / sensor->numElemsZ);

    return;
}


/* This function prints calibration levels for all acc */
void sensor_print_calibration(sensor_info_t *sensor) {
    printf("x_accel: %d", sensor->calibratedLevels[0]);
    printf("y_accel: %d", sensor->calibratedLevels[1]);
    printf("z_accel: %d", sensor->calibratedLevels[2]);
    // printf("x_gyro: %d", sensor->calibratedLevels[3]);
    // printf("y_gyro: %d", sensor->calibratedLevels[4]);
    // printf("z_gyro: %d", sensor->calibratedLevels[5]);
}

/* This function prints the calibration level for x_acc */
void sensor_print_calibration_x_acc(sensor_info_t *sensor) {
    printf("x_accel: %d", sensor->calibratedLevels[0]);
}

/* This function prints the calibration level for y_acc */
void sensor_print_calibration_y_acc(sensor_info_t *sensor) {
    printf("y_accel: %d", sensor->calibratedLevels[1]);
}

/* This function prints the calibration level for z_acc */
void sensor_print_calibration_z_acc(sensor_info_t *sensor) {
    printf("z_accel: %d", sensor->calibratedLevels[2]);
}

/* These work in my testing with a stable hand >80% of the time,
I improved this from lower than 50% from creating a running average
function that recalibrates the detection mechanism. 

If I had more time, I would try to make sense of all of the accel/gyro
data and make better predictions with tilt, etc. */

/* This checks for a left motion. If it is greater than calibrated, 
this means there is a hand movement to the right.*/
bool sensor_left(short x_accel, sensor_info_t *sensor) {
    if (x_accel < ((sensor->calibratedLevels[0]) - LEFT_MOVE_MULTIPLIER)) {
        // print the calibrated level
        printf("x_accel level: %d\n", x_accel);
        return true;
    }
    return false;
}

/* This checks for a right motion. If it is greater than calibrated, 
this means there is a hand movement to the right.*/
bool sensor_right(short x_accel, sensor_info_t *sensor) {
    if (x_accel > (sensor->calibratedLevels[0] + RIGHT_MOVE_MULTIPLIER)) {
        // print the calibrated level
        printf("x_accel level: %d\n", x_accel);
        return true;
    }
    return false;
}


/* ---- These up/down functions I implemented should definitely work,
but due to the unreliable nature of the sensor, they don't. ---- */


/* This function checks for a down motion. 
If greater than calibrated, it means there is hand 
movement down. */
bool sensor_down(short z_accel, sensor_info_t *sensor) {
    if (z_accel > (sensor->calibratedLevels[2] + DOWN_MOVE_MULTIPLIER)) {
        // print the calibrated level
        printf("z_accel level: %d\n", z_accel);
        return true;
    }
    return false;
}

/* This function checks for an up motion. 
If greater than calibrated, it means there is hand 
movement down. */
bool sensor_up(short z_accel, sensor_info_t *sensor) {
    if (z_accel < (sensor->calibratedLevels[2] - UP_MOVE_MULTIPLIER)) {
        // print the calibrated level
        printf("z_accel level: %d\n", z_accel);
        return true;
    }
    return false;
}
