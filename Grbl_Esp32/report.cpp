/*
    report.c - reporting and messaging methods
    Part of Grbl

    Copyright (c) 2012-2016 Sungeun K. Jeon for Gnea Research LLC

*/

/*
    This file functions as the primary feedback interface for Grbl. Any outgoing data, such
    as the protocol status messages, feedback messages, and status reports, are stored here.
    For the most part, these functions primarily are called from protocol.c methods. If a
    different style feedback is desired (i.e. JSON), then a user can change these following
    methods to accommodate their needs.


	ESP32 Notes:

	Major rewrite to fix issues with BlueTooth. As described here there is a
	when you try to send data a single byte at a time using SerialBT.write(...).
	https://github.com/espressif/arduino-esp32/issues/1537

	A solution is to send messages as a string using SerialBT.print(...). Use
	a short delay after each send. Therefore this file needed to be rewritten
	to work that way. AVR Grbl was written to be super efficient to give it
	good performance. This is far less efficient, but the ESP32 can handle it.
	Do not use this version of the file with AVR Grbl.

	ESP32 discussion here ...  https://github.com/bdring/Grbl_Esp32/issues/3


*/

#include "grbl.h"


// this is a generic send function that everything should use, so interfaces could be added (Bluetooth, etc)
void grbl_send(uint8_t client, char *text)
{
#ifdef ENABLE_BLUETOOTH
    if (SerialBT.hasClient() && ( client == CLIENT_BT || client == CLIENT_ALL ) )
    {

        SerialBT.print(text);
        //delay(10); // possible fix for dropped characters
    }
#endif
    if ( client == CLIENT_SERIAL || client == CLIENT_ALL )
        Serial.print(text);
}

// This is a formating version of the grbl_send(CLIENT_ALL,...) function that work like printf
void grbl_sendf(uint8_t client, const char *format, ...)
{
    char loc_buf[64];
    char * temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    size_t len = vsnprintf(NULL, 0, format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf))
    {
        temp = new char[len + 1];
        if (temp == NULL)
        {
            return;
        }
    }
    len = vsnprintf(temp, len + 1, format, arg);
    grbl_send(client, temp);
    va_end(arg);
    if (len > 64)
    {
        delete[] temp;
    }
}

// formats axis values into a string and returns that string in rpt
static void report_util_axis_values(float *axis_value, char *rpt)
{
    uint8_t idx;
    char axisVal[10];

    rpt[0] = '\0';

    for (idx = 0; idx < N_AXIS; idx++)
    {
        sprintf(axisVal, "%4.3f", axis_value[idx]);
        strcat(rpt, axisVal);

        if (idx < (N_AXIS - 1))
        {
            strcat(rpt, ",");
        }
    }
}


void get_state(char *foo)
{
    // pad them to same length
    switch (sys.state)
    {
        case STATE_IDLE:
		strcpy(foo, " Idle ");
		;
            break;
        case STATE_CYCLE:
            strcpy(foo, " Run  ");
            break;
        case STATE_HOLD:
            strcpy(foo, " Hold ");
            break;
        case STATE_HOMING:
            strcpy(foo, " Home ");
            break;
        case STATE_ALARM:
            strcpy(foo, " Alarm");
            break;
        case STATE_CHECK_MODE:
            strcpy(foo, " Check");
            break;
        default:
            strcpy(foo, "  ?  ");
            break;
    }
}

// Handles the primary confirmation protocol response for streaming interfaces and human-feedback.
// For every incoming line, this method responds with an 'ok' for a successful command or an
// 'error:'  to indicate some error event with the line or some critical system error during
// operation. Errors events can originate from the g-code parser, settings module, or asynchronously
// from a critical error, such as a triggered hard limit. Interface should always monitor for these
// responses.
void report_status_message(uint8_t status_code, uint8_t client)
{
    switch (status_code)
    {
        case STATUS_OK: // STATUS_OK
            grbl_send(client, "ok\r\n");
            break;
        default:
            grbl_sendf(client, "error:%d\r\n", status_code);
    }
}



// Prints alarm messages.
void report_alarm_message(uint8_t alarm_code)
{
    grbl_sendf(CLIENT_ALL, "ALARM:%d\r\n", alarm_code);		// OK to send to all clients
    delay_ms(500); // Force delay to ensure message clears serial write buffer.
}

// Prints feedback messages. This serves as a centralized method to provide additional
// user feedback for things that are not of the status/alarm message protocol. These are
// messages such as setup warnings, switch toggling, and how to exit alarms.
// NOTE: For interfaces, messages are always placed within brackets. And if silent mode
// is installed, the message number codes are less than zero.
void report_feedback_message(uint8_t message_code)  // OK to send to all clients
{
    switch (message_code)
    {
        case MESSAGE_CRITICAL_EVENT:
            grbl_send(CLIENT_ALL, "[MSG:Reset to continue]\r\n");
            break;
        case MESSAGE_ALARM_LOCK:
            grbl_send(CLIENT_ALL, "[MSG:'$H'|'$X' to unlock]\r\n");
            break;
        case MESSAGE_ALARM_UNLOCK:
            grbl_send(CLIENT_ALL, "[MSG:Caution: Unlocked]\r\n");
            break;
        case MESSAGE_ENABLED:
            grbl_send(CLIENT_ALL, "[MSG:Enabled]\r\n");
            break;
        case MESSAGE_DISABLED:
            grbl_send(CLIENT_ALL, "[MSG:Disabled]\r\n");
            break;
        case MESSAGE_CHECK_LIMITS:
            grbl_send(CLIENT_ALL, "[MSG:Check Limits]\r\n");
            break;
        case MESSAGE_PROGRAM_END:
            grbl_send(CLIENT_ALL, "[MSG:Pgm End]\r\n");
            break;
        case MESSAGE_RESTORE_DEFAULTS:
            grbl_send(CLIENT_ALL, "[MSG:Restoring defaults]\r\n");
            break;
        case MESSAGE_SLEEP_MODE:
            grbl_send(CLIENT_ALL, "[MSG:Sleeping]\r\n");
            break;
    }
}


// Welcome message
void report_init_message(uint8_t client)
{
    grbl_send(client, "\r\nGrbl " GRBL_VERSION " ['$' for help]\r\n");
}

// Grbl help message
void report_grbl_help(uint8_t client)
{
    grbl_send(client, "[HLP:$$ $# $G $I $N $x=val $Nx=line $J=line $SLP $C $X $H $F ~ ! ? ctrl-x]\r\n");
}


// Grbl global settings print out.
// NOTE: The numbering scheme here must correlate to storing in settings.c
void report_grbl_settings(uint8_t client)
{
    // Print Grbl settings.
    char setting[20];
    char rpt[800];

    rpt[0] = '\0';

    sprintf(setting, "$0=%d\r\n", settings.pulse_microseconds);
    strcat(rpt, setting);
    sprintf(setting, "$1=%d\r\n", settings.stepper_idle_lock_time);
    strcat(rpt, setting);
    sprintf(setting, "$2=%d\r\n", settings.step_invert_mask);
    strcat(rpt, setting);
    sprintf(setting, "$3=%d\r\n", settings.dir_invert_mask);
    strcat(rpt, setting);
    sprintf(setting, "$4=%d\r\n", bit_istrue(settings.flags, BITFLAG_INVERT_ST_ENABLE));
    strcat(rpt, setting);
    sprintf(setting, "$5=%d\r\n", bit_istrue(settings.flags, BITFLAG_INVERT_LIMIT_PINS));
    strcat(rpt, setting);
    sprintf(setting, "$10=%d\r\n", settings.status_report_mask);
    strcat(rpt, setting);

    sprintf(setting, "$11=%4.3f\r\n", settings.junction_deviation);
    strcat(rpt, setting);

    sprintf(setting, "$20=%d\r\n", bit_istrue(settings.flags, BITFLAG_SOFT_LIMIT_ENABLE));
    strcat(rpt, setting);
    sprintf(setting, "$21=%d\r\n", bit_istrue(settings.flags, BITFLAG_HARD_LIMIT_ENABLE));
    strcat(rpt, setting);
    sprintf(setting, "$22=%d\r\n", bit_istrue(settings.flags, BITFLAG_HOMING_ENABLE));
    strcat(rpt, setting);
    sprintf(setting, "$23=%d\r\n", settings.homing_dir_mask);
    strcat(rpt, setting);

    sprintf(setting, "$24=%4.3f\r\n", settings.homing_feed_rate);
    strcat(rpt, setting);
    sprintf(setting, "$25=%4.3f\r\n", settings.homing_seek_rate);
    strcat(rpt, setting);

    sprintf(setting, "$26=%d\r\n", settings.homing_debounce_delay);
    strcat(rpt, setting);

    sprintf(setting, "$27=%4.3f\r\n", settings.homing_pulloff);
    strcat(rpt, setting);




    // Print axis settings
    uint8_t idx, set_idx;
    uint8_t val = AXIS_SETTINGS_START_VAL;
    for (set_idx = 0; set_idx < AXIS_N_SETTINGS; set_idx++)
    {
        for (idx = 0; idx < N_AXIS; idx++)
        {
            switch (set_idx)
            {
                case 0:
                    sprintf(setting, "$%d=%4.3f\r\n", val + idx, settings.steps_per_mm[idx]);
                    strcat(rpt, setting);
                    break;
                case 1:
                    sprintf(setting, "$%d=%4.3f\r\n", val + idx, settings.max_rate[idx]);
                    strcat(rpt, setting);
                    break;
                case 2:
                    sprintf(setting, "$%d=%4.3f\r\n", val + idx, settings.acceleration[idx] / (60 * 60));
                    strcat(rpt, setting);
                    break;
                case 3:
                    sprintf(setting, "$%d=%4.3f\r\n", val + idx, -settings.max_travel[idx]);
                    strcat(rpt, setting);
                    break;
            }
        }
        val += AXIS_SETTINGS_INCREMENT;
    }

    grbl_send(client, rpt);
}





// Prints Grbl NGC parameters (coordinate offsets, probing)
void report_ngc_parameters(uint8_t client)
{
    char temp[50];
    char ngc_rpt[400];

    ngc_rpt[0] = '\0';

    strcat(ngc_rpt, "[G92:"); // Print G92,G92.1 which are not persistent in memory
    report_util_axis_values(gc_state.coord_offset, temp);
    strcat(ngc_rpt, temp);
    strcat(ngc_rpt, "]\r\n");

    grbl_send(client, ngc_rpt);

}



// Print current gcode parser mode state
void report_gcode_modes(uint8_t client)
{
    char temp[20];
    char modes_rpt[75];


    strcpy(modes_rpt, "[GC:G");



    sprintf(temp, "%d", gc_state.modal.motion);
    strcat(modes_rpt, temp);

    sprintf(temp, " G%d", gc_state.modal.distance + 90);
    strcat(modes_rpt, temp);

    sprintf(temp, " G%d", 94 - gc_state.modal.feed_rate);
    strcat(modes_rpt, temp);


    if (gc_state.modal.program_flow)
    {
        //report_util_gcode_modes_M();
        switch (gc_state.modal.program_flow)
        {
            case PROGRAM_FLOW_PAUSED :
                strcat(modes_rpt, " M0"); //serial_write('0'); break;
            // case PROGRAM_FLOW_OPTIONAL_STOP : serial_write('1'); break; // M1 is ignored and not supported.
            case PROGRAM_FLOW_COMPLETED_M2 :
            case PROGRAM_FLOW_COMPLETED_M30 :
                sprintf(temp, " M%d", gc_state.modal.program_flow);
                strcat(modes_rpt, temp);
                break;
        }
    }


    sprintf(temp, " F%4.3f", gc_state.feed_rate);
    strcat(modes_rpt, temp);


    strcat(modes_rpt, "]\r\n");

    grbl_send(client, modes_rpt);
}



// Prints specified startup line
void report_startup_line(uint8_t n, char *line, uint8_t client)
{
    grbl_sendf(client, "$N%d=%s\r\n", n, line);	// OK to send to all
}

void report_execute_startup_message(char *line, uint8_t status_code, uint8_t client)
{
    grbl_sendf(client, ">%s:", line);  	// OK to send to all
    report_status_message(status_code, client);
}

// Prints build info line
void report_build_info(char *line, uint8_t client)
{
    char build_info[50];

    strcpy(build_info, "[VER:" GRBL_VERSION "." GRBL_VERSION_BUILD ":");
    strcat(build_info, line);
    strcat(build_info, "]\r\n[OPT:");

#ifdef HOMING_FORCE_SET_ORIGIN
    strcat(build_info, "Z");
#endif
#ifdef HOMING_SINGLE_AXIS_COMMANDS
    strcat(build_info, "H");
#endif
#ifdef ENABLE_BLUETOOTH
    strcat(build_info, "B");
#endif
#ifndef ENABLE_RESTORE_EEPROM_WIPE_ALL // NOTE: Shown when disabled.
    strcat(build_info, "*");
#endif
#ifndef ENABLE_RESTORE_EEPROM_DEFAULT_SETTINGS // NOTE: Shown when disabled. 
    strcat(build_info, "$");
#endif
#ifndef ENABLE_RESTORE_EEPROM_CLEAR_PARAMETERS // NOTE: Shown when disabled.
    strcat(build_info, "#");
#endif
#ifndef ENABLE_BUILD_INFO_WRITE_COMMAND // NOTE: Shown when disabled.
    strcat(build_info, "I");
#endif
#ifndef FORCE_BUFFER_SYNC_DURING_EEPROM_WRITE // NOTE: Shown when disabled.
    strcat(build_info, "E");
#endif
#ifndef FORCE_BUFFER_SYNC_DURING_WCO_CHANGE // NOTE: Shown when disabled.
    strcat(build_info, "W");
#endif
    // NOTE: Compiled values, like override increments/max/min values, may be added at some point later.
    // These will likely have a comma delimiter to separate them.

    strcat(build_info, "]\r\n");
    grbl_send(client, build_info); // ok to send to all
}




// Prints the character string line Grbl has received from the user, which has been pre-parsed,
// and has been sent into protocol_execute_line() routine to be executed by Grbl.
void report_echo_line_received(char *line, uint8_t client)
{
    grbl_sendf(client, "[echo: %s]\r\n", line);
}

// Prints real-time data. This function grabs a real-time snapshot of the stepper subprogram
// and the actual location of the CNC machine. Users may change the following function to their
// specific needs, but the desired real-time data report must be as short as possible. This is
// requires as it minimizes the computational overhead and allows grbl to keep running smoothly,
// especially during g-code programs with fast, short line segments and high frequency reports (5-20Hz).
void report_realtime_status(uint8_t client)
{
    uint8_t idx;
    int32_t current_position[N_AXIS]; // Copy current state of the system position variable
    memcpy(current_position, sys_position, sizeof(sys_position));
    float print_position[N_AXIS];

    char status[200];
    char temp[50];

    system_convert_array_steps_to_mpos(print_position, current_position);

    // Report current machine state and sub-states
    strcpy(status, "<");
    switch (sys.state)
    {
        case STATE_IDLE:
            strcat(status, "Idle");
            break;
        case STATE_CYCLE:
            strcat(status, "Run");
            break;
        case STATE_HOLD:

            if (!(sys.suspend & SUSPEND_JOG_CANCEL))
            {
                strcat(status, "Hold:");
                if (sys.suspend & SUSPEND_HOLD_COMPLETE)
                {
                    strcat(status, "0");  // Ready to resume
                }
                else
                {
                    strcat(status, "1");  // Actively holding
                }
                break;
            } // Continues to print jog state during jog cancel.
        case STATE_JOG:
            strcat(status, "Jog");
            break;
        case STATE_HOMING:
            strcat(status, "Home");
            break;
        case STATE_ALARM:
            strcat(status, "Alarm");
            break;
        case STATE_CHECK_MODE:
            strcat(status, "Check");
            break;
        case STATE_SLEEP:
            strcat(status, "Sleep");
            break;
    }

    float wco[N_AXIS];
	if (bit_isfalse(settings.status_report_mask, BITFLAG_RT_STATUS_POSITION_TYPE) || (sys.report_wco_counter == 0))
    {
        for (idx = 0; idx < N_AXIS; idx++)
        {
            // Apply work coordinate offsets and tool length offset to current position.
            wco[idx] = gc_state.coord_system[idx] + gc_state.coord_offset[idx];
            if (bit_isfalse(settings.status_report_mask, BITFLAG_RT_STATUS_POSITION_TYPE))
            {
                print_position[idx] -= wco[idx];
            }
        }
    }

    // Report machine position
    if (bit_istrue(settings.status_report_mask, BITFLAG_RT_STATUS_POSITION_TYPE))
    {
        strcat(status, "|MPos:");
    }
    else
    {
        strcat(status, "|WPos:");
    }
    report_util_axis_values(print_position, temp);
    strcat(status, temp);

    // Returns planner and serial read buffer states.
#ifdef REPORT_FIELD_BUFFER_STATE
    if (bit_istrue(settings.status_report_mask, BITFLAG_RT_STATUS_BUFFER_STATE))
    {
        sprintf(temp, "|Bf:%d,%d", plan_get_block_buffer_available(), serial_get_rx_buffer_available(CLIENT_SERIAL));
        strcat(status, temp);
    }
#endif

    // Report realtime feed speed
#ifdef REPORT_FIELD_CURRENT_FEED_SPEED

    sprintf(temp, "|F:%4.3f", st_get_realtime_rate());
    strcat(status, temp);
#endif

#ifdef REPORT_FIELD_PIN_STATE
    uint8_t lim_pin_state = limits_get_state();

    if (lim_pin_state | ctrl_pin_state)
    {
        strcat(status, "|Pn:");
        if (lim_pin_state)
        {
            if (bit_istrue(lim_pin_state, bit(X_AXIS)))
            {
                strcat(status, "X");
            }
            if (bit_istrue(lim_pin_state, bit(Y_AXIS)))
            {
                strcat(status, "Y");
            }
        }
    }
#endif



    strcat(status, ">\r\n");

    grbl_send(client, status);
}
