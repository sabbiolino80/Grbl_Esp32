/*
 serial.cpp - Header for system level commands and real-time processes
 Part of Grbl
 Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC

 */

#include "grbl.h"

#define RX_RING_BUFFER (RX_BUFFER_SIZE+1)
#define TX_RING_BUFFER (TX_BUFFER_SIZE+1)

portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;

uint8_t serial_rx_buffer[CLIENT_COUNT][RX_RING_BUFFER];
uint8_t serial_rx_buffer_head[CLIENT_COUNT] = {0};
volatile uint8_t serial_rx_buffer_tail[CLIENT_COUNT] = {0};


// Returns the number of bytes available in the RX serial buffer.
uint8_t serial_get_rx_buffer_available(uint8_t client)
{
	uint8_t client_idx = client - 1;

	uint8_t rtail = serial_rx_buffer_tail[client_idx]; // Copy to limit multiple calls to volatile
	if (serial_rx_buffer_head[client_idx] >= rtail)
	{
		return (RX_BUFFER_SIZE - (serial_rx_buffer_head[client_idx] - rtail));
	}
	return ((rtail - serial_rx_buffer_head[client_idx] - 1));
}

void serial_init()
{
	Serial.begin(BAUD_RATE);

	// create a task to check for incoming data
	xTaskCreatePinnedToCore(serialCheckTask,    // task
			"servoSyncTask", // name for task
			2048,   // size of task stack
			NULL,   // parameters
			1, // priority
                                &serialCheckTaskHandle,
                                0 // core
			);

}

// this task runs and checks for data on all interfaces
// REaltime stuff is acted upon, then characters are added to the appropriate buffer
void serialCheckTask(void *pvParameters)
{
	uint8_t data;
	uint8_t next_head;
	uint8_t client; // who send the data

	uint8_t client_idx = 0;  // index of data buffer

	while (true) // run continuously
	{
		while (Serial.available()
#ifdef ENABLE_BLUETOOTH
				|| (SerialBT.hasClient() && SerialBT.available())
#endif
		)
		{
			client = CLIENT_SERIAL;
			data = 0;
			if (Serial.available())
			{
				client = CLIENT_SERIAL;
				data = Serial.read();
			}
			else
            {
                //currently is wifi or BT but better to prepare both can be live
#ifdef ENABLE_BLUETOOTH
				if (SerialBT.hasClient() && SerialBT.available())
				{
					client = CLIENT_BT;
					data = SerialBT.read();
					//Serial.write(data);  // echo all data to serial
				}
#else
      return;
#endif
			}

			client_idx = client - 1;  // for zero based array

			// Pick off realtime command characters directly from the serial stream. These characters are
			// not passed into the main buffer, but these set system state flag bits for realtime execution.
			switch (data)
			{
			case CMD_RESET:
				mc_reset();   // Call motion control reset routine.
				//report_init_message(client); // fool senders into thinking a reset happened.
				break;
			case CMD_STATUS_REPORT:
				report_realtime_status(client);
				break; // direct call instead of setting flag
			case CMD_CYCLE_START:
				system_set_exec_state_flag(EXEC_CYCLE_START);
				break; // Set as true
			case CMD_FEED_HOLD:
				system_set_exec_state_flag(EXEC_FEED_HOLD);
				break; // Set as true
			default:
                    if (data > 0x7F)   // Real-time control characters are extended ACSII only.
                    {
					switch (data)
					{
					case CMD_JOG_CANCEL:
                                if (sys.state & STATE_JOG)   // Block all other states from invoking motion cancel.
                                {
							system_set_exec_state_flag(EXEC_MOTION_CANCEL);
						}
						break;

					}
					// Throw away any unfound extended-ASCII character by not passing it to the serial buffer.
				}
                    else     // Write character to buffer
                    {

					vTaskEnterCritical(&myMutex);
					next_head = serial_rx_buffer_head[client_idx] + 1;
					if (next_head == RX_RING_BUFFER)
					{
						next_head = 0;
					}

					// Write data to buffer unless it is full.
					if (next_head != serial_rx_buffer_tail[client_idx])
					{
						serial_rx_buffer[client_idx][serial_rx_buffer_head[client_idx]] = data;
						serial_rx_buffer_head[client_idx] = next_head;
					}
					vTaskExitCritical(&myMutex);
				}
			}  // switch data
		}  // if something available
		vTaskDelay(1 / portTICK_RATE_MS);  // Yield to other tasks
	}  // while(true)
}

// ==================== call this in main protocol loop if you want it in the main task =========
// be sure to stop task.
// Realtime stuff is acted upon, then characters are added to the appropriate buffer
void serialCheck()
{
	uint8_t data;
	uint8_t next_head;
	uint8_t client; // who send the data

	uint8_t client_idx = 0;  // index of data buffer

	while (Serial.available()
#ifdef ENABLE_BLUETOOTH
			|| (SerialBT.hasClient() && SerialBT.available())
#endif
	)
	{
		client = CLIENT_SERIAL;
		data = 0;
		if (Serial.available())
		{
			client = CLIENT_SERIAL;
			data = Serial.read();
		}
		else
        {
            //currently is wifi or BT but better to prepare both can be live
#ifdef ENABLE_BLUETOOTH
			if (SerialBT.hasClient() && SerialBT.available())
			{
				client = CLIENT_BT;
				data = SerialBT.read();
			}
#else
      return;
#endif
		}

		client_idx = client - 1;  // for zero based array

		// Pick off realtime command characters directly from the serial stream. These characters are
		// not passed into the main buffer, but these set system state flag bits for realtime execution.
		switch (data)
		{
		case CMD_RESET:
			mc_reset();
			break; // Call motion control reset routine.
		case CMD_STATUS_REPORT:
			report_realtime_status(client);
			break; // direct call instead of setting flag
		case CMD_CYCLE_START:
			system_set_exec_state_flag(EXEC_CYCLE_START);
			break; // Set as true
		case CMD_FEED_HOLD:
			system_set_exec_state_flag(EXEC_FEED_HOLD);
			break; // Set as true
		default:
                if (data > 0x7F)   // Real-time control characters are extended ACSII only.
                {
				switch (data)
				{
				case CMD_JOG_CANCEL:
                            if (sys.state & STATE_JOG)   // Block all other states from invoking motion cancel.
                            {
						system_set_exec_state_flag(EXEC_MOTION_CANCEL);
					}
					break;
				}
				// Throw away any unfound extended-ASCII character by not passing it to the serial buffer.
			}
                else     // Write character to buffer
                {


				next_head = serial_rx_buffer_head[client_idx] + 1;
				if (next_head == RX_RING_BUFFER)
				{
					next_head = 0;
				}

				// Write data to buffer unless it is full.
				if (next_head != serial_rx_buffer_tail[client_idx])
				{
					serial_rx_buffer[client_idx][serial_rx_buffer_head[client_idx]] = data;
					serial_rx_buffer_head[client_idx] = next_head;
				}
			}
		}  // switch data
	}  // if something available
}

void serial_reset_read_buffer(uint8_t client)
{
	for (uint8_t client_num = 1; client_num <= CLIENT_COUNT; client_num++)
	{
		if (client == client_num || client == CLIENT_ALL)
		{
			serial_rx_buffer_tail[client_num - 1] = serial_rx_buffer_head[client_num - 1];
		}
	}
}

// Writes one byte to the TX serial buffer. Called by main program.
void serial_write(uint8_t data)
{
	Serial.write((char) data);
}
// Fetches the first byte in the serial read buffer. Called by main program.
uint8_t serial_read(uint8_t client)
{
	uint8_t client_idx = client - 1;

	uint8_t tail = serial_rx_buffer_tail[client_idx]; // Temporary serial_rx_buffer_tail (to optimize for volatile)
	if (serial_rx_buffer_head[client_idx] == tail)
	{
		return SERIAL_NO_DATA;
	}
	else
	{
		vTaskEnterCritical(&myMutex); // make sure buffer is not modified while reading by newly read chars from the serial when we are here
		uint8_t data = serial_rx_buffer[client_idx][tail];

		tail++;
		if (tail == RX_RING_BUFFER)
		{
			tail = 0;
		}
		serial_rx_buffer_tail[client_idx] = tail;
		vTaskExitCritical(&myMutex);
		return data;
	}
}
