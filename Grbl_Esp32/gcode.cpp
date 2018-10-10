/*
  gcode.c - rs274/ngc parser.
  Part of Grbl

  Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
  Copyright (c) 2009-2011 Simen Svale Skogsrud

*/

#include "grbl.h"



#define AXIS_COMMAND_NONE 0
#define AXIS_COMMAND_NON_MODAL 1
#define AXIS_COMMAND_MOTION_MODE 2

// Declare gc extern struct
parser_state_t gc_state;
parser_block_t gc_block;

#define FAIL(status) return(status);


void gc_init()
{
  memset(&gc_state, 0, sizeof(parser_state_t));

  // Load default G54 coordinate system.
  if (!(settings_read_coord_data(gc_state.modal.coord_select, gc_state.coord_system))) {
    report_status_message(STATUS_SETTING_READ_FAIL, CLIENT_SERIAL);
  }
}


// Sets g-code parser position in mm. Input in steps. Called by the system abort and hard
// limit pull-off routines.
void gc_sync_position()
{
  system_convert_array_steps_to_mpos(gc_state.position, sys_position);
}


// Executes one line of 0-terminated G-Code. The line is assumed to contain only uppercase
// characters and signed floating point values (no whitespace). Comments and block delete
// characters have been removed. In this function, all units and positions are converted and
// exported to grbl's internal functions in terms of (mm, mm/min) and absolute machine
// coordinates, respectively.
uint8_t gc_execute_line(char *line, uint8_t client)
{
  /* -------------------------------------------------------------------------------------
     STEP 1: Initialize parser block struct and copy current g-code state modes. The parser
     updates these modes and commands as the block line is parser and will only be used and
     executed after successful error-checking. The parser block struct also contains a block
     values struct, word tracking variables, and a non-modal commands tracker for the new
     block. This struct contains all of the necessary information to execute the block. */

  memset(&gc_block, 0, sizeof(parser_block_t)); // Initialize the parser block struct.
  memcpy(&gc_block.modal, &gc_state.modal, sizeof(gc_modal_t)); // Copy current modes

  uint8_t axis_command = AXIS_COMMAND_NONE;
  uint8_t axis_0, axis_1;
  uint8_t coord_select = 0; // Tracks G10 P coordinate selection for execution

  // Initialize bitflag tracking variables for axis indices compatible operations.
  uint8_t axis_words = 0; // XYZ tracking
  uint8_t ijk_words = 0; // IJK tracking

  // Initialize command and value words and parser flags variables.
  uint16_t command_words = 0; // Tracks G and M command words. Also used for modal group violations.
  uint16_t value_words = 0; // Tracks value words.
  uint8_t gc_parser_flags = GC_PARSER_NONE;

  // Determine if the line is a jogging motion or a normal g-code block.
  if (line[0] == '$') { // NOTE: `$J=` already parsed when passed to this function.
    // Set G1 and G94 enforced modes to ensure accurate error checks.
    gc_parser_flags |= GC_PARSER_JOG_MOTION;
    gc_block.modal.motion = MOTION_MODE_LINEAR;
    gc_block.modal.feed_rate = FEED_RATE_MODE_UNITS_PER_MIN;
  }

  /* -------------------------------------------------------------------------------------
     STEP 2: Import all g-code words in the block line. A g-code word is a letter followed by
     a number, which can either be a 'G'/'M' command or sets/assigns a command value. Also,
     perform initial error-checks for command word modal group violations, for any repeated
     words, and for negative values set for the value words F, N, P, T, and S. */

  uint8_t word_bit; // Bit-value for assigning tracking variables
  uint8_t char_counter;
  char letter;
  float value;
  uint8_t int_value = 0;
  uint16_t mantissa = 0;
  if (gc_parser_flags & GC_PARSER_JOG_MOTION) {
    char_counter = 3;  // Start parsing after `$J=`
  }
  else {
    char_counter = 0;
  }

  while (line[char_counter] != 0) { // Loop until no more g-code words in line.

    // Import the next g-code word, expecting a letter followed by a value. Otherwise, error out.
    letter = line[char_counter];
    if ((letter < 'A') || (letter > 'Z')) {
      FAIL(STATUS_EXPECTED_COMMAND_LETTER);  // [Expected word letter]
    }
    char_counter++;
    if (!read_float(line, &char_counter, &value)) {
      FAIL(STATUS_BAD_NUMBER_FORMAT);  // [Expected word value]
    }

    // Convert values to smaller uint8 significand and mantissa values for parsing this word.
    // NOTE: Mantissa is multiplied by 100 to catch non-integer command values. This is more
    // accurate than the NIST gcode requirement of x10 when used for commands, but not quite
    // accurate enough for value words that require integers to within 0.0001. This should be
    // a good enough comprimise and catch most all non-integer errors. To make it compliant,
    // we would simply need to change the mantissa to int16, but this add compiled flash space.
    // Maybe update this later.
    int_value = trunc(value);
    mantissa =  round(100 * (value - int_value)); // Compute mantissa for Gxx.x commands.
    // NOTE: Rounding must be used to catch small floating point errors.

    // Check if the g-code word is supported or errors due to modal group violations or has
    // been repeated in the g-code block. If ok, update the command or record its value.
    switch (letter) {

      /* 'G' and 'M' Command Words: Parse commands and check for modal group violations.
         NOTE: Modal group numbers are defined in Table 4 of NIST RS274-NGC v3, pg.20 */

      case 'G':
        // Determine 'G' command and its modal group
        switch (int_value) {
          case 10: case 28: case 30: case 92:
            // Check for G10/28/30/92 being called with G0/1/2/3/38 on same block.
            // * G43.1 is also an axis command but is not explicitly defined this way.
            if (mantissa == 0) { // Ignore G28.1, G30.1, and G92.1
              if (axis_command) {
                FAIL(STATUS_GCODE_AXIS_COMMAND_CONFLICT);  // [Axis word/command conflict]
              }
              axis_command = AXIS_COMMAND_NON_MODAL;
            }
          // No break. Continues to next line.
          case 4: case 53:
            word_bit = MODAL_GROUP_G0;
            gc_block.non_modal_command = int_value;
            if ((int_value == 28) || (int_value == 30) || (int_value == 92)) {
              if (!((mantissa == 0) || (mantissa == 10))) {
                FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND);
              }
              gc_block.non_modal_command += mantissa;
              mantissa = 0; // Set to zero to indicate valid non-integer G command.
            }
            break;
          case 0: case 1: case 2: case 3: case 38:
            // Check for G0/1/2/3/38 being called with G10/28/30/92 on same block.
            // * G43.1 is also an axis command but is not explicitly defined this way.
            if (axis_command) {
              FAIL(STATUS_GCODE_AXIS_COMMAND_CONFLICT);  // [Axis word/command conflict]
            }
            axis_command = AXIS_COMMAND_MOTION_MODE;
          // No break. Continues to next line.
          case 80:
            word_bit = MODAL_GROUP_G1;
            gc_block.modal.motion = int_value;
            if (int_value == 38) {
              if (!((mantissa == 20) || (mantissa == 30) || (mantissa == 40) || (mantissa == 50))) {
                FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND); // [Unsupported G38.x command]
              }
              gc_block.modal.motion += (mantissa / 10) + 100;
              mantissa = 0; // Set to zero to indicate valid non-integer G command.
            }
            break;
          case 17: case 18: case 19:
            word_bit = MODAL_GROUP_G2;
            gc_block.modal.plane_select = int_value - 17;
            break;
          case 90: case 91:
            if (mantissa == 0) {
              word_bit = MODAL_GROUP_G3;
              gc_block.modal.distance = int_value - 90;
            } 
//            else {
//              word_bit = MODAL_GROUP_G4;
//              if ((mantissa != 10) || (int_value == 90)) {
//                FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND);  // [G90.1 not supported]
//              }
//              mantissa = 0; // Set to zero to indicate valid non-integer G command.
//              // Otherwise, arc IJK incremental mode is default. G91.1 does nothing.
//            }
            break;
          case 93: case 94:
            word_bit = MODAL_GROUP_G5;
            gc_block.modal.feed_rate = 94 - int_value;
            break;
          case 40:
            word_bit = MODAL_GROUP_G7;
            // NOTE: Not required since cutter radius compensation is always disabled. Only here
            // to support G40 commands that often appear in g-code program headers to setup defaults.
            // gc_block.modal.cutter_comp = CUTTER_COMP_DISABLE; // G40
            break;
          case 54: case 55: case 56: case 57: case 58: case 59:
            // NOTE: G59.x are not supported. (But their int_values would be 60, 61, and 62.)
            word_bit = MODAL_GROUP_G12;
            gc_block.modal.coord_select = int_value - 54; // Shift to array indexing.
            break;
          case 61:
            word_bit = MODAL_GROUP_G13;
            if (mantissa != 0) {
              FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND);  // [G61.1 not supported]
            }
            // gc_block.modal.control = CONTROL_MODE_EXACT_PATH; // G61
            break;
          default: FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND); // [Unsupported G command]
        }
        if (mantissa > 0) {
          FAIL(STATUS_GCODE_COMMAND_VALUE_NOT_INTEGER);  // [Unsupported or invalid Gxx.x command]
        }
        // Check for more than one command per modal group violations in the current block
        // NOTE: Variable 'word_bit' is always assigned, if the command is valid.
        if ( bit_istrue(command_words, bit(word_bit)) ) {
          FAIL(STATUS_GCODE_MODAL_GROUP_VIOLATION);
        }
        command_words |= bit(word_bit);
        break;

      case 'M':

        // Determine 'M' command and its modal group
        if (mantissa > 0) {
          FAIL(STATUS_GCODE_COMMAND_VALUE_NOT_INTEGER);  // [No Mxx.x commands]
        }
        switch (int_value) {
          case 0: case 1: case 2: case 30:
            word_bit = MODAL_GROUP_M4;
            switch (int_value) {
              case 0: gc_block.modal.program_flow = PROGRAM_FLOW_PAUSED; break; // Program pause
              case 1: break; // Optional stop not supported. Ignore.
              default: gc_block.modal.program_flow = int_value; // Program end and reset
            }
            break;

          default:
            FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND); // [Unsupported M command]
        }

        // Check for more than one command per modal group violations in the current block
        // NOTE: Variable 'word_bit' is always assigned, if the command is valid.
        if ( bit_istrue(command_words, bit(word_bit)) ) {
          FAIL(STATUS_GCODE_MODAL_GROUP_VIOLATION);
        }
        command_words |= bit(word_bit);
        break;

      // NOTE: All remaining letters assign values.
      default:

        /* Non-Command Words: This initial parsing phase only checks for repeats of the remaining
           legal g-code words and stores their value. Error-checking is performed later since some
           words (I,J,K,L,P,R) have multiple connotations and/or depend on the issued commands. */
        switch (letter) {
#ifdef A_AXIS
          case 'A': word_bit = WORD_A; gc_block.values.xyz[A_AXIS] = value; axis_words |= (1 << A_AXIS); break;
#endif
#ifdef B_AXIS
          case 'B': word_bit = WORD_B; gc_block.values.xyz[B_AXIS] = value; axis_words |= (1 << B_AXIS); break;
#endif
#ifdef C_AXIS
          case 'C': word_bit = WORD_C; gc_block.values.xyz[C_AXIS] = value; axis_words |= (1 << C_AXIS); break;
#endif
          // case 'D': // Not supported
          case 'F': word_bit = WORD_F; gc_block.values.f = value; break;
          // case 'H': // Not supported
          case 'I': word_bit = WORD_I; gc_block.values.ijk[X_AXIS] = value; ijk_words |= (1 << X_AXIS); break;
          case 'J': word_bit = WORD_J; gc_block.values.ijk[Y_AXIS] = value; ijk_words |= (1 << Y_AXIS); break;
          case 'L': word_bit = WORD_L; gc_block.values.l = int_value; break;
          case 'P': word_bit = WORD_P; gc_block.values.p = value; break;
          // NOTE: For certain commands, P value must be an integer, but none of these commands are supported.
          case 'X': word_bit = WORD_X; gc_block.values.xyz[X_AXIS] = value; axis_words |= (1 << X_AXIS); break;
          case 'Y': word_bit = WORD_Y; gc_block.values.xyz[Y_AXIS] = value; axis_words |= (1 << Y_AXIS); break;
          default: FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND);
        }

        // NOTE: Variable 'word_bit' is always assigned, if the non-command letter is valid.
        if (bit_istrue(value_words, bit(word_bit))) {
          FAIL(STATUS_GCODE_WORD_REPEATED);  // [Word repeated]
        }
        // Check for invalid negative values for words F, N, P, T, and S.
        // NOTE: Negative value check is done here simply for code-efficiency.
        if ( bit(word_bit) & (bit(WORD_F) | bit(WORD_P) | bit(WORD_T) | bit(WORD_S)) ) {
          if (value < 0.0) {
            FAIL(STATUS_NEGATIVE_VALUE);  // [Word value cannot be negative]
          }
        }
        value_words |= bit(word_bit); // Flag to indicate parameter assigned.

    }
  }
  // Parsing complete!


  /* -------------------------------------------------------------------------------------
     STEP 3: Error-check all commands and values passed in this block. This step ensures all of
     the commands are valid for execution and follows the NIST standard as closely as possible.
     If an error is found, all commands and values in this block are dumped and will not update
     the active system g-code modes. If the block is ok, the active system g-code modes will be
     updated based on the commands of this block, and signal for it to be executed.

     Also, we have to pre-convert all of the values passed based on the modes set by the parsed
     block. There are a number of error-checks that require target information that can only be
     accurately calculated if we convert these values in conjunction with the error-checking.
     This relegates the next execution step as only updating the system g-code modes and
     performing the programmed actions in order. The execution step should not require any
     conversion calculations and would only require minimal checks necessary to execute.
  */

  /* NOTE: At this point, the g-code block has been parsed and the block line can be freed.
     NOTE: It's also possible, at some future point, to break up STEP 2, to allow piece-wise
     parsing of the block on a per-word basis, rather than the entire block. This could remove
     the need for maintaining a large string variable for the entire block and free up some memory.
     To do this, this would simply need to retain all of the data in STEP 1, such as the new block
     data struct, the modal group and value bitflag tracking variables, and axis array indices
     compatible variables. This data contains all of the information necessary to error-check the
     new g-code block when the EOL character is received. However, this would break Grbl's startup
     lines in how it currently works and would require some refactoring to make it compatible.
  */

  // [0. Non-specific/common error-checks and miscellaneous setup]:

  // Determine implicit axis command conditions. Axis words have been passed, but no explicit axis
  // command has been sent. If so, set axis command to current motion mode.
  if (axis_words) {
    if (!axis_command) {
      axis_command = AXIS_COMMAND_MOTION_MODE;  // Assign implicit motion-mode
    }
  }


  // Track for unused words at the end of error-checking.
  // NOTE: Single-meaning value words are removed all at once at the end of error-checking, because
  // they are always used when present. This was done to save a few bytes of flash. For clarity, the
  // single-meaning value words may be removed as they are used. Also, axis words are treated in the
  // same way. If there is an explicit/implicit axis command, XYZ words are always used and are
  // are removed at the end of error-checking.

  // [1. Comments ]: MSG's NOT SUPPORTED. Comment handling performed by protocol.

  // [2. Set feed rate mode ]: G93 F word missing with G1,G2/3 active, implicitly or explicitly. Feed rate
  //   is not defined after switching to G94 from G93.
  // NOTE: For jogging, ignore prior feed rate mode. Enforce G94 and check for required F word.
  if (gc_parser_flags & GC_PARSER_JOG_MOTION) {
    if (bit_isfalse(value_words, bit(WORD_F))) {
      FAIL(STATUS_GCODE_UNDEFINED_FEED_RATE);
    }
  } else {
    if (gc_block.modal.feed_rate == FEED_RATE_MODE_INVERSE_TIME) { // = G93
      // NOTE: G38 can also operate in inverse time, but is undefined as an error. Missing F word check added here.
      if (axis_command == AXIS_COMMAND_MOTION_MODE) {
        if ((gc_block.modal.motion != MOTION_MODE_NONE) || (gc_block.modal.motion != MOTION_MODE_SEEK)) {
          if (bit_isfalse(value_words, bit(WORD_F))) {
            FAIL(STATUS_GCODE_UNDEFINED_FEED_RATE);  // [F word missing]
          }
        }
      }
      // NOTE: It seems redundant to check for an F word to be passed after switching from G94 to G93. We would
      // accomplish the exact same thing if the feed rate value is always reset to zero and undefined after each
      // inverse time block, since the commands that use this value already perform undefined checks. This would
      // also allow other commands, following this switch, to execute and not error out needlessly. This code is
      // combined with the above feed rate mode and the below set feed rate error-checking.

      // [3. Set feed rate ]: F is negative (done.)
      // - In inverse time mode: Always implicitly zero the feed rate value before and after block completion.
      // NOTE: If in G93 mode or switched into it from G94, just keep F value as initialized zero or passed F word
      // value in the block. If no F word is passed with a motion command that requires a feed rate, this will error
      // out in the motion modes error-checking. However, if no F word is passed with NO motion command that requires
      // a feed rate, we simply move on and the state feed rate value gets updated to zero and remains undefined.
    } else { // = G94
      // - In units per mm mode: If F word passed, ensure value is in mm/min, otherwise push last state value.
      if (gc_state.modal.feed_rate == FEED_RATE_MODE_UNITS_PER_MIN) { // Last state is also G94
        if (bit_istrue(value_words, bit(WORD_F))) {

        } else {
          gc_block.values.f = gc_state.feed_rate; // Push last state feed rate
        }
      } // Else, switching to G94 from G93, so don't push last state feed rate. Its undefined or the passed F word value.
    }
  }
  // bit_false(value_words,bit(WORD_F)); // NOTE: Single-meaning value word. Set at end of error-checking.


  // [10. Dwell ]: P value missing. P is negative (done.) NOTE: See below.
  if (gc_block.non_modal_command == NON_MODAL_DWELL) {
    if (bit_isfalse(value_words, bit(WORD_P))) {
      FAIL(STATUS_GCODE_VALUE_WORD_MISSING);  // [P word missing]
    }
    bit_false(value_words, bit(WORD_P));
  }

  // [11. Set active plane ]: N/A
  switch (gc_block.modal.plane_select) {
    //case PLANE_SELECT_XY:
      axis_0 = X_AXIS;
      axis_1 = Y_AXIS;
      //break;
  }

  uint8_t idx;


  // [15. Coordinate system selection ]: *N/A. Error, if cutter radius comp is active.
  // TODO: An EEPROM read of the coordinate data may require a buffer sync when the cycle
  // is active. The read pauses the processor temporarily and may cause a rare crash. For
  // future versions on processors with enough memory, all coordinate data should be stored
  // in memory and written to EEPROM only when there is not a cycle active.
  float block_coord_system[N_AXIS];
  memcpy(block_coord_system, gc_state.coord_system, sizeof(gc_state.coord_system));
  if ( bit_istrue(command_words, bit(MODAL_GROUP_G12)) ) { // Check if called in block
    if (gc_block.modal.coord_select > N_COORDINATE_SYSTEM) {
      FAIL(STATUS_GCODE_UNSUPPORTED_COORD_SYS);  // [Greater than N sys]
    }
    if (gc_state.modal.coord_select != gc_block.modal.coord_select) {
      if (!(settings_read_coord_data(gc_block.modal.coord_select, block_coord_system))) {
        FAIL(STATUS_SETTING_READ_FAIL);
      }
    }
  }

  // [16. Set path control mode ]: N/A. Only G61. G61.1 and G64 NOT SUPPORTED.
  // [17. Set distance mode ]: N/A. Only G91.1. G90.1 NOT SUPPORTED.
  // [18. Set retract mode ]: NOT SUPPORTED.

  // [19. Remaining non-modal actions ]: Check go to predefined position, set G10, or set axis offsets.
  // NOTE: We need to separate the non-modal commands that are axis word-using (G10/G28/G30/G92), as these
  // commands all treat axis words differently. G10 as absolute offsets or computes current position as
  // the axis value, G92 similarly to G10 L20, and G28/30 as an intermediate target position that observes
  // all the current coordinate system and G92 offsets.
  switch (gc_block.non_modal_command) {
    case NON_MODAL_SET_COORDINATE_DATA:
      // [G10 Errors]: L missing and is not 2 or 20. P word missing. (Negative P value done.)
      // [G10 L2 Errors]: R word NOT SUPPORTED. P value not 0 to nCoordSys(max 9). Axis words missing.
      // [G10 L20 Errors]: P must be 0 to nCoordSys(max 9). Axis words missing.
      if (!axis_words) {
        FAIL(STATUS_GCODE_NO_AXIS_WORDS)
      }; // [No axis words]
      if (bit_isfalse(value_words, ((1 << WORD_P) | (1 << WORD_L)))) {
        FAIL(STATUS_GCODE_VALUE_WORD_MISSING);  // [P/L word missing]
      }
      coord_select = trunc(gc_block.values.p); // Convert p value to int.
      if (coord_select > N_COORDINATE_SYSTEM) {
        FAIL(STATUS_GCODE_UNSUPPORTED_COORD_SYS);  // [Greater than N sys]
      }
      if (gc_block.values.l != 20) {
        if (gc_block.values.l == 2) {
          if (bit_istrue(value_words, bit(WORD_R))) {
            FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND);  // [G10 L2 R not supported]
          }
        } else {
          FAIL(STATUS_GCODE_UNSUPPORTED_COMMAND);  // [Unsupported L]
        }
      }
      bit_false(value_words, (bit(WORD_L) | bit(WORD_P)));

      // Determine coordinate system to change and try to load from EEPROM.
      if (coord_select > 0) {
        coord_select--;  // Adjust P1-P6 index to EEPROM coordinate data indexing.
      }
      else {
        coord_select = gc_block.modal.coord_select;  // Index P0 as the active coordinate system
      }

      // NOTE: Store parameter data in IJK values. By rule, they are not in use with this command.
      // FIXME: Instead of IJK, we'd better use: float vector[N_AXIS]; // [DG]
      if (!settings_read_coord_data(coord_select, gc_block.values.ijk)) {
        FAIL(STATUS_SETTING_READ_FAIL);  // [EEPROM read fail]
      }

      // Pre-calculate the coordinate data changes.
      for (idx = 0; idx < N_AXIS; idx++) { // Axes indices are consistent, so loop may be used.
        // Update axes defined only in block. Always in machine coordinates. Can change non-active system.
        if (bit_istrue(axis_words, bit(idx)) ) {
          if (gc_block.values.l == 20) {
            // L20: Update coordinate system axis at current position (with modifiers) with programmed value
            // WPos = MPos - WCS - G92 - TLO  ->  WCS = MPos - G92 - TLO - WPos
            gc_block.values.ijk[idx] = gc_state.position[idx] - gc_state.coord_offset[idx] - gc_block.values.xyz[idx];
          } else {
            // L2: Update coordinate system axis to programmed value.
            gc_block.values.ijk[idx] = gc_block.values.xyz[idx];
          }
        } // Else, keep current stored value.
      }
      break;
    case NON_MODAL_SET_COORDINATE_OFFSET:
      // [G92 Errors]: No axis words.
      if (!axis_words) {
        FAIL(STATUS_GCODE_NO_AXIS_WORDS);  // [No axis words]
      }

      // Update axes defined only in block. Offsets current system to defined value. Does not update when
      // active coordinate system is selected, but is still active unless G92.1 disables it.
      for (idx = 0; idx < N_AXIS; idx++) { // Axes indices are consistent, so loop may be used.
        if (bit_istrue(axis_words, bit(idx)) ) {
          // WPos = MPos - WCS - G92 - TLO  ->  G92 = MPos - WCS - TLO - WPos
          gc_block.values.xyz[idx] = gc_state.position[idx] - block_coord_system[idx] - gc_block.values.xyz[idx];
        } else {
          gc_block.values.xyz[idx] = gc_state.coord_offset[idx];
        }
      }
      break;

    default:

      // At this point, the rest of the explicit axis commands treat the axis values as the traditional
      // target position with the coordinate system offsets, G92 offsets, absolute override, and distance
      // modes applied. This includes the motion mode commands. We can now pre-compute the target position.
        if (axis_words) {
          for (idx = 0; idx < N_AXIS; idx++) { // Axes indices are consistent, so loop may be used to save flash space.
            if ( bit_isfalse(axis_words, bit(idx)) ) {
              gc_block.values.xyz[idx] = gc_state.position[idx]; // No axis word in block. Keep same axis position.
            } else {
              // Update specified value according to distance mode or ignore if absolute override is active.
              // NOTE: G53 is never active with G28/30 since they are in the same modal group.
              if (gc_block.non_modal_command != NON_MODAL_ABSOLUTE_OVERRIDE) {
                // Apply coordinate offsets based on distance mode.
                if (gc_block.modal.distance == DISTANCE_MODE_ABSOLUTE) {
                  gc_block.values.xyz[idx] += block_coord_system[idx] + gc_state.coord_offset[idx];
                } else {  // Incremental mode
                  gc_block.values.xyz[idx] += gc_state.position[idx];
                }
              }
            }
          }
        }
      

      // Check remaining non-modal commands for errors.
      switch (gc_block.non_modal_command) {
        case NON_MODAL_GO_HOME_0: // G28
        case NON_MODAL_GO_HOME_1: // G30
          // [G28/30 Errors]: Cutter compensation is enabled.
          // Retreive G28/30 go-home position data (in machine coordinates) from EEPROM
          // NOTE: Store parameter data in IJK values. By rule, they are not in use with this command.
          if (gc_block.non_modal_command == NON_MODAL_GO_HOME_0) {
            if (!settings_read_coord_data(SETTING_INDEX_G28, gc_block.values.ijk)) {
              FAIL(STATUS_SETTING_READ_FAIL);
            }
          } else { // == NON_MODAL_GO_HOME_1
            if (!settings_read_coord_data(SETTING_INDEX_G30, gc_block.values.ijk)) {
              FAIL(STATUS_SETTING_READ_FAIL);
            }
          }
          if (axis_words) {
            // Move only the axes specified in secondary move.
            for (idx = 0; idx < N_AXIS; idx++) {
              if (!(axis_words & (1 << idx))) {
                gc_block.values.ijk[idx] = gc_state.position[idx];
              }
            }
          } else {
            axis_command = AXIS_COMMAND_NONE; // Set to none if no intermediate motion.
          }
          break;
        case NON_MODAL_SET_HOME_0: // G28.1
        case NON_MODAL_SET_HOME_1: // G30.1
          // [G28.1/30.1 Errors]: Cutter compensation is enabled.
          // NOTE: If axis words are passed here, they are interpreted as an implicit motion mode.
          break;
        case NON_MODAL_RESET_COORDINATE_OFFSET:
          // NOTE: If axis words are passed here, they are interpreted as an implicit motion mode.
          break;
        case NON_MODAL_ABSOLUTE_OVERRIDE:
          // [G53 Errors]: G0 and G1 are not active. Cutter compensation is enabled.
          // NOTE: All explicit axis word commands are in this modal group. So no implicit check necessary.
          if (!(gc_block.modal.motion == MOTION_MODE_SEEK || gc_block.modal.motion == MOTION_MODE_LINEAR)) {
            FAIL(STATUS_GCODE_G53_INVALID_MOTION_MODE); // [G53 G0/1 not active]
          }
          break;
      }
  }

  // [20. Motion modes ]:
  if (gc_block.modal.motion == MOTION_MODE_NONE) {
    // [G80 Errors]: Axis word are programmed while G80 is active.
    // NOTE: Even non-modal commands or TLO that use axis words will throw this strict error.
    if (axis_words) {
      FAIL(STATUS_GCODE_AXIS_WORDS_EXIST);  // [No axis words allowed]
    }

    // Check remaining motion modes, if axis word are implicit (exist and not used by G10/28/30/92), or
    // was explicitly commanded in the g-code block.
  } else if ( axis_command == AXIS_COMMAND_MOTION_MODE ) {

    if (gc_block.modal.motion == MOTION_MODE_SEEK) {
      // [G0 Errors]: Axis letter not configured or without real value (done.)
      // Axis words are optional. If missing, set axis command flag to ignore execution.
      if (!axis_words) {
        axis_command = AXIS_COMMAND_NONE;
      }

      // All remaining motion modes (all but G0 and G80), require a valid feed rate value. In units per mm mode,
      // the value must be positive. In inverse time mode, a positive value must be passed with each block.
    } else {
      // Check if feed rate is defined for the motion modes that require it.
      if (gc_block.values.f == 0.0) {
        FAIL(STATUS_GCODE_UNDEFINED_FEED_RATE);  // [Feed rate undefined]
      }

      switch (gc_block.modal.motion) {
        case MOTION_MODE_LINEAR:
          // [G1 Errors]: Feed rate undefined. Axis letter not configured or without real value.
          // Axis words are optional. If missing, set axis command flag to ignore execution.
          if (!axis_words) {
            axis_command = AXIS_COMMAND_NONE;
          }
          break;
      }
    }
  }

  // [21. Program flow ]: No error checks required.

  // [0. Non-specific error-checks]: Complete unused value words check, i.e. IJK used when in arc
  // radius mode, or axis words that aren't used in the block.
  if (gc_parser_flags & GC_PARSER_JOG_MOTION) {
    // Jogging only uses the F feed rate and XYZ value words. N is valid, but S and T are invalid.
    bit_false(value_words, ( bit(WORD_F)));
  } else {
    bit_false(value_words, ( bit(WORD_F) | bit(WORD_S) | bit(WORD_T))); // Remove single-meaning value words.
  }

  if (axis_command) {
    bit_false(value_words, (bit(WORD_X) | bit(WORD_Y) | bit(WORD_Z) | bit(WORD_A) | bit(WORD_B) | bit(WORD_C)));  // Remove axis words.
  }
  if (value_words) {
    FAIL(STATUS_GCODE_UNUSED_WORDS);  // [Unused words]
  }

  /* -------------------------------------------------------------------------------------
     STEP 4: EXECUTE!!
     Assumes that all error-checking has been completed and no failure modes exist. We just
     need to update the state and execute the block according to the order-of-execution.
  */

  // Initialize planner data struct for motion blocks.
  plan_line_data_t plan_data;
  plan_line_data_t *pl_data = &plan_data;
  memset(pl_data, 0, sizeof(plan_line_data_t)); // Zero pl_data struct

  // Intercept jog commands and complete error checking for valid jog commands and execute.
  // NOTE: G-code parser state is not updated, except the position to ensure sequential jog
  // targets are computed correctly. The final parser position after a jog is updated in
  // protocol_execute_realtime() when jogging completes or is canceled.
  if (gc_parser_flags & GC_PARSER_JOG_MOTION) {
    // Only distance and unit modal commands and G53 absolute override command are allowed.
    // NOTE: Feed rate word and axis word checks have already been performed in STEP 3.
    if (command_words & ~(bit(MODAL_GROUP_G3) | bit(MODAL_GROUP_G0)))  {
      FAIL(STATUS_INVALID_JOG_COMMAND)
    };
    if (!(gc_block.non_modal_command == NON_MODAL_ABSOLUTE_OVERRIDE || gc_block.non_modal_command == NON_MODAL_NO_ACTION)) {
      FAIL(STATUS_INVALID_JOG_COMMAND);
    }


    uint8_t status = jog_execute(&plan_data, &gc_block);
    if (status == STATUS_OK) {
      memcpy(gc_state.position, gc_block.values.xyz, sizeof(gc_block.values.xyz));
    }
    return (status);
  }


  // [2. Set feed rate mode ]:
  gc_state.modal.feed_rate = gc_block.modal.feed_rate;
  if (gc_state.modal.feed_rate) {
    pl_data->condition |= PL_COND_FLAG_INVERSE_TIME;  // Set condition flag for planner use.
  }

  // [3. Set feed rate ]:
  gc_state.feed_rate = gc_block.values.f; // Always copy this value. See feed rate error-checking.
  pl_data->feed_rate = gc_state.feed_rate; // Record data for planner use.



  // [10. Dwell ]:
  if (gc_block.non_modal_command == NON_MODAL_DWELL) {
    mc_dwell(gc_block.values.p);
  }

  // [11. Set active plane ]:
  gc_state.modal.plane_select = gc_block.modal.plane_select;


  // [15. Coordinate system selection ]:
  if (gc_state.modal.coord_select != gc_block.modal.coord_select) {
    gc_state.modal.coord_select = gc_block.modal.coord_select;
    memcpy(gc_state.coord_system, block_coord_system, N_AXIS * sizeof(float));
    system_flag_wco_change();
  }

  // [16. Set path control mode ]: G61.1/G64 NOT SUPPORTED
  // gc_state.modal.control = gc_block.modal.control; // NOTE: Always default.

  // [17. Set distance mode ]:
  gc_state.modal.distance = gc_block.modal.distance;

  // [18. Set retract mode ]: NOT SUPPORTED

  // [19. Go to predefined position, Set G10, or Set axis offsets ]:
  switch (gc_block.non_modal_command) {
    case NON_MODAL_SET_COORDINATE_DATA:
      settings_write_coord_data(coord_select, gc_block.values.ijk);
      // Update system coordinate system if currently active.
      if (gc_state.modal.coord_select == coord_select) {
        memcpy(gc_state.coord_system, gc_block.values.ijk, N_AXIS * sizeof(float));
        system_flag_wco_change();
      }
      break;
    case NON_MODAL_GO_HOME_0: case NON_MODAL_GO_HOME_1:
      // Move to intermediate position before going home. Obeys current coordinate system and offsets
      // and absolute and incremental modes.
      pl_data->condition |= PL_COND_FLAG_RAPID_MOTION; // Set rapid motion condition flag.
      if (axis_command) {
        mc_line(gc_block.values.xyz, pl_data);
      }
      mc_line(gc_block.values.ijk, pl_data);
      memcpy(gc_state.position, gc_block.values.ijk, N_AXIS * sizeof(float));
      break;
    case NON_MODAL_SET_HOME_0:
      settings_write_coord_data(SETTING_INDEX_G28, gc_state.position);
      break;
    case NON_MODAL_SET_HOME_1:
      settings_write_coord_data(SETTING_INDEX_G30, gc_state.position);
      break;
    case NON_MODAL_SET_COORDINATE_OFFSET:
      memcpy(gc_state.coord_offset, gc_block.values.xyz, sizeof(gc_block.values.xyz));
      system_flag_wco_change();
      break;
    case NON_MODAL_RESET_COORDINATE_OFFSET:
      clear_vector(gc_state.coord_offset); // Disable G92 offsets by zeroing offset vector.
      system_flag_wco_change();
      break;
  }


  // [20. Motion modes ]:
  // NOTE: Commands G10,G28,G30,G92 lock out and prevent axis words from use in motion modes.
  // Enter motion modes only if there are axis words or a motion mode command word in the block.
  gc_state.modal.motion = gc_block.modal.motion;
  if (gc_state.modal.motion != MOTION_MODE_NONE) {
    if (axis_command == AXIS_COMMAND_MOTION_MODE) {
      uint8_t gc_update_pos = GC_UPDATE_POS_TARGET;
      if (gc_state.modal.motion == MOTION_MODE_LINEAR) {
        mc_line(gc_block.values.xyz, pl_data);
      } else if (gc_state.modal.motion == MOTION_MODE_SEEK) {
        pl_data->condition |= PL_COND_FLAG_RAPID_MOTION; // Set rapid motion condition flag.
        mc_line(gc_block.values.xyz, pl_data);
      } else {
        // NOTE: gc_block.values.xyz is returned from mc_probe_cycle with the updated position value. So
        // upon a successful probing cycle, the machine position and the returned value should be the same.
        //gc_update_pos = mc_probe_cycle(gc_block.values.xyz, pl_data, gc_parser_flags);
      }

      // As far as the parser is concerned, the position is now == target. In reality the
      // motion control system might still be processing the action and the real tool position
      // in any intermediate location.
      if (gc_update_pos == GC_UPDATE_POS_TARGET) {
        memcpy(gc_state.position, gc_block.values.xyz, sizeof(gc_block.values.xyz)); // gc_state.position[] = gc_block.values.xyz[]
      } else if (gc_update_pos == GC_UPDATE_POS_SYSTEM) {
        gc_sync_position(); // gc_state.position[] = sys_position
      } // == GC_UPDATE_POS_NONE
    }
  }

  // [21. Program flow ]:
  // M0,M1,M2,M30: Perform non-running program flow actions. During a program pause, the buffer may
  // refill and can only be resumed by the cycle start run-time command.
  gc_state.modal.program_flow = gc_block.modal.program_flow;
  if (gc_state.modal.program_flow) {
    protocol_buffer_synchronize(); // Sync and finish all remaining buffered motions before moving on.
    if (gc_state.modal.program_flow == PROGRAM_FLOW_PAUSED) {
      if (sys.state != STATE_CHECK_MODE) {
        system_set_exec_state_flag(EXEC_FEED_HOLD); // Use feed hold for program pause.
        protocol_execute_realtime(); // Execute suspend.
      }
    } else { // == PROGRAM_FLOW_COMPLETED
      // Upon program complete, only a subset of g-codes reset to certain defaults, according to
      // LinuxCNC's program end descriptions and testing. Only modal groups [G-code 1,2,3,5,7,12]
      // and [M-code 7,8,9] reset to [G1,G17,G90,G94,G40,G54,M5,M9,M48]. The remaining modal groups
      // [G-code 4,6,8,10,13,14,15] and [M-code 4,5,6] and the modal words [F,S,T,H] do not reset.
      gc_state.modal.motion = MOTION_MODE_LINEAR;
      gc_state.modal.plane_select = PLANE_SELECT_XY;
      gc_state.modal.distance = DISTANCE_MODE_ABSOLUTE;
      gc_state.modal.feed_rate = FEED_RATE_MODE_UNITS_PER_MIN;
      // gc_state.modal.cutter_comp = CUTTER_COMP_DISABLE; // Not supported.
      gc_state.modal.coord_select = 0; // G54


      // Execute coordinate change
      if (sys.state != STATE_CHECK_MODE) {
        if (!(settings_read_coord_data(gc_state.modal.coord_select, gc_state.coord_system))) {
          FAIL(STATUS_SETTING_READ_FAIL);
        }
        system_flag_wco_change(); // Set to refresh immediately just in case something altered.
      }
      report_feedback_message(MESSAGE_PROGRAM_END);
    }
    gc_state.modal.program_flow = PROGRAM_FLOW_RUNNING; // Reset program flow.
  }

  // TODO: % to denote start of program.

  return (STATUS_OK);
}
