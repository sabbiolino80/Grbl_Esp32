/*
 motion_control.h - high level interface for issuing motion commands
 Part of Grbl

 Copyright (c) 2011-2015 Sungeun K. Jeon
 Copyright (c) 2009-2011 Simen Svale Skogsrud

 */

#ifndef motion_control_h
#define motion_control_h

#include "grbl.h"

#define HOMING_CYCLE_ALL  0  // Must be zero.
#define HOMING_CYCLE_X    bit(X_AXIS)
#define HOMING_CYCLE_Y    bit(Y_AXIS)

// Execute linear motion in absolute millimeter coordinates. Feed rate given in millimeters/second
// unless invert_feed_rate is true. Then the feed_rate means that the motion should be completed in
// (1 minute)/feed_rate time.
void mc_line(float *target, plan_line_data_t *pl_data);

// Dwell for a specific number of seconds
void mc_dwell(float seconds);

// Perform homing cycle to locate machine zero. Requires limit switches.
void mc_homing_cycle(uint8_t cycle_mask);

// Performs system reset. If in motion state, kills all motion and sets system alarm.
void mc_reset();

#endif
