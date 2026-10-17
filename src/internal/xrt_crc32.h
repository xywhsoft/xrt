#ifndef XRT_INTERNAL_CRC32_H
#define XRT_INTERNAL_CRC32_H
#include "xrt_internal.h"
#include <xrt/crc32.h>

#if defined(XRT_FEATURE_CRC32) || defined(XRT_FEATURE_INFLATE) || \
	defined(XRT_FEATURE_DEFLATE) || defined(XRT_FEATURE_VFS_PACK)
/* Internal unchecked accumulator: initialized to UINT32_MAX, complemented on
 * completion. Only trusted, already validated ranges may reach this entry. */
uint32 __xrtCrc32UpdateRaw(uint32 iCrc, const void* pData, size_t iSize);
#endif
#endif
