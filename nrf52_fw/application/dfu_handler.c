//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include "ble_dfu.h"
#include "nrf_sdh.h"
#include "nrf_power.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_log.h"
#include "nrf_bootloader_info.h"

#include "dfu_handler.h"
#include "ble_manager.h"
//---------------------------------------------------------------------------------------------------------------------
// GLOBALS
//---------------------------------------------------------------------------------------------------------------------
/* nrf_sdh state observer. */
static void buttonless_dfu_sdh_state_observer(nrf_sdh_state_evt_t state, void * p_context);
NRF_SDH_STATE_OBSERVER(m_buttonless_dfu_state_obs, 0) = {
    .handler = buttonless_dfu_sdh_state_observer,
};
//---------------------------------------------------------------------------------------------------------------------
// PRIVATE FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
static void buttonless_dfu_sdh_state_observer(nrf_sdh_state_evt_t state, void * p_context) {
    if (state == NRF_SDH_EVT_STATE_DISABLED) {
        // Softdevice was disabled before going into reset. Inform bootloader to skip CRC on next boot.
        nrf_power_gpregret2_set(BOOTLOADER_DFU_SKIP_CRC);

        //Go to system off.
        nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
    }
}

static void dfu_handler_evt_cb(ble_dfu_buttonless_evt_type_t event) {
    switch (event) {
        case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE: {
            NRF_LOG_INFO("Device is preparing to enter bootloader mode.");

            ble_manager_prepare_for_dfu();
            break;
        }

        case BLE_DFU_EVT_BOOTLOADER_ENTER:
            NRF_LOG_INFO("Device will enter bootloader mode.");
            break;

        case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED:
            NRF_LOG_ERROR("Request to enter bootloader mode failed asynchroneously.");
            break;

        case BLE_DFU_EVT_RESPONSE_SEND_ERROR:
            NRF_LOG_ERROR("Request to send a response to client failed.");
            APP_ERROR_CHECK(false);
            break;

        default:
            NRF_LOG_ERROR("Unknown event from ble_dfu_buttonless.");
            break;
    }
}
//---------------------------------------------------------------------------------------------------------------------
// PUBLIC FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
void dfu_handler_init(void) {
    ble_dfu_buttonless_init_t dfus_init = {0};

    dfus_init.evt_handler = dfu_handler_evt_cb;

    ret_code_t ret = ble_dfu_buttonless_init(&dfus_init);
    APP_ERROR_CHECK(ret);
}