#include <xrt/imap_body.h>

#include "../internal/xrt_mail.h"



#if defined(XIMAP_FEATURE_IMAP_BODY)

/* 创建稳定的 BODYSTRUCTURE 协议错误。 */
static bool __xrtImapBodyError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 跳过 BODYSTRUCTURE 数据项之间的 SP。 */
static size_t __xrtImapBodySpace(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) && (Text.Data[iPosition] == ' ') ) {
		iPosition++;
	}
	return iPosition;
}

/* BODY fields require SP; only multipart's 1*body permits adjacent lists.
 * Let the data cursor validate its state before inspecting the old offset. */
static xmailnext __xrtImapBodyNext(
	ximapdatacursor* pCursor, ximapdataview* pValue, bool bAdjacentChildren)
{
	size_t iPosition = pCursor->Position;
	xmailnext Next = xrtImapDataNext(pCursor, pValue);
	if ( (Next == XMAIL_NEXT_ITEM) && (iPosition != 0) &&
		(pValue->Source.Data == pCursor->Text.Data + iPosition) &&
		!(bAdjacentChildren && pValue->Kind == XIMAP_DATA_LIST) ) {
		pCursor->Position = iPosition;
		__xrtImapBodyError("missing space between IMAP body fields");
		return XMAIL_NEXT_ERROR;
	}
	return Next;
}



/* 判断数据值是否是可在线路内完整表示的 IMAP string。 */
static bool __xrtImapBodyString(const ximapdataview* pValue)
{
	/* Complete lines cannot carry literal contents. A bare atom belongs to
	 * the separate astring grammar, not to IMAP string or nstring. */
	return pValue->Kind == XIMAP_DATA_QUOTED;
}



/* 判断数据值是否是可选 IMAP string。 */
static bool __xrtImapBodyNString(const ximapdataview* pValue)
{
	return (pValue->Kind == XIMAP_DATA_NIL) || __xrtImapBodyString(pValue);
}



/* 从游标读取一个必须存在的数据值。 */
static bool __xrtImapBodyRequired(
	ximapdatacursor* pCursor,
	ximapdataview* pValue
)
{
	xmailnext Next = __xrtImapBodyNext(pCursor, pValue, false);

	if ( Next == XMAIL_NEXT_ITEM ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_END ) {
		return __xrtImapBodyError("incomplete IMAP BODYSTRUCTURE");
	}
	return false;
}

/* A child parser owns its diagnostic. Validate a successful value separately
 * so a failed diagnostic allocation cannot be replaced by a parent error. */
static bool __xrtImapBodyRequiredString(
	ximapdatacursor* pCursor, ximapdataview* pValue, bool bOptional)
{
	if ( !__xrtImapBodyRequired(pCursor, pValue) ) return false;
	return (bOptional ? __xrtImapBodyNString(pValue) : __xrtImapBodyString(pValue)) ||
		__xrtImapBodyError("invalid IMAP body string field");
}

static bool __xrtImapBodyRequiredKind(
	ximapdatacursor* pCursor, ximapdataview* pValue, ximapdatakind Kind)
{
	if ( !__xrtImapBodyRequired(pCursor, pValue) ) return false;
	return pValue->Kind == Kind || __xrtImapBodyError("invalid IMAP body field type");
}

/* Known body sizes and extension numbers use the 63-bit IMAP domain. */
static bool __xrtImapBodyNumber64(const ximapdataview* pValue)
{
	if ( pValue->Number <= (uint64)INT64_MAX ) return true;
	__xrtMailError(XERR_RANGE, XMAIL_ERROR_LIMIT, "IMAP body number exceeds 63 bits");
	return false;
}

static bool __xrtImapBodyEnd(ximapdatacursor* pCursor)
{
	ximapdataview Extra;
	xmailnext Next = __xrtImapBodyNext(pCursor, &Extra, false);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	return Next == XMAIL_NEXT_END || __xrtImapBodyError("unexpected IMAP envelope data");
}

/* Four nstrings; NIL group markers and empty strings remain valid. */
static bool __xrtImapBodyAddress(const ximapdataview* pAddress)
{
	ximapdatacursor Cursor;
	ximapdataview Value;
	if ( pAddress->Kind != XIMAP_DATA_LIST ) return __xrtImapBodyError("invalid IMAP envelope address");
	if ( !xrtImapDataCursorInit(&Cursor, pAddress->Value) ) return false;
	for ( size_t i = 0; i < 4u; i++ )
		if ( !__xrtImapBodyRequiredString(&Cursor, &Value, true) ) return false;
	return __xrtImapBodyEnd(&Cursor);
}

static bool __xrtImapBodyAddresses(const ximapdataview* pAddresses)
{
	ximapdatacursor Cursor;
	ximapdataview Address;
	bool Found = false;
	if ( pAddresses->Kind == XIMAP_DATA_NIL ) return true;
	if ( pAddresses->Kind != XIMAP_DATA_LIST ) return __xrtImapBodyError("invalid IMAP envelope address list");
	if ( !xrtImapDataCursorInit(&Cursor, pAddresses->Value) ) return false;
	for ( ;; ) {
		xmailnext Next = __xrtImapBodyNext(&Cursor, &Address, true);
		if ( Next == XMAIL_NEXT_ERROR ) return false;
		if ( Next == XMAIL_NEXT_END ) return Found || __xrtImapBodyError("empty IMAP envelope address list");
		if ( !__xrtImapBodyAddress(&Address) ) return false;
		Found = true;
	}
}

static bool __xrtImapBodyEnvelope(const ximapdataview* pEnvelope)
{
	ximapdatacursor Cursor;
	ximapdataview Value;
	if ( !xrtImapDataCursorInit(&Cursor, pEnvelope->Value) ) return false;
	for ( size_t i = 0; i < 10u; i++ ) {
		if ( !__xrtImapBodyRequired(&Cursor, &Value) ) return false;
		if ( i >= 2u && i <= 7u ) {
			if ( !__xrtImapBodyAddresses(&Value) ) return false;
		} else if ( !__xrtImapBodyNString(&Value) ) return __xrtImapBodyError("invalid IMAP envelope nstring");
	}
	return __xrtImapBodyEnd(&Cursor);
}



/* 校验 body-fld-param 的 NIL 或成对字符串列表。 */
static bool __xrtImapBodyParameters(const ximapdataview* pParameters)
{
	ximapdatacursor Cursor;
	ximapdataview Name;
	ximapdataview Value;
	xmailnext Next;
	size_t iCount = 0;

	if ( pParameters->Kind == XIMAP_DATA_NIL ) {
		return true;
	}
	if ( pParameters->Kind != XIMAP_DATA_LIST )
		return __xrtImapBodyError("invalid IMAP body parameters");
	if ( !xrtImapDataCursorInit(&Cursor, pParameters->Value) ) return false;
	for ( ;; ) {
		Next = __xrtImapBodyNext(&Cursor, &Name, false);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0 || __xrtImapBodyError("empty IMAP body parameters");
		}
		if ( Next == XMAIL_NEXT_ERROR ) return false;
		if ( !__xrtImapBodyString(&Name) )
			return __xrtImapBodyError("invalid IMAP body parameter name");
		if ( !__xrtImapBodyRequiredString(&Cursor, &Value, false) ) return false;
		iCount++;
	}
}



/* 校验 body-fld-dsp 的 NIL 或 `(type parameters)` 结构。 */
static bool __xrtImapBodyDisposition(const ximapdataview* pDisposition)
{
	ximapdatacursor Cursor;
	ximapdataview Type;
	ximapdataview Parameters;
	ximapdataview Extra;

	if ( pDisposition->Kind == XIMAP_DATA_NIL ) {
		return true;
	}
	if ( pDisposition->Kind != XIMAP_DATA_LIST )
		return __xrtImapBodyError("invalid IMAP body disposition");
	if ( !xrtImapDataCursorInit(&Cursor, pDisposition->Value) ||
		!__xrtImapBodyRequiredString(&Cursor, &Type, false) ||
		!__xrtImapBodyRequired(&Cursor, &Parameters) ||
		!__xrtImapBodyParameters(&Parameters) ) return false;
	xmailnext Next = __xrtImapBodyNext(&Cursor, &Extra, false);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	return Next == XMAIL_NEXT_END || __xrtImapBodyError("unexpected IMAP body disposition data");
}



/* 校验 body-fld-lang 的 NIL、单字符串或非空字符串列表。 */
static bool __xrtImapBodyLanguage(const ximapdataview* pLanguage)
{
	ximapdatacursor Cursor;
	ximapdataview Value;
	xmailnext Next;
	size_t iCount = 0;

	if ( (pLanguage->Kind == XIMAP_DATA_NIL) ||
		__xrtImapBodyString(pLanguage) ) {
		return true;
	}
	if ( pLanguage->Kind != XIMAP_DATA_LIST )
		return __xrtImapBodyError("invalid IMAP body language");
	if ( !xrtImapDataCursorInit(&Cursor, pLanguage->Value) ) return false;
	for ( ;; ) {
		Next = __xrtImapBodyNext(&Cursor, &Value, false);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0 || __xrtImapBodyError("empty IMAP body language list");
		}
		if ( Next == XMAIL_NEXT_ERROR ) return false;
		if ( !__xrtImapBodyString(&Value) )
			return __xrtImapBodyError("invalid IMAP body language string");
		iCount++;
	}
}



/* 递归校验服务器定义的 body-extension 值。 */
static bool __xrtImapBodyExtension(
	const ximapdataview* pValue,
	size_t iDepth
)
{
	ximapdatacursor Cursor;
	ximapdataview Child;
	xmailnext Next;
	size_t iCount = 0;

	if ( iDepth > XIMAP_BODY_DEPTH_MAX ) {
		return __xrtImapBodyError("IMAP body extension nesting is too deep");
	}
	if ( pValue->Kind == XIMAP_DATA_NUMBER ) return __xrtImapBodyNumber64(pValue);
	if ( __xrtImapBodyNString(pValue) ) return true;
	if ( pValue->Kind != XIMAP_DATA_LIST )
		return __xrtImapBodyError("invalid IMAP body extension");
	if ( !xrtImapDataCursorInit(&Cursor, pValue->Value) ) return false;
	for ( ;; ) {
		Next = __xrtImapBodyNext(&Cursor, &Child, false);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0 || __xrtImapBodyError("empty IMAP body extension list");
		}
		if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyExtension(&Child, iDepth + 1u) ) return false;
		iCount++;
	}
}



/* 前置声明递归 BODYSTRUCTURE 解析器。 */
static bool __xrtImapBodyParseList(
	const ximapdataview* pList,
	size_t iDepth,
	ximapbodyview* pBody
);



/* 校验未知扩展尾并保留其原始数据区。 */
static bool __xrtImapBodyExtensionTail(
	ximapdatacursor* pCursor,
	xstrview Text,
	xstrview* pExtensions
)
{
	ximapdataview Value;
	xmailnext Next;
	size_t iStart = __xrtImapBodySpace(Text, pCursor->Position);
	size_t iEnd = iStart;

	for ( ;; ) {
		Next = __xrtImapBodyNext(pCursor, &Value, false);
		if ( Next == XMAIL_NEXT_END ) {
			pExtensions->Data = Text.Data != NULL ? Text.Data + iStart : NULL;
			pExtensions->Size = iEnd - iStart;
			return true;
		}
		if ( (Next != XMAIL_NEXT_ITEM) ||
			!__xrtImapBodyExtension(&Value, 1u) ) {
			return false;
		}
		iEnd = (size_t)((Value.Source.Data + Value.Source.Size) - Text.Data);
	}
}



/* 按 RFC 顺序读取一个单部分的可选扩展字段。 */
static bool __xrtImapBodyOneExtensions(
	ximapdatacursor* pCursor,
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	xmailnext Next;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP body MD5 field");
	}
	pBody->Md5 = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyDisposition(&Value) ) return false;
	pBody->Disposition = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyLanguage(&Value) ) return false;
	pBody->Language = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP body location field");
	}
	pBody->Location = Value;
	return __xrtImapBodyExtensionTail(pCursor, Text, &pBody->Extensions);
}



/* 按 RFC 顺序读取 multipart 的可选扩展字段。 */
static bool __xrtImapBodyMultiExtensions(
	ximapdatacursor* pCursor,
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	xmailnext Next;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyParameters(&Value) ) return false;
	pBody->Parameters = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyDisposition(&Value) ) return false;
	pBody->Disposition = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyLanguage(&Value) ) return false;
	pBody->Language = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP multipart location");
	}
	pBody->Location = Value;
	return __xrtImapBodyExtensionTail(pCursor, Text, &pBody->Extensions);
}



/* 解析普通、TEXT 或 MESSAGE/RFC822、MESSAGE/GLOBAL 单部分。 */
static bool __xrtImapBodyOnePart(
	ximapdatacursor* pCursor,
	const ximapdataview* pType,
	xstrview Text,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	ximapbodyview Nested;
	bool bText;
	bool bMessage;

	pBody->Type = *pType;
	if ( !__xrtImapBodyString(&pBody->Type) )
		return __xrtImapBodyError("invalid IMAP body media type");
	if ( !__xrtImapBodyRequiredString(pCursor, &pBody->Subtype, false) ||
		!__xrtImapBodyRequired(pCursor, &pBody->Parameters) ||
		!__xrtImapBodyParameters(&pBody->Parameters) ||
		!__xrtImapBodyRequiredString(pCursor, &pBody->Id, true) ||
		!__xrtImapBodyRequiredString(pCursor, &pBody->Description, true) ||
		!__xrtImapBodyRequiredString(pCursor, &pBody->Encoding, false) ||
		!__xrtImapBodyRequiredKind(pCursor, &Value, XIMAP_DATA_NUMBER) ||
		!__xrtImapBodyNumber64(&Value) ) return false;
	pBody->Octets = Value.Number;
	bText = __xrtMailAsciiEqualI(
		pBody->Type.Value,
		XRT_STR_LITERAL("TEXT")
	);
	bMessage = __xrtMailAsciiEqualI(
		pBody->Type.Value,
		XRT_STR_LITERAL("MESSAGE")
	) && (__xrtMailAsciiEqualI(
		pBody->Subtype.Value,
		XRT_STR_LITERAL("RFC822")
	) || __xrtMailAsciiEqualI(
		pBody->Subtype.Value,
		XRT_STR_LITERAL("GLOBAL")
	));
	if ( bText ) {
		pBody->Kind = XIMAP_BODY_TEXT;
		if ( !__xrtImapBodyRequiredKind(pCursor, &Value, XIMAP_DATA_NUMBER) ||
			!__xrtImapBodyNumber64(&Value) ) return false;
		pBody->Lines = Value.Number;
	} else if ( bMessage ) {
		pBody->Kind = XIMAP_BODY_MESSAGE;
		if ( !__xrtImapBodyRequiredKind(pCursor, &pBody->Envelope, XIMAP_DATA_LIST) ||
			!__xrtImapBodyEnvelope(&pBody->Envelope) ||
			!__xrtImapBodyRequiredKind(pCursor, &pBody->Body, XIMAP_DATA_LIST) ||
			!__xrtImapBodyParseList(
				&pBody->Body,
				iDepth + 1u,
				&Nested
			) || !__xrtImapBodyRequiredKind(pCursor, &Value, XIMAP_DATA_NUMBER) ||
			!__xrtImapBodyNumber64(&Value) ) return false;
		pBody->Lines = Value.Number;
	} else {
		pBody->Kind = XIMAP_BODY_BASIC;
	}
	return __xrtImapBodyOneExtensions(pCursor, Text, pBody);
}



/* 解析一个或多个子部分、subtype 与 multipart 扩展字段。 */
static bool __xrtImapBodyMultipart(
	ximapdatacursor* pCursor,
	const ximapdataview* pFirst,
	xstrview Text,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapdataview Value = *pFirst;
	ximapbodyview Child;
	const char* sChildren = pFirst->Source.Data;
	const char* sChildrenEnd = sChildren;

	pBody->Kind = XIMAP_BODY_MULTIPART;
	for ( ;; ) {
		if ( !__xrtImapBodyParseList(&Value, iDepth + 1u, &Child) ) return false;
		if ( pBody->ChildCount == SIZE_MAX ) {
			return __xrtImapBodyError("too many IMAP multipart children");
		}
		pBody->ChildCount++;
		sChildrenEnd = Value.Source.Data + Value.Source.Size;
		xmailnext Next = __xrtImapBodyNext(pCursor, &Value, true);
		if ( Next == XMAIL_NEXT_ERROR ) return false;
		if ( Next == XMAIL_NEXT_END )
			return __xrtImapBodyError("missing IMAP multipart subtype");
		if ( Value.Kind != XIMAP_DATA_LIST ) {
			break;
		}
	}
	pBody->Children.Data = sChildren;
	pBody->Children.Size = (size_t)(sChildrenEnd - sChildren);
	pBody->Subtype = Value;
	if ( !__xrtImapBodyString(&pBody->Subtype) ) {
		return __xrtImapBodyError("invalid IMAP multipart subtype");
	}
	return __xrtImapBodyMultiExtensions(pCursor, Text, pBody);
}



/* 解析一个已由通用数据层定界的括号 BODYSTRUCTURE 值。 */
static bool __xrtImapBodyParseList(
	const ximapdataview* pList,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapbodyview Body;
	ximapdatacursor Cursor;
	ximapdataview First;

	if ( iDepth > XIMAP_BODY_DEPTH_MAX ) {
		return __xrtImapBodyError("IMAP BODYSTRUCTURE nesting is too deep");
	}
	if ( pList->Kind != XIMAP_DATA_LIST ) {
		return __xrtImapBodyError("invalid IMAP BODYSTRUCTURE list");
	}
	if ( !xrtImapDataCursorInit(&Cursor, pList->Value) ||
		!__xrtImapBodyRequired(&Cursor, &First) ) return false;
	memset(&Body, 0, sizeof(Body));
	Body.Source = pList->Source;
	if ( First.Kind == XIMAP_DATA_LIST ) {
		if ( !__xrtImapBodyMultipart(
			&Cursor,
			&First,
			pList->Value,
			iDepth,
			&Body
		) ) {
			return false;
		}
	} else if ( !__xrtImapBodyOnePart(
		&Cursor,
		&First,
		pList->Value,
		iDepth,
		&Body
	) ) {
		return false;
	}
	*pBody = Body;
	return true;
}



/* 解析并递归校验完整 BODYSTRUCTURE。 */
XRT_API bool xrtImapBodyParse(
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdatacursor Cursor;
	ximapdataview List;
	ximapdataview Extra;
	ximapbodyview Body;
	xmailnext Next;

	if ( !xrtMemRangeValid(pBody, sizeof(*pBody)) ||
		!xrtMemRangeValid(Text.Data, Text.Size) ||
		xrtMemRangesOverlap(pBody, sizeof(*pBody), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapDataCursorInit(&Cursor, Text) ) return false;
	Next = xrtImapDataNext(&Cursor, &List);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( (Next != XMAIL_NEXT_ITEM) || (List.Kind != XIMAP_DATA_LIST) ) {
		return __xrtImapBodyError("invalid IMAP BODYSTRUCTURE");
	}
	Next = xrtImapDataNext(&Cursor, &Extra);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( Next != XMAIL_NEXT_END )
		return __xrtImapBodyError("unexpected IMAP BODYSTRUCTURE trailing data");
	if ( !__xrtImapBodyParseList(&List, 0, &Body) ) return false;
	*pBody = Body;
	return true;
}



/* 初始化 multipart 直接子部分游标。 */
XRT_API bool xrtImapBodyChildCursorInit(
	ximapbodycursor* pCursor,
	const ximapbodyview* pBody
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pBody, sizeof(*pBody)) || (pBody == NULL) ||
		(pBody->Kind != XIMAP_BODY_MULTIPART) ||
		(pBody->ChildCount == 0) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pBody,
			sizeof(*pBody)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pBody->Children.Data,
			pBody->Children.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapDataCursorInit(&pCursor->Data, pBody->Children) ) return false;
	pCursor->Remaining = pBody->ChildCount;
	return true;
}



/* 返回下一个 multipart 直接子部分。 */
XRT_API xmailnext xrtImapBodyChildNext(
	ximapbodycursor* pCursor,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	ximapbodyview Body;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pBody, sizeof(*pBody)) || (pCursor == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pBody,
			sizeof(*pBody)) ||
		xrtMemRangesOverlap(pBody, sizeof(*pBody), pCursor->Data.Text.Data,
			pCursor->Data.Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Remaining == 0 ) {
		Next = __xrtImapBodyNext(&pCursor->Data, &Value, true);
		if ( Next == XMAIL_NEXT_END ) {
			return XMAIL_NEXT_END;
		}
		if ( Next == XMAIL_NEXT_ERROR ) return XMAIL_NEXT_ERROR;
		__xrtImapBodyError("unexpected IMAP multipart child data");
		return XMAIL_NEXT_ERROR;
	}
	Next = __xrtImapBodyNext(&pCursor->Data, &Value, true);
	if ( Next == XMAIL_NEXT_ERROR ) return XMAIL_NEXT_ERROR;
	if ( (Next != XMAIL_NEXT_ITEM) || (Value.Kind != XIMAP_DATA_LIST) ) {
		__xrtImapBodyError("invalid IMAP multipart child cursor");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtImapBodyParseList(&Value, 0, &Body) ) return XMAIL_NEXT_ERROR;
	pCursor->Remaining--;
	*pBody = Body;
	return XMAIL_NEXT_ITEM;
}



/* 初始化成对参数游标。 */
XRT_API bool xrtImapBodyParamCursorInit(
	ximapbodyparamcursor* pCursor,
	const ximapdataview* pParameters
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pParameters, sizeof(*pParameters)) ||
		(pParameters == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pParameters,
			sizeof(*pParameters)) ||
		((pParameters->Kind != XIMAP_DATA_NIL) &&
		 (pParameters->Kind != XIMAP_DATA_LIST)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtImapBodyParameters(pParameters) &&
		xrtImapDataCursorInit(&pCursor->Data, pParameters->Value);
}



/* 返回下一参数名和值。 */
XRT_API xmailnext xrtImapBodyParamNext(
	ximapbodyparamcursor* pCursor,
	ximapbodyparam* pParameter
)
{
	ximapbodyparam Parameter;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pParameter, sizeof(*pParameter)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pParameter,
			sizeof(*pParameter)) ||
		xrtMemRangesOverlap(pParameter, sizeof(*pParameter), pCursor->Data.Text.Data,
			pCursor->Data.Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Next = __xrtImapBodyNext(&pCursor->Data, &Parameter.Name, false);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( !__xrtImapBodyString(&Parameter.Name) ) {
		__xrtImapBodyError("invalid IMAP body parameter pair");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtImapBodyRequiredString(&pCursor->Data, &Parameter.Value, false) )
		return XMAIL_NEXT_ERROR;
	*pParameter = Parameter;
	return XMAIL_NEXT_ITEM;
}

#endif
