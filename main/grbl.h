/*
 grbl.h - Header for system level commands and real-time processes
 Part of Grbl
 Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC

 */

// Grbl versioning system
#define GRBL_VERSION "1.1f"
#define GRBL_VERSION_BUILD "20180919"

#include <sdkconfig.h>
//TODO #include <Arduino.h>
//TODO #include <EEPROM.h>
#include "arduino/EEPROM/src/EEPROM.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include <driver/rmt.h>
#include <esp_task_wdt.h>

#include "driver/timer.h"

#include "esp_types.h"
#include <stdint.h>
#include "freertos/queue.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include <rom/ets_sys.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h> // PSoc Required for labs

// Define the Grbl system include files. NOTE: Do not alter organization.
#include "config.h"
#include "cpu_map.h"
//#include "grbl.h"
#include "nuts_bolts.h"
#include "defaults.h"
#include "settings.h"
#include "system.h"

#include "planner.h"

#include "eeprom.h"
#include "gcode.h"
#include "limits.h"
#include "motion_control.h"
#include "print.h"
#include "protocol.h"
#include "report.h"
#include "serial.h"
#include "stepper.h"
#include "jog.h"

#ifdef ENABLE_BLUETOOTH
#include "grbl_bluetooth.h"
#include "arduino/BluetoothSerial.h"
#endif

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
