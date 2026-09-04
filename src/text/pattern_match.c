#include "../internal/xrt_pattern.h"



#if defined(XRT_FEATURE_PATTERN)

#define __XRT_PATTERN_FIELDS_INLINE 16u



typedef struct __xrt_pattern_fields {
	xstrview Values[__XRT_PATTERN_FIELDS_INLINE];
	size_t Count;
	bool Overflow;
} __xrt_pattern_fields;



/* 选择 Rank 更小的候选，并用加一编码保留零值未命中。 */
static uint32 __xrtPatternCandidate(
	const xpattern* pPattern,
	uint32 iCurrentPlusOne,
	uint32 iCandidatePlusOne
)
{
	if ( iCandidatePlusOne == 0 ) {
		return iCurrentPlusOne;
	}
	if ( iCurrentPlusOne == 0 ) {
		return iCandidatePlusOne;
	}
	return pPattern->Entries[iCandidatePlusOne - 1u].Rank <
		pPattern->Entries[iCurrentPlusOne - 1u].Rank ?
		iCandidatePlusOne : iCurrentPlusOne;
}



/* 精确分隔符转移通常只有一条，小数组线性比较避免哈希开销。 */
static uint32 __xrtPatternSeparatorNext(
	const xpattern* pPattern,
	const __xrt_pattern_state* pState,
	uint8 iByte
)
{
	const __xrt_pattern_separator_transition* arrTransition =
		pPattern->SeparatorTransitions + pState->SeparatorOffset;

	if ( pState->SeparatorCount <= __XRT_PATTERN_LINEAR_MAX ) {
		for ( size_t i = 0; i < pState->SeparatorCount; i++ ) {
			if ( arrTransition[i].Byte == iByte ) {
				return arrTransition[i].NextPlusOne;
			}
		}
		return 0;
	}
	{
		size_t iLow = 0;
		size_t iHigh = pState->SeparatorCount;

		while ( iLow < iHigh ) {
			size_t iMiddle = iLow + ((iHigh - iLow) >> 1u);

			if ( arrTransition[iMiddle].Byte < iByte ) {
				iLow = iMiddle + 1u;
			} else {
				iHigh = iMiddle;
			}
		}
		return (iLow < pState->SeparatorCount) &&
			(arrTransition[iLow].Byte == iByte) ?
			arrTransition[iLow].NextPlusOne : 0;
	}
}



/* 自适应查找字面字段，未命中时返回已经确定化的参数默认状态。 */
static uint32 __xrtPatternLiteralNext(
	const xpattern* pPattern,
	uint32 iState,
	const char* sField,
	size_t iSize,
	uint32 iFallbackPlusOne
)
{
	const __xrt_pattern_state* pState = &pPattern->States[iState];
	const __xrt_pattern_literal_transition* arrTransition =
		pPattern->Literals + pState->LiteralOffset;

	if ( iSize > UINT32_MAX ) {
		return iFallbackPlusOne;
	}
	if ( pState->DispatchKind == __XRT_PATTERN_DISPATCH_LINEAR ) {
		for ( size_t i = 0; i < pState->LiteralCount; i++ ) {
			if ( (arrTransition[i].Size == (uint32)iSize) &&
				 (memcmp(arrTransition[i].Data, sField, iSize) == 0) ) {
				return arrTransition[i].NextPlusOne;
			}
		}
	} else if ( pState->DispatchKind == __XRT_PATTERN_DISPATCH_HASH ) {
		uint64 iHash = __xrtPatternHashField(sField, iSize, iState);
		size_t iSlot = (size_t)(iHash & (pState->LiteralSlots - 1u));

		for ( ;; ) {
			const __xrt_pattern_literal_transition* pTransition =
				&arrTransition[iSlot];

			if ( pTransition->NextPlusOne == 0 ) {
				break;
			}
			if ( (pTransition->Size == (uint32)iSize) &&
				 (memcmp(pTransition->Data, sField, iSize) == 0) ) {
				return pTransition->NextPlusOne;
			}
			iSlot = (iSlot + 1u) & (pState->LiteralSlots - 1u);
		}
	}
	return iFallbackPlusOne;
}



/* 局部混合字段 DFA：稀疏字节覆盖，未覆盖字节走通配默认边。 */
static uint32 __xrtPatternAffixNext(
	const xpattern* pPattern,
	uint32 iStatePlusOne,
	uint8 iByte
)
{
	const __xrt_pattern_affix_state* pState =
		&pPattern->AffixStates[iStatePlusOne - 1u];
	const __xrt_pattern_affix_transition* arrTransition =
		pPattern->AffixTransitions + pState->TransitionOffset;

	if ( pState->TransitionCount <= __XRT_PATTERN_LINEAR_MAX ) {
		for ( size_t i = 0; i < pState->TransitionCount; i++ ) {
			if ( arrTransition[i].Byte == iByte ) {
				return arrTransition[i].NextPlusOne;
			}
		}
		return pState->DefaultPlusOne;
	}
	{
		size_t iLow = 0;
		size_t iHigh = pState->TransitionCount;

		while ( iLow < iHigh ) {
			size_t iMiddle = iLow + ((iHigh - iLow) >> 1u);

			if ( arrTransition[iMiddle].Byte < iByte ) {
				iLow = iMiddle + 1u;
			} else {
				iHigh = iMiddle;
			}
		}
		return (iLow < pState->TransitionCount) &&
			(arrTransition[iLow].Byte == iByte) ?
			arrTransition[iLow].NextPlusOne : pState->DefaultPlusOne;
	}
}



/*
	每种分隔符规模生成独立扫描循环；入口只分派一次，热循环中没有模式
	开关。Condition 使用循环局部变量 iCurrent。
*/
#define __XRT_PATTERN_DEFINE_RUN(Name, Declaration, Condition) \
	static uint32 Name(const xpattern* pPattern, xstrview Text) \
	{ \
		Declaration \
		uint32 iState = 0; \
		uint32 iBestPlusOne = 0; \
		size_t iPosition = 0; \
		for ( ;; ) { \
			const __xrt_pattern_state* pState = &pPattern->States[iState]; \
			uint32 iNextPlusOne; \
			iBestPlusOne = __xrtPatternCandidate( \
				pPattern, iBestPlusOne, pState->TailPlusOne \
			); \
			if ( iPosition == Text.Size ) { \
				iBestPlusOne = __xrtPatternCandidate( \
					pPattern, iBestPlusOne, pState->TerminalPlusOne \
				); \
				break; \
			} \
			{ \
				uint8 iCurrent = (uint8)Text.Data[iPosition]; \
				if ( Condition ) { \
					iNextPlusOne = __xrtPatternSeparatorNext( \
						pPattern, pState, iCurrent \
					); \
					iPosition++; \
				} else { \
					size_t iStart = iPosition++; \
					uint32 iAffixPlusOne = pPattern->AffixRoots[iState]; \
					uint32 iFallbackPlusOne; \
					if ( iAffixPlusOne != 0 ) { \
						iAffixPlusOne = __xrtPatternAffixNext( \
							pPattern, iAffixPlusOne, iCurrent \
						); \
					} \
					while ( iPosition < Text.Size ) { \
						iCurrent = (uint8)Text.Data[iPosition]; \
						if ( Condition ) { \
							break; \
						} \
						if ( iAffixPlusOne != 0 ) { \
							iAffixPlusOne = __xrtPatternAffixNext( \
								pPattern, iAffixPlusOne, iCurrent \
							); \
						} \
						iPosition++; \
					} \
					iFallbackPlusOne = iAffixPlusOne != 0 ? \
						pPattern->AffixStates[ \
							iAffixPlusOne - 1u \
						].OutputPlusOne : pState->DefaultPlusOne; \
					iNextPlusOne = __xrtPatternLiteralNext( \
						pPattern, \
						iState, \
						Text.Data + iStart, \
						iPosition - iStart, \
						iFallbackPlusOne \
					); \
				} \
			} \
			if ( iNextPlusOne == 0 ) { \
				break; \
			} \
			iState = iNextPlusOne - 1u; \
		} \
		return iBestPlusOne; \
	}



__XRT_PATTERN_DEFINE_RUN(
	__xrtPatternRunAffixNone,
	(void)pPattern->SeparatorKind;,
	false
)



__XRT_PATTERN_DEFINE_RUN(
	__xrtPatternRunAffixSlash,
	(void)pPattern->SeparatorKind;,
	iCurrent == (uint8)'/'
)



__XRT_PATTERN_DEFINE_RUN(
	__xrtPatternRunAffixByte,
	const uint8 iSeparator = (uint8)pPattern->Separators.Data[0];,
	iCurrent == iSeparator
)



__XRT_PATTERN_DEFINE_RUN(
	__xrtPatternRunAffixTwo,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1])
)



__XRT_PATTERN_DEFINE_RUN(
	__xrtPatternRunAffixThree,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1]) ||
	(iCurrent == arrSeparator[2])
)



__XRT_PATTERN_DEFINE_RUN(
	__xrtPatternRunAffixFour,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1]) ||
	(iCurrent == arrSeparator[2]) || (iCurrent == arrSeparator[3])
)



__XRT_PATTERN_DEFINE_RUN(
	__xrtPatternRunAffixBitmap,
	const uint64* arrMask = pPattern->SeparatorMask;,
	(arrMask[iCurrent >> 6u] & (UINT64_C(1) << (iCurrent & 63u))) != 0
)



#undef __XRT_PATTERN_DEFINE_RUN



/* 不含混合字段的快照使用更短的字段热循环。 */
#define __XRT_PATTERN_DEFINE_RUN_PLAIN(Name, Declaration, Condition) \
	static uint32 Name(const xpattern* pPattern, xstrview Text) \
	{ \
		Declaration \
		uint32 iState = 0; \
		uint32 iBestPlusOne = 0; \
		size_t iPosition = 0; \
		for ( ;; ) { \
			const __xrt_pattern_state* pState = &pPattern->States[iState]; \
			uint32 iNextPlusOne; \
			iBestPlusOne = __xrtPatternCandidate( \
				pPattern, iBestPlusOne, pState->TailPlusOne \
			); \
			if ( iPosition == Text.Size ) { \
				iBestPlusOne = __xrtPatternCandidate( \
					pPattern, iBestPlusOne, pState->TerminalPlusOne \
				); \
				break; \
			} \
			{ \
				uint8 iCurrent = (uint8)Text.Data[iPosition]; \
				if ( Condition ) { \
					iNextPlusOne = __xrtPatternSeparatorNext( \
						pPattern, pState, iCurrent \
					); \
					iPosition++; \
				} else { \
					size_t iStart = iPosition++; \
					while ( iPosition < Text.Size ) { \
						iCurrent = (uint8)Text.Data[iPosition]; \
						if ( Condition ) { \
							break; \
						} \
						iPosition++; \
					} \
					iNextPlusOne = __xrtPatternLiteralNext( \
						pPattern, \
						iState, \
						Text.Data + iStart, \
						iPosition - iStart, \
						pState->DefaultPlusOne \
					); \
				} \
			} \
			if ( iNextPlusOne == 0 ) { \
				break; \
			} \
			iState = iNextPlusOne - 1u; \
		} \
		return iBestPlusOne; \
	}



__XRT_PATTERN_DEFINE_RUN_PLAIN(
	__xrtPatternRunNone,
	(void)pPattern->SeparatorKind;,
	false
)



__XRT_PATTERN_DEFINE_RUN_PLAIN(
	__xrtPatternRunSlash,
	(void)pPattern->SeparatorKind;,
	iCurrent == (uint8)'/'
)



__XRT_PATTERN_DEFINE_RUN_PLAIN(
	__xrtPatternRunByte,
	const uint8 iSeparator = (uint8)pPattern->Separators.Data[0];,
	iCurrent == iSeparator
)



__XRT_PATTERN_DEFINE_RUN_PLAIN(
	__xrtPatternRunTwo,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1])
)



__XRT_PATTERN_DEFINE_RUN_PLAIN(
	__xrtPatternRunThree,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1]) ||
	(iCurrent == arrSeparator[2])
)



__XRT_PATTERN_DEFINE_RUN_PLAIN(
	__xrtPatternRunFour,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1]) ||
	(iCurrent == arrSeparator[2]) || (iCurrent == arrSeparator[3])
)



__XRT_PATTERN_DEFINE_RUN_PLAIN(
	__xrtPatternRunBitmap,
	const uint64* arrMask = pPattern->SeparatorMask;,
	(arrMask[iCurrent >> 6u] & (UINT64_C(1) << (iCurrent & 63u))) != 0
)



#undef __XRT_PATTERN_DEFINE_RUN_PLAIN



/* Match 专用扫描器在同一次状态遍历中保留少量字段边界。 */
#define __XRT_PATTERN_DEFINE_RUN_FIELDS(Name, Declaration, Condition) \
	static uint32 Name( \
		const xpattern* pPattern, \
		xstrview Text, \
		__xrt_pattern_fields* pFields \
	) \
	{ \
		Declaration \
		uint32 iState = 0; \
		uint32 iBestPlusOne = 0; \
		size_t iPosition = 0; \
		for ( ;; ) { \
			const __xrt_pattern_state* pState = &pPattern->States[iState]; \
			uint32 iNextPlusOne; \
			iBestPlusOne = __xrtPatternCandidate( \
				pPattern, iBestPlusOne, pState->TailPlusOne \
			); \
			if ( iPosition == Text.Size ) { \
				iBestPlusOne = __xrtPatternCandidate( \
					pPattern, iBestPlusOne, pState->TerminalPlusOne \
				); \
				break; \
			} \
			{ \
				uint8 iCurrent = (uint8)Text.Data[iPosition]; \
				if ( Condition ) { \
					iNextPlusOne = __xrtPatternSeparatorNext( \
						pPattern, pState, iCurrent \
					); \
					iPosition++; \
				} else { \
					size_t iStart = iPosition++; \
					uint32 iAffixPlusOne = pPattern->AffixRoots[iState]; \
					uint32 iFallbackPlusOne; \
					if ( iAffixPlusOne != 0 ) { \
						iAffixPlusOne = __xrtPatternAffixNext( \
							pPattern, iAffixPlusOne, iCurrent \
						); \
					} \
					while ( iPosition < Text.Size ) { \
						iCurrent = (uint8)Text.Data[iPosition]; \
						if ( Condition ) { \
							break; \
						} \
						if ( iAffixPlusOne != 0 ) { \
							iAffixPlusOne = __xrtPatternAffixNext( \
								pPattern, iAffixPlusOne, iCurrent \
							); \
						} \
						iPosition++; \
					} \
					if ( pFields->Count < __XRT_PATTERN_FIELDS_INLINE ) { \
						pFields->Values[pFields->Count] = (xstrview){ \
							Text.Data + iStart, iPosition - iStart \
						}; \
					} else { \
						pFields->Overflow = true; \
					} \
					pFields->Count++; \
					iFallbackPlusOne = iAffixPlusOne != 0 ? \
						pPattern->AffixStates[ \
							iAffixPlusOne - 1u \
						].OutputPlusOne : pState->DefaultPlusOne; \
					iNextPlusOne = __xrtPatternLiteralNext( \
						pPattern, \
						iState, \
						Text.Data + iStart, \
						iPosition - iStart, \
						iFallbackPlusOne \
					); \
				} \
			} \
			if ( iNextPlusOne == 0 ) { \
				break; \
			} \
			iState = iNextPlusOne - 1u; \
		} \
		return iBestPlusOne; \
	}



__XRT_PATTERN_DEFINE_RUN_FIELDS(
	__xrtPatternRunAffixFieldsNone,
	(void)pPattern->SeparatorKind;,
	false
)



__XRT_PATTERN_DEFINE_RUN_FIELDS(
	__xrtPatternRunAffixFieldsSlash,
	(void)pPattern->SeparatorKind;,
	iCurrent == (uint8)'/'
)



__XRT_PATTERN_DEFINE_RUN_FIELDS(
	__xrtPatternRunAffixFieldsByte,
	const uint8 iSeparator = (uint8)pPattern->Separators.Data[0];,
	iCurrent == iSeparator
)



__XRT_PATTERN_DEFINE_RUN_FIELDS(
	__xrtPatternRunAffixFieldsTwo,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1])
)



__XRT_PATTERN_DEFINE_RUN_FIELDS(
	__xrtPatternRunAffixFieldsThree,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1]) ||
	(iCurrent == arrSeparator[2])
)



__XRT_PATTERN_DEFINE_RUN_FIELDS(
	__xrtPatternRunAffixFieldsFour,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1]) ||
	(iCurrent == arrSeparator[2]) || (iCurrent == arrSeparator[3])
)



__XRT_PATTERN_DEFINE_RUN_FIELDS(
	__xrtPatternRunAffixFieldsBitmap,
	const uint64* arrMask = pPattern->SeparatorMask;,
	(arrMask[iCurrent >> 6u] & (UINT64_C(1) << (iCurrent & 63u))) != 0
)



#undef __XRT_PATTERN_DEFINE_RUN_FIELDS



/* 不含混合字段时，记录边界也不引入局部 DFA 状态检查。 */
#define __XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS(Name, Declaration, Condition) \
	static uint32 Name( \
		const xpattern* pPattern, \
		xstrview Text, \
		__xrt_pattern_fields* pFields \
	) \
	{ \
		Declaration \
		uint32 iState = 0; \
		uint32 iBestPlusOne = 0; \
		size_t iPosition = 0; \
		for ( ;; ) { \
			const __xrt_pattern_state* pState = &pPattern->States[iState]; \
			uint32 iNextPlusOne; \
			iBestPlusOne = __xrtPatternCandidate( \
				pPattern, iBestPlusOne, pState->TailPlusOne \
			); \
			if ( iPosition == Text.Size ) { \
				iBestPlusOne = __xrtPatternCandidate( \
					pPattern, iBestPlusOne, pState->TerminalPlusOne \
				); \
				break; \
			} \
			{ \
				uint8 iCurrent = (uint8)Text.Data[iPosition]; \
				if ( Condition ) { \
					iNextPlusOne = __xrtPatternSeparatorNext( \
						pPattern, pState, iCurrent \
					); \
					iPosition++; \
				} else { \
					size_t iStart = iPosition++; \
					while ( iPosition < Text.Size ) { \
						iCurrent = (uint8)Text.Data[iPosition]; \
						if ( Condition ) { \
							break; \
						} \
						iPosition++; \
					} \
					if ( pFields->Count < __XRT_PATTERN_FIELDS_INLINE ) { \
						pFields->Values[pFields->Count] = (xstrview){ \
							Text.Data + iStart, iPosition - iStart \
						}; \
					} else { \
						pFields->Overflow = true; \
					} \
					pFields->Count++; \
					iNextPlusOne = __xrtPatternLiteralNext( \
						pPattern, \
						iState, \
						Text.Data + iStart, \
						iPosition - iStart, \
						pState->DefaultPlusOne \
					); \
				} \
			} \
			if ( iNextPlusOne == 0 ) { \
				break; \
			} \
			iState = iNextPlusOne - 1u; \
		} \
		return iBestPlusOne; \
	}



__XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS(
	__xrtPatternRunFieldsNone,
	(void)pPattern->SeparatorKind;,
	false
)



__XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS(
	__xrtPatternRunFieldsSlash,
	(void)pPattern->SeparatorKind;,
	iCurrent == (uint8)'/'
)



__XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS(
	__xrtPatternRunFieldsByte,
	const uint8 iSeparator = (uint8)pPattern->Separators.Data[0];,
	iCurrent == iSeparator
)



__XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS(
	__xrtPatternRunFieldsTwo,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1])
)



__XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS(
	__xrtPatternRunFieldsThree,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1]) ||
	(iCurrent == arrSeparator[2])
)



__XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS(
	__xrtPatternRunFieldsFour,
	const uint8* arrSeparator = (const uint8*)pPattern->Separators.Data;,
	(iCurrent == arrSeparator[0]) || (iCurrent == arrSeparator[1]) ||
	(iCurrent == arrSeparator[2]) || (iCurrent == arrSeparator[3])
)



__XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS(
	__xrtPatternRunFieldsBitmap,
	const uint64* arrMask = pPattern->SeparatorMask;,
	(arrMask[iCurrent >> 6u] & (UINT64_C(1) << (iCurrent & 63u))) != 0
)



#undef __XRT_PATTERN_DEFINE_RUN_PLAIN_FIELDS



uint32 __xrtPatternSelect(const xpattern* pPattern, xstrview Text)
{
	if ( pPattern->StateCount == 0 ) {
		return 0;
	}
	if ( pPattern->AffixRoots != NULL ) {
		switch ( pPattern->SeparatorKind ) {
			case __XRT_PATTERN_SEPARATOR_NONE:
				return __xrtPatternRunAffixNone(pPattern, Text);
			case __XRT_PATTERN_SEPARATOR_SLASH:
				return __xrtPatternRunAffixSlash(pPattern, Text);
			case __XRT_PATTERN_SEPARATOR_BYTE:
				return __xrtPatternRunAffixByte(pPattern, Text);
			case __XRT_PATTERN_SEPARATOR_TWO:
				return __xrtPatternRunAffixTwo(pPattern, Text);
			case __XRT_PATTERN_SEPARATOR_THREE:
				return __xrtPatternRunAffixThree(pPattern, Text);
			case __XRT_PATTERN_SEPARATOR_FOUR:
				return __xrtPatternRunAffixFour(pPattern, Text);
			case __XRT_PATTERN_SEPARATOR_BITMAP:
				return __xrtPatternRunAffixBitmap(pPattern, Text);
			default:
				return 0;
		}
	}
	switch ( pPattern->SeparatorKind ) {
		case __XRT_PATTERN_SEPARATOR_NONE:
			return __xrtPatternRunNone(pPattern, Text);
		case __XRT_PATTERN_SEPARATOR_SLASH:
			return __xrtPatternRunSlash(pPattern, Text);
		case __XRT_PATTERN_SEPARATOR_BYTE:
			return __xrtPatternRunByte(pPattern, Text);
		case __XRT_PATTERN_SEPARATOR_TWO:
			return __xrtPatternRunTwo(pPattern, Text);
		case __XRT_PATTERN_SEPARATOR_THREE:
			return __xrtPatternRunThree(pPattern, Text);
		case __XRT_PATTERN_SEPARATOR_FOUR:
			return __xrtPatternRunFour(pPattern, Text);
		case __XRT_PATTERN_SEPARATOR_BITMAP:
			return __xrtPatternRunBitmap(pPattern, Text);
		default:
			return 0;
	}
}



static uint32 __xrtPatternSelectFields(
	const xpattern* pPattern,
	xstrview Text,
	__xrt_pattern_fields* pFields
)
{
	if ( pPattern->StateCount == 0 ) {
		return 0;
	}
	if ( pPattern->AffixRoots != NULL ) {
		switch ( pPattern->SeparatorKind ) {
			case __XRT_PATTERN_SEPARATOR_NONE:
				return __xrtPatternRunAffixFieldsNone(
					pPattern, Text, pFields
				);
			case __XRT_PATTERN_SEPARATOR_SLASH:
				return __xrtPatternRunAffixFieldsSlash(
					pPattern, Text, pFields
				);
			case __XRT_PATTERN_SEPARATOR_BYTE:
				return __xrtPatternRunAffixFieldsByte(
					pPattern, Text, pFields
				);
			case __XRT_PATTERN_SEPARATOR_TWO:
				return __xrtPatternRunAffixFieldsTwo(
					pPattern, Text, pFields
				);
			case __XRT_PATTERN_SEPARATOR_THREE:
				return __xrtPatternRunAffixFieldsThree(
					pPattern, Text, pFields
				);
			case __XRT_PATTERN_SEPARATOR_FOUR:
				return __xrtPatternRunAffixFieldsFour(
					pPattern, Text, pFields
				);
			case __XRT_PATTERN_SEPARATOR_BITMAP:
				return __xrtPatternRunAffixFieldsBitmap(
					pPattern, Text, pFields
				);
			default:
				return 0;
		}
	}
	switch ( pPattern->SeparatorKind ) {
		case __XRT_PATTERN_SEPARATOR_NONE:
			return __xrtPatternRunFieldsNone(pPattern, Text, pFields);
		case __XRT_PATTERN_SEPARATOR_SLASH:
			return __xrtPatternRunFieldsSlash(pPattern, Text, pFields);
		case __XRT_PATTERN_SEPARATOR_BYTE:
			return __xrtPatternRunFieldsByte(pPattern, Text, pFields);
		case __XRT_PATTERN_SEPARATOR_TWO:
			return __xrtPatternRunFieldsTwo(pPattern, Text, pFields);
		case __XRT_PATTERN_SEPARATOR_THREE:
			return __xrtPatternRunFieldsThree(pPattern, Text, pFields);
		case __XRT_PATTERN_SEPARATOR_FOUR:
			return __xrtPatternRunFieldsFour(pPattern, Text, pFields);
		case __XRT_PATTERN_SEPARATOR_BITMAP:
			return __xrtPatternRunFieldsBitmap(pPattern, Text, pFields);
		default:
			return 0;
	}
}



/* 捕获重放使用位图分类；它只在调用方确实请求捕获时运行。 */
static bool __xrtPatternCompiledSeparator(
	const xpattern* pPattern,
	uint8 iByte
)
{
	return (pPattern->SeparatorMask[iByte >> 6u] &
		(UINT64_C(1) << (iByte & 63u))) != 0;
}



bool __xrtPatternCaptureReplay(
	const xpattern* pPattern,
	const __xrt_pattern_entry* pEntry,
	const __xrt_pattern_metadata* pMetadata,
	xstrview Text,
	xstrview* arrCapture
)
{
	size_t iPosition = 0;
	size_t iCapture = 0;

	for ( size_t i = 0; i < pMetadata->ReplayCount; i++ ) {
		uint8 iKind = pMetadata->Replay[i];

		if ( iKind == __XRT_PATTERN_ATOM_SEPARATOR ) {
			if ( iPosition >= Text.Size ) {
				return false;
			}
			iPosition++;
		} else if ( iKind == __XRT_PATTERN_ATOM_LITERAL ) {
			while ( (iPosition < Text.Size) &&
				 !__xrtPatternCompiledSeparator(
					pPattern,
					(uint8)Text.Data[iPosition]
				 ) ) {
				iPosition++;
			}
		} else if ( iKind == __XRT_PATTERN_ATOM_CAPTURE ) {
			size_t iStart = iPosition;

			while ( (iPosition < Text.Size) &&
				 !__xrtPatternCompiledSeparator(
					pPattern,
					(uint8)Text.Data[iPosition]
				 ) ) {
				iPosition++;
			}
			if ( iPosition == iStart ) {
				return false;
			}
			arrCapture[iCapture++] = (xstrview){
				Text.Data + iStart,
				iPosition - iStart
			};
		} else if ( iKind == __XRT_PATTERN_ATOM_AFFIX ) {
			const __xrt_pattern_capture* pCapture =
				&pMetadata->Captures[iCapture];
			size_t iStart = iPosition;
			size_t iFixed =
				(size_t)pCapture->PrefixSize + pCapture->SuffixSize;

			while ( (iPosition < Text.Size) &&
				 !__xrtPatternCompiledSeparator(
					pPattern,
					(uint8)Text.Data[iPosition]
				 ) ) {
				iPosition++;
			}
			if ( (iPosition - iStart) <= iFixed ) {
				return false;
			}
			arrCapture[iCapture++] = (xstrview){
				Text.Data + iStart + pCapture->PrefixSize,
				(iPosition - iStart) - iFixed
			};
		} else if ( iKind == __XRT_PATTERN_ATOM_TAIL ) {
			arrCapture[iCapture++] = (xstrview){
				Text.Data != NULL ? Text.Data + iPosition : NULL,
				Text.Size - iPosition
			};
			iPosition = Text.Size;
		} else {
			return false;
		}
	}
	return (iPosition == Text.Size) && (iCapture == pEntry->CaptureCount);
}



/* 常见短路径直接按选择阶段记录的字段视图填充，不再重扫输入。 */
static bool __xrtPatternCaptureFields(
	const __xrt_pattern_entry* pEntry,
	const __xrt_pattern_metadata* pMetadata,
	const __xrt_pattern_fields* pFields,
	xstrview* arrCapture
)
{
	for ( size_t i = 0; i < pEntry->CaptureCount; i++ ) {
		const __xrt_pattern_capture* pCapture = &pMetadata->Captures[i];
		xstrview Field;

		if ( (pCapture->Kind == __XRT_PATTERN_ATOM_TAIL) ||
			 (pCapture->FieldIndex >= pFields->Count) ||
			 (pCapture->FieldIndex >= __XRT_PATTERN_FIELDS_INLINE) ) {
			return false;
		}
		Field = pFields->Values[pCapture->FieldIndex];
		if ( pCapture->Kind == __XRT_PATTERN_ATOM_CAPTURE ) {
			arrCapture[i] = Field;
		} else if ( pCapture->Kind == __XRT_PATTERN_ATOM_AFFIX ) {
			size_t iFixed =
				(size_t)pCapture->PrefixSize + pCapture->SuffixSize;

			if ( Field.Size <= iFixed ) {
				return false;
			}
			arrCapture[i] = (xstrview){
				Field.Data + pCapture->PrefixSize,
				Field.Size - iFixed
			};
		} else {
			return false;
		}
	}
	return true;
}



static void __xrtPatternMatchClear(xpatternmatch* pMatch)
{
	pMatch->Id = XPATTERN_ID_INVALID;
	pMatch->PatternIndex = XRT_NPOS;
	pMatch->Value = NULL;
	pMatch->CaptureCount = 0;
}



/* 填充不涉及捕获的公共命中元数据。 */
static void __xrtPatternMatchSet(
	const xpattern* pPattern,
	uint32 iEntryPlusOne,
	xpatternmatch* pMatch
)
{
	const __xrt_pattern_entry* pEntry =
		&pPattern->Entries[iEntryPlusOne - 1u];

	pMatch->Id = pEntry->Id;
	pMatch->PatternIndex = iEntryPlusOne - 1u;
	pMatch->Value = pEntry->Value;
	pMatch->CaptureCount = pEntry->CaptureCount;
}



XRT_API xpatternresult xrtPatternLookup(
	const xpattern* pPattern,
	xstrview Text,
	xpatternmatch* pMatch
)
{
	uint32 iEntryPlusOne;

	if ( pMatch != NULL ) {
		__xrtPatternMatchClear(pMatch);
	}
	if ( (pPattern == NULL) || (pMatch == NULL) ||
		 ((Text.Data == NULL) && (Text.Size != 0)) ) {
		__xrtPatternSetInvalidArgument();
		return XPATTERN_ERROR;
	}
	iEntryPlusOne = __xrtPatternSelect(pPattern, Text);
	if ( iEntryPlusOne == 0 ) {
		return XPATTERN_NONE;
	}
	__xrtPatternMatchSet(pPattern, iEntryPlusOne, pMatch);
	return XPATTERN_MATCH;
}



XRT_API xpatternresult xrtPatternMatch(
	const xpattern* pPattern,
	xstrview Text,
	xstrview* arrCapture,
	size_t iCapacity,
	xpatternmatch* pMatch
)
{
	uint32 iEntryPlusOne;
	const __xrt_pattern_entry* pEntry;
	const __xrt_pattern_metadata* pMetadata;
	__xrt_pattern_fields Fields;

	if ( pMatch != NULL ) {
		__xrtPatternMatchClear(pMatch);
	}
	if ( (pPattern == NULL) || (pMatch == NULL) ||
		 ((Text.Data == NULL) && (Text.Size != 0)) ||
		 ((arrCapture == NULL) && (iCapacity != 0)) ) {
		__xrtPatternSetInvalidArgument();
		return XPATTERN_ERROR;
	}
	memset(&Fields, 0, sizeof(Fields));
	iEntryPlusOne = pPattern->MaxCaptureCount != 0 ?
		__xrtPatternSelectFields(pPattern, Text, &Fields) :
		__xrtPatternSelect(pPattern, Text);
	if ( iEntryPlusOne == 0 ) {
		return XPATTERN_NONE;
	}
	__xrtPatternMatchSet(pPattern, iEntryPlusOne, pMatch);
	pEntry = &pPattern->Entries[iEntryPlusOne - 1u];
	pMetadata = &pPattern->Metadata[iEntryPlusOne - 1u];
	if ( (pEntry->CaptureCount > iCapacity) ||
		 ((pEntry->CaptureCount != 0) && (arrCapture == NULL)) ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_CAPACITY,
			"match",
			"capture output capacity is insufficient",
			true,
			pMatch->PatternIndex,
			false,
			0
		);
		return XPATTERN_ERROR;
	}
	if ( pEntry->CaptureCount == 0 ) {
		return XPATTERN_MATCH;
	}
	if ( !Fields.Overflow &&
		 __xrtPatternCaptureFields(pEntry, pMetadata, &Fields, arrCapture) ) {
		return XPATTERN_MATCH;
	}
	if ( !__xrtPatternCaptureReplay(
		pPattern,
		pEntry,
		pMetadata,
		Text,
		arrCapture
	) ) {
		__xrtPatternSetInternal();
		return XPATTERN_ERROR;
	}
	return XPATTERN_MATCH;
}



XRT_API xpatternresult xrtPatternTest(
	const xpattern* pPattern,
	xstrview Text
)
{
	if ( (pPattern == NULL) ||
		 ((Text.Data == NULL) && (Text.Size != 0)) ) {
		__xrtPatternSetInvalidArgument();
		return XPATTERN_ERROR;
	}
	return __xrtPatternSelect(pPattern, Text) != 0 ?
		XPATTERN_MATCH : XPATTERN_NONE;
}

#endif
