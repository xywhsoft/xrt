#include "../internal/xrt_pattern.h"



#if defined(XRT_FEATURE_PATTERN)

typedef struct __xrt_pattern_measure {
	size_t AtomCount;
	size_t CaptureCount;
	size_t DataBytes;
} __xrt_pattern_measure;



typedef struct __xrt_pattern_field {
	__xrt_pattern_atom_kind Kind;
	size_t NameOffset;
	size_t NameSize;
	size_t LiteralSize;
	size_t PrefixSize;
	size_t SuffixSize;
	size_t CaptureOpen;
	size_t CaptureEnd;
} __xrt_pattern_field;



/* 已验证字面量范围的双花括号解码；pTarget==NULL 时只返回字节数。 */
static size_t __xrtPatternLiteralDecode(
	xstrview Pattern,
	size_t iStart,
	size_t iEnd,
	char* pTarget
)
{
	size_t iWrite = 0;

	for ( size_t i = iStart; i < iEnd; ) {
		char iByte = Pattern.Data[i];

		if ( pTarget != NULL ) {
			pTarget[iWrite] = iByte;
		}
		iWrite++;
		if ( (iByte == '{') || (iByte == '}') ) {
			i += 2u;
		} else {
			i++;
		}
	}
	return iWrite;
}



/* 比较已验证并按双花括号转义的字面量范围，不建立临时字符串。 */
static bool __xrtPatternLiteralEqual(
	xstrview Pattern,
	size_t iStart,
	size_t iEnd,
	const char* sText,
	size_t iTextSize
)
{
	size_t iText = 0;

	for ( size_t i = iStart; i < iEnd; ) {
		char iExpected = Pattern.Data[i];

		if ( (iText >= iTextSize) || (sText[iText] != iExpected) ) {
			return false;
		}
		iText++;
		if ( (iExpected == '{') || (iExpected == '}') ) {
			i += 2u;
		} else {
			i++;
		}
	}
	return iText == iTextSize;
}



/* 检查 size_t 加法并按指定类型对齐。 */
static bool __xrtPatternLayout(
	size_t* pTotal,
	size_t iAlignment,
	size_t iCount,
	size_t iItemSize,
	size_t* pOffset
)
{
	size_t iPadding;
	size_t iBytes;

	if ( (pTotal == NULL) || (iAlignment == 0) ) {
		__xrtPatternSetInternal();
		return false;
	}
	iPadding = (iAlignment - (*pTotal % iAlignment)) % iAlignment;
	if ( *pTotal > (SIZE_MAX - iPadding) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	*pTotal += iPadding;
	if ( pOffset != NULL ) {
		*pOffset = *pTotal;
	}
	if ( (iItemSize != 0) && (iCount > (SIZE_MAX / iItemSize)) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	iBytes = iCount * iItemSize;
	if ( *pTotal > (SIZE_MAX - iBytes) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	*pTotal += iBytes;
	return true;
}



/* 设置带可机器读取位置的错误；短数据始终完整写入固定缓冲区。 */
void __xrtPatternError(
	xerrkind Kind,
	xpatternerror Code,
	cstr sOperation,
	cstr sMessage,
	bool bHasPattern,
	size_t iPattern,
	bool bHasOffset,
	size_t iOffset
)
{
	char arrData[128];
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	memset(arrData, 0, sizeof(arrData));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.pattern";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	if ( bHasPattern && bHasOffset ) {
		(void)snprintf(
			arrData,
			sizeof(arrData),
			"pattern=%llu;offset=%llu",
			(unsigned long long)iPattern,
			(unsigned long long)iOffset
		);
		Desc.Data = arrData;
	} else if ( bHasPattern ) {
		(void)snprintf(
			arrData,
			sizeof(arrData),
			"pattern=%llu",
			(unsigned long long)iPattern
		);
		Desc.Data = arrData;
	} else if ( bHasOffset ) {
		(void)snprintf(
			arrData,
			sizeof(arrData),
			"offset=%llu",
			(unsigned long long)iOffset
		);
		Desc.Data = arrData;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 检查明确长度视图没有非法的空指针与非零长度组合。 */
bool __xrtPatternViewValid(xstrview Text)
{
	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	return true;
}



/* 检查 ASCII 捕获名，避免 Unicode 归一化和 locale 进入热模块语义。 */
static bool __xrtPatternNameValid(const char* sName, size_t iSize)
{
	uint8 iByte;

	if ( (sName == NULL) || (iSize == 0) ) {
		return false;
	}
	iByte = (uint8)sName[0];
	if ( !(((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
		   ((iByte >= (uint8)'a') && (iByte <= (uint8)'z')) ||
		   (iByte == (uint8)'_')) ) {
		return false;
	}
	for ( size_t i = 1u; i < iSize; i++ ) {
		iByte = (uint8)sName[i];
		if ( !(((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
			   ((iByte >= (uint8)'a') && (iByte <= (uint8)'z')) ||
			   ((iByte >= (uint8)'0') && (iByte <= (uint8)'9')) ||
			   (iByte == (uint8)'_')) ) {
			return false;
		}
	}
	return true;
}



/*
	把一个非分隔字段分类为字面量、整字段捕获、前后缀捕获或尾捕获；
	字面量中的双花括号在写入阶段还原为单个字节。
*/
static bool __xrtPatternField(
	xstrview Pattern,
	size_t iStart,
	size_t iEnd,
	bool bLast,
	bool bHasPatternIndex,
	size_t iPatternIndex,
	cstr sOperation,
	__xrt_pattern_field* pField
)
{
	const char* sData = Pattern.Data;
	bool bCapture = false;
	bool bTail = false;

	memset(pField, 0, sizeof(*pField));
	for ( size_t i = iStart; i < iEnd; ) {
		if ( sData[i] == '{' ) {
			size_t iClose;
			size_t iName;

			if ( ((i + 1u) < iEnd) && (sData[i + 1u] == '{') ) {
				pField->LiteralSize++;
				i += 2u;
				continue;
			}
			if ( bCapture ) {
				__xrtPatternError(
					XERR_VALUE,
					XPATTERN_ERROR_PATTERN,
					sOperation,
					"a pattern field cannot contain multiple captures",
					bHasPatternIndex,
					iPatternIndex,
					true,
					i
				);
				return false;
			}
			iClose = i + 1u;
			while ( (iClose < iEnd) && (sData[iClose] != '}') ) {
				if ( sData[iClose] == '{' ) {
					__xrtPatternError(
						XERR_VALUE,
						XPATTERN_ERROR_PATTERN,
						sOperation,
						"capture contains an unexpected brace",
						bHasPatternIndex,
						iPatternIndex,
						true,
						iClose
					);
					return false;
				}
				iClose++;
			}
			if ( iClose == iEnd ) {
				__xrtPatternError(
					XERR_VALUE,
					XPATTERN_ERROR_PATTERN,
					sOperation,
					"capture is missing its closing brace",
					bHasPatternIndex,
					iPatternIndex,
					true,
					i
				);
				return false;
			}
			iName = i + 1u;
			if ( (iName < iClose) && (sData[iName] == '*') ) {
				bTail = true;
				iName++;
			}
			if ( !__xrtPatternNameValid(sData + iName, iClose - iName) ) {
				__xrtPatternError(
					XERR_VALUE,
					XPATTERN_ERROR_PATTERN,
					sOperation,
					"capture name is not a valid ASCII identifier",
					bHasPatternIndex,
					iPatternIndex,
					true,
					iName
				);
				return false;
			}
			bCapture = true;
			pField->NameOffset = iName;
			pField->NameSize = iClose - iName;
			pField->PrefixSize = pField->LiteralSize;
			pField->CaptureOpen = i;
			pField->CaptureEnd = iClose + 1u;
			i = iClose + 1u;
			continue;
		}
		if ( sData[i] == '}' ) {
			if ( ((i + 1u) >= iEnd) || (sData[i + 1u] != '}') ) {
				__xrtPatternError(
					XERR_VALUE,
					XPATTERN_ERROR_PATTERN,
					sOperation,
					"literal brace must be escaped by doubling it",
					bHasPatternIndex,
					iPatternIndex,
					true,
					i
				);
				return false;
			}
			pField->LiteralSize++;
			i += 2u;
			continue;
		}
		pField->LiteralSize++;
		i++;
	}
	if ( !bCapture ) {
		pField->Kind = __XRT_PATTERN_ATOM_LITERAL;
		return true;
	}
	pField->SuffixSize = pField->LiteralSize - pField->PrefixSize;
	if ( bTail ) {
		if ( (pField->CaptureOpen != iStart) ||
			 (pField->CaptureEnd != iEnd) || !bLast ) {
			__xrtPatternError(
				XERR_VALUE,
				XPATTERN_ERROR_PATTERN,
				sOperation,
				"tail capture must occupy the final pattern field",
				bHasPatternIndex,
				iPatternIndex,
				true,
				pField->CaptureOpen
			);
			return false;
		}
		pField->Kind = __XRT_PATTERN_ATOM_TAIL;
	} else if ( pField->LiteralSize == 0 ) {
		pField->Kind = __XRT_PATTERN_ATOM_CAPTURE;
	} else {
		pField->Kind = __XRT_PATTERN_ATOM_AFFIX;
	}
	return true;
}



/* 判断当前捕获名是否已在前面的字段中出现。 */
static bool __xrtPatternNameSeen(
	xstrview Pattern,
	size_t iLimit,
	size_t iName,
	size_t iNameSize
)
{
	size_t i = 0;

	while ( i < iLimit ) {
		if ( Pattern.Data[i] != '{' ) {
			i += (Pattern.Data[i] == '}') && ((i + 1u) < iLimit) &&
				(Pattern.Data[i + 1u] == '}') ? 2u : 1u;
			continue;
		}
		if ( ((i + 1u) < iLimit) && (Pattern.Data[i + 1u] == '{') ) {
			i += 2u;
			continue;
		}
		{
			size_t iOldName = i + 1u;
			size_t iClose = iOldName;

			while ( (iClose < iLimit) && (Pattern.Data[iClose] != '}') ) {
				iClose++;
			}
			if ( (iOldName < iClose) && (Pattern.Data[iOldName] == '*') ) {
				iOldName++;
			}
			if ( ((iClose - iOldName) == iNameSize) &&
				 (memcmp(
					Pattern.Data + iOldName,
					Pattern.Data + iName,
					iNameSize
				 ) == 0) ) {
				return true;
			}
			i = iClose < iLimit ? iClose + 1u : iLimit;
		}
	}
	return false;
}



/* 验证完整模式并可选地写入解析原子与捕获名称。 */
static bool __xrtPatternParse(
	xstrview Pattern,
	const __xrt_pattern_options* pOptions,
	bool bHasPatternIndex,
	size_t iPatternIndex,
	cstr sOperation,
	__xrt_pattern_measure* pMeasure,
	__xrt_pattern_atom* arrAtom,
	__xrt_pattern_capture* arrCapture,
	char* sStorage
)
{
	__xrt_pattern_measure Measure;
	size_t iStorage = 0;
	size_t iField = 0;
	size_t i = 0;

	memset(&Measure, 0, sizeof(Measure));
	while ( i < Pattern.Size ) {
		if ( __xrtPatternIsSeparator(pOptions, (uint8)Pattern.Data[i]) ) {
			if ( arrAtom != NULL ) {
				arrAtom[Measure.AtomCount].Kind = __XRT_PATTERN_ATOM_SEPARATOR;
				arrAtom[Measure.AtomCount].Byte = (uint8)Pattern.Data[i];
			}
			Measure.AtomCount++;
			i++;
			continue;
		}
		{
			size_t iStart = i;
			size_t iEnd;
			__xrt_pattern_field Field;

			while ( (i < Pattern.Size) &&
				!__xrtPatternIsSeparator(pOptions, (uint8)Pattern.Data[i]) ) {
				i++;
			}
			iEnd = i;
			if ( !__xrtPatternField(
				Pattern,
				iStart,
				iEnd,
				iEnd == Pattern.Size,
				bHasPatternIndex,
				iPatternIndex,
				sOperation,
				&Field
			) ) {
				return false;
			}
			if ( (Field.Kind == __XRT_PATTERN_ATOM_CAPTURE) ||
				 (Field.Kind == __XRT_PATTERN_ATOM_AFFIX) ||
				 (Field.Kind == __XRT_PATTERN_ATOM_TAIL) ) {
				if ( Measure.CaptureCount >= pOptions->MaxCaptures ) {
					__xrtPatternError(
						XERR_RANGE,
						XPATTERN_ERROR_LIMIT,
						sOperation,
						"pattern exceeds its capture limit",
						bHasPatternIndex,
						iPatternIndex,
						true,
						iStart
					);
					return false;
				}
				if ( __xrtPatternNameSeen(
					Pattern,
					iStart,
					Field.NameOffset,
					Field.NameSize
				) ) {
					__xrtPatternError(
						XERR_VALUE,
						XPATTERN_ERROR_PATTERN,
						sOperation,
						"capture name is duplicated in the pattern",
						bHasPatternIndex,
						iPatternIndex,
						true,
						Field.NameOffset
					);
					return false;
				}
				if ( arrAtom != NULL ) {
					arrAtom[Measure.AtomCount].Kind = (uint8)Field.Kind;
					arrAtom[Measure.AtomCount].CaptureIndex =
						(uint32)Measure.CaptureCount;
					if ( Field.Kind == __XRT_PATTERN_ATOM_AFFIX ) {
						arrAtom[Measure.AtomCount].Text = (xstrview){
							sStorage + iStorage + Field.NameSize,
							Field.PrefixSize
						};
						arrAtom[Measure.AtomCount].Suffix = (xstrview){
							sStorage + iStorage + Field.NameSize + Field.PrefixSize,
							Field.SuffixSize
						};
					}
				}
				if ( arrCapture != NULL ) {
					arrCapture[Measure.CaptureCount].Name =
						(xstrview){ sStorage + iStorage, Field.NameSize };
					arrCapture[Measure.CaptureCount].FieldIndex = (uint32)iField;
					arrCapture[Measure.CaptureCount].PrefixSize =
						(uint32)Field.PrefixSize;
					arrCapture[Measure.CaptureCount].SuffixSize =
						(uint32)Field.SuffixSize;
					arrCapture[Measure.CaptureCount].Kind = (uint8)Field.Kind;
					memcpy(
						sStorage + iStorage,
						Pattern.Data + Field.NameOffset,
						Field.NameSize
					);
				}
				if ( Measure.DataBytes > (SIZE_MAX - Field.NameSize) ) {
					__xrtPatternSetSizeOverflow();
					return false;
				}
				Measure.DataBytes += Field.NameSize;
				iStorage += Field.NameSize;
				if ( Field.Kind == __XRT_PATTERN_ATOM_AFFIX ) {
					size_t iLiteralBytes = Field.PrefixSize + Field.SuffixSize;

					if ( (iLiteralBytes < Field.PrefixSize) ||
						 (Measure.DataBytes > (SIZE_MAX - iLiteralBytes)) ) {
						__xrtPatternSetSizeOverflow();
						return false;
					}
					if ( sStorage != NULL ) {
						(void)__xrtPatternLiteralDecode(
							Pattern,
							iStart,
							Field.CaptureOpen,
							sStorage + iStorage
						);
						(void)__xrtPatternLiteralDecode(
							Pattern,
							Field.CaptureEnd,
							iEnd,
							sStorage + iStorage + Field.PrefixSize
						);
					}
					Measure.DataBytes += iLiteralBytes;
					iStorage += iLiteralBytes;
				}
				Measure.CaptureCount++;
			} else {
				if ( arrAtom != NULL ) {
					arrAtom[Measure.AtomCount].Kind = __XRT_PATTERN_ATOM_LITERAL;
					arrAtom[Measure.AtomCount].Text =
						(xstrview){ sStorage + iStorage, Field.LiteralSize };
				}
				if ( sStorage != NULL ) {
					(void)__xrtPatternLiteralDecode(
						Pattern,
						iStart,
						iEnd,
						sStorage + iStorage
					);
				}
				if ( Measure.DataBytes > (SIZE_MAX - Field.LiteralSize) ) {
					__xrtPatternSetSizeOverflow();
					return false;
				}
				Measure.DataBytes += Field.LiteralSize;
				iStorage += Field.LiteralSize;
			}
			Measure.AtomCount++;
			iField++;
		}
	}
	if ( pMeasure != NULL ) {
		*pMeasure = Measure;
	}
	return true;
}



/* 验证配置并将分隔字节集合规范化为升序无重复数组。 */
bool __xrtPatternOptionsInit(
	const xpatternconfig* pConfig,
	__xrt_pattern_options* pOptions,
	cstr sOperation
)
{
	if ( (pConfig == NULL) || (pOptions == NULL) ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	if ( !__xrtPatternViewValid(pConfig->Separators) ) {
		return false;
	}
	if ( (pConfig->Flags != 0) ||
		 (pConfig->Separators.Size > 256u) ||
		 (pConfig->MaxPatternBytes == 0) ||
		 (pConfig->MaxPatternBytes >= UINT32_MAX) ||
		 (pConfig->MaxPatterns == 0) ||
		 (pConfig->MaxPatterns >= UINT32_MAX) ||
		 (pConfig->MaxCaptures == 0) ||
		 (pConfig->MaxCaptures > UINT32_MAX) ||
		 (pConfig->MaxStates == 0) ||
		 (pConfig->MaxStates >= UINT32_MAX) ||
		 (pConfig->MaxCompiledBytes < sizeof(xpattern)) ) {
		__xrtPatternError(
			XERR_ARGUMENT,
			XPATTERN_ERROR_CONFIG,
			sOperation,
			"invalid pattern configuration",
			false,
			0,
			false,
			0
		);
		return false;
	}
	for ( size_t i = 0; i <
		(sizeof(pConfig->Reserved) / sizeof(pConfig->Reserved[0])); i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtPatternError(
				XERR_ARGUMENT,
				XPATTERN_ERROR_CONFIG,
				sOperation,
				"reserved pattern configuration fields must be zero",
				false,
				0,
				false,
				0
			);
			return false;
		}
	}
	memset(pOptions, 0, sizeof(*pOptions));
	for ( size_t i = 0; i < pConfig->Separators.Size; i++ ) {
		uint8 iByte = (uint8)pConfig->Separators.Data[i];
		uint64 iBit = UINT64_C(1) << (iByte & 63u);
		uint64* pWord = &pOptions->SeparatorMask[iByte >> 6u];

		if ( (iByte == (uint8)'{') || (iByte == (uint8)'}') ) {
			__xrtPatternError(
				XERR_ARGUMENT,
				XPATTERN_ERROR_CONFIG,
				sOperation,
				"brace bytes cannot be configured as separators",
				false,
				0,
				false,
				0
			);
			return false;
		}
		*pWord |= iBit;
	}
	for ( size_t i = 0; i < 256u; i++ ) {
		if ( (pOptions->SeparatorMask[i >> 6u] &
			 (UINT64_C(1) << (i & 63u))) != 0 ) {
			pOptions->Separators[pOptions->SeparatorCount++] = (uint8)i;
		}
	}
	if ( pOptions->SeparatorCount == 0 ) {
		pOptions->SeparatorKind = __XRT_PATTERN_SEPARATOR_NONE;
	} else if ( pOptions->SeparatorCount == 1u ) {
		pOptions->SeparatorKind = pOptions->Separators[0] == (uint8)'/' ?
			__XRT_PATTERN_SEPARATOR_SLASH : __XRT_PATTERN_SEPARATOR_BYTE;
	} else if ( pOptions->SeparatorCount == 2u ) {
		pOptions->SeparatorKind = __XRT_PATTERN_SEPARATOR_TWO;
	} else if ( pOptions->SeparatorCount == 3u ) {
		pOptions->SeparatorKind = __XRT_PATTERN_SEPARATOR_THREE;
	} else if ( pOptions->SeparatorCount == 4u ) {
		pOptions->SeparatorKind = __XRT_PATTERN_SEPARATOR_FOUR;
	} else {
		pOptions->SeparatorKind = __XRT_PATTERN_SEPARATOR_BITMAP;
	}
	pOptions->MaxPatternBytes = pConfig->MaxPatternBytes;
	pOptions->MaxPatterns = pConfig->MaxPatterns;
	pOptions->MaxCaptures = pConfig->MaxCaptures;
	pOptions->MaxStates = pConfig->MaxStates;
	pOptions->MaxCompiledBytes = pConfig->MaxCompiledBytes;
	return true;
}



/* 创建并填充一条单块源模式，供 Builder 重复编译时直接复用。 */
__xrt_pattern_source* __xrtPatternSourceCreate(
	const xpatternspec* pSpec,
	const __xrt_pattern_options* pOptions,
	cstr sOperation,
	xpatternid Id,
	uint64 iOrder,
	bool bHasPatternIndex,
	size_t iPatternIndex
)
{
	__xrt_pattern_measure Measure;
	__xrt_pattern_source* pSource;
	size_t iBytes = sizeof(*pSource);
	size_t iAtomOffset;
	size_t iCaptureOffset;
	size_t iPatternOffset;
	size_t iDataOffset;
	uint8* pBase;

	if ( (pSpec == NULL) || (pOptions == NULL) ||
		 (sOperation == NULL) ||
		 !__xrtPatternViewValid(pSpec != NULL ? pSpec->Pattern : (xstrview){ 0 }) ) {
		return NULL;
	}
	if ( pSpec->Flags != 0 ) {
		__xrtPatternError(
			XERR_ARGUMENT,
			XPATTERN_ERROR_CONFIG,
			sOperation,
			"pattern specification flags must be zero",
			bHasPatternIndex,
			iPatternIndex,
			false,
			0
		);
		return NULL;
	}
	if ( pSpec->Pattern.Size > pOptions->MaxPatternBytes ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			sOperation,
			"pattern exceeds its byte limit",
			bHasPatternIndex,
			iPatternIndex,
			false,
			0
		);
		return NULL;
	}
	if ( !__xrtPatternParse(
		pSpec->Pattern,
		pOptions,
		bHasPatternIndex,
		iPatternIndex,
		sOperation,
		&Measure,
		NULL,
		NULL,
		NULL
	) ) {
		return NULL;
	}
	if ( !__xrtPatternLayout(
		&iBytes,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_atom),
		Measure.AtomCount,
		sizeof(__xrt_pattern_atom),
		&iAtomOffset
	) || !__xrtPatternLayout(
		&iBytes,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_capture),
		Measure.CaptureCount,
		sizeof(__xrt_pattern_capture),
		&iCaptureOffset
	) || !__xrtPatternLayout(
		&iBytes,
		1u,
		pSpec->Pattern.Size + 1u,
		1u,
		&iPatternOffset
	) || !__xrtPatternLayout(
		&iBytes,
		1u,
		Measure.DataBytes,
		1u,
		&iDataOffset
	) ) {
		return NULL;
	}
	pSource = (__xrt_pattern_source*)xrtCalloc(1u, iBytes);
	if ( pSource == NULL ) {
		return NULL;
	}
	pBase = (uint8*)pSource;
	pSource->Id = Id;
	pSource->Order = iOrder;
	pSource->Value = pSpec->Value;
	pSource->Priority = pSpec->Priority;
	pSource->Flags = pSpec->Flags;
	pSource->Atoms = (__xrt_pattern_atom*)(pBase + iAtomOffset);
	pSource->AtomCount = Measure.AtomCount;
	pSource->Captures = (__xrt_pattern_capture*)(pBase + iCaptureOffset);
	pSource->CaptureCount = Measure.CaptureCount;
	pSource->Pattern = (xstrview){
		(const char*)(pBase + iPatternOffset),
		pSpec->Pattern.Size
	};
	pSource->StorageBytes = iBytes;
	if ( pSpec->Pattern.Size != 0 ) {
		memcpy(
			(char*)pSource->Pattern.Data,
			pSpec->Pattern.Data,
			pSpec->Pattern.Size
		);
	}
	((char*)pSource->Pattern.Data)[pSpec->Pattern.Size] = 0;
	if ( !__xrtPatternParse(
		pSource->Pattern,
		pOptions,
		bHasPatternIndex,
		iPatternIndex,
		sOperation,
		NULL,
		pSource->Atoms,
		pSource->Captures,
		(char*)(pBase + iDataOffset)
	) ) {
		xrtFree(pSource);
		return NULL;
	}
	return pSource;
}



void __xrtPatternSourceFree(__xrt_pattern_source* pSource)
{
	xrtFree(pSource);
}



/* 初始化公开默认配置。 */
XRT_API void xrtPatternConfigInit(xpatternconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtPatternSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Separators = XRT_STR_LITERAL("/");
	pConfig->MaxPatternBytes = XPATTERN_PATTERN_DEFAULT;
	pConfig->MaxPatterns = XPATTERN_PATTERNS_DEFAULT;
	pConfig->MaxCaptures = XPATTERN_CAPTURES_DEFAULT;
	pConfig->MaxStates = XPATTERN_STATES_DEFAULT;
	pConfig->MaxCompiledBytes = XPATTERN_COMPILED_DEFAULT;
}



/* 解码并比较一个已经验证为字面量的模式字段。 */
static bool __xrtPatternLiteralMatch(
	xstrview Pattern,
	size_t iStart,
	size_t iEnd,
	xstrview Text,
	const __xrt_pattern_options* pOptions,
	size_t* pText
)
{
	size_t iText = *pText;

	for ( size_t i = iStart; i < iEnd; ) {
		char iExpected = Pattern.Data[i];

		if ( (iText >= Text.Size) ||
			 __xrtPatternIsSeparator(pOptions, (uint8)Text.Data[iText]) ||
			 (Text.Data[iText] != iExpected) ) {
			return false;
		}
		iText++;
		if ( (iExpected == '{') || (iExpected == '}') ) {
			i += 2u;
		} else {
			i++;
		}
	}
	if ( (iText < Text.Size) &&
		 !__xrtPatternIsSeparator(pOptions, (uint8)Text.Data[iText]) ) {
		return false;
	}
	*pText = iText;
	return true;
}



/* 已验证模式的一次性匹配实现。 */
static xpatternresult __xrtPatternExtractValid(
	xstrview Pattern,
	xstrview Text,
	const __xrt_pattern_options* pOptions,
	xstrview* arrCapture
)
{
	size_t iPattern = 0;
	size_t iText = 0;
	size_t iCapture = 0;

	while ( iPattern < Pattern.Size ) {
		if ( __xrtPatternIsSeparator(
			pOptions,
			(uint8)Pattern.Data[iPattern]
		) ) {
			if ( (iText >= Text.Size) ||
				 (Text.Data[iText] != Pattern.Data[iPattern]) ) {
				return XPATTERN_NONE;
			}
			iPattern++;
			iText++;
			continue;
		}
		{
			size_t iStart = iPattern;
			size_t iEnd;
			__xrt_pattern_field Field;

			while ( (iPattern < Pattern.Size) &&
				!__xrtPatternIsSeparator(
					pOptions,
					(uint8)Pattern.Data[iPattern]
				) ) {
				iPattern++;
			}
			iEnd = iPattern;
			/* 第一次验证已经成功，这里分类不应再失败。 */
			if ( !__xrtPatternField(
				Pattern,
				iStart,
				iEnd,
				iEnd == Pattern.Size,
				false,
				0,
				"extract",
				&Field
			) ) {
				return XPATTERN_ERROR;
			}
			if ( Field.Kind == __XRT_PATTERN_ATOM_LITERAL ) {
				if ( !__xrtPatternLiteralMatch(
					Pattern,
					iStart,
					iEnd,
					Text,
					pOptions,
					&iText
				) ) {
					return XPATTERN_NONE;
				}
			} else if ( Field.Kind == __XRT_PATTERN_ATOM_CAPTURE ) {
				size_t iValue = iText;

				while ( (iText < Text.Size) &&
					!__xrtPatternIsSeparator(
						pOptions,
						(uint8)Text.Data[iText]
					) ) {
					iText++;
				}
				if ( iText == iValue ) {
					return XPATTERN_NONE;
				}
				arrCapture[iCapture++] = (xstrview){
					Text.Data + iValue,
					iText - iValue
				};
			} else if ( Field.Kind == __XRT_PATTERN_ATOM_AFFIX ) {
				size_t iFieldStart = iText;
				size_t iFieldEnd;
				size_t iFixed = Field.PrefixSize + Field.SuffixSize;

				while ( (iText < Text.Size) &&
					!__xrtPatternIsSeparator(
						pOptions,
						(uint8)Text.Data[iText]
					) ) {
					iText++;
				}
				iFieldEnd = iText;
				if ( ((iFieldEnd - iFieldStart) <= iFixed) ||
					 !__xrtPatternLiteralEqual(
						Pattern,
						iStart,
						Field.CaptureOpen,
						Text.Data + iFieldStart,
						Field.PrefixSize
					 ) || !__xrtPatternLiteralEqual(
						Pattern,
						Field.CaptureEnd,
						iEnd,
						Text.Data + iFieldEnd - Field.SuffixSize,
						Field.SuffixSize
					 ) ) {
					return XPATTERN_NONE;
				}
				arrCapture[iCapture++] = (xstrview){
					Text.Data + iFieldStart + Field.PrefixSize,
					(iFieldEnd - iFieldStart) - iFixed
				};
			} else {
				arrCapture[iCapture++] = (xstrview){
					Text.Data != NULL ? Text.Data + iText : NULL,
					Text.Size - iText
				};
				iText = Text.Size;
			}
		}
	}
	return iText == Text.Size ? XPATTERN_MATCH : XPATTERN_NONE;
}



/* 使用高级配置执行零分配单条匹配。 */
XRT_API xpatternresult xrtPatternExtractConfig(
	xstrview Pattern,
	xstrview Text,
	const xpatternconfig* pConfig,
	xstrview* arrCapture,
	size_t iCapacity,
	size_t* pCaptureCount
)
{
	__xrt_pattern_options Options;
	__xrt_pattern_measure Measure;

	if ( pCaptureCount != NULL ) {
		*pCaptureCount = 0;
	}
	if ( (pCaptureCount == NULL) ||
		 ((arrCapture == NULL) && (iCapacity != 0)) ) {
		__xrtPatternSetInvalidArgument();
		return XPATTERN_ERROR;
	}
	if ( !__xrtPatternViewValid(Pattern) ||
		 !__xrtPatternViewValid(Text) ||
		 !__xrtPatternOptionsInit(pConfig, &Options, "extract") ) {
		return XPATTERN_ERROR;
	}
	if ( Pattern.Size > Options.MaxPatternBytes ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			"extract",
			"pattern exceeds its byte limit",
			false,
			0,
			false,
			0
		);
		return XPATTERN_ERROR;
	}
	if ( !__xrtPatternParse(
		Pattern,
		&Options,
		false,
		0,
		"extract",
		&Measure,
		NULL,
		NULL,
		NULL
	) ) {
		return XPATTERN_ERROR;
	}
	*pCaptureCount = Measure.CaptureCount;
	if ( (Measure.CaptureCount > iCapacity) ||
		 ((Measure.CaptureCount != 0) && (arrCapture == NULL)) ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_CAPACITY,
			"extract",
			"capture output capacity is insufficient",
			false,
			0,
			false,
			0
		);
		return XPATTERN_ERROR;
	}
	return __xrtPatternExtractValid(Pattern, Text, &Options, arrCapture);
}



/* 默认入口直接使用默认配置，保持与编译 API 相同语义。 */
XRT_API xpatternresult xrtPatternExtract(
	xstrview Pattern,
	xstrview Text,
	xstrview* arrCapture,
	size_t iCapacity,
	size_t* pCaptureCount
)
{
	xpatternconfig Config;

	xrtPatternConfigInit(&Config);
	return xrtPatternExtractConfig(
		Pattern,
		Text,
		&Config,
		arrCapture,
		iCapacity,
		pCaptureCount
	);
}



/* 批量解析源模式并进入共享编译器。 */
XRT_API xpattern* xrtPatternCompileManyConfig(
	const xpatternspec* arrSpec,
	size_t iCount,
	const xpatternconfig* pConfig
)
{
	__xrt_pattern_options Options;
	__xrt_pattern_source** arrSource = NULL;
	xpattern* pPattern = NULL;
	size_t iParsed = 0;

	if ( !__xrtPatternOptionsInit(pConfig, &Options, "compile") ) {
		return NULL;
	}
	if ( (arrSpec == NULL) && (iCount != 0) ) {
		__xrtPatternSetInvalidArgument();
		return NULL;
	}
	if ( iCount > Options.MaxPatterns ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			"compile",
			"pattern set exceeds its pattern limit",
			false,
			0,
			false,
			0
		);
		return NULL;
	}
	if ( iCount > (SIZE_MAX / sizeof(*arrSource)) ) {
		__xrtPatternSetSizeOverflow();
		return NULL;
	}
	if ( iCount != 0 ) {
		arrSource = (__xrt_pattern_source**)xrtCalloc(
			iCount,
			sizeof(*arrSource)
		);
		if ( arrSource == NULL ) {
			return NULL;
		}
	}
	for ( ; iParsed < iCount; iParsed++ ) {
		xpatternid Id =
			(((uint64)1u) << 32u) | ((uint64)((uint32)iParsed + 1u));

		arrSource[iParsed] = __xrtPatternSourceCreate(
			&arrSpec[iParsed],
			&Options,
			"compile",
			Id,
			(uint64)iParsed,
			true,
			iParsed
		);
		if ( arrSource[iParsed] == NULL ) {
			break;
		}
	}
	if ( iParsed == iCount ) {
		pPattern = __xrtPatternCompileSources(
			arrSource,
			iCount,
			&Options,
			"compile"
		);
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtPatternSourceFree(arrSource != NULL ? arrSource[i] : NULL);
	}
	xrtFree(arrSource);
	return pPattern;
}



XRT_API xpattern* xrtPatternCompileMany(
	const xpatternspec* arrSpec,
	size_t iCount
)
{
	xpatternconfig Config;

	xrtPatternConfigInit(&Config);
	return xrtPatternCompileManyConfig(arrSpec, iCount, &Config);
}



XRT_API xpattern* xrtPatternCompileConfig(
	xstrview Pattern,
	const xpatternconfig* pConfig
)
{
	xpatternspec Spec;

	memset(&Spec, 0, sizeof(Spec));
	Spec.Pattern = Pattern;
	return xrtPatternCompileManyConfig(&Spec, 1u, pConfig);
}



XRT_API xpattern* xrtPatternCompile(xstrview Pattern)
{
	xpatternconfig Config;

	xrtPatternConfigInit(&Config);
	return xrtPatternCompileConfig(Pattern, &Config);
}



XRT_API xpattern* xrtPatternRef(xpattern* pPattern)
{
	if ( pPattern == NULL ) {
		__xrtPatternSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pPattern->RefCount) < 0 ) {
		__xrtPatternSetInvalidState();
		return NULL;
	}
	return pPattern;
}



XRT_API void xrtPatternRelease(xpattern* pPattern)
{
	if ( (pPattern != NULL) && (xrtRefRelease(&pPattern->RefCount) == 0) ) {
		xrtFree(pPattern);
	}
}



XRT_API size_t xrtPatternCount(const xpattern* pPattern)
{
	if ( pPattern == NULL ) {
		__xrtPatternSetInvalidArgument();
		return 0;
	}
	return pPattern->Count;
}



XRT_API size_t xrtPatternCompiledBytes(const xpattern* pPattern)
{
	if ( pPattern == NULL ) {
		__xrtPatternSetInvalidArgument();
		return 0;
	}
	return pPattern->CompiledBytes;
}



XRT_API xstrview xrtPatternSeparators(const xpattern* pPattern)
{
	if ( pPattern == NULL ) {
		__xrtPatternSetInvalidArgument();
		return (xstrview){ 0 };
	}
	return pPattern->Separators;
}



/* 验证编译对象及模式索引并返回内部条目。 */
static const __xrt_pattern_entry* __xrtPatternEntry(
	const xpattern* pPattern,
	size_t iPattern
)
{
	if ( pPattern == NULL ) {
		__xrtPatternSetInvalidArgument();
		return NULL;
	}
	if ( iPattern >= pPattern->Count ) {
		__xrtPatternSetRange();
		return NULL;
	}
	return &pPattern->Entries[iPattern];
}



XRT_API xstrview xrtPatternSource(
	const xpattern* pPattern,
	size_t iPattern
)
{
	const __xrt_pattern_entry* pEntry = __xrtPatternEntry(pPattern, iPattern);

	return pEntry != NULL ? pPattern->Metadata[iPattern].Source :
		(xstrview){ 0 };
}



XRT_API xpatternid xrtPatternId(
	const xpattern* pPattern,
	size_t iPattern
)
{
	const __xrt_pattern_entry* pEntry = __xrtPatternEntry(pPattern, iPattern);

	return pEntry != NULL ? pEntry->Id : XPATTERN_ID_INVALID;
}



XRT_API ptr xrtPatternValue(
	const xpattern* pPattern,
	size_t iPattern
)
{
	const __xrt_pattern_entry* pEntry = __xrtPatternEntry(pPattern, iPattern);

	return pEntry != NULL ? pEntry->Value : NULL;
}



XRT_API size_t xrtPatternCaptureCount(
	const xpattern* pPattern,
	size_t iPattern
)
{
	const __xrt_pattern_entry* pEntry = __xrtPatternEntry(pPattern, iPattern);

	return pEntry != NULL ? pEntry->CaptureCount : 0;
}



XRT_API size_t xrtPatternMaxCaptureCount(const xpattern* pPattern)
{
	if ( pPattern == NULL ) {
		__xrtPatternSetInvalidArgument();
		return 0;
	}
	return pPattern->MaxCaptureCount;
}



XRT_API bool xrtPatternCaptureName(
	const xpattern* pPattern,
	size_t iPattern,
	size_t iCapture,
	xstrview* pName
)
{
	const __xrt_pattern_entry* pEntry;
	const __xrt_pattern_metadata* pMetadata;

	if ( pName == NULL ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	*pName = (xstrview){ 0 };
	pEntry = __xrtPatternEntry(pPattern, iPattern);
	if ( pEntry == NULL ) {
		return false;
	}
	if ( iCapture >= pEntry->CaptureCount ) {
		__xrtPatternSetRange();
		return false;
	}
	pMetadata = &pPattern->Metadata[iPattern];
	*pName = pMetadata->Captures[iCapture].Name;
	return true;
}



XRT_API size_t xrtPatternCaptureIndex(
	const xpattern* pPattern,
	size_t iPattern,
	xstrview Name
)
{
	const __xrt_pattern_entry* pEntry;
	const __xrt_pattern_metadata* pMetadata;

	if ( !__xrtPatternViewValid(Name) ) {
		return XRT_NPOS;
	}
	pEntry = __xrtPatternEntry(pPattern, iPattern);
	if ( pEntry == NULL ) {
		return XRT_NPOS;
	}
	pMetadata = &pPattern->Metadata[iPattern];
	for ( size_t i = 0; i < pEntry->CaptureCount; i++ ) {
		xstrview Current = pMetadata->Captures[i].Name;

		if ( (Current.Size == Name.Size) &&
			 ((Name.Size == 0) ||
			  (memcmp(Current.Data, Name.Data, Name.Size) == 0)) ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 从分号分隔错误数据中读取一个无符号字段。 */
static bool __xrtPatternErrorField(
	const xerror* pError,
	cstr sField,
	size_t* pValue
)
{
	cstr sData;
	cstr sFound;
	char* sEnd;
	unsigned long long iValue;
	size_t iFieldSize;

	if ( (sField == NULL) || (pValue == NULL) ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	if ( (pError == NULL) || (xrtErrorDomain(pError) == NULL) ||
		 (strcmp(xrtErrorDomain(pError), "xrt.pattern") != 0) ) {
		return false;
	}
	sData = xrtErrorData(pError);
	if ( sData == NULL ) {
		return false;
	}
	iFieldSize = strlen(sField);
	sFound = sData;
	for ( ;; ) {
		sFound = strstr(sFound, sField);
		if ( sFound == NULL ) {
			return false;
		}
		if ( ((sFound == sData) || (sFound[-1] == ';')) &&
			 (sFound[iFieldSize] == '=') ) {
			break;
		}
		sFound++;
	}
	sFound += iFieldSize + 1u;
	iValue = strtoull(sFound, &sEnd, 10);
	if ( (sEnd == sFound) || ((*sEnd != 0) && (*sEnd != ';')) ||
		 (iValue > (unsigned long long)SIZE_MAX) ) {
		return false;
	}
	*pValue = (size_t)iValue;
	return true;
}



XRT_API bool xrtPatternErrorOffset(
	const xerror* pError,
	size_t* pOffset
)
{
	return __xrtPatternErrorField(pError, "offset", pOffset);
}



XRT_API bool xrtPatternErrorPattern(
	const xerror* pError,
	size_t* pPatternIndex
)
{
	return __xrtPatternErrorField(pError, "pattern", pPatternIndex);
}

#endif
