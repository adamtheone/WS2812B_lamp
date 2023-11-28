#ifndef MAIN_LOGIC_H__
#define MAIN_LOGIC_H__
//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include <stdint.h>
#include "app_timer.h"
//---------------------------------------------------------------------------------------------------------------------
// DEFINES
//---------------------------------------------------------------------------------------------------------------------
// GPIOS
#define USER_LED_BLUE               NRF_GPIO_PIN_MAP(0,6)
#define USER_LED_GREEN              NRF_GPIO_PIN_MAP(0,30)

#define BATTERY_MEAS_AIN            NRF_SAADC_INPUT_AIN7
#define COINCELL_MEAS_AIN           NRF_SAADC_INPUT_AIN3
#define BATTERY_MEAS_EN_PIN         NRF_GPIO_PIN_MAP(0,14)
#define CHG_DET_PIN                 NRF_GPIO_PIN_MAP(0,17)

// PILL CONFIG
#define LOCATION_UNSET              0x00
#define LOCATION_MASK               0x0F
#define LOCATION_MAX                LOCATION_MASK

// SCHEDULER
#define SCHED_MAX_EVENT_DATA_SIZE   0
#define SCHED_QUEUE_SIZE            5

// TIME
static const uint32_t TIMER_TICK_MS_DIVIDER = 
    (uint32_t)APP_TIMER_CLOCK_FREQ / (uint32_t)(1000 * (1 + APP_TIMER_CONFIG_RTC_FREQUENCY) );
#define ELAPSED_MS_BETWEEN(old_tick, new_tick) \
    (app_timer_cnt_diff_compute(new_tick, old_tick) / TIMER_TICK_MS_DIVIDER)
//---------------------------------------------------------------------------------------------------------------------
// TYPEDEFS
//---------------------------------------------------------------------------------------------------------------------
typedef enum {
    CPT_START_STREAM    = 'O',
    CPT_STOP_STREAM     = 'X',
    CPT_STREAM_DATA     = 'D',
    CPT_SET_LOCATION    = 'L',

    CPT_RESPONSE        = 'R',
} command_packet_type_t;

typedef struct {
    uint8_t location_4bit;
} pill_config_t;
//---------------------------------------------------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Gets the configuration data containing the pre-set and stored paramerers.
 *
 * @return pill_config_t* Pointer to the structure containing the configuration data.
 */
pill_config_t* main_logic_get_config(void);

/**
 * @brief Processes the incoming commands from the NUS central (phone), so the tasks
 * can be done from the main context.
 *
 * @param[in] p_event_data Pointer to the event data.
 * @param[in] event_size   Size of the event data.
 */
void main_logic_command_processor(void * p_event_data, uint16_t event_size);

/**
 * @brief Initializes the main logic. Called from original main.c once.
 *
 */
void main_logic_init(void);

/**
 * @brief Main logic loop. Called from original main.c contiously.
 */
void main_logic_loop(void);

/**
 * @brief Feeds the watchdog. This functions has to be called within 
 * NRFX_WDT_CONFIG_RELOAD_VALUE ms.
 *
 */
void main_logic_feed_the_dog(void);

//---------------------------------------------------------------------------------------------------------------------
#endif /* MAIN_LOGIC_H__ */
