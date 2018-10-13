/*
  gcode.h - rs274/ngc parser.
  Part of Grbl

  Copyright (c) 2011-2015 Sungeun K. Jeon
  Copyright (c) 2009-2011 Simen Svale Skogsrud

*/
#ifndef gcode_h
#define gcode_h

/*
G0 : Rapid linear Move
G1 : Linear Move
Usage
G0 Xnnn Ynnn Znnn Ennn Fnnn Snnn
G1 Xnnn Ynnn Znnn Ennn Fnnn Snnn
Parameters
Not all parameters need to be used, but at least one has to be used
Xnnn The position to move to on the X axis
Ynnn The position to move to on the Y axis
Znnn The position to move to on the Z axis
Ennn The amount to extrude between the starting point and ending point
Fnnn The feedrate per minute of the move between the starting point and ending point (if supplied)
Snnn Flag to check if an endstop was hit (S1 to check, S0 to ignore, S2 see note, default is S0)

G2 & G3: Controlled Arc Move
Usage
G2 Xnnn Ynnn Innn Jnnn Ennn Fnnn (Clockwise Arc)
G3 Xnnn Ynnn Innn Jnnn Ennn Fnnn (Counter-Clockwise Arc)
Parameters
Xnnn The position to move to on the X axis
Ynnn The position to move to on the Y axis
Innn The point in X space from the current X position to maintain a constant distance from
Jnnn The point in Y space from the current Y position to maintain a constant distance from
Ennn The amount to extrude between the starting point and ending point
Fnnn The feedrate per minute of the move between the starting point and ending point (if supplied)

G4: Dwell (pausa)
Parameters
Pnnn Time to wait, in milliseconds (In Teacup, P0, wait until all previous moves are finished)
Snnn Time to wait, in seconds (Only on Repetier, Marlin, Smoothieware, and RepRapFirmware 1.16 and later)

G10: Tool Offset?
Usage
G10 Pnnn Xnnn Ynnn Znnn Rnnn Snnn1
Parameters
Pnnn Tool number
Xnnn X offset
Ynnn Y offset
U,V,Wnnn U, V and W axis offsets5
Znnn Z offset2
Rnnn Standby temperature(s)
Snnn Active temperature(s)

G28: Move to Origin (Home)
Parameters
This command can be used without any additional parameters.
X Flag to go back to the X axis origin
Y Flag to go back to the Y axis origin
Z Flag to go back to the Z axis origin

G80 stop canned cycle

G90: Set to Absolute Positioning

G91: Set to Relative Positioning

G92: Set Position (preset, senza paraetri = tutti a 0)
Parameters
This command can be used without any additional parameters.
Xnnn new X axis position
Ynnn new Y axis position
Znnn new Z axis position
Ennn new extruder position

G92.1 - reset axis offsets to zero and set parameters 5211 - 5219 to zero.

G93: Feed Rate Mode (Inverse Time Mode)

G94: Feed Rate Mode (Units per Minute)

M0: Stop or Unconditional stop
Parameters
This command can be used without any additional parameters.
Pnnn Time to wait, in milliseconds1
Snnn Time to wait, in seconds2

M1: Sleep or Conditional stop

M2: Program End

M30: Program End and Reset

Coordinates
Word Description
X X-axe
Y Y-axe
Z Z-axe
I X-offset
J Y-offset
K Z-offset
R Radius
Instruments (tools)
Word Description
T Tool Selection
H Tool Length Offset
F Feed Rate (X-Y-Z movement speed)
S Spin (turning) Speed 

 */


// Define modal group internal numbers for checking multiple command violations and tracking the
// type of command that is called in the block. A modal group is a group of g-code commands that are
// mutually exclusive, or cannot exist on the same line, because they each toggle a state or execute
// a unique motion. These are defined in the NIST RS274-NGC v3 g-code standard, available online,
// and are similar/identical to other g-code interpreters by manufacturers (Haas,Fanuc,Mazak,etc).
// NOTE: Modal group define values must be sequential and starting from zero.
#define MODAL_GROUP_G0 0 // [G4,G10,G92,G92.1] Non-modal  - G10 and G92 preset G28 HOME
#define MODAL_GROUP_G1 1 // [G0,G1,G2,G3,G80] Motion
#define MODAL_GROUP_G3 3 // [G90,G91] Distance mode - absolute or relative
#define MODAL_GROUP_G5 5 // [G93,G94] Feed rate mode - mm/min or inverse time
#define MODAL_GROUP_M4 11  // [M0,M1,M2,M30] Stopping


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
#define NON_MODAL_SET_COORDINATE_OFFSET 92 // G92 (Do not alter value)
#define NON_MODAL_RESET_COORDINATE_OFFSET 102 //G92.1 (Do not alter value)

// Modal Group G1: Motion modes
#define MOTION_MODE_SEEK 0 // G0 (Default: Must be zero)
#define MOTION_MODE_LINEAR 1 // G1 (Do not alter value)
#define MOTION_MODE_NONE 80 // G80 (Do not alter value)


// Modal Group G3: Distance mode
#define DISTANCE_MODE_ABSOLUTE 0 // G90 (Default: Must be zero)
#define DISTANCE_MODE_INCREMENTAL 1 // G91 (Do not alter value)

// Modal Group M4: Program flow
#define PROGRAM_FLOW_RUNNING 0 // (Default: Must be zero)
#define PROGRAM_FLOW_PAUSED 3 // M0
#define PROGRAM_FLOW_OPTIONAL_STOP 1 // M1 NOTE: Not supported, but valid and ignored.
#define PROGRAM_FLOW_COMPLETED_M2  2 // M2 (Do not alter value)
#define PROGRAM_FLOW_COMPLETED_M30 30 // M30 (Do not alter value)

// Modal Group G5: Feed rate mode
#define FEED_RATE_MODE_UNITS_PER_MIN  0 // G94 (Default: Must be zero)
#define FEED_RATE_MODE_INVERSE_TIME   1 // G93 (Do not alter value)


// Define parameter word mapping.
#define WORD_F  0
#define WORD_I  1
#define WORD_J  2
//#define WORD_K  3
#define WORD_L  4
#define WORD_P  6
#define WORD_R  7
//#define WORD_S  8
//#define WORD_T  9
#define WORD_X  10
#define WORD_Y  11
//#define WORD_Z  12
//#define WORD_A  13
//#define WORD_B  14
//#define WORD_C  15

// Define g-code parser position updating flags
#define GC_UPDATE_POS_TARGET   0 // Must be zero
#define GC_UPDATE_POS_SYSTEM   1
#define GC_UPDATE_POS_NONE     2

// Define gcode parser flags for handling special cases.
#define GC_PARSER_NONE                  0 // Must be zero.
#define GC_PARSER_JOG_MOTION            bit(0)
#define GC_PARSER_CHECK_MANTISSA        bit(1)


// NOTE: When this struct is zeroed, the above defines set the defaults for the system.
typedef struct {
  uint8_t motion;          // {G0,G1,G2,G3,G80}
  uint8_t feed_rate;       // {G93,G94}
  uint8_t distance;        // {G90,G91}
  uint8_t program_flow;    // {M0,M1,M2,M30}
} gc_modal_t;

typedef struct {
  float f;         // Feed
  float ijk[N_AXIS];    // I,J Axis arc offsets
  uint8_t l;       // G10  parameters
  float p;         // G10 or dwell parameters
  float xyz[N_AXIS];    // X,Y Translational axes
} gc_values_t;


typedef struct {
  gc_modal_t modal;

  float feed_rate;              // Millimeters/min

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
