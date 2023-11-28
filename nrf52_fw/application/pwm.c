//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include "nrf_drv_pwm.h"
#include "nrf_gpio.h"

#include "pwm.h"
//---------------------------------------------------------------------------------------------------------------------
// DEFINES
//---------------------------------------------------------------------------------------------------------------------
#define WS2812B_PIN  NRF_GPIO_PIN_MAP(1,11)

#define LEDS_NUM_MAX                91
#define LEDS_NUM_MAX_EXCL_FIRST     LEDS_NUM_MAX - 1
#define LEDS_TOTAL_BYTE_WIDTH       LEDS_NUM_MAX * 3
#define LEDS_TOTAL_BIT_WIDTH        LEDS_TOTAL_BYTE_WIDTH * 8
//---------------------------------------------------------------------------------------------------------------------
// GLOBALS
//---------------------------------------------------------------------------------------------------------------------
static rgb_color_t leds_buffer[LEDS_NUM_MAX];

static nrfx_pwm_t m_pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t pwm_duty_cycle_values[LEDS_TOTAL_BIT_WIDTH];
volatile bool pwm_sequence_finished = true;

static nrf_pwm_sequence_t pwm_sequence = {
    .values.p_individual = pwm_duty_cycle_values,
    .length          = (sizeof(pwm_duty_cycle_values) / sizeof(uint16_t)),
    .repeats         = 0,
    .end_delay       = 0
};
//---------------------------------------------------------------------------------------------------------------------
// PRIVATE FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
static void pwm_handler(nrfx_pwm_evt_type_t event_type) {
    switch(event_type) {
	case NRFX_PWM_EVT_FINISHED:
	    pwm_sequence_finished = true;
	    break;
	default:
	    break;
    }
}

static void convert_rgb_to_pwm_sequence(void) {
    uint8_t * ptr = (uint8_t *)leds_buffer;
    uint32_t i = 0;
    for(int led = 0; led < LEDS_TOTAL_BYTE_WIDTH; led++) {
        for(int bit = 7; bit >= 0; bit--) {
            uint8_t b = (*ptr >> bit) & 0x01;
            uint16_t pwm = 0;
            if(b == 1) {
                pwm = WS2812_T1H;
            } else {
                pwm = WS2812_T0H;
            }
            pwm_duty_cycle_values[i++].channel_1 = pwm;
        }
        ptr++;
    }
}

static uint32_t drv_ws2812_led_set(uint8_t led_id, uint32_t color) {
    uint32_t err_code = NRF_SUCCESS;
    if(led_id >= LEDS_NUM_MAX) {
        err_code = NRF_ERROR_INVALID_PARAM;
    }
 
    leds_buffer[led_id].r = (color & 0x00FF0000) >> 16;
    leds_buffer[led_id].g = (color & 0x0000FF00) >> 8;
    leds_buffer[led_id].b = (color & 0x000000FF);
    
    return err_code;
}

static uint32_t drv_ws2812_display(void) {
    if(!pwm_sequence_finished) {
        return NRF_ERROR_BUSY;
    }
    convert_rgb_to_pwm_sequence();
    pwm_sequence_finished = false;
    uint32_t err_code = nrfx_pwm_simple_playback(&m_pwm0, &pwm_sequence, 1, NRFX_PWM_FLAG_STOP);
    return err_code;
}
//---------------------------------------------------------------------------------------------------------------------
// PUBLIC FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
uint32_t pwm_init(void) {
    nrfx_pwm_config_t pwm_config = NRFX_PWM_DEFAULT_CONFIG;
    pwm_config.output_pins[0] = NRFX_PWM_PIN_NOT_USED; 
    pwm_config.output_pins[1] = WS2812B_PIN; 
    pwm_config.output_pins[2] = NRFX_PWM_PIN_NOT_USED;
    pwm_config.output_pins[3] = NRFX_PWM_PIN_NOT_USED;
    pwm_config.load_mode    = NRF_PWM_LOAD_INDIVIDUAL;
    // WS2812 protocol requires a 800 kHz PWM frequency. PWM Top value = 20 and Base Clock = 16 MHz achieves this
    pwm_config.top_value    = 20; 
    pwm_config.base_clock   = NRF_PWM_CLK_16MHz;
    
    drv_ws2812_led_set(0,0);

    return nrfx_pwm_init(&m_pwm0, &pwm_config, pwm_handler);
}

void drv_ws2812_update(uint8_t r, uint8_t g, uint8_t b, uint8_t leds_num) {
    if (leds_num > LEDS_NUM_MAX_EXCL_FIRST) {
        leds_num = LEDS_NUM_MAX_EXCL_FIRST;
    }
    for (uint8_t i = 1; i < leds_num + 1; i++) {
        drv_ws2812_led_set(i, ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16));
    }

    for (uint8_t i = leds_num; i < LEDS_NUM_MAX; i++) {
        drv_ws2812_led_set(i, 0);
    }
    drv_ws2812_display();
}