#ifndef _SDK_UTILS_H
#define _SDK_UTILS_H

#define RETURN_ON_FAIL(response, succeess, retValOnFail) 	do{ if((response) != (succeess)) { return (retValOnFail);} } while(0)

#endif //_SDK_UTILS_H