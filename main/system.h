/*
 system.h - Header for system level commands and real-time processes
 Part of Grbl
 Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC

 */

#ifndef system_h
#define system_h
#include "grbl.h"
#include "grbl.h"

// Define global system variables
typedef struct
{
	uint8_t state;               // Tracks the current system state of Grbl.
	uint8_t abort;               // System abort flag. Forces exit back to main loop for reset.
	uint8_t suspend;             // System suspend bitflag variable that manages holds, cancels, and safety door.
	uint8_t soft_limit;          // Tracks soft limit errors for the state machine. (boolean)
	uint8_t step_control;        // Governs the step segment generator depending on system state.
	uint8_t homing_axis_lock;    // Locks axes when limits engage. Used as an axis motion mask in the stepper ISR.
	uint8_t report_wco_counter;  // Tracks when to add work coordinate offset data to status reports.

} system_t;
extern system_t sys;

// Define system executor bit map. Used internally by realtime protocol as realtime command flags,
// which notifies the main program to execute the specified realtime command asynchronously.
// NOTE: The system executor uses an unsigned 8-bit volatile variable (8 flag limit.) The default
// flags are always false, so the realtime protocol only needs to check for a non-zero value to
// know when there is a realtime command to execute.
#define EXEC_STATUS_REPORT  bit(0) // bitmask 00000001
#define EXEC_CYCLE_START    bit(1) // bitmask 00000010
#define EXEC_CYCLE_STOP     bit(2) // bitmask 00000100
#define EXEC_FEED_HOLD      bit(3) // bitmask 00001000
#define EXEC_RESET          bit(4) // bitmask 00010000
#define EXEC_MOTION_CANCEL  bit(6) // bitmask 01000000
#define EXEC_SLEEP          bit(7) // bitmask 10000000

// Alarm executor codes. Valid values (1-255). Zero is reserved.
#define EXEC_ALARM_HARD_LIMIT           1
#define EXEC_ALARM_SOFT_LIMIT           2
#define EXEC_ALARM_ABORT_CYCLE          3
#define EXEC_ALARM_HOMING_FAIL_RESET    6
#define EXEC_ALARM_HOMING_FAIL_DOOR     7
#define EXEC_ALARM_HOMING_FAIL_PULLOFF  8
#define EXEC_ALARM_HOMING_FAIL_APPROACH 9

// Define system state bit map. The state variable primarily tracks the individual functions
// of Grbl to manage each without overlapping. It is also used as a messaging flag for
// critical events.
#define STATE_IDLE          0      // Must be zero. No flags.
#define STATE_ALARM         bit(0) // In alarm state. Locks out all g-code processes. Allows settings access.
#define STATE_CHECK_MODE    bit(1) // G-code check mode. Locks out planner and motion only.
#define STATE_HOMING        bit(2) // Performing homing cycle
#define STATE_CYCLE         bit(3) // Cycle is running or motions are being executed.
#define STATE_HOLD          bit(4) // Active feed hold
#define STATE_JOG           bit(5) // Jogging mode.
#define STATE_SLEEP         bit(7) // Sleep state.

// Define system suspend flags. Used in various ways to manage suspend states and procedures.
#define SUSPEND_DISABLE           0      // Must be zero.
#define SUSPEND_HOLD_COMPLETE     bit(0) // Indicates initial feed hold is complete.
#define SUSPEND_MOTION_CANCEL     bit(6) // Indicates a canceled resume motion. Currently used by probing routine.
#define SUSPEND_JOG_CANCEL        bit(7) // Indicates a jog cancel in process and to reset buffers when complete.

// Define step segment generator state flags.
#define STEP_CONTROL_NORMAL_OP            0  // Must be zero.
#define STEP_CONTROL_END_MOTION           bit(0)
#define STEP_CONTROL_EXECUTE_HOLD         bit(1)
#define STEP_CONTROL_EXECUTE_SYS_MOTION   bit(2)

// NOTE: These position variables may need to be declared as volatiles, if problems arise.
extern int32_t sys_position[N_AXIS];      // Real-time machine (aka home) position vector in steps.

extern volatile uint8_t sys_rt_exec_state;   // Global realtime executor bitflag variable for state management. See EXEC bitmasks.
extern volatile uint8_t sys_rt_exec_alarm;   // Global realtime executor bitflag variable for setting various alarms.
extern volatile uint8_t sys_rt_exec_motion_override; // Global realtime executor bitflag variable for motion-based overrides.

void system_ini(); // Renamed from system_init() due to conflict with esp32 files

// Special handlers for setting and clearing Grbl's real-time execution flags.
void system_set_exec_state_flag(uint8_t mask);
void system_clear_exec_state_flag(uint8_t mask);
void system_set_exec_alarm(uint8_t code);
void system_clear_exec_alarm();
void system_set_exec_motion_override_flag(uint8_t mask);
void system_clear_exec_motion_overrides();

// Execute the startup script lines stored in EEPROM upon initialization
void system_execute_startup();
uint8_t system_execute_line(char *line, uint8_t client);

void system_flag_wco_change();

// Returns machine position of axis 'idx'. Must be sent a 'step' array.
float system_convert_axis_steps_to_mpos(int32_t *steps, uint8_t idx);

// Updates a machine 'position' array based on the 'step' array sent.
void system_convert_array_steps_to_mpos(float *position, int32_t *steps);

// Checks and reports if target array exceeds machine travel limits.
uint8_t system_check_travel_limits(float *target);
uint8_t get_limit_pin_mask(uint8_t axis_idx);

// Special handlers for setting and clearing Grbl's real-time execution flags.
void system_set_exec_state_flag(uint8_t mask);
void system_clear_exec_state_flag(uint8_t mask);
void system_set_exec_alarm(uint8_t code);
void system_clear_exec_alarm();
void system_set_exec_motion_override_flag(uint8_t mask);
void system_clear_exec_motion_overrides();

#endif
