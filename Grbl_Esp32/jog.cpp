/*
  jog.h - Jogging methods
  Part of Grbl

  Copyright (c) 2016 Sungeun K. Jeon for Gnea Research LLC

*/

#include "grbl.h"


// Sets up valid jog motion received from g-code parser, checks for soft-limits, and executes the jog.
uint8_t jog_execute(plan_line_data_t *pl_data, parser_block_t *gc_block)
{
  // Initialize planner data struct for jogging motions.
  // NOTE: Spindle are allowed to fully function with overrides during a jog.
  pl_data->feed_rate = gc_block->values.f;
#ifdef USE_LINE_NUMBERS
  pl_data->line_number = gc_block->values.n;
#endif

  if (bit_istrue(settings.flags, BITFLAG_SOFT_LIMIT_ENABLE)) {
    if (system_check_travel_limits(gc_block->values.xyz)) {
      return (STATUS_TRAVEL_EXCEEDED);
    }
  }

  // Valid jog command. Plan, set state, and execute.
  mc_line(gc_block->values.xyz, pl_data);
  if (sys.state == STATE_IDLE) {
    if (plan_get_current_block() != NULL) { // Check if there is a block to execute.
      sys.state = STATE_JOG;
      st_prep_buffer();
      st_wake_up();  // NOTE: Manual start. No state machine required.
    }
  }

  return (STATUS_OK);
}
