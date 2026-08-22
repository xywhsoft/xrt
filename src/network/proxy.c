#include "../internal/xrt_proxy.h"



#if defined(XRT_FEATURE_NET_PROXY)

/* 判断借用字节视图是否满足空值一致性。 */
static bool __xrtNetProxyBytesValid(xbytesview Value)
{
	return (Value.Data != NULL) || (Value.Size == 0);
}



/* 判断借用字符串视图是否满足空值一致性。 */
static bool __xrtNetProxyStringValid(xstrview Value)
{
	return (Value.Data != NULL) || (Value.Size == 0);
}



/* 安全累加代理对象的单块分配大小。 */
static bool __xrtNetProxyAddSize(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 设置代理配置或对象生命周期错误。 */
static bool __xrtNetProxyError(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtNetSetError(Kind, Code, sOperation, sMessage, 0);
	return false;
}



/* 验证主机视图不为空、不含零字节并保持在统一网络主机上限内。 */
bool __xrtNetProxyHostValid(
	xstrview Host,
	cstr sOperation,
	cstr sName
)
{
	if ( !__xrtNetProxyStringValid(Host) || (Host.Size == 0) ) {
		return __xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CONFIG,
			sOperation, sName
		);
	}
	if ( Host.Size > XRT_NET_PROXY_HOST_LIMIT ) {
		return __xrtNetProxyError(
			XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
			sOperation, "proxy host exceeds the configured network limit"
		);
	}
	if ( memchr(Host.Data, 0, Host.Size) != NULL ) {
		return __xrtNetProxyError(
			XERR_VALUE, XNET_ERROR_PROXY_CONFIG,
			sOperation, "proxy host contains a null byte"
		);
	}
	return true;
}



/* 验证代理配置，并计算规范认证策略和精确单块分配大小。 */
static bool __xrtNetProxyConfigValid(
	const xnetproxyconfig* pConfig,
	xnetproxyauth* pAuth,
	size_t* pSize
)
{
	xnetproxyauth Auth;
	size_t iSize = sizeof(xnetproxy);
	bool bCredentials;

	if ( (pConfig == NULL) || (pAuth == NULL) || (pSize == NULL) ) {
		return __xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CONFIG,
			"create-proxy", "proxy config or output is null"
		);
	}
	if ( (pConfig->Type != XNET_PROXY_SOCKS5) &&
		(pConfig->Type != XNET_PROXY_HTTP_CONNECT) ) {
		return __xrtNetProxyError(
			XERR_VALUE, XNET_ERROR_PROXY_CONFIG,
			"create-proxy", "proxy type is unsupported"
		);
	}
	if ( !__xrtNetProxyHostValid(
		pConfig->Host, "create-proxy", "proxy host is empty"
	) ) {
		return false;
	}
	if ( pConfig->Port == 0 ) {
		(void)__xrtNetProxyError(
			XERR_RANGE, XNET_ERROR_PROXY_CONFIG,
			"create-proxy", "proxy port is zero"
		);
		return false;
	}
	if ( !__xrtNetProxyBytesValid(pConfig->Username) ||
		!__xrtNetProxyBytesValid(pConfig->Password) ) {
		return __xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CONFIG,
			"create-proxy", "proxy credentials contain an invalid view"
		);
	}
	if ( (pConfig->Type == XNET_PROXY_SOCKS5) &&
		((pConfig->Username.Size > UINT8_MAX) ||
		 (pConfig->Password.Size > UINT8_MAX)) ) {
		return __xrtNetProxyError(
			XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
			"create-proxy", "SOCKS5 credentials exceed the protocol limit"
		);
	}
	if ( (pConfig->Type == XNET_PROXY_HTTP_CONNECT) &&
		(pConfig->Username.Size != 0) &&
		(memchr(
			pConfig->Username.Data,
			':',
			pConfig->Username.Size
		) != NULL) ) {
		return __xrtNetProxyError(
			XERR_VALUE, XNET_ERROR_PROXY_CONFIG,
			"create-proxy",
			"HTTP Basic proxy username contains a colon"
		);
	}
	if ( (pConfig->Auth < XNET_PROXY_AUTH_AUTO) ||
		(pConfig->Auth > XNET_PROXY_AUTH_OPTIONAL) ) {
		return __xrtNetProxyError(
			XERR_VALUE, XNET_ERROR_PROXY_CONFIG,
			"create-proxy", "proxy authentication policy is invalid"
		);
	}

	bCredentials = (pConfig->Username.Size != 0) ||
		(pConfig->Password.Size != 0);
	Auth = pConfig->Auth;
	if ( Auth == XNET_PROXY_AUTH_AUTO ) {
		Auth = bCredentials ?
			XNET_PROXY_AUTH_REQUIRED : XNET_PROXY_AUTH_NONE;
	}
	if ( ((Auth == XNET_PROXY_AUTH_REQUIRED) ||
		(Auth == XNET_PROXY_AUTH_OPTIONAL)) && !bCredentials ) {
		return __xrtNetProxyError(
			XERR_VALUE, XNET_ERROR_PROXY_CONFIG,
			"create-proxy", "proxy authentication requires credentials"
		);
	}
	if ( (Auth == XNET_PROXY_AUTH_NONE) && bCredentials ) {
		return __xrtNetProxyError(
			XERR_VALUE, XNET_ERROR_PROXY_CONFIG,
			"create-proxy", "anonymous proxy policy cannot contain credentials"
		);
	}
	if ( !__xrtNetProxyAddSize(&iSize, pConfig->Host.Size + 1u) ||
		!__xrtNetProxyAddSize(&iSize, pConfig->Username.Size) ||
		!__xrtNetProxyAddSize(&iSize, pConfig->Password.Size) ) {
		return __xrtNetProxyError(
			XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
			"create-proxy", "proxy allocation size overflows"
		);
	}

	*pAuth = Auth;
	*pSize = iSize;
	return true;
}



/* 把一个借用字节视图复制到代理对象尾部。 */
static xbytesview __xrtNetProxyCopyBytes(
	bytes* pStorage,
	xbytesview Value
)
{
	xbytesview Copy = { *pStorage, Value.Size };

	if ( Value.Size != 0 ) {
		memcpy(*pStorage, Value.Data, Value.Size);
		*pStorage += Value.Size;
	}
	return Copy;
}



/* 初始化没有固定数组和隐含容量的代理配置。 */
XRT_API void xrtNetProxyConfigInit(xnetproxyconfig* pConfig)
{
	if ( pConfig == NULL ) {
		(void)__xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CONFIG,
			"init-proxy-config", "proxy config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Type = XNET_PROXY_SOCKS5;
	pConfig->Auth = XNET_PROXY_AUTH_AUTO;
}



/* 创建持有主机、用户名和密码深拷贝的不可变代理对象。 */
XRT_API xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pConfig)
{
	xnetproxy* pProxy;
	xnetproxyauth Auth;
	bytes pStorage;
	size_t iSize;

	if ( !__xrtNetProxyConfigValid(pConfig, &Auth, &iSize) ) {
		return NULL;
	}
	pProxy = (xnetproxy*)xrtMalloc(iSize);
	if ( pProxy == NULL ) {
		return NULL;
	}
	memset(pProxy, 0, sizeof(*pProxy));
	pProxy->References = 1;
	pProxy->AllocationSize = iSize;
	pProxy->Info.Type = pConfig->Type;
	pProxy->Info.Port = pConfig->Port;
	pProxy->Info.Auth = Auth;

	pStorage = (bytes)(pProxy + 1);
	pProxy->Info.Host.Data = (cstr)pStorage;
	pProxy->Info.Host.Size = pConfig->Host.Size;
	memcpy(pStorage, pConfig->Host.Data, pConfig->Host.Size);
	pStorage[pConfig->Host.Size] = 0;
	pStorage += pConfig->Host.Size + 1u;
	pProxy->Info.Username = __xrtNetProxyCopyBytes(
		&pStorage, pConfig->Username
	);
	pProxy->Info.Password = __xrtNetProxyCopyBytes(
		&pStorage, pConfig->Password
	);
	return pProxy;
}



/* 增加不可变代理对象的共享引用。 */
XRT_API xnetproxy* xrtNetProxyRetain(const xnetproxy* pProxy)
{
	if ( pProxy == NULL ) {
		(void)__xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CONFIG,
			"retain-proxy", "proxy is null"
		);
		return NULL;
	}
	if ( xrtRefRetain((volatile int32*)&pProxy->References) < 0 ) {
		(void)__xrtNetProxyError(
			XERR_STATE, XNET_ERROR_PROXY_CREATE,
			"retain-proxy", "proxy reference is invalid"
		);
		return NULL;
	}
	return (xnetproxy*)pProxy;
}



/* 最后一个引用释放前清零对象、凭据和全部元数据。 */
XRT_API void xrtNetProxyRelease(xnetproxy* pProxy)
{
	size_t iSize;

	if ( (pProxy == NULL) ||
		(xrtRefRelease(&pProxy->References) != 0) ) {
		return;
	}
	iSize = pProxy->AllocationSize;
	xrtSecureZero(pProxy, iSize);
	xrtFree(pProxy);
}



/* 复制由代理对象持有的只读信息视图。 */
XRT_API bool xrtNetProxyInfo(
	const xnetproxy* pProxy,
	xnetproxyinfo* pInfo
)
{
	if ( (pProxy == NULL) || (pInfo == NULL) ) {
		return __xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CONFIG,
			"get-proxy-info", "proxy or info output is null"
		);
	}
	*pInfo = pProxy->Info;
	return true;
}

#endif



#if defined(XRT_FEATURE_NET_PROXY_HANDSHAKE)

/* 设置终态错误并持有一份跨执行上下文可读取的错误引用。 */
xnetproxyhandshakestate __xrtNetProxyHandshakeFailCause(
	xnetproxyhandshake* pHandshake,
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pBuilt;
	const xerror* pError;

	if ( pHandshake == NULL ) {
		memset(&Desc, 0, sizeof(Desc));
		Desc.Kind = Kind;
		Desc.Domain = "xrt.net";
		Desc.Code = (int32)Code;
		Desc.Operation = sOperation;
		Desc.Message = sMessage;
		Desc.Cause = pCause;
		pBuilt = xrtErrorBuild(&Desc);
		if ( pBuilt != NULL ) {
			__xrtErrorSetOwned(pBuilt);
		}
		return XNET_PROXY_HANDSHAKE_ERROR;
	}
	if ( pHandshake->State == XNET_PROXY_HANDSHAKE_ERROR ) {
		return pHandshake->State;
	}
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.net";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pBuilt = xrtErrorBuild(&Desc);
	if ( pBuilt != NULL ) {
		__xrtErrorSetOwned(pBuilt);
	}
	pError = xrtGetError();
	xrtErrorFree(pHandshake->Error);
	pHandshake->Error = pError != NULL ? xrtErrorRef(pError) : NULL;
	pHandshake->State = XNET_PROXY_HANDSHAKE_ERROR;
	return pHandshake->State;
}



/* 设置没有额外原因链的代理握手错误。 */
xnetproxyhandshakestate __xrtNetProxyHandshakeFail(
	xnetproxyhandshake* pHandshake,
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	return __xrtNetProxyHandshakeFailCause(
		pHandshake, Kind, Code, sOperation, sMessage, NULL
	);
}



/* 追加完整协议报文，失败时把握手原子地切换到错误终态。 */
bool __xrtNetProxyHandshakeQueue(
	xnetproxyhandshake* pHandshake,
	const void* pData,
	size_t iSize
)
{
	if ( (pHandshake == NULL) || (pData == NULL) || (iSize == 0) ) {
		(void)__xrtNetProxyHandshakeFail(
			pHandshake, XERR_INTERNAL, XNET_ERROR_PROXY_PROTOCOL,
			"queue-proxy-handshake", "proxy backend produced an empty message"
		);
		return false;
	}
	if ( !xrtNetBufEmpty(&pHandshake->Output) ) {
		(void)__xrtNetProxyHandshakeFail(
			pHandshake, XERR_STATE, XNET_ERROR_PROXY_PROTOCOL,
			"queue-proxy-handshake", "previous proxy output is still pending"
		);
		return false;
	}
	if ( !xrtNetBufAppend(&pHandshake->Output, pData, iSize) ) {
		(void)__xrtNetProxyHandshakeFail(
			pHandshake, XERR_MEMORY, XNET_ERROR_PROXY_CREATE,
			"queue-proxy-handshake", "proxy output allocation failed"
		);
		return false;
	}
	pHandshake->State = XNET_PROXY_HANDSHAKE_WRITE;
	return true;
}



/* 初始化通用握手容量和空所有权字段。 */
XRT_API void xrtNetProxyHandshakeConfigInit(
	xnetproxyhandshakeconfig* pConfig
)
{
	if ( pConfig == NULL ) {
		(void)__xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CONFIG,
			"init-proxy-handshake-config", "proxy handshake config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->ReceiveLimit = XRT_NET_PROXY_RECEIVE_LIMIT;
}



/* 验证通用握手字段以及当前构建中真正存在的协议后端。 */
static bool __xrtNetProxyHandshakeConfigValid(
	const xnetproxyhandshakeconfig* pConfig,
	size_t* pSize
)
{
	xnetproxyinfo Info = {0};
	size_t iSize = sizeof(xnetproxyhandshake);
	#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)
		xnetaddr Address;
	#endif

	if ( (pConfig == NULL) || (pSize == NULL) ||
		(pConfig->Proxy == NULL) ) {
		return __xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CONFIG,
			"create-proxy-handshake", "proxy handshake config is incomplete"
		);
	}
	if ( !__xrtNetProxyHostValid(
		pConfig->TargetHost,
		"create-proxy-handshake",
		"proxy target host is empty"
	) || (pConfig->TargetPort == 0) ) {
		if ( pConfig->TargetPort == 0 ) {
			(void)__xrtNetProxyError(
				XERR_RANGE, XNET_ERROR_PROXY_CONFIG,
				"create-proxy-handshake", "proxy target port is zero"
			);
		}
		return false;
	}
	if ( pConfig->ReceiveLimit < 4u ) {
		return __xrtNetProxyError(
			XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
			"create-proxy-handshake", "proxy receive limit is too small"
		);
	}
	if ( !xrtNetProxyInfo(pConfig->Proxy, &Info) ) {
		return false;
	}
	if ( Info.Type == XNET_PROXY_SOCKS5 ) {
		#if !defined(XRT_FEATURE_NET_PROXY_SOCKS5)
			return __xrtNetProxyError(
				XERR_UNSUPPORTED, XNET_ERROR_PROXY_UNSUPPORTED,
				"create-proxy-handshake", "SOCKS5 support is not compiled"
			);
		#else
			if ( !__xrtNetAddrTryParse(
				&Address, pConfig->TargetHost, pConfig->TargetPort
			) && (pConfig->TargetHost.Size > UINT8_MAX) ) {
				return __xrtNetProxyError(
					XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
					"create-proxy-handshake",
					"SOCKS5 target host exceeds the protocol limit"
				);
			}
		#endif
	} else if ( Info.Type == XNET_PROXY_HTTP_CONNECT ) {
		#if !defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)
			return __xrtNetProxyError(
				XERR_UNSUPPORTED, XNET_ERROR_PROXY_UNSUPPORTED,
				"create-proxy-handshake",
				"HTTP CONNECT support is not compiled"
			);
		#endif
	} else {
		return __xrtNetProxyError(
			XERR_UNSUPPORTED, XNET_ERROR_PROXY_UNSUPPORTED,
			"create-proxy-handshake", "proxy protocol is unsupported"
		);
	}
	if ( !__xrtNetProxyAddSize(
		&iSize, pConfig->TargetHost.Size + 1u
	) ) {
		return __xrtNetProxyError(
			XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
			"create-proxy-handshake", "proxy handshake allocation size overflows"
		);
	}
	*pSize = iSize;
	return true;
}



/* 创建通用握手所有权，并由选定协议后端生成首个报文。 */
XRT_API xnetproxyhandshake* xrtNetProxyHandshakeCreate(
	const xnetproxyhandshakeconfig* pConfig
)
{
	xnetproxyhandshake* pHandshake;
	xnetproxyinfo Info = {0};
	size_t iSize;

	if ( !__xrtNetProxyHandshakeConfigValid(pConfig, &iSize) ) {
		return NULL;
	}
	pHandshake = (xnetproxyhandshake*)xrtMalloc(iSize);
	if ( pHandshake == NULL ) {
		return NULL;
	}
	memset(pHandshake, 0, sizeof(*pHandshake));
	pHandshake->AllocationSize = iSize;
	pHandshake->TargetSize = pConfig->TargetHost.Size;
	pHandshake->ReceiveLimit = pConfig->ReceiveLimit;
	pHandshake->TargetPort = pConfig->TargetPort;
	pHandshake->TargetHost = (str)(pHandshake + 1);
	memcpy(
		pHandshake->TargetHost,
		pConfig->TargetHost.Data,
		pConfig->TargetHost.Size
	);
	pHandshake->TargetHost[pConfig->TargetHost.Size] = 0;
	if ( !xrtNetBufInit(&pHandshake->Output, pConfig->Pool) ) {
		xrtSecureZero(pHandshake, iSize);
		xrtFree(pHandshake);
		return NULL;
	}
	pHandshake->Proxy = xrtNetProxyRetain(pConfig->Proxy);
	if ( pHandshake->Proxy == NULL ) {
		xrtNetBufClear(&pHandshake->Output);
		xrtSecureZero(pHandshake, iSize);
		xrtFree(pHandshake);
		return NULL;
	}
	if ( !xrtNetProxyInfo(pHandshake->Proxy, &Info) ) {
		xrtNetProxyHandshakeDestroy(pHandshake);
		return NULL;
	}

	#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)
		if ( Info.Type == XNET_PROXY_SOCKS5 ) {
			if ( __xrtNetProxySocks5Start(pHandshake) ) {
				return pHandshake;
			}
		}
	#endif
	#if defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)
		if ( Info.Type == XNET_PROXY_HTTP_CONNECT ) {
			if ( __xrtNetProxyHttpStart(pHandshake) ) {
				return pHandshake;
			}
		}
	#endif

	xrtNetProxyHandshakeDestroy(pHandshake);
	return NULL;
}



/* 销毁握手的错误、代理引用、动态绑定域名和敏感输出。 */
XRT_API void xrtNetProxyHandshakeDestroy(xnetproxyhandshake* pHandshake)
{
	size_t iSize;

	if ( pHandshake == NULL ) {
		return;
	}
	iSize = pHandshake->AllocationSize;
	__xrtNetBufClearSecure(&pHandshake->Output);
	xrtErrorFree(pHandshake->Error);
	xrtNetProxyRelease(pHandshake->Proxy);
	if ( pHandshake->BoundHost != NULL ) {
		xrtSecureZero(
			pHandshake->BoundHost,
			pHandshake->Bound.Host.Size + 1u
		);
		xrtFree(pHandshake->BoundHost);
	}
	xrtSecureZero(pHandshake, iSize);
	xrtFree(pHandshake);
}



/* 返回无需锁定的当前传输状态。 */
XRT_API xnetproxyhandshakestate xrtNetProxyHandshakeState(
	const xnetproxyhandshake* pHandshake
)
{
	return pHandshake != NULL ?
		pHandshake->State : XNET_PROXY_HANDSHAKE_ERROR;
}



/* 在输出完全确认后调用选定协议解析器消费输入前缀。 */
XRT_API xnetproxyhandshakestate xrtNetProxyHandshakeStep(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
)
{
	xnetproxyinfo Info = {0};

	if ( pHandshake == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XNET_PROXY_HANDSHAKE_ERROR;
	}
	if ( (pHandshake->State == XNET_PROXY_HANDSHAKE_READY) ||
		(pHandshake->State == XNET_PROXY_HANDSHAKE_ERROR) ) {
		return pHandshake->State;
	}
	if ( !xrtNetBufEmpty(&pHandshake->Output) ) {
		pHandshake->State = XNET_PROXY_HANDSHAKE_WRITE;
		return pHandshake->State;
	}
	if ( pInput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return pHandshake->State;
	}
	pHandshake->State = XNET_PROXY_HANDSHAKE_READ;
	if ( !xrtNetProxyInfo(pHandshake->Proxy, &Info) ) {
		return __xrtNetProxyHandshakeFailCause(
			pHandshake,
			XERR_STATE,
			XNET_ERROR_PROXY_CONFIG,
			"step-proxy-handshake",
			"proxy handshake lost its proxy configuration",
			xrtGetError()
		);
	}

	#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)
		if ( Info.Type == XNET_PROXY_SOCKS5 ) {
			return __xrtNetProxySocks5Step(pHandshake, pInput);
		}
	#endif
	#if defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)
		if ( Info.Type == XNET_PROXY_HTTP_CONNECT ) {
			return __xrtNetProxyHttpStep(pHandshake, pInput);
		}
	#endif

	return __xrtNetProxyHandshakeFail(
		pHandshake, XERR_UNSUPPORTED, XNET_ERROR_PROXY_UNSUPPORTED,
		"step-proxy-handshake", "proxy protocol backend is unavailable"
	);
}



/* 借用当前协议输出的首个连续 Span。 */
XRT_API bool xrtNetProxyHandshakeOutput(
	const xnetproxyhandshake* pHandshake,
	xnetspan* pOutput
)
{
	if ( pOutput != NULL ) {
		pOutput->Data = NULL;
		pOutput->Size = 0;
	}
	if ( (pHandshake == NULL) || (pOutput == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtNetBufEmpty(&pHandshake->Output) ) {
		return false;
	}
	return xrtNetBufFront(&pHandshake->Output, pOutput);
}



/* 清零并消费真正已经写入传输的输出前缀。 */
XRT_API size_t xrtNetProxyHandshakeSent(
	xnetproxyhandshake* pHandshake,
	size_t iSize
)
{
	size_t iAvailable;

	if ( pHandshake == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	iAvailable = xrtNetBufSize(&pHandshake->Output);
	if ( (pHandshake->State != XNET_PROXY_HANDSHAKE_WRITE) ||
		(iSize > iAvailable) ) {
		(void)__xrtNetProxyError(
			XERR_STATE, XNET_ERROR_PROXY_PROTOCOL,
			"send-proxy-handshake", "proxy output acknowledgement is invalid"
		);
		return 0;
	}
	if ( iSize != 0 ) {
		(void)__xrtNetBufConsumeSecure(&pHandshake->Output, iSize);
	}
	if ( xrtNetBufEmpty(&pHandshake->Output) ) {
		pHandshake->State = XNET_PROXY_HANDSHAKE_READ;
	}
	return iSize;
}



/* READY 后复制代理服务器返回的绑定端点。 */
XRT_API bool xrtNetProxyHandshakeBound(
	const xnetproxyhandshake* pHandshake,
	xnetproxyendpoint* pEndpoint
)
{
	if ( (pHandshake == NULL) || (pEndpoint == NULL) ) {
		return __xrtNetProxyError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_PROTOCOL,
			"get-proxy-bound", "proxy handshake or endpoint output is null"
		);
	}
	if ( pHandshake->State != XNET_PROXY_HANDSHAKE_READY ) {
		return __xrtNetProxyError(
			XERR_STATE, XNET_ERROR_PROXY_PROTOCOL,
			"get-proxy-bound", "proxy handshake is not ready"
		);
	}
	if ( !pHandshake->HasBound ) {
		return __xrtNetProxyError(
			XERR_NOT_FOUND, XNET_ERROR_PROXY_PROTOCOL,
			"get-proxy-bound",
			"proxy protocol did not report a bound endpoint"
		);
	}
	*pEndpoint = pHandshake->Bound;
	return true;
}



/* 返回握手终态捕获的错误引用。 */
XRT_API const xerror* xrtNetProxyHandshakeError(
	const xnetproxyhandshake* pHandshake
)
{
	return pHandshake != NULL ? pHandshake->Error : NULL;
}



/* 复制已经收到的线路回复码。 */
XRT_API bool xrtNetProxyHandshakeCode(
	const xnetproxyhandshake* pHandshake,
	uint32* pCode
)
{
	if ( (pHandshake == NULL) || (pCode == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !pHandshake->HasCode ) {
		return false;
	}
	*pCode = pHandshake->Code;
	return true;
}

#endif
