#ifndef XRT_XDEFLATE_H
#define XRT_XDEFLATE_H

/* Internal whole-buffer deflater shared by HTTP and WebSocket protocol layers. */

#include "third_party/miniz/miniz_tdef.h"
#include "third_party/miniz/miniz_tdef.c"

typedef enum {
	__XDEFLATE_FORMAT_RAW = 0,
	__XDEFLATE_FORMAT_ZLIB,
	__XDEFLATE_FORMAT_GZIP
} __xdeflate_format;

typedef struct {
	uint8* pData;
	size_t iLen;
	size_t iCap;
} __xdeflate_pmd_output;


static mz_bool __xdeflatePmdPut(const void* pData, int iLen, void* pUserData)
{
	__xdeflate_pmd_output* pOut = (__xdeflate_pmd_output*)pUserData;
	uint8* pNew;
	size_t iNeed;
	size_t iCap;
	if ( !pOut || iLen < 0 || (!pData && iLen > 0) ) { return MZ_FALSE; }
	if ( iLen == 0 ) { return MZ_TRUE; }
	if ( (size_t)iLen > SIZE_MAX - pOut->iLen ) { return MZ_FALSE; }
	iNeed = pOut->iLen + (size_t)iLen;
	if ( iNeed > pOut->iCap ) {
		iCap = pOut->iCap > 0u ? pOut->iCap : 256u;
		while ( iCap < iNeed ) {
			if ( iCap > SIZE_MAX / 2u ) { iCap = iNeed; break; }
			iCap *= 2u;
		}
		pNew = (uint8*)XNET_ALLOC(iCap);
		if ( !pNew ) { return MZ_FALSE; }
		if ( pOut->iLen > 0u ) { memcpy(pNew, pOut->pData, pOut->iLen); }
		XNET_FREE(pOut->pData);
		pOut->pData = pNew;
		pOut->iCap = iCap;
	}
	memcpy(pOut->pData + pOut->iLen, pData, (size_t)iLen);
	pOut->iLen = iNeed;
	return MZ_TRUE;
}


static bool __xdeflatePmdMessage(const void* pData, size_t iLen, int iLevel,
	void** ppOut, size_t* pOutLen)
{
	static const uint8 aTail[4] = { 0u, 0u, 0xffu, 0xffu };
	__xdeflate_pmd_output tOut;
	tdefl_compressor* pCompressor;
	int iFlags;
	tdefl_status eStatus;
	if ( ppOut ) { *ppOut = NULL; }
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( !ppOut || !pOutLen || (!pData && iLen > 0u) ) { return false; }
	memset(&tOut, 0, sizeof(tOut));
	pCompressor = (tdefl_compressor*)XNET_ALLOC(sizeof(*pCompressor));
	if ( !pCompressor ) { return false; }
	if ( iLevel < 0 ) { iLevel = MZ_DEFAULT_LEVEL; }
	if ( iLevel > 10 ) { iLevel = 10; }
	iFlags = (int)tdefl_create_comp_flags_from_zip_params(iLevel, -15, MZ_DEFAULT_STRATEGY);
	eStatus = tdefl_init(pCompressor, __xdeflatePmdPut, &tOut, iFlags);
	if ( eStatus == TDEFL_STATUS_OKAY ) {
		eStatus = tdefl_compress_buffer(pCompressor, pData, iLen, TDEFL_SYNC_FLUSH);
	}
	XNET_FREE(pCompressor);
	if ( eStatus < TDEFL_STATUS_OKAY || tOut.iLen < sizeof(aTail) ||
		memcmp(tOut.pData + tOut.iLen - sizeof(aTail), aTail, sizeof(aTail)) != 0 ) {
		XNET_FREE(tOut.pData);
		return false;
	}
	tOut.iLen -= sizeof(aTail);
	*ppOut = tOut.pData;
	*pOutLen = tOut.iLen;
	return true;
}


static bool __xdeflateCompress(__xdeflate_format eFormat, const void* pData, size_t iLen,
	int iLevel, void** ppOut, size_t* pOutLen)
{
	void* pDeflated;
	size_t iDeflatedLen = 0u;
	int iFlags;
	if ( ppOut ) { *ppOut = NULL; }
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( !ppOut || !pOutLen || (!pData && iLen > 0u) ) { return false; }
	if ( iLevel < 0 ) { iLevel = MZ_DEFAULT_LEVEL; }
	iFlags = (int)tdefl_create_comp_flags_from_zip_params(iLevel,
		eFormat == __XDEFLATE_FORMAT_ZLIB ? 15 : -15, MZ_DEFAULT_STRATEGY);
	pDeflated = tdefl_compress_mem_to_heap(pData, iLen, &iDeflatedLen, iFlags);
	if ( !pDeflated ) { return false; }
	if ( eFormat == __XDEFLATE_FORMAT_GZIP ) {
		uint8* pGzip;
		mz_uint32 iCrc;
		mz_uint32 iInputSize = (mz_uint32)(iLen & 0xFFFFFFFFu);
		if ( iDeflatedLen > ((size_t)-1) - 18u ) {
			free(pDeflated);
			return false;
		}
		pGzip = (uint8*)malloc(iDeflatedLen + 18u);
		if ( !pGzip ) {
			free(pDeflated);
			return false;
		}
		pGzip[0] = 0x1Fu;
		pGzip[1] = 0x8Bu;
		pGzip[2] = 8u;
		pGzip[3] = 0u;
		pGzip[4] = pGzip[5] = pGzip[6] = pGzip[7] = 0u;
		pGzip[8] = 0u;
		pGzip[9] = 255u;
		if ( iDeflatedLen > 0u ) { memcpy(pGzip + 10u, pDeflated, iDeflatedLen); }
		iCrc = mz_crc32(MZ_CRC32_INIT, (const mz_uint8*)pData, iLen);
		pGzip[10u + iDeflatedLen] = (uint8)iCrc;
		pGzip[11u + iDeflatedLen] = (uint8)(iCrc >> 8u);
		pGzip[12u + iDeflatedLen] = (uint8)(iCrc >> 16u);
		pGzip[13u + iDeflatedLen] = (uint8)(iCrc >> 24u);
		pGzip[14u + iDeflatedLen] = (uint8)iInputSize;
		pGzip[15u + iDeflatedLen] = (uint8)(iInputSize >> 8u);
		pGzip[16u + iDeflatedLen] = (uint8)(iInputSize >> 16u);
		pGzip[17u + iDeflatedLen] = (uint8)(iInputSize >> 24u);
		free(pDeflated);
		pDeflated = pGzip;
		iDeflatedLen += 18u;
	}
	*ppOut = pDeflated;
	*pOutLen = iDeflatedLen;
	return true;
}

#endif
