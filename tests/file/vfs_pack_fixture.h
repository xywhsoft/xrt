#ifndef XRT_TEST_VFS_PACK_FIXTURE_H
#define XRT_TEST_VFS_PACK_FIXTURE_H



static void testVfsPackFixtureU16(uint8* pData, uint16 iValue)
{
	pData[0] = (uint8)iValue;
	pData[1] = (uint8)(iValue >> 8u);
}

static void testVfsPackFixtureU32(uint8* pData, uint32 iValue)
{
	pData[0] = (uint8)iValue;
	pData[1] = (uint8)(iValue >> 8u);
	pData[2] = (uint8)(iValue >> 16u);
	pData[3] = (uint8)(iValue >> 24u);
}

static void testVfsPackFixtureU64(uint8* pData, uint64 iValue)
{
	testVfsPackFixtureU32(pData, (uint32)iValue);
	testVfsPackFixtureU32(pData + 4u, (uint32)(iValue >> 32u));
}

static uint32 testVfsPackFixtureCrcUpdate(uint32 iCrc,
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

static uint32 testVfsPackFixtureCrc(const void* pData, size_t iSize)
{
	return testVfsPackFixtureCrcUpdate(
		UINT32_MAX, pData, iSize) ^ UINT32_MAX;
}



static bytes testVfsPackFixtureBuild(cstr sPath,
	const uint8* pStored, size_t iStoredSize,
	const uint8* pOriginal, size_t iOriginalSize,
	uint8 iCodec, bool bBadContentCrc, size_t* pArchiveSize)
{
	size_t iPathSize = strlen(sPath);
	size_t iIndexSize = 40u + iPathSize;
	size_t iSize = 80u + iStoredSize + iIndexSize + 32u;
	bytes pArchive = (bytes)xrtCalloc(iSize, 1u);
	uint8* pIndex;
	uint8* pTrailer;
	uint32 iContentCrc;
	uint32 iMetadataCrc;

	testRequire((pArchive != NULL) && (iPathSize <= UINT16_MAX),
		"pack fixture allocation or path length failed");
	memcpy(pArchive, "XRTPACK\0", 8u);
	testVfsPackFixtureU16(pArchive + 8u, 1u);
	testVfsPackFixtureU32(pArchive + 12u, 80u);
	testVfsPackFixtureU32(pArchive + 20u, 1u);
	testVfsPackFixtureU64(pArchive + 24u, 80u);
	testVfsPackFixtureU64(pArchive + 32u, iStoredSize);
	testVfsPackFixtureU64(pArchive + 40u, 80u + iStoredSize);
	testVfsPackFixtureU64(pArchive + 48u, iIndexSize);
	testVfsPackFixtureU64(pArchive + 56u, iSize);
	testVfsPackFixtureU64(pArchive + 64u, iOriginalSize);
	if ( iStoredSize != 0u )
		memcpy(pArchive + 80u, pStored, iStoredSize);
	pIndex = pArchive + 80u + iStoredSize;
	testVfsPackFixtureU16(pIndex, (uint16)iPathSize);
	pIndex[2] = iCodec;
	testVfsPackFixtureU64(pIndex + 8u, 80u);
	testVfsPackFixtureU64(pIndex + 16u, iStoredSize);
	testVfsPackFixtureU64(pIndex + 24u, iOriginalSize);
	iContentCrc = testVfsPackFixtureCrc(pOriginal, iOriginalSize);
	if ( bBadContentCrc ) iContentCrc ^= UINT32_C(0x9E3779B9);
	testVfsPackFixtureU32(pIndex + 32u, iContentCrc);
	memcpy(pIndex + 40u, sPath, iPathSize);
	testVfsPackFixtureU32(pArchive + 72u,
		testVfsPackFixtureCrc(pIndex, iIndexSize));
	pTrailer = pIndex + iIndexSize;
	memcpy(pTrailer, "XRTPEND\0", 8u);
	testVfsPackFixtureU64(pTrailer + 8u, iSize);
	iMetadataCrc = testVfsPackFixtureCrcUpdate(
		UINT32_MAX, pTrailer, 24u);
	iMetadataCrc = testVfsPackFixtureCrcUpdate(
		iMetadataCrc, pArchive, 80u) ^ UINT32_MAX;
	testVfsPackFixtureU32(pTrailer + 24u, iMetadataCrc);
	*pArchiveSize = iSize;
	return pArchive;
}



#if defined(TEST_VFS_PACK_FIXTURE_FILE)

static xfile testVfsPackFixtureFile(const void* pData,
	size_t iSize, str* pPath)
{
	xfile File = xrtFileTemp(NULL, "xrt-vfs-pack-", ".bin", pPath);

	testRequire(File != NULL, "pack fixture temp file creation failed");
	testRequire(xrtWriteFull(File, pData, iSize, NULL),
		"pack fixture temp file write failed");
	testRequire(xrtClose(File), "pack fixture temp file close failed");
	return xrtFileOpen(*pPath, NULL);
}

#endif

#endif
