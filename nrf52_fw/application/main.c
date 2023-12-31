#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_util_platform.h"
#include "nrf_pwr_mgmt.h"
#include "ble_dfu.h"
#include "nrf_drv_wdt.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "pwm.h"
#include "main_logic.h"
#include "ble_manager.h"
#include "dfu_handler.h"
#include "flash_manager.h"
#include "app_scheduler.h"

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define DEVICE_NAME                     "WS2812B"                                  /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                MSEC_TO_UNITS(50, UNIT_0_625_MS)            /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define APP_ADV_DURATION                0                                           /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(7.5, UNIT_1_25_MS)            /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(300, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

#define SCHED_MAX_EVENT_DATA_SIZE   0
#define SCHED_QUEUE_SIZE            5

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

static nrf_drv_wdt_channel_id m_channel_id;
static ble_advertising_init_t  m_adv_init;
/* forward declarations, becuase I don't have time now: */
ble_advertising_init_t  *p_adv_init = &m_adv_init;
ble_advertising_t       *p_advertising = &m_advertising;
uint16_t                *p_conn_handle = &m_conn_handle;
ble_nus_t               *p_nus = &m_nus;
/**/


void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name) {
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void timers_init(void) {
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

static void gap_params_init(void) {
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

static void nrf_qwr_error_handler(uint32_t nrf_error) {
    APP_ERROR_HANDLER(nrf_error);
}

static void services_init(void) {
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize ButtonlessDFU
    dfu_handler_init();

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = ble_manager_nus_rx_cb;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


static void on_conn_params_evt(ble_conn_params_evt_t * p_evt) {
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


static void conn_params_error_handler(uint32_t nrf_error) {
    APP_ERROR_HANDLER(nrf_error);
}


static void conn_params_init(void) {
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt) {
    uint32_t err_code;

    switch (ble_adv_evt) {
        case BLE_ADV_EVT_FAST:
            break;
        case BLE_ADV_EVT_IDLE:
            break;
        default:
            break;
    }
}


static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context) {
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:  {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

static void ble_stack_init(void) {
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt) {
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED)) {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}

void gatt_init(void) {
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}


static void advertising_init(void) {
    uint32_t               err_code;

    memset(&m_adv_init, 0, sizeof(m_adv_init));

    m_adv_init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    m_adv_init.advdata.include_appearance = false;
    m_adv_init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    //init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    //init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    m_adv_init.config.ble_adv_fast_enabled  = true;
    m_adv_init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    m_adv_init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    m_adv_init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &m_adv_init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static void log_init(void) {
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void power_management_init(void){
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

static void idle_state_handle(void) {
    if (NRF_LOG_PROCESS() == false) {
        nrf_pwr_mgmt_run();
    }
}

static void advertising_start(void) {
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

void wdt_event_handler(void) {
    // The max amount of time we can spend in WDT interrupt 
    // is two cycles of 32768[Hz] clock - after that, reset occurs
}

static void wdt_init(void) {
    ret_code_t err_code;
    nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;
    err_code = nrf_drv_wdt_init(&config, wdt_event_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_wdt_channel_alloc(&m_channel_id);
    APP_ERROR_CHECK(err_code);
    nrf_drv_wdt_enable();
}

int main(void) {
    log_init();
    
    // Initialize the async SVCI interface to bootloader before any interrupts are enabled.
    ret_code_t ret = ble_dfu_buttonless_async_svci_init();
    if (ret == NRF_SUCCESS) {
        NRF_LOG_ERROR("No bootloader present");
    }

    timers_init();
    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();

    NRF_LOG_INFO("Debug logging for UART over RTT started.");
    advertising_start();

    // SCHEDULER
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);

    // WDT
    //wdt_init();

    // PWM
    APP_ERROR_CHECK(pwm_init());

    // FDS
    flash_manager_init();

    // Enter main loop.
    for (;;) {
        //nrf_drv_wdt_channel_feed(m_channel_id);
        app_sched_execute();
        idle_state_handle();
    }
}


/**
 * @}
 */
