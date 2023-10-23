#include "uart.h"
#include "mymodule.h"
#include "gl.h"
#include "armtimer.h"
#include "interrupts.h"
#include "strings.h"
#include "gpio.h"
#include "keyboard.h"
#include "timer.h"
#include "printf.h"
#include "sensor.h"
#include "i2c.h"

void main(void)
{
    // Base initilizations
    interrupts_init();
    gpio_init(); // for keyboard
    timer_init(); // 
    uart_init();
    keyboard_init(KEYBOARD_CLOCK, KEYBOARD_DATA); // for keyboard (sets up everthing we need for ps2 interrupts)
    i2c_init(); // for sensor
    sensor_dev_init(); // for sensor

    // Enable armtimer interrupts and initialize the timer [changed for sensor polling]
    armtimer_init(50000);
    interrupts_register_handler(INTERRUPTS_BASIC_ARM_TIMER_IRQ, timer_interrupt, NULL); 
    interrupts_enable_source(INTERRUPTS_BASIC_ARM_TIMER_IRQ);

    interrupts_global_enable(); 
    armtimer_enable_interrupts();

    graphics_controls_init(keyboard_read_next);
    start_screen();
    tetris_run();

    //uart_putchar(EOT);
}
