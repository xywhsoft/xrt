#include "../internal/xrt_pattern.h"



#if defined(XRT_FEATURE_PATTERN)

typedef enum __xrt_pattern_nfa_edge_kind {
	__XRT_PATTERN_NFA_SEPARATOR = 1,
	__XRT_PATTERN_NFA_LITERAL = 2,
	__XRT_PATTERN_NFA_AFFIX = 3
} __xrt_pattern_nfa_edge_kind;



typedef struct __xrt_pattern_nfa_edge {
	xstrview Text;
	xstrview Suffix;
	uint32 Next;
	uint8 Kind;
	uint8 Byte;
	uint16 Reserved;
} __xrt_pattern_nfa_edge;



typedef struct __xrt_pattern_nfa_node {
	__xrt_pattern_nfa_edge* Edges;
	size_t EdgeCount;
	size_t EdgeCapacity;
	uint32* EdgeSlots;
	size_t EdgeSlotCapacity;
	uint32 Param;
	uint32 Terminal;
	uint32 Tail;
} __xrt_pattern_nfa_node;



typedef struct __xrt_pattern_nfa {
	__xrt_pattern_nfa_node* Nodes;
	size_t Count;
	size_t Capacity;
} __xrt_pattern_nfa;



typedef struct __xrt_pattern_temp_literal {
	xstrview Text;
	uint32 Next;
} __xrt_pattern_temp_literal;



typedef struct __xrt_pattern_temp_separator {
	uint32 Next;
	uint8 Byte;
	uint8 Reserved[3];
} __xrt_pattern_temp_separator;



typedef struct __xrt_pattern_affix_nfa_edge {
	uint32 Next;
	uint8 Byte;
	uint8 Reserved[3];
} __xrt_pattern_affix_nfa_edge;



typedef struct __xrt_pattern_affix_nfa_node {
	__xrt_pattern_affix_nfa_edge* Edges;
	size_t EdgeCount;
	size_t EdgeCapacity;
	uint32* Accepts;
	size_t AcceptCount;
	size_t AcceptCapacity;
	uint32 Wild;
} __xrt_pattern_affix_nfa_node;



typedef struct __xrt_pattern_affix_nfa {
	__xrt_pattern_affix_nfa_node* Nodes;
	size_t Count;
	size_t Capacity;
} __xrt_pattern_affix_nfa;



typedef struct __xrt_pattern_temp_affix_transition {
	uint32 Next;
	uint8 Byte;
	uint8 Reserved[3];
} __xrt_pattern_temp_affix_transition;



typedef struct __xrt_pattern_temp_affix_state {
	uint32* Nodes;
	size_t NodeCount;
	uint64 SetHash;
	__xrt_pattern_temp_affix_transition* Transitions;
	size_t TransitionCount;
	size_t TransitionCapacity;
	uint32 Default;
	uint32 Output;
} __xrt_pattern_temp_affix_state;



typedef struct __xrt_pattern_temp_state {
	uint32* Nodes;
	size_t NodeCount;
	uint64 SetHash;
	__xrt_pattern_temp_literal* Literals;
	size_t LiteralCount;
	size_t LiteralCapacity;
	__xrt_pattern_temp_separator* Separators;
	size_t SeparatorCount;
	size_t SeparatorCapacity;
	__xrt_pattern_temp_affix_state* AffixStates;
	size_t AffixStateCount;
	uint32 Default;
	uint32 Terminal;
	uint32 Tail;
} __xrt_pattern_temp_state;



typedef struct __xrt_pattern_state_slot {
	uint64 Hash;
	uint32 StatePlusOne;
	uint32 Reserved;
} __xrt_pattern_state_slot;



typedef struct __xrt_pattern_affix_dfa {
	__xrt_pattern_temp_affix_state* States;
	size_t Count;
	size_t Capacity;
	__xrt_pattern_state_slot* Slots;
	size_t SlotCount;
	size_t SlotCapacity;
} __xrt_pattern_affix_dfa;



typedef struct __xrt_pattern_dfa {
	__xrt_pattern_temp_state* States;
	size_t Count;
	size_t Capacity;
	__xrt_pattern_state_slot* Slots;
	size_t SlotCount;
	size_t SlotCapacity;
} __xrt_pattern_dfa;



typedef struct __xrt_pattern_u32_vector {
	uint32* Data;
	size_t Count;
	size_t Capacity;
} __xrt_pattern_u32_vector;



typedef struct __xrt_pattern_key_ref {
	const __xrt_pattern_nfa_edge* Edge;
} __xrt_pattern_key_ref;



typedef struct __xrt_pattern_rank_item {
	const __xrt_pattern_source* Source;
	uint32 Index;
} __xrt_pattern_rank_item;



typedef struct __xrt_pattern_compiler {
	__xrt_pattern_source* const* Sources;
	size_t SourceCount;
	const __xrt_pattern_options* Options;
	cstr Operation;
	uint32* Ranks;
	size_t AffixStateCount;
	__xrt_pattern_nfa Nfa;
	__xrt_pattern_dfa Dfa;
} __xrt_pattern_compiler;



/* 通用冷路径动态数组扩容。 */
static bool __xrtPatternGrow(
	ptr* pData,
	size_t* pCapacity,
	size_t iNeed,
	size_t iItemSize
)
{
	size_t iCapacity;
	ptr pNew;

	if ( iNeed <= *pCapacity ) {
		return true;
	}
	iCapacity = *pCapacity != 0 ? *pCapacity : 8u;
	while ( iCapacity < iNeed ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			iCapacity = iNeed;
			break;
		}
		iCapacity *= 2u;
	}
	if ( (iItemSize != 0) && (iCapacity > (SIZE_MAX / iItemSize)) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	pNew = xrtRealloc(*pData, iCapacity * iItemSize);
	if ( pNew == NULL ) {
		return false;
	}
	*pData = pNew;
	*pCapacity = iCapacity;
	return true;
}



/* 释放逻辑 trie 的全部冷路径边数组。 */
static void __xrtPatternNfaFree(__xrt_pattern_nfa* pNfa)
{
	for ( size_t i = 0; i < pNfa->Count; i++ ) {
		xrtFree(pNfa->Nodes[i].Edges);
		xrtFree(pNfa->Nodes[i].EdgeSlots);
	}
	xrtFree(pNfa->Nodes);
	memset(pNfa, 0, sizeof(*pNfa));
}



/* 编译期边哈希避免高扇出节点逐次追加时退化为 O(N^2)。 */
static uint64 __xrtPatternNfaEdgeHash(
	uint8 iKind,
	uint8 iByte,
	xstrview Text,
	xstrview Suffix
)
{
	if ( iKind == __XRT_PATTERN_NFA_SEPARATOR ) {
		return __xrtPatternHashMix(
			UINT64_C(0x6a09e667f3bcc909) ^ (uint64)iByte
		);
	}
	{
		uint64 iHash = __xrtPatternHashField(
		Text.Data,
		Text.Size,
		UINT32_C(0x7f4a7c15) ^ (uint32)iKind
		);

		if ( iKind == __XRT_PATTERN_NFA_AFFIX ) {
			iHash ^= __xrtPatternHashMix(__xrtPatternHashField(
				Suffix.Data,
				Suffix.Size,
				UINT32_C(0x85ebca6b)
			));
		}
		return __xrtPatternHashMix(iHash);
	}
}



static bool __xrtPatternNfaEdgeEqual(
	const __xrt_pattern_nfa_edge* pEdge,
	uint8 iKind,
	uint8 iByte,
	xstrview Text,
	xstrview Suffix
)
{
	if ( pEdge->Kind != iKind ) {
		return false;
	}
	if ( iKind == __XRT_PATTERN_NFA_SEPARATOR ) {
		return pEdge->Byte == iByte;
	}
	return (pEdge->Text.Size == Text.Size) &&
		((Text.Size == 0) ||
		 (memcmp(pEdge->Text.Data, Text.Data, Text.Size) == 0)) &&
		(iKind != __XRT_PATTERN_NFA_AFFIX ||
		 ((pEdge->Suffix.Size == Suffix.Size) &&
		  ((Suffix.Size == 0) ||
		   (memcmp(pEdge->Suffix.Data, Suffix.Data, Suffix.Size) == 0))));
}



/* 小扇出保持线性数组；第九条边开始建立低负载临时索引。 */
static bool __xrtPatternNfaEdgeSlots(
	__xrt_pattern_nfa_node* pNode,
	size_t iNeed
)
{
	uint32* arrSlot;
	size_t iCapacity;

	if ( (pNode->EdgeSlotCapacity == 0) && (iNeed <= 8u) ) {
		return true;
	}
	iCapacity = pNode->EdgeSlotCapacity != 0 ?
		pNode->EdgeSlotCapacity : 16u;
	while ( iNeed > (iCapacity / 2u) ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			__xrtPatternSetSizeOverflow();
			return false;
		}
		iCapacity *= 2u;
	}
	if ( iCapacity == pNode->EdgeSlotCapacity ) {
		return true;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(*arrSlot)) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	arrSlot = (uint32*)xrtCalloc(iCapacity, sizeof(*arrSlot));
	if ( arrSlot == NULL ) {
		return false;
	}
	for ( size_t i = 0; i < pNode->EdgeCount; i++ ) {
		const __xrt_pattern_nfa_edge* pEdge = &pNode->Edges[i];
		uint64 iHash = __xrtPatternNfaEdgeHash(
			pEdge->Kind,
			pEdge->Byte,
			pEdge->Text,
			pEdge->Suffix
		);
		size_t iSlot = (size_t)(iHash & (iCapacity - 1u));

		while ( arrSlot[iSlot] != 0 ) {
			iSlot = (iSlot + 1u) & (iCapacity - 1u);
		}
		arrSlot[iSlot] = (uint32)i + 1u;
	}
	xrtFree(pNode->EdgeSlots);
	pNode->EdgeSlots = arrSlot;
	pNode->EdgeSlotCapacity = iCapacity;
	return true;
}



/* 追加一个空逻辑 trie 节点。 */
static bool __xrtPatternNfaNode(
	__xrt_pattern_compiler* pCompiler,
	uint32* pIndex
)
{
	__xrt_pattern_nfa* pNfa = &pCompiler->Nfa;
	__xrt_pattern_nfa_node* pNode;

	if ( pNfa->Count >= pCompiler->Options->MaxStates ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			pCompiler->Operation,
			"logical pattern trie exceeds its state limit",
			false,
			0,
			false,
			0
		);
		return false;
	}
	if ( !__xrtPatternGrow(
		(ptr*)&pNfa->Nodes,
		&pNfa->Capacity,
		pNfa->Count + 1u,
		sizeof(*pNfa->Nodes)
	) ) {
		return false;
	}
	pNode = &pNfa->Nodes[pNfa->Count];
	memset(pNode, 0, sizeof(*pNode));
	pNode->Param = __XRT_PATTERN_INDEX_NONE;
	pNode->Terminal = __XRT_PATTERN_INDEX_NONE;
	pNode->Tail = __XRT_PATTERN_INDEX_NONE;
	*pIndex = (uint32)pNfa->Count++;
	return true;
}



/* 查找或创建一条精确分隔符/字面字段边。 */
static bool __xrtPatternNfaEdge(
	__xrt_pattern_compiler* pCompiler,
	uint32 iNode,
	uint8 iKind,
	uint8 iByte,
	xstrview Text,
	xstrview Suffix,
	uint32* pNext
)
{
	__xrt_pattern_nfa_node* pNode = &pCompiler->Nfa.Nodes[iNode];
	__xrt_pattern_nfa_edge* pEdge;
	uint32 iNext;
	uint64 iHash = __xrtPatternNfaEdgeHash(iKind, iByte, Text, Suffix);

	if ( pNode->EdgeSlotCapacity != 0 ) {
		size_t iSlot = (size_t)(
			iHash & (pNode->EdgeSlotCapacity - 1u)
		);

		while ( pNode->EdgeSlots[iSlot] != 0 ) {
			pEdge = &pNode->Edges[pNode->EdgeSlots[iSlot] - 1u];
			if ( __xrtPatternNfaEdgeEqual(
				pEdge,
				iKind,
				iByte,
				Text,
				Suffix
			) ) {
				*pNext = pEdge->Next;
				return true;
			}
			iSlot = (iSlot + 1u) & (pNode->EdgeSlotCapacity - 1u);
		}
	} else {
		for ( size_t i = 0; i < pNode->EdgeCount; i++ ) {
			pEdge = &pNode->Edges[i];
			if ( __xrtPatternNfaEdgeEqual(
				pEdge,
				iKind,
				iByte,
				Text,
				Suffix
			) ) {
				*pNext = pEdge->Next;
				return true;
			}
		}
	}
	if ( !__xrtPatternNfaEdgeSlots(pNode, pNode->EdgeCount + 1u) ) {
		return false;
	}
	if ( !__xrtPatternNfaNode(pCompiler, &iNext) ) {
		return false;
	}
	/* Node 数组可能重分配，必须重新取得父节点。 */
	pNode = &pCompiler->Nfa.Nodes[iNode];
	if ( !__xrtPatternGrow(
		(ptr*)&pNode->Edges,
		&pNode->EdgeCapacity,
		pNode->EdgeCount + 1u,
		sizeof(*pNode->Edges)
	) ) {
		return false;
	}
	pEdge = &pNode->Edges[pNode->EdgeCount++];
	memset(pEdge, 0, sizeof(*pEdge));
	pEdge->Kind = iKind;
	pEdge->Byte = iByte;
	pEdge->Text = Text;
	pEdge->Suffix = Suffix;
	pEdge->Next = iNext;
	if ( pNode->EdgeSlotCapacity != 0 ) {
		size_t iSlot = (size_t)(
			iHash & (pNode->EdgeSlotCapacity - 1u)
		);

		while ( pNode->EdgeSlots[iSlot] != 0 ) {
			iSlot = (iSlot + 1u) & (pNode->EdgeSlotCapacity - 1u);
		}
		pNode->EdgeSlots[iSlot] = (uint32)pNode->EdgeCount;
	}
	*pNext = iNext;
	return true;
}



/* 同结构模式只允许优先级明确区分；否则编译失败而非依赖隐式顺序。 */
static bool __xrtPatternNfaTerminal(
	__xrt_pattern_compiler* pCompiler,
	uint32* pCurrent,
	uint32 iEntry
)
{
	if ( *pCurrent == __XRT_PATTERN_INDEX_NONE ) {
		*pCurrent = iEntry;
		return true;
	}
	if ( pCompiler->Sources[*pCurrent]->Priority ==
		 pCompiler->Sources[iEntry]->Priority ) {
		__xrtPatternError(
			XERR_EXISTS,
			XPATTERN_ERROR_CONFLICT,
			pCompiler->Operation,
			"indistinguishable patterns have the same priority",
			true,
			iEntry,
			false,
			0
		);
		return false;
	}
	if ( pCompiler->Sources[iEntry]->Priority >
		 pCompiler->Sources[*pCurrent]->Priority ) {
		*pCurrent = iEntry;
	}
	return true;
}



/* 把一条已解析源模式插入逻辑 trie。 */
static bool __xrtPatternNfaInsert(
	__xrt_pattern_compiler* pCompiler,
	uint32 iEntry
)
{
	const __xrt_pattern_source* pSource = pCompiler->Sources[iEntry];
	uint32 iNode = 0;

	for ( size_t i = 0; i < pSource->AtomCount; i++ ) {
		const __xrt_pattern_atom* pAtom = &pSource->Atoms[i];
		uint32 iNext;

		if ( pAtom->Kind == __XRT_PATTERN_ATOM_SEPARATOR ) {
			if ( !__xrtPatternNfaEdge(
				pCompiler,
				iNode,
				__XRT_PATTERN_NFA_SEPARATOR,
				pAtom->Byte,
				(xstrview){ 0 },
				(xstrview){ 0 },
				&iNext
			) ) {
				return false;
			}
			iNode = iNext;
		} else if ( pAtom->Kind == __XRT_PATTERN_ATOM_LITERAL ) {
			if ( !__xrtPatternNfaEdge(
				pCompiler,
				iNode,
				__XRT_PATTERN_NFA_LITERAL,
				0,
				pAtom->Text,
				(xstrview){ 0 },
				&iNext
			) ) {
				return false;
			}
			iNode = iNext;
		} else if ( pAtom->Kind == __XRT_PATTERN_ATOM_AFFIX ) {
			if ( !__xrtPatternNfaEdge(
				pCompiler,
				iNode,
				__XRT_PATTERN_NFA_AFFIX,
				0,
				pAtom->Text,
				pAtom->Suffix,
				&iNext
			) ) {
				return false;
			}
			iNode = iNext;
		} else if ( pAtom->Kind == __XRT_PATTERN_ATOM_CAPTURE ) {
			__xrt_pattern_nfa_node* pNode = &pCompiler->Nfa.Nodes[iNode];

			if ( pNode->Param == __XRT_PATTERN_INDEX_NONE ) {
				if ( !__xrtPatternNfaNode(pCompiler, &iNext) ) {
					return false;
				}
				pCompiler->Nfa.Nodes[iNode].Param = iNext;
			} else {
				iNext = pNode->Param;
			}
			iNode = iNext;
		} else if ( pAtom->Kind == __XRT_PATTERN_ATOM_TAIL ) {
			return __xrtPatternNfaTerminal(
				pCompiler,
				&pCompiler->Nfa.Nodes[iNode].Tail,
				iEntry
			);
		} else {
			__xrtPatternSetInternal();
			return false;
		}
	}
	return __xrtPatternNfaTerminal(
		pCompiler,
		&pCompiler->Nfa.Nodes[iNode].Terminal,
		iEntry
	);
}



/* 精确原子高于混合捕获，混合捕获高于整字段与尾捕获。 */
static int __xrtPatternAtomWeight(const __xrt_pattern_atom* pAtom)
{
	if ( pAtom == NULL ) {
		return 5;
	}
	if ( (pAtom->Kind == __XRT_PATTERN_ATOM_SEPARATOR) ||
		 (pAtom->Kind == __XRT_PATTERN_ATOM_LITERAL) ) {
		return 4;
	}
	if ( pAtom->Kind == __XRT_PATTERN_ATOM_AFFIX ) {
		return 3;
	}
	if ( pAtom->Kind == __XRT_PATTERN_ATOM_CAPTURE ) {
		return 2;
	}
	return 1;
}



/* 建立与输入无关的确定性优先顺序，运行期只比较一个 Rank。 */
static int __xrtPatternSourceCompare(
	const __xrt_pattern_source* pA,
	const __xrt_pattern_source* pB
)
{
	size_t iMax = pA->AtomCount > pB->AtomCount ?
		pA->AtomCount : pB->AtomCount;

	for ( size_t i = 0; i <= iMax; i++ ) {
		const __xrt_pattern_atom* pAtomA =
			i < pA->AtomCount ? &pA->Atoms[i] : NULL;
		const __xrt_pattern_atom* pAtomB =
			i < pB->AtomCount ? &pB->Atoms[i] : NULL;
		int iWeightA = __xrtPatternAtomWeight(pAtomA);
		int iWeightB = __xrtPatternAtomWeight(pAtomB);

		if ( iWeightA != iWeightB ) {
			return iWeightA > iWeightB ? -1 : 1;
		}
		if ( (pAtomA == NULL) || (pAtomB == NULL) ) {
			break;
		}
		if ( pAtomA->Kind != pAtomB->Kind ) {
			return pAtomA->Kind < pAtomB->Kind ? -1 : 1;
		}
		if ( pAtomA->Kind == __XRT_PATTERN_ATOM_SEPARATOR ) {
			if ( pAtomA->Byte != pAtomB->Byte ) {
				return pAtomA->Byte < pAtomB->Byte ? -1 : 1;
			}
		} else if ( pAtomA->Kind == __XRT_PATTERN_ATOM_LITERAL ) {
			size_t iCommon = pAtomA->Text.Size < pAtomB->Text.Size ?
				pAtomA->Text.Size : pAtomB->Text.Size;
			int iCompare = iCommon != 0 ? memcmp(
				pAtomA->Text.Data,
				pAtomB->Text.Data,
				iCommon
			) : 0;

			if ( iCompare != 0 ) {
				return iCompare;
			}
			if ( pAtomA->Text.Size != pAtomB->Text.Size ) {
				return pAtomA->Text.Size < pAtomB->Text.Size ? -1 : 1;
			}
		} else if ( pAtomA->Kind == __XRT_PATTERN_ATOM_AFFIX ) {
			size_t iFixedA = pAtomA->Text.Size + pAtomA->Suffix.Size;
			size_t iFixedB = pAtomB->Text.Size + pAtomB->Suffix.Size;
			size_t iCommon;
			int iCompare;

			if ( iFixedA != iFixedB ) {
				return iFixedA > iFixedB ? -1 : 1;
			}
			if ( pAtomA->Text.Size != pAtomB->Text.Size ) {
				return pAtomA->Text.Size > pAtomB->Text.Size ? -1 : 1;
			}
			iCommon = pAtomA->Text.Size;
			iCompare = iCommon != 0 ? memcmp(
				pAtomA->Text.Data,
				pAtomB->Text.Data,
				iCommon
			) : 0;
			if ( iCompare != 0 ) {
				return iCompare;
			}
			iCommon = pAtomA->Suffix.Size < pAtomB->Suffix.Size ?
				pAtomA->Suffix.Size : pAtomB->Suffix.Size;
			iCompare = iCommon != 0 ? memcmp(
				pAtomA->Suffix.Data,
				pAtomB->Suffix.Data,
				iCommon
			) : 0;
			if ( iCompare != 0 ) {
				return iCompare;
			}
			if ( pAtomA->Suffix.Size != pAtomB->Suffix.Size ) {
				return pAtomA->Suffix.Size < pAtomB->Suffix.Size ? -1 : 1;
			}
		}
	}
	if ( pA->Priority != pB->Priority ) {
		return pA->Priority > pB->Priority ? -1 : 1;
	}
	if ( pA->Order != pB->Order ) {
		return pA->Order < pB->Order ? -1 : 1;
	}
	return 0;
}



static int __xrtPatternRankCompare(const void* pLeft, const void* pRight)
{
	const __xrt_pattern_rank_item* pA =
		(const __xrt_pattern_rank_item*)pLeft;
	const __xrt_pattern_rank_item* pB =
		(const __xrt_pattern_rank_item*)pRight;

	return __xrtPatternSourceCompare(pA->Source, pB->Source);
}



/* 分配并写入每条源模式的全局优先 Rank。 */
static bool __xrtPatternRanks(__xrt_pattern_compiler* pCompiler)
{
	__xrt_pattern_rank_item* arrItem;

	if ( pCompiler->SourceCount == 0 ) {
		return true;
	}
	if ( (pCompiler->SourceCount > (SIZE_MAX / sizeof(*arrItem))) ||
		 (pCompiler->SourceCount > (SIZE_MAX / sizeof(*pCompiler->Ranks))) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	arrItem = (__xrt_pattern_rank_item*)xrtMalloc(
		pCompiler->SourceCount * sizeof(*arrItem)
	);
	pCompiler->Ranks = (uint32*)xrtMalloc(
		pCompiler->SourceCount * sizeof(*pCompiler->Ranks)
	);
	if ( (arrItem == NULL) || (pCompiler->Ranks == NULL) ) {
		xrtFree(arrItem);
		return false;
	}
	for ( size_t i = 0; i < pCompiler->SourceCount; i++ ) {
		arrItem[i].Source = pCompiler->Sources[i];
		arrItem[i].Index = (uint32)i;
	}
	qsort(
		arrItem,
		pCompiler->SourceCount,
		sizeof(*arrItem),
		__xrtPatternRankCompare
	);
	for ( size_t i = 0; i < pCompiler->SourceCount; i++ ) {
		pCompiler->Ranks[arrItem[i].Index] = (uint32)i;
	}
	xrtFree(arrItem);
	return true;
}



static int __xrtPatternU32Compare(const void* pLeft, const void* pRight)
{
	uint32 iLeft = *(const uint32*)pLeft;
	uint32 iRight = *(const uint32*)pRight;

	return iLeft < iRight ? -1 : (iLeft > iRight ? 1 : 0);
}



static bool __xrtPatternU32Push(
	__xrt_pattern_u32_vector* pVector,
	uint32 iValue
)
{
	if ( !__xrtPatternGrow(
		(ptr*)&pVector->Data,
		&pVector->Capacity,
		pVector->Count + 1u,
		sizeof(*pVector->Data)
	) ) {
		return false;
	}
	pVector->Data[pVector->Count++] = iValue;
	return true;
}



static void __xrtPatternU32Unique(__xrt_pattern_u32_vector* pVector)
{
	size_t iWrite = 0;

	if ( pVector->Count > 1u ) {
		qsort(
			pVector->Data,
			pVector->Count,
			sizeof(*pVector->Data),
			__xrtPatternU32Compare
		);
	}
	for ( size_t i = 0; i < pVector->Count; i++ ) {
		if ( (iWrite == 0) ||
			 (pVector->Data[i] != pVector->Data[iWrite - 1u]) ) {
			pVector->Data[iWrite++] = pVector->Data[i];
		}
	}
	pVector->Count = iWrite;
}



/* 状态集合哈希只服务编译期去重。 */
static uint64 __xrtPatternStateHash(const uint32* arrNode, size_t iCount)
{
	uint64 iHash = UINT64_C(0x243f6a8885a308d3) ^ (uint64)iCount;

	for ( size_t i = 0; i < iCount; i++ ) {
		iHash ^= __xrtPatternHashMix(
			(uint64)arrNode[i] + UINT64_C(0x9e3779b97f4a7c15)
		);
		iHash = (iHash << 17u) | (iHash >> 47u);
		iHash *= UINT64_C(0x94d049bb133111eb);
	}
	return __xrtPatternHashMix(iHash);
}



static void __xrtPatternDfaFree(__xrt_pattern_dfa* pDfa)
{
	for ( size_t i = 0; i < pDfa->Count; i++ ) {
		for ( size_t j = 0; j < pDfa->States[i].AffixStateCount; j++ ) {
			xrtFree(pDfa->States[i].AffixStates[j].Nodes);
			xrtFree(pDfa->States[i].AffixStates[j].Transitions);
		}
		xrtFree(pDfa->States[i].Nodes);
		xrtFree(pDfa->States[i].Literals);
		xrtFree(pDfa->States[i].Separators);
		xrtFree(pDfa->States[i].AffixStates);
	}
	xrtFree(pDfa->States);
	xrtFree(pDfa->Slots);
	memset(pDfa, 0, sizeof(*pDfa));
}



/* 扩展 DFA 状态集合索引并重哈希既有状态。 */
static bool __xrtPatternDfaSlots(
	__xrt_pattern_dfa* pDfa,
	size_t iNeed
)
{
	__xrt_pattern_state_slot* arrSlot;
	size_t iCapacity = pDfa->SlotCapacity != 0 ? pDfa->SlotCapacity : 16u;

	for ( ;; ) {
		size_t iThreshold =
			((iCapacity / 10u) * 7u) +
			((((iCapacity % 10u) * 7u) + 9u) / 10u);

		if ( iNeed < iThreshold ) {
			break;
		}
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			__xrtPatternSetSizeOverflow();
			return false;
		}
		iCapacity *= 2u;
	}
	if ( iCapacity == pDfa->SlotCapacity ) {
		return true;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(*arrSlot)) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	arrSlot = (__xrt_pattern_state_slot*)xrtCalloc(
		iCapacity,
		sizeof(*arrSlot)
	);
	if ( arrSlot == NULL ) {
		return false;
	}
	for ( size_t i = 0; i < pDfa->Count; i++ ) {
		size_t iSlot = (size_t)(pDfa->States[i].SetHash & (iCapacity - 1u));

		while ( arrSlot[iSlot].StatePlusOne != 0 ) {
			iSlot = (iSlot + 1u) & (iCapacity - 1u);
		}
		arrSlot[iSlot].Hash = pDfa->States[i].SetHash;
		arrSlot[iSlot].StatePlusOne = (uint32)i + 1u;
	}
	xrtFree(pDfa->Slots);
	pDfa->Slots = arrSlot;
	pDfa->SlotCapacity = iCapacity;
	pDfa->SlotCount = pDfa->Count;
	return true;
}



/* 在 DFA 中查找或插入一个排好序的 NFA 节点集合。 */
static bool __xrtPatternDfaState(
	__xrt_pattern_compiler* pCompiler,
	const uint32* arrNode,
	size_t iNodeCount,
	uint32* pState
)
{
	__xrt_pattern_dfa* pDfa = &pCompiler->Dfa;
	uint64 iHash = __xrtPatternStateHash(arrNode, iNodeCount);
	size_t iSlot;
	__xrt_pattern_temp_state* pNew;
	uint32 iTerminal = __XRT_PATTERN_INDEX_NONE;
	uint32 iTail = __XRT_PATTERN_INDEX_NONE;

	if ( !__xrtPatternDfaSlots(pDfa, pDfa->Count + 1u) ) {
		return false;
	}
	iSlot = (size_t)(iHash & (pDfa->SlotCapacity - 1u));
	while ( pDfa->Slots[iSlot].StatePlusOne != 0 ) {
		uint32 iExisting = pDfa->Slots[iSlot].StatePlusOne - 1u;
		__xrt_pattern_temp_state* pExisting = &pDfa->States[iExisting];

		if ( (pDfa->Slots[iSlot].Hash == iHash) &&
			 (pExisting->NodeCount == iNodeCount) &&
			 ((iNodeCount == 0) ||
			  (memcmp(
				pExisting->Nodes,
				arrNode,
				iNodeCount * sizeof(*arrNode)
			  ) == 0)) ) {
			*pState = iExisting;
			return true;
		}
		iSlot = (iSlot + 1u) & (pDfa->SlotCapacity - 1u);
	}
	if ( (pDfa->Count >= pCompiler->Options->MaxStates) ||
		 (pCompiler->AffixStateCount >=
		  (pCompiler->Options->MaxStates - pDfa->Count)) ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			pCompiler->Operation,
			"determinized matcher exceeds its state limit",
			false,
			0,
			false,
			0
		);
		return false;
	}
	if ( !__xrtPatternGrow(
		(ptr*)&pDfa->States,
		&pDfa->Capacity,
		pDfa->Count + 1u,
		sizeof(*pDfa->States)
	) ) {
		return false;
	}
	pNew = &pDfa->States[pDfa->Count];
	memset(pNew, 0, sizeof(*pNew));
	pNew->Default = __XRT_PATTERN_INDEX_NONE;
	pNew->Terminal = __XRT_PATTERN_INDEX_NONE;
	pNew->Tail = __XRT_PATTERN_INDEX_NONE;
	if ( iNodeCount != 0 ) {
		if ( iNodeCount > (SIZE_MAX / sizeof(*pNew->Nodes)) ) {
			__xrtPatternSetSizeOverflow();
			return false;
		}
		pNew->Nodes = (uint32*)xrtMalloc(iNodeCount * sizeof(*pNew->Nodes));
		if ( pNew->Nodes == NULL ) {
			return false;
		}
		memcpy(pNew->Nodes, arrNode, iNodeCount * sizeof(*pNew->Nodes));
	}
	pNew->NodeCount = iNodeCount;
	pNew->SetHash = iHash;
	for ( size_t i = 0; i < iNodeCount; i++ ) {
		const __xrt_pattern_nfa_node* pNode =
			&pCompiler->Nfa.Nodes[arrNode[i]];

		if ( (pNode->Terminal != __XRT_PATTERN_INDEX_NONE) &&
			 ((iTerminal == __XRT_PATTERN_INDEX_NONE) ||
			  (pCompiler->Ranks[pNode->Terminal] <
			   pCompiler->Ranks[iTerminal])) ) {
			iTerminal = pNode->Terminal;
		}
		if ( (pNode->Tail != __XRT_PATTERN_INDEX_NONE) &&
			 ((iTail == __XRT_PATTERN_INDEX_NONE) ||
			  (pCompiler->Ranks[pNode->Tail] < pCompiler->Ranks[iTail])) ) {
			iTail = pNode->Tail;
		}
	}
	pNew->Terminal = iTerminal;
	pNew->Tail = iTail;
	*pState = (uint32)pDfa->Count;
	pDfa->Count++;
	pDfa->Slots[iSlot].Hash = iHash;
	pDfa->Slots[iSlot].StatePlusOne = *pState + 1u;
	pDfa->SlotCount++;
	return true;
}



static bool __xrtPatternTempLiteral(
	__xrt_pattern_dfa* pDfa,
	uint32 iState,
	xstrview Text,
	uint32 iNext
)
{
	__xrt_pattern_temp_state* pState = &pDfa->States[iState];
	__xrt_pattern_temp_literal* pItem;

	if ( !__xrtPatternGrow(
		(ptr*)&pState->Literals,
		&pState->LiteralCapacity,
		pState->LiteralCount + 1u,
		sizeof(*pState->Literals)
	) ) {
		return false;
	}
	pItem = &pState->Literals[pState->LiteralCount++];
	pItem->Text = Text;
	pItem->Next = iNext;
	return true;
}



static bool __xrtPatternTempSeparator(
	__xrt_pattern_dfa* pDfa,
	uint32 iState,
	uint8 iByte,
	uint32 iNext
)
{
	__xrt_pattern_temp_state* pState = &pDfa->States[iState];
	__xrt_pattern_temp_separator* pItem;

	if ( !__xrtPatternGrow(
		(ptr*)&pState->Separators,
		&pState->SeparatorCapacity,
		pState->SeparatorCount + 1u,
		sizeof(*pState->Separators)
	) ) {
		return false;
	}
	pItem = &pState->Separators[pState->SeparatorCount++];
	pItem->Byte = iByte;
	pItem->Next = iNext;
	return true;
}



static void __xrtPatternByteSort(uint8* arrByte, size_t iCount)
{
	for ( size_t i = 1u; i < iCount; i++ ) {
		uint8 iByte = arrByte[i];
		size_t j = i;

		while ( (j != 0) && (arrByte[j - 1u] > iByte) ) {
			arrByte[j] = arrByte[j - 1u];
			j--;
		}
		arrByte[j] = iByte;
	}
}



static bool __xrtPatternAffixNfaNode(
	__xrt_pattern_affix_nfa* pNfa,
	uint32* pIndex
)
{
	__xrt_pattern_affix_nfa_node* pNode;

	if ( pNfa->Count >= UINT32_MAX ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	if ( !__xrtPatternGrow(
		(ptr*)&pNfa->Nodes,
		&pNfa->Capacity,
		pNfa->Count + 1u,
		sizeof(*pNfa->Nodes)
	) ) {
		return false;
	}
	pNode = &pNfa->Nodes[pNfa->Count];
	memset(pNode, 0, sizeof(*pNode));
	pNode->Wild = __XRT_PATTERN_INDEX_NONE;
	*pIndex = (uint32)pNfa->Count++;
	return true;
}



/* 共享字节 trie 边，父节点地址在追加子节点后必须重新取得。 */
static bool __xrtPatternAffixNfaEdge(
	__xrt_pattern_affix_nfa* pNfa,
	uint32 iNode,
	uint8 iByte,
	uint32* pNext
)
{
	__xrt_pattern_affix_nfa_node* pNode = &pNfa->Nodes[iNode];
	__xrt_pattern_affix_nfa_edge* pEdge;
	uint32 iNext;

	for ( size_t i = 0; i < pNode->EdgeCount; i++ ) {
		if ( pNode->Edges[i].Byte == iByte ) {
			*pNext = pNode->Edges[i].Next;
			return true;
		}
	}
	if ( !__xrtPatternAffixNfaNode(pNfa, &iNext) ) {
		return false;
	}
	pNode = &pNfa->Nodes[iNode];
	if ( !__xrtPatternGrow(
		(ptr*)&pNode->Edges,
		&pNode->EdgeCapacity,
		pNode->EdgeCount + 1u,
		sizeof(*pNode->Edges)
	) ) {
		return false;
	}
	pEdge = &pNode->Edges[pNode->EdgeCount++];
	pEdge->Byte = iByte;
	pEdge->Next = iNext;
	*pNext = iNext;
	return true;
}



static bool __xrtPatternAffixNfaAccept(
	__xrt_pattern_affix_nfa* pNfa,
	uint32 iNode,
	uint32 iNext
)
{
	__xrt_pattern_affix_nfa_node* pNode = &pNfa->Nodes[iNode];

	for ( size_t i = 0; i < pNode->AcceptCount; i++ ) {
		if ( pNode->Accepts[i] == iNext ) {
			return true;
		}
	}
	if ( !__xrtPatternGrow(
		(ptr*)&pNode->Accepts,
		&pNode->AcceptCapacity,
		pNode->AcceptCount + 1u,
		sizeof(*pNode->Accepts)
	) ) {
		return false;
	}
	pNode->Accepts[pNode->AcceptCount++] = iNext;
	return true;
}



/* prefix trie 与同前缀的 suffix trie 都共享，通配活跃集不随模式数复制。 */
static bool __xrtPatternAffixNfaPattern(
	__xrt_pattern_affix_nfa* pNfa,
	const __xrt_pattern_nfa_edge* pEdge
)
{
	uint32 iCurrent = 0;
	uint32 iNext;

	for ( size_t i = 0; i < pEdge->Text.Size; i++ ) {
		if ( !__xrtPatternAffixNfaEdge(
			pNfa,
			iCurrent,
			(uint8)pEdge->Text.Data[i],
			&iNext
		) ) {
			return false;
		}
		iCurrent = iNext;
	}
	if ( pNfa->Nodes[iCurrent].Wild == __XRT_PATTERN_INDEX_NONE ) {
		if ( !__xrtPatternAffixNfaNode(pNfa, &iNext) ) {
			return false;
		}
		pNfa->Nodes[iCurrent].Wild = iNext;
		pNfa->Nodes[iNext].Wild = iNext;
	} else {
		iNext = pNfa->Nodes[iCurrent].Wild;
	}
	iCurrent = iNext;
	if ( pEdge->Suffix.Size == 0 ) {
		return __xrtPatternAffixNfaAccept(pNfa, iCurrent, pEdge->Next);
	}
	for ( size_t i = 0; i < pEdge->Suffix.Size; i++ ) {
		if ( !__xrtPatternAffixNfaEdge(
			pNfa,
			iCurrent,
			(uint8)pEdge->Suffix.Data[i],
			&iNext
		) ) {
			return false;
		}
		iCurrent = iNext;
	}
	return __xrtPatternAffixNfaAccept(pNfa, iCurrent, pEdge->Next);
}



static void __xrtPatternAffixDfaFree(__xrt_pattern_affix_dfa* pDfa)
{
	for ( size_t i = 0; i < pDfa->Count; i++ ) {
		xrtFree(pDfa->States[i].Nodes);
		xrtFree(pDfa->States[i].Transitions);
	}
	xrtFree(pDfa->States);
	xrtFree(pDfa->Slots);
	memset(pDfa, 0, sizeof(*pDfa));
}



static bool __xrtPatternAffixDfaSlots(
	__xrt_pattern_affix_dfa* pDfa,
	size_t iNeed
)
{
	__xrt_pattern_state_slot* arrSlot;
	size_t iCapacity = pDfa->SlotCapacity != 0 ? pDfa->SlotCapacity : 16u;

	while ( iNeed >= ((iCapacity / 4u) * 3u) ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			__xrtPatternSetSizeOverflow();
			return false;
		}
		iCapacity *= 2u;
	}
	if ( iCapacity == pDfa->SlotCapacity ) {
		return true;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(*arrSlot)) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	arrSlot = (__xrt_pattern_state_slot*)xrtCalloc(
		iCapacity,
		sizeof(*arrSlot)
	);
	if ( arrSlot == NULL ) {
		return false;
	}
	for ( size_t i = 0; i < pDfa->Count; i++ ) {
		size_t iSlot = (size_t)(pDfa->States[i].SetHash & (iCapacity - 1u));

		while ( arrSlot[iSlot].StatePlusOne != 0 ) {
			iSlot = (iSlot + 1u) & (iCapacity - 1u);
		}
		arrSlot[iSlot].Hash = pDfa->States[i].SetHash;
		arrSlot[iSlot].StatePlusOne = (uint32)i + 1u;
	}
	xrtFree(pDfa->Slots);
	pDfa->Slots = arrSlot;
	pDfa->SlotCapacity = iCapacity;
	pDfa->SlotCount = pDfa->Count;
	return true;
}



static bool __xrtPatternAffixDfaState(
	__xrt_pattern_compiler* pCompiler,
	__xrt_pattern_affix_dfa* pDfa,
	const uint32* arrNode,
	size_t iNodeCount,
	uint32* pState
)
{
	uint64 iHash = __xrtPatternStateHash(arrNode, iNodeCount);
	size_t iSlot;
	__xrt_pattern_temp_affix_state* pNew;

	if ( !__xrtPatternAffixDfaSlots(pDfa, pDfa->Count + 1u) ) {
		return false;
	}
	iSlot = (size_t)(iHash & (pDfa->SlotCapacity - 1u));
	while ( pDfa->Slots[iSlot].StatePlusOne != 0 ) {
		uint32 iExisting = pDfa->Slots[iSlot].StatePlusOne - 1u;
		const __xrt_pattern_temp_affix_state* pExisting =
			&pDfa->States[iExisting];

		if ( (pDfa->Slots[iSlot].Hash == iHash) &&
			 (pExisting->NodeCount == iNodeCount) &&
			 ((iNodeCount == 0) ||
			  (memcmp(
				pExisting->Nodes,
				arrNode,
				iNodeCount * sizeof(*arrNode)
			  ) == 0)) ) {
			*pState = iExisting;
			return true;
		}
		iSlot = (iSlot + 1u) & (pDfa->SlotCapacity - 1u);
	}
	if ( (pCompiler->Dfa.Count >= pCompiler->Options->MaxStates) ||
		 (pCompiler->AffixStateCount >=
		  (pCompiler->Options->MaxStates - pCompiler->Dfa.Count)) ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			pCompiler->Operation,
			"compiled matcher exceeds its combined state limit",
			false,
			0,
			false,
			0
		);
		return false;
	}
	if ( !__xrtPatternGrow(
		(ptr*)&pDfa->States,
		&pDfa->Capacity,
		pDfa->Count + 1u,
		sizeof(*pDfa->States)
	) ) {
		return false;
	}
	pNew = &pDfa->States[pDfa->Count];
	memset(pNew, 0, sizeof(*pNew));
	pNew->Default = __XRT_PATTERN_INDEX_NONE;
	pNew->Output = __XRT_PATTERN_INDEX_NONE;
	if ( iNodeCount != 0 ) {
		if ( iNodeCount > (SIZE_MAX / sizeof(*pNew->Nodes)) ) {
			__xrtPatternSetSizeOverflow();
			return false;
		}
		pNew->Nodes = (uint32*)xrtMalloc(iNodeCount * sizeof(*pNew->Nodes));
		if ( pNew->Nodes == NULL ) {
			return false;
		}
		memcpy(pNew->Nodes, arrNode, iNodeCount * sizeof(*pNew->Nodes));
	}
	pNew->NodeCount = iNodeCount;
	pNew->SetHash = iHash;
	*pState = (uint32)pDfa->Count++;
	pDfa->Slots[iSlot].Hash = iHash;
	pDfa->Slots[iSlot].StatePlusOne = *pState + 1u;
	pDfa->SlotCount++;
	pCompiler->AffixStateCount++;
	return true;
}



static bool __xrtPatternTempAffixTransition(
	__xrt_pattern_affix_dfa* pDfa,
	uint32 iState,
	uint8 iByte,
	uint32 iNext
)
{
	__xrt_pattern_temp_affix_state* pState = &pDfa->States[iState];
	__xrt_pattern_temp_affix_transition* pTransition;

	if ( !__xrtPatternGrow(
		(ptr*)&pState->Transitions,
		&pState->TransitionCapacity,
		pState->TransitionCount + 1u,
		sizeof(*pState->Transitions)
	) ) {
		return false;
	}
	pTransition = &pState->Transitions[pState->TransitionCount++];
	pTransition->Byte = iByte;
	pTransition->Next = iNext;
	return true;
}



/* 把一个外层 DFA 状态的全部混合字段边确定化为局部字节 DFA。 */
static bool __xrtPatternAffixBuild(
	__xrt_pattern_compiler* pCompiler,
	uint32 iOuterState,
	const __xrt_pattern_key_ref* arrEdge,
	size_t iEdgeCount,
	const __xrt_pattern_u32_vector* pOuterDefault
)
{
	__xrt_pattern_affix_nfa Nfa = { 0 };
	__xrt_pattern_affix_dfa Dfa = { 0 };
	__xrt_pattern_u32_vector Default = { 0 };
	__xrt_pattern_u32_vector Destination = { 0 };
	__xrt_pattern_u32_vector Output = { 0 };
	bool bResult = false;
	uint32 iNfaRoot;
	uint32 iRoot;

	if ( iEdgeCount == 0 ) {
		return true;
	}
	if ( !__xrtPatternAffixNfaNode(&Nfa, &iNfaRoot) || (iNfaRoot != 0) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < iEdgeCount; i++ ) {
		if ( !__xrtPatternAffixNfaPattern(&Nfa, arrEdge[i].Edge) ) {
			goto cleanup;
		}
	}
	if ( !__xrtPatternAffixDfaState(
		pCompiler,
		&Dfa,
		&iNfaRoot,
		1u,
		&iRoot
	) || (iRoot != 0) ) {
		goto cleanup;
	}
	for ( size_t iState = 0; iState < Dfa.Count; iState++ ) {
		const uint32* arrNode = Dfa.States[iState].Nodes;
		size_t iNodeCount = Dfa.States[iState].NodeCount;
		bool arrSeen[256] = { false };
		uint8 arrByte[256];
		size_t iByteCount = 0;
		uint32 iNext;

		Default.Count = 0;
		Output.Count = 0;
		for ( size_t i = 0; i < pOuterDefault->Count; i++ ) {
			if ( !__xrtPatternU32Push(&Output, pOuterDefault->Data[i]) ) {
				goto cleanup;
			}
		}
		for ( size_t i = 0; i < iNodeCount; i++ ) {
			const __xrt_pattern_affix_nfa_node* pNode = &Nfa.Nodes[arrNode[i]];

			if ( (pNode->Wild != __XRT_PATTERN_INDEX_NONE) &&
				 !__xrtPatternU32Push(&Default, pNode->Wild) ) {
				goto cleanup;
			}
			for ( size_t j = 0; j < pNode->EdgeCount; j++ ) {
				if ( !arrSeen[pNode->Edges[j].Byte] ) {
					arrSeen[pNode->Edges[j].Byte] = true;
					arrByte[iByteCount++] = pNode->Edges[j].Byte;
				}
			}
			for ( size_t j = 0; j < pNode->AcceptCount; j++ ) {
				if ( !__xrtPatternU32Push(&Output, pNode->Accepts[j]) ) {
					goto cleanup;
				}
			}
		}
		__xrtPatternU32Unique(&Output);
		if ( Output.Count != 0 ) {
			if ( !__xrtPatternDfaState(
				pCompiler,
				Output.Data,
				Output.Count,
				&iNext
			) ) {
				goto cleanup;
			}
			Dfa.States[iState].Output = iNext;
		}
		__xrtPatternU32Unique(&Default);
		if ( Default.Count != 0 ) {
			if ( !__xrtPatternAffixDfaState(
				pCompiler,
				&Dfa,
				Default.Data,
				Default.Count,
				&iNext
			) ) {
				goto cleanup;
			}
			Dfa.States[iState].Default = iNext;
		}
		__xrtPatternByteSort(arrByte, iByteCount);
		for ( size_t iByte = 0; iByte < iByteCount; iByte++ ) {
			Destination.Count = 0;
			for ( size_t i = 0; i < Default.Count; i++ ) {
				if ( !__xrtPatternU32Push(&Destination, Default.Data[i]) ) {
					goto cleanup;
				}
			}
			for ( size_t i = 0; i < iNodeCount; i++ ) {
				const __xrt_pattern_affix_nfa_node* pNode =
					&Nfa.Nodes[arrNode[i]];

				for ( size_t j = 0; j < pNode->EdgeCount; j++ ) {
					if ( (pNode->Edges[j].Byte == arrByte[iByte]) &&
						 !__xrtPatternU32Push(
							&Destination,
							pNode->Edges[j].Next
						 ) ) {
						goto cleanup;
					}
				}
			}
			__xrtPatternU32Unique(&Destination);
			if ( !__xrtPatternAffixDfaState(
				pCompiler,
				&Dfa,
				Destination.Data,
				Destination.Count,
				&iNext
			) ) {
				goto cleanup;
			}
			if ( (Dfa.States[iState].Default == __XRT_PATTERN_INDEX_NONE) ||
				 (iNext != Dfa.States[iState].Default) ) {
				if ( !__xrtPatternTempAffixTransition(
					&Dfa,
					(uint32)iState,
					arrByte[iByte],
					iNext
				) ) {
					goto cleanup;
				}
			}
		}
	}
	for ( size_t i = 0; i < Dfa.Count; i++ ) {
		xrtFree(Dfa.States[i].Nodes);
		Dfa.States[i].Nodes = NULL;
		Dfa.States[i].NodeCount = 0;
	}
	xrtFree(Dfa.Slots);
	Dfa.Slots = NULL;
	pCompiler->Dfa.States[iOuterState].AffixStates = Dfa.States;
	pCompiler->Dfa.States[iOuterState].AffixStateCount = Dfa.Count;
	Dfa.States = NULL;
	Dfa.Count = 0;
	bResult = true;

cleanup:
	for ( size_t i = 0; i < Nfa.Count; i++ ) {
		xrtFree(Nfa.Nodes[i].Edges);
		xrtFree(Nfa.Nodes[i].Accepts);
	}
	xrtFree(Nfa.Nodes);
	xrtFree(Default.Data);
	xrtFree(Destination.Data);
	xrtFree(Output.Data);
	__xrtPatternAffixDfaFree(&Dfa);
	return bResult;
}



static uint32 __xrtPatternTempAffixNext(
	const __xrt_pattern_temp_state* pOuter,
	uint32 iState,
	uint8 iByte
)
{
	const __xrt_pattern_temp_affix_state* pState = &pOuter->AffixStates[iState];

	for ( size_t i = 0; i < pState->TransitionCount; i++ ) {
		if ( pState->Transitions[i].Byte == iByte ) {
			return pState->Transitions[i].Next;
		}
	}
	return pState->Default;
}



/* 返回混合字段局部 DFA 对完整字面字段给出的外层目标集合。 */
static uint32 __xrtPatternTempAffixOutput(
	const __xrt_pattern_temp_state* pOuter,
	xstrview Text
)
{
	uint32 iState = 0;

	if ( pOuter->AffixStateCount == 0 ) {
		return pOuter->Default;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		iState = __xrtPatternTempAffixNext(
			pOuter,
			iState,
			(uint8)Text.Data[i]
		);
		if ( iState == __XRT_PATTERN_INDEX_NONE ) {
			return pOuter->Default;
		}
	}
	return pOuter->AffixStates[iState].Output;
}



static int __xrtPatternKeyCompare(const void* pLeft, const void* pRight)
{
	const __xrt_pattern_nfa_edge* pA =
		((const __xrt_pattern_key_ref*)pLeft)->Edge;
	const __xrt_pattern_nfa_edge* pB =
		((const __xrt_pattern_key_ref*)pRight)->Edge;
	size_t iCommon = pA->Text.Size < pB->Text.Size ?
		pA->Text.Size : pB->Text.Size;
	int iCompare = iCommon != 0 ? memcmp(
		pA->Text.Data,
		pB->Text.Data,
		iCommon
	) : 0;

	if ( iCompare != 0 ) {
		return iCompare;
	}
	return pA->Text.Size < pB->Text.Size ? -1 :
		(pA->Text.Size > pB->Text.Size ? 1 : 0);
}



static bool __xrtPatternKeyEqual(
	const __xrt_pattern_nfa_edge* pA,
	const __xrt_pattern_nfa_edge* pB
)
{
	return (pA->Text.Size == pB->Text.Size) &&
		((pA->Text.Size == 0) ||
		 (memcmp(pA->Text.Data, pB->Text.Data, pA->Text.Size) == 0));
}



/* 为一个 DFA 状态生成默认、字面量和精确分隔符转移。 */
static bool __xrtPatternDfaExpand(
	__xrt_pattern_compiler* pCompiler,
	uint32 iState,
	__xrt_pattern_u32_vector* pDefault,
	__xrt_pattern_u32_vector* pDestination,
	__xrt_pattern_key_ref** pKeys,
	size_t* pKeyCapacity,
	__xrt_pattern_key_ref** pAffixes,
	size_t* pAffixCapacity
)
{
	__xrt_pattern_temp_state* pState = &pCompiler->Dfa.States[iState];
	size_t iKeyCount = 0;
	size_t iAffixCount = 0;
	bool arrSeparatorSeen[256] = { false };
	uint8 arrSeparator[256];
	size_t iSeparatorCount = 0;
	uint32 iNext;

	pDefault->Count = 0;
	for ( size_t i = 0; i < pState->NodeCount; i++ ) {
		const __xrt_pattern_nfa_node* pNode =
			&pCompiler->Nfa.Nodes[pState->Nodes[i]];

		if ( (pNode->Param != __XRT_PATTERN_INDEX_NONE) &&
			 !__xrtPatternU32Push(pDefault, pNode->Param) ) {
			return false;
		}
		for ( size_t j = 0; j < pNode->EdgeCount; j++ ) {
			const __xrt_pattern_nfa_edge* pEdge = &pNode->Edges[j];

			if ( pEdge->Kind == __XRT_PATTERN_NFA_SEPARATOR ) {
				if ( !arrSeparatorSeen[pEdge->Byte] ) {
					arrSeparatorSeen[pEdge->Byte] = true;
					arrSeparator[iSeparatorCount++] = pEdge->Byte;
				}
			} else if ( pEdge->Kind == __XRT_PATTERN_NFA_LITERAL ) {
				if ( !__xrtPatternGrow(
					(ptr*)pKeys,
					pKeyCapacity,
					iKeyCount + 1u,
					sizeof(**pKeys)
				) ) {
					return false;
				}
				(*pKeys)[iKeyCount++].Edge = pEdge;
			} else if ( pEdge->Kind == __XRT_PATTERN_NFA_AFFIX ) {
				if ( !__xrtPatternGrow(
					(ptr*)pAffixes,
					pAffixCapacity,
					iAffixCount + 1u,
					sizeof(**pAffixes)
				) ) {
					return false;
				}
				(*pAffixes)[iAffixCount++].Edge = pEdge;
			} else {
				__xrtPatternSetInternal();
				return false;
			}
		}
	}
	__xrtPatternU32Unique(pDefault);
	if ( pDefault->Count != 0 ) {
		if ( !__xrtPatternDfaState(
			pCompiler,
			pDefault->Data,
			pDefault->Count,
			&iNext
		) ) {
			return false;
		}
		pCompiler->Dfa.States[iState].Default = iNext;
	}
	if ( !__xrtPatternAffixBuild(
		pCompiler,
		iState,
		*pAffixes,
		iAffixCount,
		pDefault
	) ) {
		return false;
	}
	if ( iKeyCount > 1u ) {
		qsort(*pKeys, iKeyCount, sizeof(**pKeys), __xrtPatternKeyCompare);
	}
	for ( size_t i = 0; i < iKeyCount; ) {
		size_t iEnd = i + 1u;
		uint32 iAffixOutput;

		while ( (iEnd < iKeyCount) &&
			 __xrtPatternKeyEqual((*pKeys)[i].Edge, (*pKeys)[iEnd].Edge) ) {
			iEnd++;
		}
		pDestination->Count = 0;
		pState = &pCompiler->Dfa.States[iState];
		iAffixOutput = __xrtPatternTempAffixOutput(
			pState,
			(*pKeys)[i].Edge->Text
		);
		if ( iAffixOutput != __XRT_PATTERN_INDEX_NONE ) {
			const __xrt_pattern_temp_state* pOutput =
				&pCompiler->Dfa.States[iAffixOutput];

			for ( size_t j = 0; j < pOutput->NodeCount; j++ ) {
				if ( !__xrtPatternU32Push(pDestination, pOutput->Nodes[j]) ) {
					return false;
				}
			}
		}
		for ( size_t j = i; j < iEnd; j++ ) {
			if ( !__xrtPatternU32Push(
				pDestination,
				(*pKeys)[j].Edge->Next
			) ) {
				return false;
			}
		}
		__xrtPatternU32Unique(pDestination);
		if ( !__xrtPatternDfaState(
			pCompiler,
			pDestination->Data,
			pDestination->Count,
			&iNext
		) || !__xrtPatternTempLiteral(
			&pCompiler->Dfa,
			iState,
			(*pKeys)[i].Edge->Text,
			iNext
		) ) {
			return false;
		}
		i = iEnd;
	}
	__xrtPatternByteSort(arrSeparator, iSeparatorCount);
	for ( size_t iSeparator = 0;
		iSeparator < iSeparatorCount; iSeparator++ ) {
		uint8 iByte = arrSeparator[iSeparator];

		pDestination->Count = 0;
		/* State 数组可能增长，因此每轮都按索引重新取得集合。 */
		pState = &pCompiler->Dfa.States[iState];
		for ( size_t i = 0; i < pState->NodeCount; i++ ) {
			const __xrt_pattern_nfa_node* pNode =
				&pCompiler->Nfa.Nodes[pState->Nodes[i]];

			for ( size_t j = 0; j < pNode->EdgeCount; j++ ) {
				const __xrt_pattern_nfa_edge* pEdge = &pNode->Edges[j];

				if ( (pEdge->Kind == __XRT_PATTERN_NFA_SEPARATOR) &&
					 (pEdge->Byte == iByte) &&
					 !__xrtPatternU32Push(pDestination, pEdge->Next) ) {
					return false;
				}
			}
		}
		__xrtPatternU32Unique(pDestination);
		if ( !__xrtPatternDfaState(
			pCompiler,
			pDestination->Data,
			pDestination->Count,
			&iNext
		) || !__xrtPatternTempSeparator(
			&pCompiler->Dfa,
			iState,
			iByte,
			iNext
		) ) {
			return false;
		}
	}
	return true;
}



/* 从逻辑 trie 构建无回溯字段 DFA。 */
static bool __xrtPatternDfaBuild(__xrt_pattern_compiler* pCompiler)
{
	__xrt_pattern_u32_vector Default = { 0 };
	__xrt_pattern_u32_vector Destination = { 0 };
	__xrt_pattern_key_ref* arrKey = NULL;
	__xrt_pattern_key_ref* arrAffix = NULL;
	size_t iKeyCapacity = 0;
	size_t iAffixCapacity = 0;
	uint32 iRootNode = 0;
	uint32 iRootState;
	bool bResult = false;

	if ( !__xrtPatternDfaState(
		pCompiler,
		&iRootNode,
		1u,
		&iRootState
	) || (iRootState != 0) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < pCompiler->Dfa.Count; i++ ) {
		if ( !__xrtPatternDfaExpand(
			pCompiler,
			(uint32)i,
			&Default,
			&Destination,
			&arrKey,
			&iKeyCapacity,
			&arrAffix,
			&iAffixCapacity
		) ) {
			goto cleanup;
		}
	}
	bResult = true;

cleanup:
	xrtFree(Default.Data);
	xrtFree(Destination.Data);
	xrtFree(arrKey);
	xrtFree(arrAffix);
	return bResult;
}



/* 安全累计最终单块布局。 */
static bool __xrtPatternPackLayout(
	size_t* pTotal,
	size_t iAlign,
	size_t iCount,
	size_t iSize,
	size_t* pOffset
)
{
	size_t iPadding = (iAlign - (*pTotal % iAlign)) % iAlign;
	size_t iBytes;

	if ( *pTotal > (SIZE_MAX - iPadding) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	*pTotal += iPadding;
	*pOffset = *pTotal;
	if ( (iSize != 0) && (iCount > (SIZE_MAX / iSize)) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	iBytes = iCount * iSize;
	if ( *pTotal > (SIZE_MAX - iBytes) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	*pTotal += iBytes;
	return true;
}



static bool __xrtPatternSizeAdd(size_t* pTotal, size_t iAdd)
{
	if ( *pTotal > (SIZE_MAX - iAdd) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	*pTotal += iAdd;
	return true;
}



/* 高扇出状态使用不超过 50% 负载的幂二开放寻址表。 */
static bool __xrtPatternLiteralSlots(size_t iCount, size_t* pSlots)
{
	size_t iNeed;
	size_t iSlots = 8u;

	if ( iCount <= __XRT_PATTERN_LINEAR_MAX ) {
		*pSlots = iCount;
		return true;
	}
	if ( iCount > (SIZE_MAX / 2u) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	iNeed = iCount * 2u;
	while ( iSlots < iNeed ) {
		if ( iSlots > (SIZE_MAX / 2u) ) {
			__xrtPatternSetSizeOverflow();
			return false;
		}
		iSlots *= 2u;
	}
	*pSlots = iSlots;
	return true;
}



/* 复制字节到最终字符串区并返回只读视图。 */
static xstrview __xrtPatternPackText(
	char* sStrings,
	size_t* pCursor,
	xstrview Text,
	bool bTerminate
)
{
	char* sTarget = sStrings + *pCursor;

	if ( Text.Size != 0 ) {
		memcpy(sTarget, Text.Data, Text.Size);
	}
	*pCursor += Text.Size;
	if ( bTerminate ) {
		sTarget[Text.Size] = 0;
		(*pCursor)++;
	}
	return (xstrview){ sTarget, Text.Size };
}



/* 将临时 DFA、源元数据和字符串池打包成一个只读分配。 */
static xpattern* __xrtPatternPack(__xrt_pattern_compiler* pCompiler)
{
	size_t iLiteralSlots = 0;
	size_t iSeparatorTransitions = 0;
	size_t iAffixStates = 0;
	size_t iAffixTransitions = 0;
	size_t iReplayBytes = 0;
	size_t iCaptureCount = 0;
	size_t iStringBytes = pCompiler->Options->SeparatorCount;
	size_t iEntryOffset;
	size_t iMetadataOffset;
	size_t iStateOffset;
	size_t iAffixRootOffset;
	size_t iLiteralOffset;
	size_t iSeparatorOffset;
	size_t iAffixStateOffset;
	size_t iAffixTransitionOffset;
	size_t iReplayOffset;
	size_t iCaptureOffset;
	size_t iStringOffset;
	size_t iTotal = sizeof(xpattern);
	xpattern* pPattern;
	uint8* pBase;
	uint8* arrReplay;
	__xrt_pattern_capture* arrCapture;
	char* sStrings;
	size_t iReplayCursor = 0;
	size_t iCaptureCursor = 0;
	size_t iLiteralCursor = 0;
	size_t iSeparatorCursor = 0;
	size_t iAffixStateCursor = 0;
	size_t iAffixTransitionCursor = 0;
	size_t iStringCursor = 0;

	for ( size_t i = 0; i < pCompiler->Dfa.Count; i++ ) {
		size_t iSlots;

		if ( !__xrtPatternLiteralSlots(
			pCompiler->Dfa.States[i].LiteralCount,
			&iSlots
		) || !__xrtPatternSizeAdd(&iLiteralSlots, iSlots) ||
			 !__xrtPatternSizeAdd(
				&iSeparatorTransitions,
				pCompiler->Dfa.States[i].SeparatorCount
			) ) {
			return NULL;
		}
		for ( size_t j = 0;
			j < pCompiler->Dfa.States[i].LiteralCount; j++ ) {
			if ( !__xrtPatternSizeAdd(
				&iStringBytes,
				pCompiler->Dfa.States[i].Literals[j].Text.Size
			) ) {
				return NULL;
			}
		}
		if ( !__xrtPatternSizeAdd(
			&iAffixStates,
			pCompiler->Dfa.States[i].AffixStateCount
		) ) {
			return NULL;
		}
		for ( size_t j = 0;
			j < pCompiler->Dfa.States[i].AffixStateCount; j++ ) {
			if ( !__xrtPatternSizeAdd(
				&iAffixTransitions,
				pCompiler->Dfa.States[i].AffixStates[j].TransitionCount
			) ) {
				return NULL;
			}
		}
	}
	for ( size_t i = 0; i < pCompiler->SourceCount; i++ ) {
		const __xrt_pattern_source* pSource = pCompiler->Sources[i];

		if ( !__xrtPatternSizeAdd(&iReplayBytes, pSource->AtomCount) ||
			 !__xrtPatternSizeAdd(&iCaptureCount, pSource->CaptureCount) ||
			 !__xrtPatternSizeAdd(&iStringBytes, pSource->Pattern.Size + 1u) ) {
			return NULL;
		}
		for ( size_t j = 0; j < pSource->CaptureCount; j++ ) {
			if ( !__xrtPatternSizeAdd(
				&iStringBytes,
				pSource->Captures[j].Name.Size
			) ) {
				return NULL;
			}
		}
	}
	if ( (iLiteralSlots > UINT32_MAX) ||
		 (iSeparatorTransitions > UINT32_MAX) ||
		 (iAffixStates > UINT32_MAX) ||
		 (iAffixTransitions > UINT32_MAX) ||
		 (pCompiler->Dfa.Count >= UINT32_MAX) ) {
		__xrtPatternSetSizeOverflow();
		return NULL;
	}
	if ( !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_entry),
		pCompiler->SourceCount,
		sizeof(__xrt_pattern_entry),
		&iEntryOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_metadata),
		pCompiler->SourceCount,
		sizeof(__xrt_pattern_metadata),
		&iMetadataOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_state),
		pCompiler->Dfa.Count,
		sizeof(__xrt_pattern_state),
		&iStateOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(uint32),
		iAffixStates != 0 ? pCompiler->Dfa.Count : 0,
		sizeof(uint32),
		&iAffixRootOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_literal_transition),
		iLiteralSlots,
		sizeof(__xrt_pattern_literal_transition),
		&iLiteralOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_separator_transition),
		iSeparatorTransitions,
		sizeof(__xrt_pattern_separator_transition),
		&iSeparatorOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_affix_state),
		iAffixStates,
		sizeof(__xrt_pattern_affix_state),
		&iAffixStateOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_affix_transition),
		iAffixTransitions,
		sizeof(__xrt_pattern_affix_transition),
		&iAffixTransitionOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		1u,
		iReplayBytes,
		sizeof(uint8),
		&iReplayOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		XRT_INTERNAL_ALIGNOF(__xrt_pattern_capture),
		iCaptureCount,
		sizeof(__xrt_pattern_capture),
		&iCaptureOffset
	) || !__xrtPatternPackLayout(
		&iTotal,
		1u,
		iStringBytes,
		1u,
		&iStringOffset
	) ) {
		return NULL;
	}
	if ( iTotal > pCompiler->Options->MaxCompiledBytes ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			pCompiler->Operation,
			"compiled matcher exceeds its byte limit",
			false,
			0,
			false,
			0
		);
		return NULL;
	}
	pPattern = (xpattern*)xrtCalloc(1u, iTotal);
	if ( pPattern == NULL ) {
		return NULL;
	}
	pBase = (uint8*)pPattern;
	pPattern->RefCount = 1;
	pPattern->CompiledBytes = iTotal;
	pPattern->Count = pCompiler->SourceCount;
	pPattern->SeparatorKind = pCompiler->Options->SeparatorKind;
	pPattern->StateCount = (uint32)pCompiler->Dfa.Count;
	memcpy(
		pPattern->SeparatorMask,
		pCompiler->Options->SeparatorMask,
		sizeof(pPattern->SeparatorMask)
	);
	pPattern->Entries = (__xrt_pattern_entry*)(pBase + iEntryOffset);
	pPattern->Metadata = (__xrt_pattern_metadata*)(pBase + iMetadataOffset);
	pPattern->States = (__xrt_pattern_state*)(pBase + iStateOffset);
	if ( iAffixStates != 0 ) {
		pPattern->AffixRoots = (uint32*)(pBase + iAffixRootOffset);
	}
	pPattern->Literals =
		(__xrt_pattern_literal_transition*)(pBase + iLiteralOffset);
	pPattern->SeparatorTransitions =
		(__xrt_pattern_separator_transition*)(pBase + iSeparatorOffset);
	pPattern->AffixStates =
		(__xrt_pattern_affix_state*)(pBase + iAffixStateOffset);
	pPattern->AffixTransitions =
		(__xrt_pattern_affix_transition*)(pBase + iAffixTransitionOffset);
	arrReplay = pBase + iReplayOffset;
	arrCapture = (__xrt_pattern_capture*)(pBase + iCaptureOffset);
	sStrings = (char*)(pBase + iStringOffset);
	if ( pCompiler->Options->SeparatorCount != 0 ) {
		pPattern->Separators = (xstrview){
			sStrings,
			pCompiler->Options->SeparatorCount
		};
		memcpy(
			sStrings,
			pCompiler->Options->Separators,
			pCompiler->Options->SeparatorCount
		);
		iStringCursor += pCompiler->Options->SeparatorCount;
	}
	for ( size_t i = 0; i < pCompiler->SourceCount; i++ ) {
		const __xrt_pattern_source* pSource = pCompiler->Sources[i];
		__xrt_pattern_entry* pEntry = &pPattern->Entries[i];
		__xrt_pattern_metadata* pMetadata = &pPattern->Metadata[i];

		pEntry->Id = pSource->Id;
		pEntry->Rank = pCompiler->Ranks[i];
		pEntry->Value = pSource->Value;
		pMetadata->Source = __xrtPatternPackText(
			sStrings,
			&iStringCursor,
			pSource->Pattern,
			true
		);
		pMetadata->Replay = arrReplay + iReplayCursor;
		pMetadata->ReplayCount = (uint32)pSource->AtomCount;
		for ( size_t j = 0; j < pSource->AtomCount; j++ ) {
			const __xrt_pattern_atom* pFrom = &pSource->Atoms[j];

			arrReplay[iReplayCursor++] = pFrom->Kind;
		}
		pMetadata->Captures = arrCapture + iCaptureCursor;
		pEntry->CaptureCount = (uint32)pSource->CaptureCount;
		if ( pSource->CaptureCount > pPattern->MaxCaptureCount ) {
			pPattern->MaxCaptureCount = pSource->CaptureCount;
		}
		for ( size_t j = 0; j < pSource->CaptureCount; j++ ) {
			__xrt_pattern_capture* pTo = &arrCapture[iCaptureCursor++];

			*pTo = pSource->Captures[j];
			pTo->Name = __xrtPatternPackText(
				sStrings,
				&iStringCursor,
				pSource->Captures[j].Name,
				false
			);
		}
	}
	for ( size_t i = 0; i < pCompiler->Dfa.Count; i++ ) {
		const __xrt_pattern_temp_state* pFrom = &pCompiler->Dfa.States[i];
		__xrt_pattern_state* pTo = &pPattern->States[i];
		size_t iSlots = 0;

		(void)__xrtPatternLiteralSlots(pFrom->LiteralCount, &iSlots);
		pTo->LiteralOffset = (uint32)iLiteralCursor;
		pTo->LiteralCount = (uint32)pFrom->LiteralCount;
		pTo->LiteralSlots = (uint32)iSlots;
		pTo->SeparatorOffset = (uint32)iSeparatorCursor;
		pTo->SeparatorCount = (uint16)pFrom->SeparatorCount;
		if ( pPattern->AffixRoots != NULL ) {
			pPattern->AffixRoots[i] = pFrom->AffixStateCount != 0 ?
				(uint32)iAffixStateCursor + 1u : 0;
		}
		pTo->DefaultPlusOne = pFrom->Default != __XRT_PATTERN_INDEX_NONE ?
			pFrom->Default + 1u : 0;
		pTo->TerminalPlusOne = pFrom->Terminal != __XRT_PATTERN_INDEX_NONE ?
			pFrom->Terminal + 1u : 0;
		pTo->TailPlusOne = pFrom->Tail != __XRT_PATTERN_INDEX_NONE ?
			pFrom->Tail + 1u : 0;
		if ( pFrom->LiteralCount == 0 ) {
			pTo->DispatchKind = __XRT_PATTERN_DISPATCH_NONE;
		} else if ( pFrom->LiteralCount <= __XRT_PATTERN_LINEAR_MAX ) {
			pTo->DispatchKind = __XRT_PATTERN_DISPATCH_LINEAR;
			for ( size_t j = 0; j < pFrom->LiteralCount; j++ ) {
				__xrt_pattern_literal_transition* pTransition =
					&pPattern->Literals[iLiteralCursor++];
				xstrview Text = __xrtPatternPackText(
					sStrings,
					&iStringCursor,
					pFrom->Literals[j].Text,
					false
				);

				pTransition->Data = Text.Data;
				pTransition->Size = (uint32)Text.Size;
				pTransition->NextPlusOne = pFrom->Literals[j].Next + 1u;
			}
		} else {
			pTo->DispatchKind = __XRT_PATTERN_DISPATCH_HASH;
			for ( size_t j = 0; j < pFrom->LiteralCount; j++ ) {
				uint64 iHash = __xrtPatternHashField(
					pFrom->Literals[j].Text.Data,
					pFrom->Literals[j].Text.Size,
					(uint32)i
				);
				size_t iSlot = (size_t)(iHash & (iSlots - 1u));
				__xrt_pattern_literal_transition* pTransition;

				while ( pPattern->Literals[
					pTo->LiteralOffset + iSlot
				].NextPlusOne != 0 ) {
					iSlot = (iSlot + 1u) & (iSlots - 1u);
				}
				pTransition = &pPattern->Literals[pTo->LiteralOffset + iSlot];
				{
					xstrview Text = __xrtPatternPackText(
						sStrings,
						&iStringCursor,
						pFrom->Literals[j].Text,
						false
					);

					pTransition->Data = Text.Data;
					pTransition->Size = (uint32)Text.Size;
				}
				pTransition->NextPlusOne = pFrom->Literals[j].Next + 1u;
			}
			iLiteralCursor += iSlots;
		}
		for ( size_t j = 0; j < pFrom->SeparatorCount; j++ ) {
			__xrt_pattern_separator_transition* pTransition =
				&pPattern->SeparatorTransitions[iSeparatorCursor++];

			pTransition->Byte = pFrom->Separators[j].Byte;
			pTransition->NextPlusOne = pFrom->Separators[j].Next + 1u;
		}
		if ( pFrom->AffixStateCount != 0 ) {
			size_t iAffixBase = iAffixStateCursor;

			for ( size_t j = 0; j < pFrom->AffixStateCount; j++ ) {
				const __xrt_pattern_temp_affix_state* pAffixFrom =
					&pFrom->AffixStates[j];
				__xrt_pattern_affix_state* pAffixTo =
					&pPattern->AffixStates[iAffixStateCursor++];

				pAffixTo->TransitionOffset = (uint32)iAffixTransitionCursor;
				pAffixTo->TransitionCount =
					(uint16)pAffixFrom->TransitionCount;
				pAffixTo->DefaultPlusOne =
					pAffixFrom->Default != __XRT_PATTERN_INDEX_NONE ?
					(uint32)iAffixBase + pAffixFrom->Default + 1u : 0;
				pAffixTo->OutputPlusOne =
					pAffixFrom->Output != __XRT_PATTERN_INDEX_NONE ?
					pAffixFrom->Output + 1u : 0;
				for ( size_t k = 0;
					k < pAffixFrom->TransitionCount; k++ ) {
					__xrt_pattern_affix_transition* pTransition =
						&pPattern->AffixTransitions[iAffixTransitionCursor++];

					pTransition->Byte = pAffixFrom->Transitions[k].Byte;
					pTransition->NextPlusOne = (uint32)iAffixBase +
						pAffixFrom->Transitions[k].Next + 1u;
				}
			}
		}
	}
	if ( (iReplayCursor != iReplayBytes) ||
		 (iCaptureCursor != iCaptureCount) ||
		 (iLiteralCursor != iLiteralSlots) ||
		 (iSeparatorCursor != iSeparatorTransitions) ||
		 (iAffixStateCursor != iAffixStates) ||
		 (iAffixTransitionCursor != iAffixTransitions) ||
		 (iStringCursor != iStringBytes) ) {
		xrtFree(pPattern);
		__xrtPatternSetInternal();
		return NULL;
	}
	return pPattern;
}



/* 释放编译过程的全部临时状态。 */
static void __xrtPatternCompilerFree(__xrt_pattern_compiler* pCompiler)
{
	xrtFree(pCompiler->Ranks);
	__xrtPatternNfaFree(&pCompiler->Nfa);
	__xrtPatternDfaFree(&pCompiler->Dfa);
}



xpattern* __xrtPatternCompileSources(
	__xrt_pattern_source* const* arrSource,
	size_t iCount,
	const __xrt_pattern_options* pOptions,
	cstr sOperation
)
{
	__xrt_pattern_compiler Compiler;
	xpattern* pPattern = NULL;
	uint32 iRoot;

	if ( (pOptions == NULL) || ((arrSource == NULL) && (iCount != 0)) ||
		 (iCount > pOptions->MaxPatterns) || (iCount >= UINT32_MAX) ) {
		__xrtPatternSetInvalidArgument();
		return NULL;
	}
	memset(&Compiler, 0, sizeof(Compiler));
	Compiler.Sources = arrSource;
	Compiler.SourceCount = iCount;
	Compiler.Options = pOptions;
	Compiler.Operation = sOperation;
	if ( !__xrtPatternRanks(&Compiler) ||
		 !__xrtPatternNfaNode(&Compiler, &iRoot) || (iRoot != 0) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtPatternNfaInsert(&Compiler, (uint32)i) ) {
			goto cleanup;
		}
	}
	if ( !__xrtPatternDfaBuild(&Compiler) ) {
		goto cleanup;
	}
	pPattern = __xrtPatternPack(&Compiler);

cleanup:
	__xrtPatternCompilerFree(&Compiler);
	return pPattern;
}

#endif
