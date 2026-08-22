#ifndef XRT_INTERNAL_SLOT_MAP_H
#define XRT_INTERNAL_SLOT_MAP_H

#include "xrt_array.h"



#if defined(XRT_FEATURE_SLOT_MAP)

#define XRT_SLOT_GENERATION_MAX	UINT32_MAX



/* 内部槽记录以空指针表示空闲，以零代际表示永久退役。 */
typedef struct xslotentry {
	ptr Value;
	uint32 Generation;
	uint32 NextFree;
} xslotentry;

#endif

#endif
