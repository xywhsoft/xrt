#ifndef XRT_INTERNAL_ARRAY_H
#define XRT_INTERNAL_ARRAY_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_ARRAY)

/* 检查数组公开状态是否自洽，供数组类型薄封装复用。 */
bool __xrtArrayValid(const xarray* pArray);



/* 在调用方已经验证数组后保证最低容量。 */
bool __xrtArrayReserveValid(xarray* pArray, size_t iCapacity);



/* 在调用方已经验证数组后增加未初始化尾部元素。 */
ptr __xrtArrayAddValid(xarray* pArray, size_t iCount);

#endif

#endif
