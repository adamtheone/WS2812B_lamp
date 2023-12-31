#ifndef MAIN_H__
#define MAIN_H__
//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include "nrf_queue.h"
#include "ble_nus.h"
#include "ble_advertising.h"
//---------------------------------------------------------------------------------------------------------------------
// ugly GLOBALS - because I don't have time :(
//---------------------------------------------------------------------------------------------------------------------
extern const nrf_queue_t        *p_nus_rx_queue;
extern ble_advertising_t        *p_advertising;
extern ble_advertising_init_t   *p_adv_init;
extern uint16_t                 *p_conn_handle;
extern ble_nus_t                *p_nus;
//---------------------------------------------------------------------------------------------------------------------
#endif /* MAIN_H__ */