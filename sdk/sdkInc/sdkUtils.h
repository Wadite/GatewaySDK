#ifndef _SDK_UTILS_H
#define _SDK_UTILS_H

#include "osal.h"

#define RETURN_ON_FAIL(response, succeess, retValOnFail) 	do {                                \
                                                                if((response) != (succeess))    \
                                                                {                               \
                                                                    return (retValOnFail);      \
                                                                }                               \
                                                            } while(0)

#define RESET_ON_FAIL(response, success)    do {                                                \
                                                if((response) != (success))                     \
                                                {                                               \
                                                    printk("Error %d, resetting..", response);  \
                                                    OsalSleep(1000);                            \
                                                    OsalSystemReset();                          \
                                                    OsalSleep(1000);                            \
                                                }                                               \
                                            } while(0)

#endif //_SDK_UTILS_H