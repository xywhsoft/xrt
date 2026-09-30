#include "../internal/xrt_vfs.h"
#include "../internal/xrt_compress.h"
#include "../third_party/lzma/LzmaDec.h"

#include <stdlib.h>



#if defined(XRT_FEATURE_VFS_PACK)

SRes __xrtLzmaDecode(Byte* pDestination, SizeT* pDestinationSize,
	const Byte* pSource, SizeT* pSourceSize,
	const Byte* pProperties, unsigned iPropertiesSize,
	ELzmaFinishMode FinishMode, ELzmaStatus* pStatus,
	ISzAllocPtr pAllocator);

#define XRT_VFS_PACK_HEADER_SIZE 80u
#define XRT_VFS_PACK_TRAILER_SIZE 32u
#define XRT_VFS_PACK_RECORD_SIZE 40u
#define XRT_VFS_PACK_LEGACY_HEADER_SIZE 64u
#define XRT_VFS_PACK_LEGACY_TRAILER_SIZE 24u
#define XRT_VFS_PACK_LEGACY_RECORD_SIZE 26u

typedef enum xrt_vfs_pack_state {
	XRT_VFS_PACK_UNLOADED = 0,
	XRT_VFS_PACK_LOADING,
	XRT_VFS_PACK_READY,
	XRT_VFS_PACK_FAILED
} xrt_vfs_pack_state;

typedef struct xrt_vfs_pack_blob {
	volatile int32 RefCount;
	bytes Data;
	size_t Size;
} xrt_vfs_pack_blob;

typedef struct xrt_vfs_pack_entry {
	str Path;
	size_t PathSize;
	uint64 DataOffset;
	uint64 StoredSize;
	uint64 OriginalSize;
	uint32 OriginalCrc32;
	uint8 Codec;
	xmutex Lock;
	xcond Ready;
	bool SyncReady;
	xrt_vfs_pack_state State;
	size_t Waiters;
	xrt_vfs_pack_blob* Blob;
	xerror* Failure;
	bool Cached;
	uint64 LastUse;
} xrt_vfs_pack_entry;

typedef struct xrt_vfs_pack_node {
	str Path;
	size_t PathSize;
	size_t ParentSize;
	size_t NameOffset;
	bool Directory;
	size_t Entry;
} xrt_vfs_pack_node;

typedef struct xrt_vfs_pack_file {
	xrt_vfs_pack_blob* Blob;
	uint64 Cursor;
} xrt_vfs_pack_file;

typedef struct xrt_vfs_pack_dir {
	xvfspack Pack;
	size_t* Items;
	size_t Count;
	size_t Position;
} xrt_vfs_pack_dir;

struct xvfs_pack_impl {
	volatile int32 RefCount;
	xfile Source;
	uint64 Offset;
	uint64 Length;
	xvfspackformat Format;
	xvfspackoptions Options;
	xrt_vfs_pack_entry* Entries;
	size_t EntryCount;
	xrt_vfs_pack_node* Nodes;
	xrt_vfs_pack_node** Folded;
	size_t NodeCount;
	bool FoldCollision;
	xmutex CacheLock;
	bool CacheLockReady;
	xvfspackstats Stats;
	uint64 CacheClock;
};



static const uint8 __xrtVfsPackHeaderMagic[8] = {
	'X', 'R', 'T', 'P', 'A', 'C', 'K', 0
};
static const uint8 __xrtVfsPackTrailerMagic[8] = {
	'X', 'R', 'T', 'P', 'E', 'N', 'D', 0
};
static const uint8 __xrtVfsPackLegacyHeaderMagic[8] = {
	'X', 'S', 'V', 'F', 'H', 'D', 'R', 0
};
static const uint8 __xrtVfsPackLegacyTrailerMagic[8] = {
	'X', 'S', 'V', 'P', 'A', 'C', 'K', 0
};



static uint16 __xrtVfsPackU16(const uint8* pData)
{
	return (uint16)((uint16)pData[0] | ((uint16)pData[1] << 8u));
}

static uint32 __xrtVfsPackU32(const uint8* pData)
{
	return (uint32)pData[0] |
		((uint32)pData[1] << 8u) |
		((uint32)pData[2] << 16u) |
		((uint32)pData[3] << 24u);
}

static uint64 __xrtVfsPackU64(const uint8* pData)
{
	return (uint64)__xrtVfsPackU32(pData) |
		((uint64)__xrtVfsPackU32(pData + 4u) << 32u);
}

static bool __xrtVfsPackAdd(uint64 iLeft, uint64 iRight, uint64* pOutput)
{
	if ( iLeft > UINT64_MAX - iRight ) return false;
	*pOutput = iLeft + iRight;
	return true;
}

static uint32 __xrtVfsPackCrc(const void* pData, size_t iSize)
{
	return __xrtCompressCrc32Update(UINT32_MAX, pData, iSize) ^ UINT32_MAX;
}

static uint32 __xrtVfsPackCrcPair(const void* pFirst, size_t iFirst,
	const void* pSecond, size_t iSecond)
{
	uint32 iCrc = __xrtCompressCrc32Update(UINT32_MAX, pFirst, iFirst);
	iCrc = __xrtCompressCrc32Update(iCrc, pSecond, iSecond);
	return iCrc ^ UINT32_MAX;
}

static bool __xrtVfsPackCorrupt(cstr sOperation, cstr sMessage)
{
	__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_CORRUPT,
		sOperation, sMessage);
	return false;
}

static bool __xrtVfsPackChecksum(cstr sOperation, cstr sMessage)
{
	__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_CHECKSUM,
		sOperation, sMessage);
	return false;
}

static bool __xrtVfsPackLimit(cstr sOperation, cstr sMessage)
{
	__xrtVfsError(XERR_RANGE, XVFS_ERROR_LIMIT,
		sOperation, sMessage);
	return false;
}



static void __xrtVfsPackBlobRef(xrt_vfs_pack_blob* pBlob)
{
	if ( xrtRefRetain(&pBlob->RefCount) < 0 ) abort();
}

static void __xrtVfsPackBlobRelease(xrt_vfs_pack_blob* pBlob)
{
	if ( (pBlob == NULL) || (xrtRefRelease(&pBlob->RefCount) != 0) ) return;
	xrtFree(pBlob->Data);
	xrtFree(pBlob);
}



static bool __xrtVfsPackReadSource(xfile Source,
	uint64 iBase, uint64 iLength, uint64 iRelative,
	void* pData, size_t iSize, cstr sOperation)
{
	uint64 iAbsolute;
	size_t iRead = 0u;

	if ( (iRelative > iLength) ||
		((uint64)iSize > iLength - iRelative) ||
		!__xrtVfsPackAdd(iBase, iRelative, &iAbsolute) ) {
		return __xrtVfsPackCorrupt(sOperation,
			"the archive range is truncated or overflows");
	}
	if ( iSize == 0u ) return true;
	if ( !xrtReadAtFull(Source, iAbsolute, pData, iSize, &iRead) ) return false;
	if ( iRead != iSize ) {
		return __xrtVfsPackCorrupt(sOperation,
			"the archive source ended before the declared range");
	}
	return true;
}



static bool __xrtVfsPackPath(xstrview Path)
{
	if ( xrtUtf8Valid(Path, NULL) && __xrtVfsRelativePath(Path, false) )
		return true;
	xrtClearError();
	return __xrtVfsPackCorrupt("pack-index",
		"the archive contains a non-canonical UTF-8 path");
}

static bool __xrtVfsPackPathAfter(const xrt_vfs_pack_entry* pPrevious,
	xstrview Path)
{
	if ( (pPrevious == NULL) ||
		(xrtStrCompare((xstrview){ pPrevious->Path,
			pPrevious->PathSize }, Path) < 0) ) return true;
	return __xrtVfsPackCorrupt("pack-index",
		"archive paths are duplicated or not strictly sorted");
}



static bool __xrtVfsPackEntrySet(xrt_vfs_pack_entry* pEntry,
	xstrview Path, uint8 iCodec, uint64 iDataOffset,
	uint64 iStoredSize, uint64 iOriginalSize, uint32 iCrc)
{
	pEntry->Path = xrtStrDupView(Path);
	if ( pEntry->Path == NULL ) return false;
	pEntry->PathSize = Path.Size;
	pEntry->Codec = iCodec;
	pEntry->DataOffset = iDataOffset;
	pEntry->StoredSize = iStoredSize;
	pEntry->OriginalSize = iOriginalSize;
	pEntry->OriginalCrc32 = iCrc;
	return true;
}

static bool __xrtVfsPackEntryLimits(const struct xvfs_pack_impl* pPack,
	uint8 iCodec, uint64 iStoredSize, uint64 iOriginalSize,
	uint64* pDecoded)
{
	uint64 iTotal;

	if ( (iCodec != XVFS_PACK_STORE) && (iCodec != XVFS_PACK_LZMA1) )
		return __xrtVfsPackCorrupt("pack-index",
			"the archive uses an unsupported codec");
	if ( iOriginalSize > pPack->Options.MaxEntryBytes )
		return __xrtVfsPackLimit("pack-index",
			"an archive entry exceeds the decoded size limit");
	if ( (iStoredSize > (uint64)SIZE_MAX) ||
		(iOriginalSize > (uint64)SIZE_MAX) )
		return __xrtVfsPackLimit("pack-index",
			"an archive entry is not addressable on this platform");
	if ( (iCodec == XVFS_PACK_STORE) && (iStoredSize != iOriginalSize) )
		return __xrtVfsPackCorrupt("pack-index",
			"a STORE entry has different stored and original sizes");
	if ( (iCodec == XVFS_PACK_LZMA1) &&
		((iStoredSize < LZMA_PROPS_SIZE) || (iOriginalSize == 0u)) )
		return __xrtVfsPackCorrupt("pack-index",
			"an LZMA1 entry has invalid lengths");
	if ( !__xrtVfsPackAdd(*pDecoded, iOriginalSize, &iTotal) ||
		(iTotal > pPack->Options.MaxDecodedBytes) )
		return __xrtVfsPackLimit("pack-index",
			"the archive exceeds the total decoded size limit");
	*pDecoded = iTotal;
	return true;
}



static bool __xrtVfsPackEntriesAllocate(struct xvfs_pack_impl* pPack,
	uint32 iCount)
{
	if ( iCount > pPack->Options.MaxEntries )
		return __xrtVfsPackLimit("pack-header",
			"the archive entry count exceeds the configured limit");
	if ( (iCount != 0u) &&
		(sizeof(pPack->Entries[0]) > SIZE_MAX / (size_t)iCount) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( iCount != 0u ) {
		pPack->Entries = (xrt_vfs_pack_entry*)xrtCalloc(
			(size_t)iCount, sizeof(pPack->Entries[0]));
		if ( pPack->Entries == NULL ) return false;
	}
	pPack->EntryCount = (size_t)iCount;
	return true;
}



static bool __xrtVfsPackParseXrt(struct xvfs_pack_impl* pPack, xfile Source,
	const uint8 arrHeader[XRT_VFS_PACK_HEADER_SIZE])
{
	uint8 arrTrailer[XRT_VFS_PACK_TRAILER_SIZE];
	uint32 iEntryCount;
	uint64 iDataOffset;
	uint64 iDataSize;
	uint64 iIndexOffset;
	uint64 iIndexSize;
	uint64 iArchiveSize;
	uint64 iDecodedHeader;
	uint64 iIndexEnd;
	uint64 iExpectedEnd;
	bytes pIndex = NULL;
	uint64 iPosition = 0u;
	uint64 iDataCursor;
	uint64 iDecoded = 0u;
	bool bResult = false;

	if ( (__xrtVfsPackU16(arrHeader + 8u) != 1u) ||
		(__xrtVfsPackU16(arrHeader + 10u) != 0u) ||
		(__xrtVfsPackU32(arrHeader + 12u) != XRT_VFS_PACK_HEADER_SIZE) ||
		(__xrtVfsPackU32(arrHeader + 16u) != 0u) ||
		(__xrtVfsPackU32(arrHeader + 76u) != 0u) )
		return __xrtVfsPackCorrupt("pack-header",
			"the XRT pack header version, size, flags, or reserved fields are invalid");
	iEntryCount = __xrtVfsPackU32(arrHeader + 20u);
	iDataOffset = __xrtVfsPackU64(arrHeader + 24u);
	iDataSize = __xrtVfsPackU64(arrHeader + 32u);
	iIndexOffset = __xrtVfsPackU64(arrHeader + 40u);
	iIndexSize = __xrtVfsPackU64(arrHeader + 48u);
	iArchiveSize = __xrtVfsPackU64(arrHeader + 56u);
	iDecodedHeader = __xrtVfsPackU64(arrHeader + 64u);
	if ( (iDataOffset != XRT_VFS_PACK_HEADER_SIZE) ||
		!__xrtVfsPackAdd(iDataOffset, iDataSize, &iExpectedEnd) ||
		(iExpectedEnd != iIndexOffset) ||
		!__xrtVfsPackAdd(iIndexOffset, iIndexSize, &iIndexEnd) ||
		!__xrtVfsPackAdd(iIndexEnd, XRT_VFS_PACK_TRAILER_SIZE,
			&iExpectedEnd) ||
		(iExpectedEnd != iArchiveSize) || (iArchiveSize != pPack->Length) )
		return __xrtVfsPackCorrupt("pack-header",
			"the XRT pack physical layout is not exact");
	if ( (iIndexSize > pPack->Options.MaxIndexBytes) ||
		(iIndexSize > (uint64)SIZE_MAX) )
		return __xrtVfsPackLimit("pack-index",
			"the archive index exceeds the configured limit");
	if ( iDecodedHeader > pPack->Options.MaxDecodedBytes )
		return __xrtVfsPackLimit("pack-header",
			"the archive decoded size exceeds the configured limit");
	if ( !__xrtVfsPackReadSource(Source, pPack->Offset, pPack->Length,
		iIndexEnd, arrTrailer, sizeof(arrTrailer), "pack-trailer") ) return false;
	if ( (memcmp(arrTrailer, __xrtVfsPackTrailerMagic, 8u) != 0) ||
		(__xrtVfsPackU64(arrTrailer + 8u) != iArchiveSize) ||
		(__xrtVfsPackU64(arrTrailer + 16u) != 0u) ||
		(__xrtVfsPackU32(arrTrailer + 28u) != 0u) )
		return __xrtVfsPackCorrupt("pack-trailer",
			"the XRT pack trailer is invalid");
	if ( __xrtVfsPackCrcPair(arrTrailer, 24u, arrHeader,
		XRT_VFS_PACK_HEADER_SIZE) != __xrtVfsPackU32(arrTrailer + 24u) )
		return __xrtVfsPackChecksum("pack-trailer",
			"the XRT pack metadata checksum is invalid");
	if ( !__xrtVfsPackEntriesAllocate(pPack, iEntryCount) ) return false;
	if ( iIndexSize != 0u ) {
		pIndex = (bytes)xrtMalloc((size_t)iIndexSize);
		if ( pIndex == NULL ) return false;
		if ( !__xrtVfsPackReadSource(Source, pPack->Offset, pPack->Length,
			iIndexOffset, pIndex, (size_t)iIndexSize, "pack-index") ) goto done;
	}
	if ( __xrtVfsPackCrc(pIndex, (size_t)iIndexSize) !=
		__xrtVfsPackU32(arrHeader + 72u) ) {
		(void)__xrtVfsPackChecksum("pack-index",
			"the archive index checksum is invalid");
		goto done;
	}
	iDataCursor = iDataOffset;
	for ( size_t i = 0u; i < pPack->EntryCount; i++ ) {
		xrt_vfs_pack_entry* pEntry = &pPack->Entries[i];
		uint16 iPathSize;
		uint8 iCodec;
		uint64 iData;
		uint64 iStored;
		uint64 iOriginal;
		xstrview Path;

		if ( (iPosition > iIndexSize) ||
			(iIndexSize - iPosition < XRT_VFS_PACK_RECORD_SIZE) ) {
			(void)__xrtVfsPackCorrupt("pack-index",
				"the archive index record is truncated");
			goto done;
		}
		iPathSize = __xrtVfsPackU16(pIndex + (size_t)iPosition);
		iCodec = pIndex[(size_t)iPosition + 2u];
		if ( (iPathSize == 0u) ||
			(pIndex[(size_t)iPosition + 3u] != 0u) ||
			(__xrtVfsPackU32(pIndex + (size_t)iPosition + 4u) != 0u) ||
			(__xrtVfsPackU32(pIndex + (size_t)iPosition + 36u) != 0u) ||
			((uint64)iPathSize > iIndexSize - iPosition -
				XRT_VFS_PACK_RECORD_SIZE) ) {
			(void)__xrtVfsPackCorrupt("pack-index",
				"the archive index record fields are invalid");
			goto done;
		}
		iData = __xrtVfsPackU64(pIndex + (size_t)iPosition + 8u);
		iStored = __xrtVfsPackU64(pIndex + (size_t)iPosition + 16u);
		iOriginal = __xrtVfsPackU64(pIndex + (size_t)iPosition + 24u);
		Path.Data = (cstr)(pIndex + (size_t)iPosition +
			XRT_VFS_PACK_RECORD_SIZE);
		Path.Size = iPathSize;
		if ( (iData != iDataCursor) ||
			!__xrtVfsPackAdd(iData, iStored, &iDataCursor) ||
			(iDataCursor > iIndexOffset) || !__xrtVfsPackPath(Path) ||
			!__xrtVfsPackPathAfter(i == 0u ? NULL :
				&pPack->Entries[i - 1u], Path) ||
			!__xrtVfsPackEntryLimits(pPack, iCodec, iStored,
				iOriginal, &iDecoded) ||
			!__xrtVfsPackEntrySet(pEntry, Path, iCodec, iData,
				iStored, iOriginal,
				__xrtVfsPackU32(pIndex + (size_t)iPosition + 32u)) ) goto done;
		iPosition += XRT_VFS_PACK_RECORD_SIZE + (uint64)iPathSize;
	}
	if ( (iPosition != iIndexSize) || (iDataCursor != iIndexOffset) ||
		(iDecoded != iDecodedHeader) ) {
		(void)__xrtVfsPackCorrupt("pack-index",
			"the archive index does not exactly describe the data region");
		goto done;
	}
	bResult = true;

done:
	xrtFree(pIndex);
	return bResult;
}



static bool __xrtVfsPackParseLegacy(struct xvfs_pack_impl* pPack, xfile Source,
	const uint8 arrHeader[XRT_VFS_PACK_LEGACY_HEADER_SIZE])
{
	uint8 arrTrailer[XRT_VFS_PACK_LEGACY_TRAILER_SIZE];
	uint32 iEntryCount;
	uint64 iDataOffset;
	uint64 iIndexOffset;
	uint64 iArchiveSize;
	uint64 iIndexSize;
	uint64 iIndexEnd;
	bytes pIndex = NULL;
	uint64 iPosition = 0u;
	uint64 iDataCursor;
	uint64 iDecoded = 0u;
	bool bResult = false;

	if ( (__xrtVfsPackU16(arrHeader + 8u) != 1u) ||
		(__xrtVfsPackU16(arrHeader + 10u) !=
			XRT_VFS_PACK_LEGACY_HEADER_SIZE) ||
		(__xrtVfsPackU32(arrHeader + 44u) != 0u) ||
		(__xrtVfsPackU64(arrHeader + 48u) != 0u) ||
		(__xrtVfsPackU64(arrHeader + 56u) != 0u) )
		return __xrtVfsPackCorrupt("pack-header",
			"the XSVPACK header version, size, flags, or reserved fields are invalid");
	iEntryCount = __xrtVfsPackU32(arrHeader + 12u);
	iDataOffset = __xrtVfsPackU64(arrHeader + 16u);
	iIndexOffset = __xrtVfsPackU64(arrHeader + 24u);
	iArchiveSize = __xrtVfsPackU64(arrHeader + 32u);
	if ( (iEntryCount == 0u) ||
		(iDataOffset != XRT_VFS_PACK_LEGACY_HEADER_SIZE) ||
		(iIndexOffset < iDataOffset) ||
		(iArchiveSize != pPack->Length) ||
		(iArchiveSize < XRT_VFS_PACK_LEGACY_HEADER_SIZE +
			XRT_VFS_PACK_LEGACY_TRAILER_SIZE) )
		return __xrtVfsPackCorrupt("pack-header",
			"the XSVPACK physical layout is invalid");
	iIndexEnd = iArchiveSize - XRT_VFS_PACK_LEGACY_TRAILER_SIZE;
	if ( iIndexOffset > iIndexEnd )
		return __xrtVfsPackCorrupt("pack-header",
			"the XSVPACK index lies outside the archive");
	iIndexSize = iIndexEnd - iIndexOffset;
	if ( iIndexSize == 0u )
		return __xrtVfsPackCorrupt("pack-index",
			"the XSVPACK index is empty");
	if ( (iIndexSize > pPack->Options.MaxIndexBytes) ||
		(iIndexSize > (uint64)SIZE_MAX) )
		return __xrtVfsPackLimit("pack-index",
			"the archive index exceeds the configured limit");
	if ( !__xrtVfsPackReadSource(Source, pPack->Offset, pPack->Length,
		iIndexEnd, arrTrailer, sizeof(arrTrailer), "pack-trailer") ) return false;
	if ( (memcmp(arrTrailer, __xrtVfsPackLegacyTrailerMagic, 8u) != 0) ||
		(__xrtVfsPackU64(arrTrailer + 8u) != pPack->Offset) ||
		(__xrtVfsPackU32(arrTrailer + 20u) != 0u) )
		return __xrtVfsPackCorrupt("pack-trailer",
			"the XSVPACK trailer is invalid for the supplied range");
	if ( __xrtVfsPackCrcPair(arrTrailer, 16u, arrHeader,
		XRT_VFS_PACK_LEGACY_HEADER_SIZE) !=
		__xrtVfsPackU32(arrTrailer + 16u) )
		return __xrtVfsPackChecksum("pack-trailer",
			"the XSVPACK metadata checksum is invalid");
	if ( !__xrtVfsPackEntriesAllocate(pPack, iEntryCount) ) return false;
	pIndex = (bytes)xrtMalloc((size_t)iIndexSize);
	if ( pIndex == NULL ) return false;
	if ( !__xrtVfsPackReadSource(Source, pPack->Offset, pPack->Length,
		iIndexOffset, pIndex, (size_t)iIndexSize, "pack-index") ) goto done;
	if ( __xrtVfsPackCrc(pIndex, (size_t)iIndexSize) !=
		__xrtVfsPackU32(arrHeader + 40u) ) {
		(void)__xrtVfsPackChecksum("pack-index",
			"the XSVPACK index checksum is invalid");
		goto done;
	}
	iDataCursor = iDataOffset;
	for ( size_t i = 0u; i < pPack->EntryCount; i++ ) {
		xrt_vfs_pack_entry* pEntry = &pPack->Entries[i];
		uint16 iPathSize;
		uint8 iCodec;
		uint64 iData;
		uint64 iStored;
		uint64 iOriginal;
		xstrview Path;
		uint64 iFixed;

		if ( (iPosition > iIndexSize) || (iIndexSize - iPosition < 2u) ) {
			(void)__xrtVfsPackCorrupt("pack-index",
				"the XSVPACK index record is truncated");
			goto done;
		}
		iPathSize = __xrtVfsPackU16(pIndex + (size_t)iPosition);
		iFixed = 2u + (uint64)iPathSize + XRT_VFS_PACK_LEGACY_RECORD_SIZE;
		if ( (iPathSize == 0u) || (iFixed > iIndexSize - iPosition) ) {
			(void)__xrtVfsPackCorrupt("pack-index",
				"the XSVPACK path or record size is invalid");
			goto done;
		}
		Path.Data = (cstr)(pIndex + (size_t)iPosition + 2u);
		Path.Size = iPathSize;
		iPosition += 2u + (uint64)iPathSize;
		iCodec = pIndex[(size_t)iPosition];
		if ( pIndex[(size_t)iPosition + 1u] != 0u ) {
			(void)__xrtVfsPackCorrupt("pack-index",
				"the XSVPACK entry flags are not zero");
			goto done;
		}
		iData = __xrtVfsPackU64(pIndex + (size_t)iPosition + 2u);
		iStored = __xrtVfsPackU32(pIndex + (size_t)iPosition + 10u);
		iOriginal = __xrtVfsPackU64(pIndex + (size_t)iPosition + 14u);
		if ( (iData != iDataCursor) ||
			!__xrtVfsPackAdd(iData, iStored, &iDataCursor) ||
			(iDataCursor > iIndexOffset) || !__xrtVfsPackPath(Path) ||
			!__xrtVfsPackPathAfter(i == 0u ? NULL :
				&pPack->Entries[i - 1u], Path) ||
			!__xrtVfsPackEntryLimits(pPack, iCodec, iStored,
				iOriginal, &iDecoded) ||
			!__xrtVfsPackEntrySet(pEntry, Path, iCodec, iData,
				iStored, iOriginal,
				__xrtVfsPackU32(pIndex + (size_t)iPosition + 22u)) ) goto done;
		iPosition += XRT_VFS_PACK_LEGACY_RECORD_SIZE;
	}
	if ( (iPosition != iIndexSize) || (iDataCursor != iIndexOffset) ) {
		(void)__xrtVfsPackCorrupt("pack-index",
			"the XSVPACK index does not exactly describe the data region");
		goto done;
	}
	bResult = true;

done:
	xrtFree(pIndex);
	return bResult;
}



static bool __xrtVfsPackEntriesSync(struct xvfs_pack_impl* pPack)
{
	for ( size_t i = 0u; i < pPack->EntryCount; i++ ) {
		xrt_vfs_pack_entry* pEntry = &pPack->Entries[i];

		if ( !xrtMutexInit(&pEntry->Lock) ) return false;
		if ( !xrtCondInit(&pEntry->Ready) ) {
			(void)xrtMutexUnit(&pEntry->Lock);
			return false;
		}
		pEntry->SyncReady = true;
	}
	return true;
}



static bool __xrtVfsPackNodeAdd(xrt_vfs_pack_node* pNodes,
	size_t* pCount, xstrview Path, bool bDirectory, size_t iEntry)
{
	xrt_vfs_pack_node* pNode = &pNodes[*pCount];
	size_t iSlash = SIZE_MAX;

	pNode->Path = xrtStrDupView(Path);
	if ( pNode->Path == NULL ) return false;
	pNode->PathSize = Path.Size;
	for ( size_t i = 0u; i < Path.Size; i++ )
		if ( Path.Data[i] == '/' ) iSlash = i;
	pNode->ParentSize = (Path.Size == 0u) ? SIZE_MAX :
		((iSlash == SIZE_MAX) ? 0u : iSlash);
	pNode->NameOffset = (iSlash == SIZE_MAX) ? 0u : iSlash + 1u;
	pNode->Directory = bDirectory;
	pNode->Entry = iEntry;
	(*pCount)++;
	return true;
}

static int __xrtVfsPackNodeCompare(const void* pLeft, const void* pRight)
{
	const xrt_vfs_pack_node* pA = (const xrt_vfs_pack_node*)pLeft;
	const xrt_vfs_pack_node* pB = (const xrt_vfs_pack_node*)pRight;
	return xrtStrCompare((xstrview){ pA->Path, pA->PathSize },
		(xstrview){ pB->Path, pB->PathSize });
}

static int __xrtVfsPackFoldCompare(const void* pLeft, const void* pRight)
{
	const xrt_vfs_pack_node* pA = *(xrt_vfs_pack_node* const*)pLeft;
	const xrt_vfs_pack_node* pB = *(xrt_vfs_pack_node* const*)pRight;
	int iResult = xrtStrCaseCompare(
		(xstrview){ pA->Path, pA->PathSize },
		(xstrview){ pB->Path, pB->PathSize });
	return iResult != 0 ? iResult : __xrtVfsPackNodeCompare(pA, pB);
}

static bool __xrtVfsPackNodesBuild(struct xvfs_pack_impl* pPack)
{
	size_t iCandidates = 1u;
	size_t iCount = 0u;
	size_t iOutput = 0u;

	for ( size_t i = 0u; i < pPack->EntryCount; i++ ) {
		if ( iCandidates == SIZE_MAX ) goto overflow;
		iCandidates++;
		for ( size_t j = 0u; j < pPack->Entries[i].PathSize; j++ ) {
			if ( pPack->Entries[i].Path[j] != '/' ) continue;
			if ( iCandidates == SIZE_MAX ) goto overflow;
			iCandidates++;
		}
	}
	if ( iCandidates > SIZE_MAX / sizeof(pPack->Nodes[0]) ) goto overflow;
	pPack->Nodes = (xrt_vfs_pack_node*)xrtCalloc(
		iCandidates, sizeof(pPack->Nodes[0]));
	if ( pPack->Nodes == NULL ) return false;
	if ( !__xrtVfsPackNodeAdd(pPack->Nodes, &iCount,
		XRT_STR_LITERAL(""), true, SIZE_MAX) ) return false;
	pPack->NodeCount = iCount;
	for ( size_t i = 0u; i < pPack->EntryCount; i++ ) {
		xrt_vfs_pack_entry* pEntry = &pPack->Entries[i];

		for ( size_t j = 0u; j < pEntry->PathSize; j++ ) {
			if ( pEntry->Path[j] != '/' ) continue;
			if ( !__xrtVfsPackNodeAdd(pPack->Nodes, &iCount,
				(xstrview){ pEntry->Path, j }, true, SIZE_MAX) ) return false;
			pPack->NodeCount = iCount;
		}
		if ( !__xrtVfsPackNodeAdd(pPack->Nodes, &iCount,
			(xstrview){ pEntry->Path, pEntry->PathSize }, false, i) ) return false;
		pPack->NodeCount = iCount;
	}
	qsort(pPack->Nodes, iCount, sizeof(pPack->Nodes[0]),
		__xrtVfsPackNodeCompare);
	for ( size_t i = 0u; i < iCount; i++ ) {
		if ( (iOutput != 0u) && xrtStrEqual(
			(xstrview){ pPack->Nodes[iOutput - 1u].Path,
				pPack->Nodes[iOutput - 1u].PathSize },
			(xstrview){ pPack->Nodes[i].Path,
				pPack->Nodes[i].PathSize }) ) {
			if ( pPack->Nodes[iOutput - 1u].Directory !=
				pPack->Nodes[i].Directory )
				return __xrtVfsPackCorrupt("pack-index",
					"an archive path is both a file and a directory");
			xrtFree(pPack->Nodes[i].Path);
			pPack->Nodes[i].Path = NULL;
			continue;
		}
		if ( iOutput != i ) {
			pPack->Nodes[iOutput] = pPack->Nodes[i];
			memset(&pPack->Nodes[i], 0, sizeof(pPack->Nodes[i]));
		}
		iOutput++;
	}
	pPack->NodeCount = iOutput;
	pPack->Folded = (xrt_vfs_pack_node**)xrtMalloc(
		iOutput * sizeof(pPack->Folded[0]));
	if ( pPack->Folded == NULL ) return false;
	for ( size_t i = 0u; i < iOutput; i++ )
		pPack->Folded[i] = &pPack->Nodes[i];
	qsort(pPack->Folded, iOutput, sizeof(pPack->Folded[0]),
		__xrtVfsPackFoldCompare);
	for ( size_t i = 1u; i < iOutput; i++ ) {
		if ( xrtStrCaseEqual(
			(xstrview){ pPack->Folded[i - 1u]->Path,
				pPack->Folded[i - 1u]->PathSize },
			(xstrview){ pPack->Folded[i]->Path,
				pPack->Folded[i]->PathSize }) ) {
			pPack->FoldCollision = true;
			break;
		}
	}
	return true;

overflow:
	__xrtErrorSetSizeOverflow();
	return false;
}



static void __xrtVfsPackReleaseMembers(struct xvfs_pack_impl* pPack,
	bool bCloseSource)
{
	if ( pPack == NULL ) return;
	if ( pPack->Entries != NULL ) {
		for ( size_t i = 0u; i < pPack->EntryCount; i++ ) {
			xrt_vfs_pack_entry* pEntry = &pPack->Entries[i];

			xrtFree(pEntry->Path);
			__xrtVfsPackBlobRelease(pEntry->Blob);
			xrtErrorFree(pEntry->Failure);
			if ( pEntry->SyncReady ) {
				if ( !xrtCondUnit(&pEntry->Ready) ) abort();
				if ( !xrtMutexUnit(&pEntry->Lock) ) abort();
			}
		}
	}
	if ( pPack->Nodes != NULL ) {
		for ( size_t i = 0u; i < pPack->NodeCount; i++ )
			xrtFree(pPack->Nodes[i].Path);
	}
	xrtFree(pPack->Folded);
	xrtFree(pPack->Nodes);
	xrtFree(pPack->Entries);
	if ( pPack->CacheLockReady && !xrtMutexUnit(&pPack->CacheLock) ) abort();
	if ( bCloseSource && (pPack->Source != NULL) ) {
		xerror* pBefore = xrtTakeError();

		(void)xrtClose(pPack->Source);
		xrtClearError();
		if ( pBefore != NULL ) __xrtErrorSetOwned(pBefore);
	}
}



XRT_API void xrtVfsPackOptionsInit(xvfspackoptions* pOptions)
{
	if ( pOptions == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->Size = (uint32)sizeof(*pOptions);
	pOptions->Version = XRT_VFS_PACK_OPTIONS_VERSION;
	pOptions->Format = XVFS_PACK_AUTO;
	pOptions->MaxEntries = XRT_VFS_PACK_MAX_ENTRIES_DEFAULT;
	pOptions->MaxIndexBytes = XRT_VFS_PACK_MAX_INDEX_BYTES_DEFAULT;
	pOptions->MaxEntryBytes = XRT_VFS_PACK_MAX_ENTRY_BYTES_DEFAULT;
	pOptions->MaxDecodedBytes = XRT_VFS_PACK_MAX_DECODED_BYTES_DEFAULT;
	pOptions->CacheBytes = XRT_VFS_PACK_CACHE_BYTES_DEFAULT;
}

static bool __xrtVfsPackOptions(const xvfspackoptions* pInput,
	xvfspackoptions* pOutput)
{
	if ( pInput == NULL ) {
		xrtVfsPackOptionsInit(pOutput);
		return true;
	}
	if ( (pInput->Size < sizeof(*pInput)) ||
		(pInput->Version != XRT_VFS_PACK_OPTIONS_VERSION) ||
		((pInput->Format != XVFS_PACK_AUTO) &&
		 (pInput->Format != XVFS_PACK_XRT_V1) &&
		 (pInput->Format != XVFS_PACK_XSVPACK_V1)) ||
		(pInput->MaxEntries == 0u) || (pInput->MaxIndexBytes == 0u) ||
		(pInput->MaxEntryBytes == 0u) ||
		(pInput->MaxDecodedBytes == 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pOutput = *pInput;
	pOutput->Size = (uint32)sizeof(*pOutput);
	return true;
}

XRT_API xvfspack xrtVfsPackCreate(xfile Source,
	uint64 iOffset, uint64 iLength, const xvfspackoptions* pOptions)
{
	xvfspack Pack;
	uint8 arrHeader[XRT_VFS_PACK_HEADER_SIZE];
	size_t iHeaderSize;
	xvfspackformat Format;

	if ( (Source == NULL) || (iLength <
		XRT_VFS_PACK_LEGACY_HEADER_SIZE +
		XRT_VFS_PACK_LEGACY_TRAILER_SIZE) ||
		((xrtFileFlags(Source) & XFILE_READ) == 0u) ||
		((xrtFileFlags(Source) & XFILE_ASYNC) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	Pack = (xvfspack)xrtCalloc(1u, sizeof(*Pack));
	if ( Pack == NULL ) return NULL;
	Pack->RefCount = 1;
	Pack->Offset = iOffset;
	Pack->Length = iLength;
	if ( !__xrtVfsPackOptions(pOptions, &Pack->Options) ) goto fail;
	iHeaderSize = iLength >= XRT_VFS_PACK_HEADER_SIZE ?
		XRT_VFS_PACK_HEADER_SIZE : XRT_VFS_PACK_LEGACY_HEADER_SIZE;
	if ( !__xrtVfsPackReadSource(Source, iOffset, iLength, 0u,
		arrHeader, iHeaderSize, "pack-header") ) goto fail;
	Format = Pack->Options.Format;
	if ( Format == XVFS_PACK_AUTO ) {
		if ( memcmp(arrHeader, __xrtVfsPackHeaderMagic, 8u) == 0 )
			Format = XVFS_PACK_XRT_V1;
		else if ( memcmp(arrHeader,
			__xrtVfsPackLegacyHeaderMagic, 8u) == 0 )
			Format = XVFS_PACK_XSVPACK_V1;
		else {
			(void)__xrtVfsPackCorrupt("pack-header",
				"the archive header magic is unknown");
			goto fail;
		}
	}
	Pack->Format = Format;
	if ( Format == XVFS_PACK_XRT_V1 ) {
		if ( (iHeaderSize != XRT_VFS_PACK_HEADER_SIZE) ||
			(memcmp(arrHeader, __xrtVfsPackHeaderMagic, 8u) != 0) ) {
			(void)__xrtVfsPackCorrupt("pack-header",
				"the source does not contain the requested XRT pack format");
			goto fail;
		}
		if ( !__xrtVfsPackParseXrt(Pack, Source, arrHeader) ) goto fail;
	} else {
		if ( memcmp(arrHeader,
			__xrtVfsPackLegacyHeaderMagic, 8u) != 0 ) {
			(void)__xrtVfsPackCorrupt("pack-header",
				"the source does not contain the requested XSVPACK format");
			goto fail;
		}
		if ( !__xrtVfsPackParseLegacy(Pack, Source, arrHeader) ) goto fail;
	}
	if ( !__xrtVfsPackEntriesSync(Pack) ||
		!__xrtVfsPackNodesBuild(Pack) ||
		!xrtMutexInit(&Pack->CacheLock) ) goto fail;
	Pack->CacheLockReady = true;
	Pack->Source = Source;
	return Pack;

fail:
	__xrtVfsPackReleaseMembers(Pack, false);
	xrtFree(Pack);
	return NULL;
}

XRT_API void xrtVfsPackRef(xvfspack Pack)
{
	if ( Pack == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	if ( xrtRefRetain(&Pack->RefCount) < 0 ) abort();
}

XRT_API void xrtVfsPackDestroy(xvfspack Pack)
{
	if ( (Pack == NULL) || (xrtRefRelease(&Pack->RefCount) != 0) ) return;
	__xrtVfsPackReleaseMembers(Pack, true);
	xrtFree(Pack);
}

XRT_API xvfspackformat xrtVfsPackFormat(xvfspack Pack)
{
	if ( Pack == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XVFS_PACK_AUTO;
	}
	return Pack->Format;
}

XRT_API bool xrtVfsPackStats(xvfspack Pack, xvfspackstats* pStats)
{
	if ( (Pack == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMutexLock(&Pack->CacheLock) ) return false;
	*pStats = Pack->Stats;
	(void)xrtMutexUnlock(&Pack->CacheLock);
	return true;
}



static void* __xrtVfsPackLzmaAlloc(ISzAllocPtr pAllocator, size_t iSize)
{
	(void)pAllocator;
	return iSize == 0u ? NULL : xrtMalloc(iSize);
}

static void __xrtVfsPackLzmaFree(ISzAllocPtr pAllocator, void* pData)
{
	(void)pAllocator;
	xrtFree(pData);
}

static const ISzAlloc __xrtVfsPackLzmaAllocator = {
	__xrtVfsPackLzmaAlloc,
	__xrtVfsPackLzmaFree
};

static xrt_vfs_pack_blob* __xrtVfsPackLoadBlob(xvfspack Pack,
	const xrt_vfs_pack_entry* pEntry)
{
	xrt_vfs_pack_blob* pBlob;
	bytes pStored = NULL;
	bytes pOutput = NULL;

	pBlob = (xrt_vfs_pack_blob*)xrtMalloc(sizeof(*pBlob));
	if ( pBlob == NULL ) return NULL;
	if ( pEntry->StoredSize != 0u ) {
		pStored = (bytes)xrtMalloc((size_t)pEntry->StoredSize);
		if ( pStored == NULL ) goto fail;
		if ( !__xrtVfsPackReadSource(Pack->Source, Pack->Offset,
			Pack->Length, pEntry->DataOffset, pStored,
			(size_t)pEntry->StoredSize, "pack-entry-read") ) goto fail;
	}
	if ( pEntry->Codec == XVFS_PACK_STORE ) {
		pOutput = pStored;
		pStored = NULL;
	} else {
		SizeT iOutput = (SizeT)pEntry->OriginalSize;
		SizeT iInput = (SizeT)pEntry->StoredSize - LZMA_PROPS_SIZE;
		ELzmaStatus Status = LZMA_STATUS_NOT_SPECIFIED;
		SRes Result;

		pOutput = (bytes)xrtMalloc((size_t)pEntry->OriginalSize);
		if ( pOutput == NULL ) goto fail;
		Result = __xrtLzmaDecode(pOutput, &iOutput,
			pStored + LZMA_PROPS_SIZE, &iInput,
			pStored, LZMA_PROPS_SIZE, LZMA_FINISH_END,
			&Status, &__xrtVfsPackLzmaAllocator);
		if ( Result == SZ_ERROR_MEM ) goto fail;
		if ( (Result != SZ_OK) ||
			(iOutput != (SizeT)pEntry->OriginalSize) ||
			(iInput + LZMA_PROPS_SIZE != (SizeT)pEntry->StoredSize) ||
			((Status != LZMA_STATUS_FINISHED_WITH_MARK) &&
			 (Status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)) ) {
			(void)__xrtVfsPackCorrupt("pack-entry-decode",
				"the LZMA1 entry stream is invalid or incomplete");
			goto fail;
		}
	}
	if ( __xrtVfsPackCrc(pOutput, (size_t)pEntry->OriginalSize) !=
		pEntry->OriginalCrc32 ) {
		(void)__xrtVfsPackChecksum("pack-entry-checksum",
			"the decoded entry checksum is invalid");
		goto fail;
	}
	xrtFree(pStored);
	pBlob->RefCount = 1;
	pBlob->Data = pOutput;
	pBlob->Size = (size_t)pEntry->OriginalSize;
	return pBlob;

fail:
	xrtFree(pOutput);
	xrtFree(pStored);
	xrtFree(pBlob);
	return NULL;
}



static bool __xrtVfsPackCacheRoom(xvfspack Pack,
	size_t iIncoming, xrt_vfs_pack_entry* pCurrent)
{
	uint64 iIncoming64 = (uint64)iIncoming;

	if ( (iIncoming64 > Pack->Options.CacheBytes) ||
		(Pack->Options.CacheBytes == 0u) ) return false;
	while ( Pack->Stats.ResidentBytes >
		Pack->Options.CacheBytes - iIncoming64 ) {
		xrt_vfs_pack_entry* pVictim = NULL;
		uint64 iOldest = UINT64_MAX;

		for ( size_t i = 0u; i < Pack->EntryCount; i++ ) {
			xrt_vfs_pack_entry* pEntry = &Pack->Entries[i];

			if ( (pEntry == pCurrent) || !pEntry->Cached ||
				(pEntry->LastUse > iOldest) ) continue;
			if ( !xrtMutexLock(&pEntry->Lock) ) abort();
			if ( pEntry->Cached && (pEntry->Waiters == 0u) &&
				(pEntry->LastUse <= iOldest) ) {
				pVictim = pEntry;
				iOldest = pEntry->LastUse;
			}
			(void)xrtMutexUnlock(&pEntry->Lock);
		}
		if ( pVictim == NULL ) return false;
		if ( !xrtMutexLock(&pVictim->Lock) ) abort();
		if ( pVictim->Cached && (pVictim->Waiters == 0u) ) {
			xrt_vfs_pack_blob* pBlob = pVictim->Blob;

			pVictim->Blob = NULL;
			pVictim->Cached = false;
			pVictim->State = XRT_VFS_PACK_UNLOADED;
			Pack->Stats.ResidentBytes -= (uint64)pBlob->Size;
			Pack->Stats.Evictions++;
			(void)xrtMutexUnlock(&pVictim->Lock);
			__xrtVfsPackBlobRelease(pBlob);
		} else {
			(void)xrtMutexUnlock(&pVictim->Lock);
		}
	}
	return true;
}

static void __xrtVfsPackAccessStat(xvfspack Pack,
	xrt_vfs_pack_entry* pEntry, bool bHit)
{
	if ( !xrtMutexLock(&Pack->CacheLock) ) abort();
	if ( bHit ) Pack->Stats.Hits++;
	else Pack->Stats.Misses++;
	if ( pEntry->Cached ) pEntry->LastUse = ++Pack->CacheClock;
	(void)xrtMutexUnlock(&Pack->CacheLock);
}

static xrt_vfs_pack_blob* __xrtVfsPackEntryAcquire(xvfspack Pack,
	xrt_vfs_pack_entry* pEntry)
{
	bool bWaiter = false;

	if ( !xrtMutexLock(&pEntry->Lock) ) return NULL;
	for ( ;; ) {
		if ( (pEntry->State == XRT_VFS_PACK_READY) &&
			(pEntry->Blob != NULL) ) {
			xrt_vfs_pack_blob* pBlob = pEntry->Blob;
			xrt_vfs_pack_blob* pDrop = NULL;

			__xrtVfsPackBlobRef(pBlob);
			if ( bWaiter ) pEntry->Waiters--;
			if ( !pEntry->Cached && (pEntry->Waiters == 0u) ) {
				pDrop = pEntry->Blob;
				pEntry->Blob = NULL;
				pEntry->State = XRT_VFS_PACK_UNLOADED;
			}
			(void)xrtMutexUnlock(&pEntry->Lock);
			__xrtVfsPackBlobRelease(pDrop);
			__xrtVfsPackAccessStat(Pack, pEntry, !bWaiter);
			return pBlob;
		}
		if ( pEntry->State == XRT_VFS_PACK_FAILED ) {
			xerror* pFailure = xrtErrorRef(pEntry->Failure);

			if ( bWaiter ) pEntry->Waiters--;
			(void)xrtMutexUnlock(&pEntry->Lock);
			__xrtVfsPackAccessStat(Pack, pEntry, false);
			xrtSetErrorTake(pFailure);
			return NULL;
		}
		if ( pEntry->State == XRT_VFS_PACK_LOADING ) {
			if ( !bWaiter ) {
				pEntry->Waiters++;
				bWaiter = true;
			}
			if ( xrtCondWait(&pEntry->Ready,
				&pEntry->Lock) != XWAIT_OK ) {
				pEntry->Waiters--;
				(void)xrtMutexUnlock(&pEntry->Lock);
				return NULL;
			}
			continue;
		}
		pEntry->State = XRT_VFS_PACK_LOADING;
		(void)xrtMutexUnlock(&pEntry->Lock);
		break;
	}

	if ( !xrtMutexLock(&Pack->CacheLock) ) abort();
	Pack->Stats.Misses++;
	Pack->Stats.Loads++;
	(void)xrtMutexUnlock(&Pack->CacheLock);
	{
		xrt_vfs_pack_blob* pBlob = __xrtVfsPackLoadBlob(Pack, pEntry);

		if ( pBlob == NULL ) {
			xerror* pFailure = xrtTakeError();

			if ( pFailure == NULL ) {
				(void)__xrtVfsPackCorrupt("pack-entry-load",
					"the archive entry load failed without an error");
				pFailure = xrtTakeError();
			}
			if ( !xrtMutexLock(&pEntry->Lock) ) abort();
			pEntry->Failure = pFailure;
			pEntry->State = XRT_VFS_PACK_FAILED;
			(void)xrtCondBroadcast(&pEntry->Ready);
			(void)xrtMutexUnlock(&pEntry->Lock);
			if ( !xrtMutexLock(&Pack->CacheLock) ) abort();
			Pack->Stats.Failures++;
			(void)xrtMutexUnlock(&Pack->CacheLock);
			xrtSetError(pFailure);
			return NULL;
		}
		if ( !xrtMutexLock(&Pack->CacheLock) ) abort();
		if ( __xrtVfsPackCacheRoom(Pack, pBlob->Size, pEntry) ) {
			if ( !xrtMutexLock(&pEntry->Lock) ) abort();
			__xrtVfsPackBlobRef(pBlob);
			pEntry->Blob = pBlob;
			pEntry->Cached = true;
			pEntry->LastUse = ++Pack->CacheClock;
			pEntry->State = XRT_VFS_PACK_READY;
			Pack->Stats.ResidentBytes += (uint64)pBlob->Size;
			(void)xrtCondBroadcast(&pEntry->Ready);
			(void)xrtMutexUnlock(&pEntry->Lock);
			(void)xrtMutexUnlock(&Pack->CacheLock);
		} else {
			(void)xrtMutexUnlock(&Pack->CacheLock);
			if ( !xrtMutexLock(&pEntry->Lock) ) abort();
			__xrtVfsPackBlobRef(pBlob);
			pEntry->Blob = pBlob;
			pEntry->Cached = false;
			pEntry->State = XRT_VFS_PACK_READY;
			(void)xrtCondBroadcast(&pEntry->Ready);
			if ( pEntry->Waiters == 0u ) {
				xrt_vfs_pack_blob* pDrop = pEntry->Blob;

				pEntry->Blob = NULL;
				pEntry->State = XRT_VFS_PACK_UNLOADED;
				(void)xrtMutexUnlock(&pEntry->Lock);
				__xrtVfsPackBlobRelease(pDrop);
			} else {
				(void)xrtMutexUnlock(&pEntry->Lock);
			}
		}
		return pBlob;
	}
}



static xrt_vfs_pack_node* __xrtVfsPackFind(xvfspack Pack,
	xvfscase CaseMode, xstrview Path)
{
	size_t iLow = 0u;
	size_t iHigh = Pack->NodeCount;

	while ( iLow < iHigh ) {
		size_t iMiddle = iLow + ((iHigh - iLow) / 2u);
		xrt_vfs_pack_node* pNode = CaseMode == XVFS_CASE_SENSITIVE ?
			&Pack->Nodes[iMiddle] : Pack->Folded[iMiddle];
		int iCompare = CaseMode == XVFS_CASE_SENSITIVE ?
			xrtStrCompare((xstrview){ pNode->Path, pNode->PathSize }, Path) :
			xrtStrCaseCompare((xstrview){ pNode->Path, pNode->PathSize }, Path);

		if ( iCompare < 0 ) iLow = iMiddle + 1u;
		else if ( iCompare > 0 ) iHigh = iMiddle;
		else return pNode;
	}
	return NULL;
}

static bool __xrtVfsPackReadAt(void* pState, uint64 iOffset,
	void* pBuffer, size_t iRequest, size_t* pRead)
{
	xrt_vfs_pack_file* pFile = (xrt_vfs_pack_file*)pState;
	size_t iStart = iOffset > (uint64)SIZE_MAX ? SIZE_MAX : (size_t)iOffset;
	size_t iDone = iStart < pFile->Blob->Size ?
		pFile->Blob->Size - iStart : 0u;

	if ( iDone > iRequest ) iDone = iRequest;
	if ( iDone != 0u ) memcpy(pBuffer, pFile->Blob->Data + iStart, iDone);
	*pRead = iDone;
	return true;
}

static bool __xrtVfsPackRead(void* pState, void* pBuffer,
	size_t iRequest, size_t* pRead)
{
	xrt_vfs_pack_file* pFile = (xrt_vfs_pack_file*)pState;
	bool bResult = __xrtVfsPackReadAt(pState, pFile->Cursor,
		pBuffer, iRequest, pRead);

	if ( bResult ) pFile->Cursor += (uint64)*pRead;
	return bResult;
}

static bool __xrtVfsPackSeekAdd(uint64 iBase,
	int64 iOffset, uint64* pResult)
{
	if ( iOffset >= 0 ) {
		uint64 iAdd = (uint64)iOffset;

		if ( iBase > UINT64_MAX - iAdd ) return false;
		*pResult = iBase + iAdd;
	} else {
		uint64 iSubtract = (uint64)(-(iOffset + 1)) + 1u;

		if ( iBase < iSubtract ) return false;
		*pResult = iBase - iSubtract;
	}
	return true;
}

static bool __xrtVfsPackSeek(void* pState, int64 iOffset,
	xseek Origin, uint64* pPosition)
{
	xrt_vfs_pack_file* pFile = (xrt_vfs_pack_file*)pState;
	uint64 iBase;
	uint64 iResult;

	if ( Origin == XSEEK_START ) iBase = 0u;
	else if ( Origin == XSEEK_CURRENT ) iBase = pFile->Cursor;
	else if ( Origin == XSEEK_END ) iBase = (uint64)pFile->Blob->Size;
	else {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtVfsPackSeekAdd(iBase, iOffset, &iResult) ) {
		__xrtVfsError(XERR_RANGE, XVFS_ERROR_OPEN,
			"pack-seek", "the requested position is out of range");
		return false;
	}
	pFile->Cursor = iResult;
	if ( pPosition != NULL ) *pPosition = iResult;
	return true;
}

static bool __xrtVfsPackFileStat(void* pState, xfileinfo* pInfo)
{
	xrt_vfs_pack_file* pFile = (xrt_vfs_pack_file*)pState;

	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = (uint64)pFile->Blob->Size;
	return true;
}

static void __xrtVfsPackFileClose(void* pState)
{
	xrt_vfs_pack_file* pFile = (xrt_vfs_pack_file*)pState;

	__xrtVfsPackBlobRelease(pFile->Blob);
	xrtFree(pFile);
}

static const xvfsfileops_v1 __xrtVfsPackFileOps = {
	(uint32)sizeof(xvfsfileops_v1),
	XRT_VFS_FILE_OPS_VERSION,
	XVFS_FILE_READ | XVFS_FILE_READ_AT | XVFS_FILE_SEEK | XVFS_FILE_STAT,
	__xrtVfsPackRead, NULL,
	__xrtVfsPackReadAt, NULL,
	__xrtVfsPackSeek, __xrtVfsPackFileStat,
	NULL, NULL,
	__xrtVfsPackFileClose
};

static bool __xrtVfsPackReadOnly(const xfileoptions* pOptions)
{
	uint32 iForbidden = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE |
		XFILE_APPEND | XFILE_EXCLUSIVE | XFILE_SYNC;

	if ( (pOptions->Flags & iForbidden) == 0u ) return true;
	__xrtVfsError(XERR_UNSUPPORTED, XVFS_ERROR_UNSUPPORTED,
		"pack-open", "the pack provider is read-only");
	return false;
}

static xvfslookup __xrtVfsPackOpen(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	const xfileoptions* pOptions, xvfsfile_v1* pFile)
{
	xvfspack Pack = (xvfspack)pContext;
	xrt_vfs_pack_node* pNode = __xrtVfsPackFind(Pack,
		CaseMode, RelativePath);
	xrt_vfs_pack_blob* pBlob;
	xrt_vfs_pack_file* pState;

	if ( (pNode == NULL) || pNode->Directory ) return XVFS_LOOKUP_MISS;
	if ( !__xrtVfsPackReadOnly(pOptions) ) return XVFS_LOOKUP_ERROR;
	pBlob = __xrtVfsPackEntryAcquire(Pack,
		&Pack->Entries[pNode->Entry]);
	if ( pBlob == NULL ) return XVFS_LOOKUP_ERROR;
	pState = (xrt_vfs_pack_file*)xrtMalloc(sizeof(*pState));
	if ( pState == NULL ) {
		__xrtVfsPackBlobRelease(pBlob);
		return XVFS_LOOKUP_ERROR;
	}
	pState->Blob = pBlob;
	pState->Cursor = 0u;
	pFile->Ops = &__xrtVfsPackFileOps;
	pFile->State = pState;
	pFile->Flags = pOptions->Flags;
	return XVFS_LOOKUP_OPENED;
}

static xvfslookup __xrtVfsPackStat(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	bool bFollowLink, xfileinfo* pInfo)
{
	xvfspack Pack = (xvfspack)pContext;
	xrt_vfs_pack_node* pNode = __xrtVfsPackFind(Pack,
		CaseMode, RelativePath);
	(void)bFollowLink;

	if ( pNode == NULL ) return XVFS_LOOKUP_MISS;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = pNode->Directory ? XFILE_TYPE_DIRECTORY : XFILE_TYPE_FILE;
	if ( !pNode->Directory ) {
		pInfo->Available = XFILE_INFO_SIZE;
		pInfo->Size = Pack->Entries[pNode->Entry].OriginalSize;
	}
	return XVFS_LOOKUP_OPENED;
}

static bool __xrtVfsPackParentEqual(const xrt_vfs_pack_node* pNode,
	xstrview Path, xvfscase CaseMode)
{
	xstrview Parent;

	if ( pNode->ParentSize == SIZE_MAX ) return false;
	Parent.Data = pNode->Path;
	Parent.Size = pNode->ParentSize;
	return CaseMode == XVFS_CASE_SENSITIVE ?
		xrtStrEqual(Parent, Path) : xrtStrCaseEqual(Parent, Path);
}

static bool __xrtVfsPackDirNext(void* pState,
	xdirentry* pEntry, bool* pEnd)
{
	xrt_vfs_pack_dir* pDir = (xrt_vfs_pack_dir*)pState;
	xrt_vfs_pack_node* pNode;

	if ( pDir->Position == pDir->Count ) {
		*pEnd = true;
		return true;
	}
	pNode = &pDir->Pack->Nodes[pDir->Items[pDir->Position++]];
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->Name.Data = pNode->Path + pNode->NameOffset;
	pEntry->Name.Size = pNode->PathSize - pNode->NameOffset;
	pEntry->Info.Type = pNode->Directory ?
		XFILE_TYPE_DIRECTORY : XFILE_TYPE_FILE;
	if ( !pNode->Directory ) {
		pEntry->Info.Available = XFILE_INFO_SIZE;
		pEntry->Info.Size = pDir->Pack->Entries[pNode->Entry].OriginalSize;
	}
	pEntry->Flags = XDIR_ENTRY_UTF8;
	return true;
}

static void __xrtVfsPackDirClose(void* pState)
{
	xrt_vfs_pack_dir* pDir = (xrt_vfs_pack_dir*)pState;

	xrtVfsPackDestroy(pDir->Pack);
	xrtFree(pDir->Items);
	xrtFree(pDir);
}

static const xvfsdirops_v1 __xrtVfsPackDirOps = {
	(uint32)sizeof(xvfsdirops_v1),
	XRT_VFS_DIR_OPS_VERSION,
	__xrtVfsPackDirNext,
	__xrtVfsPackDirClose
};

static xvfslookup __xrtVfsPackDirOpen(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	uint32 iFlags, xvfsdir_v1* pOutput)
{
	xvfspack Pack = (xvfspack)pContext;
	xrt_vfs_pack_node* pNode = __xrtVfsPackFind(Pack,
		CaseMode, RelativePath);
	xrt_vfs_pack_dir* pDir;
	size_t iCount = 0u;
	(void)iFlags;

	if ( (pNode == NULL) || !pNode->Directory ) return XVFS_LOOKUP_MISS;
	for ( size_t i = 0u; i < Pack->NodeCount; i++ )
		if ( __xrtVfsPackParentEqual(&Pack->Nodes[i],
			RelativePath, CaseMode) ) iCount++;
	pDir = (xrt_vfs_pack_dir*)xrtCalloc(1u, sizeof(*pDir));
	if ( pDir == NULL ) return XVFS_LOOKUP_ERROR;
	if ( iCount != 0u ) {
		if ( iCount > SIZE_MAX / sizeof(pDir->Items[0]) ) {
			__xrtErrorSetSizeOverflow();
			xrtFree(pDir);
			return XVFS_LOOKUP_ERROR;
		}
		pDir->Items = (size_t*)xrtMalloc(
			iCount * sizeof(pDir->Items[0]));
		if ( pDir->Items == NULL ) {
			xrtFree(pDir);
			return XVFS_LOOKUP_ERROR;
		}
	}
	for ( size_t i = 0u; i < Pack->NodeCount; i++ ) {
		if ( __xrtVfsPackParentEqual(&Pack->Nodes[i],
			RelativePath, CaseMode) ) pDir->Items[pDir->Count++] = i;
	}
	pDir->Pack = Pack;
	xrtVfsPackRef(Pack);
	pOutput->Ops = &__xrtVfsPackDirOps;
	pOutput->State = pDir;
	return XVFS_LOOKUP_OPENED;
}

static void __xrtVfsPackContextRetain(void* pContext)
{
	xrtVfsPackRef((xvfspack)pContext);
}

static void __xrtVfsPackContextRelease(void* pContext)
{
	xrtVfsPackDestroy((xvfspack)pContext);
}

static const xvfsprovider_v1 __xrtVfsPackProvider = {
	(uint32)sizeof(xvfsprovider_v1),
	XRT_VFS_PROVIDER_VERSION,
	XVFS_PROVIDER_STAT | XVFS_PROVIDER_DIRECTORY,
	__xrtVfsPackContextRetain,
	__xrtVfsPackContextRelease,
	__xrtVfsPackOpen,
	__xrtVfsPackStat,
	__xrtVfsPackDirOpen,
	NULL
};

XRT_API xvfsmount xrtVfsPackMount(xvfs Vfs,
	cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode,
	xvfspack Pack, uint32 iFlags)
{
	if ( (Vfs == NULL) || (Pack == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (CaseMode == XVFS_CASE_ASCII_INSENSITIVE) &&
		Pack->FoldCollision ) {
		__xrtVfsError(XERR_EXISTS, XVFS_ERROR_MOUNT,
			"pack-mount",
			"the pack contains paths that collide under ASCII folding");
		return NULL;
	}
	return xrtVfsMount(Vfs, sVirtualPrefix, iPriority,
		CaseMode, &__xrtVfsPackProvider, Pack, iFlags);
}

#endif
