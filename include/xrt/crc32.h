#ifndef XRT_CRC32_H
#define XRT_CRC32_H

#include <xrt/core.h>

#if defined(XRT_FEATURE_CRC32) || defined(XRT_FEATURE_INFLATE) || \
	defined(XRT_FEATURE_DEFLATE) || defined(XRT_FEATURE_VFS_PACK)

XRT_EXTERN_C_BEGIN

/* CRC-32/ISO-HDLC (ZIP/gzip), not CRC32C and not an authentication hash.
 * Empty input is zero. NULL is valid only for empty input. Invalid ranges
 * publish XERR_ARGUMENT and return zero; success preserves the current error.
 * No allocation, initialization table, shared mutable state or alignment need. */
XRT_API uint32 xrtCrc32(const void* pData, size_t iSize);

/* Extend a FINALIZED CRC returned by either public function. Start with zero;
 * an empty update returns iCrc unchanged. Callers never complement the state. */
XRT_API uint32 xrtCrc32Update(uint32 iCrc, const void* pData, size_t iSize);

XRT_EXTERN_C_END
#endif
#endif
