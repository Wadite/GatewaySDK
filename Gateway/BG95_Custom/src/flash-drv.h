#ifndef _FLASH_DRV_H_
#define _FLASH_DRV_H_

#include "sdk_common.h"

#define SIZE_OF_WORD                (4)
#define SIZE_OF_FLASH_PAGE          (4096)

typedef enum{
    FLASH_PAGE_CONFIGURATION,
    FLASH_PAGE_TOKENS,

    FLASH_PAGE_NUM
} eFlashPage;

/**
 * @brief Initialize the Flash driver.
 * 
 * @return SDK_FAILURE if flash failed to open
 * @return SDK_SUCCESS if flash opened successfully.
 */
SDK_STAT FlashInit();

/**
 * @brief Read page from flash.
 * 
 * @param flashPage enum of the requested flash page.
 * @param savePageAdd pointer to buffer to store the readen page.
 * 
 * @return SDK_INVALID_PARAMS if null ptr or bad page was passed.
 * @return SDK_FAILURE if internal flash error accord.
 * @return SDK_SUCCESS if initialized successfully.
 */
SDK_STAT FlashRead(eFlashPage flashPage, void * savePageAdd);

/**
 * @brief Write page to flash.
 * 
 * @param flashPage enum of the requested flash page.
 * @param loadStrAdd pointer to buffer that have data to write.
 * @param sizeOfLoadStr size of data to write.
 * 
 * @return SDK_INVALID_PARAMS if null ptr or bad page was passed.
 * @return SDK_FAILURE if internal flash error accord.
 * @return SDK_SUCCESS if initialized successfully.
 */
SDK_STAT FlashWrite(eFlashPage flashPage, const void * loadStrAdd, size_t sizeOfLoadStr);

/**
 * @brief Erase page from flash.
 * 
 * @param flashPage enum of the requested flash page.
 * 
 * @return SDK_FAILURE if internal flash error accord.
 * @return SDK_SUCCESS if initialized successfully.
 */
SDK_STAT FlashErase(eFlashPage flashPage);

#endif //_FLASH_DRV_H_