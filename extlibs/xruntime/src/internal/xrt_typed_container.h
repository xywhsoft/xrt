#ifndef XRT_INTERNAL_TYPED_CONTAINER_H
#define XRT_INTERNAL_TYPED_CONTAINER_H

#include "xrt_internal.h"

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)
	#include <xrt/typed_array.h>
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST)
	#include <xrt/typed_list.h>
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET)
	#include <xrt/typed_set.h>
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT)
	#include <xrt/typed_dict.h>
#endif



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)

/* 在跨模块用户回调期间拒绝当前类型数组的全部 API 重入。 */
void __xrtTypedArrayCallbackBegin(const xtypedarray* pArray);
void __xrtTypedArrayCallbackEnd(const xtypedarray* pArray);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST)

/* 在跨模块用户回调期间拒绝当前类型列表的全部 API 重入。 */
void __xrtTypedListCallbackBegin(const xtypedlist* pList);
void __xrtTypedListCallbackEnd(const xtypedlist* pList);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET)

/* 在跨模块用户回调期间拒绝当前类型集合的全部 API 重入。 */
bool __xrtTypedSetCallbackBegin(const xtypedset* pSet);
void __xrtTypedSetCallbackEnd(const xtypedset* pSet);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT)

/* 在跨模块用户回调期间拒绝当前类型字典的全部 API 重入。 */
bool __xrtTypedDictCallbackBegin(const xtypeddict* pDict);
void __xrtTypedDictCallbackEnd(const xtypeddict* pDict);

#endif

#endif
