
#include "cJSON.h"

#define JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(itemTocheck,ItemToDelete) do { if(((itemTocheck) == NULL)) \
{ \
    cJSON_Delete(ItemToDelete);\
    return NULL; \
}} while(0)

/**
 * @brief Function to create the common header of json message.
 * 
 * @return ptr to cJSON object
 */
cJSON * JsonHeaderCreate();

/**
 * @brief Function to init malloc and free hooks(must be used!)
 * 
 */
void InitJsonHooks();

/**
 * @brief Function free allocated string
 * 
 * @param ptr pointer to string that need to be freed.
 */
void FreeJsonString(void* ptr);