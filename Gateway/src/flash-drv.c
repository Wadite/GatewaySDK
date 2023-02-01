#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#include "flash-drv.h"

const static struct flash_area * s_storageArea = NULL;

inline static off_t getPageOffset(eFlashPage flashPage)
{
    return (off_t)(SIZE_OF_FLASH_PAGE * flashPage);
}

SDK_STAT FlashInit()
{
    int err = 0;
    static bool isFlashOpen = false;
 
    if(!isFlashOpen)
    {
        err = flash_area_open(FLASH_AREA_ID(storage), &s_storageArea);
        isFlashOpen = true;
    }

    return (err ? SDK_FAILURE : SDK_SUCCESS);
}

SDK_STAT FlashRead(eFlashPage flashPage, void * savePageAdd)
{
    if(!savePageAdd && (flashPage < FLASH_PAGE_NUM))
    {
        return SDK_INVALID_PARAMS;
    }

    int err = flash_area_read(s_storageArea, getPageOffset(flashPage), savePageAdd, SIZE_OF_FLASH_PAGE);
    if(err)
    {
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

SDK_STAT FlashWrite(eFlashPage flashPage, const void * loadStrAdd, size_t sizeOfLoadStr)
{
    if(!loadStrAdd && (flashPage < FLASH_PAGE_NUM))
    {
        return SDK_INVALID_PARAMS;
    }

    int ans = flash_area_write(s_storageArea, getPageOffset(flashPage), loadStrAdd, sizeOfLoadStr);
    if(ans)
    {
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

SDK_STAT FlashErase(eFlashPage flashPage)
{
    int ans = flash_area_erase(s_storageArea, getPageOffset(flashPage), SIZE_OF_FLASH_PAGE);
    if(ans)
    {
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}