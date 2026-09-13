#include "../internal/xrt_text_lines.h"

#include <stdio.h>

#if defined(XRT_FEATURE_JSONL_CORE) || defined(XRT_FEATURE_XSONL_CORE)

/* 新错误保留底层原因；如果包装本身 OOM，恢复原始错误。 */
void __xrtTextLinesError(
	const xtextlinesformat* pFormat, xtextlineserror Code, xerrkind Kind,
	cstr sOperation, cstr sMessage, const xtextlineslocation* pLocation, bool bCause
)
{
	char Data[192];
	xerror* pCause = bCause ? xrtTakeError() : NULL;
	xerror* pError;
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : Kind;
	Desc.Code = pFormat->ErrorBase + (int32)Code;
	Desc.Domain = pFormat->Domain;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	if ( pLocation != NULL ) {
		(void)snprintf(Data, sizeof(Data),
			"offset=%llu;line=%llu;column=%llu;record=%llu",
			(unsigned long long)pLocation->Offset,
			(unsigned long long)pLocation->Line,
			(unsigned long long)pLocation->Column,
			(unsigned long long)pLocation->RecordIndex);
		Desc.Data = Data;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}

/* 只识别本格式的位置，不在失败时触碰调用方输出。 */
bool __xrtTextLinesErrorLocation(
	const xerror* pError, const xtextlinesformat* pFormat, xtextlineslocation* pLocation
)
{
	unsigned long long Offset, Line, Column, Record;
	cstr sData;
	if ( (pError == NULL) || (xrtErrorDomain(pError) == NULL) ||
		 (strcmp(xrtErrorDomain(pError), pFormat->Domain) != 0) ) {
		return false;
	}
	sData = xrtErrorData(pError);
	if ( (sData == NULL) || (sscanf(sData,
		"offset=%llu;line=%llu;column=%llu;record=%llu",
		&Offset, &Line, &Column, &Record) != 4) ||
		(Offset > SIZE_MAX) || (Line > SIZE_MAX) ||
		(Column > SIZE_MAX) || (Record > SIZE_MAX) ) {
		return false;
	}
	pLocation->Offset = (size_t)Offset;
	pLocation->Line = (size_t)Line;
	pLocation->Column = (size_t)Column;
	pLocation->RecordIndex = (size_t)Record;
	return true;
}
#endif
