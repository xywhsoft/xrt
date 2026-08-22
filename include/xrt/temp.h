#ifndef XRT_TEMP_H
#define XRT_TEMP_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_TEMP_MEMORY)

#define XRT_TEMP_BLOCK_SIZE_DEFAULT	4096u
#define XRT_TEMP_SPILL_LIMIT_DEFAULT	2048u
#define XRT_TEMP_RETAIN_LIMIT_DEFAULT	65536u



typedef struct xtempblock xtempblock;



/* 临时内存配置控制常规块、独立大块和重置后的保留上限。 */
typedef struct xtempconfig {
	size_t BlockSize;
	size_t SpillLimit;
	size_t RetainLimit;
} xtempconfig;



/* arena 可放在栈、对象或协程上下文中，不允许并发操作。 */
typedef struct xtemparena {
	xtempblock* Blocks;
	xtempblock* Current;
	xtempblock* Tail;
	xtempblock* Spill;
	size_t BlockSize;
	size_t SpillLimit;
	size_t RetainLimit;
	size_t RetainedBytes;
	size_t CurrentBytes;
	size_t PeakBytes;
	uint64 ResetCount;
	uint64 ScopeSerial;
	uint64 ActiveScopeId;
	uint32 ScopeDepth;
	uint32 Flags;
} xtemparena;



/* mark 保存严格后进先出的 arena 回退位置。 */
typedef struct xtempmark {
	xtemparena* Arena;
	xtempblock* Current;
	xtempblock* Spill;
	size_t Used;
	size_t CurrentBytes;
	uint64 Id;
	uint64 ParentId;
	uint32 Depth;
	bool Active;
} xtempmark;



/* 临时内存信息用于诊断保留量和作用域状态。 */
typedef struct xtempinfo {
	size_t BlockCount;
	size_t SpillCount;
	size_t RetainedBytes;
	size_t CurrentBytes;
	size_t PeakBytes;
	uint64 ResetCount;
	uint32 ScopeDepth;
} xtempinfo;



XRT_EXTERN_C_BEGIN



/* 使用默认或指定配置初始化一个空 arena。 */
XRT_API bool xrtTempInit(xtemparena* pArena, const xtempconfig* pConfig);



/* 释放 arena 持有的全部常规块和 spill 块。 */
XRT_API void xrtTempUnit(xtemparena* pArena);



/* 从指定 arena 分配一段 16 字节对齐的临时内存。 */
XRT_API ptr xrtTempAlloc(xtemparena* pArena, size_t iSize);



/* 把二进制数据复制到指定 arena。 */
XRT_API ptr xrtTempDup(xtemparena* pArena, const void* pData, size_t iSize);



/* 把字符串视图复制为指定 arena 中的零结尾字符串。 */
XRT_API str xrtTempStr(xtemparena* pArena, xstrview Text);



/* 回收全部临时分配并保留配置允许的常规块。 */
XRT_API bool xrtTempReset(xtemparena* pArena);



/* 安全擦除 arena 持有的全部用户区，再执行普通重置。 */
XRT_API bool xrtTempSecureReset(xtemparena* pArena);



/* 安全擦除 arena 持有的全部用户区，再释放所有内存。 */
XRT_API void xrtTempSecureUnit(xtemparena* pArena);



/* 在 arena 空闲时将常规块缩减到指定保留字节数。 */
XRT_API bool xrtTempTrim(xtemparena* pArena, size_t iRetainBytes);



/* 建立一个必须后进先出结束的临时作用域。 */
XRT_API xtempmark xrtTempBegin(xtemparena* pArena);



/* 回退作用域内产生的临时分配。 */
XRT_API bool xrtTempEnd(xtempmark* pMark);



/* 结束作用域并把二进制结果复制到父作用域。 */
XRT_API ptr xrtTempEndDup(xtempmark* pMark, const void* pData, size_t iSize);



/* 结束作用域并把字符串结果复制到父作用域。 */
XRT_API str xrtTempEndStr(xtempmark* pMark, xstrview Text);



/* 获取 arena 当前状态。 */
XRT_API void xrtTempGet(const xtemparena* pArena, xtempinfo* pInfo);



/* 返回当前原生线程或协程绑定的默认 arena。 */
XRT_API xtemparena* xrtTempCurrent(void);



/* 从当前执行上下文的默认 arena 分配临时内存。 */
XRT_API ptr xrtTemp(size_t iSize);



/* 重置当前执行上下文的默认 arena。 */
XRT_API bool xrtTempClear(void);



XRT_EXTERN_C_END

#endif

#endif
