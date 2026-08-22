#include "../internal/xrt_charset.h"



#if defined(XRT_FEATURE_CHARSET_DETECT)

/* 严格验证一个显式端序的 UTF-16 字节流。 */
static bool __xrtDetectUtf16(xbytesview Data, bool bBigEndian)
{
	uint16 arrUnits[2];
	size_t iPosition = 0;

	if ( (Data.Size == 0) || ((Data.Size & 1u) != 0) ) {
		return false;
	}
	while ( iPosition < Data.Size ) {
		xrt_utf_decode Decode;

		arrUnits[0] = __xrtCharsetRead16(Data.Data + iPosition, bBigEndian);
		if ( (iPosition + 4u) <= Data.Size ) {
			arrUnits[1] = __xrtCharsetRead16(Data.Data + iPosition + 2u,
				bBigEndian);
		}
		Decode = __xrtUtf16Decode(arrUnits,
			(iPosition + 4u) <= Data.Size ? 2u : 1u);
		if ( Decode.Status != XUTF_OK ) {
			return false;
		}
		iPosition += Decode.Read * 2u;
	}
	return true;
}



/* 严格验证一个显式端序的 UTF-32 字节流。 */
static bool __xrtDetectUtf32(xbytesview Data, bool bBigEndian)
{
	if ( (Data.Size == 0) || ((Data.Size & 3u) != 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Data.Size; i += 4u ) {
		if ( !xrtUnicodeScalar(__xrtCharsetRead32(Data.Data + i, bBigEndian)) ) {
			return false;
		}
	}
	return true;
}



/* 根据 UTF-32 常见高位零字节分布给候选端序评分。 */
static uint8 __xrtDetectUtf32Score(xbytesview Data, bool bBigEndian)
{
	size_t iExpected = 0;
	size_t iOpposite = 0;
	size_t iUnits = Data.Size / 4u;
	uint8 iScore;

	/* 单个码元不足以排除带零字节的 UTF-8 或普通二进制数据。 */
	if ( iUnits < 2u ) {
		return 0;
	}

	for ( size_t i = 0; i < Data.Size; i += 4u ) {
		if ( bBigEndian ) {
			iExpected += (Data.Data[i] == 0) + (Data.Data[i + 1u] == 0);
			iOpposite += (Data.Data[i + 2u] == 0) + (Data.Data[i + 3u] == 0);
		} else {
			iExpected += (Data.Data[i + 2u] == 0) + (Data.Data[i + 3u] == 0);
			iOpposite += (Data.Data[i] == 0) + (Data.Data[i + 1u] == 0);
		}
	}
	if ( (iExpected < iUnits) || (iExpected <= iOpposite) ) {
		return 0;
	}
	if ( iExpected == (iUnits * 2u) ) {
		iScore = 90;
	} else if ( iExpected >= (iUnits + ((iUnits / 2u) + (iUnits & 1u))) ) {
		iScore = 85;
	} else {
		iScore = 80;
	}
	if ( (iUnits == 2u) && (iScore > 80u) ) {
		iScore = 80;
	} else if ( (iUnits == 3u) && (iScore > 85u) ) {
		iScore = 85;
	}
	return iScore;
}



/* 根据 UTF-16 ASCII 文本常见的隔位零字节分布给候选端序评分。 */
static uint8 __xrtDetectUtf16Score(xbytesview Data, bool bBigEndian)
{
	size_t iExpected = 0;
	size_t iOpposite = 0;
	size_t iUnits = Data.Size / 2u;
	uint8 iScore;

	/* 单个码元没有足够的字节分布信息，不能给出宽编码结论。 */
	if ( iUnits < 2u ) {
		return 0;
	}

	for ( size_t i = 0; i < Data.Size; i += 2u ) {
		if ( Data.Data[i + (bBigEndian ? 0u : 1u)] == 0 ) {
			iExpected++;
		}
		if ( Data.Data[i + (bBigEndian ? 1u : 0u)] == 0 ) {
			iOpposite++;
		}
	}
	if ( (iExpected < ((iUnits / 3u) + ((iUnits % 3u) != 0))) ||
		 (iExpected <= iOpposite) ||
		 ((iExpected - iOpposite) <= iOpposite) ) {
		return 0;
	}
	if ( iExpected == iUnits ) {
		iScore = 85;
	} else if ( iExpected >= (iUnits - (iUnits / 4u)) ) {
		iScore = 78;
	} else {
		iScore = 70;
	}
	if ( (iUnits == 2u) && (iScore > 70u) ) {
		iScore = 70;
	} else if ( (iUnits == 3u) && (iScore > 80u) ) {
		iScore = 80;
	}
	return iScore;
}



/* 保留更高置信度的候选编码。 */
static void __xrtDetectKeep(xencodingguess* pGuess, xencoding Encoding,
	uint8 iConfidence)
{
	if ( iConfidence > pGuess->Confidence ) {
		pGuess->Encoding = Encoding;
		pGuess->Confidence = iConfidence;
	}
}



/* 根据 BOM、严格合法性和零字节分布猜测 Unicode 编码。 */
XRT_API xencodingguess xrtEncodingGuess(xbytesview Data)
{
	xencodingguess Guess;
	xencoding BomEncoding;
	bool bHasZero = false;
	bool bHasNonAscii = false;

	Guess.Encoding = XENCODING_UNKNOWN;
	Guess.BomSize = 0;
	Guess.Confidence = 0;
	if ( (Data.Data == NULL) && (Data.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return Guess;
	}
	BomEncoding = xrtEncodingBom(Data, &Guess.BomSize);
	if ( BomEncoding != XENCODING_UNKNOWN ) {
		Guess.Encoding = BomEncoding;
		Guess.Confidence = 100;
		return Guess;
	}
	if ( Data.Size == 0 ) {
		return Guess;
	}

	/* 零字节存在时先检查宽编码，避免把 UTF-16 ASCII 当作 UTF-8。 */
	for ( size_t i = 0; i < Data.Size; i++ ) {
		bHasZero = bHasZero || (Data.Data[i] == 0);
		bHasNonAscii = bHasNonAscii || (Data.Data[i] >= 0x80u);
	}
	if ( bHasZero && ((Data.Size & 3u) == 0) ) {
		if ( __xrtDetectUtf32(Data, false) ) {
			__xrtDetectKeep(&Guess, XENCODING_UTF32_LE,
				__xrtDetectUtf32Score(Data, false));
		}
		if ( __xrtDetectUtf32(Data, true) ) {
			__xrtDetectKeep(&Guess, XENCODING_UTF32_BE,
				__xrtDetectUtf32Score(Data, true));
		}
	}
	if ( bHasZero && ((Data.Size & 1u) == 0) ) {
		if ( __xrtDetectUtf16(Data, false) ) {
			__xrtDetectKeep(&Guess, XENCODING_UTF16_LE,
				__xrtDetectUtf16Score(Data, false));
		}
		if ( __xrtDetectUtf16(Data, true) ) {
			__xrtDetectKeep(&Guess, XENCODING_UTF16_BE,
				__xrtDetectUtf16Score(Data, true));
		}
	}
	if ( Guess.Confidence != 0 ) {
		return Guess;
	}

	/* 无 BOM 的合法 UTF-8 仍可能是 ASCII 兼容编码，因此降低纯 ASCII 置信度。 */
	if ( xrtUtf8Valid((xstrview){ (cstr)Data.Data, Data.Size }, NULL) ) {
		Guess.Encoding = XENCODING_UTF8;
		Guess.Confidence = bHasNonAscii ? 90u : (bHasZero ? 50u : 40u);
	}
	return Guess;
}

#endif
