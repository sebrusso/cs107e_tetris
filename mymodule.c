#include "mymodule.h"
#include "uart.h"
#include "gl.h"
#include "armtimer.h"
#include "timer.h"
#include "interrupts.h"
#include "malloc.h"
#include "strings.h"
#include "fb.h"
#include "printf.h"
#include "sensor.h"
#include <assert.h>
#include "ringbuffer.h"
#include "i2c.h"
#include "gpio.h"
#include "pwm.h"
#include "audio.h"
#include "printf.h"
#include "timer.h"
#include "tetris_audio.h"


struct wav_format {
   uint32_t description; // should be 'RIFF'
   uint32_t filesize; // little endian
   uint32_t wav_header; // should be 'WAVE'
   uint32_t wav_format; // should be 'fmt '
   uint32_t type_format_size; // should be 16, as in the above four fields are 16 bytes 
   uint16_t wav_type; // 0x01 is PCM
   uint16_t num_channels; // 0x01 is mono, 0x02 is stereo, etc.
   uint32_t sample_rate; // e.g., 44100, little-endian
   uint32_t rate_plus; // sample rate * bits per sample * channels / 8
   uint16_t bits_per_sample; // e.g., 16
   uint32_t data_mark; // should be 'data'
   uint32_t data_size; // little endian
   uint8_t *data; // may be 16-bit
};

/* ------ SENSOR CONTROLS  ----*/
/* Levers for fine-tuning sensitivy and performance - EDIT THESE */
#define COOLDOWN_TIME 25
#define ARM_TIMER_START_COUNTER 10

/* ------ GAMEPLAY CONTROLS  ----*/

#define BLOCK_SIZE 50
#define PADDING_Y 20
#define BORDER_THICKNESS 3
#define NUM_ROWS 20
#define NUM_COLS 10
#define SCORE_DIGITS 5

/* ------ SENSOR VARS  ----*/
sensor_info_t *sensor; // sensor input

// For sensor and armtimer settings in sensor version
int ARMCOUNTER;
bool coolDown;
int coolDownTime;

/* ------ GAMEPLAY/GRAPHICAL GLOBAL VARS ----*/

// Definitions for colors 
color_t BORDER_COLOR = GL_BLACK;
color_t BACKGROUND_COLOR = GL_WHITE;

// Definitions for screen size
unsigned int SCREEN_WIDTH;
unsigned int SCREEN_HEIGHT;
unsigned int PADDING_X;

// For the score
unsigned int rowscleared;
unsigned int mostrows;
char score[SCORE_DIGITS + 1];
char highscore[SCORE_DIGITS + 1];
unsigned int SCORE_X; // SCORE_X and SCORE_Y initialized in score_init()
unsigned int SCORE_Y;

// Definition for keyboard inputs 
static input_fn_t controls_read;

/*For the placedblocks array, these indicate the location 
of the currently dropping block. We have an 'x' and a 'y'. */

shape_t currshape; // Shape in play
shape_t nextshape; // Shape on side
char **placedblocks; // 2D array of blocks that have been placed
int startingX; // Starting x position. Middle of the screen minus 1;
int currX; // Current x position of the block
int currY; // Current y position of the block (minus one is top left corner of the block-to-be-placed)
int lastX; // Last x position of the block
unsigned int realY; // used in read_input


/* ------ GAMEPLAY/GRAPHICAL/INPUT FUNCTIONS ----*/


/* Initializes sensor peripheral and attaches information
to the sensor_info_t sensor global declared on line 18.*/
void sensor_dev_init(void) {
    // Sensor initilizations
    printf("whoami=%x\n", lsm6ds33_get_whoami()); // this is also asserted in sensor_new()
    sensor = sensor_new(); // this gives us the pointers we need and initializes the sensor
    sensor_init(sensor);

    /* Calibration of sensor*/
    printf("calibrate the sensor\n"); 
    printf("hold still for 3 seconds\n"); 
    timer_delay_ms(1000); // wait 1 seconds
    sensor_calibrate(sensor);   
    printf("sensor calibrated\n");

    // print calibrated levels for acc
    sensor_print_calibration(sensor);

    ARMCOUNTER = ARM_TIMER_START_COUNTER;
    coolDown = false;
    coolDownTime = COOLDOWN_TIME;
}


/* During swap, we must copy the information from the 
old draw buf (new display buf) into new draw buf (old
display buf), so we can "save our progress" as we draw. */
void screen_copy_buffer(char *display, char *draw) {
    char *from = display;
    char *to = draw;
    int bytes = ((fb_get_pitch() * fb_get_height())) ;

    memcpy(to, from, bytes);

}

/*
plays audio clip from tetris_audio on loop
*/
void audio_play()
{
    printf("%s", "audio_play");
    //gpio_init();
    pwm_init();
    audio_init(sample_freq);
    
    printf("starting play\n");
    if (bits_per_sample == 8) {
        audio_write_u8((uint8_t *)wav_data, sizeof(wav_data), repeat);
    } else {
        audio_write_i16((int16_t *)wav_data, sizeof(wav_data), repeat);
    }
    printf("done playing\n");
    timer_delay_ms(1000);
    
}


/* Refresh function swaps the screen and displays the
updated contents (text) using functionality from gl and
fb libraries. */
void screen_refresh(void) {

    char *display = fb_get_draw_buffer(); // get the address of the new display buf (post-swap)
    gl_swap_buffer(); // now, swap the buffers
    char *draw = fb_get_draw_buffer(); // get the address of the draw buf 
    
    // "update" the new draw buffer
    screen_copy_buffer(display, draw);
}

void check_and_clear_row(unsigned int row) {
    armtimer_disable();
    
    for (int shapeY = 0; shapeY < 4; shapeY++) {
        if ((row + shapeY) >= NUM_ROWS) {
            break;
        }

        unsigned int zeroinrow = 0;

        for (int x = 0; x < NUM_COLS; x++) {
            if (placedblocks[row + shapeY][x] == 0) {
                zeroinrow = 1;
                break;
            }
        }

        if (zeroinrow == 0) {
            timer_delay_ms(200);

            rowscleared++;
            draw_score();

            for (int y = (row + shapeY); y > 0; y--) {
                for (int x = 0; x < NUM_COLS; x++) {
                    placedblocks[y][x] = placedblocks[y - 1][x];
                }
            }

            for (int x = 0; x < NUM_COLS; x++) {
                placedblocks[0][x] = 0;
            }

            gl_draw_rect(PADDING_X, (row + shapeY)*BLOCK_SIZE + PADDING_Y,
                    NUM_COLS*BLOCK_SIZE, BLOCK_SIZE, BACKGROUND_COLOR);

            screen_refresh();

            timer_delay(1);

            gl_draw_rect(PADDING_X, PADDING_Y,
                    NUM_COLS*BLOCK_SIZE, NUM_ROWS*BLOCK_SIZE, BACKGROUND_COLOR);

            for (int x = 0; x < NUM_COLS; x++) {
                for (int y = 0; y < NUM_ROWS; y++) {
                    char blockshape = placedblocks[y][x];
                    if (blockshape > 0) {
                        color_t color = get_color(blockshape - 1);
                        draw_block_once(x, y, color);
                    }
                }
            }

            screen_refresh();

            if (rowscleared == 5) {
                armtimer_init(200000); 
                interrupts_register_handler(INTERRUPTS_BASIC_ARM_TIMER_IRQ, timer_interrupt, NULL); 
                interrupts_enable_source(INTERRUPTS_BASIC_ARM_TIMER_IRQ);
                armtimer_enable_interrupts();
            }
        }
    }

    armtimer_enable();
}

/* Moves the shape down after being called by 
the armtimer. */
void gravity(void) {
    if (valid_shape_position(currX, currY, currshape, placedblocks, NUM_ROWS, NUM_COLS)) {
            if (currY != 0) {
                clear_shape(currX, currY - 1, currshape);
            }
            draw_shape(currX, currY, currshape);
            currY++;
        } else if (currX == startingX && currY == 0) { // Game over!
            loss_screen();
        } else {
            // We clear the last block and place the block in the placedblocks array
            place_shape(currX, currY - 1, currshape, placedblocks, NUM_ROWS, NUM_COLS);
            check_and_clear_row(currY - 1);

            // reset the current position to the top of the game area
            currY = 0;
            currX = lastX = startingX;
            currshape = nextshape;

            get_and_update_next_shape();
        }
}

/* Sensor handler that is triggered consistently
by armtimer to poll the sensor for acc data. Calls sensor 
left and right appropriately. */
void sensor_poll(void) {
    if (!coolDown) { // if not in cooldown, read the sensor
        
        sensor_read(sensor); // read the sensor
        short x_accel = sensor_get_xAccel_Avg(sensor); // get the average of the x_accel rb
        short z_accel = sensor_get_zAccel_Avg(sensor); // get the average of the z_accel rb
        
        // update realY
        if (currY == 0) {
            realY = currY;
        } else {
            realY = currY - 1;
        } 

        if (sensor_left(x_accel, sensor)) { 
            printf("left\n");
            left_input();
            coolDown = true;
        } 
        else if (sensor_right(x_accel, sensor)) {
            printf("right\n");
            right_input();
            coolDown = true;
        }

        // these functions do not work properly with sensor accelerometer output
        // else if (sensor_up(z_accel, sensor)) {
        //     printf("up\n");
        //     rotate_input();
        //     coolDown = true;
        // }
        // else if (sensor_down(z_accel, sensor)) {
        //     printf("down\n");
        //     down_input();
        //     coolDown = true;
        // }
    }
    else {  // if in cooldown, decrement the cooldown time
        coolDownTime--;
        if (coolDownTime == 0) { // if cooldown stime is 0, reset cooldown
            coolDown = false;
            coolDownTime = COOLDOWN_TIME;
        }
    }

    sensor_recalibrate(sensor); // essential to recalibrate every time
    sensor_print_calibration_z_acc(sensor);
}

/* Interrupt that is triggered by the armtimer.
   Moves the blocks down. */
void timer_interrupt(unsigned int pc, void *aux_data) { 
    if (armtimer_check_and_clear_interrupt()) {
        if (ARMCOUNTER == 0) {
            gravity();
            ARMCOUNTER = ARM_TIMER_START_COUNTER;
        } else {
            ARMCOUNTER--;
            sensor_poll();
        }
    }
}

void draw_square_with_bound(int x, int y, int blocksize, color_t color) {
        gl_draw_rect(x, y, // Fill out square
                blocksize, blocksize, color);
        gl_draw_rect(x, y, // Left
                2, blocksize, GL_BLACK);
        gl_draw_rect(x + blocksize - 2, y, // Right
                2, blocksize, GL_BLACK);
        gl_draw_rect(x, y, // Top
                blocksize, 2, GL_BLACK);
        gl_draw_rect(x, y + blocksize - 2, // Bottom
                blocksize, 2, GL_BLACK);
}

void graphics_controls_init(input_fn_t read_fn) {
    PADDING_X = 4*BLOCK_SIZE + 100;
    SCREEN_WIDTH = NUM_COLS*BLOCK_SIZE + 2*PADDING_X;
    SCREEN_HEIGHT = NUM_ROWS*BLOCK_SIZE + 2*PADDING_Y;

    gl_init(SCREEN_WIDTH, SCREEN_HEIGHT, GL_DOUBLEBUFFER);
    controls_read = read_fn;

}

void write_title(void) {
    unsigned int titleblocksize = 50;
    unsigned int gapX = 20;
    unsigned int gapY = 300;
    unsigned int offsetX = (SCREEN_WIDTH - gapX*7 - titleblocksize*18)/2;
    
    // 'T'
    draw_square_with_bound(offsetX + gapX*1 + titleblocksize*0, gapY*1, titleblocksize, GL_RED);
    draw_square_with_bound(offsetX + gapX*1 + titleblocksize*1, gapY*1, titleblocksize, GL_RED);
    draw_square_with_bound(offsetX + gapX*1 + titleblocksize*2, gapY*1, titleblocksize, GL_RED);

    draw_square_with_bound(offsetX + gapX*1 + titleblocksize*1, gapY*1 + titleblocksize*1, titleblocksize, GL_RED);
    draw_square_with_bound(offsetX + gapX*1 + titleblocksize*1, gapY*1 + titleblocksize*2, titleblocksize, GL_RED);
    draw_square_with_bound(offsetX + gapX*1 + titleblocksize*1, gapY*1 + titleblocksize*3, titleblocksize, GL_RED);
    draw_square_with_bound(offsetX + gapX*1 + titleblocksize*1, gapY*1 + titleblocksize*4, titleblocksize, GL_RED);
    
    // 'E'
    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*3, gapY*1, titleblocksize, GL_ORANGE);
    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*4, gapY*1, titleblocksize, GL_ORANGE);
    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*5, gapY*1, titleblocksize, GL_ORANGE);
    
    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*3, gapY*1 + titleblocksize*1, titleblocksize, GL_ORANGE);

    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*3, gapY*1 + titleblocksize*2, titleblocksize, GL_ORANGE);
    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*4, gapY*1 + titleblocksize*2, titleblocksize, GL_ORANGE);
    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*5, gapY*1 + titleblocksize*2, titleblocksize, GL_ORANGE);

    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*3, gapY*1 + titleblocksize*3, titleblocksize, GL_ORANGE);

    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*3, gapY*1 + titleblocksize*4, titleblocksize, GL_ORANGE);
    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*4, gapY*1 + titleblocksize*4, titleblocksize, GL_ORANGE);
    draw_square_with_bound(offsetX + gapX*2 + titleblocksize*5, gapY*1 + titleblocksize*4, titleblocksize, GL_ORANGE);
    
    // 'T'
    draw_square_with_bound(offsetX + gapX*3 + titleblocksize*6, gapY*1, titleblocksize, GL_YELLOW);
    draw_square_with_bound(offsetX + gapX*3 + titleblocksize*7, gapY*1, titleblocksize, GL_YELLOW);
    draw_square_with_bound(offsetX + gapX*3 + titleblocksize*8, gapY*1, titleblocksize, GL_YELLOW);

    draw_square_with_bound(offsetX + gapX*3 + titleblocksize*7, gapY*1 + titleblocksize*1, titleblocksize, GL_YELLOW);
    draw_square_with_bound(offsetX + gapX*3 + titleblocksize*7, gapY*1 + titleblocksize*2, titleblocksize, GL_YELLOW);
    draw_square_with_bound(offsetX + gapX*3 + titleblocksize*7, gapY*1 + titleblocksize*3, titleblocksize, GL_YELLOW);
    draw_square_with_bound(offsetX + gapX*3 + titleblocksize*7, gapY*1 + titleblocksize*4, titleblocksize, GL_YELLOW);
    
    // 'R'
    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*9, gapY*1, titleblocksize, GL_GREEN);
    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*10, gapY*1, titleblocksize, GL_GREEN);

    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*9, gapY*1 + titleblocksize*1, titleblocksize, GL_GREEN);
    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*11, gapY*1 + titleblocksize*1, titleblocksize, GL_GREEN);
    
    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*9, gapY*1 + titleblocksize*2, titleblocksize, GL_GREEN);
    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*10, gapY*1 + titleblocksize*2, titleblocksize, GL_GREEN);

    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*9, gapY*1 + titleblocksize*3, titleblocksize, GL_GREEN);
    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*11, gapY*1 + titleblocksize*3, titleblocksize, GL_GREEN);
    
    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*9, gapY*1 + titleblocksize*4, titleblocksize, GL_GREEN);
    draw_square_with_bound(offsetX + gapX*4 + titleblocksize*11, gapY*1 + titleblocksize*4, titleblocksize, GL_GREEN);

    // 'I'
    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*12, gapY*1, titleblocksize, GL_CYAN);
    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*13, gapY*1, titleblocksize, GL_CYAN);
    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*14, gapY*1, titleblocksize, GL_CYAN);

    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*13, gapY*1 + titleblocksize*1, titleblocksize, GL_CYAN);
    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*13, gapY*1 + titleblocksize*2, titleblocksize, GL_CYAN);
    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*13, gapY*1 + titleblocksize*3, titleblocksize, GL_CYAN);
    
    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*12, gapY*1 + titleblocksize*4, titleblocksize, GL_CYAN);
    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*13, gapY*1 + titleblocksize*4, titleblocksize, GL_CYAN);
    draw_square_with_bound(offsetX + gapX*5 + titleblocksize*14, gapY*1 + titleblocksize*4, titleblocksize, GL_CYAN);
    
    // 'S'
    draw_square_with_bound(offsetX + gapX*6 + titleblocksize*16, gapY*1 + titleblocksize*0, titleblocksize, GL_PURPLE);
    draw_square_with_bound(offsetX + gapX*6 + titleblocksize*17, gapY*1 + titleblocksize*0, titleblocksize, GL_PURPLE);

    draw_square_with_bound(offsetX + gapX*6 + titleblocksize*15, gapY*1 + titleblocksize*1, titleblocksize, GL_PURPLE);

    draw_square_with_bound(offsetX + gapX*6 + titleblocksize*16, gapY*1 + titleblocksize*2, titleblocksize, GL_PURPLE);

    draw_square_with_bound(offsetX + gapX*6 + titleblocksize*17, gapY*1 + titleblocksize*3, titleblocksize, GL_PURPLE);

    draw_square_with_bound(offsetX + gapX*6 + titleblocksize*15, gapY*1 + titleblocksize*4, titleblocksize, GL_PURPLE);
    draw_square_with_bound(offsetX + gapX*6 + titleblocksize*16, gapY*1 + titleblocksize*4, titleblocksize, GL_PURPLE);
    
    const char *text = "Created By:";
    gl_draw_string(center_text(text), 5*0 + gapY*1 + titleblocksize*6, text, GL_BLACK);
    
    const char *text2 = "Alexey Pajitnov";
    gl_draw_string(center_text(text2), 5*1 + gapY*1 + titleblocksize*6 + gl_get_char_height(), text2, GL_BLACK);

    const char *text3 = "Adapted For CS107E By:";
    gl_draw_string(center_text(text3), 5*2 + gapY*1 + titleblocksize*7 + gl_get_char_height()*2, text3, GL_BLACK);

    const char *text4 = "Nick Reisner, Sebastian Russo, and Devon Smith";
    gl_draw_string(center_text(text4), 5*3 + gapY*1 + titleblocksize*7 + gl_get_char_height()*3, text4, GL_BLACK);

    // const char *text5 = "Press Any Key To Play!";
    const char *text5 = "Wait 5 Seconds To Play!";
    gl_draw_string(center_text(text5), 5*4 + gapY*1 + titleblocksize*8 + gl_get_char_height()*4, text5, GL_BLACK);
}


void start_screen(void)
{
    gl_clear(BACKGROUND_COLOR);
    write_title();
    gl_swap_buffer();
    audio_play();
    // controls_read(); 
    timer_delay(5);

    
    tetris_init();
}

void loss_screen(void)
{
    armtimer_disable();

    if (rowscleared > mostrows) {
        mostrows = rowscleared;
    }

    unsigned int blockpadding = 1;
    gl_draw_rect(PADDING_X + BLOCK_SIZE*blockpadding,  
            PADDING_Y + BLOCK_SIZE*blockpadding,
            BLOCK_SIZE*(NUM_COLS - blockpadding*2),
            BLOCK_SIZE*(NUM_ROWS - blockpadding*2), 
            BACKGROUND_COLOR);

    const char *text = "GAME OVER!";
    gl_draw_string(center_text(text), PADDING_Y + BLOCK_SIZE*blockpadding + 5, text, GL_BLACK);

    // const char *text2 = "Press any key to play again.";
    const char *text2 = "Wait ten seconds to play again.";
    gl_draw_string(center_text(text2), PADDING_Y + BLOCK_SIZE*blockpadding + gl_get_char_height() + 8, text2, GL_BLACK);

    gl_draw_rect(PADDING_X - BORDER_THICKNESS + BLOCK_SIZE*blockpadding, // Left 
            PADDING_Y - BORDER_THICKNESS + BLOCK_SIZE*blockpadding,
            BORDER_THICKNESS,
            BLOCK_SIZE*(NUM_ROWS - blockpadding*2) + BORDER_THICKNESS,
            BORDER_COLOR);
    gl_draw_rect(SCREEN_WIDTH - PADDING_X - BLOCK_SIZE*blockpadding, // Right
            PADDING_Y - BORDER_THICKNESS + BLOCK_SIZE*blockpadding,
            BORDER_THICKNESS,
            BLOCK_SIZE*(NUM_ROWS - blockpadding*2) + BORDER_THICKNESS,
            BORDER_COLOR);
    gl_draw_rect(PADDING_X - BORDER_THICKNESS + BLOCK_SIZE*blockpadding, // Top
            PADDING_Y - BORDER_THICKNESS + BLOCK_SIZE*blockpadding,
            BLOCK_SIZE*(NUM_COLS - blockpadding*2) + BORDER_THICKNESS,
            BORDER_THICKNESS,
            BORDER_COLOR);
    gl_draw_rect(PADDING_X - BORDER_THICKNESS + BLOCK_SIZE*blockpadding, // Bottom
            SCREEN_HEIGHT - PADDING_Y - BLOCK_SIZE*blockpadding,
            BLOCK_SIZE*(NUM_COLS - blockpadding*2) + BORDER_THICKNESS,
            BORDER_THICKNESS,
            BORDER_COLOR);

    gl_swap_buffer();

    // controls_read();
    timer_delay(10);

    start_screen();
}


// Initializes the background and draws the background color and game border 
void background_init(void)
{
    gl_clear(BACKGROUND_COLOR);
    gl_draw_rect(PADDING_X - BORDER_THICKNESS, // Left 
            PADDING_Y - BORDER_THICKNESS,
            BORDER_THICKNESS,
            SCREEN_HEIGHT - PADDING_Y*2 + BORDER_THICKNESS, 
            BORDER_COLOR);
    gl_draw_rect(SCREEN_WIDTH - PADDING_X, // Right
            PADDING_Y - BORDER_THICKNESS,
            BORDER_THICKNESS,
            SCREEN_HEIGHT - PADDING_Y*2 + BORDER_THICKNESS, 
            BORDER_COLOR);
    gl_draw_rect(PADDING_X - BORDER_THICKNESS, // Top
            PADDING_Y - BORDER_THICKNESS,
            SCREEN_WIDTH - PADDING_X*2 + BORDER_THICKNESS*2, 
            BORDER_THICKNESS,
            BORDER_COLOR);
    gl_draw_rect(PADDING_X - BORDER_THICKNESS, // Bottom
            SCREEN_HEIGHT - PADDING_Y,
            SCREEN_WIDTH - PADDING_X*2 + BORDER_THICKNESS*2, 
            BORDER_THICKNESS,
            BORDER_COLOR);

    score_init();
    next_block_init();
    screen_refresh();
}

void score_init(void) {
    SCORE_X = PADDING_X + NUM_COLS*BLOCK_SIZE + 55;
    SCORE_Y = PADDING_Y + gl_get_char_height() + 3;
    gl_draw_string(SCORE_X, SCORE_Y - gl_get_char_height() - 3, "SCORE", GL_BLACK);
    gl_draw_string(SCORE_X, SCORE_Y + gl_get_char_height() + 10, "HIGH SCORE", GL_BLACK);

    rowscleared = 0;
    score[SCORE_DIGITS] = '\0';
    snprintf(score, SCORE_DIGITS + 1, "%05d", rowscleared);

    highscore[SCORE_DIGITS] = '\0';
    snprintf(highscore, SCORE_DIGITS + 1, "%05d", mostrows);

    gl_draw_string(SCORE_X, SCORE_Y, score, GL_BLACK);
    gl_draw_string(SCORE_X, SCORE_Y + gl_get_char_height()*2 + 13, highscore, GL_BLACK);
}

void next_block_init(void) {
    nextshape = random_start_shape();
    draw_square_with_bound(SCORE_X, SCREEN_HEIGHT/2 - BLOCK_SIZE*2, BLOCK_SIZE*4 + 10, BACKGROUND_COLOR);
    draw_shape_raw(SCORE_X + 5, SCREEN_HEIGHT/2 - BLOCK_SIZE*1 + 10, nextshape, BLOCK_SIZE);
    gl_draw_string(SCORE_X, SCREEN_HEIGHT/2 - BLOCK_SIZE*2 - gl_get_char_height() - 5, "NEXT BLOCK", GL_BLACK);
}

void get_and_update_next_shape(void) {
    nextshape = random_start_shape();

    gl_draw_rect(SCORE_X + 5, SCREEN_HEIGHT/2 - BLOCK_SIZE*2 + 5,
                BLOCK_SIZE*4, BLOCK_SIZE*4, BACKGROUND_COLOR);
    draw_shape_raw(SCORE_X + 5, SCREEN_HEIGHT/2 - BLOCK_SIZE*1 + 10, nextshape, BLOCK_SIZE);

    gl_swap_buffer();

    gl_draw_rect(SCORE_X + 5, SCREEN_HEIGHT/2 - BLOCK_SIZE*2 + 5,
                BLOCK_SIZE*4, BLOCK_SIZE*4, BACKGROUND_COLOR);
    draw_shape_raw(SCORE_X + 5, SCREEN_HEIGHT/2 - BLOCK_SIZE*1 + 10, nextshape, BLOCK_SIZE);
}


/* Initializes the 2D-array that tracks the location of placed bricks */
void placedblocks_init(void) {

    placedblocks = malloc(sizeof(char*)*NUM_ROWS); // Allocate memory for the columns

    for (int y = 0; y < NUM_ROWS; y++) { // Allocate memory for the rows
        placedblocks[y] = malloc(sizeof(char)*NUM_COLS);
    }

    for (int y = 0; y < NUM_ROWS; y++) { // Initialize the array to 0
        for (int x = 0; x < NUM_COLS; x++) {
            placedblocks[y][x] = 0;
        }
    }
}

/* Initializes tetris graphics and mechanics
   objects -- background and placedblocks array. */
void tetris_init(void) {
    currshape = random_start_shape();
    startingX = (NUM_COLS / 2) - 2;
    currX = lastX = startingX;
    currY = 0;

    background_init();
    placedblocks_init();
    armtimer_init(50000); // one mag less for sensor implementation

    interrupts_register_handler(INTERRUPTS_BASIC_ARM_TIMER_IRQ, timer_interrupt, NULL); 
    interrupts_enable_source(INTERRUPTS_BASIC_ARM_TIMER_IRQ);
    armtimer_enable_interrupts();
    armtimer_enable();
}

/* Draws a block at a particular (x,y). This (x,y) corresponds to
   the game grid, not pixels. */
void draw_block_once(unsigned int x, unsigned int y, color_t color) {
    if (x >= 0 && x < NUM_COLS && y >= 0 && y < NUM_ROWS) {
        gl_draw_rect(x*BLOCK_SIZE + PADDING_X, y*BLOCK_SIZE + PADDING_Y, // Fill out square
                BLOCK_SIZE, BLOCK_SIZE, color);
        gl_draw_rect(x*BLOCK_SIZE + PADDING_X, y*BLOCK_SIZE + PADDING_Y, // Left
                1, BLOCK_SIZE, GL_BLACK);
        gl_draw_rect(x*BLOCK_SIZE + BLOCK_SIZE - 1 + PADDING_X, y*BLOCK_SIZE + PADDING_Y, // Right
                1, BLOCK_SIZE, GL_BLACK);
        gl_draw_rect(x*BLOCK_SIZE + PADDING_X, y*BLOCK_SIZE + PADDING_Y, // Top
                BLOCK_SIZE, 1, GL_BLACK);
        gl_draw_rect(x*BLOCK_SIZE + PADDING_X, y*BLOCK_SIZE + BLOCK_SIZE - 1 + PADDING_Y, // Bottom
                BLOCK_SIZE, 1, GL_BLACK);
    }
}

/* Draws block as desribed in above fucntion for both bufs. */
void draw_block(unsigned int x, unsigned int y, color_t color) {
    draw_block_once(x, y, color); // draw in draw buf
    gl_swap_buffer(); // swap buf and draw in draw buf
    draw_block_once(x, y, color); // draw in new buf
}

/* Clears a block at a particular (x,y). This (x,y) corresponds to the 
   game grid, not pixels. Updates a single buffer. */
void clear_block_once(unsigned int x, unsigned int y) {
    if (x >= 0 && x < NUM_COLS && y >= 0 && y < NUM_ROWS) {
        gl_draw_rect(x*BLOCK_SIZE + PADDING_X, y*BLOCK_SIZE + PADDING_Y,
                BLOCK_SIZE, BLOCK_SIZE, BACKGROUND_COLOR);
    }
}

/* Clears both as described in above function for both bufs. */
void clear_block(unsigned int x, unsigned int y) {
    clear_block_once(x, y); // clear the block in the draw buffer
    gl_swap_buffer(); // swap the buffers
    clear_block_once(x, y); // clear the block in the new buffer
}


/* Input - 'a' / left-movement */
void left_input(void) {
    if (valid_shape_position(currX - 1, realY, currshape, placedblocks, NUM_ROWS, NUM_COLS)) {
            clear_shape(currX, realY, currshape);
            currX--;
            if (realY == (currY - 1)) draw_shape(currX, realY, currshape);
        }
}

/* Input - 'd' / right movement */
void right_input(void) {
    if (valid_shape_position(currX + 1, realY, currshape, placedblocks, NUM_ROWS, NUM_COLS)) {
            clear_shape(currX, realY, currshape);
            currX++;
            if (realY == (currY - 1)) draw_shape(currX, realY, currshape);
        }
}

/* Input - 's' / down movement */
void down_input(void) {
        if (valid_shape_position(currX, currY, currshape, placedblocks, NUM_ROWS, NUM_COLS)) {
            clear_shape(currX, realY, currshape);
            draw_shape(currX, currY, currshape);
            currY++;
        }
}

/* Input - 'w' / up movement */
void rotate_input(void) {
    shape_t potentialshape = get_next_orientation(currshape);
    if (valid_shape_position(currX, realY, potentialshape, placedblocks, NUM_ROWS, NUM_COLS)) {
        clear_shape(currX, realY, currshape);
        if (realY == (currY - 1)) draw_shape(currX, realY, potentialshape);
        currshape = potentialshape;
    }
}

/* Reads the input for the game! */
void read_input(void) {
    unsigned char next = controls_read();

    // Note: currY global is always one ahead of the drawn position of the block
    if (currY == 0) {
        realY = currY;
    } else {
        realY = currY - 1;
    } 
   
    if (next == 'a') {
        left_input();
    } 
    else if (next == 'd') {
        right_input();
    } 
    else if (next == 's') { // Case 2: 's' moves the block down until the bottom or the next lowest block [not graphical]
        down_input();
    }
    else if (next == 'w') { // Case 3: 'w' rotates the block
        rotate_input();
    }
}

/* Main game loop */
void tetris_run(void) {
    while (1) {
        read_input();
    }
}

void draw_score(void) {
    snprintf(score, SCORE_DIGITS + 1, "%05d", rowscleared);

    gl_draw_rect(SCORE_X, SCORE_Y, gl_get_char_width()*5, gl_get_char_height(), BACKGROUND_COLOR);
    gl_draw_string(SCORE_X, SCORE_Y, score, GL_BLACK);

    gl_swap_buffer();

    gl_draw_rect(SCORE_X, SCORE_Y, gl_get_char_width()*5, gl_get_char_height(), BACKGROUND_COLOR);
    gl_draw_string(SCORE_X, SCORE_Y, score, GL_BLACK);
}

int lowest_spot(void) {
    int tempY = currY - 1;
    while (valid_shape_position(currX, tempY, currshape, placedblocks, NUM_ROWS, NUM_COLS)) {
        tempY++;
    }

    return (tempY - 1);
}

unsigned int center_text(const char *text) {
    unsigned int characters = strlen(text);
    return (SCREEN_WIDTH / 2) - ((characters*gl_get_char_width()) / 2);
}