/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xws_group requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XWS_GROUP_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XWS_GROUP_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XWS_GROUP_SINGLE_HEADER_H
#define XWS_GROUP_SINGLE_HEADER_H
#define XWS_GROUP_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xws-group/include/xws-group/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XWS_GROUP_FEATURES_H
#define XWS_GROUP_FEATURES_H

/* websocket_group_future 及其直接依赖。 */
#if defined(XWS_GROUP_MODULE_ALL) || defined(XWS_GROUP_MODULE_WEBSOCKET_GROUP_FUTURE)
#ifndef XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE
#define XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE
#endif
#ifndef XWS_GROUP_MODULE_WEBSOCKET_GROUP
#define XWS_GROUP_MODULE_WEBSOCKET_GROUP
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_REF_FUTURE
#define XWS_MODULE_WEBSOCKET_CONNECTION_REF_FUTURE
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* websocket_group 及其直接依赖。 */
#if defined(XWS_GROUP_MODULE_ALL) || defined(XWS_GROUP_MODULE_WEBSOCKET_GROUP)
#ifndef XWS_GROUP_FEATURE_WEBSOCKET_GROUP
#define XWS_GROUP_FEATURE_WEBSOCKET_GROUP
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#ifndef XRT_MODULE_SET
#define XRT_MODULE_SET
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

#endif /* XWS_GROUP_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xws-group/include/xws-group/export.h */
/* ========================================================================== */

#ifndef XWS_GROUP_EXPORT_H
#define XWS_GROUP_EXPORT_H


/* 独立动态库只导出本库接口，依赖由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XWS_GROUP_BUILD_SHARED)
  #define XWS_GROUP_API __declspec(dllexport)
 #elif defined(XWS_GROUP_USE_SHARED)
  #define XWS_GROUP_API __declspec(dllimport)
 #else
  #define XWS_GROUP_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XWS_GROUP_BUILD_SHARED)
  #define XWS_GROUP_API __attribute__((visibility("default")))
 #else
  #define XWS_GROUP_API
 #endif
#else
 #define XWS_GROUP_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xws-group/include/xrt/websocket_group.h */
/* ========================================================================== */

#ifndef XRT_WEBSOCKET_GROUP_H
#define XRT_WEBSOCKET_GROUP_H




#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_SET) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT WebSocket group requires connection, set and mutex"
#endif

#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE) && \
	(!defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE))
	#error "XRT WebSocket group Future requires group, connection Future, reference Future and Future"
#endif



#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP)

/* 连接组错误用于区分参数、内存、状态、容量和索引边界。 */
typedef enum xwsgrouperror {
	XWS_GROUP_ERROR_ARGUMENT = 1,
	XWS_GROUP_ERROR_MEMORY,
	XWS_GROUP_ERROR_STATE,
	XWS_GROUP_ERROR_CAPACITY,
	XWS_GROUP_ERROR_RANGE
} xwsgrouperror;



/* 连接组并发持有不重复的 Connection 引用。 */
typedef struct xwsgroup xwsgroup;



/* 快照独立持有创建时的全部 Connection 引用。 */
typedef struct xwsgroupsnapshot xwsgroupsnapshot;

#endif



#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)

/* 批量操作槽位区分同步拒绝、等待和五种 Future 终态。 */
typedef enum xwsgroupopstate {
	XWS_GROUP_OP_REJECTED = 0,
	XWS_GROUP_OP_PENDING,
	XWS_GROUP_OP_RESOLVED,
	XWS_GROUP_OP_FAILED,
	XWS_GROUP_OP_CANCELLED,
	XWS_GROUP_OP_CLOSED
} xwsgroupopstate;



/* 槽位结果借用操作对象持有的 Connection 和错误。 */
typedef struct xwsgroupopresult {
	xwsgroupopstate State;
	xwsconn* Connection;
	const xerror* Error;
} xwsgroupopresult;



/* 批量操作保存稳定成员快照、逐成员 Future 和同步拒绝原因。 */
typedef struct xwsgroupop xwsgroupop;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP)

/* 创建连接组；Limit 为零表示不设置成员数量上限。 */
XWS_GROUP_API xwsgroup* xrtWsGroupCreate(size_t iLimit);



/* 增加连接组引用并返回原指针。 */
XWS_GROUP_API xwsgroup* xrtWsGroupRef(xwsgroup* pGroup);



/* 释放连接组引用；最后一个引用只释放成员，不关闭连接。 */
XWS_GROUP_API void xrtWsGroupDestroy(xwsgroup* pGroup);



/* 加入并持有 Connection；重复加入成功且不增加第二个引用。 */
XWS_GROUP_API bool xrtWsGroupAdd(xwsgroup* pGroup, xwsconn* pConnection);



/* 移除并释放 Connection；成员不存在时正常返回 false。 */
XWS_GROUP_API bool xrtWsGroupRemove(xwsgroup* pGroup, xwsconn* pConnection);



/* 判断 Connection 是否属于当前组。 */
XWS_GROUP_API bool xrtWsGroupHas(const xwsgroup* pGroup, const xwsconn* pConnection);



/* 返回当前成员数量。 */
XWS_GROUP_API size_t xrtWsGroupCount(const xwsgroup* pGroup);



/* 返回创建时设置的硬上限；零表示没有显式上限。 */
XWS_GROUP_API size_t xrtWsGroupLimit(const xwsgroup* pGroup);



/* 永久阻止新成员加入；重复调用保持成功。 */
XWS_GROUP_API bool xrtWsGroupSeal(xwsgroup* pGroup);



/* 返回连接组是否已经封闭。 */
XWS_GROUP_API bool xrtWsGroupSealed(const xwsgroup* pGroup);



/* 原子移走全部成员并在组锁外释放，返回数量且不重新开放已封闭的组。 */
XWS_GROUP_API size_t xrtWsGroupClear(xwsgroup* pGroup);



/* 在组锁外分配并创建保序成员快照；空组也返回有效空快照。 */
XWS_GROUP_API xwsgroupsnapshot* xrtWsGroupSnapshotCreate(const xwsgroup* pGroup);



/* 返回快照成员数量。 */
XWS_GROUP_API size_t xrtWsGroupSnapshotCount(const xwsgroupsnapshot* pSnapshot);



/* 借用指定快照成员；借用期不超过快照生命周期。 */
XWS_GROUP_API xwsconn* xrtWsGroupSnapshotGet(
	const xwsgroupsnapshot* pSnapshot,
	size_t iIndex
);



/* 释放快照和它持有的全部 Connection 引用。 */
XWS_GROUP_API void xrtWsGroupSnapshotDestroy(xwsgroupsnapshot* pSnapshot);

#endif



XRT_EXTERN_C_END



#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)

XRT_EXTERN_C_BEGIN



/* 对调用时的稳定成员快照异步提交一条 Text 或 Binary 消息；负载只复制一次。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupSendAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 对调用时的稳定成员快照异步提交 UTF-8 Text。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupTextAsync(
	xwsgroup* pGroup,
	xstrview Text
);



/* 对调用时的稳定成员快照异步提交 Binary。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupBinaryAsync(
	xwsgroup* pGroup,
	xbytesview Data
);



/*
	异步提交共享所有权消息；仅成功返回操作对象时接管 Ref。
	每个已接纳成员持有独立共享引用，最后一个引用恰好调用一次 Release。
	Ref 结构允许未对齐；结构与负载必须是完整范围且不能覆盖 Group。
*/
XWS_GROUP_API xwsgroupop* xrtWsGroupSendRefAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 异步提交共享所有权 UTF-8 Text。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupTextRefAsync(
	xwsgroup* pGroup,
	const xnetref* pRef
);



/* 异步提交共享所有权 Binary。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupBinaryRefAsync(
	xwsgroup* pGroup,
	const xnetref* pRef
);



/* 对调用时的稳定成员快照异步提交 Ping。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupPingAsync(
	xwsgroup* pGroup,
	xbytesview Payload
);



/* 对调用时的稳定成员快照异步提交 Pong。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupPongAsync(
	xwsgroup* pGroup,
	xbytesview Payload
);



/* 对调用时的稳定成员快照异步提交唯一 Close。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupCloseAsync(
	xwsgroup* pGroup,
	uint16 iCode,
	xstrview Reason
);



/* 对调用时的稳定成员快照异步等待同一种 Connection 条件。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupWaitAsync(
	xwsgroup* pGroup,
	xwsconnwait Wait
);



/* 增加批量操作引用并返回原指针。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupOpRef(xwsgroupop* pOperation);



/* 释放批量操作引用；已提交的 Connection 操作不会因此取消。 */
XWS_GROUP_API void xrtWsGroupOpDestroy(xwsgroupop* pOperation);



/* 返回稳定成员槽位总数。 */
XWS_GROUP_API size_t xrtWsGroupOpCount(const xwsgroupop* pOperation);



/* 返回成功创建逐成员 Future 的槽位数。 */
XWS_GROUP_API size_t xrtWsGroupOpAccepted(const xwsgroupop* pOperation);



/* 返回提交阶段同步拒绝的槽位数。 */
XWS_GROUP_API size_t xrtWsGroupOpRejected(const xwsgroupop* pOperation);



/* 以 O(1) 并发快照返回同步拒绝或已经进入 Future 终态的槽位数。 */
XWS_GROUP_API size_t xrtWsGroupOpDoneCount(const xwsgroupop* pOperation);



/* 读取指定槽位；输出允许未对齐，但必须完整且不能覆盖 Operation。 */
XWS_GROUP_API bool xrtWsGroupOpResult(
	const xwsgroupop* pOperation,
	size_t iIndex,
	xwsgroupopresult* pResult
);



/* 返回增加引用后的逐成员 Future；同步拒绝槽位返回空指针。 */
XWS_GROUP_API xfuture* xrtWsGroupOpItemFutureRef(
	const xwsgroupop* pOperation,
	size_t iIndex
);



/*
	返回增加引用后的完成 Future；正常时在全部已接纳操作进入终态后成功完成。
	取消该 Future 会立即取消聚合等待，并向仍未结束的逐成员 Future 传播取消请求。
*/
XWS_GROUP_API xfuture* xrtWsGroupOpFutureRef(const xwsgroupop* pOperation);



/* 向全部已接纳且尚未结束的逐成员 Future 请求协作取消，并返回成功请求数。 */
XWS_GROUP_API size_t xrtWsGroupOpCancel(xwsgroupop* pOperation);



/* 等待全部已接纳操作进入终态。 */
XWS_GROUP_API xwaitresult xrtWsGroupOpWait(xwsgroupop* pOperation);



/* 在相对毫秒数内等待全部已接纳操作进入终态。 */
XWS_GROUP_API xwaitresult xrtWsGroupOpWaitFor(
	xwsgroupop* pOperation,
	int64 iTimeout
);



/* 等待全部已接纳操作到指定单调时钟截止时间。 */




/* 等待批量操作、截止时间或调用方取消令牌中的首个事件。 */
XWS_GROUP_API xwaitresult xrtWsGroupOpWaitForCancel(
	xwsgroupop* pOperation,
	int64 iTimeout,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xws-group/include/xws-group.h */
/* ========================================================================== */

#ifndef XWS_GROUP_H
#define XWS_GROUP_H


#endif

#endif

#if defined(XWS_GROUP_IMPLEMENTATION) && !defined(XWS_GROUP_IMPLEMENTATION_ONCE)
#define XWS_GROUP_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xws-group/src/internal/xrt_websocket_group.h */
/* ========================================================================== */

#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP)
#ifndef XRT_INTERNAL_WEBSOCKET_GROUP_H
#define XRT_INTERNAL_WEBSOCKET_GROUP_H

#include <string.h>
/* 构建并安装扩展库自己的结构化错误。 */
static inline void __xwsGroupErrorSetDetail(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 取走当前原因链，并用扩展库稳定域包装后重新安装。 */
static inline void __xwsGroupErrorWrapDetail(
	xerrkind DefaultKind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}






#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP)

/* 验证 Group 固定结构并设置稳定的 Group 域错误。 */
bool __xrtWsGroupCheck(
	const xwsgroup* pGroup,
	cstr sOperation
);



/* 判断一个已验证范围是否覆盖 Group 的私有固定结构。 */
bool __xrtWsGroupOverlaps(
	const xwsgroup* pGroup,
	cbytes pData,
	size_t iSize
);



/* 设置带稳定 WebSocket Group 域的结构化错误。 */
void __xrtWsGroupError(
	xerrkind Kind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
);



/* 为底层失败补充稳定的连接组操作边界。 */
void __xrtWsGroupWrap(
	xerrkind DefaultKind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xws-group/src/internal/xrt_websocket_group_future.h */
/* ========================================================================== */

#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)
#ifndef XRT_INTERNAL_WEBSOCKET_GROUP_FUTURE_H
#define XRT_INTERNAL_WEBSOCKET_GROUP_FUTURE_H




#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)

/* 成员提交函数只在创建批量操作的调用线程内借用上下文。 */
typedef xfuture* (*__xrt_ws_group_submitproc)(
	xwsconn* pConnection,
	ptr pData
);



/* 创建已经具备稳定成员快照和完成 Future 的批量操作。 */
xwsgroupop* __xrtWsGroupOpCreate(xwsgroup* pGroup);



/* 对稳定快照逐成员提交操作，并保存接纳 Future 或同步错误。 */
bool __xrtWsGroupOpSubmit(
	xwsgroupop* pOperation,
	__xrt_ws_group_submitproc pSubmit,
	ptr pData
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xws-group/include/xrt/detail/xws_group_wait.h */
/* ========================================================================== */

#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)
#ifndef XWS_GROUP_DETAIL_WAIT_H
#define XWS_GROUP_DETAIL_WAIT_H

XRT_EXTERN_C_BEGIN
#if (defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE))
XWS_GROUP_API xwaitresult __xrtWsGroupOpWaitUntil(
	xwsgroupop* pOperation,
	double iDeadline
);
#endif
#if (defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE))
XWS_GROUP_API xwaitresult __xrtWsGroupOpWaitUntilCancel(
	xwsgroupop* pOperation,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-group/src/websocket/group.c */
/* ========================================================================== */

#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP)




#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP)

/* 连接组以通用集合保存唯一成员，并用互斥锁保护全部结构状态。 */
struct xwsgroup {
	volatile int32 References;
	xmutex Lock;
	xset Connections;
	size_t Limit;
	bool Sealed;
};



/* 快照使用单次分配保存结构和保序引用数组。 */
struct xwsgroupsnapshot {
	size_t Count;
	xwsconn* Connections[];
};



/* 验证 Group 固定结构并设置稳定的 Group 域错误。 */
bool __xrtWsGroupCheck(
	const xwsgroup* pGroup,
	cstr sOperation
)
{
	if ( xrtMemRangeValid(pGroup, sizeof(*pGroup)) ) {
		return true;
	}
	__xrtWsGroupError(
		XERR_ARGUMENT,
		XWS_GROUP_ERROR_ARGUMENT,
		sOperation,
		"WebSocket group range is invalid"
	);
	return false;
}



/* 判断一个已验证范围是否覆盖 Group 的私有固定结构。 */
bool __xrtWsGroupOverlaps(
	const xwsgroup* pGroup,
	cbytes pData,
	size_t iSize
)
{
	return xrtMemRangesOverlap(
		pData,
		iSize,
		pGroup,
		sizeof(*pGroup)
	);
}



/* 设置带稳定 WebSocket Group 域的结构化错误。 */
void __xrtWsGroupError(
	xerrkind Kind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsGroupErrorSetDetail(
		Kind,
		"xrt.websocket.group",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 为底层失败补充稳定的连接组操作边界。 */
void __xrtWsGroupWrap(
	xerrkind DefaultKind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsGroupErrorWrapDetail(
		DefaultKind,
		"xrt.websocket.group",
		(int32)Code,
		sOperation,
		sMessage
	);
}



/* 以 Connection 指针值计算稳定的集合哈希。 */
static uint64 __xrtWsGroupHash(const void* pItem, ptr pData)
{
	(void)pData;
	return xrtHash64(pItem, sizeof(xwsconn*));
}



/* 比较两个 Connection 指针值。 */
static bool __xrtWsGroupEqual(
	const void* pLeft,
	const void* pRight,
	ptr pData
)
{
	(void)pData;
	return *(xwsconn* const*)pLeft == *(xwsconn* const*)pRight;
}



/* 为新成员取得一个独立 Connection 引用。 */
static bool __xrtWsGroupCopy(
	ptr pTarget,
	const void* pSource,
	ptr pData
)
{
	xwsconn* pConnection = *(xwsconn* const*)pSource;

	(void)pData;
	pConnection = xrtWsConnRef(pConnection);
	if ( pConnection == NULL ) {
		return false;
	}
	*(xwsconn**)pTarget = pConnection;
	return true;
}



/* 归还集合成员持有的 Connection 引用。 */
static void __xrtWsGroupDrop(ptr pItem, ptr pData)
{
	(void)pData;
	xrtWsConnDestroy(*(xwsconn**)pItem);
}



/* 初始化具有唯一 Connection 所有权语义的成员集合。 */
static bool __xrtWsGroupSetInit(xset* pConnections)
{
	memset(pConnections, 0, sizeof(*pConnections));
	if ( !xrtSetInit(pConnections, sizeof(xwsconn*)) ) {
		return false;
	}
	if ( !xrtSetSetKeyPolicy(
		pConnections,
		__xrtWsGroupHash,
		__xrtWsGroupEqual,
		NULL
	) || !xrtSetSetLifecycle(
		pConnections,
		__xrtWsGroupCopy,
		__xrtWsGroupDrop,
		NULL
	) ) {
		xrtSetUnit(pConnections);
		return false;
	}
	return true;
}



/* 验证快照头、计数乘法和完整连续分配区间。 */
static bool __xrtWsGroupSnapshotCheck(
	const xwsgroupsnapshot* pSnapshot,
	cstr sOperation,
	size_t* pSize
)
{
	size_t iSize;

	if ( !xrtMemRangeValid(pSnapshot, sizeof(*pSnapshot)) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			sOperation,
			"WebSocket group snapshot range is invalid"
		);
		return false;
	}
	if ( pSnapshot->Count > ((SIZE_MAX - sizeof(*pSnapshot)) /
		sizeof(xwsconn*)) ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			sOperation,
			"WebSocket group snapshot size overflows"
		);
		return false;
	}
	iSize = sizeof(*pSnapshot) +
		(pSnapshot->Count * sizeof(xwsconn*));
	if ( !xrtMemRangeValid(pSnapshot, iSize) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			sOperation,
			"WebSocket group snapshot storage is incomplete"
		);
		return false;
	}
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return true;
}



/* 在调用方持有组锁时检查成员是否存在。 */
static bool __xrtWsGroupHasLocked(
	const xwsgroup* pGroup,
	const xwsconn* pConnection
)
{
	xwsconn* pKey = (xwsconn*)pConnection;

	return xrtSetHas(&pGroup->Connections, &pKey);
}



/* 快照访问器按集合插入顺序增加 Connection 引用。 */
static bool __xrtWsGroupSnapshotAdd(const void* pItem, ptr pData)
{
	xwsgroupsnapshot* pSnapshot = (xwsgroupsnapshot*)pData;
	xwsconn* pConnection = *(xwsconn* const*)pItem;

	pConnection = xrtWsConnRef(pConnection);
	if ( pConnection == NULL ) {
		return false;
	}
	pSnapshot->Connections[pSnapshot->Count] = pConnection;
	pSnapshot->Count++;
	return true;
}



/* 创建连接组；Limit 为零表示不设置成员数量上限。 */
XWS_GROUP_API xwsgroup* xrtWsGroupCreate(size_t iLimit)
{
	xwsgroup* pGroup = (xwsgroup*)xrtCalloc(1, sizeof(*pGroup));

	if ( pGroup == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.create",
			"WebSocket group allocation failed"
		);
		return NULL;
	}
	pGroup->References = 1;
	pGroup->Limit = iLimit;
	if ( !xrtMutexInit(&pGroup->Lock) ) {
		xrtFree(pGroup);
		return NULL;
	}
	if ( !__xrtWsGroupSetInit(&pGroup->Connections) ) {
		(void)xrtMutexUnit(&pGroup->Lock);
		xrtFree(pGroup);
		return NULL;
	}
	return pGroup;
}



/* 增加连接组引用并返回原指针。 */
XWS_GROUP_API xwsgroup* xrtWsGroupRef(xwsgroup* pGroup)
{
	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.ref"
	) ) {
		return NULL;
	}
	if ( xrtRefRetain(&pGroup->References) < 0 ) {
		__xrtWsGroupError(
			XERR_STATE,
			XWS_GROUP_ERROR_STATE,
			"websocket-group.ref",
			"WebSocket group reference cannot be retained"
		);
		return NULL;
	}
	return pGroup;
}



/* 释放连接组引用；最后一个引用只释放成员，不关闭连接。 */
XWS_GROUP_API void xrtWsGroupDestroy(xwsgroup* pGroup)
{
	if ( pGroup == NULL ) {
		return;
	}
	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.destroy"
	) ) {
		return;
	}
	if ( xrtRefRelease(&pGroup->References) != 0 ) {
		return;
	}
	xrtSetUnit(&pGroup->Connections);
	(void)xrtMutexUnit(&pGroup->Lock);
	xrtFree(pGroup);
}



/* 加入并持有 Connection；重复加入成功且不增加第二个引用。 */
XWS_GROUP_API bool xrtWsGroupAdd(xwsgroup* pGroup, xwsconn* pConnection)
{
	bool bAdded;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.add"
	) ) {
		return false;
	}
	if ( !xrtWsConnValid(pConnection) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.add",
			"WebSocket group or connection range is invalid"
		);
		return false;
	}
	if ( !xrtMutexLock(&pGroup->Lock) ) {
		return false;
	}
	if ( __xrtWsGroupHasLocked(pGroup, pConnection) ) {
		(void)xrtMutexUnlock(&pGroup->Lock);
		return true;
	}
	if ( pGroup->Sealed ) {
		(void)xrtMutexUnlock(&pGroup->Lock);
		__xrtWsGroupError(
			XERR_STATE,
			XWS_GROUP_ERROR_STATE,
			"websocket-group.add",
			"WebSocket group is sealed"
		);
		return false;
	}
	if ( (pGroup->Limit != 0) &&
		(xrtSetCount(&pGroup->Connections) >= pGroup->Limit) ) {
		(void)xrtMutexUnlock(&pGroup->Lock);
		__xrtWsGroupError(
			XERR_AGAIN,
			XWS_GROUP_ERROR_CAPACITY,
			"websocket-group.add",
			"WebSocket group member limit is reached"
		);
		return false;
	}
	bAdded = xrtSetAdd(&pGroup->Connections, &pConnection);
	(void)xrtMutexUnlock(&pGroup->Lock);
	if ( !bAdded ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.add",
			"WebSocket group member allocation failed"
		);
	}
	return bAdded;
}



/* 移除并释放 Connection；成员不存在时正常返回 false。 */
XWS_GROUP_API bool xrtWsGroupRemove(xwsgroup* pGroup, xwsconn* pConnection)
{
	xwsconn* pOwned = NULL;
	bool bRemoved;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.remove"
	) ) {
		return false;
	}
	if ( !xrtWsConnValid(pConnection) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.remove",
			"WebSocket group or connection range is invalid"
		);
		return false;
	}
	if ( !xrtMutexLock(&pGroup->Lock) ) {
		return false;
	}
	bRemoved = xrtSetTake(
		&pGroup->Connections,
		&pConnection,
		&pOwned
	);
	(void)xrtMutexUnlock(&pGroup->Lock);
	if ( bRemoved ) {
		xrtWsConnDestroy(pOwned);
	}
	return bRemoved;
}



/* 判断 Connection 是否属于当前组。 */
XWS_GROUP_API bool xrtWsGroupHas(
	const xwsgroup* pGroup,
	const xwsconn* pConnection
)
{
	bool bPresent;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.has"
	) ) {
		return false;
	}
	if ( !xrtWsConnValid(pConnection) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.has",
			"WebSocket group or connection range is invalid"
		);
		return false;
	}
	if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
		return false;
	}
	bPresent = __xrtWsGroupHasLocked(pGroup, pConnection);
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
	return bPresent;
}



/* 返回当前成员数量。 */
XWS_GROUP_API size_t xrtWsGroupCount(const xwsgroup* pGroup)
{
	size_t iCount;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.count"
	) ) {
		return 0;
	}
	if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
		return 0;
	}
	iCount = xrtSetCount(&pGroup->Connections);
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
	return iCount;
}



/* 返回创建时设置的硬上限；零表示没有显式上限。 */
XWS_GROUP_API size_t xrtWsGroupLimit(const xwsgroup* pGroup)
{
	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.limit"
	) ) {
		return 0;
	}
	return pGroup->Limit;
}



/* 永久阻止新成员加入；重复调用保持成功。 */
XWS_GROUP_API bool xrtWsGroupSeal(xwsgroup* pGroup)
{
	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.seal"
	) ) {
		return false;
	}
	if ( !xrtMutexLock(&pGroup->Lock) ) {
		return false;
	}
	pGroup->Sealed = true;
	(void)xrtMutexUnlock(&pGroup->Lock);
	return true;
}



/* 返回连接组是否已经封闭。 */
XWS_GROUP_API bool xrtWsGroupSealed(const xwsgroup* pGroup)
{
	bool bSealed;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.sealed"
	) ) {
		return false;
	}
	if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
		return false;
	}
	bSealed = pGroup->Sealed;
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
	return bSealed;
}



/* 移除并释放全部成员，返回移除数量，但不重新开放已封闭的组。 */
XWS_GROUP_API size_t xrtWsGroupClear(xwsgroup* pGroup)
{
	xset Empty;
	xset Previous;
	size_t iCount;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.clear"
	) ) {
		return 0;
	}
	if ( !__xrtWsGroupSetInit(&Empty) ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.clear",
			"WebSocket group replacement set initialization failed"
		);
		return 0;
	}
	if ( !xrtMutexLock(&pGroup->Lock) ) {
		xrtSetUnit(&Empty);
		return 0;
	}
	iCount = xrtSetCount(&pGroup->Connections);
	Previous = pGroup->Connections;
	pGroup->Connections = Empty;
	(void)xrtMutexUnlock(&pGroup->Lock);
	xrtSetUnit(&Previous);
	return iCount;
}



/* 成员增长时预留有限余量，避免高频加入导致连续精确重试。 */
static size_t __xrtWsGroupSnapshotGrow(
	const xwsgroup* pGroup,
	size_t iCount
)
{
	size_t iCapacity;
	size_t iExtra;
	size_t iMaximum = (SIZE_MAX - sizeof(xwsgroupsnapshot)) /
		sizeof(xwsconn*);

	if ( iCount >= iMaximum ) {
		return iCount;
	}
	iExtra = (iCount / 2u) + 8u;
	iCapacity = iExtra > (iMaximum - iCount) ?
		iMaximum : iCount + iExtra;
	if ( (pGroup->Limit != 0) &&
		(iCapacity > pGroup->Limit) ) {
		iCapacity = pGroup->Limit;
	}
	return iCapacity < iCount ? iCount : iCapacity;
}



/* 在锁外分配连续存储，并在成员增长时扩容重试。 */
XWS_GROUP_API xwsgroupsnapshot* xrtWsGroupSnapshotCreate(
	const xwsgroup* pGroup
)
{
	xwsgroupsnapshot* pSnapshot;
	size_t iCapacity;
	size_t iCount;
	size_t iSize;
	size_t iVisited;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.snapshot"
	) ) {
		return NULL;
	}
	if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
		return NULL;
	}
	iCapacity = xrtSetCount(&pGroup->Connections);
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);

	for ( ;; ) {
		if ( iCapacity > ((SIZE_MAX - sizeof(*pSnapshot)) /
			sizeof(xwsconn*)) ) {
			__xrtWsGroupError(
				XERR_RANGE,
				XWS_GROUP_ERROR_RANGE,
				"websocket-group.snapshot",
				"WebSocket group snapshot size overflows"
			);
			return NULL;
		}
		iSize = sizeof(*pSnapshot) +
			(iCapacity * sizeof(xwsconn*));
		pSnapshot = (xwsgroupsnapshot*)xrtCalloc(1, iSize);
		if ( pSnapshot == NULL ) {
			__xrtWsGroupWrap(
				XERR_MEMORY,
				XWS_GROUP_ERROR_MEMORY,
				"websocket-group.snapshot",
				"WebSocket group snapshot allocation failed"
			);
			return NULL;
		}
		if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
			xrtFree(pSnapshot);
			return NULL;
		}
		iCount = xrtSetCount(&pGroup->Connections);
		if ( iCount > iCapacity ) {
			(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
			xrtFree(pSnapshot);
			iCapacity = __xrtWsGroupSnapshotGrow(
				pGroup,
				iCount
			);
			continue;
		}
		iVisited = xrtSetVisit(
			(xset*)&pGroup->Connections,
			__xrtWsGroupSnapshotAdd,
			pSnapshot
		);
		(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
		if ( iVisited != iCount ) {
			xrtWsGroupSnapshotDestroy(pSnapshot);
			__xrtWsGroupWrap(
				XERR_STATE,
				XWS_GROUP_ERROR_STATE,
				"websocket-group.snapshot",
				"WebSocket group snapshot could not retain every member"
			);
			return NULL;
		}
		return pSnapshot;
	}
}



/* 返回快照成员数量。 */
XWS_GROUP_API size_t xrtWsGroupSnapshotCount(
	const xwsgroupsnapshot* pSnapshot
)
{
	if ( !__xrtWsGroupSnapshotCheck(
		pSnapshot,
		"websocket-group.snapshot-count",
		NULL
	) ) {
		return 0;
	}
	return pSnapshot->Count;
}



/* 借用指定快照成员；借用期不超过快照生命周期。 */
XWS_GROUP_API xwsconn* xrtWsGroupSnapshotGet(
	const xwsgroupsnapshot* pSnapshot,
	size_t iIndex
)
{
	if ( !__xrtWsGroupSnapshotCheck(
		pSnapshot,
		"websocket-group.snapshot-get",
		NULL
	) ) {
		return NULL;
	}
	if ( iIndex >= pSnapshot->Count ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.snapshot-get",
			"WebSocket group snapshot index is out of range"
		);
		return NULL;
	}
	return pSnapshot->Connections[iIndex];
}



/* 释放快照和它持有的全部 Connection 引用。 */
XWS_GROUP_API void xrtWsGroupSnapshotDestroy(xwsgroupsnapshot* pSnapshot)
{
	if ( pSnapshot == NULL ) {
		return;
	}
	if ( !__xrtWsGroupSnapshotCheck(
		pSnapshot,
		"websocket-group.snapshot-destroy",
		NULL
	) ) {
		return;
	}
	for ( size_t i = 0; i < pSnapshot->Count; i++ ) {
		xrtWsConnDestroy(pSnapshot->Connections[i]);
	}
	xrtFree(pSnapshot);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-group/src/websocket/group_future.c */
/* ========================================================================== */

#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)




#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)

typedef struct __xrt_ws_group_item __xrt_ws_group_item;



/* 批量操作的每个槽位保存提交结果和一个无额外分配的完成监听节点。 */
struct __xrt_ws_group_item {
	struct xwsgroupop* Operation;
	xfuture* Future;
	xerror* Error;
	xfuturewatch Watch;
	bool Completed;
};



/* 批量操作用一次连续分配保存稳定快照之外的全部槽位状态。 */
struct xwsgroupop {
	volatile int32 References;
	xmutex Lock;
	xwsgroupsnapshot* Snapshot;
	xfuture* Completion;
	xpromise* Promise;
	xcancelwatch* Watch;
	size_t Count;
	size_t Accepted;
	size_t Rejected;
	size_t Remaining;
	bool Submitting;
	__xrt_ws_group_item Items[];
};



/* 验证批量操作头、槽位乘法和完整连续分配区间。 */
static bool __xrtWsGroupOpCheck(
	const xwsgroupop* pOperation,
	cstr sOperation,
	size_t* pSize
)
{
	size_t iSize;

	if ( !xrtMemRangeValid(pOperation, sizeof(*pOperation)) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			sOperation,
			"WebSocket group operation range is invalid"
		);
		return false;
	}
	if ( (pOperation->Count > (size_t)(INT32_MAX - 2)) ||
		(pOperation->Count >
		 ((SIZE_MAX - sizeof(*pOperation)) /
		  sizeof(__xrt_ws_group_item))) ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			sOperation,
			"WebSocket group operation size overflows"
		);
		return false;
	}
	iSize = sizeof(*pOperation) +
		(pOperation->Count * sizeof(__xrt_ws_group_item));
	if ( !xrtMemRangeValid(pOperation, iSize) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			sOperation,
			"WebSocket group operation storage is incomplete"
		);
		return false;
	}
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return true;
}



/* 前置声明批量操作内部引用释放路径。 */
static void __xrtWsGroupOpRelease(xwsgroupop* pOperation);



/* 释放完成监听或调用方持有的一个批量操作内部引用。 */
static void __xrtWsGroupOpRelease(xwsgroupop* pOperation)
{
	if ( xrtRefRelease(&pOperation->References) != 0 ) {
		return;
	}
	for ( size_t i = 0; i < pOperation->Count; i++ ) {
		xrtFutureDestroy(pOperation->Items[i].Future);
		xrtErrorFree(pOperation->Items[i].Error);
	}
	if ( pOperation->Promise != NULL ) {
		xrtPromiseDestroy(pOperation->Promise);
	}
	xrtFutureDestroy(pOperation->Completion);
	xrtWsGroupSnapshotDestroy(pOperation->Snapshot);
	(void)xrtMutexUnit(&pOperation->Lock);
	xrtFree(pOperation);
}



/* 完成监听离开源 Future 后归还它持有的操作引用。 */
static void __xrtWsGroupOpWaiterRelease(ptr pData)
{
	__xrt_ws_group_item* pItem = (__xrt_ws_group_item*)pData;

	__xrtWsGroupOpRelease(pItem->Operation);
}



/* 在最后一个已接纳 Future 结束后取得唯一完成 Promise。 */
static xpromise* __xrtWsGroupOpFinishLocked(
	xwsgroupop* pOperation,
	xcancelwatch** ppWatch
)
{
	xpromise* pPromise;

	if ( pOperation->Submitting ||
		(pOperation->Remaining != 0) ||
		(pOperation->Promise == NULL) ) {
		return NULL;
	}
	pPromise = pOperation->Promise;
	pOperation->Promise = NULL;
	*ppWatch = pOperation->Watch;
	pOperation->Watch = NULL;
	return pPromise;
}



/* 发布完成 Future 的唯一成功终态。 */
static void __xrtWsGroupOpResolve(
	xwsgroupop* pOperation,
	xpromise* pPromise,
	xcancelwatch* pWatch
)
{
	if ( pPromise == NULL ) {
		return;
	}
	if ( pWatch != NULL ) {
		xrtCancelUnwatch(pWatch);
		__xrtWsGroupOpRelease(pOperation);
	}
	(void)xrtPromiseResolve(pPromise, NULL);
	xrtPromiseDestroy(pPromise);
}



/* 一个逐成员 Future 进入终态后更新剩余计数。 */
static void __xrtWsGroupOpSourceDone(ptr pData)
{
	__xrt_ws_group_item* pItem = (__xrt_ws_group_item*)pData;
	xwsgroupop* pOperation = pItem->Operation;
	xpromise* pPromise = NULL;
	xcancelwatch* pWatch = NULL;

	(void)xrtMutexLock(&pOperation->Lock);
	if ( !pItem->Completed ) {
		pItem->Completed = true;
		pOperation->Remaining--;
		pPromise = __xrtWsGroupOpFinishLocked(
			pOperation,
			&pWatch
		);
	}
	(void)xrtMutexUnlock(&pOperation->Lock);
	__xrtWsGroupOpResolve(pOperation, pPromise, pWatch);
}



/* 保存同步提交失败，并从当前执行上下文取走精确错误。 */
static void __xrtWsGroupOpReject(__xrt_ws_group_item* pItem)
{
	pItem->Error = xrtTakeError();
	if ( pItem->Error == NULL ) {
		__xrtWsGroupError(
			XERR_STATE,
			XWS_GROUP_ERROR_STATE,
			"websocket-group.submit",
			"WebSocket group member rejected an operation without an error"
		);
		pItem->Error = xrtTakeError();
	}
}



/* 为已接纳 Future 注册预分配完成监听。 */
static void __xrtWsGroupOpAccept(
	xwsgroupop* pOperation,
	__xrt_ws_group_item* pItem,
	xfuture* pFuture
)
{
	xfuturewatchresult WatchResult;
	xpromise* pPromise = NULL;
	xcancelwatch* pWatch = NULL;

	pItem->Operation = pOperation;
	pItem->Future = pFuture;
	if ( !xrtFutureWatchInit(
		&pItem->Watch,
		__xrtWsGroupOpSourceDone,
		__xrtWsGroupOpWaiterRelease,
		pItem
	) ) {
		__xrtWsGroupOpReject(pItem);
		xrtFutureDestroy(pFuture);
		pItem->Future = NULL;
		(void)xrtMutexLock(&pOperation->Lock);
		pOperation->Rejected++;
		(void)xrtMutexUnlock(&pOperation->Lock);
		return;
	}
	(void)xrtMutexLock(&pOperation->Lock);
	pOperation->Accepted++;
	pOperation->Remaining++;
	(void)xrtMutexUnlock(&pOperation->Lock);
	(void)xrtRefRetain(&pOperation->References);
	WatchResult = xrtFutureWatchAdd(pFuture, &pItem->Watch);
	if ( WatchResult == XFUTURE_WATCH_PENDING ) {
		return;
	}
	if ( WatchResult == XFUTURE_WATCH_READY ) {
		__xrtWsGroupOpSourceDone(pItem);
		__xrtWsGroupOpRelease(pOperation);
		return;
	}

	/* Watch 注册失败属于同步拒绝，不能伪装成成员 Future 已完成。 */
	__xrtWsGroupOpReject(pItem);
	xrtFutureDestroy(pFuture);
	pItem->Future = NULL;
	(void)xrtMutexLock(&pOperation->Lock);
	pItem->Completed = true;
	pOperation->Accepted--;
	pOperation->Rejected++;
	pOperation->Remaining--;
	pPromise = __xrtWsGroupOpFinishLocked(pOperation, &pWatch);
	(void)xrtMutexUnlock(&pOperation->Lock);
	__xrtWsGroupOpResolve(pOperation, pPromise, pWatch);
	__xrtWsGroupOpRelease(pOperation);
}



/* 创建尚未提交成员操作、但已经具备稳定快照和完成 Future 的对象。 */
xwsgroupop* __xrtWsGroupOpCreate(xwsgroup* pGroup)
{
	xwsgroupop* pOperation;
	xwsgroupsnapshot* pSnapshot;
	size_t iCount;
	size_t iBytes;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.operation"
	) ) {
		return NULL;
	}
	pSnapshot = xrtWsGroupSnapshotCreate(pGroup);
	if ( pSnapshot == NULL ) {
		return NULL;
	}
	iCount = xrtWsGroupSnapshotCount(pSnapshot);
	if ( (iCount > (size_t)(INT32_MAX - 2)) ||
		(iCount > ((SIZE_MAX - sizeof(*pOperation)) /
		 sizeof(__xrt_ws_group_item))) ) {
		xrtWsGroupSnapshotDestroy(pSnapshot);
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.operation",
			"WebSocket group operation size overflows"
		);
		return NULL;
	}
	iBytes = sizeof(*pOperation) +
		(iCount * sizeof(__xrt_ws_group_item));
	pOperation = (xwsgroupop*)xrtCalloc(1, iBytes);
	if ( pOperation == NULL ) {
		xrtWsGroupSnapshotDestroy(pSnapshot);
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.operation",
			"WebSocket group operation allocation failed"
		);
		return NULL;
	}
	pOperation->References = 1;
	pOperation->Snapshot = pSnapshot;
	pOperation->Count = iCount;
	pOperation->Submitting = true;
	if ( !xrtMutexInit(&pOperation->Lock) ) {
		xrtWsGroupSnapshotDestroy(pSnapshot);
		xrtFree(pOperation);
		return NULL;
	}
	pOperation->Promise = xrtPromiseCreate(
		&pOperation->Completion,
		NULL
	);
	if ( pOperation->Promise == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.operation",
			"WebSocket group completion Future allocation failed"
		);
		__xrtWsGroupOpRelease(pOperation);
		return NULL;
	}
	return pOperation;
}



/* 完成 Future 被请求取消时立即取消聚合等待，并把请求传播给成员。 */
static void __xrtWsGroupOpCancelled(ptr pData)
{
	xwsgroupop* pOperation = (xwsgroupop*)pData;
	xpromise* pPromise;
	xcancelwatch* pWatch;

	(void)xrtMutexLock(&pOperation->Lock);
	if ( pOperation->Promise == NULL ) {
		(void)xrtMutexUnlock(&pOperation->Lock);
		return;
	}
	pPromise = pOperation->Promise;
	pOperation->Promise = NULL;
	pWatch = pOperation->Watch;
	pOperation->Watch = NULL;
	(void)xrtMutexUnlock(&pOperation->Lock);

	(void)xrtWsGroupOpCancel(pOperation);
	(void)xrtPromiseCancel(pPromise);
	xrtPromiseDestroy(pPromise);
	if ( pWatch != NULL ) {
		xrtCancelUnwatch(pWatch);
		__xrtWsGroupOpRelease(pOperation);
	}
}



/* 在任何成员提交前注册完成 Future 的取消传播监听。 */
static bool __xrtWsGroupOpWatch(xwsgroupop* pOperation)
{
	xcancel* pCancel = xrtPromiseCancelToken(pOperation->Promise);

	if ( pCancel == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.operation-watch",
			"WebSocket group completion cancel token failed"
		);
		return false;
	}
	(void)xrtRefRetain(&pOperation->References);
	pOperation->Watch = xrtCancelWatch(
		pCancel,
		__xrtWsGroupOpCancelled,
		pOperation
	);
	xrtCancelDestroy(pCancel);
	if ( pOperation->Watch == NULL ) {
		__xrtWsGroupOpRelease(pOperation);
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.operation-watch",
			"WebSocket group completion cancel watch failed"
		);
		return false;
	}
	return true;
}



/* 遍历稳定快照，保留每个成员的接纳 Future 或同步拒绝错误。 */
bool __xrtWsGroupOpSubmit(
	xwsgroupop* pOperation,
	__xrt_ws_group_submitproc pSubmit,
	ptr pData
)
{
	xpromise* pPromise;
	xcancelwatch* pWatch = NULL;

	if ( !__xrtWsGroupOpWatch(pOperation) ) {
		return false;
	}

	for ( size_t i = 0; i < pOperation->Count; i++ ) {
		__xrt_ws_group_item* pItem = &pOperation->Items[i];
		xwsconn* pConnection = xrtWsGroupSnapshotGet(
			pOperation->Snapshot,
			i
		);
		xfuture* pFuture = pSubmit(pConnection, pData);

		if ( pFuture == NULL ) {
			pOperation->Rejected++;
			__xrtWsGroupOpReject(pItem);
		} else {
			__xrtWsGroupOpAccept(
				pOperation,
				pItem,
				pFuture
			);
		}
	}
	(void)xrtMutexLock(&pOperation->Lock);
	pOperation->Submitting = false;
	pPromise = __xrtWsGroupOpFinishLocked(
		pOperation,
		&pWatch
	);
	(void)xrtMutexUnlock(&pOperation->Lock);
	__xrtWsGroupOpResolve(pOperation, pPromise, pWatch);
	return true;
}



/* 增加批量操作引用并返回原指针。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupOpRef(xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-ref",
		NULL
	) ) {
		return NULL;
	}
	if ( xrtRefRetain(&pOperation->References) < 0 ) {
		__xrtWsGroupError(
			XERR_STATE,
			XWS_GROUP_ERROR_STATE,
			"websocket-group.operation-ref",
			"WebSocket group operation reference cannot be retained"
		);
		return NULL;
	}
	return pOperation;
}



/* 释放批量操作引用。 */
XWS_GROUP_API void xrtWsGroupOpDestroy(xwsgroupop* pOperation)
{
	if ( pOperation == NULL ) {
		return;
	}
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-destroy",
		NULL
	) ) {
		return;
	}
	__xrtWsGroupOpRelease(pOperation);
}



/* 返回稳定成员槽位总数。 */
XWS_GROUP_API size_t xrtWsGroupOpCount(const xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-count",
		NULL
	) ) {
		return 0;
	}
	return pOperation->Count;
}



/* 返回成功创建逐成员 Future 的槽位数。 */
XWS_GROUP_API size_t xrtWsGroupOpAccepted(const xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-accepted",
		NULL
	) ) {
		return 0;
	}
	return pOperation->Accepted;
}



/* 返回提交阶段同步拒绝的槽位数。 */
XWS_GROUP_API size_t xrtWsGroupOpRejected(const xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-rejected",
		NULL
	) ) {
		return 0;
	}
	return pOperation->Rejected;
}



/* 返回同步拒绝和已经进入 Future 终态的槽位数。 */
XWS_GROUP_API size_t xrtWsGroupOpDoneCount(const xwsgroupop* pOperation)
{
	size_t iDone;

	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-done",
		NULL
	) ) {
		return 0;
	}
	if ( !xrtMutexLock((xmutex*)&pOperation->Lock) ) {
		return 0;
	}
	iDone = pOperation->Rejected +
		(pOperation->Accepted - pOperation->Remaining);
	(void)xrtMutexUnlock((xmutex*)&pOperation->Lock);
	return iDone;
}



/* 把通用 Future 状态映射为批量操作槽位状态。 */
static xwsgroupopstate __xrtWsGroupOpState(xfuturestate State)
{
	switch ( State ) {
		case XFUTURE_RESOLVED:
			return XWS_GROUP_OP_RESOLVED;
		case XFUTURE_FAILED:
			return XWS_GROUP_OP_FAILED;
		case XFUTURE_CANCELLED:
			return XWS_GROUP_OP_CANCELLED;
		case XFUTURE_CLOSED:
			return XWS_GROUP_OP_CLOSED;
		default:
			return XWS_GROUP_OP_PENDING;
	}
}



/* 读取一个槽位的稳定成员和当前状态快照。 */
XWS_GROUP_API bool xrtWsGroupOpResult(
	const xwsgroupop* pOperation,
	size_t iIndex,
	xwsgroupopresult* pResult
)
{
	const __xrt_ws_group_item* pItem;
	xwsgroupopresult Result;
	xfuturestate State;
	size_t iOperationSize;

	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-result",
		&iOperationSize
	) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pResult, sizeof(Result)) ||
		xrtMemRangesOverlap(
			pResult,
			sizeof(Result),
			pOperation,
			iOperationSize
		) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.operation-result",
			"WebSocket group operation result range is invalid"
		);
		return false;
	}
	if ( iIndex >= pOperation->Count ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.operation-result",
			"WebSocket group operation index is out of range"
		);
		return false;
	}
	pItem = &pOperation->Items[iIndex];
	Result.Connection = xrtWsGroupSnapshotGet(
		pOperation->Snapshot,
		iIndex
	);
	if ( pItem->Future == NULL ) {
		Result.State = XWS_GROUP_OP_REJECTED;
		Result.Error = pItem->Error;
		memcpy(pResult, &Result, sizeof(Result));
		return true;
	}
	State = xrtFutureState(pItem->Future);
	Result.State = __xrtWsGroupOpState(State);
	Result.Error = State == XFUTURE_FAILED ?
		xrtFutureError(pItem->Future) : NULL;
	memcpy(pResult, &Result, sizeof(Result));
	return true;
}



/* 返回增加引用后的逐成员 Future。 */
XWS_GROUP_API xfuture* xrtWsGroupOpItemFutureRef(
	const xwsgroupop* pOperation,
	size_t iIndex
)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-item",
		NULL
	) ) {
		return NULL;
	}
	if ( iIndex >= pOperation->Count ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.operation-item",
			"WebSocket group operation index is out of range"
		);
		return NULL;
	}
	if ( pOperation->Items[iIndex].Future == NULL ) {
		return NULL;
	}
	return xrtFutureRef(pOperation->Items[iIndex].Future);
}



/* 返回增加引用后的完成 Future。 */
XWS_GROUP_API xfuture* xrtWsGroupOpFutureRef(const xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-future",
		NULL
	) ) {
		return NULL;
	}
	return xrtFutureRef(pOperation->Completion);
}



/* 向全部仍等待的逐成员 Future 请求协作取消。 */
XWS_GROUP_API size_t xrtWsGroupOpCancel(xwsgroupop* pOperation)
{
	size_t iCancelled = 0;

	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-cancel",
		NULL
	) ) {
		return 0;
	}
	for ( size_t i = 0; i < pOperation->Count; i++ ) {
		if ( (pOperation->Items[i].Future != NULL) &&
			xrtFutureCancel(pOperation->Items[i].Future) ) {
			iCancelled++;
		}
	}
	return iCancelled;
}



/* 等待全部已接纳操作进入终态。 */
XWS_GROUP_API xwaitresult xrtWsGroupOpWait(xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-wait",
		NULL
	) ) {
		return XWAIT_ERROR;
	}
	return xrtFutureWait(pOperation->Completion);
}



/* 在相对毫秒数内等待全部已接纳操作进入终态。 */
XWS_GROUP_API xwaitresult xrtWsGroupOpWaitFor(
	xwsgroupop* pOperation,
	int64 iTimeout
)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-wait",
		NULL
	) ) {
		return XWAIT_ERROR;
	}
	return xrtFutureWaitFor(pOperation->Completion, iTimeout);
}



/* 等待全部已接纳操作到指定单调时钟截止时间。 */
XWS_GROUP_API xwaitresult __xrtWsGroupOpWaitUntil(
	xwsgroupop* pOperation,
	double iDeadline
)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-wait",
		NULL
	) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return XWAIT_ERROR; }

		return XWAIT_ERROR;
	}
	return __xrtFutureWaitUntil(pOperation->Completion, iDeadline);
}



/* 等待批量操作、截止时间或调用方取消令牌中的首个事件。 */
XWS_GROUP_API xwaitresult __xrtWsGroupOpWaitUntilCancel(
	xwsgroupop* pOperation,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-wait",
		NULL
	) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return XWAIT_ERROR; }

		return XWAIT_ERROR;
	}
	return __xrtFutureWaitUntilCancel(
		pOperation->Completion,
		iDeadline,
		pCancel
	);
}

#endif

#if (defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE))
XWS_GROUP_API xwaitresult xrtWsGroupOpWaitForCancel(
	xwsgroupop* pOperation,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtWsGroupOpWaitUntilCancel(pOperation, __xrtWaitAfter(iTimeout), pCancel);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-group/src/websocket/group_broadcast.c */
/* ========================================================================== */

#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)




#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE)

/* 共享负载在全部逐连接异步发送归还引用后释放一次来源。 */
typedef struct __xrt_ws_group_payload {
	volatile int32 References;
	xnetref Source;
	bool External;
	uint8 Data[];
} __xrt_ws_group_payload;



/* 完整消息提交上下文可借用空负载，也可共享一个所有权负载。 */
typedef struct __xrt_ws_group_message {
	xwsopcode Opcode;
	xbytesview Payload;
	__xrt_ws_group_payload* Shared;
} __xrt_ws_group_message;



/* 控制帧提交上下文保存操作码和小负载视图。 */
typedef struct __xrt_ws_group_control {
	xwsopcode Opcode;
	xbytesview Payload;
} __xrt_ws_group_control;



/* Close 提交上下文在同步遍历期间借用原因文本。 */
typedef struct __xrt_ws_group_close {
	uint16 Code;
	xstrview Reason;
} __xrt_ws_group_close;



/* Wait 提交上下文保存统一的 Connection 条件。 */
typedef struct __xrt_ws_group_wait {
	xwsconnwait Wait;
} __xrt_ws_group_wait;



/* 最后一个共享负载引用归还调用方来源或释放单次复制。 */
static void __xrtWsGroupPayloadRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	__xrt_ws_group_payload* pPayload =
		(__xrt_ws_group_payload*)pContext;

	(void)pData;
	(void)iSize;
	if ( xrtRefRelease(&pPayload->References) != 0 ) {
		return;
	}
	if ( pPayload->External ) {
		pPayload->Source.Release(
			pPayload->Source.Context,
			pPayload->Source.Data,
			pPayload->Source.Size
		);
	}
	xrtFree(pPayload);
}



/* 为复制型广播创建一次共享负载；空负载不需要所有权节点。 */
static __xrt_ws_group_payload* __xrtWsGroupPayloadCopy(
	xbytesview Payload
)
{
	__xrt_ws_group_payload* pShared;
	size_t iBytes;

	if ( Payload.Size == 0 ) {
		return NULL;
	}
	if ( Payload.Size > (SIZE_MAX - sizeof(*pShared)) ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.payload",
			"WebSocket group payload size overflows"
		);
		return NULL;
	}
	iBytes = sizeof(*pShared) + Payload.Size;
	pShared = (__xrt_ws_group_payload*)xrtMalloc(iBytes);
	if ( pShared == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.payload",
			"WebSocket group shared payload allocation failed"
		);
		return NULL;
	}
	pShared->References = 1;
	pShared->Source = (xnetref) {
		pShared->Data,
		Payload.Size,
		__xrtWsGroupPayloadRelease,
		pShared
	};
	pShared->External = false;
	memcpy(pShared->Data, Payload.Data, Payload.Size);
	return pShared;
}



/* 为调用方所有权负载创建共享控制节点，但尚不调用来源 Release。 */
static __xrt_ws_group_payload* __xrtWsGroupPayloadTake(
	const xnetref* pRef
)
{
	__xrt_ws_group_payload* pShared =
		(__xrt_ws_group_payload*)xrtMalloc(sizeof(*pShared));

	if ( pShared == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.payload",
			"WebSocket group reference control allocation failed"
		);
		return NULL;
	}
	pShared->References = 1;
	pShared->Source = *pRef;
	pShared->External = true;
	return pShared;
}



/* 为一个成员取得共享负载引用。 */
static bool __xrtWsGroupPayloadRef(
	__xrt_ws_group_payload* pShared,
	xnetref* pRef
)
{
	if ( xrtRefRetain(&pShared->References) < 0 ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.payload-ref",
			"WebSocket group payload reference limit is reached"
		);
		return false;
	}
	pRef->Data = pShared->Source.Data;
	pRef->Size = pShared->Source.Size;
	pRef->Release = __xrtWsGroupPayloadRelease;
	pRef->Context = pShared;
	return true;
}



/* 为一个成员提交共享消息；空消息沿用普通异步路径。 */
static xfuture* __xrtWsGroupSubmitMessage(
	xwsconn* pConnection,
	ptr pData
)
{
	__xrt_ws_group_message* pMessage =
		(__xrt_ws_group_message*)pData;
	xnetref Ref;
	xfuture* pFuture;

	if ( pMessage->Shared == NULL ) {
		return xrtWsConnSendAsync(
			pConnection,
			pMessage->Opcode,
			pMessage->Payload
		);
	}
	if ( !__xrtWsGroupPayloadRef(pMessage->Shared, &Ref) ) {
		return NULL;
	}
	pFuture = xrtWsConnSendRefAsync(
		pConnection,
		pMessage->Opcode,
		&Ref
	);
	if ( pFuture == NULL ) {
		__xrtWsGroupPayloadRelease(
			Ref.Context,
			Ref.Data,
			Ref.Size
		);
	}
	return pFuture;
}



/* 为一个成员提交 Ping 或 Pong。 */
static xfuture* __xrtWsGroupSubmitControl(
	xwsconn* pConnection,
	ptr pData
)
{
	__xrt_ws_group_control* pControl =
		(__xrt_ws_group_control*)pData;

	return pControl->Opcode == XWS_OPCODE_PING ?
		xrtWsConnPingAsync(pConnection, pControl->Payload) :
		xrtWsConnPongAsync(pConnection, pControl->Payload);
}



/* 为一个成员提交 Close。 */
static xfuture* __xrtWsGroupSubmitClose(
	xwsconn* pConnection,
	ptr pData
)
{
	__xrt_ws_group_close* pClose = (__xrt_ws_group_close*)pData;

	return xrtWsConnCloseAsync(
		pConnection,
		pClose->Code,
		pClose->Reason
	);
}



/* 为一个成员提交 Connection 条件等待。 */
static xfuture* __xrtWsGroupSubmitWait(
	xwsconn* pConnection,
	ptr pData
)
{
	__xrt_ws_group_wait* pWait = (__xrt_ws_group_wait*)pData;

	return xrtWsConnWaitAsync(pConnection, pWait->Wait);
}



/* 校验完整消息共有的操作码、视图和 UTF-8 语义。 */
static bool __xrtWsGroupMessageValid(
	xwsopcode Opcode,
	xbytesview Payload
)
{
	if ( ((Opcode != XWS_OPCODE_TEXT) &&
		 (Opcode != XWS_OPCODE_BINARY)) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.send",
			"WebSocket group message opcode or view is invalid"
		);
		return false;
	}
	if ( (Opcode == XWS_OPCODE_TEXT) &&
		!xrtUtf8Valid(
			(xstrview) {
				(const char*)Payload.Data,
				Payload.Size
			},
			NULL
		) ) {
		__xrtWsGroupWrap(
			XERR_VALUE,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.send",
			"WebSocket group text is not valid UTF-8"
		);
		return false;
	}
	return true;
}



/* 创建操作并以一次复制的共享负载提交完整消息。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupSendAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_message Message;

	if ( !__xrtWsGroupMessageValid(Opcode, Payload) ) {
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	Message.Opcode = Opcode;
	Message.Payload = Payload;
	Message.Shared = NULL;
	if ( (Payload.Size != 0) &&
		(xrtWsGroupOpCount(pOperation) != 0) ) {
		Message.Shared = __xrtWsGroupPayloadCopy(Payload);
		if ( Message.Shared == NULL ) {
			xrtWsGroupOpDestroy(pOperation);
			return NULL;
		}
		Message.Payload = (xbytesview) {
			Message.Shared->Source.Data,
			Message.Shared->Source.Size
		};
	}
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitMessage,
		&Message
	) ) {
		if ( Message.Shared != NULL ) {
			__xrtWsGroupPayloadRelease(
				Message.Shared,
				Message.Shared->Source.Data,
				Message.Shared->Source.Size
			);
		}
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	if ( Message.Shared != NULL ) {
		__xrtWsGroupPayloadRelease(
			Message.Shared,
			Message.Shared->Source.Data,
			Message.Shared->Source.Size
		);
	}
	return pOperation;
}



/* 对稳定成员快照提交 UTF-8 Text。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupTextAsync(
	xwsgroup* pGroup,
	xstrview Text
)
{
	return xrtWsGroupSendAsync(
		pGroup,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 对稳定成员快照提交 Binary。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupBinaryAsync(
	xwsgroup* pGroup,
	xbytesview Data
)
{
	return xrtWsGroupSendAsync(
		pGroup,
		XWS_OPCODE_BINARY,
		Data
	);
}



/* 创建操作后接管并共享调用方所有权负载。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupSendRefAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	const xnetref* pRef
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_payload* pShared;
	__xrt_ws_group_message Message;
	xnetref Ref;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.send-ref"
	) ) {
		return NULL;
	}
	if ( !xrtMemRangeValid(pRef, sizeof(Ref)) ||
		__xrtWsGroupOverlaps(
			pGroup,
			(cbytes)pRef,
			sizeof(Ref)
		) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.send-ref",
			"WebSocket group reference range is invalid"
		);
		return NULL;
	}
	memcpy(&Ref, pRef, sizeof(Ref));
	if ( (Ref.Data == NULL) ||
		(Ref.Size == 0) ||
		(Ref.Release == NULL) ||
		!xrtMemRangeValid(Ref.Data, Ref.Size) ||
		__xrtWsGroupOverlaps(
			pGroup,
			Ref.Data,
			Ref.Size
		) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.send-ref",
			"WebSocket group reference is incomplete or invalid"
		);
		return NULL;
	}
	if ( !__xrtWsGroupMessageValid(
		Opcode,
		(xbytesview) { Ref.Data, Ref.Size }
	) ) {
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	pShared = __xrtWsGroupPayloadTake(&Ref);
	if ( pShared == NULL ) {
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	Message.Opcode = Opcode;
	Message.Payload = (xbytesview) {
		Ref.Data,
		Ref.Size
	};
	Message.Shared = pShared;
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitMessage,
		&Message
	) ) {
		xrtFree(pShared);
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	__xrtWsGroupPayloadRelease(
		pShared,
		pShared->Source.Data,
		pShared->Source.Size
	);
	return pOperation;
}



/* 对稳定成员快照提交共享所有权 Text。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupTextRefAsync(
	xwsgroup* pGroup,
	const xnetref* pRef
)
{
	return xrtWsGroupSendRefAsync(
		pGroup,
		XWS_OPCODE_TEXT,
		pRef
	);
}



/* 对稳定成员快照提交共享所有权 Binary。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupBinaryRefAsync(
	xwsgroup* pGroup,
	const xnetref* pRef
)
{
	return xrtWsGroupSendRefAsync(
		pGroup,
		XWS_OPCODE_BINARY,
		pRef
	);
}



/* 创建并提交一个 Ping 或 Pong 批量操作。 */
static xwsgroupop* __xrtWsGroupControlAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_control Control;

	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(Payload.Size > XWS_CLOSE_PAYLOAD_MAX) ) {
		__xrtWsGroupError(
			!xrtMemRangeValid(Payload.Data, Payload.Size) ?
				XERR_ARGUMENT : XERR_RANGE,
			!xrtMemRangeValid(Payload.Data, Payload.Size) ?
				XWS_GROUP_ERROR_ARGUMENT :
				XWS_GROUP_ERROR_RANGE,
			"websocket-group.control",
			"WebSocket group control payload is invalid"
		);
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	Control.Opcode = Opcode;
	Control.Payload = Payload;
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitControl,
		&Control
	) ) {
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	return pOperation;
}



/* 对稳定成员快照提交 Ping。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupPingAsync(
	xwsgroup* pGroup,
	xbytesview Payload
)
{
	return __xrtWsGroupControlAsync(
		pGroup,
		XWS_OPCODE_PING,
		Payload
	);
}



/* 对稳定成员快照提交 Pong。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupPongAsync(
	xwsgroup* pGroup,
	xbytesview Payload
)
{
	return __xrtWsGroupControlAsync(
		pGroup,
		XWS_OPCODE_PONG,
		Payload
	);
}



/* 对稳定成员快照提交唯一 Close。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupCloseAsync(
	xwsgroup* pGroup,
	uint16 iCode,
	xstrview Reason
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_close Close;
	size_t iPayload = 0;

	if ( !xrtWsCloseWrite(
		iCode,
		Reason,
		NULL,
		0,
		&iPayload
	) ) {
		__xrtWsGroupWrap(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.close",
			"WebSocket group Close code or reason is invalid"
		);
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	Close.Code = iCode;
	Close.Reason = Reason;
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitClose,
		&Close
	) ) {
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	return pOperation;
}



/* 对稳定成员快照提交统一 Connection 条件等待。 */
XWS_GROUP_API xwsgroupop* xrtWsGroupWaitAsync(
	xwsgroup* pGroup,
	xwsconnwait Wait
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_wait Context;

	if ( (Wait < XWS_CONN_WAIT_WRITE) ||
		(Wait > XWS_CONN_WAIT_CLOSE) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.wait",
			"WebSocket group wait condition is invalid"
		);
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	Context.Wait = Wait;
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitWait,
		&Context
	) ) {
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	return pOperation;
}

#endif
#endif

#endif
