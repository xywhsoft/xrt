#include "../internal/xrt_json.h"
#include "../internal/xrt_json_escape.h"
#include <xrt/logger.h>

#include <math.h>



#if defined(XRT_FEATURE_LOGGER_FORMAT_JSON)

/* 流式 JSON 格式器保存配置、Writer、精确长度和失败状态。 */
typedef struct xlogjsonwriter {
	xlogwriteproc Write;
	ptr UserData;
	size_t Written;
	uint32 EscapeFlags;
	xlogjsonnonfinite NonFinite;
	size_t MaxErrorDepth;
	bool Failed;
} xlogjsonwriter;



/* 验证 JSONL 配置没有未知标志或无界错误链。 */
static bool __xrtLogJsonConfigValid(const xlogjsonconfig* pConfig)
{
	const uint32 iKnownFlags =
		XLOG_JSON_TIME |
		XLOG_JSON_LEVEL |
		XLOG_JSON_LOGGER |
		XLOG_JSON_MESSAGE |
		XLOG_JSON_SOURCE |
		XLOG_JSON_THREAD |
		XLOG_JSON_FIELDS |
		XLOG_JSON_NEWLINE;
	const uint32 iKnownEscape =
		XJSON_WRITE_ESCAPE_SLASH |
		XJSON_WRITE_ESCAPE_HTML |
		XJSON_WRITE_ESCAPE_NON_ASCII;

	return
		(pConfig != NULL) &&
		((pConfig->Flags & ~iKnownFlags) == 0) &&
		((pConfig->EscapeFlags & ~iKnownEscape) == 0) &&
		(pConfig->FieldStyle >= XLOG_JSON_FIELDS_OBJECT) &&
		(pConfig->FieldStyle <= XLOG_JSON_FIELDS_ARRAY) &&
		(pConfig->NonFinite >= XLOG_JSON_NONFINITE_REJECT) &&
		(pConfig->NonFinite <= XLOG_JSON_NONFINITE_STRING) &&
		(pConfig->MaxErrorDepth > 0) &&
		(pConfig->MaxErrorDepth <= 256u);
}



/* 建立 JSON 格式器自身的稳定错误。 */
static void __xrtLogJsonError(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.log";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 校验 JSON 配置并建立稳定配置错误。 */
XRT_API bool xrtLogJsonConfigValidate(const xlogjsonconfig* pConfig)
{
	if ( !__xrtLogJsonConfigValid(pConfig) ) {
		__xrtLogJsonError(
			XERR_ARGUMENT,
			XLOG_ERROR_JSON_CONFIG,
			"format-json",
			"invalid log JSON configuration"
		);
		return false;
	}
	return true;
}



/* 提交借用字节，并在回调没有给出原因时建立日志输出错误。 */
static bool __xrtLogJsonEmit(
	xlogjsonwriter* pWriter,
	const void* pData,
	size_t iSize
)
{
	if ( pWriter->Failed ) {
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( pWriter->Written > (SIZE_MAX - iSize) ) {
		__xrtErrorSetSizeOverflow();
		pWriter->Failed = true;
		return false;
	}
	if (
		!pWriter->Write(
			(xbytesview){ (cbytes)pData, iSize },
			pWriter->UserData
		)
	) {
		if ( xrtGetError() == NULL ) {
			__xrtLogJsonError(
				XERR_IO,
				XLOG_ERROR_JSON_OUTPUT,
				"format-json",
				"log JSON writer failed"
			);
		}
		pWriter->Failed = true;
		return false;
	}
	pWriter->Written += iSize;
	return true;
}



/* 提交固定 ASCII 文本。 */
static bool __xrtLogJsonAscii(xlogjsonwriter* pWriter, cstr sText)
{
	return __xrtLogJsonEmit(pWriter, sText, strlen(sText));
}



/* 把共享 JSON 转义核心桥接到日志 Writer。 */
static bool __xrtLogJsonEscapeEmit(
	const void* pData,
	size_t iSize,
	ptr pUserData
)
{
	return __xrtLogJsonEmit(
		(xlogjsonwriter*)pUserData,
		pData,
		iSize
	);
}



/* 写出一个严格 UTF-8 JSON 字符串 token。 */
static bool __xrtLogJsonQuote(xlogjsonwriter* pWriter, xstrview Text)
{
	xjsonescaperesult Result;
	size_t iErrorOffset = 0;

	Result = __xrtJsonEscapeWrite(
		Text,
		pWriter->EscapeFlags,
		__xrtLogJsonEscapeEmit,
		pWriter,
		NULL,
		&iErrorOffset
	);
	if ( Result == XJSON_ESCAPE_OK ) {
		return true;
	}
	if ( Result == XJSON_ESCAPE_INVALID ) {
		xjsonlocation Location = { iErrorOffset, 1u, iErrorOffset + 1u };

		__xrtJsonError(
			XERR_VALUE,
			XJSON_ERROR_UNSUPPORTED,
			"format-log",
			"log string is not valid UTF-8",
			&Location
		);
	} else if ( Result == XJSON_ESCAPE_OVERFLOW ) {
		__xrtErrorSetSizeOverflow();
	} else if ( xrtGetError() == NULL ) {
		__xrtLogJsonError(
			XERR_IO,
			XLOG_ERROR_JSON_OUTPUT,
			"format-json",
			"log JSON writer failed"
		);
	}
	pWriter->Failed = true;
	return false;
}



/* 把零结尾字符串转换为借用视图。 */
static xstrview __xrtLogJsonCString(cstr sText)
{
	return (xstrview){ sText, sText != NULL ? strlen(sText) : 0 };
}



/* 写出有符号十进制整数。 */
static bool __xrtLogJsonInt(xlogjsonwriter* pWriter, int64 iValue)
{
	char arrText[32];
	size_t iSize;

	return
		xrtIntWrite(iValue, 10u, arrText, sizeof(arrText), &iSize, 0) &&
		__xrtLogJsonEmit(pWriter, arrText, iSize);
}



/* 写出无符号十进制整数。 */
static bool __xrtLogJsonUInt(xlogjsonwriter* pWriter, uint64 iValue)
{
	char arrText[32];
	size_t iSize;

	return
		xrtUIntWrite(iValue, 10u, arrText, sizeof(arrText), &iSize, 0) &&
		__xrtLogJsonEmit(pWriter, arrText, iSize);
}



/* 按配置写出有限或特殊浮点值。 */
static bool __xrtLogJsonFloat(xlogjsonwriter* pWriter, double fValue)
{
	char arrText[64];
	size_t iSize;

	if ( isfinite(fValue) ) {
		return
			xrtNumWrite(fValue, arrText, sizeof(arrText), &iSize, 0) &&
			__xrtLogJsonEmit(pWriter, arrText, iSize);
	}
	if ( pWriter->NonFinite == XLOG_JSON_NONFINITE_NULL ) {
		return __xrtLogJsonAscii(pWriter, "null");
	}
	if ( isnan(fValue) ) {
		return __xrtLogJsonAscii(pWriter, "\"NaN\"");
	}
	return __xrtLogJsonAscii(
		pWriter,
		signbit(fValue) ? "\"-Infinity\"" : "\"Infinity\""
	);
}



/* 在对象中开始一个固定名称成员。 */
static bool __xrtLogJsonMember(
	xlogjsonwriter* pWriter,
	bool* pAny,
	cstr sName
)
{
	if ( *pAny && !__xrtLogJsonAscii(pWriter, ",") ) {
		return false;
	}
	*pAny = true;
	return
		__xrtLogJsonQuote(pWriter, __xrtLogJsonCString(sName)) &&
		__xrtLogJsonAscii(pWriter, ":");
}



/* 在对象中开始一个动态名称成员。 */
static bool __xrtLogJsonMemberView(
	xlogjsonwriter* pWriter,
	bool* pAny,
	xstrview Name
)
{
	if ( *pAny && !__xrtLogJsonAscii(pWriter, ",") ) {
		return false;
	}
	*pAny = true;
	return
		__xrtLogJsonQuote(pWriter, Name) &&
		__xrtLogJsonAscii(pWriter, ":");
}



/* 返回字段类型的稳定英文名称。 */
static cstr __xrtLogJsonFieldTypeName(xlogfieldtype Type)
{
	static const cstr arrNames[] = {
		"null", "bool", "int", "uint", "float", "string", "time", "error"
	};

	return arrNames[(size_t)Type];
}



/* 写出完整结构化错误及其原因链。 */
static bool __xrtLogJsonErrorValue(
	xlogjsonwriter* pWriter,
	const xerror* pError,
	size_t iDepth
)
{
	bool bAny = false;
	cstr sOperation;
	cstr sData;
	const xerror* pCause;

	if ( pError == NULL ) {
		return __xrtLogJsonAscii(pWriter, "null");
	}
	if ( iDepth >= pWriter->MaxErrorDepth ) {
		__xrtLogJsonError(
			XERR_RANGE,
			XLOG_ERROR_JSON_DEPTH,
			"format-json",
			"log error cause exceeds configured depth"
		);
		pWriter->Failed = true;
		return false;
	}
	sOperation = xrtErrorOperation(pError);
	sData = xrtErrorData(pError);
	pCause = xrtErrorCause(pError);
	return
		__xrtLogJsonAscii(pWriter, "{") &&
		__xrtLogJsonMember(pWriter, &bAny, "kind") &&
		__xrtLogJsonInt(pWriter, (int64)xrtErrorKind(pError)) &&
		__xrtLogJsonMember(pWriter, &bAny, "domain") &&
		__xrtLogJsonQuote(
			pWriter,
			__xrtLogJsonCString(xrtErrorDomain(pError))
		) &&
		__xrtLogJsonMember(pWriter, &bAny, "code") &&
		__xrtLogJsonInt(pWriter, (int64)xrtErrorCode(pError)) &&
		(
			(xrtErrorSystemCode(pError) == 0) ||
			(
				__xrtLogJsonMember(pWriter, &bAny, "system_code") &&
				__xrtLogJsonInt(
					pWriter,
					(int64)xrtErrorSystemCode(pError)
				)
			)
		) &&
		(
			(sOperation[0] == 0) ||
			(
				__xrtLogJsonMember(pWriter, &bAny, "operation") &&
				__xrtLogJsonQuote(
					pWriter,
					__xrtLogJsonCString(sOperation)
				)
			)
		) &&
		__xrtLogJsonMember(pWriter, &bAny, "message") &&
		__xrtLogJsonQuote(
			pWriter,
			__xrtLogJsonCString(xrtErrorMessage(pError))
		) &&
		(
			(sData[0] == 0) ||
			(
				__xrtLogJsonMember(pWriter, &bAny, "data") &&
				__xrtLogJsonQuote(pWriter, __xrtLogJsonCString(sData))
			)
		) &&
		(
			(pCause == NULL) ||
			(
				__xrtLogJsonMember(pWriter, &bAny, "cause") &&
				__xrtLogJsonErrorValue(pWriter, pCause, iDepth + 1u)
			)
		) &&
		__xrtLogJsonAscii(pWriter, "}");
}



/* 写出一个结构化字段值。 */
static bool __xrtLogJsonFieldValue(
	xlogjsonwriter* pWriter,
	const xlogfield* pField
)
{
	if ( pField->Type == XLOG_FIELD_NULL ) {
		return __xrtLogJsonAscii(pWriter, "null");
	}
	if ( pField->Type == XLOG_FIELD_BOOL ) {
		return __xrtLogJsonAscii(
			pWriter,
			pField->Value.Boolean ? "true" : "false"
		);
	}
	if ( pField->Type == XLOG_FIELD_INT ) {
		return __xrtLogJsonInt(pWriter, pField->Value.Integer);
	}
	if ( pField->Type == XLOG_FIELD_UINT ) {
		return __xrtLogJsonUInt(pWriter, pField->Value.Unsigned);
	}
	if ( pField->Type == XLOG_FIELD_FLOAT ) {
		return __xrtLogJsonFloat(pWriter, pField->Value.Float);
	}
	if ( pField->Type == XLOG_FIELD_STRING ) {
		return __xrtLogJsonQuote(pWriter, pField->Value.String);
	}
	if ( pField->Type == XLOG_FIELD_TIME ) {
		return __xrtLogJsonInt(pWriter, (int64)pField->Value.Time);
	}
	return __xrtLogJsonErrorValue(pWriter, pField->Value.Error, 0);
}



/* 写出便于常规查询的字段对象。 */
static bool __xrtLogJsonFieldsObject(
	xlogjsonwriter* pWriter,
	const xlogrecord* pRecord
)
{
	bool bAny = false;

	if ( !__xrtLogJsonAscii(pWriter, "{") ) {
		return false;
	}
	for ( size_t i = 0; i < pRecord->FieldCount; i++ ) {
		if (
			!__xrtLogJsonMemberView(
				pWriter,
				&bAny,
				pRecord->Fields[i].Name
			) ||
			!__xrtLogJsonFieldValue(pWriter, &pRecord->Fields[i])
		) {
			return false;
		}
	}
	return __xrtLogJsonAscii(pWriter, "}");
}



/* 写出可无损保留重名和类型的字段数组。 */
static bool __xrtLogJsonFieldsArray(
	xlogjsonwriter* pWriter,
	const xlogrecord* pRecord
)
{
	if ( !__xrtLogJsonAscii(pWriter, "[") ) {
		return false;
	}
	for ( size_t i = 0; i < pRecord->FieldCount; i++ ) {
		const xlogfield* pField = &pRecord->Fields[i];
		bool bAny = false;

		if (
			((i != 0) && !__xrtLogJsonAscii(pWriter, ",")) ||
			!__xrtLogJsonAscii(pWriter, "{") ||
			!__xrtLogJsonMember(pWriter, &bAny, "name") ||
			!__xrtLogJsonQuote(pWriter, pField->Name) ||
			!__xrtLogJsonMember(pWriter, &bAny, "type") ||
			!__xrtLogJsonQuote(
				pWriter,
				__xrtLogJsonCString(
					__xrtLogJsonFieldTypeName(pField->Type)
				)
			) ||
			!__xrtLogJsonMember(pWriter, &bAny, "value") ||
			!__xrtLogJsonFieldValue(pWriter, pField) ||
			!__xrtLogJsonAscii(pWriter, "}")
		) {
			return false;
		}
	}
	return __xrtLogJsonAscii(pWriter, "]");
}



/* 写出存在的源码元数据。 */
static bool __xrtLogJsonSource(
	xlogjsonwriter* pWriter,
	const xlogrecord* pRecord
)
{
	bool bAny = false;

	if ( !__xrtLogJsonAscii(pWriter, "{") ) {
		return false;
	}
	if (
		(pRecord->File.Size != 0) &&
		(
			!__xrtLogJsonMember(pWriter, &bAny, "file") ||
			!__xrtLogJsonQuote(pWriter, pRecord->File)
		)
	) {
		return false;
	}
	if (
		(pRecord->Function.Size != 0) &&
		(
			!__xrtLogJsonMember(pWriter, &bAny, "function") ||
			!__xrtLogJsonQuote(pWriter, pRecord->Function)
		)
	) {
		return false;
	}
	if (
		(pRecord->Line != 0) &&
		(
			!__xrtLogJsonMember(pWriter, &bAny, "line") ||
			!__xrtLogJsonUInt(pWriter, (uint64)pRecord->Line)
		)
	) {
		return false;
	}
	return __xrtLogJsonAscii(pWriter, "}");
}



/* 在输出前拒绝不允许的非有限值和过深或循环的错误链。 */
static bool __xrtLogJsonFieldsValid(
	const xlogrecord* pRecord,
	const xlogjsonconfig* pConfig
)
{
	if ( (pConfig->Flags & XLOG_JSON_FIELDS) == 0 ) {
		return true;
	}
	for ( size_t i = 0; i < pRecord->FieldCount; i++ ) {
		const xlogfield* pField = &pRecord->Fields[i];

		if (
			(pField->Type == XLOG_FIELD_FLOAT) &&
			!isfinite(pField->Value.Float) &&
			(pConfig->NonFinite == XLOG_JSON_NONFINITE_REJECT)
		) {
			__xrtLogJsonError(
				XERR_VALUE,
				XLOG_ERROR_JSON_VALUE,
				"format-json",
				"non-finite log field requires a JSON policy"
			);
			return false;
		}
		if (
			(pField->Type == XLOG_FIELD_ERROR) &&
			(pField->Value.Error != NULL)
		) {
			const xerror* pError = pField->Value.Error;
			size_t iDepth = 0;

			while ( pError != NULL ) {
				iDepth++;
				if ( iDepth > pConfig->MaxErrorDepth ) {
					__xrtLogJsonError(
						XERR_RANGE,
						XLOG_ERROR_JSON_DEPTH,
						"format-json",
						"log error cause exceeds configured depth"
					);
					return false;
				}
				pError = xrtErrorCause(pError);
			}
		}
	}
	return true;
}



/* 初始化完整 JSON Lines 配置。 */
XRT_API bool xrtLogJsonConfigInit(xlogjsonconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pConfig->Flags =
		XLOG_JSON_TIME |
		XLOG_JSON_LEVEL |
		XLOG_JSON_LOGGER |
		XLOG_JSON_MESSAGE |
		XLOG_JSON_SOURCE |
		XLOG_JSON_THREAD |
		XLOG_JSON_FIELDS |
		XLOG_JSON_NEWLINE;
	pConfig->EscapeFlags = 0;
	pConfig->FieldStyle = XLOG_JSON_FIELDS_OBJECT;
	pConfig->NonFinite = XLOG_JSON_NONFINITE_REJECT;
	pConfig->MaxErrorDepth = XLOG_JSON_ERROR_DEPTH_DEFAULT;
	return true;
}



/* 无中间对象和整行分配地格式化一条 JSON Lines 记录。 */
XRT_API bool xrtLogJsonWrite(
	const xlogrecord* pRecord,
	const xlogjsonconfig* pConfig,
	xlogwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
)
{
	xlogjsonwriter Writer;
	xerror* pPrevious;
	bool bAny = false;
	bool bHasSource;
	bool bResult;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( !xrtLogRecordValidate(pRecord) ) {
		return false;
	}
	if ( !xrtLogJsonConfigValidate(pConfig) || (pWrite == NULL) ) {
		if ( pWrite == NULL ) {
			__xrtLogJsonError(
				XERR_ARGUMENT,
				XLOG_ERROR_JSON_CONFIG,
				"format-json",
				"invalid log JSON configuration"
			);
		}
		return false;
	}
	if ( !__xrtLogJsonFieldsValid(pRecord, pConfig) ) {
		return false;
	}
	Writer.Write = pWrite;
	Writer.UserData = pUserData;
	Writer.Written = 0;
	Writer.EscapeFlags = pConfig->EscapeFlags;
	Writer.NonFinite = pConfig->NonFinite;
	Writer.MaxErrorDepth = pConfig->MaxErrorDepth;
	Writer.Failed = false;
	pPrevious = xrtErrorRef(xrtGetError());
	xrtClearError();
	bHasSource =
		(pRecord->File.Size != 0) ||
		(pRecord->Function.Size != 0) ||
		(pRecord->Line != 0);
	bResult = __xrtLogJsonAscii(&Writer, "{");
	if ( bResult && ((pConfig->Flags & XLOG_JSON_TIME) != 0) ) {
		bResult =
			__xrtLogJsonMember(&Writer, &bAny, "time") &&
			__xrtLogJsonInt(&Writer, (int64)pRecord->Time);
	}
	if ( bResult && ((pConfig->Flags & XLOG_JSON_LEVEL) != 0) ) {
		bResult =
			__xrtLogJsonMember(&Writer, &bAny, "level") &&
			__xrtLogJsonQuote(
				&Writer,
				__xrtLogJsonCString(xrtLogLevelName(pRecord->Level))
			);
	}
	if ( bResult && ((pConfig->Flags & XLOG_JSON_LOGGER) != 0) ) {
		bResult =
			__xrtLogJsonMember(&Writer, &bAny, "logger") &&
			__xrtLogJsonQuote(&Writer, pRecord->Logger);
	}
	if ( bResult && ((pConfig->Flags & XLOG_JSON_MESSAGE) != 0) ) {
		bResult =
			__xrtLogJsonMember(&Writer, &bAny, "message") &&
			__xrtLogJsonQuote(&Writer, pRecord->Message);
	}
	if (
		bResult && bHasSource &&
		((pConfig->Flags & XLOG_JSON_SOURCE) != 0)
	) {
		bResult =
			__xrtLogJsonMember(&Writer, &bAny, "source") &&
			__xrtLogJsonSource(&Writer, pRecord);
	}
	if (
		bResult && (pRecord->ThreadId != 0) &&
		((pConfig->Flags & XLOG_JSON_THREAD) != 0)
	) {
		bResult =
			__xrtLogJsonMember(&Writer, &bAny, "thread") &&
			__xrtLogJsonUInt(&Writer, pRecord->ThreadId);
	}
	if ( bResult && ((pConfig->Flags & XLOG_JSON_FIELDS) != 0) ) {
		bResult =
			__xrtLogJsonMember(&Writer, &bAny, "fields") &&
			(
				(pConfig->FieldStyle == XLOG_JSON_FIELDS_OBJECT)
				? __xrtLogJsonFieldsObject(&Writer, pRecord)
				: __xrtLogJsonFieldsArray(&Writer, pRecord)
			);
	}
	if ( bResult ) {
		bResult = __xrtLogJsonAscii(&Writer, "}");
	}
	if ( bResult && ((pConfig->Flags & XLOG_JSON_NEWLINE) != 0) ) {
		bResult = __xrtLogJsonAscii(&Writer, "\n");
	}
	if ( pWritten != NULL ) {
		*pWritten = Writer.Written;
	}
	if ( bResult ) {
		__xrtErrorSetOwned(pPrevious);
		return true;
	}
	xrtErrorFree(pPrevious);
	if ( xrtGetError() == NULL ) {
		__xrtLogJsonError(
			XERR_IO,
			XLOG_ERROR_JSON_OUTPUT,
			"format-json",
			"log JSON writer failed"
		);
	}
	return false;
}

#endif
