/*
    cpu_map.h - Header for system level commands and real-time processes
    Part of Grbl
    Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC

*/

#ifndef cpu_map_h
#define cpu_map_h

/*
    Not all pins can can work for all functions.
    Check features like pull-ups, pwm, etc before
    re-assigning numbers

    (gpio34-39) are inputs only and don't have software pullup/down functions
    You MUST use external pullups or noise WILL cause problems.

    Unlike the AVR version certain pins are not forced into the same port.
    Therefore, bit masks are not use the same way and typically should not be
    changed. They are just preserved right now to make it easy to stay in sync
    with AVR grbl

*/

// This is the CPU Map for the ESP32 CNC Controller R2


#define LED     GPIO_NUM_2
#define EV_H20  GPIO_NUM_5

// It is OK to comment out any step and direction pins. This
// won't affect operation except that there will be no output
// form the pins. Grbl will virtually move the axis. This could
// be handy if you are using a servo, etc. for another axis.
#define X_STEP_PIN      GPIO_NUM_23
#define Y_STEP_PIN      GPIO_NUM_19

#define X_DIRECTION_PIN   GPIO_NUM_21
#define Y_DIRECTION_PIN   GPIO_NUM_18

// OK to comment out to use pin for other features
#define STEPPERS_DISABLE_PIN GPIO_NUM_22

#define X_LIMIT_PIN        GPIO_NUM_25
#define Y_LIMIT_PIN       GPIO_NUM_26



// These are some ESP32 CPU Settings that the program needs, but are generally not changed
#define F_TIMERS  80000000    // a reference to the speed of ESP32 timers
#define F_STEPPER_TIMER 20000000  // frequency of step pulse timer
#define STEPPER_OFF_TIMER_PRESCALE 8 // gives a frequency of 10MHz
#define STEPPER_OFF_PERIOD_uSEC  3  // each tick is

#define STEP_PULSE_MIN 2   // uSeconds
#define STEP_PULSE_MAX 10  // uSeconds

// =============== Don't change or comment these out ======================
// They are for legacy purposes and will not affect your I/O

#define X_STEP_BIT    0  // don't change
#define Y_STEP_BIT    1  // don't change
#define STEP_MASK       B111 // don't change

#define X_DIRECTION_BIT   0 // don't change
#define Y_DIRECTION_BIT   1  // don't change

#define X_LIMIT_BIT       0  // don't change
#define Y_LIMIT_BIT       1  // don't change
#define LIMIT_MASK        B111  // don't change


// =======================================================================

#endif
