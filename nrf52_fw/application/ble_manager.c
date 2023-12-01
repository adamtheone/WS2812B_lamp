//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include <stdlib.h>
#include "sdk_errors.h"
#include "nrf_log.h"
#include "nrf_queue.h"
#include "app_scheduler.h"

#include "ble_manager.h"
#include "main.h"
#include "pwm.h"
//---------------------------------------------------------------------------------------------------------------------
// DEFINES
//---------------------------------------------------------------------------------------------------------------------
NRF_QUEUE_DEF(data_t, nus_rx_queue, 10, NRF_QUEUE_MODE_NO_OVERFLOW);
const nrf_queue_t *p_nus_rx_queue = &nus_rx_queue;
//---------------------------------------------------------------------------------------------------------------------
// GLOBALS
//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
// PRIVATE FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
static void ble_manager_disconnect(uint16_t conn_handle, void * p_context) {
    UNUSED_PARAMETER(p_context);

    ret_code_t err_code = sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("Failed to disconnect connection. Connection handle: %d Error: %d", conn_handle, err_code);
    } else {
        NRF_LOG_DEBUG("Disconnected connection handle %d", conn_handle);
    }
}
//---------------------------------------------------------------------------------------------------------------------
// PUBLIC FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
void command_processor(void * p_event_data, uint16_t event_size) {
    data_t command;

    ret_code_t ret = nrf_queue_pop(&nus_rx_queue, &command);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("Error popping the last element from NUS rx queue: %d", ret);
    }

    if (!command.len) {
        NRF_LOG_ERROR("Invalid NUS rx packet len");
    }

    NRF_LOG_HEXDUMP_INFO(command.data, command.len);

    command.data[command.len] = '\0';

    char* ptr = (char*)command.data;
    uint8_t leds_num = atoi(ptr);
    ptr = strstr(ptr, " ") + 1;
    uint8_t r = atoi(ptr);
    ptr = strstr(ptr, " ") + 1;
    uint8_t g = atoi(ptr);
    ptr = strstr(ptr, " ") + 1;
    uint8_t b = atoi(ptr);

    drv_ws2812_update(r, g, b, leds_num);
    NRF_LOG_INFO("leds:%d r:%d g:%d b:%d", leds_num, r, g, b);
}

void ble_manager_prepare_for_dfu(void) {
    // Prevent device from advertising on disconnect.
    p_adv_init->config.ble_adv_on_disconnect_disabled = true;
    ble_advertising_modes_config_set(p_advertising, &p_adv_init->config);

    // Disconnect all other bonded devices that currently are connected.
    // This is required to receive a service changed indication
    // on bootup after a successful (or aborted) Device Firmware Update.
    uint32_t conn_count = ble_conn_state_for_each_connected(ble_manager_disconnect, NULL);
    NRF_LOG_INFO("Disconnected %d links.", conn_count);
}

void ble_manager_nus_rx_cb(ble_nus_evt_t * p_evt) {
    ret_code_t ret;
    data_t queue_data;

    uint16_t nus_data_len = p_evt->params.rx_data.length;
    uint8_t const* nus_data = p_evt->params.rx_data.p_data;
        
    if (nus_data_len > MAX_DATA_BUFF_LEN) {
        NRF_LOG_ERROR("Incoming NUS rx data len is too large %d / %d", nus_data_len, MAX_DATA_BUFF_LEN);
    }

    memcpy(queue_data.data, nus_data, nus_data_len);
    queue_data.len = nus_data_len;
    
    ret = nrf_queue_push(&nus_rx_queue, &queue_data);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("Error pushing data to NUS rx queue: %d", ret);
    }

    ret = app_sched_event_put(NULL, 0, command_processor);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("Error scheduling NUS rx event: %d", ret);
    }
}

ret_code_t ble_manager_nus_tx(uint8_t* data, uint8_t len) {
    ret_code_t ret;
    uint8_t retries = 50;

    if (*p_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return BLE_CONN_HANDLE_INVALID;
    }
    do {
        uint16_t length = (uint16_t)len;
        ret = ble_nus_data_send(p_nus, data, &length, *p_conn_handle);
        if ((ret != NRF_ERROR_INVALID_STATE) &&
            (ret != NRF_ERROR_RESOURCES) &&
            (ret != NRF_ERROR_NOT_FOUND)) {
            APP_ERROR_CHECK(ret);
        }
    } while (retries-- && ret == NRF_ERROR_RESOURCES);

    return ret;
}
