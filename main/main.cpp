#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_event_loop.h"

#include "driver/gpio.h"

#include "grbl.h"
#include "ntc.h"

// Declare system global variable structure
system_t sys;
int32_t sys_position[N_AXIS]; // Real-time machine (aka home) position vector in steps.
volatile uint8_t sys_rt_exec_state; // Global realtime executor bitflag variable for state management. See EXEC bitmasks.
volatile uint8_t sys_rt_exec_alarm; // Global realtime executor bitflag variable for setting various alarms.
volatile uint8_t sys_rt_exec_motion_override; // Global realtime executor bitflag variable for motion-based overrides.

ntc timeserver;
uint8_t plan[4];

esp_err_t event_handler(void *ctx, system_event_t *event)
{
	return ESP_OK;
}

extern "C"
{
void app_main();
}

void app_main(void)
{
//	nvs_flash_init();
//	tcpip_adapter_init();
//	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
//	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
//	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//	wifi_config_t sta_config = { .sta = { .ssid = CONFIG_ESP_WIFI_SSID,
//			.password = CONFIG_ESP_WIFI_PASSWORD, .bssid_set = false } };
//	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
//	ESP_ERROR_CHECK(esp_wifi_start());
//	ESP_ERROR_CHECK(esp_wifi_connect());

	//gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
	//int level = 0;

	serial_init();   // Setup serial baud rate and interrupts
	settings_init(); // Load Grbl settings from EEPROM
	stepper_init();  // Configure stepper pins and interrupt timers
	system_ini(); // Configure pinout pins and pin-change interrupt (Renamed due to conflict with esp32 files)
	timeserver.initLocalTime();

#ifdef ENABLE_BLUETOOTH
	char line[LINE_BUFFER_SIZE] = "Waterino";
	Serial.printf("Starting Bluetooth");
	bluetooth_init(line);
#endif

	memset(sys_position, 0, sizeof(sys_position)); // Clear machine position.

	// Initialize system state.
	sys.state = STATE_IDLE;

	// Check for power-up and set system alarm if homing is enabled to force homing cycle
	// by setting Grbl's alarm state. Alarm locks out all g-code commands, including the
	// startup scripts, but allows access to settings and internal commands. Only a homing
	// cycle '$H' or kill alarm locks '$X' will disable the alarm.
	// NOTE: The startup script will run after successful completion of the homing cycle, but
	// not after disabling the alarm locks. Prevents motion startup blocks from crashing into
	// things uncontrollably. Very bad.
#ifdef HOMING_INIT_LOCK
	if (bit_istrue(settings.flags, BITFLAG_HOMING_ENABLE))
	{
		sys.state = STATE_ALARM;
	}
#endif

	while (true)
	{
		//gpio_set_level(GPIO_NUM_4, level);
		//level = !level;
		//vTaskDelay(300 / portTICK_PERIOD_MS);

		// Reset system variables.
		uint8_t prior_state = sys.state;
		memset(&sys, 0, sizeof(system_t)); // Clear system struct variable.
		sys.state = prior_state;
		sys_rt_exec_state = 0;
		sys_rt_exec_alarm = 0;
		sys_rt_exec_motion_override = 0;

		// Reset Grbl primary systems.
		serial_reset_read_buffer(CLIENT_ALL); // Clear serial read buffer

		gc_init(); // Set g-code parser to default state

		limits_init();

		plan_reset(); // Clear block buffer and planner variables
		st_reset(); // Clear stepper subsystem variables
		// Sync cleared gcode and planner positions to current system position.
		plan_sync_position();
		gc_sync_position();

		// put your main code here, to run repeatedly:
		report_init_message(CLIENT_ALL);

		// Start Grbl main loop. Processes program inputs and executes them.
		protocol_main_loop();

	}
}

