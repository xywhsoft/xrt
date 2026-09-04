#ifndef XRT_INTERNAL_PATTERN_H
#define XRT_INTERNAL_PATTERN_H

#include <xrt/pattern.h>
#include <xrt/memory.h>

#include "xrt_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#if defined(XRT_FEATURE_PATTERN)

#define __XRT_PATTERN_INDEX_NONE UINT32_MAX
#define __XRT_PATTERN_SLOT_NONE UINT32_MAX
#define __XRT_PATTERN_LINEAR_MAX 4u



typedef enum __xrt_pattern_atom_kind {
	__XRT_PATTERN_ATOM_SEPARATOR = 1,
	__XRT_PATTERN_ATOM_LITERAL = 2,
	__XRT_PATTERN_ATOM_CAPTURE = 3,
	__XRT_PATTERN_ATOM_AFFIX = 4,
	__XRT_PATTERN_ATOM_TAIL = 5
} __xrt_pattern_atom_kind;



typedef enum __xrt_pattern_separator_kind {
	__XRT_PATTERN_SEPARATOR_NONE = 0,
	__XRT_PATTERN_SEPARATOR_SLASH,
	__XRT_PATTERN_SEPARATOR_BYTE,
	__XRT_PATTERN_SEPARATOR_TWO,
	__XRT_PATTERN_SEPARATOR_THREE,
	__XRT_PATTERN_SEPARATOR_FOUR,
	__XRT_PATTERN_SEPARATOR_BITMAP
} __xrt_pattern_separator_kind;



typedef enum __xrt_pattern_dispatch_kind {
	__XRT_PATTERN_DISPATCH_NONE = 0,
	__XRT_PATTERN_DISPATCH_LINEAR,
	__XRT_PATTERN_DISPATCH_HASH
} __xrt_pattern_dispatch_kind;



/* 编译配置的自包含规范化形式，不再借用调用方分隔符视图。 */
typedef struct __xrt_pattern_options {
	uint64 SeparatorMask[4];
	uint8 Separators[256];
	size_t SeparatorCount;
	__xrt_pattern_separator_kind SeparatorKind;
	size_t MaxPatternBytes;
	size_t MaxPatterns;
	size_t MaxCaptures;
	size_t MaxStates;
	size_t MaxCompiledBytes;
} __xrt_pattern_options;



/* Builder 与编译快照共用的已解析原子语义。 */
typedef struct __xrt_pattern_atom {
	xstrview Text;
	xstrview Suffix;
	uint32 CaptureIndex;
	uint8 Kind;
	uint8 Byte;
	uint16 Reserved;
} __xrt_pattern_atom;



typedef struct __xrt_pattern_capture {
	xstrview Name;
	uint32 FieldIndex;
	uint32 PrefixSize;
	uint32 SuffixSize;
	uint8 Kind;
	uint8 Reserved[3];
} __xrt_pattern_capture;



/* 每条 Builder 源模式使用单块存储并缓存解析结果。 */
typedef struct __xrt_pattern_source {
	xpatternid Id;
	uint64 Order;
	ptr Value;
	int32 Priority;
	uint32 Flags;
	xstrview Pattern;
	__xrt_pattern_atom* Atoms;
	size_t AtomCount;
	__xrt_pattern_capture* Captures;
	size_t CaptureCount;
	size_t StorageBytes;
} __xrt_pattern_source;



typedef struct __xrt_pattern_entry {
	xpatternid Id;
	ptr Value;
	uint32 Rank;
	uint32 CaptureCount;
} __xrt_pattern_entry;



/* 源文本、捕获名和重放程序只在冷路径访问。 */
typedef struct __xrt_pattern_metadata {
	xstrview Source;
	const uint8* Replay;
	__xrt_pattern_capture* Captures;
	uint32 ReplayCount;
	uint32 Reserved;
} __xrt_pattern_metadata;



/* Literal transition uses NextPlusOne==0 as an empty hash slot. */
typedef struct __xrt_pattern_literal_transition {
	const char* Data;
	uint32 Size;
	uint32 NextPlusOne;
} __xrt_pattern_literal_transition;



typedef struct __xrt_pattern_separator_transition {
	uint32 NextPlusOne;
	uint8 Byte;
	uint8 Reserved[3];
} __xrt_pattern_separator_transition;



/* 混合字段的局部字节 DFA 使用稀疏覆盖加默认转移。 */
typedef struct __xrt_pattern_affix_transition {
	uint32 NextPlusOne;
	uint8 Byte;
	uint8 Reserved[3];
} __xrt_pattern_affix_transition;



typedef struct __xrt_pattern_affix_state {
	uint32 TransitionOffset;
	uint32 DefaultPlusOne;
	uint32 OutputPlusOne;
	uint16 TransitionCount;
	uint16 Reserved;
} __xrt_pattern_affix_state;



typedef struct __xrt_pattern_state {
	uint32 LiteralOffset;
	uint32 LiteralCount;
	uint32 LiteralSlots;
	uint32 SeparatorOffset;
	uint32 DefaultPlusOne;
	uint32 TerminalPlusOne;
	uint32 TailPlusOne;
	uint16 SeparatorCount;
	uint8 DispatchKind;
	uint8 Reserved;
} __xrt_pattern_state;



/* 全部指针都指向 pPattern 自身单块分配中的只读区域。 */
struct xpattern {
	volatile int32 RefCount;
	uint32 Reserved;
	size_t CompiledBytes;
	size_t Count;
	size_t MaxCaptureCount;
	xstrview Separators;
	uint64 SeparatorMask[4];
	__xrt_pattern_separator_kind SeparatorKind;
	uint32 StateCount;
	__xrt_pattern_entry* Entries;
	__xrt_pattern_metadata* Metadata;
	__xrt_pattern_state* States;
	uint32* AffixRoots;
	__xrt_pattern_literal_transition* Literals;
	__xrt_pattern_separator_transition* SeparatorTransitions;
	__xrt_pattern_affix_state* AffixStates;
	__xrt_pattern_affix_transition* AffixTransitions;
};



typedef struct __xrt_pattern_builder_slot {
	__xrt_pattern_source* Source;
	uint32 Generation;
	uint32 NextFree;
} __xrt_pattern_builder_slot;



struct xpatternbuilder {
	__xrt_pattern_options Options;
	__xrt_pattern_builder_slot* Slots;
	size_t SlotCount;
	size_t SlotCapacity;
	size_t Count;
	uint32 FreeSlot;
	uint64 NextOrder;
	uint64 Version;
	uint64 CompiledVersion;
	xpattern* Cached;
};



#define __xrtPatternSetInvalidArgument() \
	xrtSetErrorInfo(XERR_ARGUMENT, "xrt.pattern", 0, "invalid argument")
#define __xrtPatternSetOutOfMemory() \
	xrtSetErrorInfo(XERR_MEMORY, "xrt.pattern", 0, "out of memory")
#define __xrtPatternSetSizeOverflow() \
	xrtSetErrorInfo(XERR_RANGE, "xrt.pattern", 0, "size overflow")
#define __xrtPatternSetRange() \
	xrtSetErrorInfo(XERR_RANGE, "xrt.pattern", 0, "value out of range")
#define __xrtPatternSetInvalidState() \
	xrtSetErrorInfo(XERR_STATE, "xrt.pattern", 0, "invalid state")
#define __xrtPatternSetInternal() \
	xrtSetErrorInfo(XERR_INTERNAL, "xrt.pattern", 0, "internal error")



/* 设置带稳定代码以及可选模式索引、字节位置的模块错误。 */
void __xrtPatternError(
	xerrkind Kind,
	xpatternerror Code,
	cstr sOperation,
	cstr sMessage,
	bool bHasPattern,
	size_t iPattern,
	bool bHasOffset,
	size_t iOffset
);



/* 验证字符串视图的指针和大小组合。 */
bool __xrtPatternViewValid(xstrview Text);



/* 验证并复制配置，同时建立 O(1) 分隔字节位图。 */
bool __xrtPatternOptionsInit(
	const xpatternconfig* pConfig,
	__xrt_pattern_options* pOptions,
	cstr sOperation
);



/* 返回一个字节是否属于规范化分隔符集合。 */
static inline bool __xrtPatternIsSeparator(
	const __xrt_pattern_options* pOptions,
	uint8 iByte
)
{
	return (
		pOptions->SeparatorMask[iByte >> 6u] &
		(UINT64_C(1) << (iByte & 63u))
	) != 0;
}



/* 高扇出状态使用的快速、带状态盐的字段哈希。 */
static inline uint64 __xrtPatternHashMix(uint64 iValue)
{
	iValue ^= iValue >> 30u;
	iValue *= UINT64_C(0xbf58476d1ce4e5b9);
	iValue ^= iValue >> 27u;
	iValue *= UINT64_C(0x94d049bb133111eb);
	iValue ^= iValue >> 31u;
	return iValue;
}



static inline uint64 __xrtPatternHashField(
	const char* sData,
	size_t iSize,
	uint32 iState
)
{
	uint64 iHash =
		UINT64_C(0x9e3779b97f4a7c15) ^
		((uint64)iState * UINT64_C(0xd6e8feb86659fd93)) ^
		((uint64)iSize * UINT64_C(0xa0761d6478bd642f));
	size_t i = 0;

	while ( (iSize - i) >= 8u ) {
		uint64 iWord;

		memcpy(&iWord, sData + i, sizeof(iWord));
		iHash ^= __xrtPatternHashMix(iWord + (uint64)i);
		iHash = (iHash << 27u) | (iHash >> 37u);
		iHash *= UINT64_C(0x3c79ac492ba7b653);
		i += 8u;
	}
	while ( i < iSize ) {
		iHash ^= (uint64)(uint8)sData[i];
		iHash *= UINT64_C(0x100000001b3);
		i++;
	}
	return __xrtPatternHashMix(iHash);
}



/* 创建一条自包含、已解析的 Builder 源模式。 */
__xrt_pattern_source* __xrtPatternSourceCreate(
	const xpatternspec* pSpec,
	const __xrt_pattern_options* pOptions,
	cstr sOperation,
	xpatternid Id,
	uint64 iOrder,
	bool bHasPatternIndex,
	size_t iPatternIndex
);



/* 释放源模式的单块存储。 */
void __xrtPatternSourceFree(__xrt_pattern_source* pSource);



/* 把一组已解析源模式编译成不可变单块快照。 */
xpattern* __xrtPatternCompileSources(
	__xrt_pattern_source* const* arrSource,
	size_t iCount,
	const __xrt_pattern_options* pOptions,
	cstr sOperation
);



/* 清理输出并执行不产生捕获的确定性匹配。 */
uint32 __xrtPatternSelect(const xpattern* pPattern, xstrview Text);



/* 按获胜模式重放输入并输出顺序捕获；编译器保证该模式必然匹配。 */
bool __xrtPatternCaptureReplay(
	const xpattern* pPattern,
	const __xrt_pattern_entry* pEntry,
	const __xrt_pattern_metadata* pMetadata,
	xstrview Text,
	xstrview* arrCapture
);

#endif

#endif
