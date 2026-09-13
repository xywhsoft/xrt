#include "../internal/xrt_text_lines.h"

#if defined(XRT_FEATURE_JSONL_READ) || defined(XRT_FEATURE_XSONL_READ)

/* 分隔符由扫描器处理；空白只接受 JSON 的 ASCII 空格、Tab、CR。 */
static bool __xrtTextLinesBlank(xstrview Text)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( (Text.Data[i] != ' ') && (Text.Data[i] != '\t') && (Text.Data[i] != '\r') ) {
			return false;
		}
	}
	return true;
}

/* 借用每个物理行，并在成功前保持汇总 Array 私有。 */
xvalue* __xrtTextLinesRead(
	xstrview Text, const xtextlinesreadconfig* pConfig,
	const xtextlinesformat* pFormat, bool bValidate
)
{
	xvalue* pArray;
	xvalue* pItem = NULL;
	size_t iStart = 0;
	xtextlineslocation Location = { 0, 1, 1, 0 };
	xtextvaluebudget Budget = { pConfig->MaxTotalValues, pConfig->MaxTotalDecodedBytes };

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( Text.Size > pConfig->MaxInputBytes ) {
		__xrtTextLinesError(pFormat, XTEXT_LINES_LIMIT, XERR_RANGE,
			"read", "input exceeds total byte limit", NULL, false);
		return NULL;
	}
	pArray = bValidate ? xrtValueNull() : xrtValueArray();
	if ( pArray == NULL ) {
		return NULL;
	}
	while ( iStart < Text.Size ) {
		const char* pLf = (const char*)memchr(Text.Data + iStart, '\n', Text.Size - iStart);
		size_t iEnd = pLf != NULL ? (size_t)(pLf - Text.Data) : Text.Size;
		size_t iSize = iEnd - iStart;
		xstrview Record;
		Location.Offset = iStart;
		Location.Column = 1;
		if ( (pLf != NULL) && (iSize != 0) && (Text.Data[iEnd - 1] == '\r') ) {
			iSize--;
		}
		Record = (xstrview){ Text.Data + iStart, iSize };
		if ( iSize > pConfig->MaxLineBytes ) {
			__xrtTextLinesError(pFormat, XTEXT_LINES_LIMIT, XERR_RANGE,
				"read", "line exceeds record byte limit", &Location, false);
			goto fail;
		}
		if ( __xrtTextLinesBlank(Record) ) {
			if ( pConfig->RejectEmpty ) {
				__xrtTextLinesError(pFormat, XTEXT_LINES_SYNTAX, XERR_PROTOCOL,
					"read", "empty lines are rejected by configuration", &Location, false);
				goto fail;
			}
		} else {
			if ( Location.RecordIndex >= pConfig->MaxRecords ) {
				__xrtTextLinesError(pFormat, XTEXT_LINES_LIMIT, XERR_RANGE,
					"read", "record count exceeds configured limit", &Location, false);
				goto fail;
			}
			pItem = pConfig->Read(Record, pConfig->Record, &Budget, bValidate);
			if ( pItem == NULL ) {
				size_t iOffset, iLine, iColumn;
				const xerror* pError = xrtGetError();
				if ( (pError != NULL) && __xrtTextValueErrorLocation(pError, pFormat->RecordDomain,
					&iOffset, &iLine, &iColumn) && (iOffset <= iSize) ) {
					Location.Offset += iOffset;
					Location.Column += iOffset;
				}
				__xrtTextLinesError(pFormat,
					(pError != NULL && xrtErrorKind(pError) == XERR_RANGE)
						? XTEXT_LINES_LIMIT : XTEXT_LINES_RECORD,
					XERR_PROTOCOL, "read", "record could not be decoded", &Location, true);
				goto fail;
			}
			if ( !bValidate && !xrtValueArrayAppendTake(pArray, &pItem) ) {
				__xrtTextLinesError(pFormat, XTEXT_LINES_RECORD, XERR_STATE,
					"read", "record could not be added to array", &Location, true);
				goto fail;
			}
			xrtValueRelease(pItem);
			pItem = NULL;
			Location.RecordIndex++;
		}
		if ( pLf == NULL ) {
			break;
		}
		iStart = iEnd + 1;
		Location.Line++;
	}
	return pArray;

fail:
	{
		xerror* pError = xrtTakeError();
		xrtValueRelease(pItem);
		xrtValueRelease(pArray);
		xrtSetErrorTake(pError);
	}
	return NULL;
}
#endif
