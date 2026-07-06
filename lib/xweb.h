#ifndef XRT_XWEB_H
#define XRT_XWEB_H

/*
	xweb 是 XRT 的高层 HTTP 服务基座。

	设计目标：
	1. xhttpd 继续负责 HTTP/1.1 连接、解析、发送等底层工作；
	2. xweb 负责路由、请求参数、常用响应、静态文件等 Web 服务语义；
	3. API 对 C 友好，xlang 的 http.server 标准库只需要薄封装这一层。
*/

#if !defined(XRT_BUILD_CORE)
typedef struct xrt_web_server xwebserver;
typedef struct xrt_web_app xwebapp;
typedef struct xrt_web_request xwebrequest;
typedef struct xrt_web_response xwebresponse;

typedef int xwebaction;

#define XWEB_NEXT 0
#define XWEB_DONE 1
#define XWEB_ASYNC 2
#define XWEB_ERROR (-1)

typedef xwebaction (*xwebhandler)(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData);
typedef bool (*xwebbodybegin)(xwebrequest* pReq, ptr pUserData);
typedef bool (*xwebbodychunk)(xwebrequest* pReq, const void* pData, size_t iLen, ptr pUserData);
typedef xwebaction (*xwebbodyend)(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData);
typedef xfuture* (*xwebbodyendasync)(xwebrequest* pReq, ptr pUserData);
typedef xwebaction (*xweberrorhandler)(xwebrequest* pReq, xwebresponse* pResp, uint32 iStatusCode, const char* sMessage, ptr pUserData);
typedef void (*xwebfree)(ptr pUserData);

typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	uint32 iBodyLimit;
	uint32 iHeaderLimit;
	uint32 iHeaderBytesLimit;
	uint32 iHeaderTimeoutMs;
	uint32 iBodyTimeoutMs;
	uint32 iIdleTimeoutMs;
	uint32 iWriteTimeoutMs;
	uint32 iMultipartPartLimit;
	uint32 iMultipartPartSizeLimit;
	uint32 iWorkerCount;
	const xtlsconfig* pTlsConfig;
} xwebconfig;

typedef struct {
	uint32 iFlags;
	const char* sIndexNames;
	const char* sCacheControl;
	size_t iChunkSize;
} xwebstaticconfig;

#define XWEB_METHOD_ANY        0xFFFFFFFFu
#define XWEB_METHOD_GET        (1u << XHTTPD_METHOD_GET)
#define XWEB_METHOD_HEAD       (1u << XHTTPD_METHOD_HEAD)
#define XWEB_METHOD_POST       (1u << XHTTPD_METHOD_POST)
#define XWEB_METHOD_PUT        (1u << XHTTPD_METHOD_PUT)
#define XWEB_METHOD_DELETE     (1u << XHTTPD_METHOD_DELETE)
#define XWEB_METHOD_PATCH      (1u << XHTTPD_METHOD_PATCH)
#define XWEB_METHOD_OPTIONS    (1u << XHTTPD_METHOD_OPTIONS)

#define XWEB_STATIC_F_NONE             0x00000000u
#define XWEB_STATIC_F_ALLOW_DOTFILES   0x00000001u
#define XWEB_STATIC_F_ALLOW_BACKSLASH  0x00000002u

// 前置公开声明：这些 helper 的实现位于路由与静态文件逻辑之后。
// 单头文件模式下，运行库会在完整实现展开前引用这些符号。
XXAPI bool xrtWebAppStaticEx(xwebapp* pApp, const char* sMount, const char* sRoot, const char* sIndexNames, const char* sCacheControl, size_t iChunkSize, uint32 iFlags);
XXAPI bool xrtWebServerTlsHost(xwebserver* pServer, const char* sHost, const char* sCertFile, const char* sKeyFile);
XXAPI bool xrtWebServerRemoveTlsHost(xwebserver* pServer, const char* sHost);
XXAPI bool xrtWebServerStaticEx(xwebserver* pServer, const char* sMount, const char* sRoot, const char* sIndexNames, const char* sCacheControl, size_t iChunkSize, uint32 iFlags);
XXAPI const char* xrtWebRequestTarget(const xwebrequest* pReq);
XXAPI str xrtWebRequestTargetCopy(const xwebrequest* pReq);
XXAPI str xrtWebRequestTlsSNICopy(const xwebrequest* pReq);
#endif

typedef struct __xweb_route_node __xweb_route_node;
typedef struct __xweb_route __xweb_route;
typedef struct __xweb_static_mount __xweb_static_mount;
typedef struct __xweb_vhost __xweb_vhost;
typedef struct __xweb_sni_cert __xweb_sni_cert;
typedef struct __xweb_stream_ctx __xweb_stream_ctx;

typedef struct {
	char* sText;
	__xweb_route_node* pNode;
} __xweb_route_edge;

typedef struct {
	const char* sName;
	const char* sValue;
	size_t iLen;
} __xweb_param_view;

struct __xweb_route {
	uint32 iMethodMask;
	char* sPattern;
	char** arrParamNames;
	size_t iParamCount;
	xwebhandler pHandler;
	xwebbodybegin pBodyBegin;
	xwebbodychunk pBody;
	xwebbodyend pBodyEnd;
	xwebbodyendasync pBodyEndAsync;
	xwebfree pFreeUserData;
	ptr pUserData;
};

struct __xweb_route_node {
	__xweb_route_edge* arrEdges;
	size_t iEdgeCount;
	size_t iEdgeCap;
	__xweb_route_node* pVar;
	__xweb_route_node* pWild;
	__xweb_route* pRoute;
	char* sParamName;
};

struct __xweb_static_mount {
	char* sMount;
	char* sRoot;
	xwebstaticconfig tConfig;
	__xweb_static_mount* pNext;
};

struct xrt_web_app {
	volatile long iRefCount;
	volatile long iLock;
	__xweb_route_node* arrRoots[XHTTPD_METHOD_OPTIONS + 1];
	__xweb_static_mount* pStaticHead;
	xweberrorhandler pErrorHandler;
	xwebfree pFreeErrorUserData;
	ptr pErrorUserData;
};

struct __xweb_vhost {
	char* sHost;
	xwebapp* pApp;
	__xweb_vhost* pNext;
};

struct __xweb_sni_cert {
	char* sHost;
	char* sCertFile;
	char* sKeyFile;
	__xweb_sni_cert* pNext;
};

struct xrt_web_server {
	xnetengine* pEngine;
	xhttpdserver* pHttpd;
	xwebconfig tConfig;
	xhttpdevents tEvents;
	volatile long iRequestCount;
	volatile long iAppLock;
	volatile long iStreamLock;
	bool bOwnEngine;
	xwebapp* pApp;
	__xweb_vhost* pVHostHead;
	__xweb_sni_cert* pSniHead;
	__xweb_stream_ctx* pStreamHead;
};

struct xrt_web_request {
	xwebserver* pServer;
	xwebapp* pApp;
	xhttpdconn* pConn;
	const xhttpdrequest* pRaw;
	__xweb_param_view arrParams[32];
	size_t iParamCount;
};

struct xrt_web_response {
	xwebrequest* pReq;
	xhttpdresponse* pRaw;
	bool bCommitted;
};

struct __xweb_stream_ctx {
	xhttpdconn* pConn;
	xwebapp* pApp;
	__xweb_route* pRoute;
	xwebrequest tReq;
	xwebresponse tResp;
	__xweb_stream_ctx* pNext;
};


// 内部函数：判断字符串是否相等
static bool __xwebStrEq(const char* sA, const char* sB)
{
	if ( !sA || !sB ) { return false; }
	return strcmp(sA, sB) == 0;
}


// 内部函数：ASCII 字符转大写
static char __xwebAsciiUpper(char ch)
{
	if ( ch >= 'a' && ch <= 'z' ) { return (char)(ch - 32); }
	return ch;
}


// 内部函数：比较 HTTP 方法 token
static bool __xwebTokenEq(const char* sText, size_t iLen, const char* sExpected)
{
	size_t i;
	if ( !sText || !sExpected || strlen(sExpected) != iLen ) { return false; }
	for ( i = 0u; i < iLen; ++i ) {
		if ( __xwebAsciiUpper(sText[i]) != sExpected[i] ) { return false; }
	}
	return true;
}


// 内部函数：复制固定长度字符串
static char* __xwebCopyStrN(const char* sText, size_t iLen)
{
	char* sRet;
	if ( !sText ) { return NULL; }
	sRet = (char*)xrtMalloc(iLen + 1u);
	if ( !sRet ) { return NULL; }
	if ( iLen > 0u ) { memcpy(sRet, sText, iLen); }
	sRet[iLen] = '\0';
	return sRet;
}


// 内部函数：复制 C 字符串
static char* __xwebCopyStr(const char* sText)
{
	if ( !sText ) { return NULL; }
	return __xwebCopyStrN(sText, strlen(sText));
}


// 内部函数：轻量自旋锁
static void __xwebLock(volatile long* pLock)
{
	if ( !pLock ) { return; }
	while ( __xnetAtomicCompareExchange32(pLock, 1, 0) != 0 ) {
		#if defined(_WIN32) || defined(_WIN64)
			Sleep(0);
		#else
			sched_yield();
		#endif
	}
}


// 内部函数：释放轻量自旋锁
static void __xwebUnlock(volatile long* pLock)
{
	if ( pLock ) { __xnetAtomicExchange32(pLock, 0); }
}


// 内部函数：引用 xwebapp
static void __xwebAppRetain(xwebapp* pApp)
{
	if ( pApp ) { __xnetAtomicAddFetch32(&pApp->iRefCount, 1); }
}


// 内部函数：释放 xwebapp 内部资源
static void __xwebAppFree(xwebapp* pApp);


// 内部函数：释放 xwebapp 引用
static void __xwebAppRelease(xwebapp* pApp)
{
	if ( !pApp ) { return; }
	if ( __xnetAtomicAddFetch32(&pApp->iRefCount, -1) == 0 ) {
		__xwebAppFree(pApp);
	}
}


// 内部函数：释放路由对象
static void __xwebRouteDestroy(__xweb_route* pRoute)
{
	if ( !pRoute ) { return; }
	if ( pRoute->pFreeUserData ) { pRoute->pFreeUserData(pRoute->pUserData); }
	if ( pRoute->arrParamNames ) {
		for ( size_t i = 0u; i < pRoute->iParamCount; ++i ) {
			xrtFree(pRoute->arrParamNames[i]);
		}
		xrtFree(pRoute->arrParamNames);
	}
	xrtFree(pRoute->sPattern);
	xrtFree(pRoute);
}


// 内部函数：释放路由节点
static void __xwebRouteNodeDestroy(__xweb_route_node* pNode)
{
	if ( !pNode ) { return; }
	for ( size_t i = 0u; i < pNode->iEdgeCount; ++i ) {
		xrtFree(pNode->arrEdges[i].sText);
		__xwebRouteNodeDestroy(pNode->arrEdges[i].pNode);
	}
	xrtFree(pNode->arrEdges);
	__xwebRouteNodeDestroy(pNode->pVar);
	__xwebRouteNodeDestroy(pNode->pWild);
	__xwebRouteDestroy(pNode->pRoute);
	xrtFree(pNode->sParamName);
	xrtFree(pNode);
}


// 内部函数：创建路由节点
static __xweb_route_node* __xwebRouteNodeCreate(void)
{
	__xweb_route_node* pNode = (__xweb_route_node*)xrtMalloc(sizeof(__xweb_route_node));
	if ( !pNode ) { return NULL; }
	memset(pNode, 0, sizeof(__xweb_route_node));
	return pNode;
}


// 内部函数：添加路由参数名
static bool __xwebRouteAddParam(__xweb_route* pRoute, const char* sName, size_t iLen)
{
	char** arrNew;
	char* sCopy;
	if ( !pRoute || !sName || iLen == 0u ) { return false; }
	arrNew = (char**)xrtRealloc(pRoute->arrParamNames, sizeof(char*) * (pRoute->iParamCount + 1u));
	if ( !arrNew ) { return false; }
	pRoute->arrParamNames = arrNew;
	sCopy = __xwebCopyStrN(sName, iLen);
	if ( !sCopy ) { return false; }
	pRoute->arrParamNames[pRoute->iParamCount++] = sCopy;
	return true;
}


// 内部函数：查找静态边
static __xweb_route_node* __xwebRouteFindEdge(__xweb_route_node* pNode, const char* sSeg, size_t iLen)
{
	if ( !pNode || !sSeg ) { return NULL; }
	for ( size_t i = 0u; i < pNode->iEdgeCount; ++i ) {
		const char* sText = pNode->arrEdges[i].sText;
		if ( strlen(sText) == iLen && memcmp(sText, sSeg, iLen) == 0 ) {
			return pNode->arrEdges[i].pNode;
		}
	}
	return NULL;
}


// 内部函数：获取或创建静态边
static __xweb_route_node* __xwebRouteEnsureEdge(__xweb_route_node* pNode, const char* sSeg, size_t iLen)
{
	__xweb_route_edge* arrNew;
	__xweb_route_node* pChild;
	char* sCopy;
	if ( !pNode || !sSeg || iLen == 0u ) { return NULL; }
	pChild = __xwebRouteFindEdge(pNode, sSeg, iLen);
	if ( pChild ) { return pChild; }
	if ( pNode->iEdgeCount == pNode->iEdgeCap ) {
		size_t iNextCap = pNode->iEdgeCap == 0u ? 4u : pNode->iEdgeCap * 2u;
		arrNew = (__xweb_route_edge*)xrtRealloc(pNode->arrEdges, sizeof(__xweb_route_edge) * iNextCap);
		if ( !arrNew ) { return NULL; }
		pNode->arrEdges = arrNew;
		pNode->iEdgeCap = iNextCap;
	}
	sCopy = __xwebCopyStrN(sSeg, iLen);
	pChild = __xwebRouteNodeCreate();
	if ( !sCopy || !pChild ) {
		xrtFree(sCopy);
		__xwebRouteNodeDestroy(pChild);
		return NULL;
	}
	pNode->arrEdges[pNode->iEdgeCount].sText = sCopy;
	pNode->arrEdges[pNode->iEdgeCount].pNode = pChild;
	pNode->iEdgeCount++;
	return pChild;
}


// 内部函数：跳过路径中的斜杠
static const char* __xwebSkipSlash(const char* sText)
{
	while ( sText && *sText == '/' ) { sText++; }
	return sText;
}


// 内部函数：读取下一个路径段
static bool __xwebNextSegment(const char** ppCur, const char** ppSeg, size_t* pSegLen, bool* pLast)
{
	const char* sCur;
	const char* sEnd;
	if ( !ppCur || !*ppCur || !ppSeg || !pSegLen || !pLast ) { return false; }
	sCur = __xwebSkipSlash(*ppCur);
	if ( *sCur == '\0' ) { return false; }
	sEnd = sCur;
	while ( *sEnd && *sEnd != '/' ) { sEnd++; }
	*ppSeg = sCur;
	*pSegLen = (size_t)(sEnd - sCur);
	*pLast = *sEnd == '\0' || *__xwebSkipSlash(sEnd) == '\0';
	*ppCur = sEnd;
	return true;
}


// 内部函数：解析路由方法掩码
static uint32 __xwebMethodToMask(uint32 iMethod)
{
	if ( iMethod <= XHTTPD_METHOD_OPTIONS ) { return 1u << iMethod; }
	return 0u;
}


// xrtWebMethodMask 相关处理
XXAPI uint32 xrtWebMethodMask(const char* sMethods)
{
	uint32 iMask = 0u;
	const char* p;
	const char* sToken;
	size_t iLen;

	if ( !sMethods ) { return 0u; }
	p = sMethods;
	while ( *p ) {
		while ( *p == ' ' || *p == '\t' || *p == ',' || *p == '|' || *p == ';' ) { p++; }
		if ( *p == '\0' ) { break; }
		sToken = p;
		while ( *p && *p != ' ' && *p != '\t' && *p != ',' && *p != '|' && *p != ';' ) { p++; }
		iLen = (size_t)(p - sToken);
		if ( iLen == 1u && sToken[0] == '*' ) {
			return XWEB_METHOD_ANY;
		} else if ( iLen == 3u && __xwebTokenEq(sToken, iLen, "ANY") ) {
			return XWEB_METHOD_ANY;
		} else if ( iLen == 3u && __xwebTokenEq(sToken, iLen, "GET") ) {
			iMask |= XWEB_METHOD_GET;
		} else if ( iLen == 4u && __xwebTokenEq(sToken, iLen, "HEAD") ) {
			iMask |= XWEB_METHOD_HEAD;
		} else if ( iLen == 4u && __xwebTokenEq(sToken, iLen, "POST") ) {
			iMask |= XWEB_METHOD_POST;
		} else if ( iLen == 3u && __xwebTokenEq(sToken, iLen, "PUT") ) {
			iMask |= XWEB_METHOD_PUT;
		} else if ( iLen == 6u && __xwebTokenEq(sToken, iLen, "DELETE") ) {
			iMask |= XWEB_METHOD_DELETE;
		} else if ( iLen == 5u && __xwebTokenEq(sToken, iLen, "PATCH") ) {
			iMask |= XWEB_METHOD_PATCH;
		} else if ( iLen == 7u && __xwebTokenEq(sToken, iLen, "OPTIONS") ) {
			iMask |= XWEB_METHOD_OPTIONS;
		} else {
			return 0u;
		}
	}
	return iMask;
}


// 内部函数：判断路径段是否为变量段
static bool __xwebSegmentIsVar(const char* sSeg, size_t iLen, const char** ppName, size_t* pNameLen)
{
	if ( !sSeg || iLen < 3u || sSeg[0] != '{' || sSeg[iLen - 1u] != '}' ) { return false; }
	if ( ppName ) { *ppName = sSeg + 1u; }
	if ( pNameLen ) { *pNameLen = iLen - 2u; }
	return true;
}


// 内部函数：校验变量名
static bool __xwebValidName(const char* sName, size_t iLen)
{
	if ( !sName || iLen == 0u ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		char ch = sName[i];
		bool bOk = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
			(ch >= '0' && ch <= '9') || ch == '_';
		if ( !bOk ) { return false; }
	}
	return true;
}


// 内部函数：校验静态文件相对路径
static bool __xwebSafeStaticPath(const char* sPath, uint32 iFlags)
{
	const char* sCur;
	const char* sSeg;
	size_t iLen;
	bool bLast;
	if ( !sPath ) { return false; }
	if ( sPath[0] == '\0' ) { return true; }
	if ( sPath[0] == '/' || sPath[0] == '\\' ) { return false; }
	if ( (iFlags & XWEB_STATIC_F_ALLOW_BACKSLASH) == 0u && strchr(sPath, '\\') != NULL ) { return false; }
	sCur = sPath;
	while ( __xwebNextSegment(&sCur, &sSeg, &iLen, &bLast) ) {
		(void)bLast;
		if ( iLen == 0u ) { continue; }
		if ( iLen == 1u && sSeg[0] == '.' ) { return false; }
		if ( iLen == 2u && sSeg[0] == '.' && sSeg[1] == '.' ) { return false; }
		if ( (iFlags & XWEB_STATIC_F_ALLOW_DOTFILES) == 0u && sSeg[0] == '.' ) { return false; }
	}
	return true;
}


// 内部函数：判断静态目录挂载是否匹配
static bool __xwebMountMatches(const char* sMount, const char* sPath, const char** ppRel)
{
	size_t iMountLen;
	if ( !sMount || !sPath || !ppRel ) { return false; }
	if ( sMount[0] == '/' && sMount[1] == '\0' ) {
		*ppRel = sPath[0] == '/' ? sPath + 1u : sPath;
		return true;
	}
	iMountLen = strlen(sMount);
	if ( strncmp(sPath, sMount, iMountLen) != 0 ) { return false; }
	if ( sPath[iMountLen] != '\0' && sPath[iMountLen] != '/' ) { return false; }
	*ppRel = sPath[iMountLen] == '/' ? sPath + iMountLen + 1u : sPath + iMountLen;
	return true;
}


// 内部函数：匹配路由树
static __xweb_route* __xwebRouteMatchNode(__xweb_route_node* pNode, const char* sPath, __xweb_param_view* arrParams, size_t* pParamCount)
{
	const char* sSeg;
	size_t iLen;
	bool bLast;
	__xweb_route_node* pStatic;
	__xweb_route* pRoute;
	if ( !pNode || !sPath || !pParamCount ) { return NULL; }
	sPath = __xwebSkipSlash(sPath);
	if ( *sPath == '\0' ) { return pNode->pRoute; }
	if ( !__xwebNextSegment(&sPath, &sSeg, &iLen, &bLast) ) { return pNode->pRoute; }

	// 优先级：静态段 > 变量段 > 尾通配符。
	pStatic = __xwebRouteFindEdge(pNode, sSeg, iLen);
	if ( pStatic ) {
		pRoute = __xwebRouteMatchNode(pStatic, bLast ? "" : sPath, arrParams, pParamCount);
		if ( pRoute ) { return pRoute; }
	}
	if ( pNode->pVar && *pParamCount < 32u ) {
		size_t iIndex = *pParamCount;
		arrParams[iIndex].sName = pNode->pVar->sParamName;
		arrParams[iIndex].sValue = sSeg;
		arrParams[iIndex].iLen = iLen;
		(*pParamCount)++;
		pRoute = __xwebRouteMatchNode(pNode->pVar, bLast ? "" : sPath, arrParams, pParamCount);
		if ( pRoute ) { return pRoute; }
		*pParamCount = iIndex;
	}
	if ( pNode->pWild && pNode->pWild->pRoute && *pParamCount < 32u ) {
		size_t iIndex = *pParamCount;
		const char* sRest = sSeg;
		const char* sTail = sSeg + strlen(sSeg);
		arrParams[iIndex].sName = pNode->pWild->sParamName ? pNode->pWild->sParamName : "*";
		arrParams[iIndex].sValue = sRest;
		arrParams[iIndex].iLen = (size_t)(sTail - sRest);
		(*pParamCount)++;
		return pNode->pWild->pRoute;
	}
	return NULL;
}


// 内部函数：匹配路由
static __xweb_route* __xwebRouteMatch(xwebapp* pApp, uint32 iMethod, const char* sPath, __xweb_param_view* arrParams, size_t* pParamCount)
{
	__xweb_route_node* pRoot;
	if ( pParamCount ) { *pParamCount = 0u; }
	if ( !pApp || !sPath || !pParamCount || iMethod > XHTTPD_METHOD_OPTIONS ) { return NULL; }
	pRoot = pApp->arrRoots[iMethod];
	if ( !pRoot ) { return NULL; }
	return __xwebRouteMatchNode(pRoot, sPath, arrParams, pParamCount);
}


// 内部函数：按注册 pattern 查找路由叶子节点
static __xweb_route_node* __xwebRouteFindPatternLeaf(__xweb_route_node* pRoot, const char* sPattern)
{
	const char* sCur;
	const char* sSeg;
	size_t iLen;
	bool bLast;
	__xweb_route_node* pNode;
	if ( !pRoot || !sPattern ) { return NULL; }
	pNode = pRoot;
	sCur = sPattern;
	while ( __xwebNextSegment(&sCur, &sSeg, &iLen, &bLast) ) {
		const char* sName;
		size_t iNameLen;
		if ( iLen == 1u && sSeg[0] == '*' ) {
			if ( !bLast ) { return NULL; }
			return pNode->pWild;
		}
		if ( __xwebSegmentIsVar(sSeg, iLen, &sName, &iNameLen) ) {
			(void)sName;
			(void)iNameLen;
			pNode = pNode->pVar;
		} else {
			pNode = __xwebRouteFindEdge(pNode, sSeg, iLen);
		}
		if ( !pNode ) { return NULL; }
	}
	return pNode;
}


// 内部函数：清理本次注册过程中已经挂载的部分路由
static void __xwebRouteClearPattern(xwebapp* pApp, uint32 iMethodMask, const char* sPattern)
{
	if ( !pApp || !sPattern ) { return; }
	for ( uint32 iMethod = 0u; iMethod <= XHTTPD_METHOD_OPTIONS; ++iMethod ) {
		__xweb_route_node* pNode;
		if ( iMethod == XHTTPD_METHOD_UNKNOWN ) { continue; }
		if ( (iMethodMask & __xwebMethodToMask(iMethod)) == 0u ) { continue; }
		pNode = __xwebRouteFindPatternLeaf(pApp->arrRoots[iMethod], sPattern);
		if ( pNode && pNode->pRoute ) {
			__xwebRouteDestroy(pNode->pRoute);
			pNode->pRoute = NULL;
		}
	}
}


// 内部函数：释放静态挂载
static void __xwebStaticMountDestroy(__xweb_static_mount* pMount)
{
	while ( pMount ) {
		__xweb_static_mount* pNext = pMount->pNext;
		xrtFree(pMount->sMount);
		xrtFree(pMount->sRoot);
		xrtFree(pMount);
		pMount = pNext;
	}
}


// 内部函数：释放 xwebapp 内部资源
static void __xwebAppFree(xwebapp* pApp)
{
	if ( !pApp ) { return; }
	for ( size_t i = 0u; i <= XHTTPD_METHOD_OPTIONS; ++i ) {
		__xwebRouteNodeDestroy(pApp->arrRoots[i]);
	}
	__xwebStaticMountDestroy(pApp->pStaticHead);
	if ( pApp->pFreeErrorUserData ) { pApp->pFreeErrorUserData(pApp->pErrorUserData); }
	xrtFree(pApp);
}


// 内部函数：响应简单文本
static xwebaction __xwebRespondSimple(xwebrequest* pReq, xwebresponse* pResp, uint32 iStatusCode, const char* sText)
{
	if ( !pReq || !pResp ) { return XWEB_ERROR; }
	xrtWebResponseStatus(pResp, iStatusCode, NULL);
	if ( !xrtWebResponseText(pResp, sText ? sText : "", "text/plain; charset=utf-8") ) { return XWEB_ERROR; }
	return XWEB_DONE;
}


// 内部函数：响应错误，优先交给 app 的错误处理器
static xwebaction __xwebRespondError(xwebrequest* pReq, xwebresponse* pResp, uint32 iStatusCode, const char* sText)
{
	xweberrorhandler pHandler = NULL;
	ptr pUserData = NULL;
	xwebaction iAction;
	if ( !pReq || !pResp ) { return XWEB_ERROR; }
	if ( pReq->pApp ) {
		__xwebLock(&pReq->pApp->iLock);
		pHandler = pReq->pApp->pErrorHandler;
		pUserData = pReq->pApp->pErrorUserData;
		__xwebUnlock(&pReq->pApp->iLock);
	}
	if ( pHandler ) {
		iAction = pHandler(pReq, pResp, iStatusCode, sText ? sText : "", pUserData);
		if ( iAction == XWEB_DONE || iAction == XWEB_ASYNC || pResp->bCommitted ) { return iAction; }
	}
	return __xwebRespondSimple(pReq, pResp, iStatusCode, sText);
}


// 内部函数：处理静态文件
static bool __xwebFormatHttpDateUnix(int64 iUnixTime, char* sOut, size_t iOutCap)
{
	static const char* arrWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static const char* arrMonth[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	time_t tRaw;
	struct tm tTm;
	if ( !sOut || iOutCap < 32u ) { return false; }
	if ( iUnixTime < 0 ) { iUnixTime = 0; }
	tRaw = (time_t)iUnixTime;
#if defined(_WIN32) || defined(_WIN64)
# ifdef __TINYC__
	{
		struct tm* pTm = gmtime(&tRaw);
		if ( !pTm ) { return false; }
		tTm = *pTm;
	}
# else
	if ( gmtime_s(&tTm, &tRaw) != 0 ) { return false; }
# endif
#else
	if ( gmtime_r(&tRaw, &tTm) == NULL ) { return false; }
#endif
	snprintf(sOut, iOutCap, "%s, %02d %s %04d %02d:%02d:%02d GMT",
		arrWeek[tTm.tm_wday >= 0 && tTm.tm_wday < 7 ? tTm.tm_wday : 0],
		tTm.tm_mday,
		arrMonth[tTm.tm_mon >= 0 && tTm.tm_mon < 12 ? tTm.tm_mon : 0],
		tTm.tm_year + 1900,
		tTm.tm_hour,
		tTm.tm_min,
		tTm.tm_sec);
	return true;
}


static void __xwebStaticETag(char* sOut, size_t iOutCap, size_t iSize, int64 iMTime)
{
	if ( !sOut || iOutCap == 0u ) { return; }
	snprintf(sOut, iOutCap, "\"%llx-%llx\"",
		(unsigned long long)iSize,
		(unsigned long long)(iMTime > 0 ? iMTime : 0));
}


static bool __xwebStaticETagMatches(const char* sHeader, const char* sETag)
{
	size_t iETagLen;
	const char* p;
	if ( !sHeader || !sETag ) { return false; }
	iETagLen = strlen(sETag);
	p = sHeader;
	while ( *p ) {
		const char* sBegin;
		const char* sEnd;
		while ( *p == ' ' || *p == '\t' || *p == ',' ) { p++; }
		if ( *p == '*' ) { return true; }
		sBegin = p;
		while ( *p && *p != ',' ) { p++; }
		sEnd = p;
		while ( sEnd > sBegin && (sEnd[-1] == ' ' || sEnd[-1] == '\t') ) { sEnd--; }
		if ( (size_t)(sEnd - sBegin) == iETagLen && strncmp(sBegin, sETag, iETagLen) == 0 ) { return true; }
		if ( sEnd - sBegin > 2 && sBegin[0] == 'W' && sBegin[1] == '/' ) {
			sBegin += 2;
			if ( (size_t)(sEnd - sBegin) == iETagLen && strncmp(sBegin, sETag, iETagLen) == 0 ) { return true; }
		}
	}
	return false;
}


static bool __xwebParseSizeT(const char** ppText, size_t* pOut)
{
	const char* p;
	size_t iVal = 0u;
	size_t iMax = ~(size_t)0u;
	bool bAny = false;
	if ( !ppText || !*ppText || !pOut ) { return false; }
	p = *ppText;
	while ( *p >= '0' && *p <= '9' ) {
		size_t iDigit = (size_t)(*p - '0');
		if ( iVal > (iMax - iDigit) / 10u ) { return false; }
		iVal = iVal * 10u + iDigit;
		p++;
		bAny = true;
	}
	if ( !bAny ) { return false; }
	*ppText = p;
	*pOut = iVal;
	return true;
}


static int __xwebParseByteRange(const char* sRange, size_t iFileSize, size_t* pStart, size_t* pLen)
{
	const char* p;
	size_t iStart = 0u;
	size_t iEnd = 0u;
	size_t iSuffix = 0u;
	if ( pStart ) { *pStart = 0u; }
	if ( pLen ) { *pLen = 0u; }
	if ( !sRange || !pStart || !pLen ) { return 0; }
	while ( *sRange == ' ' || *sRange == '\t' ) { sRange++; }
	if ( strncmp(sRange, "bytes=", 6u) != 0 ) { return 0; }
	p = sRange + 6u;
	while ( *p == ' ' || *p == '\t' ) { p++; }
	if ( strchr(p, ',') != NULL ) { return 0; }
	if ( iFileSize == 0u ) { return -1; }
	if ( *p == '-' ) {
		p++;
		if ( !__xwebParseSizeT(&p, &iSuffix) || iSuffix == 0u ) { return -1; }
		while ( *p == ' ' || *p == '\t' ) { p++; }
		if ( *p != '\0' ) { return -1; }
		if ( iSuffix >= iFileSize ) {
			*pStart = 0u;
			*pLen = iFileSize;
		} else {
			*pStart = iFileSize - iSuffix;
			*pLen = iSuffix;
		}
		return 1;
	}
	if ( !__xwebParseSizeT(&p, &iStart) ) { return -1; }
	if ( *p != '-' ) { return -1; }
	p++;
	if ( *p >= '0' && *p <= '9' ) {
		if ( !__xwebParseSizeT(&p, &iEnd) ) { return -1; }
		if ( iEnd < iStart ) { return -1; }
	} else {
		iEnd = iFileSize - 1u;
	}
	while ( *p == ' ' || *p == '\t' ) { p++; }
	if ( *p != '\0' ) { return -1; }
	if ( iStart >= iFileSize ) { return -1; }
	if ( iEnd >= iFileSize ) { iEnd = iFileSize - 1u; }
	*pStart = iStart;
	*pLen = iEnd - iStart + 1u;
	return 1;
}


static bool __xwebTryStatic(xwebrequest* pReq, xwebresponse* pResp)
{
		__xweb_static_mount* pMount;
	const char* sRel;
	char sDecoded[XHTTPD_TARGET_CAP];
	size_t iDecodedLen = 0u;
	char* sJoined = NULL;
	char* sPath = NULL;
	bool bHead;
	bool bOk = false;
	size_t iFileSize;
	int64 iMTime;
	char sETag[64];
	char sLastModified[64];
	const char* sIfNoneMatch;
	const char* sIfRange;
	const char* sRange;
	size_t iRangeStart = 0u;
	size_t iRangeLen = 0u;
	int iRangeState = 0;
	if ( !pReq || !pResp || !pReq->pApp || !pReq->pRaw ) { return false; }
	if ( pReq->pRaw->iMethod != XHTTPD_METHOD_GET && pReq->pRaw->iMethod != XHTTPD_METHOD_HEAD ) { return false; }
	bHead = pReq->pRaw->iMethod == XHTTPD_METHOD_HEAD;
	for ( pMount = pReq->pApp->pStaticHead; pMount; pMount = pMount->pNext ) {
		const char* sIndexNames;
		char* sIndexPath = NULL;
		char* sIndexNorm = NULL;
		if ( !__xwebMountMatches(pMount->sMount, pReq->pRaw->sPath, &sRel) ) { continue; }
		if ( !xrtPercentDecodeTo(sRel, strlen(sRel), sDecoded, sizeof(sDecoded), &iDecodedLen, false) ) {
			(void)__xwebRespondError(pReq, pResp, 400u, "Bad Request");
			return true;
		}
		if ( !__xwebSafeStaticPath(sDecoded, pMount->tConfig.iFlags) ) {
			(void)__xwebRespondError(pReq, pResp, 403u, "Forbidden");
			return true;
		}
		sJoined = (char*)xrtPathJoin(2u, (str)pMount->sRoot, (str)(sDecoded[0] ? sDecoded : "."));
		sPath = (char*)xrtPathNormalize((str)sJoined, 0u);
		xrtFree(sJoined);
		sJoined = NULL;
		if ( !sPath ) {
			(void)__xwebRespondError(pReq, pResp, 500u, "Internal Server Error");
			return true;
		}
		if ( xrtDirExists((str)sPath) ) {
			sIndexNames = pMount->tConfig.sIndexNames ? pMount->tConfig.sIndexNames : "index.html;index.htm";
			while ( sIndexNames && *sIndexNames ) {
				const char* sSemi = strchr(sIndexNames, ';');
				size_t iNameLen = sSemi ? (size_t)(sSemi - sIndexNames) : strlen(sIndexNames);
				if ( iNameLen > 0u ) {
					char* sName = __xwebCopyStrN(sIndexNames, iNameLen);
					sIndexPath = (char*)xrtPathJoin(2u, (str)sPath, (str)sName);
					xrtFree(sName);
					sIndexNorm = (char*)xrtPathNormalize((str)sIndexPath, 0u);
					xrtFree(sIndexPath);
					sIndexPath = NULL;
					if ( sIndexNorm && xrtFileExists((str)sIndexNorm) ) {
						xrtFree(sPath);
						sPath = sIndexNorm;
						sIndexNorm = NULL;
						break;
					}
					xrtFree(sIndexNorm);
					sIndexNorm = NULL;
				}
				if ( !sSemi ) { break; }
				sIndexNames = sSemi + 1u;
			}
		}
		if ( !xrtFileExists((str)sPath) ) {
			xrtFree(sPath);
			sPath = NULL;
			continue;
		}
		iFileSize = xrtFileGetSize((str)sPath);
		iMTime = xrtFileGetChangeTime((str)sPath);
		__xwebStaticETag(sETag, sizeof(sETag), iFileSize, iMTime);
		if ( !__xwebFormatHttpDateUnix(iMTime > XRT_TIME_19700101 ? iMTime - XRT_TIME_19700101 : 0, sLastModified, sizeof(sLastModified)) ) {
			sLastModified[0] = '\0';
		}
		xrtWebResponseStatus(pResp, 200u, NULL);
		(void)xrtWebResponseSetHeader(pResp, "Content-Type", xrtWebMimeByPath(sPath));
		(void)xrtWebResponseSetHeader(pResp, "Accept-Ranges", "bytes");
		(void)xrtWebResponseSetHeader(pResp, "ETag", sETag);
		if ( sLastModified[0] ) { (void)xrtWebResponseSetHeader(pResp, "Last-Modified", sLastModified); }
		if ( pMount->tConfig.sCacheControl && pMount->tConfig.sCacheControl[0] ) {
			(void)xrtWebResponseSetHeader(pResp, "Cache-Control", pMount->tConfig.sCacheControl);
		}
		sIfNoneMatch = xrtHttpdRequestHeader(pReq->pRaw, "If-None-Match");
		if ( __xwebStaticETagMatches(sIfNoneMatch, sETag) ) {
			xrtWebResponseStatus(pResp, 304u, NULL);
			(void)xrtWebResponseSetHeader(pResp, "Content-Length", "0");
			pResp->bCommitted = xrtHttpdConnRespond(pReq->pConn, pResp->pRaw) == XRT_NET_OK;
			xrtFree(sPath);
			return pResp->bCommitted;
		}
		sRange = xrtHttpdRequestHeader(pReq->pRaw, "Range");
		sIfRange = xrtHttpdRequestHeader(pReq->pRaw, "If-Range");
		iRangeState = __xwebParseByteRange(sRange, iFileSize, &iRangeStart, &iRangeLen);
		if ( iRangeState != 0 && sIfRange && sIfRange[0] && strcmp(sIfRange, sETag) != 0 && (!sLastModified[0] || strcmp(sIfRange, sLastModified) != 0) ) {
			iRangeState = 0;
		}
		if ( iRangeState < 0 ) {
			char sContentRange[64];
			snprintf(sContentRange, sizeof(sContentRange), "bytes */%llu", (unsigned long long)iFileSize);
			xrtWebResponseStatus(pResp, 416u, NULL);
			(void)xrtWebResponseSetHeader(pResp, "Content-Range", sContentRange);
			(void)xrtWebResponseSetHeader(pResp, "Content-Length", "0");
			pResp->bCommitted = xrtHttpdConnRespond(pReq->pConn, pResp->pRaw) == XRT_NET_OK;
			xrtFree(sPath);
			return pResp->bCommitted;
		}
		if ( iRangeState > 0 ) {
			char sLen[32];
			char sContentRange[96];
			xrtWebResponseStatus(pResp, 206u, NULL);
			snprintf(sLen, sizeof(sLen), "%llu", (unsigned long long)iRangeLen);
			snprintf(sContentRange, sizeof(sContentRange), "bytes %llu-%llu/%llu",
				(unsigned long long)iRangeStart,
				(unsigned long long)(iRangeStart + iRangeLen - 1u),
				(unsigned long long)iFileSize);
			(void)xrtWebResponseSetHeader(pResp, "Content-Length", sLen);
			(void)xrtWebResponseSetHeader(pResp, "Content-Range", sContentRange);
			if ( bHead ) {
				pResp->bCommitted = xrtHttpdConnRespond(pReq->pConn, pResp->pRaw) == XRT_NET_OK;
				bOk = true;
			} else {
				pResp->bCommitted = xrtHttpdConnSendFileRange(pReq->pConn, pResp->pRaw, sPath, iRangeStart, iRangeLen, pMount->tConfig.iChunkSize) == XRT_NET_OK;
				bOk = pResp->bCommitted;
			}
			xrtFree(sPath);
			return bOk;
		}
		if ( bHead ) {
			char sLen[32];
			snprintf(sLen, sizeof(sLen), "%llu", (unsigned long long)iFileSize);
			(void)xrtWebResponseSetHeader(pResp, "Content-Length", sLen);
			pResp->bCommitted = xrtHttpdConnRespond(pReq->pConn, pResp->pRaw) == XRT_NET_OK;
			bOk = true;
		} else {
			bOk = xrtWebResponseFile(pResp, sPath, pMount->tConfig.iChunkSize);
		}
		xrtFree(sPath);
		return bOk;
	}
	return false;
}


// 内部函数：Host 匹配，忽略大小写并允许请求头携带端口
static bool __xwebHostMatches(const char* sRule, const char* sHost)
{
	size_t iRuleLen;
	size_t iHostLen;
	if ( !sRule || !sHost ) { return false; }
	iRuleLen = strlen(sRule);
	iHostLen = 0u;
	while ( sHost[iHostLen] && sHost[iHostLen] != ':' ) { iHostLen++; }
	if ( iRuleLen != iHostLen ) { return false; }
	for ( size_t i = 0u; i < iRuleLen; ++i ) {
		char a = sRule[i];
		char b = sHost[i];
		if ( a >= 'A' && a <= 'Z' ) { a = (char)(a + 32); }
		if ( b >= 'A' && b <= 'Z' ) { b = (char)(b + 32); }
		if ( a != b ) { return false; }
	}
	return true;
}


// 内部函数：按请求选择并引用 app
static xwebapp* __xwebServerAcquireApp(xwebserver* pServer, const xhttpdrequest* pReq)
{
	xwebapp* pApp = NULL;
	const char* sHost = NULL;
	if ( !pServer ) { return NULL; }
	if ( pReq ) { sHost = xrtHttpdRequestHeader(pReq, "Host"); }
	__xwebLock(&pServer->iAppLock);
	if ( sHost && sHost[0] ) {
		for ( __xweb_vhost* pCur = pServer->pVHostHead; pCur; pCur = pCur->pNext ) {
			if ( __xwebHostMatches(pCur->sHost, sHost) ) {
				pApp = pCur->pApp;
				break;
			}
		}
	}
	if ( !pApp ) { pApp = pServer->pApp; }
	__xwebAppRetain(pApp);
	__xwebUnlock(&pServer->iAppLock);
	return pApp;
}


// 内部函数：释放虚拟主机表
static void __xwebVHostDestroy(__xweb_vhost* pHost)
{
	while ( pHost ) {
		__xweb_vhost* pNext = pHost->pNext;
		xrtFree(pHost->sHost);
		__xwebAppRelease(pHost->pApp);
		xrtFree(pHost);
		pHost = pNext;
	}
}


// 内部函数：释放 SNI 证书表
static void __xwebSniCertDestroy(__xweb_sni_cert* pCert)
{
	while ( pCert ) {
		__xweb_sni_cert* pNext = pCert->pNext;
		xrtFree(pCert->sHost);
		xrtFree(pCert->sCertFile);
		xrtFree(pCert->sKeyFile);
		xrtFree(pCert);
		pCert = pNext;
	}
}


// 内部函数：TLS SNI 回调，按域名切换当前会话证书
static void __xwebOnSNI(xtlssession* pSession, const char* sHostName, ptr pUserData)
{
	xwebserver* pServer = (xwebserver*)pUserData;
	const char* sCertFile = NULL;
	const char* sKeyFile = NULL;
	if ( !pServer || !pSession || !sHostName || !sHostName[0] ) { return; }
	__xwebLock(&pServer->iAppLock);
	for ( __xweb_sni_cert* pCur = pServer->pSniHead; pCur; pCur = pCur->pNext ) {
		if ( __xwebHostMatches(pCur->sHost, sHostName) ) {
			sCertFile = pCur->sCertFile;
			sKeyFile = pCur->sKeyFile;
			break;
		}
	}
	__xwebUnlock(&pServer->iAppLock);
	if ( sCertFile && sKeyFile ) {
		(void)xrtNetTlsSessionSetCert(pSession, sCertFile, sKeyFile);
	}
}


// 内部函数：高层请求入口
static bool __xwebRequestMultipartBoundary(const xwebrequest* pReq, xrtstrview* pBoundary);
XXAPI const void* xrtWebRequestBody(const xwebrequest* pReq, size_t* pLen);


static bool __xwebRequestMultipartWithinLimits(const xwebrequest* pReq, uint32 iPartLimit, uint32 iPartSizeLimit)
{
	xrtstrview tBoundary;
	const char* sBody;
	size_t iBodyLen = 0u;
	size_t iOffset = 0u;
	uint32 iCount = 0u;
	xrtmultipartpartview tPart;

	if ( iPartLimit == 0u && iPartSizeLimit == 0u ) { return true; }
	if ( !__xwebRequestMultipartBoundary(pReq, &tBoundary) ) { return true; }
	sBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( !sBody ) { return true; }

	while ( xrtMultipartNextN(sBody, iBodyLen, tBoundary.sPtr, tBoundary.iLen, &iOffset, &tPart) ) {
		iCount++;
		if ( iPartLimit > 0u && iCount > iPartLimit ) { return false; }
		if ( iPartSizeLimit > 0u && tPart.tBody.iLen > (size_t)iPartSizeLimit ) { return false; }
	}
	return true;
}


// 内部函数：销毁 xweb 流式请求上下文
static void __xwebStreamCtxDestroy(__xweb_stream_ctx* pCtx)
{
	if ( !pCtx ) { return; }
	if ( pCtx->pApp ) { __xwebAppRelease(pCtx->pApp); }
	xrtFree(pCtx);
}


// 内部函数：加入 xweb 流式请求上下文
static bool __xwebStreamCtxPut(xwebserver* pServer, __xweb_stream_ctx* pCtx)
{
	if ( !pServer || !pCtx || !pCtx->pConn ) { return false; }
	__xwebLock(&pServer->iStreamLock);
	pCtx->pNext = pServer->pStreamHead;
	pServer->pStreamHead = pCtx;
	__xwebUnlock(&pServer->iStreamLock);
	return true;
}


// 内部函数：查找 xweb 流式请求上下文
static __xweb_stream_ctx* __xwebStreamCtxFind(xwebserver* pServer, xhttpdconn* pConn)
{
	__xweb_stream_ctx* pCur;
	if ( !pServer || !pConn ) { return NULL; }
	__xwebLock(&pServer->iStreamLock);
	for ( pCur = pServer->pStreamHead; pCur; pCur = pCur->pNext ) {
		if ( pCur->pConn == pConn ) { break; }
	}
	__xwebUnlock(&pServer->iStreamLock);
	return pCur;
}


// 内部函数：移除 xweb 流式请求上下文
static __xweb_stream_ctx* __xwebStreamCtxDetach(xwebserver* pServer, xhttpdconn* pConn)
{
	__xweb_stream_ctx* pPrev = NULL;
	__xweb_stream_ctx* pCur;
	if ( !pServer || !pConn ) { return NULL; }
	__xwebLock(&pServer->iStreamLock);
	pCur = pServer->pStreamHead;
	while ( pCur ) {
		if ( pCur->pConn == pConn ) {
			if ( pPrev ) { pPrev->pNext = pCur->pNext; }
			else { pServer->pStreamHead = pCur->pNext; }
			pCur->pNext = NULL;
			__xwebUnlock(&pServer->iStreamLock);
			return pCur;
		}
		pPrev = pCur;
		pCur = pCur->pNext;
	}
	__xwebUnlock(&pServer->iStreamLock);
	return NULL;
}


// 内部函数：清理所有 xweb 流式请求上下文
static void __xwebStreamCtxClearAll(xwebserver* pServer)
{
	__xweb_stream_ctx* pHead;
	if ( !pServer ) { return; }
	__xwebLock(&pServer->iStreamLock);
	pHead = pServer->pStreamHead;
	pServer->pStreamHead = NULL;
	__xwebUnlock(&pServer->iStreamLock);
	while ( pHead ) {
		__xweb_stream_ctx* pNext = pHead->pNext;
		pHead->pNext = NULL;
		__xwebStreamCtxDestroy(pHead);
		pHead = pNext;
	}
}


// 内部函数：xhttpd request body streaming begin 回调
static bool __xwebOnRequestBodyBegin(ptr pOwner, xhttpdserver* pHttpd, xhttpdconn* pConn, const xhttpdrequest* pRawReq)
{
	xwebserver* pServer = (xwebserver*)pOwner;
	xwebapp* pApp;
	__xweb_stream_ctx* pCtx;
	__xweb_route* pRoute;
	(void)pHttpd;
	if ( !pServer || !pConn || !pRawReq ) { return false; }
	pApp = __xwebServerAcquireApp(pServer, pRawReq);
	if ( !pApp ) { return false; }
	pCtx = (__xweb_stream_ctx*)xrtMalloc(sizeof(__xweb_stream_ctx));
	if ( !pCtx ) {
		__xwebAppRelease(pApp);
		return false;
	}
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->pConn = pConn;
	pCtx->pApp = pApp;
	pCtx->tReq.pServer = pServer;
	pCtx->tReq.pApp = pApp;
	pCtx->tReq.pConn = pConn;
	pCtx->tReq.pRaw = pRawReq;
	pCtx->tResp.pReq = &pCtx->tReq;
	__xwebLock(&pApp->iLock);
	pRoute = __xwebRouteMatch(pApp, pRawReq->iMethod, pRawReq->sPath, pCtx->tReq.arrParams, &pCtx->tReq.iParamCount);
	__xwebUnlock(&pApp->iLock);
	if ( !pRoute || !pRoute->pBody || (!pRoute->pBodyEnd && !pRoute->pBodyEndAsync) ) {
		__xwebStreamCtxDestroy(pCtx);
		return false;
	}
	pCtx->pRoute = pRoute;
	if ( pRoute->pBodyBegin && !pRoute->pBodyBegin(&pCtx->tReq, pRoute->pUserData) ) {
		__xwebStreamCtxDestroy(pCtx);
		return false;
	}
	if ( !__xwebStreamCtxPut(pServer, pCtx) ) {
		__xwebStreamCtxDestroy(pCtx);
		return false;
	}
	__xnetAtomicAddFetch32(&pServer->iRequestCount, 1);
	return true;
}


// 内部函数：xhttpd request body streaming chunk 回调
static bool __xwebOnRequestBody(ptr pOwner, xhttpdserver* pHttpd, xhttpdconn* pConn, const xhttpdrequest* pRawReq, const void* pData, size_t iLen)
{
	xwebserver* pServer = (xwebserver*)pOwner;
	__xweb_stream_ctx* pCtx;
	(void)pHttpd;
	(void)pRawReq;
	if ( !pServer || !pConn ) { return false; }
	pCtx = __xwebStreamCtxFind(pServer, pConn);
	if ( !pCtx || !pCtx->pRoute || !pCtx->pRoute->pBody ) { return false; }
	return pCtx->pRoute->pBody(&pCtx->tReq, pData, iLen, pCtx->pRoute->pUserData);
}


// 内部函数：xhttpd request body streaming end 回调
static xfuture* __xwebOnRequestBodyEndAsync(ptr pOwner, xhttpdserver* pHttpd, xhttpdconn* pConn, const xhttpdrequest* pRawReq)
{
	xwebserver* pServer = (xwebserver*)pOwner;
	__xweb_stream_ctx* pCtx;
	xfuture* pFuture;
	(void)pHttpd;
	(void)pRawReq;
	if ( !pServer || !pConn ) { return NULL; }
	pCtx = __xwebStreamCtxFind(pServer, pConn);
	if ( !pCtx || !pCtx->pRoute || !pCtx->pRoute->pBodyEndAsync ) { return NULL; }
	pFuture = pCtx->pRoute->pBodyEndAsync(&pCtx->tReq, pCtx->pRoute->pUserData);
	if ( !pFuture ) { return NULL; }
	pCtx = __xwebStreamCtxDetach(pServer, pConn);
	__xwebStreamCtxDestroy(pCtx);
	return pFuture;
}


static bool __xwebOnRequestBodyEnd(ptr pOwner, xhttpdserver* pHttpd, xhttpdconn* pConn, const xhttpdrequest* pRawReq, xhttpdresponse* pRawResp)
{
	xwebserver* pServer = (xwebserver*)pOwner;
	__xweb_stream_ctx* pCtx;
	xwebaction iAction = XWEB_NEXT;
	bool bRet = true;
	(void)pHttpd;
	(void)pRawReq;
	if ( !pServer || !pConn || !pRawResp ) { return false; }
	pCtx = __xwebStreamCtxDetach(pServer, pConn);
	if ( !pCtx || !pCtx->pRoute || !pCtx->pRoute->pBodyEnd ) {
		__xwebStreamCtxDestroy(pCtx);
		return false;
	}
	pCtx->tResp.pRaw = pRawResp;
	(void)xrtWebResponseSetHeader(&pCtx->tResp, "X-Content-Type-Options", "nosniff");
	iAction = pCtx->pRoute->pBodyEnd(&pCtx->tReq, &pCtx->tResp, pCtx->pRoute->pUserData);
	if ( iAction == XWEB_DONE || pCtx->tResp.bCommitted ) {
		bRet = true;
	}
	else if ( iAction == XWEB_ERROR ) {
		(void)__xwebRespondError(&pCtx->tReq, &pCtx->tResp, 500u, "Internal Server Error");
		bRet = true;
	}
	else if ( iAction == XWEB_ASYNC ) {
		/* 当前流式请求上下文会在本函数结束释放，暂不开放异步完成语义。 */
		(void)__xwebRespondError(&pCtx->tReq, &pCtx->tResp, 500u, "Async stream body end is not supported");
		bRet = true;
	}
	else {
		(void)__xwebRespondError(&pCtx->tReq, &pCtx->tResp, 404u, "Not Found");
		bRet = true;
	}
	__xwebStreamCtxDestroy(pCtx);
	return bRet;
}


// 内部函数：xhttpd close 回调，释放未结束的流式请求上下文
static void __xwebOnClose(ptr pOwner, xhttpdserver* pHttpd, xhttpdconn* pConn, xnet_result iReason)
{
	xwebserver* pServer = (xwebserver*)pOwner;
	__xweb_stream_ctx* pCtx;
	(void)pHttpd;
	(void)iReason;
	if ( !pServer || !pConn ) { return; }
	pCtx = __xwebStreamCtxDetach(pServer, pConn);
	__xwebStreamCtxDestroy(pCtx);
}


static bool __xwebOnRequest(ptr pOwner, xhttpdserver* pHttpd, xhttpdconn* pConn, const xhttpdrequest* pRawReq, xhttpdresponse* pRawResp)
{
	xwebserver* pServer = (xwebserver*)pOwner;
	xwebapp* pApp;
	xwebrequest tReq;
	xwebresponse tResp;
	__xweb_route* pRoute;
	xwebaction iAction;
	(void)pHttpd;
	if ( !pServer || !pConn || !pRawReq || !pRawResp ) { return false; }
	__xnetAtomicAddFetch32(&pServer->iRequestCount, 1);
	pApp = __xwebServerAcquireApp(pServer, pRawReq);
	if ( !pApp ) { return false; }
	memset(&tReq, 0, sizeof(tReq));
	memset(&tResp, 0, sizeof(tResp));
	tReq.pServer = pServer;
	tReq.pApp = pApp;
	tReq.pConn = pConn;
	tReq.pRaw = pRawReq;
	tResp.pReq = &tReq;
	tResp.pRaw = pRawResp;
	(void)xrtWebResponseSetHeader(&tResp, "X-Content-Type-Options", "nosniff");
	if ( !__xwebRequestMultipartWithinLimits(&tReq, pServer->tConfig.iMultipartPartLimit, pServer->tConfig.iMultipartPartSizeLimit) ) {
		(void)__xwebRespondError(&tReq, &tResp, 413u, "Payload Too Large");
		__xwebAppRelease(pApp);
		return true;
	}
	__xwebLock(&pApp->iLock);
	pRoute = __xwebRouteMatch(pApp, pRawReq->iMethod, pRawReq->sPath, tReq.arrParams, &tReq.iParamCount);
	__xwebUnlock(&pApp->iLock);
	if ( pRoute && pRoute->pHandler ) {
		iAction = pRoute->pHandler(&tReq, &tResp, pRoute->pUserData);
		if ( iAction == XWEB_ASYNC ) { __xwebAppRelease(pApp); return true; }
		if ( iAction == XWEB_DONE || tResp.bCommitted ) { __xwebAppRelease(pApp); return true; }
		if ( iAction == XWEB_ERROR ) {
			(void)__xwebRespondError(&tReq, &tResp, 500u, "Internal Server Error");
			__xwebAppRelease(pApp);
			return true;
		}
	}
	if ( __xwebTryStatic(&tReq, &tResp) ) { __xwebAppRelease(pApp); return true; }
	(void)__xwebRespondError(&tReq, &tResp, 404u, "Not Found");
	__xwebAppRelease(pApp);
	return true;
}


// 初始化 xweb 配置
XXAPI void xrtWebConfigInit(xwebconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xwebconfig));
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0u);
	pCfg->iBacklog = 128u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->iBodyLimit = pCfg->iRecvLimit;
	pCfg->iHeaderLimit = 100u;
	pCfg->iHeaderBytesLimit = 64u * 1024u;
	pCfg->iHeaderTimeoutMs = 30000u;
	pCfg->iBodyTimeoutMs = 30000u;
	pCfg->iIdleTimeoutMs = 60000u;
	pCfg->iWriteTimeoutMs = 30000u;
	pCfg->iMultipartPartLimit = 1000u;
	pCfg->iMultipartPartSizeLimit = 0u;
	pCfg->iWorkerCount = 1u;
}


// 初始化静态文件配置
XXAPI void xrtWebStaticConfigInit(xwebstaticconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xwebstaticconfig));
	pCfg->iFlags = XWEB_STATIC_F_NONE;
	pCfg->sIndexNames = "index.html;index.htm";
	pCfg->iChunkSize = 64u * 1024u;
}


// 创建 xweb app
XXAPI xwebapp* xrtWebAppCreate(void)
{
	xwebapp* pApp = (xwebapp*)xrtMalloc(sizeof(xwebapp));
	if ( !pApp ) { return NULL; }
	memset(pApp, 0, sizeof(xwebapp));
	pApp->iRefCount = 1;
	return pApp;
}


// 引用 xweb app
XXAPI void xrtWebAppRetain(xwebapp* pApp)
{
	__xwebAppRetain(pApp);
}


// 释放 xweb app
XXAPI void xrtWebAppRelease(xwebapp* pApp)
{
	__xwebAppRelease(pApp);
}


// 销毁 xweb app
XXAPI void xrtWebAppDestroy(xwebapp* pApp)
{
	__xwebAppRelease(pApp);
}


// 设置 app 错误处理器
XXAPI bool xrtWebAppError(xwebapp* pApp, xweberrorhandler pHandler, ptr pUserData, xwebfree pFreeUserData)
{
	xwebfree pOldFree;
	ptr pOldUserData;
	if ( !pApp ) { return false; }
	__xwebLock(&pApp->iLock);
	pOldFree = pApp->pFreeErrorUserData;
	pOldUserData = pApp->pErrorUserData;
	pApp->pErrorHandler = pHandler;
	pApp->pErrorUserData = pUserData;
	pApp->pFreeErrorUserData = pFreeUserData;
	__xwebUnlock(&pApp->iLock);
	if ( pOldFree ) { pOldFree(pOldUserData); }
	return true;
}


// 创建 xweb 服务
XXAPI xwebserver* xrtWebServerCreate(xnetengine* pEngine, const xwebconfig* pCfg)
{
	xwebserver* pServer;
	xhttpdconfig tHttpdCfg;
	xwebconfig tCfg;
	xtlsconfig tTlsCfg;
	bool bOwnEngine = false;
	if ( pCfg ) { tCfg = *pCfg; }
	else { xrtWebConfigInit(&tCfg); }
	if ( !pEngine ) {
		xnetengineconfig tEngineCfg;
		xrtNetEngineConfigInit(&tEngineCfg);
		if ( tCfg.iWorkerCount > 0u ) { tEngineCfg.iWorkerCount = tCfg.iWorkerCount; }
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		if ( !pEngine ) { return NULL; }
		if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
			xrtNetEngineDestroy(pEngine);
			return NULL;
		}
		bOwnEngine = true;
	}
	pServer = (xwebserver*)xrtMalloc(sizeof(xwebserver));
	if ( !pServer ) {
		if ( bOwnEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
		return NULL;
	}
	memset(pServer, 0, sizeof(xwebserver));
	pServer->pEngine = pEngine;
	pServer->bOwnEngine = bOwnEngine;
	pServer->pApp = xrtWebAppCreate();
	if ( !pServer->pApp ) {
		if ( bOwnEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
		xrtFree(pServer);
		return NULL;
	}
	pServer->tConfig = tCfg;
	memset(&pServer->tEvents, 0, sizeof(pServer->tEvents));
	pServer->tEvents.OnRequest = __xwebOnRequest;
	pServer->tEvents.OnRequestBodyBegin = __xwebOnRequestBodyBegin;
	pServer->tEvents.OnRequestBody = __xwebOnRequestBody;
	pServer->tEvents.OnRequestBodyEnd = __xwebOnRequestBodyEnd;
	pServer->tEvents.OnRequestBodyEndAsync = __xwebOnRequestBodyEndAsync;
	pServer->tEvents.OnClose = __xwebOnClose;
	xrtHttpdConfigInit(&tHttpdCfg);
	tHttpdCfg.tBindAddr = pServer->tConfig.tBindAddr;
	tHttpdCfg.iFlags = pServer->tConfig.iFlags;
	tHttpdCfg.iBacklog = pServer->tConfig.iBacklog;
	tHttpdCfg.iRecvLimit = pServer->tConfig.iRecvLimit;
	tHttpdCfg.iBodyLimit = pServer->tConfig.iBodyLimit;
	tHttpdCfg.iHeaderLimit = pServer->tConfig.iHeaderLimit;
	tHttpdCfg.iHeaderBytesLimit = pServer->tConfig.iHeaderBytesLimit;
	tHttpdCfg.iHeaderTimeoutMs = pServer->tConfig.iHeaderTimeoutMs;
	tHttpdCfg.iBodyTimeoutMs = pServer->tConfig.iBodyTimeoutMs;
	tHttpdCfg.iIdleTimeoutMs = pServer->tConfig.iIdleTimeoutMs;
	tHttpdCfg.iWriteTimeoutMs = pServer->tConfig.iWriteTimeoutMs;
	tHttpdCfg.pTlsConfig = pServer->tConfig.pTlsConfig;
	if ( tHttpdCfg.pTlsConfig ) {
		tTlsCfg = *tHttpdCfg.pTlsConfig;
		tTlsCfg.OnSNI = __xwebOnSNI;
		tTlsCfg.pSNIUserData = pServer;
		tHttpdCfg.pTlsConfig = &tTlsCfg;
	}
	pServer->pHttpd = xrtHttpdCreate(pEngine, &tHttpdCfg, &pServer->tEvents, pServer);
	if ( !pServer->pHttpd ) {
		xrtWebAppDestroy(pServer->pApp);
		if ( bOwnEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
		xrtFree(pServer);
		return NULL;
	}
	return pServer;
}


// 使用常用参数创建 xweb 服务
XXAPI xwebserver* xrtWebServerCreateHostEx(xnetengine* pEngine, const char* sHost, uint16 iPort, uint32 iBacklog, uint32 iRecvLimit, uint32 iBodyLimit, uint32 iWorkerCount)
{
	xwebconfig tCfg;
	xrtWebConfigInit(&tCfg);
	if ( sHost && sHost[0] ) {
		if ( xrtNetAddrParse(&tCfg.tBindAddr, sHost, iPort) != XRT_NET_OK ) { return NULL; }
	}
	else {
		xrtNetAddrInitAny(&tCfg.tBindAddr, AF_INET, iPort);
	}
	if ( iBacklog > 0u ) { tCfg.iBacklog = iBacklog; }
	if ( iRecvLimit > 0u ) { tCfg.iRecvLimit = iRecvLimit; }
	if ( iBodyLimit > 0u ) { tCfg.iBodyLimit = iBodyLimit; }
	if ( iWorkerCount > 0u ) { tCfg.iWorkerCount = iWorkerCount; }
	return xrtWebServerCreate(pEngine, &tCfg);
}


// 使用常用参数创建 TLS xweb 服务
XXAPI xwebserver* xrtWebServerCreateHostTlsEx(xnetengine* pEngine, const char* sCertFile, const char* sKeyFile, const char* sHost, uint16 iPort, uint32 iBacklog, uint32 iRecvLimit, uint32 iBodyLimit, uint32 iWorkerCount)
{
	xwebconfig tCfg;
	xtlsconfig tTlsCfg;
	if ( !sCertFile || !sCertFile[0] || !sKeyFile || !sKeyFile[0] ) { return NULL; }
	memset(&tTlsCfg, 0, sizeof(tTlsCfg));
	tTlsCfg.sCertFile = sCertFile;
	tTlsCfg.sKeyFile = sKeyFile;
	xrtWebConfigInit(&tCfg);
	if ( sHost && sHost[0] ) {
		if ( xrtNetAddrParse(&tCfg.tBindAddr, sHost, iPort) != XRT_NET_OK ) { return NULL; }
	}
	else {
		xrtNetAddrInitAny(&tCfg.tBindAddr, AF_INET, iPort);
	}
	if ( iBacklog > 0u ) { tCfg.iBacklog = iBacklog; }
	if ( iRecvLimit > 0u ) { tCfg.iRecvLimit = iRecvLimit; }
	if ( iBodyLimit > 0u ) { tCfg.iBodyLimit = iBodyLimit; }
	if ( iWorkerCount > 0u ) { tCfg.iWorkerCount = iWorkerCount; }
	tCfg.pTlsConfig = &tTlsCfg;
	return xrtWebServerCreate(pEngine, &tCfg);
}


// 销毁 xweb 服务
XXAPI void xrtWebServerDestroy(xwebserver* pServer)
{
	if ( !pServer ) { return; }
	xrtWebServerStop(pServer);
	if ( pServer->pHttpd ) { xrtHttpdDestroy(pServer->pHttpd); }
	__xwebStreamCtxClearAll(pServer);
	__xwebVHostDestroy(pServer->pVHostHead);
	__xwebSniCertDestroy(pServer->pSniHead);
	xrtWebAppDestroy(pServer->pApp);
	if ( pServer->bOwnEngine && pServer->pEngine ) {
		xrtNetEngineStop(pServer->pEngine);
		xrtNetEngineDestroy(pServer->pEngine);
	}
	xrtFree(pServer);
}


// 启动 xweb 服务
XXAPI xnet_result xrtWebServerStart(xwebserver* pServer)
{
	if ( !pServer || !pServer->pHttpd ) { return XRT_NET_ERROR; }
	return xrtHttpdStart(pServer->pHttpd);
}


// 启动 xweb 服务，返回 bool，供窄 ABI 绑定使用
XXAPI bool xrtWebServerStartOk(xwebserver* pServer)
{
	return xrtWebServerStart(pServer) == XRT_NET_OK;
}


// 停止 xweb 服务
XXAPI void xrtWebServerStop(xwebserver* pServer)
{
	if ( pServer && pServer->pHttpd ) { xrtHttpdStop(pServer->pHttpd); }
}


// 获取 xweb 实际监听端口

// 优雅 drain xweb 服务：停止监听并等待已有连接结束
XXAPI bool xrtWebServerDrain(xwebserver* pServer, uint32 iTimeoutMs)
{
	return (!pServer || !pServer->pHttpd) ? true : xrtHttpdDrain(pServer->pHttpd, iTimeoutMs);
}


XXAPI uint16 xrtWebServerPort(const xwebserver* pServer)
{
	return (pServer && pServer->pHttpd) ? xrtHttpdBoundPort(pServer->pHttpd) : 0u;
}


// 获取累计请求数量
XXAPI uint32 xrtWebServerRequestCount(const xwebserver* pServer)
{
	return pServer ? (uint32)__xnetAtomicLoad32((volatile long*)&pServer->iRequestCount) : 0u;
}


// 获取当前连接数量
XXAPI uint32 xrtWebServerConnectionCount(xwebserver* pServer)
{
	return (pServer && pServer->pHttpd) ? xrtHttpdConnectionCount(pServer->pHttpd) : 0u;
}


// 替换默认 app。调用者仍持有自己的 app 引用。
XXAPI bool xrtWebServerSetApp(xwebserver* pServer, xwebapp* pApp)
{
	xwebapp* pOld;
	if ( !pServer || !pApp ) { return false; }
	__xwebAppRetain(pApp);
	__xwebLock(&pServer->iAppLock);
	pOld = pServer->pApp;
	pServer->pApp = pApp;
	__xwebUnlock(&pServer->iAppLock);
	__xwebAppRelease(pOld);
	return true;
}


// 热替换默认 app，语义同 SetApp，命名用于 reload 场景。
XXAPI bool xrtWebServerReloadApp(xwebserver* pServer, xwebapp* pApp)
{
	return xrtWebServerSetApp(pServer, pApp);
}


// 设置虚拟主机 app。sHost 不包含端口，匹配时忽略大小写。
XXAPI bool xrtWebServerHost(xwebserver* pServer, const char* sHost, xwebapp* pApp)
{
	__xweb_vhost* pCur;
	__xweb_vhost* pNew;
	char* sCopy;
	if ( !pServer || !sHost || !sHost[0] || !pApp ) { return false; }
	sCopy = __xwebCopyStr(sHost);
	pNew = (__xweb_vhost*)xrtMalloc(sizeof(__xweb_vhost));
	if ( !sCopy || !pNew ) {
		xrtFree(sCopy);
		xrtFree(pNew);
		return false;
	}
	memset(pNew, 0, sizeof(__xweb_vhost));
	pNew->sHost = sCopy;
	__xwebAppRetain(pApp);
	pNew->pApp = pApp;

	__xwebLock(&pServer->iAppLock);
	for ( pCur = pServer->pVHostHead; pCur; pCur = pCur->pNext ) {
		if ( __xwebHostMatches(pCur->sHost, sHost) ) {
			xwebapp* pOld = pCur->pApp;
			xrtFree(pCur->sHost);
			pCur->sHost = pNew->sHost;
			pCur->pApp = pNew->pApp;
			__xwebUnlock(&pServer->iAppLock);
			xrtFree(pNew);
			__xwebAppRelease(pOld);
			return true;
		}
	}
	pNew->pNext = pServer->pVHostHead;
	pServer->pVHostHead = pNew;
	__xwebUnlock(&pServer->iAppLock);
	return true;
}


// 移除虚拟主机 app
XXAPI bool xrtWebServerRemoveHost(xwebserver* pServer, const char* sHost)
{
	__xweb_vhost* pPrev = NULL;
	__xweb_vhost* pCur;
	if ( !pServer || !sHost || !sHost[0] ) { return false; }
	__xwebLock(&pServer->iAppLock);
	pCur = pServer->pVHostHead;
	while ( pCur ) {
		if ( __xwebHostMatches(pCur->sHost, sHost) ) {
			if ( pPrev ) { pPrev->pNext = pCur->pNext; }
			else { pServer->pVHostHead = pCur->pNext; }
			__xwebUnlock(&pServer->iAppLock);
			xrtFree(pCur->sHost);
			__xwebAppRelease(pCur->pApp);
			xrtFree(pCur);
			return true;
		}
		pPrev = pCur;
		pCur = pCur->pNext;
	}
	__xwebUnlock(&pServer->iAppLock);
	return false;
}


// 设置 TLS SNI host 证书。重复 host 会替换证书路径。
XXAPI bool xrtWebServerTlsHost(xwebserver* pServer, const char* sHost, const char* sCertFile, const char* sKeyFile)
{
	__xweb_sni_cert* pCur;
	__xweb_sni_cert* pNew;
	char* sHostCopy;
	char* sCertCopy;
	char* sKeyCopy;
	if ( !pServer || !sHost || !sHost[0] || !sCertFile || !sCertFile[0] || !sKeyFile || !sKeyFile[0] ) {
		return false;
	}
	sHostCopy = __xwebCopyStr(sHost);
	sCertCopy = __xwebCopyStr(sCertFile);
	sKeyCopy = __xwebCopyStr(sKeyFile);
	pNew = (__xweb_sni_cert*)xrtMalloc(sizeof(__xweb_sni_cert));
	if ( !sHostCopy || !sCertCopy || !sKeyCopy || !pNew ) {
		xrtFree(sHostCopy);
		xrtFree(sCertCopy);
		xrtFree(sKeyCopy);
		xrtFree(pNew);
		return false;
	}
	memset(pNew, 0, sizeof(__xweb_sni_cert));
	pNew->sHost = sHostCopy;
	pNew->sCertFile = sCertCopy;
	pNew->sKeyFile = sKeyCopy;

	__xwebLock(&pServer->iAppLock);
	for ( pCur = pServer->pSniHead; pCur; pCur = pCur->pNext ) {
		if ( __xwebHostMatches(pCur->sHost, sHost) ) {
			char* sOldHost = pCur->sHost;
			char* sOldCert = pCur->sCertFile;
			char* sOldKey = pCur->sKeyFile;
			pCur->sHost = pNew->sHost;
			pCur->sCertFile = pNew->sCertFile;
			pCur->sKeyFile = pNew->sKeyFile;
			__xwebUnlock(&pServer->iAppLock);
			xrtFree(sOldHost);
			xrtFree(sOldCert);
			xrtFree(sOldKey);
			xrtFree(pNew);
			return true;
		}
	}
	pNew->pNext = pServer->pSniHead;
	pServer->pSniHead = pNew;
	__xwebUnlock(&pServer->iAppLock);
	return true;
}


// 移除 TLS SNI host 证书
XXAPI bool xrtWebServerRemoveTlsHost(xwebserver* pServer, const char* sHost)
{
	__xweb_sni_cert* pPrev = NULL;
	__xweb_sni_cert* pCur;
	if ( !pServer || !sHost || !sHost[0] ) { return false; }
	__xwebLock(&pServer->iAppLock);
	pCur = pServer->pSniHead;
	while ( pCur ) {
		if ( __xwebHostMatches(pCur->sHost, sHost) ) {
			if ( pPrev ) { pPrev->pNext = pCur->pNext; }
			else { pServer->pSniHead = pCur->pNext; }
			__xwebUnlock(&pServer->iAppLock);
			pCur->pNext = NULL;
			__xwebSniCertDestroy(pCur);
			return true;
		}
		pPrev = pCur;
		pCur = pCur->pNext;
	}
	__xwebUnlock(&pServer->iAppLock);
	return false;
}


// 设置默认 app 错误处理器
XXAPI bool xrtWebServerError(xwebserver* pServer, xweberrorhandler pHandler, ptr pUserData, xwebfree pFreeUserData)
{
	return pServer ? xrtWebAppError(pServer->pApp, pHandler, pUserData, pFreeUserData) : false;
}


// 内部函数：注册 app 路由核心逻辑
static bool __xwebAppRouteCore(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, xwebbodybegin pBodyBegin, xwebbodychunk pBody, xwebbodyend pBodyEnd, xwebbodyendasync pBodyEndAsync, ptr pUserData, xwebfree pFreeUserData)
{
	const char* sCur;
	const char* sSeg;
	size_t iLen;
	bool bLast;
	bool bAnyMethod = false;
	bool bUserDataOwnerAssigned = false;
	uint32 iRegisteredMask = 0u;
	if ( !pApp || !sPattern || (!pHandler && (!pBody || (!pBodyEnd && !pBodyEndAsync))) ) { return false; }
	if ( sPattern[0] != '/' ) { return false; }
	__xwebLock(&pApp->iLock);
	for ( uint32 iMethod = 0u; iMethod <= XHTTPD_METHOD_OPTIONS; ++iMethod ) {
		__xweb_route_node* pNode;
		__xweb_route* pRoute;
		if ( iMethod == XHTTPD_METHOD_UNKNOWN ) { continue; }
		if ( iMethodMask != XWEB_METHOD_ANY && (iMethodMask & __xwebMethodToMask(iMethod)) == 0u ) { continue; }
		bAnyMethod = true;
		pRoute = (__xweb_route*)xrtMalloc(sizeof(__xweb_route));
		if ( !pRoute ) {
			__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
			__xwebUnlock(&pApp->iLock);
			return false;
		}
		memset(pRoute, 0, sizeof(__xweb_route));
		pRoute->iMethodMask = __xwebMethodToMask(iMethod);
		pRoute->sPattern = __xwebCopyStr(sPattern);
		pRoute->pHandler = pHandler;
		pRoute->pBodyBegin = pBodyBegin;
		pRoute->pBody = pBody;
		pRoute->pBodyEnd = pBodyEnd;
		pRoute->pBodyEndAsync = pBodyEndAsync;
		pRoute->pUserData = pUserData;
		// 一个 RouteEx 调用可能同时注册多个方法；userData 只允许一个 route 拥有析构权。
		if ( !bUserDataOwnerAssigned ) {
			pRoute->pFreeUserData = pFreeUserData;
			bUserDataOwnerAssigned = true;
		}
		if ( !pRoute->sPattern ) {
			__xwebRouteDestroy(pRoute);
			__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
			__xwebUnlock(&pApp->iLock);
			return false;
		}
		if ( !pApp->arrRoots[iMethod] ) {
			pApp->arrRoots[iMethod] = __xwebRouteNodeCreate();
			if ( !pApp->arrRoots[iMethod] ) {
				__xwebRouteDestroy(pRoute);
				__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
				__xwebUnlock(&pApp->iLock);
				return false;
			}
		}
		pNode = pApp->arrRoots[iMethod];
		sCur = sPattern;
		while ( __xwebNextSegment(&sCur, &sSeg, &iLen, &bLast) ) {
			const char* sName;
			size_t iNameLen;
			if ( iLen == 1u && sSeg[0] == '*' ) {
				if ( !bLast ) {
					__xwebRouteDestroy(pRoute);
					__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
					__xwebUnlock(&pApp->iLock);
					return false;
				}
				if ( !pNode->pWild ) {
					pNode->pWild = __xwebRouteNodeCreate();
					if ( !pNode->pWild ) {
						__xwebRouteDestroy(pRoute);
						__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
						__xwebUnlock(&pApp->iLock);
						return false;
					}
					pNode->pWild->sParamName = __xwebCopyStr("*");
					if ( !pNode->pWild->sParamName ) {
						__xwebRouteDestroy(pRoute);
						__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
						__xwebUnlock(&pApp->iLock);
						return false;
					}
				}
				pNode = pNode->pWild;
				break;
			}
			if ( __xwebSegmentIsVar(sSeg, iLen, &sName, &iNameLen) ) {
				if ( !__xwebValidName(sName, iNameLen) ) {
					__xwebRouteDestroy(pRoute);
					__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
					__xwebUnlock(&pApp->iLock);
					return false;
				}
				if ( !__xwebRouteAddParam(pRoute, sName, iNameLen) ) {
					__xwebRouteDestroy(pRoute);
					__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
					__xwebUnlock(&pApp->iLock);
					return false;
				}
				if ( !pNode->pVar ) {
					pNode->pVar = __xwebRouteNodeCreate();
					if ( !pNode->pVar ) {
						__xwebRouteDestroy(pRoute);
						__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
						__xwebUnlock(&pApp->iLock);
						return false;
					}
					pNode->pVar->sParamName = __xwebCopyStrN(sName, iNameLen);
					if ( !pNode->pVar->sParamName ) {
						__xwebRouteDestroy(pRoute);
						__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
						__xwebUnlock(&pApp->iLock);
						return false;
					}
				}
				pNode = pNode->pVar;
			} else {
				pNode = __xwebRouteEnsureEdge(pNode, sSeg, iLen);
				if ( !pNode ) {
					__xwebRouteDestroy(pRoute);
					__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
					__xwebUnlock(&pApp->iLock);
					return false;
				}
			}
		}
		if ( pNode->pRoute ) {
			__xwebRouteDestroy(pRoute);
			__xwebRouteClearPattern(pApp, iRegisteredMask, sPattern);
			__xwebUnlock(&pApp->iLock);
			return false;
		}
		pNode->pRoute = pRoute;
		iRegisteredMask |= __xwebMethodToMask(iMethod);
	}
	__xwebUnlock(&pApp->iLock);
	return bAnyMethod;
}


// 在 app 中注册路由，可指定 userData 析构回调
XXAPI bool xrtWebAppRouteEx(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, ptr pUserData, xwebfree pFreeUserData)
{
	return __xwebAppRouteCore(pApp, iMethodMask, sPattern, pHandler, NULL, NULL, NULL, NULL, pUserData, pFreeUserData);
}


// 在 app 中注册 request body 流式路由，可指定 userData 析构回调
XXAPI bool xrtWebAppStreamRouteEx(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyend pEnd, ptr pUserData, xwebfree pFreeUserData)
{
	return __xwebAppRouteCore(pApp, iMethodMask, sPattern, NULL, pBegin, pBody, pEnd, NULL, pUserData, pFreeUserData);
}


// 在 app 中注册 request body 流式路由
XXAPI bool xrtWebAppStreamRoute(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyend pEnd, ptr pUserData)
{
	return xrtWebAppStreamRouteEx(pApp, iMethodMask, sPattern, pBegin, pBody, pEnd, pUserData, NULL);
}


XXAPI bool xrtWebAppStreamRouteAsyncEx(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyendasync pEndAsync, ptr pUserData, xwebfree pFreeUserData)
{
	return __xwebAppRouteCore(pApp, iMethodMask, sPattern, NULL, pBegin, pBody, NULL, pEndAsync, pUserData, pFreeUserData);
}


XXAPI bool xrtWebAppStreamRouteAsync(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyendasync pEndAsync, ptr pUserData)
{
	return xrtWebAppStreamRouteAsyncEx(pApp, iMethodMask, sPattern, pBegin, pBody, pEndAsync, pUserData, NULL);
}


// 在 app 中注册路由
XXAPI bool xrtWebAppRoute(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, ptr pUserData)
{
	return xrtWebAppRouteEx(pApp, iMethodMask, sPattern, pHandler, pUserData, NULL);
}


// 在 app 中注册 GET 路由
XXAPI bool xrtWebAppGet(xwebapp* pApp, const char* sPattern, xwebhandler pHandler, ptr pUserData)
{
	return xrtWebAppRoute(pApp, XWEB_METHOD_GET, sPattern, pHandler, pUserData);
}


// 在 app 中注册 POST 路由
XXAPI bool xrtWebAppPost(xwebapp* pApp, const char* sPattern, xwebhandler pHandler, ptr pUserData)
{
	return xrtWebAppRoute(pApp, XWEB_METHOD_POST, sPattern, pHandler, pUserData);
}


// 在 app 中注册任意方法路由
XXAPI bool xrtWebAppAny(xwebapp* pApp, const char* sPattern, xwebhandler pHandler, ptr pUserData)
{
	return xrtWebAppRoute(pApp, XWEB_METHOD_ANY, sPattern, pHandler, pUserData);
}


// 注册默认 app 路由，可指定 userData 析构回调
XXAPI bool xrtWebServerRouteEx(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, ptr pUserData, xwebfree pFreeUserData)
{
	bool bRet;
	xwebapp* pApp;
	if ( !pServer ) { return false; }
	__xwebLock(&pServer->iAppLock);
	pApp = pServer->pApp;
	__xwebAppRetain(pApp);
	__xwebUnlock(&pServer->iAppLock);
	bRet = xrtWebAppRouteEx(pApp, iMethodMask, sPattern, pHandler, pUserData, pFreeUserData);
	__xwebAppRelease(pApp);
	return bRet;
}


// 注册默认 app 路由
XXAPI bool xrtWebServerRoute(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, ptr pUserData)
{
	return xrtWebServerRouteEx(pServer, iMethodMask, sPattern, pHandler, pUserData, NULL);
}


// 注册默认 app request body 流式路由，可指定 userData 析构回调
XXAPI bool xrtWebServerStreamRouteEx(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyend pEnd, ptr pUserData, xwebfree pFreeUserData)
{
	bool bRet;
	xwebapp* pApp;
	if ( !pServer ) { return false; }
	__xwebLock(&pServer->iAppLock);
	pApp = pServer->pApp;
	__xwebAppRetain(pApp);
	__xwebUnlock(&pServer->iAppLock);
	bRet = xrtWebAppStreamRouteEx(pApp, iMethodMask, sPattern, pBegin, pBody, pEnd, pUserData, pFreeUserData);
	__xwebAppRelease(pApp);
	return bRet;
}


// 注册默认 app request body 流式路由
XXAPI bool xrtWebServerStreamRoute(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyend pEnd, ptr pUserData)
{
	return xrtWebServerStreamRouteEx(pServer, iMethodMask, sPattern, pBegin, pBody, pEnd, pUserData, NULL);
}


XXAPI bool xrtWebServerStreamRouteAsyncEx(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyendasync pEndAsync, ptr pUserData, xwebfree pFreeUserData)
{
	bool bRet;
	xwebapp* pApp;
	if ( !pServer ) { return false; }
	__xwebLock(&pServer->iAppLock);
	pApp = pServer->pApp;
	__xwebAppRetain(pApp);
	__xwebUnlock(&pServer->iAppLock);
	bRet = xrtWebAppStreamRouteAsyncEx(pApp, iMethodMask, sPattern, pBegin, pBody, pEndAsync, pUserData, pFreeUserData);
	__xwebAppRelease(pApp);
	return bRet;
}


XXAPI bool xrtWebServerStreamRouteAsync(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyendasync pEndAsync, ptr pUserData)
{
	return xrtWebServerStreamRouteAsyncEx(pServer, iMethodMask, sPattern, pBegin, pBody, pEndAsync, pUserData, NULL);
}


// 注册 GET 路由
XXAPI bool xrtWebServerGet(xwebserver* pServer, const char* sPattern, xwebhandler pHandler, ptr pUserData)
{
	return xrtWebServerRoute(pServer, XWEB_METHOD_GET, sPattern, pHandler, pUserData);
}


// 注册 POST 路由
XXAPI bool xrtWebServerPost(xwebserver* pServer, const char* sPattern, xwebhandler pHandler, ptr pUserData)
{
	return xrtWebServerRoute(pServer, XWEB_METHOD_POST, sPattern, pHandler, pUserData);
}


// 注册任意方法路由
XXAPI bool xrtWebServerAny(xwebserver* pServer, const char* sPattern, xwebhandler pHandler, ptr pUserData)
{
	return xrtWebServerRoute(pServer, XWEB_METHOD_ANY, sPattern, pHandler, pUserData);
}


// 在 app 中注册静态目录
XXAPI bool xrtWebAppStatic(xwebapp* pApp, const char* sMount, const char* sRoot, const xwebstaticconfig* pCfg)
{
	__xweb_static_mount* pMount;
	xwebstaticconfig tCfg;
	if ( !pApp || !sMount || !sRoot || sMount[0] != '/' ) { return false; }
	xrtWebStaticConfigInit(&tCfg);
	if ( pCfg ) { tCfg = *pCfg; }
	pMount = (__xweb_static_mount*)xrtMalloc(sizeof(__xweb_static_mount));
	if ( !pMount ) { return false; }
	memset(pMount, 0, sizeof(__xweb_static_mount));
	pMount->sMount = __xwebCopyStr(sMount);
	pMount->sRoot = __xwebCopyStr(sRoot);
	pMount->tConfig = tCfg;
	if ( !pMount->sMount || !pMount->sRoot ) {
		xrtFree(pMount->sMount);
		xrtFree(pMount->sRoot);
		xrtFree(pMount);
		return false;
	}
	__xwebLock(&pApp->iLock);
	pMount->pNext = pApp->pStaticHead;
	pApp->pStaticHead = pMount;
	__xwebUnlock(&pApp->iLock);
	return true;
}


// 使用默认配置注册 app 静态目录
XXAPI bool xrtWebAppStaticDefault(xwebapp* pApp, const char* sMount, const char* sRoot)
{
	return xrtWebAppStatic(pApp, sMount, sRoot, NULL);
}


// 使用常用参数注册 app 静态目录
XXAPI bool xrtWebAppStaticEx(xwebapp* pApp, const char* sMount, const char* sRoot, const char* sIndexNames, const char* sCacheControl, size_t iChunkSize, uint32 iFlags)
{
	xwebstaticconfig tCfg;
	xrtWebStaticConfigInit(&tCfg);
	tCfg.iFlags = iFlags;
	if ( sIndexNames && sIndexNames[0] ) { tCfg.sIndexNames = sIndexNames; }
	if ( sCacheControl && sCacheControl[0] ) { tCfg.sCacheControl = sCacheControl; }
	if ( iChunkSize > 0u ) { tCfg.iChunkSize = iChunkSize; }
	return xrtWebAppStatic(pApp, sMount, sRoot, &tCfg);
}


// 注册默认 app 静态目录
XXAPI bool xrtWebServerStatic(xwebserver* pServer, const char* sMount, const char* sRoot, const xwebstaticconfig* pCfg)
{
	bool bRet;
	xwebapp* pApp;
	if ( !pServer ) { return false; }
	__xwebLock(&pServer->iAppLock);
	pApp = pServer->pApp;
	__xwebAppRetain(pApp);
	__xwebUnlock(&pServer->iAppLock);
	bRet = xrtWebAppStatic(pApp, sMount, sRoot, pCfg);
	__xwebAppRelease(pApp);
	return bRet;
}


// 使用默认配置注册默认 app 静态目录
XXAPI bool xrtWebServerStaticDefault(xwebserver* pServer, const char* sMount, const char* sRoot)
{
	return xrtWebServerStatic(pServer, sMount, sRoot, NULL);
}


// 使用常用参数注册默认 app 静态目录
XXAPI bool xrtWebServerStaticEx(xwebserver* pServer, const char* sMount, const char* sRoot, const char* sIndexNames, const char* sCacheControl, size_t iChunkSize, uint32 iFlags)
{
	bool bRet;
	xwebapp* pApp;
	if ( !pServer ) { return false; }
	__xwebLock(&pServer->iAppLock);
	pApp = pServer->pApp;
	__xwebAppRetain(pApp);
	__xwebUnlock(&pServer->iAppLock);
	bRet = xrtWebAppStaticEx(pApp, sMount, sRoot, sIndexNames, sCacheControl, iChunkSize, iFlags);
	__xwebAppRelease(pApp);
	return bRet;
}


// 获取请求方法文本
XXAPI const char* xrtWebRequestMethod(const xwebrequest* pReq)
{
	return (pReq && pReq->pRaw) ? pReq->pRaw->sMethod : "";
}


// 复制请求方法（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestMethodCopy(const xwebrequest* pReq)
{
	return xrtCopyStr((str)xrtWebRequestMethod(pReq), 0);
}


// 获取请求方法 ID
XXAPI uint32 xrtWebRequestMethodId(const xwebrequest* pReq)
{
	return (pReq && pReq->pRaw) ? pReq->pRaw->iMethod : XHTTPD_METHOD_UNKNOWN;
}


// 获取原始请求目标，包含 path 和 query
XXAPI const char* xrtWebRequestTarget(const xwebrequest* pReq)
{
	return (pReq && pReq->pRaw) ? pReq->pRaw->sTarget : "";
}


// 复制原始请求目标，返回值需要使用 xrtFree 释放
XXAPI str xrtWebRequestTargetCopy(const xwebrequest* pReq)
{
	return xrtCopyStr((str)xrtWebRequestTarget(pReq), 0);
}


// 获取请求路径
XXAPI const char* xrtWebRequestPath(const xwebrequest* pReq)
{
	return (pReq && pReq->pRaw) ? pReq->pRaw->sPath : "";
}


// 复制请求路径（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestPathCopy(const xwebrequest* pReq)
{
	return xrtCopyStr((str)xrtWebRequestPath(pReq), 0);
}


// 获取请求 query
XXAPI const char* xrtWebRequestQuery(const xwebrequest* pReq)
{
	return (pReq && pReq->pRaw) ? pReq->pRaw->sQuery : "";
}


// 复制请求 query（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestQueryCopy(const xwebrequest* pReq)
{
	return xrtCopyStr((str)xrtWebRequestQuery(pReq), 0);
}


// 获取请求头
// 复制 URL 查询片段并执行百分号解码
static str __xwebQueryViewDecodeCopy(xrtstrview tView)
{
	char* sOut;
	size_t iOutLen = 0u;
	if ( tView.sPtr == NULL ) { return xCore.sNull; }
	if ( tView.iLen == 0u ) { return xrtCopyStr((str)"", 0); }
	sOut = (char*)xrtMalloc(tView.iLen + 1u);
	if ( !sOut ) { return xCore.sNull; }
	if ( !xrtPercentDecodeTo(tView.sPtr, tView.iLen, sOut, tView.iLen + 1u, &iOutLen, true) ) {
		xrtFree(sOut);
		return xCore.sNull;
	}
	sOut[iOutLen] = '\0';
	return (str)sOut;
}


// 判断字符串视图是否等于指定 ASCII 文本（忽略大小写）
static bool __xwebStrViewEqNoCaseText(xrtstrview tView, const char* sText)
{
	size_t iLen;
	if ( !sText ) { return false; }
	iLen = strlen(sText);
	if ( tView.iLen != iLen || !tView.sPtr ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		if ( __xwebAsciiUpper(tView.sPtr[i]) != __xwebAsciiUpper(sText[i]) ) { return false; }
	}
	return true;
}


// 获取 query 键值对数量
XXAPI size_t xrtWebRequestQueryCount(const xwebrequest* pReq)
{
	return (pReq && pReq->pRaw) ? xrtQueryCount(pReq->pRaw->sQuery) : 0u;
}


// 按索引获取 query 键值对视图，返回视图只在请求生命周期内有效
XXAPI bool xrtWebRequestQueryPairAt(const xwebrequest* pReq, size_t iIndex, xrtquerypair* pOut)
{
	size_t iOffset = 0u;
	size_t iCur = 0u;
	xrtquerypair tPair;
	if ( pOut ) { memset(pOut, 0, sizeof(xrtquerypair)); }
	if ( !pReq || !pReq->pRaw || !pOut ) { return false; }
	while ( xrtQueryNext(pReq->pRaw->sQuery, &iOffset, &tPair) ) {
		if ( iCur == iIndex ) {
			*pOut = tPair;
			return true;
		}
		iCur++;
	}
	return false;
}


// 判断 query 键值对是否包含显式 value
XXAPI bool xrtWebRequestQueryHasValueAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtquerypair tPair;
	return xrtWebRequestQueryPairAt(pReq, iIndex, &tPair) && (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u;
}


// 复制并解码 query 名称（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestQueryNameCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtquerypair tPair;
	if ( !xrtWebRequestQueryPairAt(pReq, iIndex, &tPair) ) { return xCore.sNull; }
	return __xwebQueryViewDecodeCopy(tPair.tKey);
}


// 复制并解码 query 值（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestQueryValueCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtquerypair tPair;
	if ( !xrtWebRequestQueryPairAt(pReq, iIndex, &tPair) ) { return xCore.sNull; }
	if ( (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) == 0u ) { return xrtCopyStr((str)"", 0); }
	return __xwebQueryViewDecodeCopy(tPair.tValue);
}


XXAPI const char* xrtWebRequestHeader(const xwebrequest* pReq, const char* sName)
{
	return (pReq && pReq->pRaw) ? xrtHttpdRequestHeader(pReq->pRaw, sName) : NULL;
}


// 复制请求头（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestHeaderCopy(const xwebrequest* pReq, const char* sName)
{
	return xrtCopyStr((str)xrtWebRequestHeader(pReq, sName), 0);
}


// 获取请求体
// 获取请求头数量
XXAPI size_t xrtWebRequestHeaderCount(const xwebrequest* pReq)
{
	return (pReq && pReq->pRaw) ? (size_t)xrtHttpdRequestHeaderCount(pReq->pRaw) : 0u;
}


// 按索引获取请求头名称，返回指针只在请求生命周期内有效
XXAPI const char* xrtWebRequestHeaderNameAt(const xwebrequest* pReq, size_t iIndex)
{
	if ( !pReq || !pReq->pRaw || iIndex > UINT32_MAX ) { return NULL; }
	return xrtHttpdRequestHeaderNameAt(pReq->pRaw, (uint32)iIndex);
}


// 按索引获取请求头值，返回指针只在请求生命周期内有效
XXAPI const char* xrtWebRequestHeaderValueAt(const xwebrequest* pReq, size_t iIndex)
{
	if ( !pReq || !pReq->pRaw || iIndex > UINT32_MAX ) { return NULL; }
	return xrtHttpdRequestHeaderValueAt(pReq->pRaw, (uint32)iIndex);
}


// 复制请求头名称（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestHeaderNameCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	return xrtCopyStr((str)xrtWebRequestHeaderNameAt(pReq, iIndex), 0);
}


// 复制请求头值（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestHeaderValueCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	return xrtCopyStr((str)xrtWebRequestHeaderValueAt(pReq, iIndex), 0);
}


XXAPI const void* xrtWebRequestBody(const xwebrequest* pReq, size_t* pLen)
{
	if ( pLen ) { *pLen = (pReq && pReq->pRaw) ? pReq->pRaw->iBodyLen : 0u; }
	return (pReq && pReq->pRaw) ? pReq->pRaw->pBody : NULL;
}


// 将请求体作为文本复制（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestBodyTextCopy(const xwebrequest* pReq)
{
	size_t iLen = 0u;
	const void* pBody = xrtWebRequestBody(pReq, &iLen);
	return xrtCopyStr((str)pBody, iLen);
}


// 获取请求声明的 Content-Length。chunked 请求在解析完成后返回实际 body 字节数。
XXAPI int64 xrtWebRequestContentLength(const xwebrequest* pReq)
{
	return (pReq && pReq->pRaw) ? pReq->pRaw->iContentLength : -1;
}


// 复制请求本地地址文本，返回值需要使用 xrtFree 释放
// 获取请求 Content-Type
XXAPI const char* xrtWebRequestContentType(const xwebrequest* pReq)
{
	return xrtWebRequestHeader(pReq, "Content-Type");
}


// 判断请求体是否为 application/x-www-form-urlencoded
XXAPI bool xrtWebRequestIsFormUrlEncoded(const xwebrequest* pReq)
{
	const char* sContentType = xrtWebRequestContentType(pReq);
	xrtmediatypeview tType;
	if ( !sContentType || !xrtHttpMediaTypeParse(sContentType, &tType) ) { return false; }
	return __xwebStrViewEqNoCaseText(tType.tType, "application") &&
		__xwebStrViewEqNoCaseText(tType.tSubType, "x-www-form-urlencoded");
}


// 解析 multipart/form-data 边界，返回视图只在请求头生命周期内有效
static bool __xwebRequestMultipartBoundary(const xwebrequest* pReq, xrtstrview* pBoundary)
{
	const char* sContentType = xrtWebRequestContentType(pReq);
	xrtmediatypeview tType;
	if ( pBoundary ) { memset(pBoundary, 0, sizeof(xrtstrview)); }
	if ( !pBoundary || !sContentType || !xrtHttpMediaTypeParse(sContentType, &tType) ) { return false; }
	if ( !__xwebStrViewEqNoCaseText(tType.tType, "multipart") ||
		!__xwebStrViewEqNoCaseText(tType.tSubType, "form-data") ) {
		return false;
	}
	return xrtMultipartBoundaryFromContentType(sContentType, pBoundary);
}


// 判断请求体是否为 multipart/form-data
XXAPI bool xrtWebRequestIsMultipartForm(const xwebrequest* pReq)
{
	xrtstrview tBoundary;
	return __xwebRequestMultipartBoundary(pReq, &tBoundary);
}


// 获取表单字段数量
XXAPI size_t xrtWebRequestFormCount(const xwebrequest* pReq)
{
	const char* sBody;
	size_t iBodyLen = 0u;
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( !xrtWebRequestIsFormUrlEncoded(pReq) ) { return 0u; }
	sBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( !sBody ) { return 0u; }
	while ( xrtFormUrlEncodedNextN(sBody, iBodyLen, &iOffset, &tPair) ) { iCount++; }
	return iCount;
}


// 按索引获取表单字段视图，返回视图只在请求生命周期内有效
XXAPI bool xrtWebRequestFormPairAt(const xwebrequest* pReq, size_t iIndex, xrtquerypair* pOut)
{
	const char* sBody;
	size_t iBodyLen = 0u;
	size_t iOffset = 0u;
	size_t iCur = 0u;
	xrtquerypair tPair;
	if ( pOut ) { memset(pOut, 0, sizeof(xrtquerypair)); }
	if ( !pOut || !xrtWebRequestIsFormUrlEncoded(pReq) ) { return false; }
	sBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( !sBody ) { return false; }
	while ( xrtFormUrlEncodedNextN(sBody, iBodyLen, &iOffset, &tPair) ) {
		if ( iCur == iIndex ) {
			*pOut = tPair;
			return true;
		}
		iCur++;
	}
	return false;
}


// 判断表单字段是否包含显式 value
XXAPI bool xrtWebRequestFormHasValueAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtquerypair tPair;
	return xrtWebRequestFormPairAt(pReq, iIndex, &tPair) && (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u;
}


// 复制并解码表单字段名称（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestFormNameCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtquerypair tPair;
	if ( !xrtWebRequestFormPairAt(pReq, iIndex, &tPair) ) { return xCore.sNull; }
	return __xwebQueryViewDecodeCopy(tPair.tKey);
}


// 复制并解码表单字段值（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestFormValueCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtquerypair tPair;
	if ( !xrtWebRequestFormPairAt(pReq, iIndex, &tPair) ) { return xCore.sNull; }
	if ( (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) == 0u ) { return xrtCopyStr((str)"", 0); }
	return __xwebQueryViewDecodeCopy(tPair.tValue);
}


// 按名称复制并解码表单字段值（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestFormValueCopy(const xwebrequest* pReq, const char* sName)
{
	const char* sBody;
	char* sOut;
	size_t iBodyLen = 0u;
	size_t iOutLen = 0u;
	if ( !sName || !xrtWebRequestIsFormUrlEncoded(pReq) ) { return xCore.sNull; }
	sBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( !sBody ) { return xCore.sNull; }
	sOut = (char*)xrtMalloc(iBodyLen + 1u);
	if ( !sOut ) { return xCore.sNull; }
	if ( !xrtQueryFindValueToN(sBody, iBodyLen, sName, strlen(__xrt_cstr(sName)), sOut, iBodyLen + 1u, &iOutLen) ) {
		xrtFree(sOut);
		return xCore.sNull;
	}
	sOut[iOutLen] = '\0';
	return (str)sOut;
}


// 获取 multipart part 数量
XXAPI size_t xrtWebRequestMultipartCount(const xwebrequest* pReq)
{
	xrtstrview tBoundary;
	const char* sBody;
	size_t iBodyLen = 0u;
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtmultipartpartview tPart;
	if ( !__xwebRequestMultipartBoundary(pReq, &tBoundary) ) { return 0u; }
	sBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( !sBody ) { return 0u; }
	while ( xrtMultipartNextN(sBody, iBodyLen, tBoundary.sPtr, tBoundary.iLen, &iOffset, &tPart) ) { iCount++; }
	return iCount;
}


// 按索引获取 multipart part 视图，返回视图只在请求生命周期内有效
XXAPI bool xrtWebRequestMultipartPartAt(const xwebrequest* pReq, size_t iIndex, xrtmultipartpartview* pOut)
{
	xrtstrview tBoundary;
	const char* sBody;
	size_t iBodyLen = 0u;
	size_t iOffset = 0u;
	size_t iCur = 0u;
	xrtmultipartpartview tPart;
	if ( pOut ) { memset(pOut, 0, sizeof(xrtmultipartpartview)); }
	if ( !pOut || !__xwebRequestMultipartBoundary(pReq, &tBoundary) ) { return false; }
	sBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( !sBody ) { return false; }
	while ( xrtMultipartNextN(sBody, iBodyLen, tBoundary.sPtr, tBoundary.iLen, &iOffset, &tPart) ) {
		if ( iCur == iIndex ) {
			*pOut = tPart;
			return true;
		}
		iCur++;
	}
	return false;
}


// 判断 multipart part 是否为文件字段
XXAPI bool xrtWebRequestMultipartIsFileAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtmultipartpartview tPart;
	return xrtWebRequestMultipartPartAt(pReq, iIndex, &tPart) &&
		((tPart.iFlags & (XRT_MULTIPART_F_HAS_FILENAME | XRT_MULTIPART_F_HAS_FILENAME_EXT)) != 0u);
}


// 复制 multipart 字段名，返回值需要使用 xrtFree 释放
XXAPI str xrtWebRequestMultipartNameCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtmultipartpartview tPart;
	if ( !xrtWebRequestMultipartPartAt(pReq, iIndex, &tPart) ||
		(tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) == 0u ) {
		return xCore.sNull;
	}
	return xrtCopyStr((str)tPart.tName.sPtr, tPart.tName.iLen);
}


// 复制 multipart 文件名，支持 filename* 解码，返回值需要使用 xrtFree 释放
XXAPI str xrtWebRequestMultipartFileNameCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtmultipartpartview tPart;
	char* sOut;
	size_t iRawLen;
	size_t iOutLen = 0u;
	if ( !xrtWebRequestMultipartPartAt(pReq, iIndex, &tPart) ||
		(tPart.iFlags & (XRT_MULTIPART_F_HAS_FILENAME | XRT_MULTIPART_F_HAS_FILENAME_EXT)) == 0u ) {
		return xCore.sNull;
	}
	iRawLen = tPart.tFileName.iLen;
	if ( tPart.tFileNameExt.iLen > iRawLen ) { iRawLen = tPart.tFileNameExt.iLen; }
	sOut = (char*)xrtMalloc(iRawLen + 1u);
	if ( !sOut ) { return xCore.sNull; }
	if ( !xrtMultipartDecodeFileNameTo(&tPart, sOut, iRawLen + 1u, &iOutLen) ) {
		xrtFree(sOut);
		return xCore.sNull;
	}
	sOut[iOutLen] = '\0';
	return (str)sOut;
}


// 复制 multipart Content-Type，返回值需要使用 xrtFree 释放
XXAPI str xrtWebRequestMultipartContentTypeCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtmultipartpartview tPart;
	if ( !xrtWebRequestMultipartPartAt(pReq, iIndex, &tPart) ||
		(tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) == 0u ) {
		return xCore.sNull;
	}
	return xrtCopyStr((str)tPart.tContentType.sPtr, tPart.tContentType.iLen);
}


// 获取 multipart body 视图，返回视图只在请求生命周期内有效
XXAPI bool xrtWebRequestMultipartBodyViewAt(const xwebrequest* pReq, size_t iIndex, const void** ppData, size_t* pLen)
{
	xrtmultipartpartview tPart;
	if ( ppData ) { *ppData = NULL; }
	if ( pLen ) { *pLen = 0u; }
	if ( !xrtWebRequestMultipartPartAt(pReq, iIndex, &tPart) ) { return false; }
	if ( ppData ) { *ppData = tPart.tBody.sPtr; }
	if ( pLen ) { *pLen = tPart.tBody.iLen; }
	return true;
}


// 获取 multipart body 大小
XXAPI size_t xrtWebRequestMultipartBodySizeAt(const xwebrequest* pReq, size_t iIndex)
{
	size_t iLen = 0u;
	(void)xrtWebRequestMultipartBodyViewAt(pReq, iIndex, NULL, &iLen);
	return iLen;
}


// 复制 multipart body 为文本，返回值需要使用 xrtFree 释放
XXAPI str xrtWebRequestMultipartBodyTextCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	const void* pData = NULL;
	size_t iLen = 0u;
	if ( !xrtWebRequestMultipartBodyViewAt(pReq, iIndex, &pData, &iLen) ) { return xCore.sNull; }
	return xrtCopyStr((str)pData, iLen);
}


// 按字段名复制第一个非文件 multipart 字段值，返回值需要使用 xrtFree 释放
XXAPI str xrtWebRequestMultipartValueCopy(const xwebrequest* pReq, const char* sName)
{
	xrtstrview tBoundary;
	const char* sBody;
	size_t iBodyLen = 0u;
	size_t iOffset = 0u;
	size_t iNameLen;
	xrtmultipartpartview tPart;
	if ( !sName || !__xwebRequestMultipartBoundary(pReq, &tBoundary) ) { return xCore.sNull; }
	iNameLen = strlen(__xrt_cstr(sName));
	sBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( !sBody ) { return xCore.sNull; }
	while ( xrtMultipartNextN(sBody, iBodyLen, tBoundary.sPtr, tBoundary.iLen, &iOffset, &tPart) ) {
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u &&
			(tPart.iFlags & (XRT_MULTIPART_F_HAS_FILENAME | XRT_MULTIPART_F_HAS_FILENAME_EXT)) == 0u &&
			tPart.tName.iLen == iNameLen &&
			(iNameLen == 0u || memcmp(tPart.tName.sPtr, sName, iNameLen) == 0) ) {
			return xrtCopyStr((str)tPart.tBody.sPtr, tPart.tBody.iLen);
		}
	}
	return xCore.sNull;
}


XXAPI str xrtWebRequestLocalAddrCopy(const xwebrequest* pReq)
{
	return (pReq && pReq->pConn) ? xrtHttpdConnLocalAddrText(pReq->pConn) : xCore.sNull;
}


// 复制请求远端地址文本，返回值需要使用 xrtFree 释放
XXAPI str xrtWebRequestRemoteAddrCopy(const xwebrequest* pReq)
{
	return (pReq && pReq->pConn) ? xrtHttpdConnRemoteAddrText(pReq->pConn) : xCore.sNull;
}


// 复制请求 TLS SNI，非 TLS 或客户端未发送 SNI 时返回空字符串
XXAPI str xrtWebRequestTlsSNICopy(const xwebrequest* pReq)
{
	return (pReq && pReq->pConn) ? xrtHttpdConnTlsSNICopy(pReq->pConn) : xCore.sNull;
}


// 获取请求本地端口
XXAPI int xrtWebRequestLocalPort(const xwebrequest* pReq)
{
	return (pReq && pReq->pConn) ? xrtHttpdConnLocalPort(pReq->pConn) : 0;
}


// 获取请求远端端口
XXAPI int xrtWebRequestRemotePort(const xwebrequest* pReq)
{
	return (pReq && pReq->pConn) ? xrtHttpdConnRemotePort(pReq->pConn) : 0;
}


// 获取路由变量
XXAPI bool xrtWebRequestParam(const xwebrequest* pReq, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( !pReq || !sName || !sOut || iOutCap == 0u ) { return false; }
	for ( size_t i = 0u; i < pReq->iParamCount; ++i ) {
		if ( __xwebStrEq(pReq->arrParams[i].sName, sName) ) {
			size_t iLen = pReq->arrParams[i].iLen;
			if ( iLen + 1u > iOutCap ) { return false; }
			memcpy(sOut, pReq->arrParams[i].sValue, iLen);
			sOut[iLen] = '\0';
			if ( pOutLen ) { *pOutLen = iLen; }
			return true;
		}
	}
	return false;
}


// 复制路由变量（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestParamCopy(const xwebrequest* pReq, const char* sName)
{
	if ( !pReq || !sName ) { return xCore.sNull; }
	for ( size_t i = 0u; i < pReq->iParamCount; ++i ) {
		if ( __xwebStrEq(pReq->arrParams[i].sName, sName) ) {
			return xrtCopyStr((str)pReq->arrParams[i].sValue, pReq->arrParams[i].iLen);
		}
	}
	return xCore.sNull;
}


// 获取并解码 query 值
// 获取路由变量数量
XXAPI size_t xrtWebRequestParamCount(const xwebrequest* pReq)
{
	return pReq ? pReq->iParamCount : 0u;
}


// 按索引获取路由变量名称，返回指针只在请求生命周期内有效
XXAPI const char* xrtWebRequestParamNameAt(const xwebrequest* pReq, size_t iIndex)
{
	if ( !pReq || iIndex >= pReq->iParamCount ) { return NULL; }
	return pReq->arrParams[iIndex].sName;
}


// 按索引获取路由变量值视图，值不保证以 0 结尾
XXAPI bool xrtWebRequestParamValueViewAt(const xwebrequest* pReq, size_t iIndex, const char** ppValue, size_t* pLen)
{
	if ( ppValue ) { *ppValue = NULL; }
	if ( pLen ) { *pLen = 0u; }
	if ( !pReq || iIndex >= pReq->iParamCount ) { return false; }
	if ( ppValue ) { *ppValue = pReq->arrParams[iIndex].sValue; }
	if ( pLen ) { *pLen = pReq->arrParams[iIndex].iLen; }
	return true;
}


// 复制路由变量名称（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestParamNameCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	return xrtCopyStr((str)xrtWebRequestParamNameAt(pReq, iIndex), 0);
}


// 复制路由变量值（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestParamValueCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	if ( !pReq || iIndex >= pReq->iParamCount ) { return xCore.sNull; }
	return xrtCopyStr((str)pReq->arrParams[iIndex].sValue, pReq->arrParams[iIndex].iLen);
}


XXAPI bool xrtWebRequestQueryValue(const xwebrequest* pReq, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( !pReq || !pReq->pRaw || !sName ) { return false; }
	return xrtQueryFindValueTo(pReq->pRaw->sQuery, sName, sOut, iOutCap, pOutLen);
}


// 复制并解码 query 值（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestQueryValueCopy(const xwebrequest* pReq, const char* sName)
{
	char* sOut;
	size_t iCap;
	size_t iLen = 0u;
	if ( !pReq || !pReq->pRaw || !sName ) { return xCore.sNull; }
	iCap = strlen(__xrt_cstr(pReq->pRaw->sQuery)) + 1u;
	if ( iCap == 0u ) { return xCore.sNull; }
	sOut = xrtMalloc(iCap);
	if ( !sOut ) { return xCore.sNull; }
	if ( !xrtQueryFindValueTo(pReq->pRaw->sQuery, sName, sOut, iCap, &iLen) ) {
		xrtFree(sOut);
		return xCore.sNull;
	}
	if ( iLen == 0u ) {
		xrtFree(sOut);
		return xCore.sNull;
	}
	return (str)sOut;
}


// 获取 cookie 值
XXAPI bool xrtWebRequestCookie(const xwebrequest* pReq, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	const char* sCookie;
	xrtcookiepair tCookie;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( !pReq || !sName || !sOut || iOutCap == 0u ) { return false; }
	sCookie = xrtWebRequestHeader(pReq, "Cookie");
	if ( !sCookie || !xrtCookieFind(sCookie, sName, &tCookie) ) { return false; }
	if ( tCookie.tValue.iLen + 1u > iOutCap ) { return false; }
	memcpy(sOut, tCookie.tValue.sPtr, tCookie.tValue.iLen);
	sOut[tCookie.tValue.iLen] = '\0';
	if ( pOutLen ) { *pOutLen = tCookie.tValue.iLen; }
	return true;
}


// 复制 cookie 值（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestCookieCopy(const xwebrequest* pReq, const char* sName)
{
	const char* sCookie;
	xrtcookiepair tCookie;
	if ( !pReq || !sName ) { return xCore.sNull; }
	sCookie = xrtWebRequestHeader(pReq, "Cookie");
	if ( !sCookie || !xrtCookieFind(sCookie, sName, &tCookie) ) { return xCore.sNull; }
	return xrtCopyStr((str)tCookie.tValue.sPtr, tCookie.tValue.iLen);
}


// 设置响应状态
// 获取 Cookie 数量
XXAPI size_t xrtWebRequestCookieCount(const xwebrequest* pReq)
{
	const char* sCookie = xrtWebRequestHeader(pReq, "Cookie");
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtcookiepair tCookie;
	if ( !sCookie ) { return 0u; }
	while ( xrtCookieNext(sCookie, &iOffset, &tCookie) ) { iCount++; }
	return iCount;
}


// 按索引获取 Cookie 视图，返回视图只在请求生命周期内有效
XXAPI bool xrtWebRequestCookiePairAt(const xwebrequest* pReq, size_t iIndex, xrtcookiepair* pOut)
{
	const char* sCookie = xrtWebRequestHeader(pReq, "Cookie");
	size_t iOffset = 0u;
	size_t iCur = 0u;
	xrtcookiepair tCookie;
	if ( pOut ) { memset(pOut, 0, sizeof(xrtcookiepair)); }
	if ( !sCookie || !pOut ) { return false; }
	while ( xrtCookieNext(sCookie, &iOffset, &tCookie) ) {
		if ( iCur == iIndex ) {
			*pOut = tCookie;
			return true;
		}
		iCur++;
	}
	return false;
}


// 复制 Cookie 名称（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestCookieNameCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtcookiepair tCookie;
	if ( !xrtWebRequestCookiePairAt(pReq, iIndex, &tCookie) ) { return xCore.sNull; }
	return xrtCopyStr((str)tCookie.tName.sPtr, tCookie.tName.iLen);
}


// 复制 Cookie 值（返回值需要使用 xrtFree 释放）
XXAPI str xrtWebRequestCookieValueCopyAt(const xwebrequest* pReq, size_t iIndex)
{
	xrtcookiepair tCookie;
	if ( !xrtWebRequestCookiePairAt(pReq, iIndex, &tCookie) ) { return xCore.sNull; }
	return xrtCopyStr((str)tCookie.tValue.sPtr, tCookie.tValue.iLen);
}


XXAPI void xrtWebResponseStatus(xwebresponse* pResp, uint32 iStatusCode, const char* sReason)
{
	if ( !pResp || !pResp->pRaw ) { return; }
	xrtHttpdResponseSetStatus(pResp->pRaw, iStatusCode, sReason);
}


// 设置响应头
XXAPI bool xrtWebResponseSetHeader(xwebresponse* pResp, const char* sName, const char* sValue)
{
	return (pResp && pResp->pRaw) ? xrtHttpdResponseSetHeader(pResp->pRaw, sName, sValue) : false;
}


// 追加响应头
XXAPI bool xrtWebResponseAddHeader(xwebresponse* pResp, const char* sName, const char* sValue)
{
	return (pResp && pResp->pRaw) ? xrtHttpdResponseAddHeader(pResp->pRaw, sName, sValue) : false;
}


// 设置 Cookie。maxAge < 0 表示会话 Cookie；sameSite 为 XRT_SAME_SITE_*，0 表示不输出。
XXAPI bool xrtWebResponseCookie(xwebresponse* pResp, const char* sName, const char* sValue, const char* sPath, int32 iMaxAge, uint32 iSameSite, uint32 iFlags)
{
	xrtsetcookieview tCookie;
	char* sHeader;
	size_t iCap;
	size_t iNameLen;
	size_t iValueLen;
	size_t iPathLen;
	bool bOk;
	if ( !pResp || !sName || !sName[0] || !sValue ) { return false; }
	iNameLen = strlen(__xrt_cstr(sName));
	iValueLen = strlen(__xrt_cstr(sValue));
	iPathLen = (sPath && sPath[0]) ? strlen(__xrt_cstr(sPath)) : 0u;
	iCap = iNameLen + iValueLen + iPathLen + 160u;
	sHeader = (char*)xrtMalloc(iCap);
	if ( !sHeader ) { return false; }
	memset(&tCookie, 0, sizeof(tCookie));
	tCookie.tName = xrtStrView(sName, iNameLen);
	tCookie.tValue = xrtStrView(sValue, iValueLen);
	tCookie.iFlags = XRT_SET_COOKIE_F_HAS_VALUE | (iFlags & (XRT_SET_COOKIE_F_SECURE | XRT_SET_COOKIE_F_HTTP_ONLY | XRT_SET_COOKIE_F_PARTITIONED | XRT_SET_COOKIE_F_SAME_PARTY));
	if ( iPathLen > 0u ) {
		tCookie.tPath = xrtStrView(sPath, iPathLen);
		tCookie.iFlags |= XRT_SET_COOKIE_F_HAS_PATH;
	}
	if ( iMaxAge >= 0 ) {
		tCookie.iMaxAge = iMaxAge;
		tCookie.iFlags |= XRT_SET_COOKIE_F_HAS_MAX_AGE;
	}
	if ( iSameSite == XRT_SAME_SITE_LAX || iSameSite == XRT_SAME_SITE_STRICT || iSameSite == XRT_SAME_SITE_NONE ) {
		tCookie.iSameSite = (uint8)iSameSite;
		tCookie.iFlags |= XRT_SET_COOKIE_F_HAS_SAME_SITE;
	}
	bOk = xrtSetCookieBuildTo(&tCookie, sHeader, iCap, NULL) && xrtWebResponseAddHeader(pResp, "Set-Cookie", sHeader);
	xrtFree(sHeader);
	return bOk;
}


// 删除 Cookie。通过 Max-Age=0 覆盖同名同路径 Cookie。
XXAPI bool xrtWebResponseDeleteCookie(xwebresponse* pResp, const char* sName, const char* sPath)
{
	return xrtWebResponseCookie(pResp, sName, "", (sPath && sPath[0]) ? sPath : "/", 0, XRT_SAME_SITE_UNSPECIFIED, XRT_SET_COOKIE_F_NONE);
}


// 输出文本响应
XXAPI bool xrtWebResponseText(xwebresponse* pResp, const char* sText, const char* sContentType)
{
	if ( !pResp || !pResp->pRaw ) { return false; }
	return xrtHttpdResponseSetBodyCopy(pResp->pRaw, sText ? sText : "", sText ? strlen(sText) : 0u, sContentType ? sContentType : "text/plain; charset=utf-8");
}


// 输出 JSON 文本响应
XXAPI bool xrtWebResponseJsonText(xwebresponse* pResp, const char* sText)
{
	return xrtWebResponseText(pResp, sText ? sText : "null", "application/json; charset=utf-8");
}


// 输出二进制响应体
XXAPI bool xrtWebResponseBody(xwebresponse* pResp, const void* pData, size_t iLen, const char* sContentType)
{
	if ( !pResp || !pResp->pRaw || (!pData && iLen > 0u) ) { return false; }
	return xrtHttpdResponseSetBodyCopy(pResp->pRaw, pData, iLen, sContentType);
}


// 预留普通响应 body 缓冲容量。
XXAPI bool xrtWebResponseReserveBody(xwebresponse* pResp, size_t iCap)
{
	return (pResp && pResp->pRaw) ? xrtHttpdResponseReserveBody(pResp->pRaw, iCap) : false;
}


// 追加普通响应 body。用于提交响应前分段构建 body，流式响应请使用 Start/Send/End。
XXAPI bool xrtWebResponseAppendBody(xwebresponse* pResp, const void* pData, size_t iLen)
{
	if ( !pResp || !pResp->pRaw || (!pData && iLen > 0u) ) { return false; }
	return xrtHttpdResponseAppendBodyCopy(pResp->pRaw, pData, iLen);
}


// 追加普通文本响应 body。
XXAPI bool xrtWebResponseAppendText(xwebresponse* pResp, const char* sText)
{
	return xrtWebResponseAppendBody(pResp, sText ? sText : "", sText ? strlen(sText) : 0u);
}


// 开始流式响应
XXAPI bool xrtWebResponseStart(xwebresponse* pResp)
{
	if ( !pResp || !pResp->pReq || !pResp->pReq->pConn || !pResp->pRaw || pResp->bCommitted ) { return false; }
	pResp->bCommitted = xrtHttpdConnStart(pResp->pReq->pConn, pResp->pRaw) == XRT_NET_OK;
	return pResp->bCommitted;
}


// 写入流式响应数据
XXAPI bool xrtWebResponseSend(xwebresponse* pResp, const void* pData, size_t iLen)
{
	if ( !pResp || !pResp->pReq || !pResp->pReq->pConn || (!pData && iLen > 0u) ) { return false; }
	if ( !pResp->bCommitted ) { return false; }
	return xrtHttpdConnSend(pResp->pReq->pConn, pData, iLen) == XRT_NET_OK;
}


// 写入流式响应文本
XXAPI bool xrtWebResponseWriteText(xwebresponse* pResp, const char* sText)
{
	return xrtWebResponseSend(pResp, sText ? sText : "", sText ? strlen(sText) : 0u);
}


// 结束流式响应
XXAPI bool xrtWebResponseEnd(xwebresponse* pResp)
{
	if ( !pResp || !pResp->pReq || !pResp->pReq->pConn || !pResp->bCommitted ) { return false; }
	return xrtHttpdConnEnd(pResp->pReq->pConn) == XRT_NET_OK;
}


// 输出文件响应
XXAPI bool xrtWebResponseFile(xwebresponse* pResp, const char* sFilePath, size_t iChunkSize)
{
	if ( !pResp || !pResp->pReq || !pResp->pReq->pConn || !pResp->pRaw || !sFilePath ) { return false; }
	pResp->bCommitted = xrtHttpdConnSendFile(pResp->pReq->pConn, pResp->pRaw, sFilePath, iChunkSize) == XRT_NET_OK;
	return pResp->bCommitted;
}


// 重定向响应
XXAPI bool xrtWebResponseRedirect(xwebresponse* pResp, const char* sURL, uint32 iStatusCode)
{
	if ( !pResp || !sURL ) { return false; }
	if ( iStatusCode == 0u ) { iStatusCode = 302u; }
	xrtWebResponseStatus(pResp, iStatusCode, NULL);
	if ( !xrtWebResponseSetHeader(pResp, "Location", sURL) ) { return false; }
	return xrtWebResponseText(pResp, "", "text/plain; charset=utf-8");
}


// 根据路径推断 MIME
XXAPI const char* xrtWebMimeByPath(const char* sPath)
{
	const char* sExt;
	if ( !sPath ) { return "application/octet-stream"; }
	sExt = strrchr(sPath, '.');
	if ( !sExt ) { return "application/octet-stream"; }
	sExt++;
	if ( strcmp(sExt, "html") == 0 || strcmp(sExt, "htm") == 0 ) { return "text/html; charset=utf-8"; }
	if ( strcmp(sExt, "css") == 0 ) { return "text/css; charset=utf-8"; }
	if ( strcmp(sExt, "js") == 0 || strcmp(sExt, "mjs") == 0 ) { return "application/javascript; charset=utf-8"; }
	if ( strcmp(sExt, "json") == 0 ) { return "application/json; charset=utf-8"; }
	if ( strcmp(sExt, "txt") == 0 || strcmp(sExt, "log") == 0 ) { return "text/plain; charset=utf-8"; }
	if ( strcmp(sExt, "png") == 0 ) { return "image/png"; }
	if ( strcmp(sExt, "jpg") == 0 || strcmp(sExt, "jpeg") == 0 ) { return "image/jpeg"; }
	if ( strcmp(sExt, "gif") == 0 ) { return "image/gif"; }
	if ( strcmp(sExt, "webp") == 0 ) { return "image/webp"; }
	if ( strcmp(sExt, "svg") == 0 ) { return "image/svg+xml"; }
	if ( strcmp(sExt, "ico") == 0 ) { return "image/x-icon"; }
	if ( strcmp(sExt, "wasm") == 0 ) { return "application/wasm"; }
	if ( strcmp(sExt, "pdf") == 0 ) { return "application/pdf"; }
	if ( strcmp(sExt, "zip") == 0 ) { return "application/zip"; }
	return "application/octet-stream";
}

#endif
