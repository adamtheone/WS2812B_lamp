//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include <string.h>
#include "nrf_log.h"
#include "fds.h"

#include "flash_manager.h"
//---------------------------------------------------------------------------------------------------------------------
// GLOBALS
//---------------------------------------------------------------------------------------------------------------------
static bool fds_task_done = false;
//---------------------------------------------------------------------------------------------------------------------
// PRIVATE FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------
static void fds_evt_handler(fds_evt_t const * p_evt) {
    NRF_LOG_INFO("FDS EVENT: %d", p_evt->id);
    switch (p_evt->id) {
        case FDS_EVT_INIT:
            fds_task_done = true;
            if (p_evt->result == NRF_SUCCESS) {
            } else {
                NRF_LOG_ERROR("FDS_EVT_INIT FAILED: %d", p_evt->result);
            }
            break;

        case FDS_EVT_WRITE: {
            fds_task_done = true;
            if (p_evt->result == NRF_SUCCESS) {
                NRF_LOG_INFO("Record ID:\t0x%04x",  p_evt->write.record_id);
                NRF_LOG_INFO("File ID:\t0x%04x",    p_evt->write.file_id);
                NRF_LOG_INFO("Record key:\t0x%04x", p_evt->write.record_key);
            } else {
                NRF_LOG_ERROR("FDS_EVT_INIT FAILED: %d", p_evt->result);
            }
        } break;

        case FDS_EVT_DEL_RECORD: {
            fds_task_done = true;
            if (p_evt->result == NRF_SUCCESS) {
                NRF_LOG_INFO("Record ID:\t0x%04x",  p_evt->del.record_id);
                NRF_LOG_INFO("File ID:\t0x%04x",    p_evt->del.file_id);
                NRF_LOG_INFO("Record key:\t0x%04x", p_evt->del.record_key);
            }
        } break;

        default:
            break;
    }
}
//---------------------------------------------------------------------------------------------------------------------
// PUBLIC PROTOTYPES
//---------------------------------------------------------------------------------------------------------------------
void flash_manager_init(void) {
    ret_code_t err_code;

    /* Register first to receive an event when initialization is complete. */
    (void) fds_register(fds_evt_handler);

    err_code = fds_init();
    APP_ERROR_CHECK(err_code);

    while (!fds_task_done) {
        // TODO: watchdog feed, power manage
    }

    fds_stat_t stat = {0};

    err_code = fds_stat(&stat);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("Found %d valid records.", stat.valid_records);
    NRF_LOG_INFO("Found %d dirty records (ready to be garbage collected).", stat.dirty_records);
}

ret_code_t flash_manager_read_config(pill_config_t* data) {
    ret_code_t err_code;
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};
    err_code = fds_record_find(FDS_FILE_ID, FDS_REC_KEY, &desc, &tok);

    if (err_code == NRF_SUCCESS) {
        fds_flash_record_t fds_record = {0};

        /* Open the record and read its contents. */
        err_code = fds_record_open(&desc, &fds_record);
        APP_ERROR_CHECK(err_code);

        /* Copy the configuration from flash into m_dummy_cfg. */
        memcpy(data, fds_record.p_data, sizeof(pill_config_t));

        /* Close the record when done reading. */
        err_code = fds_record_close(&desc);
        APP_ERROR_CHECK(err_code);
    }
    return err_code;
}

ret_code_t flash_manager_write_config(pill_config_t* data){
    ret_code_t err_code;
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};

    fds_record_t const record = {
        .file_id           = FDS_FILE_ID,
        .key               = FDS_REC_KEY,
        .data.p_data       = data,
        /* The length of a record is always expressed in 4-byte units (words). */
        .data.length_words = (sizeof(pill_config_t) + 3) / sizeof(uint32_t),
    };

    err_code = fds_record_find(FDS_FILE_ID, FDS_REC_KEY, &desc, &tok);

    if (err_code == NRF_SUCCESS) {
        err_code = fds_record_update(&desc, &record);
        APP_ERROR_CHECK(err_code);
    } else {
        err_code = fds_record_write(&desc, &record);
        APP_ERROR_CHECK(err_code);
    }
    return err_code;
}