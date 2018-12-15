/*
  defaults.h - defaults settings configuration file
  Part of Grbl

  Copyright (c) 2012-2016 Sungeun K. Jeon for Gnea Research LLC

*/

/* The defaults.h file serves as a central default settings selector for different machine
   types, from DIY CNC mills to CNC conversions of off-the-shelf machines. The settings
   files listed here are supplied by users, so your results may vary. However, this should
   give you a good starting point as you get to know your machine and tweak the settings for
   your nefarious needs.
   NOTE: Ensure one and only one of these DEFAULTS_XXX values is defined in config.h */

#ifndef defaults_h


#ifdef DEFAULTS_GENERIC
// Grbl generic default settings. Should work across different machines.
#define DEFAULT_STEP_PULSE_MICROSECONDS 3
#define DEFAULT_STEPPER_IDLE_LOCK_TIME 250 // msec (0-254, 255 keeps steppers enabled)

#define DEFAULT_STEPPING_INVERT_MASK 0 // uint8_t
#define DEFAULT_DIRECTION_INVERT_MASK 2 // uint8_t
#define DEFAULT_INVERT_ST_ENABLE 0 // boolean
#define DEFAULT_INVERT_LIMIT_PINS 1 // boolean

#define DEFAULT_STATUS_REPORT_MASK 2 // MPos enabled

#define DEFAULT_JUNCTION_DEVIATION 0.01 // mm


#define DEFAULT_SOFT_LIMIT_ENABLE 0 // false
#define DEFAULT_HARD_LIMIT_ENABLE 1  // false

#define DEFAULT_HOMING_ENABLE 1  // false
#define DEFAULT_HOMING_DIR_MASK 3 // move negative X,Y
#define DEFAULT_HOMING_FEED_RATE 20.0 // mm/min
#define DEFAULT_HOMING_SEEK_RATE 200.0 // mm/min
#define DEFAULT_HOMING_DEBOUNCE_DELAY 250 // msec (0-65k)
#define DEFAULT_HOMING_PULLOFF 3.0 // mm


#define DEFAULT_X_STEPS_PER_MM 8.0
#define DEFAULT_Y_STEPS_PER_MM 7.555

#define DEFAULT_X_MAX_RATE 15000.0 // mm/min
#define DEFAULT_Y_MAX_RATE 15000.0 // mm/min

#define DEFAULT_X_ACCELERATION (200.0*60*60) // 10*60*60 mm/min^2 = 10 mm/sec^2
#define DEFAULT_Y_ACCELERATION (200.0*60*60) // 10*60*60 mm/min^2 = 10 mm/sec^2

#define DEFAULT_X_MAX_TRAVEL 359.0 // mm NOTE: Must be a positive value.
#define DEFAULT_Y_MAX_TRAVEL 90.0 // mm NOTE: Must be a positive value.


#endif



#endif
