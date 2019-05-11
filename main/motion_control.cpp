/*
 motion_control.c - high level interface for issuing motion commands
 Part of Grbl

 Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
 Copyright (c) 2009-2011 Simen Svale Skogsrud

 */

#include "grbl.h"

// Execute linear motion in absolute millimeter coordinates. Feed rate given in millimeters/second
// unless invert_feed_rate is true. Then the feed_rate means that the motion should be completed in
// (1 minute)/feed_rate time.
// NOTE: This is the primary gateway to the grbl planner. All line motions
//  must pass through this routine before being passed to the planner. The seperation of
// mc_line and plan_buffer_line is done primarily to place non-planner-type functions from being
// in the planner and to let backlash compensation or canned cycle integration simple and direct.
void mc_line(float *target, plan_line_data_t *pl_data)
{
	// If enabled, check for soft limit violations. Placed here all line motions are picked up
	// from everywhere in Grbl.
	if (bit_istrue(settings.flags, BITFLAG_SOFT_LIMIT_ENABLE))
	{
		// NOTE: Block jog state. Jogging is a special case and soft limits are handled independently.
		if (sys.state != STATE_JOG)
		{
			limits_soft_check(target);
		}
	}

	// If in check gcode mode, prevent motion by blocking planner. Soft limits still work.
	if (sys.state == STATE_CHECK_MODE)
	{
		return;
	}

	// NOTE: Backlash compensation may be installed here. It will need direction info to track when
	// to insert a backlash line motion(s) before the intended line motion and will require its own
	// plan_check_full_buffer() and check for system abort loop. Also for position reporting
	// backlash steps will need to be also tracked, which will need to be kept at a system level.
	// There are likely some other things that will need to be tracked as well. However, we feel
	// that backlash compensation should NOT be handled by Grbl itself, because there are a myriad
	// of ways to implement it and can be effective or ineffective for different CNC machines. This
	// would be better handled by the interface as a post-processor task, where the original g-code
	// is translated and inserts backlash motions that best suits the machine.
	// NOTE: Perhaps as a middle-ground, all that needs to be sent is a flag or special command that
	// indicates to Grbl what is a backlash compensation motion, so that Grbl executes the move but
	// doesn't update the machine position values. Since the position values used by the g-code
	// parser and planner are separate from the system machine positions, this is doable.

	// If the buffer is full: good! That means we are well ahead of the robot.
	// Remain in this loop until there is room in the buffer.
	do
	{
		protocol_execute_realtime(); // Check for any run-time commands
		if (sys.abort)
		{
			return;  // Bail, if system abort.
		}
		if (plan_check_full_buffer())
		{
			protocol_auto_cycle_start();  // Auto-cycle start when buffer is full.
		}
		else
		{
			break;
		}
	} while (1);

	// Plan and queue motion into planner buffer
	// uint8_t plan_status; // Not used in normal operation.
	plan_buffer_line(target, pl_data);
}

// Execute dwell in seconds.
void mc_dwell(float seconds)
{
	if (sys.state == STATE_CHECK_MODE)
	{
		return;
	}
	protocol_buffer_synchronize();
	delay_sec(seconds, DELAY_MODE_DWELL);
}

// Perform homing cycle to locate and set machine zero. Only '$H' executes this command.
// NOTE: There should be no motions in the buffer and Grbl must be in an idle state before
// executing the homing cycle. This prevents incorrect buffered plans after homing.
void mc_homing_cycle(uint8_t cycle_mask)
{
	// TODO: Move the pin-specific LIMIT_PIN call to limits.c as a function.
	limits_disable(); // Disable hard limits pin change register for cycle duration

	// -------------------------------------------------------------------------------------
	// Perform homing routine. NOTE: Special motion case. Only system reset works.

#ifdef HOMING_SINGLE_AXIS_COMMANDS
    if (cycle_mask)
    {
    limits_go_home(cycle_mask);  // Perform homing cycle based on mask.
  }
  else
#endif
	{
		// Search to engage all axes limit switches at faster homing seek rate.
		limits_go_home(HOMING_CYCLE_0);  // Homing cycle 0
#ifdef HOMING_CYCLE_1
		limits_go_home(HOMING_CYCLE_1);  // Homing cycle 1
#endif
#ifdef HOMING_CYCLE_2
    limits_go_home(HOMING_CYCLE_2);  // Homing cycle 2
#endif
	}

	protocol_execute_realtime(); // Check for reset and set system abort.
	if (sys.abort)
	{
		return;  // Did not complete. Alarm state set by mc_alarm.
	}

	// Homing cycle complete! Setup system for normal operation.
	// -------------------------------------------------------------------------------------

	// Sync gcode parser and planner positions to homed position.
	gc_sync_position();
	plan_sync_position();

	// If hard limits feature enabled, re-enable hard limits pin change register after homing cycle.
	limits_init();
}

// Method to ready the system to reset by setting the realtime reset command and killing any
// active processes in the system. This also checks if a system reset is issued while Grbl
// is in a motion state. If so, kills the steppers and sets the system alarm to flag position
// lost, since there was an abrupt uncontrolled deceleration. Called at an interrupt level by
// realtime abort command and hard limits. So, keep to a minimum.
void mc_reset()
{
	// Only this function can set the system reset. Helps prevent multiple kill calls.
	if (bit_isfalse(sys_rt_exec_state, EXEC_RESET))
	{
		system_set_exec_state_flag(EXEC_RESET);

		// Kill steppers only if in any motion state, i.e. cycle, actively holding, or homing.
		// NOTE: If steppers are kept enabled via the step idle delay setting, this also keeps
		// the steppers enabled by avoiding the go_idle call altogether, unless the motion state is
		// violated, by which, all bets are off.
		if ((sys.state & (STATE_CYCLE | STATE_HOMING | STATE_JOG)) || (sys.step_control & (STEP_CONTROL_EXECUTE_HOLD | STEP_CONTROL_EXECUTE_SYS_MOTION)))
		{
			if (sys.state == STATE_HOMING)
			{
				if (!sys_rt_exec_alarm)
				{
					system_set_exec_alarm(EXEC_ALARM_HOMING_FAIL_RESET);
				}
			}
			else
			{
				system_set_exec_alarm(EXEC_ALARM_ABORT_CYCLE);
			}
			st_go_idle(); // Force kill steppers. Position has likely been lost.
		}
	}
}
