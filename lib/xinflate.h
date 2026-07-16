#ifndef XRT_XINFLATE_H
#define XRT_XINFLATE_H

/* Internal streaming inflater shared by HTTP and WebSocket protocol layers. */

#include "third_party/miniz/miniz_tinfl.h"
#include "third_party/miniz/miniz_tinfl.c"

#define __XINFLATE_GZIP_HEADER_LIMIT 65536u

typedef enum {
	__XINFLATE_FORMAT_RAW = 0,
	__XINFLATE_FORMAT_ZLIB,
	__XINFLATE_FORMAT_DEFLATE_AUTO,
	__XINFLATE_FORMAT_GZIP
} __xinflate_format;

typedef enum {
	__XINFLATE_OK = 0,
	__XINFLATE_DONE,
	__XINFLATE_ERROR,
	__XINFLATE_LIMIT,
	__XINFLATE_CALLBACK_ABORT
} __xinflate_result;

typedef bool (*__xinflate_output_fn)(ptr pUserData, const void* pData, size_t iLen);

typedef enum {
	__XINFLATE_GZIP_FIXED = 0,
	__XINFLATE_GZIP_EXTRA_LENGTH,
	__XINFLATE_GZIP_EXTRA,
	__XINFLATE_GZIP_NAME,
	__XINFLATE_GZIP_COMMENT,
	__XINFLATE_GZIP_HEADER_CRC,
	__XINFLATE_GZIP_DEFLATE,
	__XINFLATE_GZIP_TRAILER,
	__XINFLATE_GZIP_BOUNDARY
} __xinflate_gzip_state;

typedef struct {
	tinfl_decompressor tInflater;
	uint8* pDictionary;
	size_t iDictionaryOffset;
	uint64 iOutputLimit;
	uint64 iTotalOutput;
	__xinflate_format eRequestedFormat;
	__xinflate_format eCodecFormat;
	bool bCodecInitialized;
	bool bDone;
	bool bFailed;
	uint8 aProbe[2];
	uint32 iProbeCount;
	__xinflate_gzip_state eGzipState;
	uint8 aGzipFixed[10];
	uint32 iGzipFixedCount;
	uint8 iGzipFlags;
	uint8 aGzipPair[2];
	uint32 iGzipPairCount;
	uint32 iGzipExtraRemain;
	uint32 iGzipHeaderBytes;
	uint32 iGzipHeaderCrc;
	uint32 iGzipDataCrc;
	uint32 iGzipDataSize;
	uint8 aGzipTrailer[8];
	uint32 iGzipTrailerCount;
} __xinflate;


static uint32 __xinflateCrc32Update(uint32 iCrc, const void* pData, size_t iLen)
{
	static const uint32 aTable[16] = {
		0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
		0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
		0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
		0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu
	};
	const uint8* pBytes = (const uint8*)pData;
	for ( size_t i = 0u; i < iLen; ++i ) {
		iCrc ^= pBytes[i];
		iCrc = (iCrc >> 4u) ^ aTable[iCrc & 15u];
		iCrc = (iCrc >> 4u) ^ aTable[iCrc & 15u];
	}
	return iCrc;
}


static void __xinflateCodecReset(__xinflate* pState, __xinflate_format eFormat)
{
	if ( !pState ) { return; }
	tinfl_init(&pState->tInflater);
	pState->iDictionaryOffset = 0u;
	pState->eCodecFormat = eFormat;
	pState->bCodecInitialized = true;
}


static void __xinflateGzipResetHeader(__xinflate* pState)
{
	if ( !pState ) { return; }
	pState->eGzipState = __XINFLATE_GZIP_FIXED;
	pState->iGzipFixedCount = 0u;
	pState->iGzipFlags = 0u;
	pState->iGzipPairCount = 0u;
	pState->iGzipExtraRemain = 0u;
	pState->iGzipHeaderBytes = 0u;
	pState->iGzipHeaderCrc = UINT32_MAX;
	pState->iGzipDataCrc = UINT32_MAX;
	pState->iGzipDataSize = 0u;
	pState->iGzipTrailerCount = 0u;
	pState->bCodecInitialized = false;
}


static bool __xinflateInit(__xinflate* pState, __xinflate_format eFormat, uint64 iOutputLimit)
{
	if ( !pState || (eFormat != __XINFLATE_FORMAT_RAW && eFormat != __XINFLATE_FORMAT_ZLIB &&
		eFormat != __XINFLATE_FORMAT_DEFLATE_AUTO && eFormat != __XINFLATE_FORMAT_GZIP) ) { return false; }
	memset(pState, 0, sizeof(*pState));
	pState->pDictionary = (uint8*)XNET_ALLOC(TINFL_LZ_DICT_SIZE);
	if ( !pState->pDictionary ) { return false; }
	memset(pState->pDictionary, 0, TINFL_LZ_DICT_SIZE);
	pState->eRequestedFormat = eFormat;
	pState->iOutputLimit = iOutputLimit;
	if ( eFormat == __XINFLATE_FORMAT_GZIP ) { __xinflateGzipResetHeader(pState); }
	else if ( eFormat != __XINFLATE_FORMAT_DEFLATE_AUTO ) { __xinflateCodecReset(pState, eFormat); }
	return true;
}


static void __xinflateUnit(__xinflate* pState)
{
	if ( !pState ) { return; }
	if ( pState->pDictionary ) { XNET_FREE(pState->pDictionary); }
	memset(pState, 0, sizeof(*pState));
}


static __xinflate_result __xinflateEmit(__xinflate* pState, const void* pData, size_t iLen,
	__xinflate_output_fn pfnOutput, ptr pUserData)
{
	if ( !pState || (!pData && iLen > 0u) ) { return __XINFLATE_ERROR; }
	if ( iLen == 0u ) { return __XINFLATE_OK; }
	if ( pState->iTotalOutput > pState->iOutputLimit ||
		(uint64)iLen > pState->iOutputLimit - pState->iTotalOutput ) { return __XINFLATE_LIMIT; }
	if ( pState->eRequestedFormat == __XINFLATE_FORMAT_GZIP ) {
		pState->iGzipDataCrc = __xinflateCrc32Update(pState->iGzipDataCrc, pData, iLen);
		pState->iGzipDataSize += (uint32)iLen;
	}
	if ( pfnOutput && !pfnOutput(pUserData, pData, iLen) ) { return __XINFLATE_CALLBACK_ABORT; }
	pState->iTotalOutput += (uint64)iLen;
	return __XINFLATE_OK;
}


static __xinflate_result __xinflateFeedCodec(__xinflate* pState, const uint8* pData,
	size_t iLen, bool bHasMoreInput, __xinflate_output_fn pfnOutput, ptr pUserData,
	size_t* pConsumed, bool* pDone)
{
	size_t iOffset = 0u;
	if ( pConsumed ) { *pConsumed = 0u; }
	if ( pDone ) { *pDone = false; }
	if ( !pState || !pState->bCodecInitialized || (!pData && iLen > 0u) ) { return __XINFLATE_ERROR; }
	for ( ;; ) {
		size_t iInput = iLen - iOffset;
		size_t iOutput = TINFL_LZ_DICT_SIZE - pState->iDictionaryOffset;
		mz_uint32 iFlags = pState->eCodecFormat == __XINFLATE_FORMAT_ZLIB
			? TINFL_FLAG_PARSE_ZLIB_HEADER : 0u;
		tinfl_status eStatus;
		__xinflate_result eEmit;
		if ( bHasMoreInput ) { iFlags |= TINFL_FLAG_HAS_MORE_INPUT; }
		eStatus = tinfl_decompress(&pState->tInflater, pData ? pData + iOffset : NULL,
			&iInput, pState->pDictionary, pState->pDictionary + pState->iDictionaryOffset,
			&iOutput, iFlags);
		iOffset += iInput;
		eEmit = __xinflateEmit(pState, pState->pDictionary + pState->iDictionaryOffset,
			iOutput, pfnOutput, pUserData);
		if ( eEmit != __XINFLATE_OK ) { return eEmit; }
		pState->iDictionaryOffset = (pState->iDictionaryOffset + iOutput) & (TINFL_LZ_DICT_SIZE - 1u);
		if ( eStatus == TINFL_STATUS_DONE ) {
			if ( pConsumed ) { *pConsumed = iOffset; }
			if ( pDone ) { *pDone = true; }
			return __XINFLATE_OK;
		}
		if ( eStatus == TINFL_STATUS_HAS_MORE_OUTPUT ) { continue; }
		if ( eStatus == TINFL_STATUS_NEEDS_MORE_INPUT ) {
			if ( pConsumed ) { *pConsumed = iOffset; }
			return (iOffset == iLen && bHasMoreInput) ? __XINFLATE_OK : __XINFLATE_ERROR;
		}
		return __XINFLATE_ERROR;
	}
}


static bool __xinflateLooksLikeZlib(const uint8 aProbe[2])
{
	uint32 iHeader = ((uint32)aProbe[0] << 8u) | aProbe[1];
	return (aProbe[0] & 15u) == 8u && (aProbe[0] >> 4u) <= 7u && (iHeader % 31u) == 0u;
}


static __xinflate_result __xinflateWriteDeflate(__xinflate* pState, const uint8* pData,
	size_t iLen, bool bFinal, __xinflate_output_fn pfnOutput, ptr pUserData)
{
	size_t iOffset = 0u;
	bool bDone = false;
	if ( pState->eRequestedFormat == __XINFLATE_FORMAT_DEFLATE_AUTO && !pState->bCodecInitialized ) {
		while ( pState->iProbeCount < 2u && iOffset < iLen ) {
			pState->aProbe[pState->iProbeCount++] = pData[iOffset++];
		}
		if ( pState->iProbeCount < 2u ) { return bFinal ? __XINFLATE_ERROR : __XINFLATE_OK; }
		__xinflateCodecReset(pState, __xinflateLooksLikeZlib(pState->aProbe)
			? __XINFLATE_FORMAT_ZLIB : __XINFLATE_FORMAT_RAW);
		{
			size_t iConsumed = 0u;
			__xinflate_result eResult = __xinflateFeedCodec(pState, pState->aProbe, 2u,
				!bFinal || iOffset < iLen, pfnOutput, pUserData, &iConsumed, &bDone);
			if ( eResult != __XINFLATE_OK || iConsumed != 2u || bDone ) {
				return (eResult == __XINFLATE_OK && bDone && iOffset == iLen) ? __XINFLATE_DONE :
					(eResult == __XINFLATE_OK ? __XINFLATE_ERROR : eResult);
			}
		}
	}
	{
		size_t iConsumed = 0u;
		__xinflate_result eResult = __xinflateFeedCodec(pState, pData ? pData + iOffset : NULL,
			iLen - iOffset, !bFinal, pfnOutput, pUserData, &iConsumed, &bDone);
		if ( eResult != __XINFLATE_OK ) { return eResult; }
		iOffset += iConsumed;
		if ( bDone ) {
			if ( iOffset != iLen ) { return __XINFLATE_ERROR; }
			pState->bDone = true;
			return __XINFLATE_DONE;
		}
	}
	return bFinal ? __XINFLATE_ERROR : __XINFLATE_OK;
}


static bool __xinflateGzipHeaderAdd(__xinflate* pState, uint8 iByte)
{
	if ( !pState || pState->iGzipHeaderBytes >= __XINFLATE_GZIP_HEADER_LIMIT ) { return false; }
	pState->iGzipHeaderBytes++;
	pState->iGzipHeaderCrc = __xinflateCrc32Update(pState->iGzipHeaderCrc, &iByte, 1u);
	return true;
}


static void __xinflateGzipSelectOptional(__xinflate* pState)
{
	if ( (pState->iGzipFlags & 0x04u) != 0u ) { pState->eGzipState = __XINFLATE_GZIP_EXTRA_LENGTH; }
	else if ( (pState->iGzipFlags & 0x08u) != 0u ) { pState->eGzipState = __XINFLATE_GZIP_NAME; }
	else if ( (pState->iGzipFlags & 0x10u) != 0u ) { pState->eGzipState = __XINFLATE_GZIP_COMMENT; }
	else if ( (pState->iGzipFlags & 0x02u) != 0u ) { pState->eGzipState = __XINFLATE_GZIP_HEADER_CRC; }
	else {
		__xinflateCodecReset(pState, __XINFLATE_FORMAT_RAW);
		pState->eGzipState = __XINFLATE_GZIP_DEFLATE;
	}
}


static void __xinflateGzipAfterExtra(__xinflate* pState)
{
	pState->iGzipFlags &= (uint8)~0x04u;
	__xinflateGzipSelectOptional(pState);
}


static void __xinflateGzipAfterName(__xinflate* pState)
{
	pState->iGzipFlags &= (uint8)~0x08u;
	__xinflateGzipSelectOptional(pState);
}


static void __xinflateGzipAfterComment(__xinflate* pState)
{
	pState->iGzipFlags &= (uint8)~0x10u;
	__xinflateGzipSelectOptional(pState);
}


static __xinflate_result __xinflateWriteGzip(__xinflate* pState, const uint8* pData,
	size_t iLen, bool bFinal, __xinflate_output_fn pfnOutput, ptr pUserData)
{
	size_t iOffset = 0u;
	while ( iOffset < iLen ) {
		if ( pState->eGzipState == __XINFLATE_GZIP_BOUNDARY ) { __xinflateGzipResetHeader(pState); }
		if ( pState->eGzipState == __XINFLATE_GZIP_FIXED ) {
			uint8 iByte = pData[iOffset++];
			if ( !__xinflateGzipHeaderAdd(pState, iByte) ) { return __XINFLATE_ERROR; }
			pState->aGzipFixed[pState->iGzipFixedCount++] = iByte;
			if ( pState->iGzipFixedCount == 10u ) {
				if ( pState->aGzipFixed[0] != 0x1fu || pState->aGzipFixed[1] != 0x8bu ||
					pState->aGzipFixed[2] != 8u || (pState->aGzipFixed[3] & 0xe0u) != 0u ) {
					return __XINFLATE_ERROR;
				}
				pState->iGzipFlags = pState->aGzipFixed[3];
				__xinflateGzipSelectOptional(pState);
			}
			continue;
		}
		if ( pState->eGzipState == __XINFLATE_GZIP_EXTRA_LENGTH ) {
			uint8 iByte = pData[iOffset++];
			if ( !__xinflateGzipHeaderAdd(pState, iByte) ) { return __XINFLATE_ERROR; }
			pState->aGzipPair[pState->iGzipPairCount++] = iByte;
			if ( pState->iGzipPairCount == 2u ) {
				pState->iGzipExtraRemain = (uint32)pState->aGzipPair[0] |
					((uint32)pState->aGzipPair[1] << 8u);
				pState->iGzipPairCount = 0u;
				pState->eGzipState = pState->iGzipExtraRemain > 0u
					? __XINFLATE_GZIP_EXTRA : __XINFLATE_GZIP_NAME;
				if ( pState->iGzipExtraRemain == 0u ) { __xinflateGzipAfterExtra(pState); }
			}
			continue;
		}
		if ( pState->eGzipState == __XINFLATE_GZIP_EXTRA ) {
			uint8 iByte = pData[iOffset++];
			if ( !__xinflateGzipHeaderAdd(pState, iByte) ) { return __XINFLATE_ERROR; }
			if ( --pState->iGzipExtraRemain == 0u ) { __xinflateGzipAfterExtra(pState); }
			continue;
		}
		if ( pState->eGzipState == __XINFLATE_GZIP_NAME ||
			pState->eGzipState == __XINFLATE_GZIP_COMMENT ) {
			__xinflate_gzip_state eState = pState->eGzipState;
			uint8 iByte = pData[iOffset++];
			if ( !__xinflateGzipHeaderAdd(pState, iByte) ) { return __XINFLATE_ERROR; }
			if ( iByte == 0u ) {
				if ( eState == __XINFLATE_GZIP_NAME ) { __xinflateGzipAfterName(pState); }
				else { __xinflateGzipAfterComment(pState); }
			}
			continue;
		}
		if ( pState->eGzipState == __XINFLATE_GZIP_HEADER_CRC ) {
			if ( pState->iGzipHeaderBytes >= __XINFLATE_GZIP_HEADER_LIMIT ) { return __XINFLATE_ERROR; }
			pState->iGzipHeaderBytes++;
			pState->aGzipPair[pState->iGzipPairCount++] = pData[iOffset++];
			if ( pState->iGzipPairCount == 2u ) {
				uint32 iExpected = (uint32)pState->aGzipPair[0] | ((uint32)pState->aGzipPair[1] << 8u);
				if ( ((pState->iGzipHeaderCrc ^ UINT32_MAX) & 0xffffu) != iExpected ) { return __XINFLATE_ERROR; }
				pState->iGzipPairCount = 0u;
				pState->iGzipFlags &= (uint8)~0x02u;
				__xinflateCodecReset(pState, __XINFLATE_FORMAT_RAW);
				pState->eGzipState = __XINFLATE_GZIP_DEFLATE;
			}
			continue;
		}
		if ( pState->eGzipState == __XINFLATE_GZIP_DEFLATE ) {
			size_t iConsumed = 0u;
			bool bDone = false;
			__xinflate_result eResult = __xinflateFeedCodec(pState, pData + iOffset,
				iLen - iOffset, !bFinal, pfnOutput, pUserData, &iConsumed, &bDone);
			if ( eResult != __XINFLATE_OK ) { return eResult; }
			iOffset += iConsumed;
			if ( bDone ) { pState->eGzipState = __XINFLATE_GZIP_TRAILER; }
			else { break; }
			continue;
		}
		if ( pState->eGzipState == __XINFLATE_GZIP_TRAILER ) {
			pState->aGzipTrailer[pState->iGzipTrailerCount++] = pData[iOffset++];
			if ( pState->iGzipTrailerCount == 8u ) {
				uint32 iCrc = MZ_READ_LE32(pState->aGzipTrailer);
				uint32 iSize = MZ_READ_LE32(pState->aGzipTrailer + 4u);
				if ( iCrc != (pState->iGzipDataCrc ^ UINT32_MAX) || iSize != pState->iGzipDataSize ) {
					return __XINFLATE_ERROR;
				}
				pState->eGzipState = __XINFLATE_GZIP_BOUNDARY;
			}
			continue;
		}
	}
	if ( bFinal ) {
		if ( pState->eGzipState == __XINFLATE_GZIP_DEFLATE ) {
			size_t iConsumed = 0u;
			bool bDone = false;
			__xinflate_result eResult = __xinflateFeedCodec(pState, NULL, 0u, false,
				pfnOutput, pUserData, &iConsumed, &bDone);
			if ( eResult != __XINFLATE_OK || !bDone ) { return eResult == __XINFLATE_OK ? __XINFLATE_ERROR : eResult; }
			pState->eGzipState = __XINFLATE_GZIP_TRAILER;
		}
		if ( pState->eGzipState != __XINFLATE_GZIP_BOUNDARY ) { return __XINFLATE_ERROR; }
		pState->bDone = true;
		return __XINFLATE_DONE;
	}
	return __XINFLATE_OK;
}


static __xinflate_result __xinflateWrite(__xinflate* pState, const void* pData,
	size_t iLen, bool bFinal, __xinflate_output_fn pfnOutput, ptr pUserData)
{
	__xinflate_result eResult;
	if ( !pState || !pState->pDictionary || (!pData && iLen > 0u) || pState->bDone || pState->bFailed ) {
		return __XINFLATE_ERROR;
	}
	eResult = pState->eRequestedFormat == __XINFLATE_FORMAT_GZIP
		? __xinflateWriteGzip(pState, (const uint8*)pData, iLen, bFinal, pfnOutput, pUserData)
		: __xinflateWriteDeflate(pState, (const uint8*)pData, iLen, bFinal, pfnOutput, pUserData);
	if ( eResult == __XINFLATE_DONE ) { pState->bDone = true; }
	if ( eResult == __XINFLATE_ERROR || eResult == __XINFLATE_LIMIT ||
		eResult == __XINFLATE_CALLBACK_ABORT ) { pState->bFailed = true; }
	return eResult;
}


typedef struct {
	uint8* pData;
	size_t iLen;
	size_t iCap;
} __xinflate_pmd_output;


static bool __xinflatePmdOutput(ptr pUserData, const void* pData, size_t iLen)
{
	__xinflate_pmd_output* pOut = (__xinflate_pmd_output*)pUserData;
	uint8* pNew;
	size_t iNeed;
	size_t iCap;
	if ( !pOut || (!pData && iLen > 0u) ) { return false; }
	if ( iLen == 0u ) { return true; }
	if ( iLen > SIZE_MAX - pOut->iLen ) { return false; }
	iNeed = pOut->iLen + iLen;
	if ( iNeed > pOut->iCap ) {
		iCap = pOut->iCap > 0u ? pOut->iCap : 256u;
		while ( iCap < iNeed ) {
			if ( iCap > SIZE_MAX / 2u ) { iCap = iNeed; break; }
			iCap *= 2u;
		}
		pNew = (uint8*)XNET_ALLOC(iCap);
		if ( !pNew ) { return false; }
		if ( pOut->iLen > 0u ) { memcpy(pNew, pOut->pData, pOut->iLen); }
		XNET_FREE(pOut->pData);
		pOut->pData = pNew;
		pOut->iCap = iCap;
	}
	memcpy(pOut->pData + pOut->iLen, pData, iLen);
	pOut->iLen = iNeed;
	return true;
}


static __xinflate_result __xinflatePmdMessage(const void* pData, size_t iLen,
	uint64 iOutputLimit, void** ppOut, size_t* pOutLen)
{
	static const uint8 aTail[4] = { 0u, 0u, 0xffu, 0xffu };
	__xinflate tState;
	__xinflate_pmd_output tOut;
	__xinflate_result eResult;
	if ( ppOut ) { *ppOut = NULL; }
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( !ppOut || !pOutLen || (!pData && iLen > 0u) ) { return __XINFLATE_ERROR; }
	memset(&tOut, 0, sizeof(tOut));
	if ( !__xinflateInit(&tState, __XINFLATE_FORMAT_RAW, iOutputLimit) ) { return __XINFLATE_ERROR; }
	eResult = __xinflateWrite(&tState, pData, iLen, false, __xinflatePmdOutput, &tOut);
	if ( eResult == __XINFLATE_OK ) {
		eResult = __xinflateWrite(&tState, aTail, sizeof(aTail), false, __xinflatePmdOutput, &tOut);
	}
	__xinflateUnit(&tState);
	if ( eResult != __XINFLATE_OK ) {
		XNET_FREE(tOut.pData);
		return eResult;
	}
	*ppOut = tOut.pData;
	*pOutLen = tOut.iLen;
	return __XINFLATE_OK;
}

#endif
