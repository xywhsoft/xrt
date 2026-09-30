#ifdef FILE_BACKEND_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include "../../src/internal/xrt_file.h"
#endif

#include "../test.h"



typedef struct test_file_backend_observer {
	uint32 Closes;
} test_file_backend_observer;



typedef struct test_file_backend_state {
	test_file_backend_observer* Observer;
	unsigned char Data[32];
	size_t Size;
	size_t Cursor;
	uint32 Reads;
	uint32 Writes;
	uint32 ReadAts;
	uint32 WriteAts;
	uint32 Seeks;
	uint32 Stats;
	uint32 Resizes;
	uint32 Flushes;
} test_file_backend_state;



static test_file_backend_state* testFileBackendState(
	test_file_backend_observer* pObserver)
{
	test_file_backend_state* pState =
		(test_file_backend_state*)xrtCalloc(1u, sizeof(*pState));

	testRequire(pState != NULL, "test backend state allocation failed");
	pState->Observer = pObserver;
	return pState;
}



static bool testFileBackendRead(ptr pState, ptr pBuffer,
	size_t iRequest, size_t* pRead)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;
	size_t iAvailable = (pFile->Cursor < pFile->Size) ?
		(pFile->Size - pFile->Cursor) : 0u;
	size_t iDone = iRequest < iAvailable ? iRequest : iAvailable;

	pFile->Reads++;
	if ( iDone != 0u ) {
		memcpy(pBuffer, pFile->Data + pFile->Cursor, iDone);
		pFile->Cursor += iDone;
	}
	if ( pRead != NULL ) {
		*pRead = iDone;
	}
	return true;
}



static bool testFileBackendWrite(ptr pState, const void* pBuffer,
	size_t iRequest, size_t* pWritten)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;

	pFile->Writes++;
	if ( (pFile->Cursor > sizeof(pFile->Data)) ||
		 (iRequest > (sizeof(pFile->Data) - pFile->Cursor)) ) {
		return false;
	}
	if ( iRequest != 0u ) {
		memcpy(pFile->Data + pFile->Cursor, pBuffer, iRequest);
		pFile->Cursor += iRequest;
		if ( pFile->Cursor > pFile->Size ) {
			pFile->Size = pFile->Cursor;
		}
	}
	if ( pWritten != NULL ) {
		*pWritten = iRequest;
	}
	return true;
}



static bool testFileBackendReadAt(ptr pState, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;
	size_t iStart = (size_t)iOffset;
	size_t iAvailable = (iStart < pFile->Size) ?
		(pFile->Size - iStart) : 0u;
	size_t iDone = iRequest < iAvailable ? iRequest : iAvailable;

	pFile->ReadAts++;
	if ( iDone != 0u ) {
		memcpy(pBuffer, pFile->Data + iStart, iDone);
	}
	if ( pRead != NULL ) {
		*pRead = iDone;
	}
	return true;
}



static bool testFileBackendWriteAt(ptr pState, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;
	size_t iStart = (size_t)iOffset;

	pFile->WriteAts++;
	if ( (iStart > sizeof(pFile->Data)) ||
		 (iRequest > (sizeof(pFile->Data) - iStart)) ) {
		return false;
	}
	if ( iRequest != 0u ) {
		memcpy(pFile->Data + iStart, pBuffer, iRequest);
		if ((iStart + iRequest) > pFile->Size ) {
			pFile->Size = iStart + iRequest;
		}
	}
	if ( pWritten != NULL ) {
		*pWritten = iRequest;
	}
	return true;
}



static bool testFileBackendSeek(ptr pState, int64 iOffset,
	xseek Origin, uint64* pPosition)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;
	int64 iBase = (Origin == XSEEK_START) ? 0 :
		((Origin == XSEEK_CURRENT) ? (int64)pFile->Cursor :
		(int64)pFile->Size);
	int64 iPosition = iBase + iOffset;

	pFile->Seeks++;
	if ( (iPosition < 0) || ((uint64)iPosition > sizeof(pFile->Data)) ) {
		return false;
	}
	pFile->Cursor = (size_t)iPosition;
	if ( pPosition != NULL ) {
		*pPosition = (uint64)iPosition;
	}
	return true;
}



static bool testFileBackendStat(ptr pState, xfileinfo* pInfo)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;

	pFile->Stats++;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = (uint64)pFile->Size;
	return true;
}



static bool testFileBackendResize(ptr pState, uint64 iSize)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;

	pFile->Resizes++;
	if ( iSize > sizeof(pFile->Data) ) {
		return false;
	}
	pFile->Size = (size_t)iSize;
	if ( pFile->Cursor > pFile->Size ) {
		pFile->Cursor = pFile->Size;
	}
	return true;
}



static bool testFileBackendFlush(ptr pState)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;

	pFile->Flushes++;
	return true;
}



static bool testFileBackendClose(ptr pState)
{
	test_file_backend_state* pFile = (test_file_backend_state*)pState;

	pFile->Observer->Closes++;
	xrtFree(pFile);
	return true;
}



static const xrt_file_backend_ops testFileBackendOps = {
	sizeof(xrt_file_backend_ops),
	XRT_FILE_BACKEND_VERSION,
	XRT_FILE_BACKEND_READ | XRT_FILE_BACKEND_WRITE |
	XRT_FILE_BACKEND_READ_AT | XRT_FILE_BACKEND_WRITE_AT |
	XRT_FILE_BACKEND_SEEK | XRT_FILE_BACKEND_STAT |
	XRT_FILE_BACKEND_RESIZE | XRT_FILE_BACKEND_FLUSH,
	testFileBackendRead,
	testFileBackendWrite,
	testFileBackendReadAt,
	testFileBackendWriteAt,
	testFileBackendSeek,
	testFileBackendStat,
	testFileBackendResize,
	testFileBackendFlush,
	NULL,
	NULL,
	NULL,
	testFileBackendClose
};



static void testFileBackendDispatch(void)
{
	test_file_backend_observer Observer = { 0u };
	test_file_backend_state* pState = testFileBackendState(&Observer);
	xfile File = __xrtFileTakeBackend(&testFileBackendOps,
		pState, XFILE_READ | XFILE_WRITE);
	char arrRead[4] = { 0, 0, 0, 0 };
	xfileinfo Info;
	size_t iDone;
	uint64 iPosition;

	testRequire(File != NULL, "test backend construction failed");
	testRequire(xrtWrite(File, "abc", 3u, &iDone) && (iDone == 3u),
		"test backend write dispatch failed");
	testRequire(xrtSeek(File, 0, XSEEK_START, &iPosition) &&
		(iPosition == 0u), "test backend seek dispatch failed");
	testRequire(xrtRead(File, arrRead, 3u, &iDone) && (iDone == 3u) &&
		(memcmp(arrRead, "abc", 3u) == 0),
		"test backend read dispatch failed");
	testRequire(xrtWriteAt(File, 1u, "Z", 1u, &iDone) &&
		(iDone == 1u), "test backend write-at dispatch failed");
	testRequire(xrtReadAt(File, 0u, arrRead, 3u, &iDone) &&
		(iDone == 3u) && (memcmp(arrRead, "aZc", 3u) == 0),
		"test backend read-at dispatch failed");
	testRequire(xrtFileStat(File, &Info) && (Info.Size == 3u),
		"test backend stat dispatch failed");
	testRequire(xrtFileResize(File, 2u) && xrtFlush(File),
		"test backend resize or flush dispatch failed");
	testRequire((pState->Reads == 1u) && (pState->Writes == 1u) &&
		(pState->ReadAts == 1u) && (pState->WriteAts == 1u) &&
		(pState->Seeks == 1u) && (pState->Stats == 1u) &&
		(pState->Resizes == 1u) && (pState->Flushes == 1u),
		"test backend dispatch counts are incorrect");

	testRequire(xrtFileNative(File) == (intptr_t)-1,
		"non-native backend exposed a native handle");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"native handle rejection reported the wrong error");
	xrtClearError();
	testRequire(!xrtFileLock(File, XFILE_LOCK_SHARED, false),
		"non-native backend accepted a native file lock");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"native lock rejection reported the wrong error");
	xrtClearError();
	testRequire(xrtFileMap(File, 0u, 1u, XFILE_MAP_READ) == NULL,
		"non-native backend accepted a native file mapping");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"native mapping rejection reported the wrong error");
	xrtClearError();
	testRequire(xrtClose(File) && (Observer.Closes == 1u),
		"test backend was not closed exactly once");
}



static void testFileBackendValidation(void)
{
	test_file_backend_observer Observer = { 0u };
	xrt_file_backend_ops Invalid = testFileBackendOps;
	test_file_backend_state* pState;

	Invalid.Capabilities &= ~XRT_FILE_BACKEND_WRITE;
	pState = testFileBackendState(&Observer);
	testRequire(__xrtFileTakeBackend(&Invalid, pState,
		XFILE_READ | XFILE_WRITE) == NULL,
		"backend accepted a capability and callback mismatch");
	testRequire((Observer.Closes == 1u) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid backend was not consumed exactly once");
	xrtClearError();
}



#if defined(XRT_FEATURE_NET_FILE)
static void testFileBackendAsyncCapability(void)
{
	test_file_backend_observer Observer = { 0u };
	test_file_backend_state* pState = testFileBackendState(&Observer);
	xfile File = __xrtFileTakeBackend(&testFileBackendOps, pState,
		XFILE_READ | XFILE_WRITE | XFILE_ASYNC);
	bool* pAssociated = NULL;

	testRequire(File != NULL,
		"async capability test backend construction failed");
	testRequire(!__xrtFileAsyncBind(File, 1u, &pAssociated),
		"non-native backend accepted native completion binding");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"async binding rejection reported the wrong error");
	xrtClearError();
	testRequire(xrtClose(File) && (Observer.Closes == 1u),
		"async capability test backend was not closed exactly once");
}
#endif



static void testFileBackendConstructionOOM(void)
{
	test_file_backend_observer Observer = { 0u };
	test_file_backend_state* pState = testFileBackendState(&Observer);

	testRequire(xrtMemDebugFailAfter(0u),
		"backend construction OOM injection setup failed");
	testRequire(__xrtFileTakeBackend(&testFileBackendOps, pState,
		XFILE_READ | XFILE_WRITE) == NULL,
		"backend wrapper survived construction OOM");
	testRequire(xrtMemDebugFailTriggered() &&
		(Observer.Closes == 1u) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"backend construction OOM violated ownership or error contract");
	xrtMemDebugFailClear();
	xrtClearError();
}



int main(void)
{
	testFileBackendDispatch();
	testFileBackendValidation();
	#if defined(XRT_FEATURE_NET_FILE)
		testFileBackendAsyncCapability();
	#endif
	testFileBackendConstructionOOM();
	testMemoryDebugDrain("file backend tests leaked memory");
	return 0;
}
