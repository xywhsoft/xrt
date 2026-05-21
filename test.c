
#include "test/test_xnet_internal_env.h"



/*
 * Runner 配置区：
 *
 * 常用运行模式：
 * - XRT_TEST_MODE_SINGLE
 * - XRT_TEST_MODE_PRESET
 * - XRT_TEST_MODE_ALL
 *
 * 常用单测 ID：
 * - "future_core"
 * - "memglobal_core"
 * - "mempool_core"
 * - "memtelemetry"
 * - "memtelemetry_baseline"
 * - "queue_core"
 * - "temparena_core"
 * - "xurl_core"
 * - "xnet2_sync"
 *
 * 常用预设 ID：
 * - "memory_smoke"
 * - "runtime_smoke"
 * - "container_smoke"
 * - "network_smoke"
 * - "network_edges"
 */

#define XRT_TEST_MODE_SINGLE	1
#define XRT_TEST_MODE_PRESET	2
#define XRT_TEST_MODE_ALL		3

#ifndef XRT_TEST_RUN_MODE
	#define XRT_TEST_RUN_MODE		XRT_TEST_MODE_SINGLE
#endif

#ifndef XRT_TEST_SINGLE_ID
	#define XRT_TEST_SINGLE_ID		"future_core"
#endif

#ifndef XRT_TEST_PRESET_ID
	#define XRT_TEST_PRESET_ID		"memory_smoke"
#endif

#ifndef XRT_TEST_FAIL_FAST
	#define XRT_TEST_FAIL_FAST		1
#endif

#ifndef XRT_TEST_RUNNER_QUIET
	#define XRT_TEST_RUNNER_QUIET	1
#endif

#ifndef XRT_TEST_ENABLE_ONERROR
	#define XRT_TEST_ENABLE_ONERROR	0
#endif

#define XRT_TEST_FLAG_NONE		0u
#define XRT_TEST_FLAG_SELF_INIT	0x0001u

#define XRT_COUNT_OF(arr)		(sizeof(arr) / sizeof((arr)[0]))

static int __g_iXrtTestExtraArgCount;
static char** __g_arrXrtTestExtraArgs;
static uint32 __xrtTestParseUint32ExtraArg(int iIndex, uint32 iDefaultValue);


#if defined(__GNUC__) || defined(__clang__)
/*
 * The monolithic test TU intentionally mixes `str`/`u16str` APIs with plain
 * C string literals and debug-style printf output. Keep warning gating focused
 * on the library/runtime code while letting the legacy harness stay readable.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif


#include "test/test_jnum.h"
#include "test/test_suplib.h"
#include "test/test_base.h"
#include "test/test_charset.h"
#include "test/test_os.h"
#include "test/test_math.h"
#include "test/test_string.h"
#ifndef XRT_NO_REGEX
	#include "test/test_regex.h"
#endif
#include "test/test_path.h"
#include "test/test_time.h"
#ifndef XRT_NO_LOGGER
	#include "test/test_logger.h"
#endif
#include "test/test_file.h"
#include "test/test_file_core.h"
#include "test/test_file_dirscan_contract.h"
#ifndef XRT_NO_NETWORK
	#include "test/test_file_async.h"
#endif
#include "test/test_thread_core.h"
#ifndef XRT_NO_QUEUE
	#include "test/test_queue_core.h"
#endif
#include "test/test_runtime_phase2.h"
#ifndef XRT_NO_COROUTINE
	#include "test/test_coroutine.h"
	#include "test/test_coroutine_min.h"
#endif
#include "test/test_rwlock.h"
#include "test/test_hash.h"
#include "test/test_network.h"
#include "test/test_crypto.h"
#include "test/test_xid.h"
#include "test/test_buffer.h"
#include "test/test_array_ptr.h"
#include "test/test_array_struct.h"
#include "test/test_bsmm.h"
#include "test/test_memunit.h"
#include "test/test_mempool_fs.h"
#include "test/test_stack_ptr.h"
#include "test/test_stack.h"
#include "test/test_dynstack_ptr.h"
#include "test/test_dynstack.h"
#include "test/test_avltree.h"
#include "test/test_avltree_iterator.h"
#include "test/test_mempool.h"
#include "test/test_dict.h"
#include "test/test_dict_iterator.h"
#include "test/test_list.h"
#include "test/test_list_iterator.h"
#include "test/test_value.h"
#include "test/test_json.h"
#include "test/test_json_xson_bench.h"
#include "test/test_xson.h"
#include "test/test_template.h"
#include "test/test_other.h"
#include "test/test_future_core.h"
#include "test/test_memglobal_core.h"
#include "test/test_mempool_core.h"
#include "test/test_memtelemetry.h"
#ifndef XRT_NO_NETWORK
	#ifndef XRT_NO_COROUTINE
		#include "test/test_memtelemetry_baseline.h"
	#endif
#endif
#include "test/test_temparena_core.h"
#ifdef XRT_MEM_DEBUG
	#include "test/test_memdebug_core.h"
#endif
#ifndef XRT_NO_XURL
	#ifndef XRT_NO_HTTP_UTIL
		#include "test/test_xurl.h"
	#endif
#endif
#ifndef XRT_NO_NETWORK
	#include "test/test_xnet2_base.h"
	#include "test/test_xnet2_codec.h"
	#include "test/test_xnet2_dgram.h"
	#include "test/test_xnet2_engine.h"
	#include "test/test_xnet2_mem.h"
	#include "test/test_xnet2_port.h"
	#include "test/test_xnet2_stream.h"
	#ifndef XRT_NO_COROUTINE
		#include "test/test_xnet2_sync.h"
	#endif
	#include "test/test_xnet_http.h"
	#include "test/test_xnet_httpd.h"
	#include "test/test_xnet_native_core.h"
	#ifndef XRT_NO_COROUTINE
		#include "test/test_xnet2_listener_accept_core.h"
	#endif
	#include "test/test_xnet_ws.h"
	#include "test/test_xnet_proxy.h"
	#ifndef XRT_NO_NETTLS
		#include "test/test_xnet2_tls.h"
		#include "test/test_tls_boundary.h"
		#include "test/test_tls_anchor_verify.h"
	#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif



typedef int (*xrt_test_proc)(xrtGlobalData* pCore);

typedef struct
{
	const char* sId;
	const char* sName;
	const char* sGroup;
	uint32 iFlags;
	xrt_test_proc Proc;
} xrt_test_entry;

typedef struct
{
	const char* sId;
	const char* sName;
	const char** arrTestId;
	size_t iTestCount;
} xrt_test_preset;

typedef struct
{
	size_t iPassCount;
	size_t iFailCount;
	size_t iSkipCount;
} xrt_test_summary;

static int __g_iXrtTestExtraArgCount = 0;
static char** __g_arrXrtTestExtraArgs = NULL;



// ONERROR相关处理
void OnError(str sError)
{
	printf("X Runtime Error : %s\n", sError);
}



#define XRT_TEST_WRAP_CORE(procWrapper, procTest)	\
	static int procWrapper(xrtGlobalData* pCore)	\
	{	\
		procTest(pCore);	\
		return 0;	\
	}

#define XRT_TEST_WRAP_CORE_INT(procWrapper, procTest)	\
	static int procWrapper(xrtGlobalData* pCore)	\
	{	\
		return procTest(pCore);	\
	}

#define XRT_TEST_WRAP_VOID(procWrapper, procTest)	\
	static int procWrapper(xrtGlobalData* pCore)	\
	{	\
		(void)pCore;	\
		procTest();	\
		return 0;	\
	}

#define XRT_TEST_WRAP_BOOL(procWrapper, procTest)	\
	static int procWrapper(xrtGlobalData* pCore)	\
	{	\
		(void)pCore;	\
		return procTest() ? 0 : 1;	\
	}

#define XRT_TEST_WRAP_INT(procWrapper, procTest)	\
	static int procWrapper(xrtGlobalData* pCore)	\
	{	\
		(void)pCore;	\
		return procTest();	\
	}



XRT_TEST_WRAP_CORE(__xrtTestRun_JNum, Test_JNum)
XRT_TEST_WRAP_CORE(__xrtTestRun_SupLib, Test_SupLib)
XRT_TEST_WRAP_CORE(__xrtTestRun_Base, Test_Base)
XRT_TEST_WRAP_CORE(__xrtTestRun_Charset, Test_Charset)
XRT_TEST_WRAP_CORE_INT(__xrtTestRun_OS, Test_OS)
XRT_TEST_WRAP_CORE(__xrtTestRun_Math, Test_Math)
XRT_TEST_WRAP_CORE(__xrtTestRun_Approx, Test_Approx)
XRT_TEST_WRAP_CORE(__xrtTestRun_String, Test_String)
#ifndef XRT_NO_REGEX
	XRT_TEST_WRAP_INT(__xrtTestRun_Regex, Test_Regex)
#endif
XRT_TEST_WRAP_CORE(__xrtTestRun_Path, Test_Path)
XRT_TEST_WRAP_CORE(__xrtTestRun_Time, Test_Time)
#ifndef XRT_NO_LOGGER
	XRT_TEST_WRAP_INT(__xrtTestRun_Logger, Test_Logger)
#endif
XRT_TEST_WRAP_CORE(__xrtTestRun_File, Test_File)
XRT_TEST_WRAP_CORE(__xrtTestRun_FileCore, Test_FileCore)
XRT_TEST_WRAP_CORE(__xrtTestRun_DirScanContract, Test_FileDirScanContract)
#ifndef XRT_NO_NETWORK
	XRT_TEST_WRAP_CORE(__xrtTestRun_FileAsync, Test_FileAsync)
#endif
XRT_TEST_WRAP_INT(__xrtTestRun_ThreadCore, Test_ThreadCore)
#ifndef XRT_NO_QUEUE
	XRT_TEST_WRAP_INT(__xrtTestRun_QueueCore, Test_QueueCore)
#endif
XRT_TEST_WRAP_CORE(__xrtTestRun_RuntimePhase2, Test_Runtime_Phase2)
#ifndef XRT_NO_COROUTINE
	XRT_TEST_WRAP_CORE(__xrtTestRun_Coroutine, Test_Coroutine)
	XRT_TEST_WRAP_INT(__xrtTestRun_CoroutineMin, Test_CoroutineMin)
#endif
XRT_TEST_WRAP_CORE(__xrtTestRun_RWLock, Test_RWLock)
XRT_TEST_WRAP_CORE(__xrtTestRun_Hash, Test_Hash)
XRT_TEST_WRAP_CORE(__xrtTestRun_Network, Test_Network)
XRT_TEST_WRAP_CORE(__xrtTestRun_Crypto, Test_Crypto)
XRT_TEST_WRAP_CORE(__xrtTestRun_XID, Test_XID)
XRT_TEST_WRAP_CORE(__xrtTestRun_Buffer, Test_Buffer)
XRT_TEST_WRAP_CORE(__xrtTestRun_ArrayPtr, Test_Array_Ptr)
XRT_TEST_WRAP_CORE(__xrtTestRun_ArrayStruct, Test_Array_Struct)
XRT_TEST_WRAP_CORE(__xrtTestRun_BSMM, Test_BSMM)
XRT_TEST_WRAP_CORE(__xrtTestRun_MemUnit, Test_MemUnit)
XRT_TEST_WRAP_CORE(__xrtTestRun_MemPoolFS, Test_MemPoolFS)
XRT_TEST_WRAP_CORE(__xrtTestRun_StackPtr, Test_Stack_Ptr)
XRT_TEST_WRAP_CORE(__xrtTestRun_Stack, Test_Stack)
XRT_TEST_WRAP_CORE(__xrtTestRun_DynStackPtr, Test_DynStack_Ptr)
XRT_TEST_WRAP_CORE(__xrtTestRun_DynStack, Test_DynStack)
XRT_TEST_WRAP_CORE(__xrtTestRun_AVLTree, Test_AVLTree)
XRT_TEST_WRAP_CORE(__xrtTestRun_AVLTreeIterator, Test_AVLTree_Iterator)
XRT_TEST_WRAP_CORE(__xrtTestRun_MemPool, Test_MemPool)
XRT_TEST_WRAP_CORE(__xrtTestRun_Dict, Test_Dict)
XRT_TEST_WRAP_CORE(__xrtTestRun_DictIterator, Test_Dict_Iterator)
XRT_TEST_WRAP_CORE(__xrtTestRun_List, Test_List)
XRT_TEST_WRAP_CORE(__xrtTestRun_ListIterator, Test_List_Iterator)
XRT_TEST_WRAP_CORE(__xrtTestRun_ValueBasic, Test_Value_Basic)
XRT_TEST_WRAP_CORE(__xrtTestRun_ValueOperations, Test_Value_Operations)
XRT_TEST_WRAP_CORE(__xrtTestRun_ValueFull, Test_Value_Full)
XRT_TEST_WRAP_CORE(__xrtTestRun_JSON, Test_JSON)
XRT_TEST_WRAP_INT(__xrtTestRun_JSONXSONBench, Test_JSON_XSON_Bench)
XRT_TEST_WRAP_INT(__xrtTestRun_XSON, Test_XSON)
XRT_TEST_WRAP_CORE(__xrtTestRun_Template, Test_Template)
XRT_TEST_WRAP_CORE(__xrtTestRun_Other, Test_Other)
XRT_TEST_WRAP_VOID(__xrtTestRun_MemGlobalCore, Test_MemGlobalCore)
XRT_TEST_WRAP_VOID(__xrtTestRun_MemPoolCore, Test_MemPoolCore)
XRT_TEST_WRAP_VOID(__xrtTestRun_MemTelemetry, Test_MemTelemetry)
#ifndef XRT_NO_NETWORK
	#ifndef XRT_NO_COROUTINE
		XRT_TEST_WRAP_CORE(__xrtTestRun_MemTelemetryBaseline, Test_MemTelemetryBaseline)
	#endif
#endif
XRT_TEST_WRAP_VOID(__xrtTestRun_TempArenaCore, Test_TempArenaCore)


// 内部函数：__xrtTestRun_FutureCore
static int __xrtTestRun_FutureCore(xrtGlobalData* pCore)
{
	int iRet;

	(void)pCore;
	iRet = __Test_FutureCore_RunAll();
	if ( iRet == 0 ) {
		printf("Future Core Test : PASS\n");
		return 0;
	}

	printf("Future Core Test : FAIL (%d)\n", iRet);
	return iRet;
}

#ifdef XRT_MEM_DEBUG
// 内部函数：__xrtTestRun_MemDebugCore
static int __xrtTestRun_MemDebugCore(xrtGlobalData* pCore)
{
	(void)pCore;
	xrtMemDebugReset();
	xrtMemDebugEnable(TRUE);
	Test_MemDebugCore();
	xrtMemDebugEnable(FALSE);
	xrtMemDebugReset();
	return 0;
}
#endif

#ifndef XRT_NO_XURL
	#ifndef XRT_NO_HTTP_UTIL
		// 内部函数：__xrtTestRun_XUrlCore
		static int __xrtTestRun_XUrlCore(xrtGlobalData* pCore)
		{
			(void)pCore;
			return Test_XUrlCore() ? 0 : 1;
		}
	#endif
#endif

#ifndef XRT_NO_NETWORK
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNet2Base, Test_XNet2_Base)
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNet2Codec, Test_XNet2_Codec)
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNet2Dgram, Test_XNet2_Dgram)
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNet2Engine, Test_XNet2_Engine)
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNet2Mem, Test_XNet2_Mem)
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNet2Port, Test_XNet2_Port)
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNet2Stream, Test_XNet2_Stream)
	#ifndef XRT_NO_COROUTINE
		XRT_TEST_WRAP_INT(__xrtTestRun_XNet2Sync, Test_XNet2_Sync)
	#endif
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNetHttp, Test_XNet_Http)
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNetHttpd, Test_XNet_Httpd)
	#ifndef XRT_NO_COROUTINE
		XRT_TEST_WRAP_INT(__xrtTestRun_XNet2ListenerAcceptCore, Test_XNet2_ListenerAcceptCore)
	#endif
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNetWs, Test_XNet_Ws)
	XRT_TEST_WRAP_VOID(__xrtTestRun_XNetProxy, Test_XNet_Proxy)
	#ifndef XRT_NO_NETTLS
		XRT_TEST_WRAP_VOID(__xrtTestRun_XNet2TLS, Test_XNet2_TLS)


		// 内部函数：__xrtTestRun_TLSBoundaryStress
		static int __xrtTestRun_TLSBoundaryStress(xrtGlobalData* pCore)
		{
			int iRet;
			(void)pCore;
			iRet = Test_TLSBoundaryStress();
			xrtUnit();
			return iRet;
		}
		XRT_TEST_WRAP_INT(__xrtTestRun_TLSAnchorVerify, Test_TLSAnchorVerify)
	#endif
#endif

// 内部函数：__xrtTestParseUint32ExtraArg
static uint32 __xrtTestParseUint32ExtraArg(int iIndex, uint32 iDefaultValue)
{
	char* sEnd = NULL;
	unsigned long iValue;

	if ( (__g_arrXrtTestExtraArgs == NULL) || (iIndex < 0) || (iIndex >= __g_iXrtTestExtraArgCount) ) {
		return iDefaultValue;
	}

	iValue = strtoul(__g_arrXrtTestExtraArgs[iIndex], &sEnd, 10);
	if ( (sEnd == __g_arrXrtTestExtraArgs[iIndex]) || (sEnd == NULL) || (*sEnd != '\0') ) {
		return iDefaultValue;
	}

	return (uint32)iValue;
}

#ifndef XRT_NO_NETWORK
// 内部函数：__xrtTestRun_XNetNativeCore
static int __xrtTestRun_XNetNativeCore(xrtGlobalData* pCore)
{
	(void)pCore;

	if ( __g_iXrtTestExtraArgCount <= 0 ) {
		return Test_XNetNativeCore();
	}

	return Test_XNetNativeCoreWithRounds(
		__xrtTestParseUint32ExtraArg(0, __Test_XNetNativeCore_DefaultPlainRounds()),
		__xrtTestParseUint32ExtraArg(1, __Test_XNetNativeCore_DefaultTlsRounds()));
}
#endif



static const xrt_test_entry __g_arrXrtTests[] = {
	{ "jnum", "JNum", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_JNum },
	{ "suplib", "SupLib", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_SupLib },
	{ "base", "Base", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Base },
	{ "charset", "Charset", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Charset },
	{ "os", "OS", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_OS },
	{ "math", "Math", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Math },
	{ "approx", "Approx", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Approx },
	{ "string", "String", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_String },
	#ifndef XRT_NO_REGEX
		{ "regex", "Regex", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_Regex },
	#endif
	{ "path", "Path", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Path },
	{ "time", "Time", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Time },
	#ifndef XRT_NO_LOGGER
		{ "logger", "Logger", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Logger },
	#endif
	{ "file", "File", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_File },
	{ "file_core", "File Core", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_FileCore },
	{ "dirscan_contract", "DirScan Contract", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_DirScanContract },
	#ifndef XRT_NO_NETWORK
		{ "file_async", "File Async", "runtime", XRT_TEST_FLAG_NONE, __xrtTestRun_FileAsync },
	#endif
	{ "thread_core", "Thread Core", "runtime", XRT_TEST_FLAG_NONE, __xrtTestRun_ThreadCore },
	#ifndef XRT_NO_QUEUE
		{ "queue_core", "Queue Core", "runtime", XRT_TEST_FLAG_NONE, __xrtTestRun_QueueCore },
	#endif
	{ "runtime_phase2", "Runtime Phase2", "runtime", XRT_TEST_FLAG_NONE, __xrtTestRun_RuntimePhase2 },
	#ifndef XRT_NO_COROUTINE
		{ "coroutine", "Coroutine", "async", XRT_TEST_FLAG_NONE, __xrtTestRun_Coroutine },
		{ "coroutine_min", "Coroutine Min", "async", XRT_TEST_FLAG_NONE, __xrtTestRun_CoroutineMin },
	#endif
	{ "rwlock", "RWLock", "runtime", XRT_TEST_FLAG_NONE, __xrtTestRun_RWLock },
	{ "hash", "Hash", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Hash },
	{ "network", "Network", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_Network },
	{ "crypto", "Crypto", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_Crypto },
	{ "xid", "XID", "base", XRT_TEST_FLAG_NONE, __xrtTestRun_XID },
	{ "buffer", "Buffer", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_Buffer },
	{ "array_ptr", "Array Ptr", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_ArrayPtr },
	{ "array_struct", "Array Struct", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_ArrayStruct },
	{ "bsmm", "BSMM", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_BSMM },
	{ "memunit", "MemUnit", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_MemUnit },
	{ "mempool_fs", "MemPool FS", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_MemPoolFS },
	{ "stack_ptr", "Stack Ptr", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_StackPtr },
	{ "stack", "Stack", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_Stack },
	{ "dynstack_ptr", "DynStack Ptr", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_DynStackPtr },
	{ "dynstack", "DynStack", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_DynStack },
	{ "avltree", "AVLTree", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_AVLTree },
	{ "avltree_iterator", "AVLTree Iterator", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_AVLTreeIterator },
	{ "mempool", "MemPool", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_MemPool },
	{ "dict", "Dict", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_Dict },
	{ "dict_iterator", "Dict Iterator", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_DictIterator },
	{ "list", "List", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_List },
	{ "list_iterator", "List Iterator", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_ListIterator },
	{ "value_basic", "Value Basic", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_ValueBasic },
	{ "value_operations", "Value Operations", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_ValueOperations },
	{ "value_full", "Value Full", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_ValueFull },
	{ "json", "JSON", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_JSON },
	{ "json_xson_bench", "JSON XSON Bench", "benchmark", XRT_TEST_FLAG_NONE, __xrtTestRun_JSONXSONBench },
	{ "xson", "XSON", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_XSON },
	{ "template", "Template", "container", XRT_TEST_FLAG_NONE, __xrtTestRun_Template },
	{ "other", "Other", "misc", XRT_TEST_FLAG_NONE, __xrtTestRun_Other },
	{ "future_core", "Future Core", "async", XRT_TEST_FLAG_NONE, __xrtTestRun_FutureCore },
	{ "memglobal_core", "MemGlobal Core", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_MemGlobalCore },
	{ "mempool_core", "MemPool Core", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_MemPoolCore },
	{ "memtelemetry", "MemTelemetry", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_MemTelemetry },
	#ifndef XRT_NO_NETWORK
		#ifndef XRT_NO_COROUTINE
			{ "memtelemetry_baseline", "MemTelemetry Baseline", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_MemTelemetryBaseline },
		#endif
	#endif
	{ "temparena_core", "TempArena Core", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_TempArenaCore },
	#ifdef XRT_MEM_DEBUG
		{ "memdebug_core", "MemDebug Core", "memory", XRT_TEST_FLAG_NONE, __xrtTestRun_MemDebugCore },
	#endif
	#ifndef XRT_NO_XURL
		#ifndef XRT_NO_HTTP_UTIL
			{ "xurl_core", "XUrl Core", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XUrlCore },
		#endif
	#endif
	#ifndef XRT_NO_NETWORK
		{ "xnet2_base", "XNet2 Base", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2Base },
		{ "xnet2_codec", "XNet2 Codec", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2Codec },
		{ "xnet2_dgram", "XNet2 Dgram", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2Dgram },
		{ "xnet2_engine", "XNet2 Engine", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2Engine },
		{ "xnet2_mem", "XNet2 Mem", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2Mem },
		{ "xnet2_port", "XNet2 Port", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2Port },
		{ "xnet2_stream", "XNet2 Stream", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2Stream },
		#ifndef XRT_NO_COROUTINE
			{ "xnet2_sync", "XNet2 Sync", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2Sync },
		#endif
		{ "xnet_http", "XNet HTTP", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNetHttp },
		{ "xnet_httpd", "XNet HTTPD", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNetHttpd },
		{ "xnet_native_core", "XNet Native Core", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNetNativeCore },
		#ifndef XRT_NO_COROUTINE
			{ "xnet2_listener_accept_core", "XNet2 Listener Accept Core", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2ListenerAcceptCore },
		#endif
		{ "xnet_ws", "XNet WS", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNetWs },
		{ "xnet_proxy", "XNet Proxy", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNetProxy },
		#ifndef XRT_NO_NETTLS
			{ "xnet2_tls", "XNet2 TLS", "net", XRT_TEST_FLAG_NONE, __xrtTestRun_XNet2TLS },
			{ "tls_boundary", "TLS Boundary", "net", XRT_TEST_FLAG_SELF_INIT, __xrtTestRun_TLSBoundaryStress },
			{ "tls_anchor_verify", "TLS Anchor Verify", "net", XRT_TEST_FLAG_SELF_INIT, __xrtTestRun_TLSAnchorVerify },
		#endif
	#endif
};

static const char* __g_arrPresetMemorySmoke[] = {
	"bsmm",
	"memunit",
	"mempool_fs",
	"mempool",
	"memglobal_core",
	"mempool_core",
	"memtelemetry",
	"temparena_core",
	#ifdef XRT_MEM_DEBUG
		"memdebug_core",
	#endif
};

static const char* __g_arrPresetRuntimeSmoke[] = {
	"base",
	"os",
	"time",
	#ifndef XRT_NO_LOGGER
		"logger",
	#endif
	"file",
	"dirscan_contract",
	#ifndef XRT_NO_NETWORK
		"file_async",
	#endif
	"thread_core",
	#ifndef XRT_NO_QUEUE
		"queue_core",
	#endif
	"runtime_phase2",
	#ifndef XRT_NO_COROUTINE
		"coroutine_min",
		"coroutine",
	#endif
	"future_core",
};

static const char* __g_arrPresetContainerSmoke[] = {
	"buffer",
	"array_ptr",
	"array_struct",
	"stack_ptr",
	"stack",
	"dynstack_ptr",
	"dynstack",
	"avltree",
	"dict",
	"list",
	"value_full",
	"json",
	"xson",
	#ifndef XRT_NO_REGEX
		"regex",
	#endif
	"template",
};

static const char* __g_arrPresetNetworkSmoke[] = {
	"network",
	#ifndef XRT_NO_XURL
		#ifndef XRT_NO_HTTP_UTIL
			"xurl_core",
		#endif
	#endif
	#ifndef XRT_NO_NETWORK
		"xnet2_base",
		"xnet2_port",
		"xnet2_engine",
		"xnet2_stream",
		"xnet2_dgram",
		#ifndef XRT_NO_COROUTINE
			"xnet2_sync",
		#endif
		"xnet2_codec",
		"xnet_http",
		"xnet_httpd",
		"xnet_ws",
		"xnet_proxy",
		"xnet2_mem",
		#ifndef XRT_NO_NETTLS
			"xnet2_tls",
			"tls_boundary",
		#endif
	#endif
};

#ifndef XRT_NO_NETWORK
	static const char* __g_arrPresetXNet2Stage[] = {
		"xnet2_base",
		"xnet2_port",
		"xnet2_engine",
		"xnet2_stream",
		"xnet2_dgram",
		#ifndef XRT_NO_NETTLS
			"xnet2_tls",
		#endif
		#ifndef XRT_NO_COROUTINE
			"xnet2_sync",
		#endif
		"xnet2_codec",
		"xnet_http",
		"xnet_httpd",
		"xnet_ws",
		"xnet_proxy",
		"xnet2_mem",
	};

	static const char* __g_arrPresetNetworkEdges[] = {
		#ifndef XRT_NO_COROUTINE
			"xnet2_listener_accept_core",
		#endif
		"xnet_native_core",
		#ifndef XRT_NO_NETTLS
			"tls_boundary",
		#endif
	};
#endif

static const xrt_test_preset __g_arrXrtPresets[] = {
	{ "memory_smoke", "Memory Smoke", __g_arrPresetMemorySmoke, XRT_COUNT_OF(__g_arrPresetMemorySmoke) },
	{ "runtime_smoke", "Runtime Smoke", __g_arrPresetRuntimeSmoke, XRT_COUNT_OF(__g_arrPresetRuntimeSmoke) },
	{ "container_smoke", "Container Smoke", __g_arrPresetContainerSmoke, XRT_COUNT_OF(__g_arrPresetContainerSmoke) },
	{ "network_smoke", "Network Smoke", __g_arrPresetNetworkSmoke, XRT_COUNT_OF(__g_arrPresetNetworkSmoke) },
	#ifndef XRT_NO_NETWORK
		{ "xnet2_stage", "XNet2 Stage", __g_arrPresetXNet2Stage, XRT_COUNT_OF(__g_arrPresetXNet2Stage) },
		{ "network_edges", "Network Edges", __g_arrPresetNetworkEdges, XRT_COUNT_OF(__g_arrPresetNetworkEdges) },
	#endif
};



// 内部函数：__xrtTestFindEntry
static const xrt_test_entry* __xrtTestFindEntry(const char* sId)
{
	size_t i;

	if ( sId == NULL || sId[0] == '\0' ) {
		return NULL;
	}

	for ( i = 0; i < XRT_COUNT_OF(__g_arrXrtTests); i++ ) {
		if ( strcmp(__g_arrXrtTests[i].sId, sId) == 0 ) {
			return &__g_arrXrtTests[i];
		}
	}

	return NULL;
}


// 内部函数：__xrtTestFindPreset
static const xrt_test_preset* __xrtTestFindPreset(const char* sId)
{
	size_t i;

	if ( sId == NULL || sId[0] == '\0' ) {
		return NULL;
	}

	for ( i = 0; i < XRT_COUNT_OF(__g_arrXrtPresets); i++ ) {
		if ( strcmp(__g_arrXrtPresets[i].sId, sId) == 0 ) {
			return &__g_arrXrtPresets[i];
		}
	}

	return NULL;
}


// 内部函数：__xrtTestPrintCatalog
static void __xrtTestPrintCatalog(void)
{
	size_t i;

	printf("Available tests:\n");
	for ( i = 0; i < XRT_COUNT_OF(__g_arrXrtTests); i++ ) {
		printf("  %-20s %s [%s]\n",
			__g_arrXrtTests[i].sId,
			__g_arrXrtTests[i].sName,
			__g_arrXrtTests[i].sGroup);
	}

	printf("\nAvailable presets:\n");
	for ( i = 0; i < XRT_COUNT_OF(__g_arrXrtPresets); i++ ) {
		printf("  %-20s %s\n",
			__g_arrXrtPresets[i].sId,
			__g_arrXrtPresets[i].sName);
	}
	printf("  %-20s %s\n", "all", "Run all registered tests");
}


// 内部函数：__xrtTestInvoke
static int __xrtTestInvoke(const xrt_test_entry* pEntry)
{
	xrtGlobalData* pCore = NULL;
	int iRet;

	if ( pEntry == NULL || pEntry->Proc == NULL ) {
		return 9000;
	}

	if ( (pEntry->iFlags & XRT_TEST_FLAG_SELF_INIT) == 0u ) {
		pCore = xrtInit();
		if ( pCore == NULL ) {
			return 9001;
		}
		#if XRT_TEST_ENABLE_ONERROR
			pCore->OnError = OnError;
		#endif
	}

	iRet = pEntry->Proc(pCore);

	if ( (pEntry->iFlags & XRT_TEST_FLAG_SELF_INIT) == 0u ) {
		xrtUnit();
	}

	return iRet;
}


// 内部函数：__xrtTestRunEntry
static int __xrtTestRunEntry(const xrt_test_entry* pEntry, xrt_test_summary* pSummary)
{
	int iRet;

	if ( pEntry == NULL ) {
		if ( pSummary ) {
			pSummary->iFailCount++;
		}
		return 9002;
	}

	if ( XRT_TEST_RUNNER_QUIET ) {
		printf("[RUN ] %s\n", pEntry->sId);
	} else {
		printf("[RUN ] %s (%s / %s)\n", pEntry->sId, pEntry->sName, pEntry->sGroup);
	}

	iRet = __xrtTestInvoke(pEntry);
	if ( iRet == 0 ) {
		printf("[PASS] %s\n", pEntry->sId);
		if ( pSummary ) {
			pSummary->iPassCount++;
		}
	} else {
		printf("[FAIL] %s (%d)\n", pEntry->sId, iRet);
		if ( pSummary ) {
			pSummary->iFailCount++;
		}
	}

	return iRet;
}


// 内部函数：__xrtTestRunSingle
static int __xrtTestRunSingle(const char* sId, xrt_test_summary* pSummary)
{
	const xrt_test_entry* pEntry = __xrtTestFindEntry(sId);

	if ( pEntry == NULL ) {
		printf("Unknown test id: %s\n\n", sId ? sId : "(null)");
		__xrtTestPrintCatalog();
		if ( pSummary ) {
			pSummary->iFailCount++;
		}
		return 9100;
	}

	return __xrtTestRunEntry(pEntry, pSummary);
}


// 内部函数：__xrtTestRunPreset
static int __xrtTestRunPreset(const char* sId, xrt_test_summary* pSummary)
{
	const xrt_test_preset* pPreset = __xrtTestFindPreset(sId);
	size_t i;
	int iRet = 0;

	if ( pPreset == NULL ) {
		printf("Unknown preset id: %s\n\n", sId ? sId : "(null)");
		__xrtTestPrintCatalog();
		if ( pSummary ) {
			pSummary->iFailCount++;
		}
		return 9200;
	}

	printf("[PRESET] %s\n", pPreset->sId);
	for ( i = 0; i < pPreset->iTestCount; i++ ) {
		iRet = __xrtTestRunSingle(pPreset->arrTestId[i], pSummary);
		if ( iRet != 0 && XRT_TEST_FAIL_FAST ) {
			return iRet;
		}
	}

	return iRet;
}


// 内部函数：__xrtTestRunAll
static int __xrtTestRunAll(xrt_test_summary* pSummary)
{
	size_t i;
	int iRet = 0;

	printf("[PRESET] all\n");
	for ( i = 0; i < XRT_COUNT_OF(__g_arrXrtTests); i++ ) {
		iRet = __xrtTestRunEntry(&__g_arrXrtTests[i], pSummary);
		if ( iRet != 0 && XRT_TEST_FAIL_FAST ) {
			return iRet;
		}
	}

	return iRet;
}


// 内部函数：__xrtTestPrintSummary
static void __xrtTestPrintSummary(const xrt_test_summary* pSummary)
{
	if ( pSummary == NULL ) {
		return;
	}

	printf("\n[SUMMARY] pass=%llu fail=%llu skip=%llu\n",
		(unsigned long long)pSummary->iPassCount,
		(unsigned long long)pSummary->iFailCount,
		(unsigned long long)pSummary->iSkipCount);
}


// 内部函数：__xrtTestResolveCli
static int __xrtTestResolveCli(int argc, char** argv, int* piRunMode, const char** psRunId)
{
	if ( piRunMode == NULL || psRunId == NULL ) {
		return 9300;
	}

	__g_iXrtTestExtraArgCount = 0;
	__g_arrXrtTestExtraArgs = NULL;

	if ( argc <= 1 || argv == NULL ) {
		return 0;
	}

	if ( strcmp(argv[1], "--list") == 0 ) {
		__xrtTestPrintCatalog();
		return 1;
	}
	if ( strcmp(argv[1], "all") == 0 ) {
		*piRunMode = XRT_TEST_MODE_ALL;
		*psRunId = "all";
		return 0;
	}
	if ( strncmp(argv[1], "preset:", 7) == 0 ) {
		*piRunMode = XRT_TEST_MODE_PRESET;
		*psRunId = argv[1] + 7;
		return 0;
	}

	*piRunMode = XRT_TEST_MODE_SINGLE;
	*psRunId = argv[1];
	if ( argc > 2 ) {
		__g_iXrtTestExtraArgCount = argc - 2;
		__g_arrXrtTestExtraArgs = &argv[2];
	}
	return 0;
}



// MAIN相关处理
int main(int argc, char** argv)
{
	int iRunMode = XRT_TEST_RUN_MODE;
	const char* sRunId = (XRT_TEST_RUN_MODE == XRT_TEST_MODE_PRESET) ? XRT_TEST_PRESET_ID : XRT_TEST_SINGLE_ID;
	xrt_test_summary tSummary;
	int iCliRet;
	int iRet = 0;

	memset(&tSummary, 0, sizeof(tSummary));
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	#if defined(_WIN32) || defined(_WIN64)
		SetConsoleOutputCP(65001);
	#endif

	if ( argc > 1 && argv != NULL && strcmp(argv[1], "__subprocess_helper") == 0 ) {
		return __xrtTestSubprocessHelperMain(argc - 2, &argv[2]);
	}

	iCliRet = __xrtTestResolveCli(argc, argv, &iRunMode, &sRunId);
	if ( iCliRet != 0 ) {
		return iCliRet > 0 ? 0 : iCliRet;
	}

	if ( iRunMode == XRT_TEST_MODE_SINGLE ) {
		iRet = __xrtTestRunSingle(sRunId, &tSummary);
	} else if ( iRunMode == XRT_TEST_MODE_PRESET ) {
		iRet = __xrtTestRunPreset(sRunId, &tSummary);
	} else if ( iRunMode == XRT_TEST_MODE_ALL ) {
		iRet = __xrtTestRunAll(&tSummary);
	} else {
		printf("Unsupported run mode: %d\n", iRunMode);
		return 9400;
	}

	__xrtTestPrintSummary(&tSummary);
	return iRet;
}
