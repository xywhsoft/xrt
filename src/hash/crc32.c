#include "../internal/xrt_crc32.h"

#if defined(XRT_FEATURE_CRC32) || defined(XRT_FEATURE_INFLATE) || \
	defined(XRT_FEATURE_DEFLATE) || defined(XRT_FEATURE_VFS_PACK)

/* One read-only 64-byte table, shared by public CRC32, gzip and VFS pack. */
uint32 __xrtCrc32UpdateRaw(uint32 iCrc, const void* pData, size_t iSize)
{
	static const uint32 Table[16] = {
		UINT32_C(0x00000000), UINT32_C(0x1DB71064),
		UINT32_C(0x3B6E20C8), UINT32_C(0x26D930AC),
		UINT32_C(0x76DC4190), UINT32_C(0x6B6B51F4),
		UINT32_C(0x4DB26158), UINT32_C(0x5005713C),
		UINT32_C(0xEDB88320), UINT32_C(0xF00F9344),
		UINT32_C(0xD6D6A3E8), UINT32_C(0xCB61B38C),
		UINT32_C(0x9B64C2B0), UINT32_C(0x86D3D2D4),
		UINT32_C(0xA00AE278), UINT32_C(0xBDBDF21C)
	};
	const uint8* pBytes = (const uint8*)pData;
	size_t i;
	for ( i = 0; i < iSize; i++ ) {
		iCrc ^= pBytes[i];
		iCrc = (iCrc >> 4u) ^ Table[iCrc & 15u];
		iCrc = (iCrc >> 4u) ^ Table[iCrc & 15u];
	}
	return iCrc;
}

XRT_API uint32 xrtCrc32Update(uint32 iCrc, const void* pData, size_t iSize)
{
	if ( !__xrtRangeValid(pData, iSize) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return __xrtCrc32UpdateRaw(iCrc ^ UINT32_MAX, pData, iSize) ^ UINT32_MAX;
}

XRT_API uint32 xrtCrc32(const void* pData, size_t iSize)
{
	return xrtCrc32Update(0, pData, iSize);
}
#endif
