#ifndef XRT_CORE_H
#define XRT_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



/* XRT 版本信息。 */
#define XRT_VERSION_MAJOR	2
#define XRT_VERSION_MINOR	0
#define XRT_VERSION_PATCH	0
#define XRT_VERSION_TEXT		"2.0.0-dev"



/* 所有基于 size_t 的查找接口共用的未找到标记。 */
#define XRT_NPOS SIZE_MAX



/* 动态库符号导出规则。 */
#if defined(_WIN32) || defined(_WIN64)
	#if defined(XRT_BUILD_SHARED)
		#define XRT_API __declspec(dllexport)
	#elif defined(XRT_USE_SHARED)
		#define XRT_API __declspec(dllimport)
	#else
		#define XRT_API
	#endif
#elif defined(__GNUC__) || defined(__clang__)
	#if defined(XRT_BUILD_SHARED)
		#define XRT_API __attribute__((visibility("default")))
	#else
		#define XRT_API
	#endif
#else
	#define XRT_API
#endif



/* C 与 C++ 共用的链接规则。 */
#if defined(__cplusplus)
	#define XRT_EXTERN_C_BEGIN extern "C" {
	#define XRT_EXTERN_C_END }
#else
	#define XRT_EXTERN_C_BEGIN
	#define XRT_EXTERN_C_END
#endif



/* XRT 公共基础类型。 */
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef void* ptr;
typedef char* str;
typedef const char* cstr;
typedef unsigned char* bytes;
typedef const unsigned char* cbytes;



/* 通用 IO 与文件游标共享的移动基准。 */
typedef enum xseek {
	XSEEK_START = 0,
	XSEEK_CURRENT,
	XSEEK_END
} xseek;



/* 绝对时间使用 Unix Epoch 微秒；该标量也是 xlang time 类型的底层表示。 */
typedef int64 xtime;



/* 解析器、压缩器与归档器共用的资源边界。零值表示不限制对应项目。 */
#define XRT_RESOURCE_LIMITS_VERSION 1u
#define XRT_RESOURCE_ALLOW_SYMLINKS 0x00000001u
#define XRT_RESOURCE_ALLOW_HARDLINKS 0x00000002u
#define XRT_RESOURCE_ALLOW_DEVICE_FILES 0x00000004u
#define XRT_RESOURCE_ALLOW_EXTERNAL_ENTITIES 0x00000008u

typedef struct xrtresourcelimits {
	uint32 iSize;
	uint32 iVersion;
	uint64 iMaxInputBytes;
	uint64 iMaxOutputBytes;
	uint64 iMaxItemBytes;
	uint64 iMaxEntries;
	uint64 iMaxNodes;
	uint32 iMaxDepth;
	uint32 iMaxCompressionRatio;
	uint32 iFlags;
	uint32 iReserved;
} xrtresourcelimits;



/* 长耗时流操作共用的进度事件。回调仅在发起操作的线程内同步调用。 */
#define XRT_PROGRESS_VERSION 1u

typedef enum xrtprogressflag {
	XRT_PROGRESS_TOTAL_KNOWN = 1u << 0,
	XRT_PROGRESS_FINAL = 1u << 1
} xrtprogressflag;

typedef struct xrtprogress {
	uint32 iSize;
	uint32 iVersion;
	uint32 iFlags;
	uint32 iReserved;
	uint64 iInputBytes;
	uint64 iTotalInputBytes;
	uint64 iOutputBytes;
} xrtprogress;

/* 返回 false 请求取消。实现不得在回调返回后继续保存 pProgress 或 pUserData。 */
typedef bool (*xrtprogressproc)(const xrtprogress* pProgress, ptr pUserData);

static inline bool xrtProgressReport(xrtprogressproc pProc, ptr pUserData,
	uint64 iInputBytes, uint64 iTotalInputBytes, uint64 iOutputBytes, uint32 iFlags)
{
	xrtprogress tProgress;
	if ( pProc == NULL ) { return true; }
	tProgress.iSize = (uint32)sizeof(tProgress);
	tProgress.iVersion = XRT_PROGRESS_VERSION;
	tProgress.iFlags = iFlags;
	tProgress.iReserved = 0u;
	tProgress.iInputBytes = iInputBytes;
	tProgress.iTotalInputBytes = iTotalInputBytes;
	tProgress.iOutputBytes = iOutputBytes;
	return pProc(&tProgress, pUserData);
}



/* 字节视图只借用内存，不拥有数据，也不要求末尾补零。 */
typedef struct xbytesview {
	cbytes Data;
	size_t Size;
} xbytesview;



/* 字符串视图只借用字节，不拥有数据，也不要求末尾补零。 */
typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;



/* INIT 用于聚合初始化器；LITERAL 用于赋值和函数实参表达式。 */
#define XRT_BYTES_INIT(sData) \
	{ (const unsigned char*)(sData), sizeof(sData) - 1u }
#define XRT_STR_INIT(sText) { (sText), sizeof(sText) - 1u }



#if defined(__cplusplus)
	#define XRT_BYTES_LITERAL(sData) xbytesview{ (const unsigned char*)(sData), sizeof(sData) - 1u }
	#define XRT_STR_LITERAL(sText) xstrview{ (sText), sizeof(sText) - 1u }
#else
	#define XRT_BYTES_LITERAL(sData) ((xbytesview){ (const unsigned char*)(sData), sizeof(sData) - 1u })
	#define XRT_STR_LITERAL(sText) ((xstrview){ (sText), sizeof(sText) - 1u })
#endif



XRT_EXTERN_C_BEGIN

/* A physical strong-reference owner, not a flattened logical value.
 * Data identifies one allocation/control block; Ops must be stable for that
 * identity throughout an inspection. Trace reports EACH owned strong slot
 * exactly once, even when two slots point at the same target. It must not
 * report borrowed or weak edges. Count reports actual strong references.
 * Callbacks must not mutate ownership or let references escape. Balanced
 * temporary read cursors must end before returning; counts are read only
 * after all traces finish. Code and data must remain resident. The caller
 * supplies a quiescent point for the entire graph and a non-aliasing output. */
typedef struct xrtownershipops xrtownershipops;
typedef struct xrtownershipref {
	const void* Data;
	const xrtownershipops* Ops;
} xrtownershipref;
typedef bool (*xrtownershipvisitor)(xrtownershipref Reference, ptr pContext);
typedef bool (*xrtownershiptrace)(const void* pData, xrtownershipvisitor pVisit, ptr pContext);
/* A read-only scope policy may conservatively keep a physical node as a
 * root even if all its counted references are internal. It never removes an
 * actual external root, fabricates a reference, or skips tracing its edges. */
typedef bool (*xrtownershiprootproc)(xrtownershipref Reference, ptr pContext);
struct xrtownershipops {
	bool (*Count)(const void* pData, size_t* pCount);
	bool (*Trace)(const void* pData, xrtownershipvisitor pVisit, ptr pContext);
};
/* Separate, explicitly admitted lifecycle protocol. Never read an extension
 * past xrtownershipops: existing native descriptors contain only two entries.
 * The resolver certifies complete mutation-domain participation, including
 * callback payloads. All nodes/code stay pinned through Finalize/Clear/Finish.
 * Hold/Drop add/consume one actual strong owner (immortal SIZE_MAX nodes keep
 * their sentinel). Hold cannot allocate or call user code. Drop may finalize.
 * Claim/Restore run under exclusive freeze: quarantine weak/raw admission, not
 * existing valid strong owners. False Claim leaves state unchanged. The token
 * is unique, non-NULL, stable and exclusively serialized by the collector.
 * Finalize runs OUTSIDE freeze, while fields are intact; it must persist any
 * at-most-once user duty across abort. False never permits repeating that duty.
 * After finalization, revalidate reachability and Clear under ONE freeze.
 * Clear cannot fail, allocate, wait or call user code; every outgoing strong
 * target is still pinned. Finish runs outside freeze and may wait/reenter, but
 * cannot start new user semantic finalization or publish a claimed receiver.
 * Finish failure must be retryable without repeating completed side effects.
 * An aborted collector restores claims before dropping its real holds.
 * This protocol alone does not provide a collector, safepoint or code owner. */
typedef struct xrtownershipadapterv1 {
	size_t size;
	bool (*Hold)(const void* pData);
	void (*Drop)(const void* pData);
	bool (*Claim)(const void* pData, const void* pToken);
	void (*Restore)(const void* pData, const void* pToken);
	bool (*Finalize)(const void* pData, const void* pToken);
	void (*Clear)(const void* pData, const void* pToken);
	bool (*Finish)(const void* pData, const void* pToken);
} xrtownershipadapterv1;

/* Optional, separately versioned semantic preparation. Never extend/cast the
 * V1 table. The exact Adapter identity binds this immutable descriptor to its
 * physical participant; code/data residency follows the same real holds.
 * Ready runs read-only under graph freeze and calls no user code. Prepare
 * runs outside freeze/mutation, only for an unreachable claimed participant.
 * READY means this participant has settled; callbacks may have changed ANY
 * edge or resurrected objects, requiring a fresh whole graph before another
 * preparation or semantic Finalize. BUSY does not execute semantic callbacks
 * or change owning edges and must not wait for another participant; it lets
 * the planner try dependencies. FAILED is explicit even without a diagnostic.
 * All preparations precede object Finalize. Abort does not undo a notification
 * already delivered: retry must observe the participant's actual state. */
typedef enum xrtownershipprepareresult {
	XRT_OWNERSHIP_PREPARE_FAILED = -1,
	XRT_OWNERSHIP_PREPARE_BUSY = 0,
	XRT_OWNERSHIP_PREPARE_READY = 1
} xrtownershipprepareresult;
typedef struct xrtownershippreparationv1 {
	size_t size;
	const xrtownershipadapterv1* Adapter;
	bool (*Ready)(const void* pData);
	xrtownershipprepareresult (*Prepare)(const void* pData, const void* pToken);
} xrtownershippreparationv1;
typedef struct xrtownershipresult {
	size_t NodeCount;
	size_t EdgeCount;
	size_t ExternalRootCount;
	size_t ReachableAnchorCount;
} xrtownershipresult;

/* Stack-only participation in this XRT instance's ownership mutation domain.
 * Zero-initialize; do not copy, move, share, or edit a live scope. End on the
 * entering native thread. The fields are private bookkeeping, not graph roots.
 * This neither retains an object nor keeps the runtime's code resident. */
typedef struct xrtownershipscope {
	struct xrtownershipscope* Self;
	struct xrtownershipscope* Parent;
	uint64 Thread;
	size_t Children;
	uint32 Mode;
} xrtownershipscope;

/* Enter BEFORE any lock protecting participating edges/counts, leave AFTER
 * the complete mutation. Concurrent and nested mutations are allowed. Only a
 * currently frozen domain can delay entry; collectors never queue an upgrade
 * behind an active mutator. Scope entry/end allocate no memory or TLS slots.
 * xrtRefRetain/Release participate automatically for their atomic update, but
 * that alone does NOT cover an enclosing field update, callback, or destructor.
 * Callers adapting mutable state must guard that complete transition too. */
XRT_API bool xrtOwnershipMutationBegin(xrtownershipscope* pScope);

/* Nonblocking exclusive admission. Busy returns false, leaves the zero scope
 * and ambient error unchanged, and never waits for a reader (including self).
 * Success freezes ONLY participating transitions until ScopeEnd. It is NOT
 * whole-graph quiescence unless EVERY reachable adapter and mutation path is
 * covered, nor is it a claim/commit operation or permission to unload code.
 * Value, Future/Promise and Cancel ownership transitions participate, including
 * complete waiter callbacks and combine/continuation assembly. Native blocking
 * waits and coroutine parking do not keep an enclosing mutation active.
 * User-owned callback payloads, native objects, task/transport state, other
 * direct atomic counters and generated storage still need outer participation.
 * The holder may nest balanced mutation scopes on the SAME native thread for
 * inspection cursors/controlled commit. Do not wait for other threads, suspend
 * fibers, or call arbitrary user callbacks while frozen. Keep code/data alive
 * independently; do not acquire a lock held by a blocked participant. */
XRT_API bool xrtOwnershipFreezeTryBegin(xrtownershipscope* pScope);

/* End either kind; a frozen parent cannot end with nested scopes outstanding.
 * Invalid/copy/wrong-thread/double-end calls fail without releasing admission.
 * Invalid arguments/state set an error; success preserves the ambient error.
 * Closing a freeze is a release boundary for subsequent mutation admission. */
XRT_API bool xrtOwnershipScopeEnd(xrtownershipscope* pScope);

/* Discover from borrowed anchors AND internal owning slots. Internal slots
 * are references held by an enclosing owner being retired (e.g. module
 * globals), so each occurrence subtracts one real strong reference. Anchors
 * only seed discovery and do NOT subtract references. Sharing is deduplicated
 * globally by physical identity; edges retain their multiplicity.
 * An external root has Count > internal incoming edges. Reachability from
 * these roots determines ReachableAnchorCount (unique anchor identities).
 * false leaves output and graph unchanged, including allocation/trace errors,
 * inconsistent descriptors and overcounted edges. No destructor is invoked.
 * This is NOT a concurrent collector, lifetime pin, or unload permission. */
XRT_API bool xrtOwnershipInspect(
	const xrtownershipref* pAnchors, size_t iAnchorCount,
	const xrtownershipref* pInternalSlots, size_t iInternalSlotCount,
	xrtownershipresult* pResult);

/* Same snapshot, additionally returning one reachability bit per input
 * anchor (NULL anchors are false; duplicate anchors have identical bits).
 * pReachable has iAnchorCount elements and aliases neither input nor result.
 * pRootPolicy may be NULL; otherwise it can force additional scope roots,
 * for example native objects outside the domain being collected.
 * On failure BOTH outputs remain unchanged. The caller still owns the
 * quiescent point and every code/data lifetime through any later commit. */
XRT_API bool xrtOwnershipInspectReachable(
	const xrtownershipref* pAnchors, size_t iAnchorCount,
	const xrtownershipref* pInternalSlots, size_t iInternalSlotCount,
	bool* pReachable, xrtownershipresult* pResult,
	xrtownershiprootproc pRootPolicy, ptr pRootContext);

/* A structural snapshot of the SAME physical graph used by Inspect. It does
 * not retain nodes or authorize collection. Create requires independently
 * held, whole-graph quiescence through the caller's later pin/claim step.
 * Admission runs exactly once per discovered identity BEFORE Count or Trace,
 * allowing a collector to reject adapters whose mutation domain is unknown.
 * It must be read-only and may not wait or acquire participant-held locks.
 * NULL admission preserves Inspect's caller-provided quiescence contract.
 * Node/admission order is first discovery: anchors in input order, then
 * internal slots, then outgoing traversal; duplicate identities appear once.
 * On failure *ppSnapshot is unchanged; no user finalizer or release runs.
 * Returned node references stay borrowed: keep every node and its Ops code
 * alive independently, and never interpret a stale snapshot as a new proof. */
typedef struct xrtownershipsnapshot xrtownershipsnapshot;
typedef bool (*xrtownershipadmitproc)(xrtownershipref Reference, ptr pContext);
typedef struct xrtownershipnode {
	xrtownershipref Reference;
	size_t StrongCount;
	size_t InternalCount;
	bool Reachable;
} xrtownershipnode;
XRT_API bool xrtOwnershipSnapshotCreate(
	const xrtownershipref* pAnchors, size_t iAnchorCount,
	const xrtownershipref* pInternalSlots, size_t iInternalSlotCount,
	xrtownershipadmitproc pAdmit, ptr pAdmitContext,
	xrtownershipsnapshot** ppSnapshot);
XRT_API size_t xrtOwnershipSnapshotNodeCount(const xrtownershipsnapshot* pSnapshot);
/* The output is unchanged for NULL/invalid arguments or an out-of-range index.
 * A successful read allocates nothing and never calls an adapter again. */
XRT_API bool xrtOwnershipSnapshotNode(const xrtownershipsnapshot* pSnapshot,
	size_t iIndex, xrtownershipnode* pNode);
/* Frees only snapshot bookkeeping; it neither releases nor touches nodes. */
XRT_API void xrtOwnershipSnapshotDestroy(xrtownershipsnapshot* pSnapshot);



/* 返回当前 XRT 版本字符串。 */
XRT_API cstr xrtVersion(void);

/* Terminal retirement of this runtime instance's internal Windows TLS/FLS.
 * The host must first stop admission, join all XRT work, release all exported
 * resources, clear dynamic thread keys on their owning threads, and unbind
 * execution contexts. Native threads/fibers may remain alive but must not
 * enter this instance again, including during retirement. Call outside
 * DllMain/loader lock, while this instance and its allocator are still loaded.
 * true permits unloading with respect to internal TLS/FLS callbacks only;
 * it is NOT an object/code lease or a general resource ownership check.
 * false keeps the code resident: retirement may be partial and may only be
 * retried, never resumed. Successful calls are idempotent. No error TLS is
 * accessed by this function. Unsupported platforms return false unchanged. */
XRT_API bool xrtRuntimeRetireThreadStorage(void);



/* 初始化一组适合处理不受信任输入的通用资源边界。 */
XRT_API void xrtResourceLimitsInit(xrtresourcelimits* pLimits);



/* 原子增加有效引用计数，失败时返回 -1。 */
XRT_API int32 xrtRefRetain(volatile int32* pCount);



/* 原子减少有效引用计数，失败时返回 -1。 */
XRT_API int32 xrtRefRelease(volatile int32* pCount);



XRT_EXTERN_C_END

#endif
