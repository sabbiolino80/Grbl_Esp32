/*
  jog.h - Jogging methods
  Part of Grbl

  Copyright (c) 2016 Sungeun K. Jeon for Gnea Research LLC

*/

#ifndef jog_h
#define jog_h

#include "grbl.h"



// Sets up valid jog motion received from g-code parser, checks for soft-limits, and executes the jog.
uint8_t jog_execute(plan_line_data_t *pl_data, parser_block_t *gc_block);

#endif
