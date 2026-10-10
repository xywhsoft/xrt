#ifndef XRT_TYPED_DICT_INTERNAL_H
#define XRT_TYPED_DICT_INTERNAL_H

#include <xrt/typed_dict.h>



#if defined(XRUNTIME_FEATURE_TYPED_DICT)

/* 内建字典类型描述复用的进程期实例操作表。 */
extern const xrtinstanceops __xrtTypedDictInstanceOperations;

/* Exact built-in storage/destructor/key policy, not just a TypeId match.
 * Caller holds complete quiescence; no arbitrary callback is invoked. */
bool __xrtTypedDictOwnershipStorage(const xtypeddict*, const xrttype*);

#endif

#endif
