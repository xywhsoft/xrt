



/*
	NetHTTPD - HTTP 服务器封装 [Ver1.0]
	
	基于 nettcp.h (TCP 服务器) + nettls.h (HTTPS TLS 支持)
	
	特性:
	  - HTTP/1.0 和 HTTP/1.1 支持
	  - Keep-Alive 连接复用
	  - 事件驱动架构
	  - 双模式 API: 简易版 (内部线程) + 高级版 (共享事件循环)
	  - 静态文件服务
	  - 简单 URI 模式匹配
	  - WebSocket 升级支持
	
	仅 IPv4，跨平台 (Windows / Linux)
*/



/* ============================== 内部常量定义 ============================== */

#define __XRT_HTTPD_DEFAULT_MAX_HEADER   8192       // 默认最大请求头 8KB
#define __XRT_HTTPD_DEFAULT_MAX_BODY     1048576    // 默认最大请求正文 1MB
#define __XRT_HTTPD_DEFAULT_KEEPALIVE    60         // 默认 Keep-Alive 超时 60 秒
#define __XRT_HTTPD_DEFAULT_MAX_CLIENTS  256        // 默认最大连接数
#define __XRT_HTTPD_RECV_BUF_SIZE        8192       // 接收缓冲区大小



/* ============================== HTTP 状态码文本 ============================== */

static const char* __xrt_httpd_status_text(int iStatusCode)
{
	switch ( iStatusCode ) {
		case 100: return "Continue";
		case 101: return "Switching Protocols";
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 206: return "Partial Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 304: return "Not Modified";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 411: return "Length Required";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 415: return "Unsupported Media Type";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 505: return "HTTP Version Not Supported";
		default: return "Unknown";
	}
}



/* ============================== MIME 类型映射 ============================== */

static const char* __xrt_httpd_get_mime_type(const char* sPath)
{
	// 获取扩展名
	const char* pDot = strrchr(sPath, '.');
	if ( !pDot || pDot == sPath ) return "application/octet-stream";
	pDot++;
	
	// 常见 MIME 类型
	if ( strcasecmp(pDot, "html") == 0 || strcasecmp(pDot, "htm") == 0 ) return "text/html; charset=utf-8";
	if ( strcasecmp(pDot, "css") == 0 ) return "text/css; charset=utf-8";
	if ( strcasecmp(pDot, "js") == 0 ) return "application/javascript; charset=utf-8";
	if ( strcasecmp(pDot, "json") == 0 ) return "application/json; charset=utf-8";
	if ( strcasecmp(pDot, "xml") == 0 ) return "application/xml; charset=utf-8";
	if ( strcasecmp(pDot, "txt") == 0 ) return "text/plain; charset=utf-8";
	if ( strcasecmp(pDot, "csv") == 0 ) return "text/csv; charset=utf-8";
	
	// 图片
	if ( strcasecmp(pDot, "png") == 0 ) return "image/png";
	if ( strcasecmp(pDot, "jpg") == 0 || strcasecmp(pDot, "jpeg") == 0 ) return "image/jpeg";
	if ( strcasecmp(pDot, "gif") == 0 ) return "image/gif";
	if ( strcasecmp(pDot, "ico") == 0 ) return "image/x-icon";
	if ( strcasecmp(pDot, "svg") == 0 ) return "image/svg+xml";
	if ( strcasecmp(pDot, "webp") == 0 ) return "image/webp";
	if ( strcasecmp(pDot, "bmp") == 0 ) return "image/bmp";
	
	// 字体
	if ( strcasecmp(pDot, "woff") == 0 ) return "font/woff";
	if ( strcasecmp(pDot, "woff2") == 0 ) return "font/woff2";
	if ( strcasecmp(pDot, "ttf") == 0 ) return "font/ttf";
	if ( strcasecmp(pDot, "otf") == 0 ) return "font/otf";
	if ( strcasecmp(pDot, "eot") == 0 ) return "application/vnd.ms-fontobject";
	
	// 音视频
	if ( strcasecmp(pDot, "mp3") == 0 ) return "audio/mpeg";
	if ( strcasecmp(pDot, "wav") == 0 ) return "audio/wav";
	if ( strcasecmp(pDot, "ogg") == 0 ) return "audio/ogg";
	if ( strcasecmp(pDot, "mp4") == 0 ) return "video/mp4";
	if ( strcasecmp(pDot, "webm") == 0 ) return "video/webm";
	if ( strcasecmp(pDot, "avi") == 0 ) return "video/x-msvideo";
	
	// 压缩文件
	if ( strcasecmp(pDot, "zip") == 0 ) return "application/zip";
	if ( strcasecmp(pDot, "gz") == 0 || strcasecmp(pDot, "gzip") == 0 ) return "application/gzip";
	if ( strcasecmp(pDot, "tar") == 0 ) return "application/x-tar";
	if ( strcasecmp(pDot, "rar") == 0 ) return "application/vnd.rar";
	if ( strcasecmp(pDot, "7z") == 0 ) return "application/x-7z-compressed";
	
	// 文档
	if ( strcasecmp(pDot, "pdf") == 0 ) return "application/pdf";
	if ( strcasecmp(pDot, "doc") == 0 ) return "application/msword";
	if ( strcasecmp(pDot, "docx") == 0 ) return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
	if ( strcasecmp(pDot, "xls") == 0 ) return "application/vnd.ms-excel";
	if ( strcasecmp(pDot, "xlsx") == 0 ) return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
	if ( strcasecmp(pDot, "ppt") == 0 ) return "application/vnd.ms-powerpoint";
	if ( strcasecmp(pDot, "pptx") == 0 ) return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
	
	// 默认二进制
	return "application/octet-stream";
}



/* ============================== HTTP 客户端槽位结构 ============================== */

typedef struct {
	xnetconn* pConn;              // 底层连接
	int iClientId;                // 客户端 ID
	bool bInUse;                  // 是否使用中
	xnetbuf tRecvBuf;             // 接收缓冲区
	xhttpdreq tReq;               // 当前请求
	bool bHeaderParsed;           // 请求头是否解析完成
	bool bBodyComplete;           // 请求正文是否接收完成
	bool bKeepAlive;              // 是否 Keep-Alive 连接
	xtime tLastActive;            // 最后活跃时间
	// WebSocket 升级状态
	bool bUpgraded;               // 是否已升级为 WebSocket
	xwsevents* pWsEvents;         // WebSocket 事件回调 (升级后使用)
	xnetbuf tWsMsgBuf;            // WebSocket 消息重组缓冲区
	uint8 iWsMsgOpcode;           // WebSocket 消息操作码
	bool bWsFragmented;           // 是否正在接收 WebSocket 分片消息
} __xrt_httpd_client_slot;



/* ============================== HTTP 服务器结构 ============================== */

struct xrt_http_server {
	xtcpserver* pTcpServer;       // 底层 TCP 服务器
	
	xhttpsrvconfig tConfig;
	xhttpsrvevents tEvents;
	
	// 客户端槽位管理
	__xrt_httpd_client_slot* arrSlots;
	int iMaxClients;
	int iClientCount;
	
	volatile bool bRunning;
	
	ptr pUserData;
};



/* ============================== HTTP 请求解析 ============================== */

// 解析 HTTP 方法
static xhttp_method __xrt_httpd_parse_method(const char* sMethod)
{
	if ( strcmp(sMethod, "GET") == 0 ) return XHTTP_GET;
	if ( strcmp(sMethod, "POST") == 0 ) return XHTTP_POST;
	if ( strcmp(sMethod, "PUT") == 0 ) return XHTTP_PUT;
	if ( strcmp(sMethod, "DELETE") == 0 ) return XHTTP_DELETE;
	if ( strcmp(sMethod, "PATCH") == 0 ) return XHTTP_PATCH;
	if ( strcmp(sMethod, "HEAD") == 0 ) return XHTTP_HEAD;
	return XHTTP_GET;  // 默认 GET
}

// 解析请求行: GET /path HTTP/1.1
// 返回: 0=需要更多数据, >0=请求行长度 (含 \r\n), -1=错误
static int __xrt_httpd_parse_request_line(const char* pData, size_t iLen, xhttpdreq* pReq)
{
	// 查找 \r\n
	const char* pEnd = (const char*)memmem(pData, iLen, "\r\n", 2);
	if ( !pEnd ) return 0;
	
	size_t iLineLen = pEnd - pData;
	if ( iLineLen < 14 ) return -1;  // 最短: "GET / HTTP/1.0"
	
	// 解析方法
	const char* p = pData;
	const char* pSpace = memchr(p, ' ', pEnd - p);
	if ( !pSpace || pSpace - p >= 16 ) return -1;
	
	size_t iMethodLen = pSpace - p;
	memcpy(pReq->sMethod, p, iMethodLen);
	pReq->sMethod[iMethodLen] = '\0';
	pReq->iMethod = __xrt_httpd_parse_method(pReq->sMethod);
	
	// 解析 URI
	p = pSpace + 1;
	pSpace = memchr(p, ' ', pEnd - p);
	if ( !pSpace ) return -1;
	
	size_t iUriLen = pSpace - p;
	if ( iUriLen >= sizeof(pReq->sUri) ) iUriLen = sizeof(pReq->sUri) - 1;
	memcpy(pReq->sUri, p, iUriLen);
	pReq->sUri[iUriLen] = '\0';
	
	// 分离 Path 和 Query
	char* pQuery = strchr(pReq->sUri, '?');
	if ( pQuery ) {
		size_t iPathLen = pQuery - pReq->sUri;
		memcpy(pReq->sPath, pReq->sUri, iPathLen);
		pReq->sPath[iPathLen] = '\0';
		strncpy(pReq->sQuery, pQuery + 1, sizeof(pReq->sQuery) - 1);
		pReq->sQuery[sizeof(pReq->sQuery) - 1] = '\0';
	} else {
		strncpy(pReq->sPath, pReq->sUri, sizeof(pReq->sPath) - 1);
		pReq->sPath[sizeof(pReq->sPath) - 1] = '\0';
		pReq->sQuery[0] = '\0';
	}
	
	// 解析 HTTP 版本
	p = pSpace + 1;
	size_t iVerLen = pEnd - p;
	if ( iVerLen >= sizeof(pReq->sVersion) ) iVerLen = sizeof(pReq->sVersion) - 1;
	memcpy(pReq->sVersion, p, iVerLen);
	pReq->sVersion[iVerLen] = '\0';
	
	// HTTP/1.1 默认 Keep-Alive
	pReq->bKeepAlive = (strcmp(pReq->sVersion, "HTTP/1.1") == 0);
	
	return (int)(iLineLen + 2);
}

// 解析请求头
// 返回: 0=需要更多数据, >0=头部总长度 (含空行 \r\n\r\n), -1=错误
static int __xrt_httpd_parse_headers(const char* pData, size_t iLen, xhttpdreq* pReq)
{
	// 查找 \r\n\r\n
	const char* pEnd = (const char*)memmem(pData, iLen, "\r\n\r\n", 4);
	if ( !pEnd ) return 0;
	
	// 初始化头部字典
	if ( !pReq->pHeaders ) {
		pReq->pHeaders = xrtDictCreate(sizeof(char*));
	}
	
	const char* p = pData;
	while ( p < pEnd ) {
		// 查找行尾
		const char* pLineEnd = (const char*)memmem(p, pEnd - p, "\r\n", 2);
		if ( !pLineEnd ) break;
		
		// 空行表示头部结束
		if ( pLineEnd == p ) break;
		
		// 解析 "Key: Value"
		const char* pColon = memchr(p, ':', pLineEnd - p);
		if ( pColon ) {
			// 转小写 Key
			char sKey[256];
			size_t iKeyLen = pColon - p;
			if ( iKeyLen >= sizeof(sKey) ) iKeyLen = sizeof(sKey) - 1;
			for ( size_t i = 0; i < iKeyLen; i++ ) {
				sKey[i] = (char)tolower((unsigned char)p[i]);
			}
			sKey[iKeyLen] = '\0';
			
			// 跳过冒号后的空格
			const char* pValue = pColon + 1;
			while ( pValue < pLineEnd && *pValue == ' ' ) pValue++;
			
			// 复制值
			size_t iValueLen = pLineEnd - pValue;
			char* sValue = (char*)xrtMalloc(iValueLen + 1);
			if ( sValue ) {
				memcpy(sValue, pValue, iValueLen);
				sValue[iValueLen] = '\0';
				
				// 存入字典
				char** ppOld = NULL;
				xrtDictSetPtr((xdict)pReq->pHeaders, sKey, (uint32)iKeyLen, sValue, (ptr*)&ppOld);
				if ( ppOld && *ppOld ) xrtFree(*ppOld);
				
				// 解析特殊头部
				if ( strcmp(sKey, "content-length") == 0 ) {
					pReq->iContentLength = (size_t)atoll(sValue);
				} else if ( strcmp(sKey, "connection") == 0 ) {
					if ( strcasecmp(sValue, "close") == 0 ) {
						pReq->bKeepAlive = false;
					} else if ( strcasecmp(sValue, "keep-alive") == 0 ) {
						pReq->bKeepAlive = true;
					}
				}
			}
		}
		
		p = pLineEnd + 2;
	}
	
	return (int)(pEnd - pData + 4);
}

// 解析查询参数 (延迟解析)
static void __xrt_httpd_parse_params(xhttpdreq* pReq)
{
	if ( pReq->sQuery[0] == '\0' ) return;
	if ( pReq->pParams && xrtDictCount((xdict)pReq->pParams) > 0 ) return;  // 已解析
	
	if ( !pReq->pParams ) {
		pReq->pParams = xrtDictCreate(sizeof(char*));
	}
	
	char* p = pReq->sQuery;
	while ( *p ) {
		// 找 '=' 和 '&'
		char* pEq = strchr(p, '=');
		char* pAmp = strchr(p, '&');
		
		if ( !pEq || (pAmp && pAmp < pEq) ) {
			// 没有值的参数或格式错误，跳过
			if ( pAmp ) {
				p = pAmp + 1;
				continue;
			}
			break;
		}
		
		// 提取 key
		size_t iKeyLen = pEq - p;
		char sKey[256];
		if ( iKeyLen >= sizeof(sKey) ) iKeyLen = sizeof(sKey) - 1;
		memcpy(sKey, p, iKeyLen);
		sKey[iKeyLen] = '\0';
		
		// 提取 value
		char* pValueEnd = pAmp ? pAmp : p + strlen(p);
		size_t iValueLen = pValueEnd - (pEq + 1);
		char* sValue = (char*)xrtMalloc(iValueLen + 1);
		if ( sValue ) {
			memcpy(sValue, pEq + 1, iValueLen);
			sValue[iValueLen] = '\0';
			// TODO: URL 解码
			xrtDictSetPtr((xdict)pReq->pParams, sKey, (uint32)iKeyLen, sValue, NULL);
		}
		
		if ( !pAmp ) break;
		p = pAmp + 1;
	}
}

// 解析 Cookie (延迟解析)
static void __xrt_httpd_parse_cookies(xhttpdreq* pReq)
{
	if ( pReq->pCookies && xrtDictCount((xdict)pReq->pCookies) > 0 ) return;  // 已解析
	
	const char* sCookie = xrtHttpReqGetHeader(pReq, "cookie");
	if ( !sCookie ) return;
	
	if ( !pReq->pCookies ) {
		pReq->pCookies = xrtDictCreate(sizeof(char*));
	}
	
	const char* p = sCookie;
	while ( *p ) {
		// 跳过空格
		while ( *p == ' ' ) p++;
		
		// 找 '=' 和 ';'
		const char* pEq = strchr(p, '=');
		const char* pSemi = strchr(p, ';');
		
		if ( !pEq || (pSemi && pSemi < pEq) ) {
			if ( pSemi ) {
				p = pSemi + 1;
				continue;
			}
			break;
		}
		
		// 提取 key
		size_t iKeyLen = pEq - p;
		char sKey[256];
		if ( iKeyLen >= sizeof(sKey) ) iKeyLen = sizeof(sKey) - 1;
		memcpy(sKey, p, iKeyLen);
		sKey[iKeyLen] = '\0';
		
		// 提取 value
		const char* pValueEnd = pSemi ? pSemi : p + strlen(p);
		size_t iValueLen = pValueEnd - (pEq + 1);
		char* sValue = (char*)xrtMalloc(iValueLen + 1);
		if ( sValue ) {
			memcpy(sValue, pEq + 1, iValueLen);
			sValue[iValueLen] = '\0';
			xrtDictSetPtr((xdict)pReq->pCookies, sKey, (uint32)iKeyLen, sValue, NULL);
		}
		
		if ( !pSemi ) break;
		p = pSemi + 1;
	}
}

// 清理请求结构
static void __xrt_httpd_req_cleanup(xhttpdreq* pReq)
{
	// 释放字典
	if ( pReq->pHeaders ) {
		xrtDictDestroy((xdict)pReq->pHeaders);
		pReq->pHeaders = NULL;
	}
	if ( pReq->pParams ) {
		xrtDictDestroy((xdict)pReq->pParams);
		pReq->pParams = NULL;
	}
	if ( pReq->pCookies ) {
		xrtDictDestroy((xdict)pReq->pCookies);
		pReq->pCookies = NULL;
	}
	
	if ( pReq->pBody ) {
		xrtFree(pReq->pBody);
		pReq->pBody = NULL;
	}
	pReq->iBodyLen = 0;
}



/* ============================== HTTP 响应发送 ============================== */

// 发送数据 (自动处理 TLS)
static void __xrt_httpd_send(xnetconn* pConn, const char* pData, size_t iLen)
{
	if ( pConn->bTlsEnabled && pConn->pTlsCtx ) {
		size_t iWritten = 0;
		xrtTlsWrite(pConn->pTlsCtx, pData, iLen, &iWritten);
	} else {
		size_t iSent = 0;
		xrtSockSend(pConn, pData, iLen, &iSent);
	}
}

// 快速响应 (一次性发送)
XXAPI void xrtHttpReply(xnetconn* pConn, int iStatusCode, const char* sHeaders, const char* sBody)
{
	size_t iBodyLen = sBody ? strlen(sBody) : 0;
	
	// 构建响应头
	char sRespHeader[4096];
	int iHeaderLen = snprintf(sRespHeader, sizeof(sRespHeader),
		"HTTP/1.1 %d %s\r\n"
		"Content-Length: %zu\r\n"
		"Server: xrt/1.0\r\n"
		"%s"
		"\r\n",
		iStatusCode, __xrt_httpd_status_text(iStatusCode),
		iBodyLen,
		sHeaders ? sHeaders : "");
	
	// 发送头部
	__xrt_httpd_send(pConn, sRespHeader, iHeaderLen);
	
	// 发送正文
	if ( sBody && iBodyLen > 0 ) {
		__xrt_httpd_send(pConn, sBody, iBodyLen);
	}
}

// 格式化响应
XXAPI void xrtHttpReplyFmt(xnetconn* pConn, int iStatusCode, const char* sHeaders, const char* sFmt, ...)
{
	char sBody[16384];
	va_list args;
	va_start(args, sFmt);
	vsnprintf(sBody, sizeof(sBody), sFmt, args);
	va_end(args);
	
	xrtHttpReply(pConn, iStatusCode, sHeaders, sBody);
}

// 发送 JSON 响应
XXAPI void xrtHttpReplyJSON(xnetconn* pConn, int iStatusCode, const char* sJSON)
{
	xrtHttpReply(pConn, iStatusCode, "Content-Type: application/json; charset=utf-8\r\n", sJSON);
}

// 发送文件响应
XXAPI void xrtHttpReplyFile(xnetconn* pConn, const char* sFilePath, const char* sMimeType)
{
	// 读取文件
	size_t iFileSize = 0;
	char* pFileData = xrtFileReadAll((str)sFilePath, XRT_CP_BINARY, &iFileSize);
	
	if ( !pFileData ) {
		xrtHttpReply(pConn, 404, "Content-Type: text/plain\r\n", "File Not Found");
		return;
	}
	
	// 确定 MIME 类型
	const char* sMime = sMimeType ? sMimeType : __xrt_httpd_get_mime_type(sFilePath);
	
	// 构建响应头
	char sRespHeader[4096];
	int iHeaderLen = snprintf(sRespHeader, sizeof(sRespHeader),
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %zu\r\n"
		"Server: xrt/1.0\r\n"
		"\r\n",
		sMime, iFileSize);
	
	// 发送头部
	__xrt_httpd_send(pConn, sRespHeader, iHeaderLen);
	
	// 发送正文
	if ( iFileSize > 0 ) {
		__xrt_httpd_send(pConn, pFileData, iFileSize);
	}
	
	xrtFree(pFileData);
}

// 分块响应开始
XXAPI void xrtHttpReplyStart(xnetconn* pConn, int iStatusCode, const char* sHeaders)
{
	char sRespHeader[4096];
	int iHeaderLen = snprintf(sRespHeader, sizeof(sRespHeader),
		"HTTP/1.1 %d %s\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Server: xrt/1.0\r\n"
		"%s"
		"\r\n",
		iStatusCode, __xrt_httpd_status_text(iStatusCode),
		sHeaders ? sHeaders : "");
	
	__xrt_httpd_send(pConn, sRespHeader, iHeaderLen);
}

// 发送分块数据
XXAPI void xrtHttpReplyChunk(xnetconn* pConn, const char* pData, size_t iLen)
{
	if ( iLen == 0 ) return;
	
	// 发送块大小
	char sChunkHeader[32];
	int iChunkHeaderLen = snprintf(sChunkHeader, sizeof(sChunkHeader), "%zx\r\n", iLen);
	__xrt_httpd_send(pConn, sChunkHeader, iChunkHeaderLen);
	
	// 发送块数据
	__xrt_httpd_send(pConn, pData, iLen);
	
	// 发送块结束
	__xrt_httpd_send(pConn, "\r\n", 2);
}

// 结束分块响应
XXAPI void xrtHttpReplyEnd(xnetconn* pConn)
{
	__xrt_httpd_send(pConn, "0\r\n\r\n", 5);
}

// 重定向
XXAPI void xrtHttpRedirect(xnetconn* pConn, int iStatusCode, const char* sLocation)
{
	char sHeaders[2048];
	snprintf(sHeaders, sizeof(sHeaders), "Location: %s\r\n", sLocation);
	xrtHttpReply(pConn, iStatusCode, sHeaders, NULL);
}



/* ============================== URI 模式匹配 ============================== */

// 简单模式匹配
// 模式语法:
//   *   - 匹配任意字符 (不含 /)
//   **  - 匹配任意字符 (含 /)
//   :name - 路径参数占位符 (匹配到下一个 / 或结尾)
XXAPI bool xrtHttpReqMatch(xhttpdreq* pReq, const char* sPattern)
{
	const char* pPath = pReq->sPath;
	const char* pPat = sPattern;
	
	while ( *pPath && *pPat ) {
		if ( *pPat == '*' ) {
			if ( *(pPat + 1) == '*' ) {
				// ** 匹配任意字符 (含 /)
				pPat += 2;
				if ( *pPat == '\0' ) return true;
				// 尝试匹配剩余部分
				while ( *pPath ) {
					if ( xrtHttpReqMatch(&(xhttpdreq){ .sPath = {0} }, pPat) ) {
						// 需要复制 path 进行递归检查
						xhttpdreq tTempReq = {0};
						strncpy(tTempReq.sPath, pPath, sizeof(tTempReq.sPath) - 1);
						if ( xrtHttpReqMatch(&tTempReq, pPat) ) return true;
					}
					pPath++;
				}
				return false;
			} else {
				// * 匹配任意字符 (不含 /)
				pPat++;
				while ( *pPath && *pPath != '/' ) {
					pPath++;
				}
			}
		} else if ( *pPat == ':' ) {
			// :name 匹配路径段
			pPat++;
			while ( *pPat && *pPat != '/' ) pPat++;
			while ( *pPath && *pPath != '/' ) pPath++;
		} else {
			// 精确匹配
			if ( *pPath != *pPat ) return false;
			pPath++;
			pPat++;
		}
	}
	
	// 跳过结尾的通配符
	while ( *pPat == '*' ) pPat++;
	
	return (*pPath == '\0' && *pPat == '\0');
}



/* ============================== HTTP 请求读取 API ============================== */

// 获取请求头
XXAPI const char* xrtHttpReqGetHeader(xhttpdreq* pReq, const char* sName)
{
	if ( !pReq->pHeaders ) return NULL;
	
	// 转小写
	char sKey[256];
	size_t iLen = strlen(sName);
	if ( iLen >= sizeof(sKey) ) iLen = sizeof(sKey) - 1;
	for ( size_t i = 0; i < iLen; i++ ) {
		sKey[i] = (char)tolower((unsigned char)sName[i]);
	}
	sKey[iLen] = '\0';
	
	return (const char*)xrtDictGetPtr((xdict)pReq->pHeaders, sKey, (uint32)iLen);
}

// 获取查询参数
XXAPI const char* xrtHttpReqGetParam(xhttpdreq* pReq, const char* sName)
{
	__xrt_httpd_parse_params(pReq);
	if ( !pReq->pParams ) return NULL;
	return (const char*)xrtDictGetPtr((xdict)pReq->pParams, (ptr)sName, (uint32)strlen(sName));
}

// 获取 Cookie
XXAPI const char* xrtHttpReqGetCookie(xhttpdreq* pReq, const char* sName)
{
	__xrt_httpd_parse_cookies(pReq);
	if ( !pReq->pCookies ) return NULL;
	return (const char*)xrtDictGetPtr((xdict)pReq->pCookies, (ptr)sName, (uint32)strlen(sName));
}



/* ============================== 静态文件服务 ============================== */

// 路径安全检查 (防止路径遍历攻击)
static bool __xrt_httpd_is_path_safe(const char* sPath)
{
	// 检查 .. 路径遍历
	if ( strstr(sPath, "..") ) return false;
	
	// 检查绝对路径
	if ( sPath[0] == '/' || sPath[0] == '\\' ) return true;
	#if defined(_WIN32) || defined(_WIN64)
	if ( strlen(sPath) >= 2 && sPath[1] == ':' ) return false;
	#endif
	
	return true;
}

// 服务静态文件目录
XXAPI void xrtHttpServeDir(xnetconn* pConn, xhttpdreq* pReq, const char* sRootDir)
{
	// 安全检查
	if ( !__xrt_httpd_is_path_safe(pReq->sPath) ) {
		xrtHttpReply(pConn, 403, "Content-Type: text/plain\r\n", "Forbidden");
		return;
	}
	
	// 构建文件路径
	char sFilePath[2048];
	snprintf(sFilePath, sizeof(sFilePath), "%s%s", sRootDir, pReq->sPath);
	
	// 如果是目录，尝试 index.html
	size_t iPathLen = strlen(sFilePath);
	if ( iPathLen > 0 && (sFilePath[iPathLen - 1] == '/' || sFilePath[iPathLen - 1] == '\\') ) {
		snprintf(sFilePath + iPathLen, sizeof(sFilePath) - iPathLen, "index.html");
	} else if ( xrtDirExists(sFilePath) ) {
		snprintf(sFilePath, sizeof(sFilePath), "%s%s/index.html", sRootDir, pReq->sPath);
	}
	
	// 检查文件是否存在
	if ( !xrtFileExists(sFilePath) ) {
		xrtHttpReply(pConn, 404, "Content-Type: text/plain\r\n", "Not Found");
		return;
	}
	
	// 发送文件
	xrtHttpReplyFile(pConn, sFilePath, NULL);
}

// 服务单个文件
XXAPI void xrtHttpServeFile(xnetconn* pConn, const char* sFilePath)
{
	xrtHttpReplyFile(pConn, sFilePath, NULL);
}



/* ============================== WebSocket 升级支持 ============================== */

// WebSocket 升级响应
XXAPI void xrtHttpUpgradeWebSocket(xnetconn* pConn, xhttpdreq* pReq, const xwsevents* pEvents)
{
	// 获取 Sec-WebSocket-Key
	const char* sKey = xrtHttpReqGetHeader(pReq, "sec-websocket-key");
	if ( !sKey ) {
		xrtHttpReply(pConn, 400, "Content-Type: text/plain\r\n", "Missing Sec-WebSocket-Key");
		return;
	}
	
	// 计算 Accept
	char sAccept[64];
	__xrt_ws_compute_accept(sKey, sAccept);
	
	// 发送升级响应
	char sResponse[512];
	int iLen = snprintf(sResponse, sizeof(sResponse),
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n"
		"\r\n",
		sAccept);
	
	__xrt_httpd_send(pConn, sResponse, iLen);
	
	// 标记连接已升级 (后续数据按 WebSocket 处理)
	// 槽位状态由调用者更新
}



/* ============================== HTTP 服务器 - 槽位管理 ============================== */

// 分配槽位
static int __xrt_httpd_alloc_slot(xhttpserver* pServer)
{
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		if ( !pServer->arrSlots[i].bInUse ) {
			pServer->arrSlots[i].bInUse = true;
			pServer->arrSlots[i].iClientId = i;
			pServer->iClientCount++;
			return i;
		}
	}
	return -1;
}

// 释放槽位
static void __xrt_httpd_free_slot(xhttpserver* pServer, int iSlot)
{
	if ( iSlot < 0 || iSlot >= pServer->iMaxClients ) return;
	__xrt_httpd_client_slot* pSlot = &pServer->arrSlots[iSlot];
	
	// 清理
	xrtNetBufFree(&pSlot->tRecvBuf);
	xrtNetBufFree(&pSlot->tWsMsgBuf);
	__xrt_httpd_req_cleanup(&pSlot->tReq);
	
	memset(pSlot, 0, sizeof(__xrt_httpd_client_slot));
	pServer->iClientCount--;
}



/* ============================== HTTP 服务器 - WebSocket 帧处理 ============================== */

// 处理 WebSocket 数据 (从 HTTP 服务器槽位调用)
static void __xrt_httpd_process_ws_data(xhttpserver* pServer, __xrt_httpd_client_slot* pSlot)
{
	while ( pSlot->tRecvBuf.iSize > 0 ) {
		__xrt_ws_frame tFrame;
		int iHeaderLen = __xrt_ws_parse_frame_header(
			(uint8*)pSlot->tRecvBuf.pData, pSlot->tRecvBuf.iSize, &tFrame);
		
		if ( iHeaderLen == 0 ) break;  // 需要更多数据
		if ( iHeaderLen < 0 ) {
			// 错误，关闭连接
			if ( pSlot->pWsEvents && pSlot->pWsEvents->OnError ) {
				pSlot->pWsEvents->OnError(pServer->pUserData, pSlot->pConn, -1);
			}
			return;
		}
		
		// 检查是否有完整的帧
		size_t iTotalLen = tFrame.iHeaderLen + tFrame.iPayloadLen;
		if ( pSlot->tRecvBuf.iSize < iTotalLen ) break;
		
		// 提取载荷
		uint8* pPayload = (uint8*)pSlot->tRecvBuf.pData + tFrame.iHeaderLen;
		
		// 解掩码 (客户端发送的数据必须掩码)
		if ( tFrame.bMasked ) {
			__xrt_ws_mask_data(pPayload, tFrame.iPayloadLen, tFrame.aMask);
		}
		
		// 处理帧
		switch ( tFrame.iOpcode ) {
			case XRT_WS_OP_CONTINUATION:
				// 分片续帧
				if ( pSlot->bWsFragmented ) {
					xrtNetBufAppend(&pSlot->tWsMsgBuf, (char*)pPayload, tFrame.iPayloadLen);
					if ( tFrame.bFin ) {
						// 分片消息完成
						if ( pSlot->pWsEvents && pSlot->pWsEvents->OnMessage ) {
							pSlot->pWsEvents->OnMessage(pServer->pUserData, pSlot->pConn,
								pSlot->iWsMsgOpcode, pSlot->tWsMsgBuf.pData, pSlot->tWsMsgBuf.iSize);
						}
						xrtNetBufClear(&pSlot->tWsMsgBuf);
						pSlot->bWsFragmented = false;
					}
				}
				break;
				
			case XRT_WS_OP_TEXT:
			case XRT_WS_OP_BINARY:
				if ( tFrame.bFin ) {
					// 完整消息
					if ( pSlot->pWsEvents && pSlot->pWsEvents->OnMessage ) {
						pSlot->pWsEvents->OnMessage(pServer->pUserData, pSlot->pConn,
							tFrame.iOpcode, (char*)pPayload, tFrame.iPayloadLen);
					}
				} else {
					// 分片消息开始
					pSlot->bWsFragmented = true;
					pSlot->iWsMsgOpcode = tFrame.iOpcode;
					xrtNetBufClear(&pSlot->tWsMsgBuf);
					xrtNetBufAppend(&pSlot->tWsMsgBuf, (char*)pPayload, tFrame.iPayloadLen);
				}
				break;
				
			case XRT_WS_OP_CLOSE:
				{
					uint16 iCode = 1000;
					const char* sReason = "";
					if ( tFrame.iPayloadLen >= 2 ) {
						iCode = ((uint16)pPayload[0] << 8) | pPayload[1];
						if ( tFrame.iPayloadLen > 2 ) {
							sReason = (char*)(pPayload + 2);
						}
					}
					if ( pSlot->pWsEvents && pSlot->pWsEvents->OnClose ) {
						pSlot->pWsEvents->OnClose(pServer->pUserData, pSlot->pConn, iCode, sReason);
					}
				}
				break;
				
			case XRT_WS_OP_PING:
				// 自动回复 Pong
				{
					uint8 aPong[256];
					size_t iPongLen = __xrt_ws_encode_frame(aPong, XRT_WS_OP_PONG, true, false,
						pPayload, tFrame.iPayloadLen < 125 ? tFrame.iPayloadLen : 125);
					__xrt_httpd_send(pSlot->pConn, (char*)aPong, iPongLen);
					
					if ( pSlot->pWsEvents && pSlot->pWsEvents->OnPing ) {
						pSlot->pWsEvents->OnPing(pServer->pUserData, pSlot->pConn,
							(char*)pPayload, tFrame.iPayloadLen);
					}
				}
				break;
				
			case XRT_WS_OP_PONG:
				if ( pSlot->pWsEvents && pSlot->pWsEvents->OnPong ) {
					pSlot->pWsEvents->OnPong(pServer->pUserData, pSlot->pConn,
						(char*)pPayload, tFrame.iPayloadLen);
				}
				break;
		}
		
		// 消费数据
		xrtNetBufConsume(&pSlot->tRecvBuf, iTotalLen);
	}
}



/* ============================== HTTP 服务器 - 事件处理 ============================== */

// 前向声明
static void __xrt_httpd_on_tcp_accept(ptr pOwner, xnetconn* pConn);
static void __xrt_httpd_on_tcp_recv(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen);
static void __xrt_httpd_on_tcp_close(ptr pOwner, xnetconn* pConn);

// TCP Accept 回调
static void __xrt_httpd_on_tcp_accept(ptr pOwner, xnetconn* pConn)
{
	xhttpserver* pServer = (xhttpserver*)pOwner;
	
	// 分配槽位
	int iSlot = __xrt_httpd_alloc_slot(pServer);
	if ( iSlot < 0 ) {
		// 槽位已满，拒绝连接
		return;
	}
	
	__xrt_httpd_client_slot* pSlot = &pServer->arrSlots[iSlot];
	pSlot->pConn = pConn;
	pSlot->iClientId = iSlot;
	pSlot->tLastActive = xrtNow();
	
	// 初始化接收缓冲区
	xrtNetBufInit(&pSlot->tRecvBuf, __XRT_HTTPD_RECV_BUF_SIZE);
	
	// 关联槽位 ID 到连接
	pConn->iId = iSlot;
}

// TCP Recv 回调
static void __xrt_httpd_on_tcp_recv(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	xhttpserver* pServer = (xhttpserver*)pOwner;
	int iSlot = pConn->iId;
	
	if ( iSlot < 0 || iSlot >= pServer->iMaxClients ) return;
	__xrt_httpd_client_slot* pSlot = &pServer->arrSlots[iSlot];
	if ( !pSlot->bInUse ) return;
	
	// 更新活跃时间
	pSlot->tLastActive = xrtNow();
	
	// 追加到接收缓冲区
	xrtNetBufAppend(&pSlot->tRecvBuf, pData, iLen);
	
	// WebSocket 模式
	if ( pSlot->bUpgraded ) {
		__xrt_httpd_process_ws_data(pServer, pSlot);
		return;
	}
	
	// HTTP 模式 - 解析请求
	if ( !pSlot->bHeaderParsed ) {
		// 解析请求行
		int iLineLen = __xrt_httpd_parse_request_line(
			pSlot->tRecvBuf.pData, pSlot->tRecvBuf.iSize, &pSlot->tReq);
		
		if ( iLineLen < 0 ) {
			// 解析错误
			xrtHttpReply(pConn, 400, "Content-Type: text/plain\r\n", "Bad Request");
			if ( pServer->tEvents.OnError ) {
				pServer->tEvents.OnError(pServer->pUserData, pConn, 400);
			}
			return;
		}
		if ( iLineLen == 0 ) return;  // 需要更多数据
		
		// 消费请求行
		xrtNetBufConsume(&pSlot->tRecvBuf, iLineLen);
		
		// 解析请求头
		int iHeaderLen = __xrt_httpd_parse_headers(
			pSlot->tRecvBuf.pData, pSlot->tRecvBuf.iSize, &pSlot->tReq);
		
		if ( iHeaderLen < 0 ) {
			xrtHttpReply(pConn, 400, "Content-Type: text/plain\r\n", "Bad Request");
			if ( pServer->tEvents.OnError ) {
				pServer->tEvents.OnError(pServer->pUserData, pConn, 400);
			}
			return;
		}
		if ( iHeaderLen == 0 ) return;  // 需要更多数据
		
		// 消费头部
		xrtNetBufConsume(&pSlot->tRecvBuf, iHeaderLen);
		pSlot->bHeaderParsed = true;
		pSlot->bKeepAlive = pSlot->tReq.bKeepAlive;
	}
	
	// 接收请求正文
	if ( pSlot->tReq.iContentLength > 0 ) {
		if ( pSlot->tRecvBuf.iSize < pSlot->tReq.iContentLength ) {
			return;  // 需要更多数据
		}
		
		// 分配正文缓冲区
		pSlot->tReq.pBody = (char*)xrtMalloc(pSlot->tReq.iContentLength + 1);
		if ( pSlot->tReq.pBody ) {
			memcpy(pSlot->tReq.pBody, pSlot->tRecvBuf.pData, pSlot->tReq.iContentLength);
			pSlot->tReq.pBody[pSlot->tReq.iContentLength] = '\0';
			pSlot->tReq.iBodyLen = pSlot->tReq.iContentLength;
		}
		xrtNetBufConsume(&pSlot->tRecvBuf, pSlot->tReq.iContentLength);
	}
	
	pSlot->bBodyComplete = true;
	
	// 检查 WebSocket 升级请求
	const char* sUpgrade = xrtHttpReqGetHeader(&pSlot->tReq, "upgrade");
	if ( sUpgrade && strcasecmp(sUpgrade, "websocket") == 0 ) {
		if ( pServer->tEvents.OnUpgrade ) {
			bool bAccept = pServer->tEvents.OnUpgrade(pServer->pUserData, pConn, &pSlot->tReq);
			if ( bAccept ) {
				// 标记为已升级
				pSlot->bUpgraded = true;
				xrtNetBufInit(&pSlot->tWsMsgBuf, 4096);
			}
		}
	}
	
	// 调用请求回调 (非 WebSocket)
	if ( !pSlot->bUpgraded && pServer->tEvents.OnRequest ) {
		pServer->tEvents.OnRequest(pServer->pUserData, pConn, &pSlot->tReq);
	}
	
	// 清理请求状态 (准备下一个请求)
	if ( !pSlot->bUpgraded ) {
		__xrt_httpd_req_cleanup(&pSlot->tReq);
		memset(&pSlot->tReq, 0, sizeof(xhttpdreq));
		pSlot->bHeaderParsed = false;
		pSlot->bBodyComplete = false;
	}
}

// TCP Close 回调
static void __xrt_httpd_on_tcp_close(ptr pOwner, xnetconn* pConn)
{
	xhttpserver* pServer = (xhttpserver*)pOwner;
	int iSlot = pConn->iId;
	
	if ( iSlot < 0 || iSlot >= pServer->iMaxClients ) return;
	__xrt_httpd_client_slot* pSlot = &pServer->arrSlots[iSlot];
	if ( !pSlot->bInUse ) return;
	
	// 调用关闭回调
	if ( pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer->pUserData, pConn);
	}
	
	// 释放槽位
	__xrt_httpd_free_slot(pServer, iSlot);
}



/* ============================== HTTP 服务器 - 生命周期 ============================== */

// 创建 (简易版)
XXAPI xhttpserver* xrtHttpServerCreate(const char* sIP, uint16 iPort,
	const xhttpsrvconfig* pConfig, const xhttpsrvevents* pEvents)
{
	xhttpserver* pServer = (xhttpserver*)xrtMalloc(sizeof(xhttpserver));
	if ( !pServer ) return NULL;
	memset(pServer, 0, sizeof(xhttpserver));
	
	// 复制配置
	if ( pConfig ) {
		memcpy(&pServer->tConfig, pConfig, sizeof(xhttpsrvconfig));
	}
	
	// 设置默认值
	if ( pServer->tConfig.iMaxHeaderSize <= 0 )
		pServer->tConfig.iMaxHeaderSize = __XRT_HTTPD_DEFAULT_MAX_HEADER;
	if ( pServer->tConfig.iMaxBodySize <= 0 )
		pServer->tConfig.iMaxBodySize = __XRT_HTTPD_DEFAULT_MAX_BODY;
	if ( pServer->tConfig.iKeepAliveTimeout <= 0 )
		pServer->tConfig.iKeepAliveTimeout = __XRT_HTTPD_DEFAULT_KEEPALIVE;
	if ( pServer->tConfig.iMaxClients <= 0 )
		pServer->tConfig.iMaxClients = __XRT_HTTPD_DEFAULT_MAX_CLIENTS;
	
	// 复制事件回调
	if ( pEvents ) {
		memcpy(&pServer->tEvents, pEvents, sizeof(xhttpsrvevents));
	}
	
	// 分配客户端槽位
	pServer->iMaxClients = pServer->tConfig.iMaxClients;
	pServer->arrSlots = (__xrt_httpd_client_slot*)xrtMalloc(
		pServer->iMaxClients * sizeof(__xrt_httpd_client_slot));
	if ( !pServer->arrSlots ) {
		xrtFree(pServer);
		return NULL;
	}
	memset(pServer->arrSlots, 0, pServer->iMaxClients * sizeof(__xrt_httpd_client_slot));
	
	// 创建底层 TCP 服务器
	xnetconfig tNetConfig = {0};
	tNetConfig.iRecvBufSize = __XRT_HTTPD_RECV_BUF_SIZE;
	tNetConfig.iMaxClients = pServer->iMaxClients;
	
	xnetevents tNetEvents = {0};
	tNetEvents.OnAccept = __xrt_httpd_on_tcp_accept;
	tNetEvents.OnRecv = __xrt_httpd_on_tcp_recv;
	tNetEvents.OnClose = __xrt_httpd_on_tcp_close;
	
	pServer->pTcpServer = xrtTcpServerCreate(sIP, iPort, &tNetConfig, &tNetEvents);
	if ( !pServer->pTcpServer ) {
		xrtFree(pServer->arrSlots);
		xrtFree(pServer);
		return NULL;
	}
	
	// 设置 TCP 服务器的 Owner 为 HTTP 服务器
	xrtTcpServerSetUserData(pServer->pTcpServer, pServer);
	
	return pServer;
}

// 创建 (高级版: 共享事件循环)
XXAPI xhttpserver* xrtHttpServerCreateEx(xeventloop* pLoop, const char* sIP, uint16 iPort,
	const xhttpsrvconfig* pConfig, const xhttpsrvevents* pEvents)
{
	xhttpserver* pServer = (xhttpserver*)xrtMalloc(sizeof(xhttpserver));
	if ( !pServer ) return NULL;
	memset(pServer, 0, sizeof(xhttpserver));
	
	// 复制配置
	if ( pConfig ) {
		memcpy(&pServer->tConfig, pConfig, sizeof(xhttpsrvconfig));
	}
	
	// 设置默认值
	if ( pServer->tConfig.iMaxHeaderSize <= 0 )
		pServer->tConfig.iMaxHeaderSize = __XRT_HTTPD_DEFAULT_MAX_HEADER;
	if ( pServer->tConfig.iMaxBodySize <= 0 )
		pServer->tConfig.iMaxBodySize = __XRT_HTTPD_DEFAULT_MAX_BODY;
	if ( pServer->tConfig.iKeepAliveTimeout <= 0 )
		pServer->tConfig.iKeepAliveTimeout = __XRT_HTTPD_DEFAULT_KEEPALIVE;
	if ( pServer->tConfig.iMaxClients <= 0 )
		pServer->tConfig.iMaxClients = __XRT_HTTPD_DEFAULT_MAX_CLIENTS;
	
	// 复制事件回调
	if ( pEvents ) {
		memcpy(&pServer->tEvents, pEvents, sizeof(xhttpsrvevents));
	}
	
	// 分配客户端槽位
	pServer->iMaxClients = pServer->tConfig.iMaxClients;
	pServer->arrSlots = (__xrt_httpd_client_slot*)xrtMalloc(
		pServer->iMaxClients * sizeof(__xrt_httpd_client_slot));
	if ( !pServer->arrSlots ) {
		xrtFree(pServer);
		return NULL;
	}
	memset(pServer->arrSlots, 0, pServer->iMaxClients * sizeof(__xrt_httpd_client_slot));
	
	// 创建底层 TCP 服务器 (共享事件循环版)
	xnetconfig tNetConfig = {0};
	tNetConfig.iRecvBufSize = __XRT_HTTPD_RECV_BUF_SIZE;
	tNetConfig.iMaxClients = pServer->iMaxClients;
	
	xnetevents tNetEvents = {0};
	tNetEvents.OnAccept = __xrt_httpd_on_tcp_accept;
	tNetEvents.OnRecv = __xrt_httpd_on_tcp_recv;
	tNetEvents.OnClose = __xrt_httpd_on_tcp_close;
	
	pServer->pTcpServer = xrtTcpServerCreateEx(pLoop, sIP, iPort, &tNetConfig, &tNetEvents);
	if ( !pServer->pTcpServer ) {
		xrtFree(pServer->arrSlots);
		xrtFree(pServer);
		return NULL;
	}
	
	// 设置 TCP 服务器的 Owner 为 HTTP 服务器
	xrtTcpServerSetUserData(pServer->pTcpServer, pServer);
	
	return pServer;
}

// 销毁
XXAPI void xrtHttpServerDestroy(xhttpserver* pServer)
{
	if ( !pServer ) return;
	
	// 停止服务器
	xrtHttpServerStop(pServer);
	
	// 销毁 TCP 服务器
	if ( pServer->pTcpServer ) {
		xrtTcpServerDestroy(pServer->pTcpServer);
	}
	
	// 释放槽位
	if ( pServer->arrSlots ) {
		for ( int i = 0; i < pServer->iMaxClients; i++ ) {
			if ( pServer->arrSlots[i].bInUse ) {
				__xrt_httpd_free_slot(pServer, i);
			}
		}
		xrtFree(pServer->arrSlots);
	}
	
	xrtFree(pServer);
}

// 启动
XXAPI xnet_result xrtHttpServerStart(xhttpserver* pServer)
{
	if ( !pServer || !pServer->pTcpServer ) return XRT_NET_ERROR;
	
	pServer->bRunning = true;
	return xrtTcpServerStart(pServer->pTcpServer);
}

// 停止
XXAPI void xrtHttpServerStop(xhttpserver* pServer)
{
	if ( !pServer ) return;
	
	pServer->bRunning = false;
	if ( pServer->pTcpServer ) {
		xrtTcpServerStop(pServer->pTcpServer);
	}
}

// 启用 TLS (HTTPS)
XXAPI xnet_result xrtHttpServerEnableTLS(xhttpserver* pServer, const xtlsconfig* pTlsConfig)
{
	if ( !pServer || !pServer->pTcpServer || !pTlsConfig ) return XRT_NET_ERROR;
	return xrtTcpServerEnableTLS(pServer->pTcpServer, pTlsConfig);
}

// 用户数据
XXAPI void xrtHttpServerSetUserData(xhttpserver* pServer, ptr pData)
{
	if ( pServer ) pServer->pUserData = pData;
}

XXAPI ptr xrtHttpServerGetUserData(xhttpserver* pServer)
{
	return pServer ? pServer->pUserData : NULL;
}
