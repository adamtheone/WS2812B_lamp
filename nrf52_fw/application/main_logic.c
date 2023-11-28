//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include "app_scheduler.h"
#include "nrf_log.h"
#include "nrf_gpio.h"
#include "app_timer.h"
#include "nrf_delay.h"
#include "nrf_drv_wdt.h"

#include "fw_version.h"
#include "main_logic.h"
#include "stream_manager.h"
#include "flash_manager.h"
#include "battery_measurement.h"
#include "ble_manager.h"
//---------------------------------------------------------------------------------------------------------------------
// DEFINES
//---------------------------------------------------------------------------------------------------------------------
APP_TIMER_DEF(battery_app_timer);
APP_TIMER_DEF(led_app_timer);
//---------------------------------------------------------------------------------------------------------------------
// GLOBALS
//---------------------------------------------------------------------------------------------------------------------
static pill_config_t pill_config = {
    .location_4bit = LOCATION_UNSET
};

static nrf_drv_wdt_channel_id m_channel_id;
//---------------------------------------------------------------------------------------------------------------------
// PRIVATE FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
static void led_blink(void) {
    uint32_t pin = stream_manager_is_streaming() ? USER_LED_GREEN : USER_LED_BLUE;
    nrf_gpio_pin_clear(pin);
    nrf_delay_ms(10);
    nrf_gpio_pin_set(pin);
}

static void main_logic_timer_cb(void* context) {
    ret_code_t ret = app_sched_event_put(NULL, 0, (app_sched_event_handler_t)context);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("Error scheduling NUS rx event: %d", ret);
    }
}

static void main_logic_read_config(void) {
    ret_code_t ret;
    pill_config_t config;
    
    ret = flash_manager_read_config(&config);
    if (NRF_SUCCESS == ret) {
        pill_config.location_4bit = config.location_4bit;
        NRF_LOG_INFO("Config: ");
        NRF_LOG_INFO(" - location: %d", pill_config.location_4bit);
    } else {
        NRF_LOG_ERROR("Error reading the config from flash: %d", ret);
    }
}

void wdt_event_handler(void) {
    // The max amount of time we can spend in WDT interrupt 
    // is two cycles of 32768[Hz] clock - after that, reset occurs
}

static void main_logic_wdt_init(void) {
    ret_code_t err_code;
    nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;
    err_code = nrf_drv_wdt_init(&config, wdt_event_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_wdt_channel_alloc(&m_channel_id);
    APP_ERROR_CHECK(err_code);
    nrf_drv_wdt_enable();
}

static void main_logic_timers_init(void) {
    app_timer_create(&battery_app_timer, APP_TIMER_MODE_REPEATED, main_logic_timer_cb);
    app_timer_start(battery_app_timer, APP_TIMER_TICKS(30000), (void*)battery_measurement_runner);

    app_timer_create(&led_app_timer, APP_TIMER_MODE_REPEATED, main_logic_timer_cb);
    app_timer_start(led_app_timer, APP_TIMER_TICKS(3000), (void*)led_blink);
}
//---------------------------------------------------------------------------------------------------------------------
// PUBLIC FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
void main_logic_feed_the_dog(void) {
    nrf_drv_wdt_channel_feed(m_channel_id);
}

pill_config_t* main_logic_get_config(void) {
    return &pill_config;
}

void main_logic_command_processor(void * p_event_data, uint16_t event_size) {
    ret_code_t ret;
    data_t command;

    ret = ble_manager_rx_queue_pop(&command);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("Error popping the last element from NUS rx queue: %d", ret);
    }

    if (!command.len) {
        NRF_LOG_ERROR("Invalid NUS rx packet len");
    }

    NRF_LOG_HEXDUMP_INFO(command.data, command.len);

    switch (command.data[0]) {
        case CPT_START_STREAM:
            if (battery_measurement_get_voltage() < MINIMUM_WORKING_BATTERY_VOLTAGE) {
                uint8_t rsp[] = {CPT_RESPONSE, 0};
                ble_manager_nus_tx(rsp, sizeof(rsp));
                NRF_LOG_ERROR("Rejecting stream start because of low battery! %dmV", battery_measurement_get_voltage());
            } else {
                stream_manager_start(&command.data[1], command.len-1);
            }
            break;

        case CPT_STOP_STREAM:
            stream_manager_stop();
            break;

        case CPT_SET_LOCATION:
            if (command.len != 2) {
                NRF_LOG_ERROR("Invalid length for CPT_SET_LOCATION: %d", command.len);
                break;
            } else {
                pill_config.location_4bit = command.data[1] & LOCATION_MASK;
                flash_manager_write_config(&pill_config);
                ble_manager_update_adv_data();
                NRF_LOG_DEBUG("CPT_SET_LOCATION | Set: %d", pill_config.location_4bit);
            }
            break;

        default:
            NRF_LOG_ERROR("Invalid NUS packet type: %d", command.data[0]);
    }
}

void main_logic_init(void) {
    // WDT
    main_logic_wdt_init();

    // FDS
    flash_manager_init();
    main_logic_read_config();

    // LEDS
    nrf_gpio_cfg_output(USER_LED_BLUE);
    nrf_gpio_cfg_output(USER_LED_GREEN);
    nrf_gpio_pin_set(USER_LED_BLUE);
    nrf_gpio_pin_set(USER_LED_GREEN);

    // SCHEDULER
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);

    // CHARGING
    nrf_gpio_cfg_input(CHG_DET_PIN, NRF_GPIO_PIN_PULLUP);

    // BATTERY
    battery_measurement_init();

    // TIMER
    main_logic_timers_init();

    // BLE
    ble_manager_init();

    // STREAM
    //stream_manager_start(NULL, 0);

    NRF_LOG_INFO("Pill fw started | v%d.%d", FW_VERSION_MAJOR, FW_VERSION_MINOR);
}

void main_logic_loop(void) {
    main_logic_feed_the_dog();
    app_sched_execute();
}