#ifndef XRT_INTERNAL_BLOCK_STACK_H
#define XRT_INTERNAL_BLOCK_STACK_H

#include "xrt_internal.h"
#include "xrt_array.h"



#if defined(XRT_FEATURE_BLOCK_STACK)

/* 每个分块把管理头和过对齐数据区放在同一次分配中。 */
typedef struct xblockstackblock {
	struct xblockstackblock* Next;
	bytes Data;
} xblockstackblock;

#endif

#endif
