/*
  protocol.c - controls Grbl execution protocol and procedures
  Part of Grbl

  Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
  Copyright (c) 2009-2011 Simen Svale Skogsrud

*/

#include "grbl.h"
#include "config.h"

// Define line flags. Includes comment type tracking and line overflow detection.
#define LINE_FLAG_OVERFLOW bit(0)
#define LINE_FLAG_COMMENT_PARENTHESES bit(1)
#define LINE_FLAG_COMMENT_SEMICOLON bit(2)


static char line[LINE_BUFFER_SIZE]; // Line to be executed. Zero-terminated.

static void protocol_exec_rt_suspend();


/*
  GRBL PRIMARY LOOP:
*/
void protocol_main_loop()
{
  //uint8_t client = CLIENT_SERIAL; // default client

  // Perform some machine checks to make sure everything is good to go.
#ifdef CHECK_LIMITS_AT_INIT
  if (bit_istrue(settings.flags, BITFLAG_HARD_LIMIT_ENABLE)) {
    if (limits_get_state()) {
      sys.state = STATE_ALARM; // Ensure alarm state is active.
      report_feedback_message(MESSAGE_CHECK_LIMITS);
    }
  }
#endif
  // Check for and report alarm state after a reset, error, or an initial power up.
  // NOTE: Sleep mode disables the stepper drivers and position can't be guaranteed.
  // Re-initialize the sleep state as an ALARM mode to ensure user homes or acknowledges.
  if (sys.state & (STATE_ALARM | STATE_SLEEP)) {
    report_feedback_message(MESSAGE_ALARM_LOCK);
    sys.state = STATE_ALARM; // Ensure alarm state is set.
  } else {
    sys.state = STATE_IDLE;
    // All systems go!
    system_execute_startup(line); // Execute startup script.
  }

  // ---------------------------------------------------------------------------------
  // Primary loop! Upon a system abort, this exits back to main() to reset the system.
  // This is also where Grbl idles while waiting for something to do.
  // ---------------------------------------------------------------------------------

  uint8_t line_flags = 0;
  uint8_t char_counter = 0;
  uint8_t c;
  bool led;

  for (;;) {

    // serialCheck(); // un comment this if you do this here rather than in a separate task

    digitalWrite(LED,led);
    led = !led;

    // Process one line of incoming serial data, as the data becomes available. Performs an
    // initial filtering by removing spaces and comments and capitalizing all letters.

    uint8_t client = CLIENT_SERIAL;
    for (client = 1; client <= CLIENT_COUNT; client++)
    {
      while ((c = serial_read(client)) != SERIAL_NO_DATA) {
        if ((c == '\n') || (c == '\r')) { // End of line reached

          protocol_execute_realtime(); // Runtime command check point.
          if (sys.abort) {
            return;  // Bail to calling function upon system abort
          }

          line[char_counter] = 0; // Set string termination character.
#ifdef REPORT_ECHO_LINE_RECEIVED
          report_echo_line_received(line, client);
#endif

          // Direct and execute one line of formatted input, and report status of execution.
          if (line_flags & LINE_FLAG_OVERFLOW) {
            // Report line overflow error.
            report_status_message(STATUS_OVERFLOW, client);
          } else if (line[0] == 0) {
            // Empty or comment line. For syncing purposes.
            report_status_message(STATUS_OK, client);
          } else if (line[0] == '$') {
            // Grbl '$' system command
            report_status_message(system_execute_line(line, client), client);
          } else if (sys.state & (STATE_ALARM | STATE_JOG)) {
            // Everything else is gcode. Block if in alarm or jog mode.
            report_status_message(STATUS_SYSTEM_GC_LOCK, client);
          } else {
            // Parse and execute g-code block.
            report_status_message(gc_execute_line(line, client), client);
          }

          // Reset tracking data for next line.
          line_flags = 0;
          char_counter = 0;

        } else {

          if (line_flags) {
            // Throw away all (except EOL) comment characters and overflow characters.
            if (c == ')') {
              // End of '()' comment. Resume line allowed.
              if (line_flags & LINE_FLAG_COMMENT_PARENTHESES) {
                line_flags &= ~(LINE_FLAG_COMMENT_PARENTHESES);
              }
            }
          } else {
            if (c <= ' ') {
              // Throw away whitepace and control characters
            }
            /*
              else if (c == '/') {
            	// Block delete NOT SUPPORTED. Ignore character.
            	// NOTE: If supported, would simply need to check the system if block delete is enabled.
              }
            */
            else if (c == '(') {
              // Enable comments flag and ignore all characters until ')' or EOL.
              // NOTE: This doesn't follow the NIST definition exactly, but is good enough for now.
              // In the future, we could simply remove the items within the comments, but retain the
              // comment control characters, so that the g-code parser can error-check it.
              line_flags |= LINE_FLAG_COMMENT_PARENTHESES;
            } else if (c == ';') {
              // NOTE: ';' comment to EOL is a LinuxCNC definition. Not NIST.
              line_flags |= LINE_FLAG_COMMENT_SEMICOLON;
              // TODO: Install '%' feature
              // } else if (c == '%') {
              // Program start-end percent sign NOT SUPPORTED.
              // NOTE: This maybe installed to tell Grbl when a program is running vs manual input,
              // where, during a program, the system auto-cycle start will continue to execute
              // everything until the next '%' sign. This will help fix resuming issues with certain
              // functions that empty the planner buffer to execute its task on-time.
            } else if (char_counter >= (LINE_BUFFER_SIZE - 1)) {
              // Detect line buffer overflow and set flag.
              line_flags |= LINE_FLAG_OVERFLOW;
            } else if (c >= 'a' && c <= 'z') { // Upcase lowercase
              line[char_counter++] = c - 'a' + 'A';
            } else {
              line[char_counter++] = c;
            }
          }

        }
      } // while serial read
    } // for clients



    // If there are no more characters in the serial read buffer to be processed and executed,
    // this indicates that g-code streaming has either filled the planner buffer or has
    // completed. In either case, auto-cycle start, if enabled, any queued moves.
    protocol_auto_cycle_start();

    protocol_execute_realtime();  // Runtime command check point.
    if (sys.abort) {
      return;  // Bail to main() program loop to reset system.
    }

    // check to see if we should disable the stepper drivers ... esp32 work around for disable in main loop.
    if (stepper_idle)
    {
      if (esp_timer_get_time() > stepper_idle_counter)
      {
        set_stepper_disable(true);
      }
    }
  }

  return; /* Never reached */
}


// Block until all buffered steps are executed or in a cycle state. Works with feed hold
// during a synchronize call, if it should happen. Also, waits for clean cycle end.
void protocol_buffer_synchronize()
{
  // If system is queued, ensure cycle resumes if the auto start flag is present.
  protocol_auto_cycle_start();
  do {
    protocol_execute_realtime();   // Check and execute run-time commands
    if (sys.abort) {
      return;  // Check for system abort
    }
  } while (plan_get_current_block() || (sys.state == STATE_CYCLE));
}


// Auto-cycle start triggers when there is a motion ready to execute and if the main program is not
// actively parsing commands.
// NOTE: This function is called from the main loop, buffer sync, and mc_line() only and executes
// when one of these conditions exist respectively: There are no more blocks sent (i.e. streaming
// is finished, single commands), a command that needs to wait for the motions in the buffer to
// execute calls a buffer sync, or the planner buffer is full and ready to go.
void protocol_auto_cycle_start()
{
  if (plan_get_current_block() != NULL) { // Check if there are any blocks in the buffer.
    system_set_exec_state_flag(EXEC_CYCLE_START); // If so, execute them!
  }
}


// This function is the general interface to Grbl's real-time command execution system. It is called
// from various check points in the main program, primarily where there may be a while loop waiting
// for a buffer to clear space or any point where the execution time from the last check point may
// be more than a fraction of a second. This is a way to execute realtime commands asynchronously
// (aka multitasking) with grbl's g-code parsing and planning functions. This function also serves
// as an interface for the interrupts to set the system realtime flags, where only the main program
// handles them, removing the need to define more computationally-expensive volatile variables. This
// also provides a controlled way to execute certain tasks without having two or more instances of
// the same task, such as the planner recalculating the buffer upon a feedhold.
// NOTE: The sys_rt_exec_state variable flags are set by any process, step or serial interrupts, pinouts,
// limit switches, or the main program.
void protocol_execute_realtime()
{
  protocol_exec_rt_system();
  if (sys.suspend) {
    protocol_exec_rt_suspend();
  }
}


// Executes run-time commands, when required. This function primarily operates as Grbl's state
// machine and controls the various real-time features Grbl has to offer.
// NOTE: Do not alter this unless you know exactly what you are doing!
void protocol_exec_rt_system()
{
  uint8_t rt_exec; // Temp variable to avoid calling volatile multiple times.
  rt_exec = sys_rt_exec_alarm; // Copy volatile sys_rt_exec_alarm.
  if (rt_exec) { // Enter only if any bit flag is true
    // System alarm. Everything has shutdown by something that has gone severely wrong. Report
    // the source of the error to the user. If critical, Grbl disables by entering an infinite
    // loop until system reset/abort.
    sys.state = STATE_ALARM; // Set system alarm state
    report_alarm_message(rt_exec);
    // Halt everything upon a critical event flag. Currently hard and soft limits flag this.
    if ((rt_exec == EXEC_ALARM_HARD_LIMIT) || (rt_exec == EXEC_ALARM_SOFT_LIMIT)) {
      report_feedback_message(MESSAGE_CRITICAL_EVENT);
      system_clear_exec_state_flag(EXEC_RESET); // Disable any existing reset
      do {
        // Block everything, except reset and status reports, until user issues reset or power
        // cycles. Hard limits typically occur while unattended or not paying attention. Gives
        // the user and a GUI time to do what is needed before resetting, like killing the
        // incoming stream. The same could be said about soft limits. While the position is not
        // lost, continued streaming could cause a serious crash if by chance it gets executed.
      } while (bit_isfalse(sys_rt_exec_state, EXEC_RESET));
    }
    system_clear_exec_alarm(); // Clear alarm
  }

  rt_exec = sys_rt_exec_state; // Copy volatile sys_rt_exec_state.
  if (rt_exec) {

    // Execute system abort.
    if (rt_exec & EXEC_RESET) {
      sys.abort = true;  // Only place this is set true.
      return; // Nothing else to do but exit.
    }

    // Execute and serial print status
    if (rt_exec & EXEC_STATUS_REPORT) {
      report_realtime_status(CLIENT_ALL);
      system_clear_exec_state_flag(EXEC_STATUS_REPORT);
    }

    // NOTE: Once hold is initiated, the system immediately enters a suspend state to block all
    // main program processes until either reset or resumed. This ensures a hold completes safely.
    if (rt_exec & (EXEC_MOTION_CANCEL | EXEC_FEED_HOLD  | EXEC_SLEEP)) {

      // State check for allowable states for hold methods.
      if (!(sys.state & (STATE_ALARM | STATE_CHECK_MODE))) {

        // If in CYCLE or JOG states, immediately initiate a motion HOLD.
        if (sys.state & (STATE_CYCLE | STATE_JOG)) {
          if (!(sys.suspend & (SUSPEND_MOTION_CANCEL | SUSPEND_JOG_CANCEL))) { // Block, if already holding.
            st_update_plan_block_parameters(); // Notify stepper module to recompute for hold deceleration.
            sys.step_control = STEP_CONTROL_EXECUTE_HOLD; // Initiate suspend state with active flag.
            if (sys.state == STATE_JOG) { // Jog cancelled upon any hold event, except for sleeping.
              if (!(rt_exec & EXEC_SLEEP)) {
                sys.suspend |= SUSPEND_JOG_CANCEL;
              }
            }
          }
        }
        // If IDLE, Grbl is not in motion. Simply indicate suspend state and hold is complete.
        if (sys.state == STATE_IDLE) {
          sys.suspend = SUSPEND_HOLD_COMPLETE;
        }

        // Execute and flag a motion cancel with deceleration and return to idle. Used primarily by probing cycle
        // to halt and cancel the remainder of the motion.
        if (rt_exec & EXEC_MOTION_CANCEL) {
          // MOTION_CANCEL only occurs during a CYCLE, but a HOLD may been initiated beforehand
          // to hold the CYCLE. Motion cancel is valid for a single planner block motion only, while jog cancel
          // will handle and clear multiple planner block motions.
          if (!(sys.state & STATE_JOG)) {
            sys.suspend |= SUSPEND_MOTION_CANCEL;  // NOTE: State is STATE_CYCLE.
          }
        }

        // Execute a feed hold with deceleration, if required. Then, suspend system.
        if (rt_exec & EXEC_FEED_HOLD) {
          // Block  JOG, and SLEEP states from changing to HOLD state.
          if (!(sys.state & ( STATE_JOG | STATE_SLEEP))) {
            sys.state = STATE_HOLD;
          }
        }

      }

      if (rt_exec & EXEC_SLEEP) {
        if (sys.state == STATE_ALARM) {
          sys.suspend |= (SUSPEND_HOLD_COMPLETE);
        }
        sys.state = STATE_SLEEP;
      }

      system_clear_exec_state_flag((EXEC_MOTION_CANCEL | EXEC_FEED_HOLD | EXEC_SLEEP));
    }

    // Execute a cycle start by starting the stepper interrupt to begin executing the blocks in queue.
    if (rt_exec & EXEC_CYCLE_START) {
      // Block if called at same time as the hold commands: feed hold, motion cancel, and safety door.
      // Ensures auto-cycle-start doesn't resume a hold without an explicit user-input.
      if (!(rt_exec & (EXEC_FEED_HOLD | EXEC_MOTION_CANCEL ))) {
        // Cycle start only when IDLE or when a hold is complete and ready to resume.
        if ((sys.state == STATE_IDLE) || ((sys.state & STATE_HOLD) && (sys.suspend & SUSPEND_HOLD_COMPLETE))) {

          // Start cycle only if queued motions exist in planner buffer and the motion is not canceled.
          sys.step_control = STEP_CONTROL_NORMAL_OP; // Restore step control to normal operation
          if (plan_get_current_block() && bit_isfalse(sys.suspend, SUSPEND_MOTION_CANCEL)) {
            sys.suspend = SUSPEND_DISABLE; // Break suspend state.
            sys.state = STATE_CYCLE;
            st_prep_buffer(); // Initialize step segment buffer before beginning cycle.
            st_wake_up();
          } else { // Otherwise, do nothing. Set and resume IDLE state.
            sys.suspend = SUSPEND_DISABLE; // Break suspend state.
            sys.state = STATE_IDLE;
          }

        }
      }
      system_clear_exec_state_flag(EXEC_CYCLE_START);
    }

    if (rt_exec & EXEC_CYCLE_STOP) {
      // Reinitializes the cycle plan and stepper system after a feed hold for a resume. Called by
      // realtime command execution in the main program, ensuring that the planner re-plans safely.
      // NOTE: Bresenham algorithm variables are still maintained through both the planner and stepper
      // cycle reinitializations. The stepper path should continue exactly as if nothing has happened.
      // NOTE: EXEC_CYCLE_STOP is set by the stepper subsystem when a cycle or feed hold completes.
      if ((sys.state & (STATE_HOLD | STATE_SLEEP)) && !(sys.soft_limit) && !(sys.suspend & SUSPEND_JOG_CANCEL)) {
        // Hold complete. Set to indicate ready to resume.  Remain in HOLD or DOOR states until user
        // has issued a resume command or reset.
        plan_cycle_reinitialize();
        if (sys.step_control & STEP_CONTROL_EXECUTE_HOLD) {
          sys.suspend |= SUSPEND_HOLD_COMPLETE;
        }
        bit_false(sys.step_control, (STEP_CONTROL_EXECUTE_HOLD | STEP_CONTROL_EXECUTE_SYS_MOTION));
      } else {
        // Motion complete. Includes CYCLE/JOG/HOMING states and jog cancel/motion cancel/soft limit events.
        // NOTE: Motion and jog cancel both immediately return to idle after the hold completes.
        if (sys.suspend & SUSPEND_JOG_CANCEL) {   // For jog cancel, flush buffers and sync positions.
          sys.step_control = STEP_CONTROL_NORMAL_OP;
          plan_reset();
          st_reset();
          gc_sync_position();
          plan_sync_position();
        }
        sys.suspend = SUSPEND_DISABLE;
        sys.state = STATE_IDLE;

      }
      system_clear_exec_state_flag(EXEC_CYCLE_STOP);
    }
  }


  // Reload step segment buffer
  if (sys.state & (STATE_CYCLE | STATE_HOLD | STATE_HOMING | STATE_SLEEP | STATE_JOG)) {
    st_prep_buffer();
  }

}


// Handles Grbl system suspend procedures, such as feed hold, safety door, and parking motion.
// The system will enter this loop, create local variables for suspend tasks, and return to
// whatever function that invoked the suspend, such that Grbl resumes normal operation.
// This function is written in a way to promote custom parking motions. Simply use this as a
// template
static void protocol_exec_rt_suspend()
{

  plan_block_t *block = plan_get_current_block();
  uint8_t restore_condition;

  if (block == NULL) {
    //restore_condition = (gc_state.modal.spindle);
  }
  else {
    restore_condition = block->condition;
  }


  while (sys.suspend) {

    if (sys.abort) {
      return;
    }

    // Block until initial hold is complete and the machine has stopped motion.
    if (sys.suspend & SUSPEND_HOLD_COMPLETE) {

      if (sys.state == STATE_SLEEP) {
        report_feedback_message(MESSAGE_SLEEP_MODE);
        // Spindle should already be stopped, but do it again just to be sure.
        st_go_idle(); // Disable steppers
        while (!(sys.abort)) {
          protocol_exec_rt_system();  // Do nothing until reset.
        }
        return; // Abort received. Return to re-initialize.
      }


      system_set_exec_state_flag(EXEC_CYCLE_START); // Set to resume program.

    }
  }

  protocol_exec_rt_system();

}
