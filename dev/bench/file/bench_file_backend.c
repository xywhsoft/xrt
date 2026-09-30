#include "../bench_common.h"

#define XRT_MODULE_FILE
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



typedef struct bench_file_state {
	volatile uint64 Calls;
	unsigned char Value;
} bench_file_state;



static volatile uint64 benchFileSink;



/* 最小内存 backend 只测 core 参数检查、能力检查和间接分派。 */
static bool benchFileReadAt(ptr pState, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead)
{
	bench_file_state* pFile = (bench_file_state*)pState;

	(void)iOffset;
	pFile->Calls++;
	if ( iRequest != 0u ) {
		*(unsigned char*)pBuffer = pFile->Value;
	}
	if ( pRead != NULL ) {
		*pRead = iRequest != 0u ? 1u : 0u;
	}
	return true;
}



static bool benchFileClose(ptr pState)
{
	xrtFree(pState);
	return true;
}



static const xrt_file_backend_ops benchFileOps = {
	sizeof(xrt_file_backend_ops),
	XRT_FILE_BACKEND_VERSION,
	XRT_FILE_BACKEND_READ_AT,
	NULL,
	NULL,
	benchFileReadAt,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	benchFileClose
};



static bool benchFileDispatch(uint64 iOperations, uint64* pElapsed)
{
	bench_file_state* pState =
		(bench_file_state*)xrtCalloc(1u, sizeof(*pState));
	xfile File;
	xbenchtimer Timer;
	uint64 iSum = 0u;

	if ( pState == NULL ) {
		return false;
	}
	pState->Value = UINT8_C(0xA5);
	File = __xrtFileTakeBackend(&benchFileOps, pState, XFILE_READ);
	if ( File == NULL ) {
		return false;
	}
	xbenchTimerStart(&Timer);
	for ( uint64 i = 0u; i < iOperations; i++ ) {
		unsigned char iValue;
		size_t iRead;

		if ( !xrtReadAt(File, 0u, &iValue, 1u, &iRead) ||
			 (iRead != 1u) ) {
			(void)xrtClose(File);
			return false;
		}
		iSum += iValue;
	}
	xbenchTimerStop(&Timer);
	*pElapsed = xbenchTimerElapsedNs(&Timer);
	benchFileSink += iSum;
	return xrtClose(File);
}



/* 热缓存单字节定位读取记录真实 native syscall 路径的端到端上限。 */
static bool benchFileNative(uint64 iOperations, uint64* pElapsed)
{
	static const char sPath[] = "xrt-file-backend-benchmark.tmp";
	xfile File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE);
	xbenchtimer Timer;
	uint64 iSum = 0u;
	bool bResult = false;

	if ( (File == NULL) || !xrtWriteFull(File, "N", 1u, NULL) ) {
		if ( File != NULL ) {
			(void)xrtClose(File);
		}
		return false;
	}
	xbenchTimerStart(&Timer);
	for ( uint64 i = 0u; i < iOperations; i++ ) {
		unsigned char iValue;
		size_t iRead;

		if ( !xrtReadAt(File, 0u, &iValue, 1u, &iRead) ||
			 (iRead != 1u) ) {
			goto Exit;
		}
		iSum += iValue;
	}
	xbenchTimerStop(&Timer);
	*pElapsed = xbenchTimerElapsedNs(&Timer);
	benchFileSink += iSum;
	bResult = true;

Exit:
	bResult = xrtClose(File) && bResult;
	bResult = xrtFileDelete(sPath) && bResult;
	return bResult;
}



int main(int argc, char** argv)
{
	uint64 iDispatchOperations = xbenchArgU64(argc, argv, 1, 5000000u);
	uint64 iNativeOperations = xbenchArgU64(argc, argv, 2, 200000u);
	uint64 iDispatchElapsed;
	uint64 iNativeElapsed;

	if ( (iDispatchOperations == 0u) || (iNativeOperations == 0u) ||
		 !benchFileDispatch(iDispatchOperations, &iDispatchElapsed) ||
		 !benchFileNative(iNativeOperations, &iNativeElapsed) ) {
		return 1;
	}
	printf("file_backend_dispatch_ops_per_sec=%.6f\n",
		xbenchSafeRate(iDispatchOperations, iDispatchElapsed));
	printf("file_backend_dispatch_ns_per_op=%.6f\n",
		(double)iDispatchElapsed / (double)iDispatchOperations);
	printf("file_native_read_at_ops_per_sec=%.6f\n",
		xbenchSafeRate(iNativeOperations, iNativeElapsed));
	printf("file_native_read_at_ns_per_op=%.6f\n",
		(double)iNativeElapsed / (double)iNativeOperations);
	return benchFileSink == 0u ? 1 : 0;
}
