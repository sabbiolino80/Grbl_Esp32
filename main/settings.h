/*
 settings.h - eeprom configuration handling
 Part of Grbl

 Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
 Copyright (c) 2009-2011 Simen Svale Skogsrud

 */

#ifndef settings_h
#define settings_h

#include "grbl.h"

// Version of the EEPROM data. Will be used to migrate existing data from older versions of Grbl
// when firmware is upgraded. Always stored in byte 0 of eeprom
#define SETTINGS_VERSION 10  // NOTE: Check settings_reset() when moving to next version.

// Define bit flag masks for the boolean settings in settings.flag.
#define BITFLAG_INVERT_ST_ENABLE   bit(2)
#define BITFLAG_HARD_LIMIT_ENABLE  bit(3)
#define BITFLAG_HOMING_ENABLE      bit(4)
#define BITFLAG_SOFT_LIMIT_ENABLE  bit(5)
#define BITFLAG_INVERT_LIMIT_PINS  bit(6)

// Define status reporting boolean enable bit flags in settings.status_report_mask
#define BITFLAG_RT_STATUS_POSITION_TYPE     bit(0)
#define BITFLAG_RT_STATUS_BUFFER_STATE      bit(1)

// Define settings restore bitflags.
#define SETTINGS_RESTORE_DEFAULTS bit(0)
#define SETTINGS_RESTORE_PARAMETERS bit(1)
#define SETTINGS_RESTORE_STARTUP_LINES bit(2)
#define SETTINGS_RESTORE_BUILD_INFO bit(3)
#ifndef SETTINGS_RESTORE_ALL
#define SETTINGS_RESTORE_ALL 0xFF // All bitflags
#endif

// Define EEPROM memory address location values for Grbl settings and parameters
#define EEPROM_SIZE				    1024U
#define EEPROM_ADDR_GLOBAL          1U
#define EEPROM_ADDR_BUILD_INFO      59U
#define EEPROM_ADDR_STARTUP_BLOCK   61U

// Define Grbl axis settings numbering scheme. Starts at START_VAL, every INCREMENT, over N_SETTINGS.
// from $100-101 to $130-131
#define AXIS_N_SETTINGS          4
#define AXIS_SETTINGS_START_VAL  100 // NOTE: Reserving settings values >= 100 for axis settings. Up to 255.
#define AXIS_SETTINGS_INCREMENT  10  // Must be greater than the number of axis settings

// Global persistent settings (Stored from byte EEPROM_ADDR_GLOBAL onwards)
typedef struct
{
	// Axis settings
	float steps_per_mm[N_AXIS];
	float max_rate[N_AXIS];
	float acceleration[N_AXIS];
	float max_travel[N_AXIS];

	// Remaining Grbl settings
	uint8_t pulse_microseconds;     //$0
	uint8_t step_invert_mask;       //$2
	uint8_t dir_invert_mask;        //$3
	uint8_t stepper_idle_lock_time; //$1 // If max value 255, steppers do not disable.
	uint8_t status_report_mask;     //$10 // Mask to indicate desired report data.
	float junction_deviation;       //$11

	uint8_t flags;                  //$4,5,6,20,21,22  // Contains boolean settings

	uint8_t homing_dir_mask;        //$23
	float homing_feed_rate;         //$24
	float homing_seek_rate;         //$25
	uint16_t homing_debounce_delay; //$26
	float homing_pulloff;           //$27
} settings_t;
extern settings_t settings; // SIZE 12*float + 7*char + 1*int16 = 57 byte

// Initialize the configuration subsystem (load settings from EEPROM)
void settings_init();
void settings_restore(uint8_t restore_flag);
void write_global_settings();
uint8_t read_global_settings();

uint8_t settings_read_startup_line(uint8_t n, char *line);
void settings_store_startup_line(uint8_t n, char *line);

uint8_t settings_read_build_info(char *line);
void settings_store_build_info(char *line);

uint8_t settings_store_global_setting(uint8_t parameter, float value);

// Returns the step pin mask according to Grbl's internal axis numbering
uint8_t get_step_pin_mask(uint8_t i);

// Returns the direction pin mask according to Grbl's internal axis numbering
uint8_t get_direction_pin_mask(uint8_t i);

#endif
