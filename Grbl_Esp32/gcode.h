/*
  gcode.h - rs274/ngc parser.
  Part of Grbl

  Copyright (c) 2011-2015 Sungeun K. Jeon
  Copyright (c) 2009-2011 Simen Svale Skogsrud

*/
#ifndef gcode_h
#define gcode_h


// Define modal group internal numbers for checking multiple command violations and tracking the
// type of command that is called in the block. A modal group is a group of g-code commands that are
// mutually exclusive, or cannot exist on the same line, because they each toggle a state or execute
// a unique motion. These are defined in the NIST RS274-NGC v3 g-code standard, available online,
// and are similar/identical to other g-code interpreters by manufacturers (Haas,Fanuc,Mazak,etc).
// NOTE: Modal group define values must be sequential and starting from zero.
#define MODAL_GROUP_G0 0 // [G4,G10,G28,G28.1,G30,G30.1,G53,G92,G92.1] Non-modal
#define MODAL_GROUP_G1 1 // [G0,G1,G2,G3,G38.2,G38.3,G38.4,G38.5,G80] Motion
#define MODAL_GROUP_G2 2 // [G17,G18,G19] Plane selection
#define MODAL_GROUP_G3 3 // [G90,G91] Distance mode
#define MODAL_GROUP_G4 4 // [G91.1] Arc IJK distance mode
#define MODAL_GROUP_G5 5 // [G93,G94] Feed rate mode
#define MODAL_GROUP_G6 6 // [G20,G21] Units
#define MODAL_GROUP_G7 7 // [G40] Cutter radius compensation mode. G41/42 NOT SUPPORTED.
#define MODAL_GROUP_G8 8 // [G43.1,G49] Tool length offset
#define MODAL_GROUP_G12 9 // [G54,G55,G56,G57,G58,G59] Coordinate system selection
#define MODAL_GROUP_G13 10 // [G61] Control mode

#define MODAL_GROUP_M4 11  // [M0,M1,M2,M30] Stopping

// #define OTHER_INPUT_F 14
// #define OTHER_INPUT_S 15
// #define OTHER_INPUT_T 16

// Define command actions for within execution-type modal groups (motion, stopping, non-modal). Used
// internally by the parser to know which command to execute.
// NOTE: Some macro values are assigned specific values to make g-code state reporting and parsing
// compile a litte smaller. Necessary due to being completely out of flash on the 328p. Although not
// ideal, just be careful with values that state 'do not alter' and check both report.c and gcode.c
// to see how they are used, if you need to alter them.

// Modal Group G0: Non-modal actions
#define NON_MODAL_NO_ACTION 0 // (Default: Must be zero)
#define NON_MODAL_DWELL 4 // G4 (Do not alter value)
#define NON_MODAL_SET_COORDINATE_DATA 10 // G10 (Do not alter value)
#define NON_MODAL_GO_HOME_0 28 // G28 (Do not alter value)
#define NON_MODAL_SET_HOME_0 38 // G28.1 (Do not alter value)
#define NON_MODAL_GO_HOME_1 30 // G30 (Do not alter value)
#define NON_MODAL_SET_HOME_1 40 // G30.1 (Do not alter value)
#define NON_MODAL_ABSOLUTE_OVERRIDE 53 // G53 (Do not alter value)
#define NON_MODAL_SET_COORDINATE_OFFSET 92 // G92 (Do not alter value)
#define NON_MODAL_RESET_COORDINATE_OFFSET 102 //G92.1 (Do not alter value)

// Modal Group G1: Motion modes
#define MOTION_MODE_SEEK 0 // G0 (Default: Must be zero)
#define MOTION_MODE_LINEAR 1 // G1 (Do not alter value)
#define MOTION_MODE_CW_ARC 2  // G2 (Do not alter value)
#define MOTION_MODE_CCW_ARC 3  // G3 (Do not alter value)
#define MOTION_MODE_NONE 80 // G80 (Do not alter value)

// Modal Group G2: Plane select
#define PLANE_SELECT_XY 0 // G17 (Default: Must be zero)
#define PLANE_SELECT_ZX 1 // G18 (Do not alter value)
#define PLANE_SELECT_YZ 2 // G19 (Do not alter value)

// Modal Group G3: Distance mode
#define DISTANCE_MODE_ABSOLUTE 0 // G90 (Default: Must be zero)
#define DISTANCE_MODE_INCREMENTAL 1 // G91 (Do not alter value)

// Modal Group G4: Arc IJK distance mode
#define DISTANCE_ARC_MODE_INCREMENTAL 0 // G91.1 (Default: Must be zero)

// Modal Group M4: Program flow
#define PROGRAM_FLOW_RUNNING 0 // (Default: Must be zero)
#define PROGRAM_FLOW_PAUSED 3 // M0
#define PROGRAM_FLOW_OPTIONAL_STOP 1 // M1 NOTE: Not supported, but valid and ignored.
#define PROGRAM_FLOW_COMPLETED_M2  2 // M2 (Do not alter value)
#define PROGRAM_FLOW_COMPLETED_M30 30 // M30 (Do not alter value)

// Modal Group G5: Feed rate mode
#define FEED_RATE_MODE_UNITS_PER_MIN  0 // G94 (Default: Must be zero)
#define FEED_RATE_MODE_INVERSE_TIME   1 // G93 (Do not alter value)

// Modal Group G7: Cutter radius compensation mode
#define CUTTER_COMP_DISABLE 0 // G40 (Default: Must be zero)

// Modal Group G13: Control mode
#define CONTROL_MODE_EXACT_PATH 0 // G61 (Default: Must be zero)


// Modal Group G12: Active work coordinate system
// N/A: Stores coordinate system value (54-59) to change to.

// Define parameter word mapping.
#define WORD_F  0
#define WORD_I  1
#define WORD_J  2
#define WORD_K  3
#define WORD_L  4
#define WORD_P  6
#define WORD_R  7
#define WORD_S  8
#define WORD_T  9
#define WORD_X  10
#define WORD_Y  11
#define WORD_Z  12
#define WORD_A  13
#define WORD_B  14
#define WORD_C  15

// Define g-code parser position updating flags
#define GC_UPDATE_POS_TARGET   0 // Must be zero
#define GC_UPDATE_POS_SYSTEM   1
#define GC_UPDATE_POS_NONE     2

// Define gcode parser flags for handling special cases.
#define GC_PARSER_NONE                  0 // Must be zero.
#define GC_PARSER_JOG_MOTION            bit(0)
#define GC_PARSER_CHECK_MANTISSA        bit(1)
#define GC_PARSER_ARC_IS_CLOCKWISE      bit(2)


// NOTE: When this struct is zeroed, the above defines set the defaults for the system.
typedef struct {
  uint8_t motion;          // {G0,G1,G2,G3,G38.2,G80}
  uint8_t feed_rate;       // {G93,G94}
  uint8_t units;           // {G20,G21}
  uint8_t distance;        // {G90,G91}
  // uint8_t distance_arc; // {G91.1} NOTE: Don't track. Only default supported.
  uint8_t plane_select;    // {G17,G18,G19}
  // uint8_t cutter_comp;  // {G40} NOTE: Don't track. Only default supported.
  uint8_t coord_select;    // {G54,G55,G56,G57,G58,G59}
  // uint8_t control;      // {G61} NOTE: Don't track. Only default supported.
  uint8_t program_flow;    // {M0,M1,M2,M30}
} gc_modal_t;

typedef struct {
  float f;         // Feed
  float ijk[N_AXIS];    // I,J,K Axis arc offsets
  uint8_t l;       // G10 or canned cycles parameters
  float p;         // G10 or dwell parameters
  // float q;      // G82 peck drilling
  float r;         // Arc radius
  uint8_t t;       // Tool selection
  float xyz[N_AXIS];    // X,Y,Z Translational axes
} gc_values_t;


typedef struct {
  gc_modal_t modal;

  float feed_rate;              // Millimeters/min
  uint8_t tool;                 // Tracks tool number. NOT USED.

  float position[N_AXIS];       // Where the interpreter considers the tool to be at this point in the code

  float coord_system[N_AXIS];    // Current work coordinate system (G54+). Stores offset from absolute machine
  // position in mm. Loaded from EEPROM when called.
  float coord_offset[N_AXIS];    // Retains the G92 coordinate offset (work coordinates) relative to
  // machine zero in mm. Non-persistent. Cleared upon reset and boot.
} parser_state_t;
extern parser_state_t gc_state;


typedef struct {
  uint8_t non_modal_command;
  gc_modal_t modal;
  gc_values_t values;
} parser_block_t;


// Initialize the parser
void gc_init();

// Execute one block of rs275/ngc/g-code
uint8_t gc_execute_line(char *line, uint8_t client);

// Set g-code parser position. Input in steps.
void gc_sync_position();

#endif
