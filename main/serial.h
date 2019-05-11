/*
 serial.h - Header for system level commands and real-time processes
 Part of Grbl
 Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC

 */

#ifndef serial_h
#define serial_h

#include "grbl.h"

#ifndef RX_BUFFER_SIZE
#define RX_BUFFER_SIZE 128
#endif
#ifndef TX_BUFFER_SIZE
#define TX_BUFFER_SIZE 104
#endif

#define SERIAL_NO_DATA 0xff

// a task to read for incoming data from serial port
static TaskHandle_t serialCheckTaskHandle = 0;
void serialCheckTask(void *pvParameters);

void serialCheck();

void serial_write(uint8_t data);
// Fetches the first byte in the serial read buffer. Called by main program.
uint8_t serial_read(uint8_t client);

// See if the character is an action command like feedhold or jogging. If so, do the action and return true
uint8_t check_action_command(uint8_t data);

void serial_init();
void serial_reset_read_buffer(uint8_t client);

// Returns the number of bytes available in the RX serial buffer.
uint8_t serial_get_rx_buffer_available(uint8_t client);

#endif
