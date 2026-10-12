#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XHTTP_FEATURE_HTTP_AUTH_BASIC)

typedef struct xrt_http_basic_temporary {
	bytes Data;
	size_t Size;
	bool Heap;
	unsigned char Local[XRT_HTTP_AUTH_LOCAL_BYTES];
} xrt_http_basic_temporary;



/* 为敏感中间值选择固定栈缓冲或精确堆缓冲。 */
static bool __xrtHttpBasicTemporaryOpen(
	xrt_http_basic_temporary* pTemporary,
	size_t iSize
)
{
	pTemporary->Data = pTemporary->Local;
	pTemporary->Size = iSize;
	pTemporary->Heap = false;
	if ( iSize <= sizeof(pTemporary->Local) ) {
		return true;
	}
	pTemporary->Data = (bytes)xrtMalloc(iSize);
	if ( pTemporary->Data == NULL ) {
		return false;
	}
	pTemporary->Heap = true;
	return true;
}



/* 擦除敏感中间值，并按来源释放堆缓冲。 */
static void __xrtHttpBasicTemporaryClose(
	xrt_http_basic_temporary* pTemporary
)
{
	xrtSecureZero(pTemporary->Data, pTemporary->Size);
	if ( pTemporary->Heap ) {
		xrtFree(pTemporary->Data);
	}
	pTemporary->Data = NULL;
	pTemporary->Size = 0;
	pTemporary->Heap = false;
}



/* 验证 Basic 用户信息不含控制字符，用户名额外禁止冒号。 */
static bool __xrtHttpBasicTextValid(
	xstrview Text,
	bool bUser
)
{
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte <= UINT8_C(0x1F)) ||
			(iByte == UINT8_C(0x7F)) ||
			(bUser && (iByte == (unsigned char)':')) ) {
			return false;
		}
	}
	return true;
}



/* 计算 Basic 原文、Base64 和完整字段长度。 */
static bool __xrtHttpBasicMeasure(
	xstrview User,
	xstrview Password,
	size_t* pPlain,
	size_t* pEncoded,
	size_t* pRequired
)
{
	size_t iPlain;
	size_t iBlocks;
	size_t iEncoded;

	if ( !__xhttpViewValid(User) ||
		!__xhttpViewValid(Password) ) {
		return false;
	}
	if ( !__xrtHttpBasicTextValid(User, true) ||
		!__xrtHttpBasicTextValid(Password, false) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( User.Size > (SIZE_MAX - Password.Size - 1u) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iPlain = User.Size + Password.Size + 1u;
	if ( iPlain > (SIZE_MAX - 2u) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iBlocks = (iPlain + 2u) / 3u;
	if ( iBlocks > ((SIZE_MAX - 6u) / 4u) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iEncoded = iBlocks * 4u;
	*pPlain = iPlain;
	*pEncoded = iEncoded;
	*pRequired = iEncoded + 6u;
	return true;
}



/* 检查 Basic 写出参数和借用输入不重叠。 */
static bool __xrtHttpBasicWriteValid(
	xstrview User,
	xstrview Password,
	void* pOutput,
	size_t iCapacity,
	size_t iRequired,
	size_t* pSize
)
{
	if ( !xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize), User.Data, User.Size
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize), Password.Data, Password.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !xrtMemRangeValid(pOutput, iRequired) ||
		xrtMemRangesOverlap(
		pSize, sizeof(iRequired), pOutput, iRequired
	) || xrtMemRangesOverlap(
		pOutput, iRequired, User.Data, User.Size
	) || xrtMemRangesOverlap(
		pOutput, iRequired, Password.Data, Password.Size
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	return true;
}



/* 构建完整 Basic 字段值。 */
XHTTP_API bool xrtHttpBasicWrite(
	xstrview User,
	xstrview Password,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xrt_http_basic_temporary Temporary;
	size_t iPlain;
	size_t iEncoded;
	size_t iRequired;
	size_t iWritten;

	if ( !__xrtHttpBasicMeasure(
		User, Password, &iPlain, &iEncoded, &iRequired
	) ) {
		return false;
	}
	if ( !__xrtHttpBasicWriteValid(
		User, Password, pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	if ( !__xrtHttpBasicTemporaryOpen(
		&Temporary, iEncoded + 1u
	) ) {
		return false;
	}
	if ( User.Size != 0 ) {
		memcpy(Temporary.Data, User.Data, User.Size);
	}
	Temporary.Data[User.Size] = (uint8)':';
	if ( Password.Size != 0 ) {
		memcpy(
			Temporary.Data + User.Size + 1u,
			Password.Data,
			Password.Size
		);
	}
	if ( !xrtBase64Encode(
		Temporary.Data,
		iPlain,
		(char*)Temporary.Data,
		iEncoded + 1u,
		&iWritten,
		NULL
	) || (iWritten != iEncoded) ) {
		__xrtHttpBasicTemporaryClose(&Temporary);
		return false;
	}
	memcpy(pOutput, "Basic ", 6u);
	memcpy((bytes)pOutput + 6u, Temporary.Data, iEncoded);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	__xrtHttpBasicTemporaryClose(&Temporary);
	return true;
}



/* 验证 Basic 解码输出和描述符互不重叠。 */
static bool __xrtHttpBasicReadOutputValid(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t iRequired,
	size_t* pSize,
	xhttpbasicauth* pBasic
)
{
	if ( !xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		!xrtMemRangeValid(pBasic, sizeof(*pBasic)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize), Value.Data, Value.Size
		) || xrtMemRangesOverlap(
			pBasic, sizeof(*pBasic), Value.Data, Value.Size
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pBasic, sizeof(*pBasic)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !xrtMemRangeValid(pOutput, iRequired) ||
		xrtMemRangesOverlap(
		pOutput, iRequired, Value.Data, Value.Size
	) || xrtMemRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iRequired
	) || xrtMemRangesOverlap(
		pBasic, sizeof(*pBasic), pOutput, iRequired
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	return true;
}



/* 解码并拆分完整 Basic 字段值。 */
XHTTP_API bool xrtHttpBasicRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicauth* pBasic
)
{
	xhttpauth Auth;
	xhttpbasicauth Basic = { 0 };
	xrt_http_basic_temporary Temporary;
	size_t iDecoded;
	size_t iColon;

	if ( !__xhttpViewValid(Value) ||
		!xrtMemRangeValid(pSize, sizeof(iDecoded)) ||
		!xrtMemRangeValid(pBasic, sizeof(Basic)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !xrtMemRangeValid(
			pOutput, iCapacity
		)) || xrtMemRangesOverlap(
			pSize, sizeof(iDecoded), Value.Data, Value.Size
		) || xrtMemRangesOverlap(
			pBasic, sizeof(Basic), Value.Data, Value.Size
		) || xrtMemRangesOverlap(
			pSize, sizeof(iDecoded), pBasic, sizeof(Basic)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pBasic, &Basic, sizeof(Basic));
	if ( !xrtHttpAuthParse(Value, &Auth) ) {
		return false;
	}
	if ( !xrtHttpTokenEqual(
			Auth.Scheme, XRT_STR_LITERAL("Basic")
		) || (Auth.Kind != XHTTP_AUTH_TOKEN68) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( !xrtBase64Decode(
		Auth.Data.Data,
		Auth.Data.Size,
		NULL,
		0,
		&iDecoded,
		NULL
	) || !__xrtHttpBasicTemporaryOpen(
		&Temporary, iDecoded
	) ) {
		return false;
	}
	if ( !xrtBase64Decode(
		Auth.Data.Data,
		Auth.Data.Size,
		Temporary.Data,
		iDecoded,
		&iDecoded,
		NULL
	) ) {
		__xrtHttpBasicTemporaryClose(&Temporary);
		return false;
	}
	for ( iColon = 0; iColon < iDecoded; iColon++ ) {
		if ( Temporary.Data[iColon] == (uint8)':' ) {
			break;
		}
	}
	if ( (iColon == iDecoded) ||
		!__xrtHttpBasicTextValid(
			(xstrview){ (cstr)Temporary.Data, iColon }, true
		) || !__xrtHttpBasicTextValid(
			(xstrview){
				(cstr)Temporary.Data + iColon + 1u,
				iDecoded - iColon - 1u
			},
			false
		) ) {
		__xrtHttpBasicTemporaryClose(&Temporary);
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( !__xrtHttpBasicReadOutputValid(
		Value, pOutput, iCapacity, iDecoded, pSize, pBasic
	) ) {
		__xrtHttpBasicTemporaryClose(&Temporary);
		return false;
	}
	if ( pOutput != NULL ) {
		memcpy(pOutput, Temporary.Data, iDecoded);
		Basic.User = (xstrview){
			(cstr)pOutput,
			iColon
		};
		Basic.Password = (xstrview){
			(cstr)pOutput + iColon + 1u,
			iDecoded - iColon - 1u
		};
		memcpy(pBasic, &Basic, sizeof(Basic));
		memcpy(pSize, &iDecoded, sizeof(iDecoded));
	}
	__xrtHttpBasicTemporaryClose(&Temporary);
	return true;
}



/* 校验 Basic challenge 读取输出、输入和描述符互不覆盖。 */
static bool __xrtHttpBasicChallengeReadValid(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
)
{
	if ( !__xhttpViewValid(Value) ||
		!xrtMemRangeValid(pSize, sizeof(size_t)) ||
		!xrtMemRangeValid(pChallenge, sizeof(*pChallenge)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !xrtMemRangeValid(
			pOutput, iCapacity
		)) || xrtMemRangesOverlap(
			pSize, sizeof(size_t), Value.Data, Value.Size
		) || xrtMemRangesOverlap(
			pChallenge, sizeof(*pChallenge),
			Value.Data, Value.Size
		) || xrtMemRangesOverlap(
			pSize, sizeof(size_t),
			pChallenge, sizeof(*pChallenge)
		) || ((pOutput != NULL) && (xrtMemRangesOverlap(
			pOutput, iCapacity, Value.Data, Value.Size
		) || xrtMemRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(size_t)
		) || xrtMemRangesOverlap(
			pOutput, iCapacity,
			pChallenge, sizeof(*pChallenge)
		))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 解析并解码 RFC 7617 Basic challenge。 */
XHTTP_API bool xrtHttpBasicChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
)
{
	xhttpbasicchallenge Challenge = { 0 };
	xhttpauth Auth;
	xhttpparam Param;
	xhttpparam Realm = { 0 };
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iRequired = 0;
	bool bRealm = false;
	bool bCharset = false;

	if ( !__xrtHttpBasicChallengeReadValid(
		Value, pOutput, iCapacity, pSize, pChallenge
	) ) {
		return false;
	}
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	if ( !xrtHttpAuthParse(Value, &Auth) ) {
		return false;
	}
	if ( !xrtHttpTokenEqual(
		Auth.Scheme, XRT_STR_LITERAL("Basic")
	) || (Auth.Kind != XHTTP_AUTH_PARAMS) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	for ( ;; ) {
		Next = xrtHttpAuthParamNext(
			Auth.Data, &iOffset, &Param
		);
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("realm")
		) ) {
			if ( bRealm || !xrtHttpParamValueWrite(
				&Param, NULL, 0, &iRequired
			) ) {
				if ( bRealm ) {
					xrtSetErrorKind(XERR_VALUE);
				}
				return false;
			}
			Realm = Param;
			bRealm = true;
			continue;
		}
		if ( xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("charset")
		) ) {
			if ( bCharset || !xrtHttpParamTokenEqual(
				&Param, XRT_STR_LITERAL("UTF-8")
			) ) {
				xrtSetErrorKind(XERR_VALUE);
				return false;
			}
			bCharset = true;
			Challenge.Utf8 = true;
		}
	}
	if ( !bRealm ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		memcpy(pChallenge, &Challenge, sizeof(Challenge));
		return true;
	}
	if ( !xrtMemRangeValid(pOutput, iRequired) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	(void)__xhttpParamValueWriteUnchecked(
		&Realm, (bytes)pOutput
	);
	Challenge.Realm = (xstrview){
		(cstr)pOutput,
		iRequired
	};
	memcpy(pSize, &iRequired, sizeof(iRequired));
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	return true;
}



/* 写出 Basic challenge。 */
XHTTP_API bool xrtHttpBasicChallengeWrite(
	xstrview Realm,
	bool bUtf8,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const char Prefix[] = "Basic realm=";
	static const char Utf8[] = ", charset=\"UTF-8\"";
	size_t iQuoted;
	size_t iRequired;
	size_t iWritten;
	char* sWrite = (char*)pOutput;

	if ( !__xhttpViewValid(Realm) ||
		!xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		xrtMemRangesOverlap(
			pSize, sizeof(iRequired), Realm.Data, Realm.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtHttpQuotedWrite(
		Realm, NULL, 0, &iQuoted
	) ) {
		return false;
	}
	if ( iQuoted > (SIZE_MAX - (sizeof(Prefix) - 1u)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iRequired = (sizeof(Prefix) - 1u) + iQuoted;
	if ( bUtf8 ) {
		if ( iRequired > (SIZE_MAX - (sizeof(Utf8) - 1u)) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iRequired += sizeof(Utf8) - 1u;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !xrtMemRangeValid(pOutput, iRequired) ||
		xrtMemRangesOverlap(
		pSize, sizeof(iRequired), pOutput, iRequired
	) || xrtMemRangesOverlap(
		pOutput, iRequired, Realm.Data, Realm.Size
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	memcpy(sWrite, Prefix, sizeof(Prefix) - 1u);
	if ( !xrtHttpQuotedWrite(
		Realm,
		sWrite + sizeof(Prefix) - 1u,
		iQuoted,
		&iWritten
	) || (iWritten != iQuoted) ) {
		return false;
	}
	if ( bUtf8 ) {
		memcpy(
			sWrite + sizeof(Prefix) - 1u + iQuoted,
			Utf8,
			sizeof(Utf8) - 1u
		);
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}

#endif
