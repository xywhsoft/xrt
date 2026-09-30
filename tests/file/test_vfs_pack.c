#ifdef VFS_PACK_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/vfs.h>
#endif

#include "../test.h"



typedef struct test_pack_item {
	cstr Path;
	const uint8* Data;
	size_t StoredSize;
	size_t OriginalSize;
	uint8 Codec;
	uint32 Crc;
} test_pack_item;



static void testPackU16(uint8* pData, uint16 iValue)
{
	pData[0] = (uint8)iValue;
	pData[1] = (uint8)(iValue >> 8u);
}

static void testPackU32(uint8* pData, uint32 iValue)
{
	pData[0] = (uint8)iValue;
	pData[1] = (uint8)(iValue >> 8u);
	pData[2] = (uint8)(iValue >> 16u);
	pData[3] = (uint8)(iValue >> 24u);
}

static void testPackU64(uint8* pData, uint64 iValue)
{
	testPackU32(pData, (uint32)iValue);
	testPackU32(pData + 4u, (uint32)(iValue >> 32u));
}

static uint64 testPackReadU64(const uint8* pData)
{
	uint64 iValue = 0u;

	for ( unsigned i = 0u; i < 8u; i++ )
		iValue |= (uint64)pData[i] << (i * 8u);
	return iValue;
}

static uint32 testPackCrcUpdate(uint32 iCrc,
	const void* pData, size_t iSize)
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

	for ( size_t i = 0u; i < iSize; i++ ) {
		iCrc ^= pBytes[i];
		iCrc = (iCrc >> 4u) ^ Table[iCrc & 15u];
		iCrc = (iCrc >> 4u) ^ Table[iCrc & 15u];
	}
	return iCrc;
}

static uint32 testPackCrc(const void* pData, size_t iSize)
{
	return testPackCrcUpdate(UINT32_MAX, pData, iSize) ^ UINT32_MAX;
}



static bytes testPackXrtBuild(test_pack_item* pItems,
	size_t iCount, size_t* pSize)
{
	size_t iDataSize = 0u;
	size_t iIndexSize = 0u;
	size_t iTotal;
	size_t iDataPosition = 80u;
	size_t iIndexPosition;
	bytes pArchive;
	uint8* pIndex;
	uint8* pTrailer;

	for ( size_t i = 0u; i < iCount; i++ ) {
		pItems[i].Crc = testPackCrc(
			pItems[i].Codec == XVFS_PACK_STORE ? pItems[i].Data :
				(const uint8*)"compressed hello",
			pItems[i].OriginalSize);
		iDataSize += pItems[i].StoredSize;
		iIndexSize += 40u + strlen(pItems[i].Path);
	}
	iTotal = 80u + iDataSize + iIndexSize + 32u;
	pArchive = (bytes)xrtCalloc(iTotal, 1u);
	testRequire(pArchive != NULL, "XRT pack fixture allocation failed");
	memcpy(pArchive, "XRTPACK\0", 8u);
	testPackU16(pArchive + 8u, 1u);
	testPackU16(pArchive + 10u, 0u);
	testPackU32(pArchive + 12u, 80u);
	testPackU32(pArchive + 20u, (uint32)iCount);
	testPackU64(pArchive + 24u, 80u);
	testPackU64(pArchive + 32u, iDataSize);
	testPackU64(pArchive + 40u, 80u + iDataSize);
	testPackU64(pArchive + 48u, iIndexSize);
	testPackU64(pArchive + 56u, iTotal);
	{
		uint64 iDecoded = 0u;

		for ( size_t i = 0u; i < iCount; i++ )
			iDecoded += (uint64)pItems[i].OriginalSize;
		testPackU64(pArchive + 64u, iDecoded);
	}
	pIndex = pArchive + 80u + iDataSize;
	iIndexPosition = 0u;
	for ( size_t i = 0u; i < iCount; i++ ) {
		size_t iPathSize = strlen(pItems[i].Path);
		uint8* pRecord = pIndex + iIndexPosition;

		memcpy(pArchive + iDataPosition, pItems[i].Data,
			pItems[i].StoredSize);
		testPackU16(pRecord, (uint16)iPathSize);
		pRecord[2] = pItems[i].Codec;
		testPackU64(pRecord + 8u, iDataPosition);
		testPackU64(pRecord + 16u, pItems[i].StoredSize);
		testPackU64(pRecord + 24u, pItems[i].OriginalSize);
		testPackU32(pRecord + 32u, pItems[i].Crc);
		memcpy(pRecord + 40u, pItems[i].Path, iPathSize);
		iDataPosition += pItems[i].StoredSize;
		iIndexPosition += 40u + iPathSize;
	}
	testPackU32(pArchive + 72u, testPackCrc(pIndex, iIndexSize));
	pTrailer = pIndex + iIndexSize;
	memcpy(pTrailer, "XRTPEND\0", 8u);
	testPackU64(pTrailer + 8u, iTotal);
	testPackU64(pTrailer + 16u, 0u);
	{
		uint32 iCrc = testPackCrcUpdate(UINT32_MAX, pTrailer, 24u);

		iCrc = testPackCrcUpdate(iCrc, pArchive, 80u) ^ UINT32_MAX;
		testPackU32(pTrailer + 24u, iCrc);
	}
	*pSize = iTotal;
	return pArchive;
}

static void testPackXrtRepair(bytes pArchive, size_t iSize)
{
	uint64 iIndexOffset = testPackReadU64(pArchive + 40u);
	uint64 iIndexSize = testPackReadU64(pArchive + 48u);
	uint8* pTrailer = pArchive + iSize - 32u;
	uint32 iCrc;

	testPackU32(pArchive + 72u, testPackCrc(
		pArchive + (size_t)iIndexOffset, (size_t)iIndexSize));
	iCrc = testPackCrcUpdate(UINT32_MAX, pTrailer, 24u);
	iCrc = testPackCrcUpdate(iCrc, pArchive, 80u) ^ UINT32_MAX;
	testPackU32(pTrailer + 24u, iCrc);
}



static bytes testPackLegacyBuild(test_pack_item* pItems,
	size_t iCount, size_t iPrefix, size_t* pSize, uint64* pOffset)
{
	size_t iDataSize = 0u;
	size_t iIndexSize = 0u;
	size_t iArchiveSize;
	size_t iTotal;
	size_t iDataPosition = 64u;
	size_t iIndexPosition = 0u;
	bytes pWhole;
	uint8* pArchive;
	uint8* pIndex;
	uint8* pTrailer;

	for ( size_t i = 0u; i < iCount; i++ ) {
		pItems[i].Crc = testPackCrc(pItems[i].Data, pItems[i].OriginalSize);
		iDataSize += pItems[i].StoredSize;
		iIndexSize += 28u + strlen(pItems[i].Path);
	}
	iArchiveSize = 64u + iDataSize + iIndexSize + 24u;
	iTotal = iPrefix + iArchiveSize;
	pWhole = (bytes)xrtCalloc(iTotal, 1u);
	testRequire(pWhole != NULL, "legacy pack fixture allocation failed");
	memset(pWhole, 0xA5, iPrefix);
	pArchive = pWhole + iPrefix;
	memcpy(pArchive, "XSVFHDR\0", 8u);
	testPackU16(pArchive + 8u, 1u);
	testPackU16(pArchive + 10u, 64u);
	testPackU32(pArchive + 12u, (uint32)iCount);
	testPackU64(pArchive + 16u, 64u);
	testPackU64(pArchive + 24u, 64u + iDataSize);
	testPackU64(pArchive + 32u, iArchiveSize);
	pIndex = pArchive + 64u + iDataSize;
	for ( size_t i = 0u; i < iCount; i++ ) {
		size_t iPathSize = strlen(pItems[i].Path);
		uint8* pRecord = pIndex + iIndexPosition;

		memcpy(pArchive + iDataPosition, pItems[i].Data,
			pItems[i].StoredSize);
		testPackU16(pRecord, (uint16)iPathSize);
		memcpy(pRecord + 2u, pItems[i].Path, iPathSize);
		pRecord[2u + iPathSize] = pItems[i].Codec;
		testPackU64(pRecord + 4u + iPathSize, iDataPosition);
		testPackU32(pRecord + 12u + iPathSize,
			(uint32)pItems[i].StoredSize);
		testPackU64(pRecord + 16u + iPathSize, pItems[i].OriginalSize);
		testPackU32(pRecord + 24u + iPathSize, pItems[i].Crc);
		iDataPosition += pItems[i].StoredSize;
		iIndexPosition += 28u + iPathSize;
	}
	testPackU32(pArchive + 40u, testPackCrc(pIndex, iIndexSize));
	pTrailer = pIndex + iIndexSize;
	memcpy(pTrailer, "XSVPACK\0", 8u);
	testPackU64(pTrailer + 8u, iPrefix);
	{
		uint32 iCrc = testPackCrcUpdate(UINT32_MAX, pTrailer, 16u);

		iCrc = testPackCrcUpdate(iCrc, pArchive, 64u) ^ UINT32_MAX;
		testPackU32(pTrailer + 16u, iCrc);
	}
	*pSize = iTotal;
	*pOffset = iPrefix;
	return pWhole;
}



static xfile testPackSource(const void* pData, size_t iSize, str* pPath)
{
	xfile File = xrtFileTemp(NULL, "xrt-vfs-pack-", ".bin", pPath);

	testRequire(File != NULL, "pack source temp file creation failed");
	testRequire(xrtWriteFull(File, pData, iSize, NULL),
		"pack source write failed");
	testRequire(xrtSeek(File, 0, XSEEK_START, NULL),
		"pack source rewind failed");
	return File;
}

static void testPackRead(xfile File, cstr sExpected)
{
	char arrData[64] = { 0 };
	size_t iExpected = strlen(sExpected);
	size_t iRead = 0u;

	testRequire(xrtRead(File, arrData, sizeof(arrData), &iRead) &&
		(iRead == iExpected) &&
		(memcmp(arrData, sExpected, iExpected) == 0),
		"pack file returned unexpected content");
}



static void testPackXrtProvider(void)
{
	static const uint8 LzmaData[] = {
		0x5D, 0x00, 0x00, 0x01, 0x00,
		0x00, 0x31, 0x9B, 0xC9, 0xF3, 0xF6, 0xBC, 0x8E,
		0xC5, 0xCE, 0x1D, 0x57, 0xBA, 0x49, 0xEF, 0xDA,
		0x98, 0x6D, 0x35, 0xCB, 0xFF, 0xFF, 0xDD, 0x9D, 0x00, 0x00
	};
	test_pack_item Items[] = {
		{ "a.txt", (const uint8*)"alpha", 5u, 5u, XVFS_PACK_STORE, 0u },
		{ "dir/b.bin", (const uint8*)"beta", 4u, 4u, XVFS_PACK_STORE, 0u },
		{ "dir/c.txt", LzmaData, sizeof(LzmaData), 16u, XVFS_PACK_LZMA1, 0u }
	};
	size_t iArchiveSize;
	bytes pArchive = testPackXrtBuild(Items, 3u, &iArchiveSize);
	str sPath = NULL;
	xfile Source = testPackSource(pArchive, iArchiveSize, &sPath);
	xvfspackoptions Options;
	xvfspack Pack;
	xvfs Vfs;
	xvfsmount Mount;
	xfile First;
	xfile Second;
	xfile Evicting;
	xfile Compressed;
	xfileinfo Info;
	xdir Dir;
	xdirentry Entry;
	xdirnext Next;
	xvfspackstats Stats;

	xrtVfsPackOptionsInit(&Options);
	Options.CacheBytes = 5u;
	Pack = xrtVfsPackCreate(Source, 0u, iArchiveSize, &Options);
	testRequire(Pack != NULL, "XRT pack creation failed");
	testRequire(xrtVfsPackFormat(Pack) == XVFS_PACK_XRT_V1,
		"XRT pack auto detection returned the wrong format");
	Vfs = xrtVfsCreate();
	testRequire(Vfs != NULL, "pack VFS creation failed");
	Mount = xrtVfsPackMount(Vfs, "/app", 0,
		XVFS_CASE_ASCII_INSENSITIVE, Pack, 0u);
	testRequire(Mount != NULL, "pack mount failed");
	First = xrtVfsOpen(Vfs, "/APP/A.TXT", NULL);
	Second = xrtVfsOpen(Vfs, "/app/a.txt", NULL);
	Compressed = xrtVfsOpen(Vfs, "/app/dir/c.txt", NULL);
	Evicting = xrtVfsOpen(Vfs, "/app/dir/b.bin", NULL);
	testRequire((First != NULL) && (Second != NULL) &&
		(Compressed != NULL) && (Evicting != NULL),
		"pack files did not open");
	testPackRead(First, "alpha");
	testPackRead(Second, "alpha");
	testPackRead(Compressed, "compressed hello");
	testPackRead(Evicting, "beta");
	testRequire(xrtVfsPackStats(Pack, &Stats) &&
		(Stats.Hits >= 1u) && (Stats.Misses >= 3u) &&
		(Stats.Loads == 3u) && (Stats.Failures == 0u) &&
		(Stats.ResidentBytes <= Options.CacheBytes) &&
		(Stats.Evictions >= 1u),
		"pack cache statistics or budget are incorrect");
	xrtVfsPackDestroy(Pack);
	Pack = NULL;
	testRequire(xrtVfsStat(Vfs, "/app/dir", false, &Info) &&
		(Info.Type == XFILE_TYPE_DIRECTORY),
		"pack synthesized directory stat failed");
	Dir = xrtVfsDirOpen(Vfs, "/app/dir", 0u);
	testRequire(Dir != NULL, "pack directory open failed");
	Next = xrtDirNext(Dir, &Entry);
	testRequire((Next == XDIR_NEXT_ITEM) &&
		xrtStrEqual(Entry.Name, XRT_STR_LITERAL("b.bin")),
		"pack first directory entry is incorrect");
	Next = xrtDirNext(Dir, &Entry);
	testRequire((Next == XDIR_NEXT_ITEM) &&
		xrtStrEqual(Entry.Name, XRT_STR_LITERAL("c.txt")),
		"pack second directory entry is incorrect");
	testRequire((xrtDirNext(Dir, &Entry) == XDIR_NEXT_END) &&
		xrtDirClose(Dir), "pack directory did not end cleanly");
	testRequire(xrtVfsUnmount(Mount), "pack unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	testRequire(xrtSeek(First, 0, XSEEK_START, NULL),
		"opened pack file did not survive unmount");
	testPackRead(First, "alpha");
	testRequire(xrtClose(First) && xrtClose(Second) &&
		xrtClose(Compressed) && xrtClose(Evicting),
		"pack file close failed");
	xrtFree(pArchive);
	testRequire(xrtFileDelete(sPath), "pack temp source delete failed");
	xrtFree(sPath);
}



static void testPackLegacyProvider(void)
{
	test_pack_item Items[] = {
		{ "legacy.txt", (const uint8*)"legacy", 6u, 6u,
			XVFS_PACK_STORE, 0u }
	};
	size_t iWholeSize;
	uint64 iOffset;
	bytes pWhole = testPackLegacyBuild(Items, 1u, 13u,
		&iWholeSize, &iOffset);
	str sPath = NULL;
	xfile Source = testPackSource(pWhole, iWholeSize, &sPath);
	xvfspack Pack = xrtVfsPackCreate(Source, iOffset,
		iWholeSize - (size_t)iOffset, NULL);
	xvfs Vfs;
	xvfsmount Mount;
	xfile File;

	testRequire(Pack != NULL, "legacy XSVPACK creation failed");
	testRequire(xrtVfsPackFormat(Pack) == XVFS_PACK_XSVPACK_V1,
		"legacy XSVPACK auto detection returned the wrong format");
	Vfs = xrtVfsCreate();
	Mount = xrtVfsPackMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Pack, 0u);
	testRequire((Vfs != NULL) && (Mount != NULL),
		"legacy XSVPACK mount failed");
	File = xrtVfsOpen(Vfs, "/legacy.txt", NULL);
	testRequire(File != NULL, "legacy XSVPACK file open failed");
	testPackRead(File, "legacy");
	testRequire(xrtClose(File), "legacy XSVPACK file close failed");
	testRequire(xrtVfsUnmount(Mount), "legacy XSVPACK unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtVfsPackDestroy(Pack);
	xrtFree(pWhole);
	testRequire(xrtFileDelete(sPath), "legacy pack temp source delete failed");
	xrtFree(sPath);
}



static void testPackFailureKeepsSource(void)
{
	uint8 arrBad[88] = { 0 };
	str sPath = NULL;
	xfile Source = testPackSource(arrBad, sizeof(arrBad), &sPath);
	uint64 iBefore = 0u;
	uint64 iAfter = 0u;

	testRequire(xrtSeek(Source, 7, XSEEK_START, &iBefore),
		"bad pack source seek failed");
	testRequire(xrtVfsPackCreate(Source, 0u, sizeof(arrBad), NULL) == NULL,
		"unknown pack magic was accepted");
	xrtClearError();
	testRequire(xrtTell(Source, &iAfter) && (iBefore == iAfter),
		"failed pack creation consumed or moved the source");
	testRequire(xrtClose(Source), "failed pack source close failed");
	testRequire(xrtFileDelete(sPath), "failed pack temp source delete failed");
	xrtFree(sPath);
}



static void testPackRejectArchive(bytes pArchive, size_t iSize,
	const xvfspackoptions* pOptions, cstr sMessage)
{
	str sPath = NULL;
	xfile Source = testPackSource(pArchive, iSize, &sPath);

	testRequire(xrtVfsPackCreate(Source, 0u, iSize, pOptions) == NULL,
		sMessage);
	xrtClearError();
	testRequire(xrtClose(Source), "rejected pack source close failed");
	testRequire(xrtFileDelete(sPath), "rejected pack source delete failed");
	xrtFree(sPath);
}

static void testPackParserValidation(void)
{
	char InvalidPath[] = { 'b', 'a', 'd', (char)0xFF, '\0' };
	test_pack_item GoodItems[] = {
		{ "a", (const uint8*)"one", 3u, 3u, XVFS_PACK_STORE, 0u },
		{ "b", (const uint8*)"two", 3u, 3u, XVFS_PACK_STORE, 0u }
	};
	test_pack_item DuplicateItems[] = {
		{ "same", (const uint8*)"a", 1u, 1u, XVFS_PACK_STORE, 0u },
		{ "same", (const uint8*)"b", 1u, 1u, XVFS_PACK_STORE, 0u }
	};
	test_pack_item InvalidItems[] = {
		{ InvalidPath, (const uint8*)"x", 1u, 1u, XVFS_PACK_STORE, 0u }
	};
	size_t iGoodSize;
	size_t iOtherSize;
	bytes pGood = testPackXrtBuild(GoodItems, 2u, &iGoodSize);
	bytes pMutation = (bytes)xrtMemDup(pGood, iGoodSize);
	bytes pOther;
	uint64 iIndexOffset = testPackReadU64(pGood + 40u);
	xvfspackoptions Options;

	testRequire(pMutation != NULL, "pack validation clone allocation failed");
	pMutation[(size_t)iIndexOffset] ^= 1u;
	testPackRejectArchive(pMutation, iGoodSize, NULL,
		"pack with a corrupt index checksum was accepted");
	xrtFree(pMutation);

	pMutation = (bytes)xrtMemDup(pGood, iGoodSize);
	testRequire(pMutation != NULL, "pack overlap clone allocation failed");
	testPackU64(pMutation + (size_t)iIndexOffset + 41u + 8u, 80u);
	testPackXrtRepair(pMutation, iGoodSize);
	testPackRejectArchive(pMutation, iGoodSize, NULL,
		"pack with overlapping data extents was accepted");
	xrtFree(pMutation);

	testPackRejectArchive(pGood, iGoodSize - 1u, NULL,
		"truncated pack range was accepted");

	pOther = testPackXrtBuild(DuplicateItems, 2u, &iOtherSize);
	testPackRejectArchive(pOther, iOtherSize, NULL,
		"pack with duplicate paths was accepted");
	xrtFree(pOther);
	pOther = testPackXrtBuild(InvalidItems, 1u, &iOtherSize);
	testPackRejectArchive(pOther, iOtherSize, NULL,
		"pack with invalid UTF-8 was accepted");
	xrtFree(pOther);

	xrtVfsPackOptionsInit(&Options);
	Options.MaxDecodedBytes = 5u;
	testPackRejectArchive(pGood, iGoodSize, &Options,
		"pack exceeding the decoded budget was accepted");
	xrtFree(pGood);
}



static void testPackChecksumFailureIsStable(void)
{
	test_pack_item Items[] = {
		{ "bad", (const uint8*)"payload", 7u, 7u,
			XVFS_PACK_STORE, 0u }
	};
	size_t iSize;
	bytes pArchive = testPackXrtBuild(Items, 1u, &iSize);
	uint64 iIndexOffset = testPackReadU64(pArchive + 40u);
	str sPath = NULL;
	xfile Source;
	xvfspack Pack;
	xvfs Vfs;
	xvfsmount Mount;
	xvfspackstats First;
	xvfspackstats Second;

	testPackU32(pArchive + (size_t)iIndexOffset + 32u,
		Items[0].Crc ^ UINT32_C(0x01020304));
	testPackXrtRepair(pArchive, iSize);
	Source = testPackSource(pArchive, iSize, &sPath);
	Pack = xrtVfsPackCreate(Source, 0u, iSize, NULL);
	testRequire(Pack != NULL, "checksum failure pack parse failed too early");
	Vfs = xrtVfsCreate();
	Mount = xrtVfsPackMount(Vfs, "/", 0, XVFS_CASE_SENSITIVE, Pack, 0u);
	testRequire((Vfs != NULL) && (Mount != NULL),
		"checksum failure pack mount failed");
	testRequire(xrtVfsOpen(Vfs, "/bad", NULL) == NULL,
		"entry with a bad content checksum opened");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XVFS_ERROR_CHECKSUM),
		"bad content checksum returned the wrong error");
	xrtClearError();
	testRequire(xrtVfsPackStats(Pack, &First) &&
		(First.Loads == 1u) && (First.Failures == 1u),
		"first checksum failure stats are incorrect");
	testRequire(xrtVfsOpen(Vfs, "/bad", NULL) == NULL,
		"FAILED pack entry unexpectedly retried successfully");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XVFS_ERROR_CHECKSUM),
		"repeated checksum failure did not preserve its error");
	xrtClearError();
	testRequire(xrtVfsPackStats(Pack, &Second) &&
		(Second.Loads == First.Loads) &&
		(Second.Failures == First.Failures),
		"FAILED pack entry was decoded more than once");
	testRequire(xrtVfsUnmount(Mount), "checksum pack unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtVfsPackDestroy(Pack);
	xrtFree(pArchive);
	testRequire(xrtFileDelete(sPath), "checksum pack source delete failed");
	xrtFree(sPath);
}



int main(void)
{
	testPackXrtProvider();
	testPackLegacyProvider();
	testPackFailureKeepsSource();
	testPackParserValidation();
	testPackChecksumFailureIsStable();
	return 0;
}
