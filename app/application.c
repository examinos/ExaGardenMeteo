#include <application.h>

// LED instance
bc_led_t led;

uint64_t my_device_address;
uint64_t peer_device_address;

// Button instance
bc_button_t button;
bc_button_t button_5s;

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);

        static uint16_t event_count = 0;

        bc_radio_pub_push_button(&event_count);

        event_count++;
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        static bool base = false;
        if (base)
            bc_radio_enrollment_start();
        else
            bc_radio_enroll_to_gateway();
        base = !base;

        bc_led_set_mode(&led, BC_LED_MODE_BLINK_FAST);
    }
}

static void button_5s_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
	(void) self;
	(void) event_param;

	if (event == BC_BUTTON_EVENT_HOLD)
	{
		bc_radio_enrollment_stop();

		uint64_t devices_address[BC_RADIO_MAX_DEVICES];

		// Read all remote address
		bc_radio_get_peer_devices_address(devices_address, BC_RADIO_MAX_DEVICES);

		for (int i = 0; i < BC_RADIO_MAX_DEVICES; i++)
		{
			if (devices_address[i] != 0)
			{
				// Remove device
				bc_radio_peer_device_remove(devices_address[i]);
			}
		}

		bc_led_pulse(&led, 2000);
	}
}

void radio_event_handler(bc_radio_event_t event, void *event_param)
{
    (void) event_param;

    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    peer_device_address = bc_radio_get_event_device_address();

    if (event == BC_RADIO_EVENT_ATTACH)
    {
        bc_led_pulse(&led, 1000);
    }
    else if (event == BC_RADIO_EVENT_DETACH)
	{
		bc_led_pulse(&led, 3000);
	}
    else if (event == BC_RADIO_EVENT_ATTACH_FAILURE)
    {
        bc_led_pulse(&led, 10000);
    }

    else if (event == BC_RADIO_EVENT_INIT_DONE)
    {
        my_device_address = bc_radio_get_device_address();
    }
}

void bc_radio_on_push_button(uint64_t *peer_device_address, uint16_t *event_count)
{
	(void) peer_device_address;
	(void) event_count;

    bc_led_pulse(&led, 100);
}






static void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param)
{
    (void) event_param;
    float value;
    static uint8_t i2c_thermometer = 0x49;

    if (event == BC_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        if (bc_tag_temperature_get_temperature_celsius(self, &value))
        {
            bc_radio_pub_thermometer(i2c_thermometer, &value);
        }
    }
}

void climate_event_event_handler(bc_module_climate_event_t event, void *event_param)
{
    (void) event_param;
    float value;
    float altitude;

    static uint8_t i2c_thermometer = 0x48;
    static uint8_t i2c_humidity = 0x40;
    static uint8_t i2c_lux_meter = 0x44;
    static uint8_t i2c_barometer = 0x60;

    bc_led_pulse(&led, 10);

    switch (event)
    {
        case BC_MODULE_CLIMATE_EVENT_UPDATE_THERMOMETER:
            if (bc_module_climate_get_temperature_celsius(&value))
            {
                bc_radio_pub_thermometer(i2c_thermometer, &value);
            }
            break;

        case BC_MODULE_CLIMATE_EVENT_UPDATE_HYGROMETER:
            if (bc_module_climate_get_humidity_percentage(&value))
            {
                bc_radio_pub_humidity(i2c_humidity, &value);
            }
            break;

        case BC_MODULE_CLIMATE_EVENT_UPDATE_LUX_METER:
            if (bc_module_climate_get_illuminance_lux(&value))
            {
                bc_radio_pub_luminosity(i2c_lux_meter, &value);
            }
            break;

        case BC_MODULE_CLIMATE_EVENT_UPDATE_BAROMETER:
            if ((bc_module_climate_get_pressure_pascal(&value)) && (bc_module_climate_get_altitude_meter(&altitude)))
            {
                bc_radio_pub_barometer(i2c_barometer, &value, &altitude);
            }
            break;
        case BC_MODULE_CLIMATE_EVENT_ERROR_THERMOMETER:
        case BC_MODULE_CLIMATE_EVENT_ERROR_HYGROMETER:
        case BC_MODULE_CLIMATE_EVENT_ERROR_LUX_METER:
        case BC_MODULE_CLIMATE_EVENT_ERROR_BAROMETER:

        default:
            break;
    }
}

void application_init(void)
{
    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    bc_button_init(&button_5s, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button_5s, button_5s_event_handler, NULL);
    bc_button_set_hold_time(&button_5s, 5000);

    //temperature on core
    static bc_tag_temperature_t temperature_tag_0_49;
    bc_tag_temperature_init(&temperature_tag_0_49, BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);
    bc_tag_temperature_set_update_interval(&temperature_tag_0_49, 60000);
    bc_tag_temperature_set_event_handler(&temperature_tag_0_49, temperature_tag_event_handler, NULL);

    //climate module
    bc_module_climate_init();
    bc_module_climate_set_update_interval_thermometer(30000);
    bc_module_climate_set_update_interval_hygrometer(30000);
    bc_module_climate_set_update_interval_lux_meter(15000);
    bc_module_climate_set_update_interval_barometer(60000);
    bc_module_climate_set_event_handler(climate_event_event_handler, NULL);

    // Initialize radio
    bc_radio_init();
    bc_radio_set_event_handler(radio_event_handler, NULL);
    bc_radio_listen();
}
