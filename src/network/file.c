#include <xrt/net_file.h>
#include "../internal/xrt_internal.h"
#include "../internal/xrt_file.h"
#include "../internal/xrt_net_port.h"



#if defined(XRT_FEATURE_NET_FILE)

/* 校验文件访问、异步打开方式和 Worker 线程归属。 */
static bool __xrtNetFileCheck(
	xnetworker* pWorker,
	xfile File,
	uint32 iAccess,
	xnetcompletion* pCompletion
)
{
	uint32 iFlags;

	if ( (pWorker == NULL) || (File == NULL) ||
		 (pCompletion == NULL) || (pCompletion->Proc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pWorker) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iFlags = xrtFileFlags(File);
	if ( ((iFlags & XFILE_ASYNC) == 0u) ||
		 ((iFlags & iAccess) != iAccess) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 在绑定文件前校验一次完成事件能够表达的缓冲与偏移范围。 */
static bool __xrtNetFileRange(
	uint64 iOffset,
	const void* pData,
	size_t iSize,
	cstr sOperation
)
{
	if ( (pData == NULL) || (iSize == 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iSize > (size_t)INT_MAX) ||
		(iOffset > (uint64)INT64_MAX) ||
		((uint64)(iSize - 1u) > ((uint64)INT64_MAX - iOffset)) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_SUBMIT,
			sOperation,
			"native file operation range is too large",
			0
		);
		return false;
	}
	return true;
}



/* 打开原生完成式文件，不改变调用方的选项对象。 */
XRT_API xfile xrtNetFileOpen(
	cstr sPath,
	const xfileoptions* pOptions
)
{
	xfileoptions Options;

	if ( pOptions == NULL ) {
		xrtFileOptionsInit(&Options);
	} else {
		Options = *pOptions;
	}
	Options.Flags |= XFILE_ASYNC;
	return xrtFileOpen(sPath, &Options);
}



/* 在 Worker 端口提交一次无复制定位读取。 */
XRT_API uint64 xrtNetFileRead(
	xnetworker* pWorker,
	xfile File,
	uint64 iOffset,
	void* pData,
	size_t iSize,
	xnetcompletion* pCompletion
)
{
	uint64 Id;
	xnetport* pPort;
	bool* pAssociated;

	if ( !__xrtNetFileCheck(
		pWorker,
		File,
		XFILE_READ,
		pCompletion
	) ) {
		return 0;
	}
	if ( !__xrtNetFileRange(iOffset, pData, iSize, "read-file") ) {
		return 0;
	}
	pPort = xrtNetWorkerPort(pWorker);
	if ( (pPort == NULL) ||
		((xrtNetPortCapabilities(pPort) & XNET_PORT_CAP_FILE_IO) == 0u) ) {
		__xrtNetSetError(XERR_UNSUPPORTED, XNET_ERROR_PORT_SUBMIT,
			"read-file", "network worker has no native file I/O capability", 0);
		return 0;
	}
	if ( !__xrtFileAsyncBind(
		File,
		__xrtNetPortOwner(pPort),
		&pAssociated
	) ) {
		return 0;
	}
	Id = xrtNetWorkerOperationId(pWorker);
	if ( (Id == 0) || !__xrtNetPortFileRead(
		pPort,
		xrtFileNative(File),
		iOffset,
		pData,
		iSize,
		Id,
		pCompletion,
		pAssociated
	) ) {
		return 0;
	}
	return Id;
}



/* 在 Worker 端口提交一次无复制定位写入。 */
XRT_API uint64 xrtNetFileWrite(
	xnetworker* pWorker,
	xfile File,
	uint64 iOffset,
	const void* pData,
	size_t iSize,
	xnetcompletion* pCompletion
)
{
	uint64 Id;
	xnetport* pPort;
	bool* pAssociated;

	if ( !__xrtNetFileCheck(
		pWorker,
		File,
		XFILE_WRITE,
		pCompletion
	) ) {
		return 0;
	}
	if ( !__xrtNetFileRange(iOffset, pData, iSize, "write-file") ) {
		return 0;
	}
	pPort = xrtNetWorkerPort(pWorker);
	if ( (pPort == NULL) ||
		((xrtNetPortCapabilities(pPort) & XNET_PORT_CAP_FILE_IO) == 0u) ) {
		__xrtNetSetError(XERR_UNSUPPORTED, XNET_ERROR_PORT_SUBMIT,
			"write-file", "network worker has no native file I/O capability", 0);
		return 0;
	}
	if ( !__xrtFileAsyncBind(
		File,
		__xrtNetPortOwner(pPort),
		&pAssociated
	) ) {
		return 0;
	}
	Id = xrtNetWorkerOperationId(pWorker);
	if ( (Id == 0) || !__xrtNetPortFileWrite(
		pPort,
		xrtFileNative(File),
		iOffset,
		pData,
		iSize,
		Id,
		pCompletion,
		pAssociated
	) ) {
		return 0;
	}
	return Id;
}



/* 在端口 owner 上请求取消，并保留原操作唯一终态。 */
XRT_API bool xrtNetFileCancel(
	xnetworker* pWorker,
	uint64 Id
)
{
	if ( (pWorker == NULL) || (Id == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pWorker) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return xrtNetPortCancel(xrtNetWorkerPort(pWorker), Id);
}

#endif
