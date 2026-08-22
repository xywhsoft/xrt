#include "../internal/xrt_http_static.h"



#if defined(XRT_FEATURE_HTTP_STATIC_FILE)

/* 文件准备任务复制根内路径，并借用根和任务池到任务完成。 */
typedef struct xrt_http_static_file_prepare {
	xtaskpool* Pool;
	xroot Root;
	char Path[];
} xrt_http_static_file_prepare;



/* 设置静态文件域错误，并接管可选原因错误引用。 */
static void __xrtHttpStaticFileError(
	xerrkind Kind,
	xhttpstaticfileerror Code,
	cstr sOperation,
	cstr sMessage,
	xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : Kind;
	Desc.Domain = "http.static.file";
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 把当前执行上下文错误包装为静态文件域错误。 */
static void __xrtHttpStaticFileWrap(
	xerrkind Kind,
	xhttpstaticfileerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtHttpStaticFileError(
		Kind,
		Code,
		sOperation,
		sMessage,
		xrtTakeError()
	);
}



/* 以无前导零的小写十六进制写入一个无符号元数据值。 */
static size_t __xrtHttpStaticFileHex(
	char* sOutput,
	uint64 iValue
)
{
	static const char sHex[] = "0123456789abcdef";
	char Reverse[16];
	size_t iSize = 0;
	size_t i;

	do {
		Reverse[iSize++] = sHex[iValue & UINT64_C(0x0f)];
		iValue >>= 4;
	} while ( iValue != 0 );
	for ( i = 0; i < iSize; i++ ) {
		sOutput[i] = Reverse[iSize - i - 1];
	}
	return iSize;
}



/* 把一个带类型前缀的元数据值追加到弱 ETag opaque-tag。 */
static void __xrtHttpStaticFileTagPart(
	xhttpstaticfile* pFile,
	size_t* pOffset,
	char iPrefix,
	uint64 iValue
)
{
	size_t iOffset = *pOffset;

	if ( iOffset != 0 ) {
		pFile->ETag[iOffset++] = '-';
	}
	pFile->ETag[iOffset++] = iPrefix;
	iOffset += __xrtHttpStaticFileHex(
		pFile->ETag + iOffset,
		iValue
	);
	*pOffset = iOffset;
}



/* 从同一文件句柄的元数据构造默认弱验证器。 */
static void __xrtHttpStaticFileValidators(
	xhttpstaticfile* pFile
)
{
	const xfileinfo* pInfo = &pFile->Info;
	xhttprepresentation* pCurrent =
		&pFile->Representation;
	size_t iTagSize = 0;

	memset(pCurrent, 0, sizeof(*pCurrent));
	pCurrent->Exists = true;
	if ( (pInfo->Available &
		XFILE_INFO_MODIFY_TIME) != 0u ) {
		pCurrent->HasLastModified = true;
		pCurrent->LastModified = pInfo->Modified;
		pCurrent->LastModifiedStrong = false;
		__xrtHttpStaticFileTagPart(
			pFile,
			&iTagSize,
			's',
			pInfo->Size
		);
		__xrtHttpStaticFileTagPart(
			pFile,
			&iTagSize,
			'm',
			(uint64)pInfo->Modified
		);
		if ( (pInfo->Available &
			XFILE_INFO_IDENTITY) != 0u ) {
			__xrtHttpStaticFileTagPart(
				pFile,
				&iTagSize,
				'd',
				pInfo->Device
			);
			__xrtHttpStaticFileTagPart(
				pFile,
				&iTagSize,
				'i',
				pInfo->Identity
			);
		}
		pFile->ETag[iTagSize] = '\0';
		pCurrent->HasETag = true;
		pCurrent->ETag.Opaque.Data = pFile->ETag;
		pCurrent->ETag.Opaque.Size = iTagSize;
		pCurrent->ETag.Weak = true;
	}
}



/* 打开、检查并采用文件根内的同一普通文件句柄。 */
static xhttpstaticfile* __xrtHttpStaticFileOpen(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath,
	xcancel* pCancel
)
{
	xfileoptions Options;
	xhttpstaticfile* pStatic;
	xasyncfile* pAsync;
	xfile File;
	xfileinfo Info;

	if ( (pPool == NULL) || (Root == NULL) ||
		(sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pCancel != NULL) &&
		xrtCancelRequested(pCancel) ) {
		return NULL;
	}
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ;
	File = xrtRootFileOpen(Root, sPath, &Options);
	if ( File == NULL ) {
		__xrtHttpStaticFileWrap(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_OPEN,
			"open",
			"failed to open the root-relative static file"
		);
		return NULL;
	}
	if ( (pCancel != NULL) &&
		xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( !xrtFileStat(File, &Info) ) {
		__xrtHttpStaticFileWrap(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_STAT,
			"stat",
			"failed to inspect the opened static file"
		);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( Info.Type != XFILE_TYPE_FILE ) {
		__xrtHttpStaticFileError(
			XERR_TYPE,
			XHTTP_STATIC_FILE_ERROR_TYPE,
			"stat",
			"the opened static resource is not a regular file",
			NULL
		);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( (Info.Available & XFILE_INFO_SIZE) == 0u ) {
		__xrtHttpStaticFileError(
			XERR_UNSUPPORTED,
			XHTTP_STATIC_FILE_ERROR_SIZE,
			"stat",
			"the opened static file does not provide a stable size",
			NULL
		);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( Info.Size > (uint64)INT64_MAX ) {
		__xrtHttpStaticFileError(
			XERR_RANGE,
			XHTTP_STATIC_FILE_ERROR_SIZE,
			"stat",
			"the static file exceeds the supported HTTP body offset space",
			NULL
		);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	pStatic = (xhttpstaticfile*)xrtCalloc(
		1,
		sizeof(xhttpstaticfile)
	);
	if ( pStatic == NULL ) {
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	pAsync = xrtAsyncFileAdopt(pPool, File);
	if ( pAsync == NULL ) {
		__xrtHttpStaticFileWrap(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_ADOPT,
			"adopt",
			"failed to adopt the opened static file"
		);
		xrtFree(pStatic);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( (pCancel != NULL) &&
		xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseAsync(pAsync);
		xrtFree(pStatic);
		return NULL;
	}
	pStatic->RefCount = 1;
	pStatic->Info = Info;
	xrtAtomicPtrInit(&pStatic->File, pAsync);
	__xrtHttpStaticFileValidators(pStatic);
	return pStatic;
}



/* Future 成功值释放时归还静态文件资源引用。 */
static void __xrtHttpStaticFileValueFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpStaticFileDestroy(
		(xhttpstaticfile*)pValue
	);
}



/* 在任务池工作线程内执行受根约束的文件准备。 */
static xtaskoutcome __xrtHttpStaticFileTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_http_static_file_prepare* pPrepare =
		(xrt_http_static_file_prepare*)pData;
	xhttpstaticfile* pFile;

	pFile = __xrtHttpStaticFileOpen(
		pPrepare->Pool,
		pPrepare->Root,
		pPrepare->Path,
		pCancel
	);
	if ( pFile == NULL ) {
		return xrtCancelRequested(pCancel) ?
			XTASK_CANCELLED : XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		xrtHttpStaticFileDestroy(pFile);
		return XTASK_CANCELLED;
	}
	pResult->Value = pFile;
	pResult->Destroy = __xrtHttpStaticFileValueFree;
	return XTASK_SUCCESS;
}



/* 释放任务池已经受理的静态文件准备参数。 */
static void __xrtHttpStaticFilePrepareFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtFree(pValue);
}



/* 同步打开根内静态文件资源。 */
XRT_API xhttpstaticfile* xrtHttpStaticFileOpen(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath
)
{
	return __xrtHttpStaticFileOpen(
		pPool,
		Root,
		sPath,
		NULL
	);
}



/* 复制根内路径并提交静态文件准备任务。 */
XRT_API xfuture* xrtHttpStaticFileFuture(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath
)
{
	xrt_http_static_file_prepare* pPrepare;
	xtaskargs Args;
	xfuture* pFuture;
	size_t iPathLength;

	if ( (pPool == NULL) || (Root == NULL) ||
		(sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iPathLength = strlen(sPath);
	if ( iPathLength > (SIZE_MAX -
		sizeof(xrt_http_static_file_prepare) - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pPrepare = (xrt_http_static_file_prepare*)xrtMalloc(
		sizeof(xrt_http_static_file_prepare) +
		iPathLength + 1u
	);
	if ( pPrepare == NULL ) {
		return NULL;
	}
	pPrepare->Pool = pPool;
	pPrepare->Root = Root;
	memcpy(
		pPrepare->Path,
		sPath,
		iPathLength + 1u
	);
	memset(&Args, 0, sizeof(Args));
	Args.Destroy = __xrtHttpStaticFilePrepareFree;
	pFuture = xrtTaskSubmit(
		pPool,
		__xrtHttpStaticFileTask,
		pPrepare,
		&Args
	);
	if ( pFuture == NULL ) {
		__xrtHttpStaticFileWrap(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_SUBMIT,
			"submit",
			"failed to submit static file preparation"
		);
		xrtFree(pPrepare);
	}
	return pFuture;
}



/* 增加静态文件资源引用。 */
XRT_API xhttpstaticfile* xrtHttpStaticFileRef(
	xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pFile->RefCount) < 0 ) {
		__xrtHttpStaticFileError(
			XERR_STATE,
			XHTTP_STATIC_FILE_ERROR_REFERENCE,
			"retain",
			"the static file reference count cannot be increased",
			NULL
		);
		return NULL;
	}
	return pFile;
}



/* 释放资源引用，并异步关闭尚未取走的文件。 */
XRT_API void xrtHttpStaticFileDestroy(
	xhttpstaticfile* pFile
)
{
	xasyncfile* pAsync;

	if ( (pFile == NULL) ||
		(xrtRefRelease(&pFile->RefCount) != 0) ) {
		return;
	}
	pAsync = (xasyncfile*)xrtAtomicPtrExchange(
		&pFile->File,
		NULL,
		XMEMORY_ACQ_REL
	);
	__xrtHttpBodyFileCloseAsync(pAsync);
	xrtFree(pFile);
}



/* 返回最终文件句柄的借用元数据。 */
XRT_API const xfileinfo* xrtHttpStaticFileInfo(
	const xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return &pFile->Info;
}



/* 返回与文件资源同生命周期的借用表示验证器。 */
XRT_API const xhttprepresentation* xrtHttpStaticFileRepresentation(
	const xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return &pFile->Representation;
}



/* 返回完整静态文件长度。 */
XRT_API uint64 xrtHttpStaticFileSize(
	const xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pFile->Info.Size;
}



/* 一次性取走静态文件持有的异步文件资源。 */
XRT_API xasyncfile* xrtHttpStaticFileTakeFile(
	xhttpstaticfile* pFile
)
{
	xasyncfile* pAsync;

	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pAsync = (xasyncfile*)xrtAtomicPtrExchange(
		&pFile->File,
		NULL,
		XMEMORY_ACQ_REL
	);
	if ( pAsync == NULL ) {
		__xrtHttpStaticFileError(
			XERR_STATE,
			XHTTP_STATIC_FILE_ERROR_CONSUMED,
			"take-file",
			"the static file resource has already been consumed",
			NULL
		);
	}
	return pAsync;
}



/* 一次性采用底层异步文件并创建严格区间正文。 */
XRT_API xhttpbody* xrtHttpStaticFileTakeBody(
	xhttpstaticfile* pFile,
	uint64 iOffset,
	uint64 iLength
)
{
	xasyncfile* pAsync;
	xhttpbody* pBody;

	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (iOffset > pFile->Info.Size) ||
		(iLength > (pFile->Info.Size - iOffset)) ||
		(iOffset > (uint64)INT64_MAX) ||
		(iLength > ((uint64)INT64_MAX - iOffset)) ) {
		__xrtHttpStaticFileError(
			XERR_RANGE,
			XHTTP_STATIC_FILE_ERROR_RANGE,
			"take-body",
			"the requested body range exceeds the opened static file",
			NULL
		);
		return NULL;
	}
	pAsync = (xasyncfile*)xrtAtomicPtrExchange(
		&pFile->File,
		NULL,
		XMEMORY_ACQ_REL
	);
	if ( pAsync == NULL ) {
		__xrtHttpStaticFileError(
			XERR_STATE,
			XHTTP_STATIC_FILE_ERROR_CONSUMED,
			"take-body",
			"the static file resource has already been consumed",
			NULL
		);
		return NULL;
	}
	pBody = xrtHttpBodyFileAdopt(
		pAsync,
		iOffset,
		iLength,
		NULL
	);
	if ( pBody == NULL ) {
		xerror* pCause = xrtTakeError();
		ptr pExpected = NULL;

		if ( !xrtAtomicPtrCompareExchange(
			&pFile->File,
			&pExpected,
			pAsync,
			XMEMORY_RELEASE,
			XMEMORY_ACQUIRE
		) ) {
			__xrtHttpBodyFileCloseAsync(pAsync);
		}
		__xrtHttpStaticFileError(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_BODY,
			"take-body",
			"failed to create a body from the opened static file",
			pCause
		);
	}
	return pBody;
}



/* 一次性创建完整静态文件正文。 */
XRT_API xhttpbody* xrtHttpStaticFileTakeBodyAll(
	xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtHttpStaticFileTakeBody(
		pFile,
		0,
		pFile->Info.Size
	);
}

#endif
