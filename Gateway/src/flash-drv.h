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

SDK_STAT FlashInit();

SDK_STAT FlashRead(eFlashPage flashPage, void * savePageAdd);

SDK_STAT FlashWrite(eFlashPage flashPage, const void * loadStrAdd, size_t sizeOfLoadStr);

SDK_STAT FlashErase(eFlashPage flashPage);

#endif //_FLASH_DRV_H_