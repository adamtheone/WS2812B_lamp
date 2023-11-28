#ifndef FLASH_MANAGER_H__
#define FLASH_MANAGER_H__
//---------------------------------------------------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>

#include "main_logic.h"
//---------------------------------------------------------------------------------------------------------------------
// DEFINES
//---------------------------------------------------------------------------------------------------------------------
#define FDS_FILE_ID (0x8010)
#define FDS_REC_KEY (0x7010)
//---------------------------------------------------------------------------------------------------------------------
// TYPEDEFS 
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Initializes the flash manager.
 *
 * This function initializes the flash manager for configuration data storage.
 */
void flash_manager_init(void);

/**
 * @brief Reads the configuration data from flash.
 *
 * @param[out] data Pointer to the structure for storing the read configuration data.
 *
 * @return ret_code_t Return code indicating the success or failure of the read operation.
 *                    Use NRF_SUCCESS for success and appropriate error code for failure.
 */
ret_code_t flash_manager_read_config(pill_config_t* data);

/**
 * @brief Writes the configuration data to flash.
 *
 * @param[in] data Pointer to the structure containing the configuration data to be written.
 *
 * @return ret_code_t Return code indicating the success or failure of the write operation.
 *                    Use NRF_SUCCESS for success and appropriate error code for failure.
 */
ret_code_t flash_manager_write_config(pill_config_t* data);
//---------------------------------------------------------------------------------------------------------------------
#endif /* FLASH_MANAGER_H__ */