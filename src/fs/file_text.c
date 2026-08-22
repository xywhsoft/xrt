#include "../internal/xrt_file.h"
#include "../internal/xrt_charset.h"



#if defined(XRT_FEATURE_FILE_TEXT)

/* 检查调用方给出的编码和错误策略。 */
static bool __xrtFileTextOptions(xencoding Encoding,
	xutfpolicy Policy, bool bAllowUnknown)
{
	bool bEncoding = ((Encoding >= XENCODING_UTF8) &&
		(Encoding <= XENCODING_UTF32_BE)) ||
		(bAllowUnknown && (Encoding == XENCODING_UNKNOWN));

	if ( !bEncoding || ((Policy != XUTF_STRICT) &&
		 (Policy != XUTF_REPLACE)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 用文件文本错误包装字符集错误，并保留原始原因链。 */
static void __xrtFileTextWrap(cstr sOperation, cstr sMessage)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	if ( pCause == NULL ) {
		__xrtFileError(XERR_VALUE, XFILE_ERROR_TEXT,
			sOperation, sMessage);
		return;
	}
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = xrtErrorKind(pCause);
	Desc.Domain = "xrt.file";
	Desc.Code = XFILE_ERROR_TEXT;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCause);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 按 BOM、显式选项和严格检测确定正文编码与起始位置。 */
static bool __xrtFileTextEncoding(xbytesview Data, xencoding Requested,
	xencoding* pEncoding, size_t* pBomSize)
{
	xencoding Bom = xrtEncodingBom(Data, pBomSize);

	if ( (Requested != XENCODING_UNKNOWN) &&
		 (Bom != XENCODING_UNKNOWN) && (Requested != Bom) ) {
		__xrtFileError(XERR_VALUE, XFILE_ERROR_TEXT, "read-text",
			"the file BOM conflicts with the requested encoding");
		return false;
	}
	if ( Requested != XENCODING_UNKNOWN ) {
		*pEncoding = Requested;
		return true;
	}
	if ( Bom != XENCODING_UNKNOWN ) {
		*pEncoding = Bom;
		return true;
	}
	if ( Data.Size == 0u ) {
		*pEncoding = XENCODING_UTF8;
		return true;
	}
	{
		xencodingguess Guess = xrtEncodingGuess(Data);

		if ( Guess.Encoding == XENCODING_UNKNOWN ) {
			__xrtFileError(XERR_VALUE, XFILE_ERROR_TEXT, "read-text",
				"the file is not valid detectable Unicode text");
			return false;
		}
		*pEncoding = Guess.Encoding;
	}
	return true;
}



/* 报告带 Unicode 错误位置的文件文本失败。 */
static void __xrtFileTextInvalid(cstr sOperation, size_t iOffset)
{
	__xrtUtfSetInvalid(sOperation, iOffset);
	__xrtFileTextWrap(sOperation, "invalid UTF-8 text");
}



/* 复用拥有的合法 UTF-8 文件缓冲，并在原位移除 BOM。 */
static bool __xrtFileTextUtf8(bytes pRaw, size_t iRawSize,
	size_t iBomSize, xutfpolicy Policy, str* pText, size_t* pSize)
{
	size_t iTextSize = iRawSize - iBomSize;
	size_t iError;

	*pText = NULL;
	if ( !xrtUtf8Valid(
		(xstrview){ (cstr)(pRaw + iBomSize), iTextSize }, &iError) ) {
		if ( Policy == XUTF_REPLACE ) {
			return true;
		}
		__xrtFileTextInvalid("read-text", iError);
		return false;
	}
	if ( iBomSize != 0u ) {
		memmove(pRaw, pRaw + iBomSize, iTextSize);
	}
	pRaw[iTextSize] = 0;
	*pText = (str)pRaw;
	if ( pSize != NULL ) {
		*pSize = iTextSize;
	}
	return true;
}



/* 在源文件字节硬上限内读取、移除 BOM 并转换为拥有 UTF-8 字符串。 */
XRT_API str xrtFileReadTextLimit(cstr sPath, xencoding Encoding,
	xutfpolicy Policy, size_t iLimit, size_t* pSize)
{
	bytes pRaw;
	bytes pText;
	size_t iRawSize;
	size_t iBomSize = 0;
	size_t iTextSize = 0;
	xencoding SourceEncoding;

	if ( pSize != NULL ) {
		*pSize = 0;
	}
	if ( !__xrtFileTextOptions(Encoding, Policy, true) ) {
		return NULL;
	}
	pRaw = xrtFileReadAllLimit(sPath, iLimit, &iRawSize);
	if ( pRaw == NULL ) {
		return NULL;
	}
	if ( !__xrtFileTextEncoding(
		(xbytesview){ pRaw, iRawSize }, Encoding,
		&SourceEncoding, &iBomSize) ) {
		xrtFree(pRaw);
		return NULL;
	}
	if ( SourceEncoding == XENCODING_UTF8 ) {
		str sUtf8;

		if ( !__xrtFileTextUtf8(
			pRaw, iRawSize, iBomSize, Policy, &sUtf8, pSize) ) {
			xrtFree(pRaw);
			return NULL;
		}
		if ( sUtf8 != NULL ) {
			return sUtf8;
		}
	}
	pText = xrtTranscode(
		(xbytesview){ pRaw + iBomSize, iRawSize - iBomSize },
		SourceEncoding, XENCODING_UTF8, Policy, false, &iTextSize);
	xrtFree(pRaw);
	if ( pText == NULL ) {
		__xrtFileTextWrap("read-text",
			"failed to decode the file as Unicode text");
		return NULL;
	}
	if ( pSize != NULL ) {
		*pSize = iTextSize;
	}
	return (str)pText;
}



/* 不限制业务大小地读取并转换为 UTF-8。 */
XRT_API str xrtFileReadText(cstr sPath, xencoding Encoding,
	xutfpolicy Policy, size_t* pSize)
{
	return xrtFileReadTextLimit(
		sPath, Encoding, Policy, SIZE_MAX - 1u, pSize);
}



/* 转换 UTF-8 视图，并按选择的整文件策略写入。 */
static bool __xrtFileWriteText(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom, bool bAtomic)
{
	bytes pData;
	size_t iSize;
	bool bResult;

	if ( ((Text.Data == NULL) && (Text.Size != 0u)) ||
		 !__xrtFileTextOptions(Encoding, Policy, false) ) {
		if ( (Text.Data == NULL) && (Text.Size != 0u) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( (Encoding == XENCODING_UTF8) && !bWriteBom ) {
		size_t iError;

		if ( xrtUtf8Valid(Text, &iError) ) {
			return bAtomic ?
				xrtFileWriteAtomic(sPath,
					(xbytesview){ (cbytes)Text.Data, Text.Size }) :
				xrtFileWriteAll(sPath,
					(xbytesview){ (cbytes)Text.Data, Text.Size });
		}
		if ( Policy == XUTF_STRICT ) {
			__xrtFileTextInvalid(
				bAtomic ? "write-text-atomic" : "write-text", iError);
			return false;
		}
	}
	pData = xrtTranscode(
		(xbytesview){ (cbytes)Text.Data, Text.Size },
		XENCODING_UTF8, Encoding, Policy, bWriteBom, &iSize);
	if ( pData == NULL ) {
		__xrtFileTextWrap(bAtomic ? "write-text-atomic" : "write-text",
			"failed to encode the UTF-8 text");
		return false;
	}
	bResult = bAtomic ?
		xrtFileWriteAtomic(sPath, (xbytesview){ pData, iSize }) :
		xrtFileWriteAll(sPath, (xbytesview){ pData, iSize });
	xrtFree(pData);
	return bResult;
}



/* 把 UTF-8 文本转换为目标编码后完整写入。 */
XRT_API bool xrtFileWriteText(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom)
{
	return __xrtFileWriteText(sPath, Text, Encoding,
		Policy, bWriteBom, false);
}



/* 把 UTF-8 文本转换后原子替换目标文件。 */
XRT_API bool xrtFileWriteTextAtomic(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom)
{
	return __xrtFileWriteText(sPath, Text, Encoding,
		Policy, bWriteBom, true);
}

#endif
