#include "../internal/xrt_http_static.h"



#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)

#define XRT_HTTP_STATIC_MULTIPART_HEAD   1u
#define XRT_HTTP_STATIC_MULTIPART_FILE   2u
#define XRT_HTTP_STATIC_MULTIPART_END    3u
#define XRT_HTTP_STATIC_MULTIPART_CLOSE  4u
#define XRT_HTTP_STATIC_MULTIPART_DONE   5u



/* 每个 Part 保存自己的文件范围和预生成头部位置。 */
typedef struct xrt_http_static_multipart_part {
	xhttpbyterange Range;
	size_t HeadOffset;
	size_t HeadSize;
} xrt_http_static_multipart_part;



/* 工厂在一个分配块中拥有范围表和全部固定线缆片段。 */
typedef struct xrt_http_static_multipart_factory {
	volatile int32 RefCount;
	xasyncfile* File;
	uint64 Length;
	size_t PartCount;
	size_t EndOffset;
	size_t EndSize;
	size_t CloseOffset;
	size_t CloseSize;
	xrt_http_static_multipart_part Parts[];
} xrt_http_static_multipart_factory;



/* Reader 在固定片段和共享异步文件 cursor 之间切换。 */
typedef struct xrt_http_static_multipart_reader {
	xrt_http_static_multipart_factory* Factory;
	xrt_http_body_file_cursor Cursor;
	size_t Part;
	size_t MetaOffset;
	size_t MetaRemaining;
	uint32 State;
} xrt_http_static_multipart_reader;



/* 设置静态多范围正文采用错误。 */
static void __xrtHttpStaticMultipartBodyError(
	xerrkind Kind,
	xhttpstaticmultipartbodyerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "http.static.multipart.body";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 返回工厂分配块中紧随 Part 表之后的线缆数据。 */
static bytes __xrtHttpStaticMultipartMetadata(
	xrt_http_static_multipart_factory* pFactory
)
{
	return (bytes)(
		pFactory->Parts + pFactory->PartCount
	);
}



/* 增加工厂引用，保护可能晚于 Reader 释放的元数据 Chunk。 */
static bool __xrtHttpStaticMultipartFactoryRef(
	xrt_http_static_multipart_factory* pFactory
)
{
	if ( (pFactory == NULL) ||
		(xrtRefRetain(&pFactory->RefCount) < 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 释放工厂最后一个引用并关闭尚未打开 Reader 的文件。 */
static void __xrtHttpStaticMultipartFactoryRelease(
	xrt_http_static_multipart_factory* pFactory
)
{
	if ( (pFactory == NULL) ||
		(xrtRefRelease(&pFactory->RefCount) != 0) ) {
		return;
	}
	__xrtHttpBodyFileCloseAsync(pFactory->File);
	pFactory->File = NULL;
	xrtFree(pFactory);
}



/* 元数据 Chunk 释放时归还独立工厂引用。 */
static void __xrtHttpStaticMultipartChunkRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	__xrtHttpStaticMultipartFactoryRelease(
		(xrt_http_static_multipart_factory*)pContext
	);
}



/* 安全累加单个工厂分配块的大小。 */
static bool __xrtHttpStaticMultipartAllocAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 测量并分配一个尚未采用文件的多范围工厂。 */
static xrt_http_static_multipart_factory*
__xrtHttpStaticMultipartFactoryCreate(
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary
)
{
	xrt_http_static_multipart_factory* pFactory;
	bytes pMetadata;
	uint64 iLength;
	size_t iAllocate = sizeof(*pFactory);
	size_t iBaseSize;
	size_t iMetadata = 0;
	size_t iMetadataCapacity;
	size_t iEndSize;
	size_t iCloseSize;
	size_t iWritten;
	xhttpbyterange Range;
	size_t i;

	if ( (iCompleteLength > (uint64)INT64_MAX) ||
		!__xrtHttpRangeMultipartLength(
			pRanges,
			iRangeCount,
			iCompleteLength,
			ContentType,
			Boundary,
			&iLength
		) ) {
		if ( iCompleteLength > (uint64)INT64_MAX ) {
			__xrtErrorSetRange();
		}
		return NULL;
	}
	if ( iRangeCount >
		((SIZE_MAX - iAllocate) /
		 sizeof(xrt_http_static_multipart_part)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAllocate += iRangeCount *
		sizeof(xrt_http_static_multipart_part);
	iBaseSize = iAllocate;
	for ( i = 0; i < iRangeCount; i++ ) {
		size_t iHeadSize;
		uint64 iRangeLength;

		memcpy(
			&Range,
			(const uint8*)pRanges + (i * sizeof(Range)),
			sizeof(Range)
		);
		iRangeLength =
			(Range.Last - Range.First) +
			UINT64_C(1);

		if ( !__xrtHttpBodyFileRangeValid(
				Range.First,
				iRangeLength
			) || !xrtHttpRangeMultipartHeadWrite(
				&Range,
				iCompleteLength,
				ContentType,
				Boundary,
				NULL,
				0,
				&iHeadSize
			) || !__xrtHttpStaticMultipartAllocAdd(
				&iMetadata,
				iHeadSize
			) ) {
			return NULL;
		}
	}
	if ( !xrtHttpRangeMultipartEndWrite(
			NULL,
			0,
			&iEndSize
		) || !xrtHttpRangeMultipartCloseWrite(
			Boundary,
			NULL,
			0,
			&iCloseSize
		) || !__xrtHttpStaticMultipartAllocAdd(
			&iMetadata,
			iEndSize
		) || !__xrtHttpStaticMultipartAllocAdd(
			&iMetadata,
			iCloseSize
		) || !__xrtHttpStaticMultipartAllocAdd(
			&iAllocate,
			iMetadata
		) ) {
		return NULL;
	}
	pFactory = (xrt_http_static_multipart_factory*)
		xrtCalloc(1, iAllocate);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pFactory->RefCount = 1;
	pFactory->Length = iLength;
	pFactory->PartCount = iRangeCount;
	pMetadata = __xrtHttpStaticMultipartMetadata(
		pFactory
	);
	iMetadataCapacity = iAllocate - iBaseSize;
	iMetadata = 0;
	for ( i = 0; i < iRangeCount; i++ ) {
		memcpy(
			&Range,
			(const uint8*)pRanges + (i * sizeof(Range)),
			sizeof(Range)
		);
		pFactory->Parts[i].Range = Range;
		pFactory->Parts[i].HeadOffset = iMetadata;
		if ( !xrtHttpRangeMultipartHeadWrite(
				&pFactory->Parts[i].Range,
				iCompleteLength,
				ContentType,
				Boundary,
				pMetadata + iMetadata,
				iMetadataCapacity - iMetadata,
				&iWritten
			) ) {
			__xrtHttpStaticMultipartFactoryRelease(
				pFactory
			);
			return NULL;
		}
		pFactory->Parts[i].HeadSize = iWritten;
		iMetadata += iWritten;
	}
	pFactory->EndOffset = iMetadata;
	if ( !xrtHttpRangeMultipartEndWrite(
			pMetadata + iMetadata,
			iEndSize,
			&iWritten
		) || (iWritten != iEndSize) ) {
		__xrtHttpStaticMultipartFactoryRelease(pFactory);
		return NULL;
	}
	pFactory->EndSize = iWritten;
	iMetadata += iWritten;
	pFactory->CloseOffset = iMetadata;
	if ( !xrtHttpRangeMultipartCloseWrite(
			Boundary,
			pMetadata + iMetadata,
			iCloseSize,
			&iWritten
		) || (iWritten != iCloseSize) ) {
		__xrtHttpStaticMultipartFactoryRelease(pFactory);
		return NULL;
	}
	pFactory->CloseSize = iWritten;
	iMetadata += iWritten;
	if ( iMetadata != iMetadataCapacity ) {
		__xrtErrorSetInternal();
		__xrtHttpStaticMultipartFactoryRelease(pFactory);
		return NULL;
	}
	return pFactory;
}



/* 开始发布一个工厂内固定线缆片段。 */
static void __xrtHttpStaticMultipartMetaBegin(
	xrt_http_static_multipart_reader* pReader,
	size_t iOffset,
	size_t iSize
)
{
	pReader->MetaOffset = iOffset;
	pReader->MetaRemaining = iSize;
}



/* 发布不超过调用方上限的固定线缆 Chunk。 */
static xhttpbodystatus __xrtHttpStaticMultipartMetaNext(
	xrt_http_static_multipart_reader* pReader,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	size_t iSize;

	if ( pReader->MetaRemaining == 0 ) {
		return XHTTP_BODY_EOF;
	}
	if ( !__xrtHttpStaticMultipartFactoryRef(
		pReader->Factory
	) ) {
		return XHTTP_BODY_ERROR;
	}
	iSize = pReader->MetaRemaining < iMaxBytes ?
		pReader->MetaRemaining : iMaxBytes;
	pChunk->Data = __xrtHttpStaticMultipartMetadata(
		pReader->Factory
	) + pReader->MetaOffset;
	pChunk->Size = iSize;
	pChunk->Release =
		__xrtHttpStaticMultipartChunkRelease;
	pChunk->Context = pReader->Factory;
	pReader->MetaOffset += iSize;
	pReader->MetaRemaining -= iSize;
	return XHTTP_BODY_DATA;
}



/* 推进 Part 头、文件数据、段尾和关闭边界状态机。 */
static xhttpbodystatus __xrtHttpStaticMultipartReaderNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xrt_http_static_multipart_reader* pReader =
		(xrt_http_static_multipart_reader*)pContext;

	if ( pReader == NULL ) {
		__xrtErrorSetInvalidState();
		return XHTTP_BODY_ERROR;
	}
	for ( ;; ) {
		xhttpbodystatus Status;

		switch ( pReader->State ) {
			case XRT_HTTP_STATIC_MULTIPART_HEAD:
				Status =
					__xrtHttpStaticMultipartMetaNext(
						pReader,
						iMaxBytes,
						pChunk
					);
				if ( Status != XHTTP_BODY_EOF ) {
					return Status;
				}
				__xrtHttpBodyFileCursorInit(
					&pReader->Cursor,
					pReader->Factory->File,
					pReader->Factory
						->Parts[pReader->Part]
						.Range.First,
					(pReader->Factory
						->Parts[pReader->Part]
						.Range.Last -
					 pReader->Factory
						->Parts[pReader->Part]
						.Range.First) +
						UINT64_C(1),
					XHTTP_BODY_FILE_READ_DEFAULT
				);
				pReader->State =
					XRT_HTTP_STATIC_MULTIPART_FILE;
				break;

			case XRT_HTTP_STATIC_MULTIPART_FILE:
				Status = __xrtHttpBodyFileCursorNext(
					&pReader->Cursor,
					iMaxBytes,
					pChunk
				);
				if ( Status != XHTTP_BODY_EOF ) {
					return Status;
				}
				__xrtHttpStaticMultipartMetaBegin(
					pReader,
					pReader->Factory->EndOffset,
					pReader->Factory->EndSize
				);
				pReader->State =
					XRT_HTTP_STATIC_MULTIPART_END;
				break;

			case XRT_HTTP_STATIC_MULTIPART_END:
				Status =
					__xrtHttpStaticMultipartMetaNext(
						pReader,
						iMaxBytes,
						pChunk
					);
				if ( Status != XHTTP_BODY_EOF ) {
					return Status;
				}
				pReader->Part++;
				if ( pReader->Part <
					pReader->Factory->PartCount ) {
					__xrtHttpStaticMultipartMetaBegin(
						pReader,
						pReader->Factory
							->Parts[pReader->Part]
							.HeadOffset,
						pReader->Factory
							->Parts[pReader->Part]
							.HeadSize
					);
					pReader->State =
						XRT_HTTP_STATIC_MULTIPART_HEAD;
				} else {
					__xrtHttpStaticMultipartMetaBegin(
						pReader,
						pReader->Factory->CloseOffset,
						pReader->Factory->CloseSize
					);
					pReader->State =
						XRT_HTTP_STATIC_MULTIPART_CLOSE;
				}
				break;

			case XRT_HTTP_STATIC_MULTIPART_CLOSE:
				Status =
					__xrtHttpStaticMultipartMetaNext(
						pReader,
						iMaxBytes,
						pChunk
					);
				if ( Status != XHTTP_BODY_EOF ) {
					return Status;
				}
				pReader->State =
					XRT_HTTP_STATIC_MULTIPART_DONE;
				break;

			case XRT_HTTP_STATIC_MULTIPART_DONE:
				return XHTTP_BODY_EOF;

			default:
				__xrtErrorSetInvalidState();
				return XHTTP_BODY_ERROR;
		}
	}
}



/* 在文件 cursor 返回 AGAIN 后取得当前读取 Future。 */
static xfuture* __xrtHttpStaticMultipartReaderWait(
	ptr pContext
)
{
	xrt_http_static_multipart_reader* pReader =
		(xrt_http_static_multipart_reader*)pContext;

	if ( (pReader == NULL) ||
		(pReader->State !=
		 XRT_HTTP_STATIC_MULTIPART_FILE) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return __xrtHttpBodyFileCursorWait(
		&pReader->Cursor
	);
}



/* 取消待处理读取并异步关闭工厂采用的文件。 */
static void __xrtHttpStaticMultipartReaderClose(
	ptr pContext
)
{
	xrt_http_static_multipart_reader* pReader =
		(xrt_http_static_multipart_reader*)pContext;
	xasyncfile* pFile;

	if ( pReader == NULL ) {
		return;
	}
	__xrtHttpBodyFileCursorCancel(&pReader->Cursor);
	pFile = pReader->Factory->File;
	pReader->Factory->File = NULL;
	__xrtHttpBodyFileCloseAsync(pFile);
	xrtFree(pReader);
}



/* 快速创建一个从首个 Part 头开始的单消费 Reader。 */
static bool __xrtHttpStaticMultipartBodyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	xrt_http_static_multipart_factory* pMultipart =
		(xrt_http_static_multipart_factory*)pFactory;
	xrt_http_static_multipart_reader* pReader;

	if ( (pMultipart == NULL) ||
		(pMultipart->File == NULL) ||
		(pMultipart->PartCount == 0) ||
		(pOps == NULL) || (ppReader == NULL) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pReader = (xrt_http_static_multipart_reader*)
		xrtCalloc(1, sizeof(*pReader));
	if ( pReader == NULL ) {
		return false;
	}
	pReader->Factory = pMultipart;
	pReader->State = XRT_HTTP_STATIC_MULTIPART_HEAD;
	__xrtHttpStaticMultipartMetaBegin(
		pReader,
		pMultipart->Parts[0].HeadOffset,
		pMultipart->Parts[0].HeadSize
	);
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = __xrtHttpStaticMultipartReaderNext;
	pOps->Close = __xrtHttpStaticMultipartReaderClose;
	pOps->Wait = __xrtHttpStaticMultipartReaderWait;
	*ppReader = pReader;
	return true;
}



/* 正文释放时归还工厂自身的初始引用。 */
static void __xrtHttpStaticMultipartBodyDestroy(
	ptr pFactory
)
{
	__xrtHttpStaticMultipartFactoryRelease(
		(xrt_http_static_multipart_factory*)pFactory
	);
}



/* 为已完成的工厂创建不可重放正文对象。 */
static xhttpbody* __xrtHttpStaticMultipartBodyCreate(
	xrt_http_static_multipart_factory* pFactory
)
{
	static const xhttpbodyops Ops = {
		__xrtHttpStaticMultipartBodyOpen,
		__xrtHttpStaticMultipartBodyDestroy
	};

	return xrtHttpBodyCreate(
		&Ops,
		pFactory,
		pFactory->Length,
		XHTTP_BODY_NONE
	);
}



/* 采用可读异步文件并创建多范围正文。 */
XRT_API xhttpbody* xrtHttpStaticMultipartBodyAdopt(
	xasyncfile* pFile,
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary
)
{
	xrt_http_static_multipart_factory* pFactory;
	xhttpbody* pBody;

	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (xrtAsyncFileFlags(pFile) & XFILE_READ) == 0u ) {
		__xrtHttpStaticMultipartBodyError(
			XERR_ARGUMENT,
			XHTTP_STATIC_MULTIPART_BODY_ERROR_ADOPT,
			"adopt",
			"the adopted multipart file is not readable"
		);
		return NULL;
	}
	pFactory = __xrtHttpStaticMultipartFactoryCreate(
		pRanges,
		iRangeCount,
		iCompleteLength,
		ContentType,
		Boundary
	);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pBody = __xrtHttpStaticMultipartBodyCreate(
		pFactory
	);
	if ( pBody == NULL ) {
		__xrtHttpStaticMultipartFactoryRelease(
			pFactory
		);
		return NULL;
	}
	pFactory->File = pFile;
	return pBody;
}



/* 构造成功后才从静态文件资源取走底层异步文件。 */
XRT_API xhttpbody* xrtHttpStaticFileTakeMultipartBody(
	xhttpstaticfile* pFile,
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	xstrview ContentType,
	xstrview Boundary
)
{
	xrt_http_static_multipart_factory* pFactory;
	xasyncfile* pAsync;
	xhttpbody* pBody;

	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFactory = __xrtHttpStaticMultipartFactoryCreate(
		pRanges,
		iRangeCount,
		xrtHttpStaticFileSize(pFile),
		ContentType,
		Boundary
	);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pBody = __xrtHttpStaticMultipartBodyCreate(
		pFactory
	);
	if ( pBody == NULL ) {
		__xrtHttpStaticMultipartFactoryRelease(
			pFactory
		);
		return NULL;
	}
	pAsync = xrtHttpStaticFileTakeFile(pFile);
	if ( pAsync == NULL ) {
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	pFactory->File = pAsync;
	return pBody;
}

#endif
