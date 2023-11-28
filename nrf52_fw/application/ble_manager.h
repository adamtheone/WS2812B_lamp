#ifndef BLE_MANAGER_H__
#define BLE_MANAGER_H__
//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>

#include "ble_nus.h"
//---------------------------------------------------------------------------------------------------------------------
// DEFINES
//---------------------------------------------------------------------------------------------------------------------
#define MAX_DATA_BUFF_LEN           255
//---------------------------------------------------------------------------------------------------------------------
// TYPEDEFS
//---------------------------------------------------------------------------------------------------------------------
typedef  struct {
    uint8_t data[MAX_DATA_BUFF_LEN];
    uint8_t len;
} data_t;
//---------------------------------------------------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Transmits data over the Nordic UART Service (NUS).
 *
 * @param[in] data Pointer to the data to be transmitted.
 * @param[in] len  Length of the data to be transmitted.
 *
 * @return ret_code_t Return code indicating the success or failure of the transmission.
 *                    Use NRF_SUCCESS for success and appropriate error code for failure.
 */
ret_code_t ble_manager_nus_tx(uint8_t* data, uint8_t len);

/**
 * @brief Callback function for handling received data over the NUS.
 *
 * @param[in] p_evt Nus event pointer.
 *
 */
void ble_manager_nus_rx_cb(ble_nus_evt_t * p_evt);

/**
 * @brief Prepares the BLE manager for Device Firmware Update (DFU).
 */
void ble_manager_prepare_for_dfu(void);
//---------------------------------------------------------------------------------------------------------------------
#endif /* BLE_MANAGER_H__ */