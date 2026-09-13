#include "../internal/xrt_text_lines.h"

#if defined(XRT_FEATURE_JSONL_WRITE) || defined(XRT_FEATURE_XSONL_WRITE)

typedef struct xtextlinessink {
	const xtextlinesformat* Format;
	xtextlineswriteproc Write;
	ptr UserData;
	size_t Limit;
	size_t Written;
} xtextlinessink;

/* 包含 LF 的累计预算在提交每个块之前检查。 */
static bool __xrtTextLinesSink(xbytesview Data, ptr pUserData)
{
	xtextlinessink* pSink = (xtextlinessink*)pUserData;
	const xerror* pPrevious;
	xerror* pHeld;
	bool bResult;
	if ( Data.Size > pSink->Limit - pSink->Written ) {
		__xrtTextLinesError(pSink->Format, XTEXT_LINES_LIMIT, XERR_RANGE,
			"write", "output exceeds total byte limit", NULL, false);
		return false;
	}
	pPrevious = xrtGetError();
	pHeld = xrtErrorRef(pPrevious);
	bResult = pSink->Write(Data, pSink->UserData);
	if ( bResult ) {
		pSink->Written += Data.Size;
	} else {
		__xrtTextLinesError(pSink->Format, XTEXT_LINES_OUTPUT, XERR_IO,
			"write", "output callback failed", NULL, xrtGetError() != pPrevious);
	}
	xrtErrorFree(pHeld);
	return bResult;
}

/* 外层 Array 使用拥有 backing 的快照；每个元素都是独立文档根。 */
bool __xrtTextLinesWrite(
	const xvalue* pArray, const xtextlineswriteconfig* pConfig,
	const xtextlinesformat* pFormat, xtextlineswriteproc pWrite, ptr pUserData
)
{
	xvalueiter Iterator = { 0 };
	xvalue* pItem;
	xvalueiterresult Result;
	bool bResult = true;
	xtextlinessink Sink = { pFormat, pWrite, pUserData, pConfig->MaxOutputBytes, 0 };
	if ( (pArray == NULL) || (pWrite == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtValueType(pArray) != XVALUE_ARRAY ) {
		__xrtTextLinesError(pFormat, XTEXT_LINES_TYPE, XERR_TYPE,
			"write", "record sequence must be an Array", NULL, false);
		return false;
	}
	if ( xrtValueCount(pArray) > pConfig->MaxRecords ) {
		__xrtTextLinesError(pFormat, XTEXT_LINES_LIMIT, XERR_RANGE,
			"write", "record count exceeds configured limit", NULL, false);
		return false;
	}
	if ( !xrtValueIterBegin(pArray, &Iterator) ) {
		return false;
	}
	while ( (Result = xrtValueIterAdvance(&Iterator, NULL, &pItem)) == XVALUE_ITER_ITEM ) {
		if ( !pConfig->Write(pItem, pConfig->Record, __xrtTextLinesSink, &Sink) ||
			 !__xrtTextLinesSink((xbytesview){ (cbytes)"\n", 1 }, &Sink) ) {
			const xerror* pError = xrtGetError();
			if ( (pError == NULL) || (xrtErrorDomain(pError) == NULL) ||
				 (strcmp(xrtErrorDomain(pError), pFormat->Domain) != 0) ) {
				__xrtTextLinesError(pFormat,
					(pError != NULL && xrtErrorKind(pError) == XERR_RANGE)
						? XTEXT_LINES_LIMIT : XTEXT_LINES_RECORD,
					XERR_STATE, "write", "record could not be encoded", NULL, true);
			}
			bResult = false;
			break;
		}
	}
	if ( bResult && (Result == XVALUE_ITER_ERROR) ) {
		bResult = false;
	}
	{
		xerror* pError = xrtTakeError();
		xrtValueIterEnd(&Iterator);
		xrtSetErrorTake(pError);
	}
	return bResult;
}

static bool __xrtTextLinesBuffer(xbytesview Data, ptr pUserData)
{
	return xrtBufferAppend((xbuffer*)pUserData, Data);
}

/* 先完成全部输出和 NUL，再移交结果；失败不修改 pSize。 */
str __xrtTextLinesStringify(
	const xvalue* pArray, const xtextlineswriteconfig* pConfig,
	const xtextlinesformat* pFormat, size_t* pSize
)
{
	xbuffer Buffer;
	str sText;
	size_t iSize;
	if ( !xrtBufferInit(&Buffer) ) {
		return NULL;
	}
	if ( !__xrtTextLinesWrite(pArray, pConfig, pFormat, __xrtTextLinesBuffer, &Buffer) ||
		 !xrtBufferAppendByte(&Buffer, 0) ) {
		xerror* pError = xrtTakeError();
		xrtBufferUnit(&Buffer);
		xrtSetErrorTake(pError);
		return NULL;
	}
	iSize = Buffer.Size - 1;
	sText = (str)xrtBufferTake(&Buffer, NULL, NULL);
	if ( (sText != NULL) && (pSize != NULL) ) {
		*pSize = iSize;
	}
	return sText;
}
#endif
