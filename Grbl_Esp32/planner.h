/*
  planner.h - buffers movement commands and manages the acceleration profile plan
  Part of Grbl

  Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
  Copyright (c) 2009-2011 Simen Svale Skogsrud

*/

#ifndef planner_h
#define planner_h



// The number of linear motions that can be in the plan at any give time
#ifndef BLOCK_BUFFER_SIZE
#define BLOCK_BUFFER_SIZE 16
#endif

// Returned status message from planner.
#define PLAN_OK true
#define PLAN_EMPTY_BLOCK false

// Define planner data condition flags. Used to denote running conditions of a block.
#define PL_COND_FLAG_RAPID_MOTION      bit(0)
#define PL_COND_FLAG_SYSTEM_MOTION     bit(1) // Single motion. Circumvents planner state. Used by home/park.
#define PL_COND_FLAG_INVERSE_TIME      bit(3) // Interprets feed rate value as inverse time when set.
#define PL_COND_MOTION_MASK    (PL_COND_FLAG_RAPID_MOTION|PL_COND_FLAG_SYSTEM_MOTION)



// This struct stores a linear movement of a g-code block motion with its critical "nominal" values
// are as specified in the source g-code.
typedef struct {
  // Fields used by the bresenham algorithm for tracing the line
  // NOTE: Used by stepper algorithm to execute the block correctly. Do not alter these values.
  uint32_t steps[N_AXIS];    // Step count along each axis
  uint32_t step_event_count; // The maximum step axis count and number of steps required to complete this block.
  uint8_t direction_bits;    // The direction bit set for this block (refers to *_DIRECTION_BIT in config.h)

  // Block condition data to ensure correct execution depending on states.
  uint8_t condition;      // Block bitflag variable defining block run conditions. Copied from pl_line_data.

  // Fields used by the motion planner to manage acceleration. Some of these values may be updated
  // by the stepper module during execution of special motion cases for replanning purposes.
  float entry_speed_sqr;     // The current planned entry speed at block junction in (mm/min)^2
  float max_entry_speed_sqr; // Maximum allowable entry speed based on the minimum of junction limit and
  //   neighboring nominal speeds with overrides in (mm/min)^2
  float acceleration;        // Axis-limit adjusted line acceleration in (mm/min^2). Does not change.
  float millimeters;         // The remaining distance for this block to be executed in (mm).
  // NOTE: This value may be altered by stepper algorithm during execution.

  // Stored rate limiting data used by planner when changes occur.
  float max_junction_speed_sqr; // Junction entry speed limit based on direction vectors in (mm/min)^2
  float rapid_rate;             // Axis-limit adjusted maximum rate for this block direction in (mm/min)
  float programmed_rate;        // Programmed rate of this block (mm/min).

} plan_block_t;

// Planner data prototype. Must be used when passing new motions to the planner.
typedef struct {
  float feed_rate;          // Desired feed rate for line motion. Value is ignored, if rapid motion.
  uint8_t condition;        // Bitflag variable to indicate planner conditions. See defines above.
} plan_line_data_t;



// Initialize and reset the motion plan subsystem
void plan_reset(); // Reset all
void plan_reset_buffer(); // Reset buffer only.

// Add a new linear movement to the buffer. target[N_AXIS] is the signed, absolute target position
// in millimeters. Feed rate specifies the speed of the motion. If feed rate is inverted, the feed
// rate is taken to mean "frequency" and would complete the operation in 1/feed_rate minutes.
uint8_t plan_buffer_line(float *target, plan_line_data_t *pl_data);

// Called when the current block is no longer needed. Discards the block and makes the memory
// availible for new blocks.
void plan_discard_current_block();

// Gets the planner block for the special system motion cases. (Homing)
plan_block_t *plan_get_system_motion_block();

// Gets the current block. Returns NULL if buffer empty
plan_block_t *plan_get_current_block();

// Called periodically by step segment buffer. Mostly used internally by planner.
uint8_t plan_next_block_index(uint8_t block_index);

// Called by step segment buffer when computing executing block velocity profile.
float plan_get_exec_block_exit_speed_sqr();

// Called by main program during planner calculations and step segment buffer during initialization.
float plan_compute_profile_nominal_speed(plan_block_t *block);

// Re-calculates buffered motions profile parameters upon a motion-based override change.
void plan_update_velocity_profile_parameters();

// Reset the planner position vector (in steps)
void plan_sync_position();

// Reinitialize plan with a partially completed block
void plan_cycle_reinitialize();

// Returns the number of available blocks are in the planner buffer.
uint8_t plan_get_block_buffer_available();

// Returns the number of active blocks are in the planner buffer.
// NOTE: Deprecated. Not used unless classic status reports are enabled in config.h
uint8_t plan_get_block_buffer_count();

// Returns the status of the block ring buffer. True, if buffer is full.
uint8_t plan_check_full_buffer();

void plan_get_planner_mpos(float *target);


#endif
