#include "../internal/xrt_tls_server.h"



#if defined(XRT_FEATURE_TLS_SERVER)

/* 设置服务端角色错误并返回 false。 */
bool __xrtTlsServerError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(Kind, Code, sOperation, sMessage, SIZE_MAX);
	return false;
}



/* 包装服务端创建或驱动阶段的底层失败并保留原因链。 */
bool __xrtTlsServerCause(cstr sOperation, cstr sMessage)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_INTERNAL;

	__xrtTlsErrorCause(
		Kind, XTLS_ERROR_HANDSHAKE,
		sOperation, sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 安全累加服务端角色尾部存储尺寸。 */
static bool __xrtTlsServerAddSize(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		return __xrtTlsServerError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "create-tls-server",
			"TLS server state size overflows"
		);
	}
	*pSize += iAdd;
	return true;
}



/* 验证服务器偏好协议并统计深复制所需字节。 */
static bool __xrtTlsServerProtocols(
	const xtlsserverconfig* pConfig,
	size_t* pBytes
)
{
	size_t iBytes = 0;
	size_t iWire = 2u;

	if ( (pConfig->Protocols == NULL) &&
		(pConfig->ProtocolCount != 0) ) {
		return __xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "create-tls-server",
			"TLS server ALPN protocol array is null"
		);
	}
	for ( size_t i = 0; i < pConfig->ProtocolCount; i++ ) {
		const xstrview* pProtocol = &pConfig->Protocols[i];

		if ( (pProtocol->Data == NULL) || (pProtocol->Size == 0) ||
			(pProtocol->Size > UINT8_MAX) ||
			(pProtocol->Size > (UINT16_MAX - iWire - 1u)) ||
			(pProtocol->Size > (SIZE_MAX - iBytes)) ) {
			return __xrtTlsServerError(
				XERR_VALUE, XTLS_ERROR_EXTENSION, "create-tls-server",
				"TLS server ALPN protocol list is invalid"
			);
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( (pConfig->Protocols[j].Size == pProtocol->Size) &&
				(memcmp(
					pConfig->Protocols[j].Data,
					pProtocol->Data,
					pProtocol->Size
				) == 0) ) {
				return __xrtTlsServerError(
					XERR_EXISTS, XTLS_ERROR_EXTENSION,
					"create-tls-server",
					"TLS server ALPN protocol list contains a duplicate"
				);
			}
		}
		iBytes += pProtocol->Size;
		iWire += 1u + pProtocol->Size;
	}
	*pBytes = iBytes;
	return true;
}



/* 确认策略具有当前构建可执行的 TLS 1.3 服务端底座。 */
static bool __xrtTlsServerCapabilities(
	const xtlspolicy* pPolicy,
	size_t* pSecretCapacity
)
{
	size_t iSecret = 0;
	bool bVersion12 = false;
	bool bVersion13 = false;
	bool bGroup = false;

	if ( pPolicy == NULL ) {
		return __xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "create-tls-server",
			"TLS server policy is null"
		);
	}
	for ( size_t i = 0; i < pPolicy->VersionCount; i++ ) {
		if ( pPolicy->Versions[i] == XTLS_VERSION_12 ) {
			bVersion12 = true;
		} else if ( pPolicy->Versions[i] == XTLS_VERSION_13 ) {
			bVersion13 = true;
		}
	}
	if ( !bVersion12 && !bVersion13 ) {
		return __xrtTlsServerError(
			XERR_UNSUPPORTED, XTLS_ERROR_VERSION, "create-tls-server",
			"TLS server policy does not enable TLS 1.2 or TLS 1.3"
		);
	}
	for ( size_t i = 0; i < pPolicy->CipherCount; i++ ) {
		const xtlscipherinfo* pCipher = xrtTlsCipherInfo(
			pPolicy->Ciphers[i]
		);
		xcryptohash Hash;

		if ( (pCipher == NULL) ||
			((pCipher->Version == XTLS_VERSION_12) && !bVersion12) ||
			((pCipher->Version == XTLS_VERSION_13) && !bVersion13) ||
			((pCipher->Version != XTLS_VERSION_12) &&
			 (pCipher->Version != XTLS_VERSION_13)) ) {
			continue;
		}
		Hash = __xrtTlsHash(pCipher->Hash);
		if ( __xrtTlsRecordCipherSupported(
			pCipher->Version, pCipher->Cipher
		) && __xrtTlsScheduleHashSupported(Hash) &&
			(iSecret < pCipher->HashSize) ) {
			iSecret = pCipher->HashSize;
		}
	}
	if ( (iSecret == 0) || (iSecret > XTLS_SERVER_SECRET_MAX_SIZE) ) {
		return __xrtTlsServerError(
			XERR_UNSUPPORTED, XTLS_ERROR_CIPHER, "create-tls-server",
			"TLS server has no enabled record and schedule backend"
		);
	}
	for ( size_t i = 0; i < pPolicy->GroupCount; i++ ) {
		if ( xrtTlsGroupAvailable(pPolicy->Groups[i]) ) {
			bGroup = true;
			break;
		}
	}
	if ( !bGroup ) {
		return __xrtTlsServerError(
			XERR_UNSUPPORTED, XTLS_ERROR_KEY_EXCHANGE,
			"create-tls-server",
			"TLS server has no enabled key exchange backend"
		);
	}
	if ( pPolicy->SignatureCount == 0 ) {
		return __xrtTlsServerError(
			XERR_UNSUPPORTED, XTLS_ERROR_NEGOTIATION,
			"create-tls-server",
			"TLS server policy has no signature scheme"
		);
	}
	*pSecretCapacity = iSecret;
	return true;
}



/* 把协议配置与全部长期秘密排进一次尾部分配。 */
static void __xrtTlsServerLayout(
	xtlsserverstate* pState,
	const xtlsserverconfig* pConfig,
	size_t iSecretCapacity
)
{
	bytes pStorage = (bytes)(pState + 1);

	pState->Protocols = (xbytesview*)pStorage;
	pState->ProtocolCount = pConfig->ProtocolCount;
	pStorage += pConfig->ProtocolCount * sizeof(xbytesview);
	for ( size_t i = 0; i < pConfig->ProtocolCount; i++ ) {
		pState->Protocols[i].Data = pStorage;
		pState->Protocols[i].Size = pConfig->Protocols[i].Size;
		memcpy(
			pStorage,
			pConfig->Protocols[i].Data,
			pConfig->Protocols[i].Size
		);
		pStorage += pConfig->Protocols[i].Size;
	}
	pState->ClientHandshakeTraffic = pStorage;
	pStorage += iSecretCapacity;
	pState->ClientApplicationTraffic = pStorage;
	pStorage += iSecretCapacity;
	pState->ServerApplicationTraffic = pStorage;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		pStorage += iSecretCapacity;
		pState->MasterSecret = pStorage;
		pStorage += iSecretCapacity;
		pState->ResumptionMaster = pStorage;
	#endif
	pState->SecretCapacity = iSecretCapacity;
}



/* 释放服务器跨记录状态、SNI 和共享身份引用。 */
static void __xrtTlsServerClean(xtlssession* pSession, ptr pRole)
{
	xtlsserverstate* pState = (xtlsserverstate*)pRole;

	(void)pSession;
	if ( pState == NULL ) {
		return;
	}
	xrtTlsHandshakeReaderUnit(&pState->Reader);
	xrtTempSecureUnit(&pState->HandshakeArena);
	__xrtTlsTranscriptClear(&pState->Transcript);
	__xrtTlsRecordKeyClear(&pState->PendingReadKey);
	__xrtTlsRecordKeyClear(&pState->PendingWriteKey);
	xrtSecureZero(pState->PrivateKey, sizeof(pState->PrivateKey));
	xrtSecureZero(pState->Master, sizeof(pState->Master));
	if ( pState->ServerNameStorage != NULL ) {
		xrtSecureZero(
			pState->ServerNameStorage, pState->ServerName.Size
		);
		xrtFree(pState->ServerNameStorage);
	}
	if ( pState->RetryClientHello != NULL ) {
		xrtSecureZero(
			pState->RetryClientHello, pState->RetryClientHelloSize
		);
		xrtFree(pState->RetryClientHello);
		pState->RetryClientHello = NULL;
	}
	xrtTlsIdentityRelease((xtlsidentity*)pState->Identity);
	pState->Identity = NULL;
}



/* 初始化不借用任何外部资源的服务端配置。 */
XRT_API void xrtTlsServerConfigInit(xtlsserverconfig* pConfig)
{
	if ( pConfig == NULL ) {
		(void)__xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"init-tls-server-config", "TLS server config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->ResumeAgeTolerance =
		XTLS_SERVER_RESUME_AGE_TOLERANCE_DEFAULT;
}



/* 创建精确分配的服务端状态并等待首条 ClientHello。 */
XRT_API xtlssession* xrtTlsServerCreate(
	const xtlsserverconfig* pConfig,
	xnetbufpool* pPool
)
{
	xtlsserverconfig Config;
	const xtlscontext* pContext;
	xtlscontext* pDefault = NULL;
	const xtlspolicy* pPolicy;
	const xtlslimits* pLimits;
	xtlshandshakereaderconfig ReaderConfig;
	xtlssession* pSession;
	xtlsserverstate* pState;
	size_t iProtocolBytes = 0;
	size_t iSecretCapacity = 0;
	size_t iRoleSize = sizeof(xtlsserverstate);

	if ( pConfig == NULL ) {
		xrtTlsServerConfigInit(&Config);
	} else {
		Config = *pConfig;
	}
	pConfig = &Config;
	if ( (pConfig->Identity == NULL) && (pConfig->Select == NULL) ) {
		(void)__xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_IDENTITY, "create-tls-server",
			"TLS server requires a static identity or selector"
		);
		return NULL;
	}
	if ( !__xrtTlsServerProtocols(pConfig, &iProtocolBytes) ) {
		return NULL;
	}
	pContext = pConfig->Context;
	if ( pContext == NULL ) {
		pDefault = xrtTlsContextCreate(NULL);
		if ( pDefault == NULL ) {
			return NULL;
		}
		pContext = pDefault;
	}
	pPolicy = xrtTlsContextPolicy(pContext);
	pLimits = xrtTlsContextLimits(pContext);
	if ( (pLimits == NULL) || !__xrtTlsServerCapabilities(
		pPolicy, &iSecretCapacity
	) || !__xrtTlsServerAddSize(
		&iRoleSize, pConfig->ProtocolCount * sizeof(xbytesview)
	) || !__xrtTlsServerAddSize(
		&iRoleSize, iProtocolBytes
	) || !__xrtTlsServerAddSize(
		&iRoleSize, iSecretCapacity
	) || !__xrtTlsServerAddSize(
		&iRoleSize, iSecretCapacity
	) || !__xrtTlsServerAddSize(
		&iRoleSize, iSecretCapacity
	)
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		|| !__xrtTlsServerAddSize(
			&iRoleSize, iSecretCapacity
		) || !__xrtTlsServerAddSize(
			&iRoleSize, iSecretCapacity
		)
	#endif
	) {
		xrtTlsContextRelease(pDefault);
		return NULL;
	}
	pSession = __xrtTlsSessionCreateSized(
		pContext, pPool, XTLS_SERVER, iRoleSize, __xrtTlsServerClean
	);
	xrtTlsContextRelease(pDefault);
	if ( pSession == NULL ) {
		return NULL;
	}
	pState = (xtlsserverstate*)__xrtTlsSessionRoleData(pSession);
	__xrtTlsServerLayout(pState, pConfig, iSecretCapacity);
	pSession->KeyUpdate = xrtTlsServerKeyUpdate;
	{
		xtempconfig TempConfig = {
			4096u,
			65536u,
			0u
		};

		if ( !xrtTempInit(&pState->HandshakeArena, &TempConfig) ) {
			xrtTlsSessionDestroy(pSession);
			return NULL;
		}
	}
	pState->Select = pConfig->Select;
	pState->SelectContext = pConfig->SelectContext;
	pState->RequireProtocol = pConfig->RequireProtocol;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		pState->Resume = pConfig->Resume;
		pState->ResumeContext = pConfig->ResumeContext;
		pState->ResumeAgeTolerance = pConfig->ResumeAgeTolerance;
	#endif
	pState->Step = XTLS_SERVER_WAIT_CLIENT_HELLO;
	if ( pConfig->Identity != NULL ) {
		pState->Identity = xrtTlsIdentityRetain(pConfig->Identity);
		if ( pState->Identity == NULL ) {
			xrtTlsSessionDestroy(pSession);
			return NULL;
		}
	}
	xrtTlsHandshakeReaderConfigInit(&ReaderConfig);
	ReaderConfig.Limit = pLimits->HandshakeLimit;
	if ( ReaderConfig.Retain > ReaderConfig.Limit ) {
		ReaderConfig.Retain = ReaderConfig.Limit;
	}
	if ( !xrtTlsHandshakeReaderInit(
		&pState->Reader, &ReaderConfig
	) || !__xrtTlsSessionSetState(
		pSession, XTLS_STATE_HANDSHAKE
	) || !__xrtTlsSessionSetWait(
		pSession, XTLS_WAIT_INPUT
	) ) {
		xrtTlsSessionDestroy(pSession);
		return NULL;
	}
	return pSession;
}



/* 借用已经从 ClientHello 独立保存的 SNI。 */
XRT_API bool xrtTlsServerName(
	const xtlssession* pSession,
	xbytesview* pServerName
)
{
	xtlsserverstate* pState;

	if ( (pSession == NULL) || (pServerName == NULL) ) {
		return __xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "get-tls-server-name",
			"TLS server session or name output is null"
		);
	}
	if ( xrtTlsSessionRole(pSession) != XTLS_SERVER ) {
		return __xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "get-tls-server-name",
			"TLS session is not a server"
		);
	}
	pState = (xtlsserverstate*)__xrtTlsSessionRoleData(
		(xtlssession*)pSession
	);
	if ( (pState == NULL) || (pState->ServerName.Size == 0) ) {
		return false;
	}
	*pServerName = pState->ServerName;
	return true;
}



/* 读取动态选择器随已选身份提交的不透明宿主 Cookie。 */
XRT_API bool xrtTlsServerCookie(
	const xtlssession* pSession,
	uint64* pCookie
)
{
	xtlsserverstate* pState;

	if ( (pSession == NULL) || (pCookie == NULL) ) {
		return __xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "get-tls-server-cookie",
			"TLS server session or cookie output is null"
		);
	}
	if ( xrtTlsSessionRole(pSession) != XTLS_SERVER ) {
		return __xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "get-tls-server-cookie",
			"TLS session is not a server"
		);
	}
	pState = (xtlsserverstate*)__xrtTlsSessionRoleData(
		(xtlssession*)pSession
	);
	if ( pState == NULL ) {
		return false;
	}
	*pCookie = pState->Cookie;
	return true;
}



/* 把当前输入、输出和应用背压统一转换成等待位。 */
bool __xrtTlsServerWait(
	xtlssession* pSession,
	bool bInput
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(pSession->Context);
	uint32 iWait = XTLS_WAIT_NONE;
	xtlsstate State = xrtTlsSessionState(pSession);

	if ( pLimits == NULL ) {
		return __xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"drive-tls-server", "TLS server limits are unavailable"
		);
	}
	if ( xrtTlsSessionSendSize(pSession) != 0 ) {
		iWait |= XTLS_WAIT_OUTPUT;
	}
	if ( (State == XTLS_STATE_HANDSHAKE) && bInput ) {
		iWait |= XTLS_WAIT_INPUT;
	} else if ( (State == XTLS_STATE_READY) ||
		(State == XTLS_STATE_CLOSING) ) {
		if ( xrtTlsSessionPlainSize(pSession) >= pLimits->PlainLimit ) {
			iWait |= XTLS_WAIT_APPLICATION;
		} else if ( !pSession->CloseReceived && bInput ) {
			iWait |= XTLS_WAIT_INPUT;
		}
	}
	return __xrtTlsSessionSetWait(pSession, iWait);
}



/* 把当前服务端会话推进到失败终态，同时保留已设置的根错误。 */
static xtlsresult __xrtTlsServerFailed(xtlssession* pSession)
{
	return __xrtTlsSessionFail(pSession);
}



/* 收到 KeyUpdate 后先排队旧 epoch 应答，再原子提交下一代收发 epoch。 */
static xtlsresult __xrtTlsServerKeyUpdateCommit(
	xtlssession* pSession,
	xtlsserverstate* pState,
	xtlskeyupdate Request
)
{
	xtlssessionupdate Next;
	xtlsresult Result = XTLS_OK;

	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTls13KeyUpdateReceive(
		pState->Cipher,
		(xbytesview) {
			pState->ClientApplicationTraffic, pState->HashSize
		},
		(xbytesview) {
			pState->ServerApplicationTraffic, pState->HashSize
		}, Request, &Next, "process-key-update"
	) ) {
		return __xrtTlsServerFailed(pSession);
	}
	if ( Request == XTLS_KEY_UPDATE_REQUESTED ) {
		Result = __xrtTlsSessionRecordProtect(
			pSession, XTLS_RECORD_HANDSHAKE,
			(xbytesview) { Next.Message, Next.MessageSize }, 0
		);
		if ( Result != XTLS_OK ) {
			__xrtTlsRecordKeyClear(&Next.WriteKey);
			__xrtTlsRecordKeyClear(&Next.ReadKey);
			xrtSecureZero(&Next, sizeof(Next));
			return Result == XTLS_AGAIN ?
				XTLS_AGAIN : __xrtTlsServerFailed(pSession);
		}
	}

	/* 密文应答成功后只执行不会失败的 epoch 提交。 */
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	pSession->ReadKey = Next.ReadKey;
	xrtSecureZero(&Next.ReadKey, sizeof(Next.ReadKey));
	memcpy(
		pState->ClientApplicationTraffic,
		Next.ReadTraffic, pState->HashSize
	);
	if ( Request == XTLS_KEY_UPDATE_REQUESTED ) {
		__xrtTlsRecordKeyClear(&pSession->WriteKey);
		pSession->WriteKey = Next.WriteKey;
		xrtSecureZero(&Next.WriteKey, sizeof(Next.WriteKey));
		memcpy(
			pState->ServerApplicationTraffic,
			Next.WriteTraffic, pState->HashSize
		);
	}
	xrtSecureZero(&Next, sizeof(Next));
	return XTLS_OK;
}



/* 主动消息使用旧写 epoch 入队，成功后才提交下一代服务端写 epoch。 */
XRT_API xtlsresult xrtTlsServerKeyUpdate(
	xtlssession* pSession,
	xtlskeyupdate Request
)
{
	xtlsserverstate* pState;
	xtlssessionupdate Next;
	xtlsresult Result;

	if ( pSession == NULL ) {
		(void)__xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"key-update-tls-server", "TLS server session is null"
		);
		return XTLS_ERROR;
	}
	if ( xrtTlsSessionRole(pSession) != XTLS_SERVER ) {
		(void)__xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"key-update-tls-server", "TLS session is not a server"
		);
		return XTLS_ERROR;
	}
	if ( xrtTlsSessionState(pSession) != XTLS_STATE_READY ) {
		(void)__xrtTlsServerError(
			XERR_STATE, XTLS_ERROR_STATE,
			"key-update-tls-server",
			"TLS server KeyUpdate requires a ready session"
		);
		return XTLS_ERROR;
	}
	if ( xrtTlsSessionVersion(pSession) != XTLS_VERSION_13 ) {
		(void)__xrtTlsServerError(
			XERR_UNSUPPORTED, XTLS_ERROR_VERSION,
			"key-update-tls-server",
			"TLS KeyUpdate requires a TLS 1.3 session"
		);
		return XTLS_ERROR;
	}
	if ( (Request != XTLS_KEY_UPDATE_NOT_REQUESTED) &&
		(Request != XTLS_KEY_UPDATE_REQUESTED) ) {
		(void)__xrtTlsServerError(
			XERR_VALUE, XTLS_ERROR_HANDSHAKE,
			"key-update-tls-server",
			"TLS server KeyUpdate request value is invalid"
		);
		return XTLS_ERROR;
	}
	pState = (xtlsserverstate*)__xrtTlsSessionRoleData(pSession);
	if ( (pState == NULL) || (pState->Step != XTLS_SERVER_READY) ) {
		(void)__xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"key-update-tls-server",
			"TLS server KeyUpdate role state is invalid"
		);
		return XTLS_ERROR;
	}
	Result = __xrtTlsSessionKeyUpdateWritable(
		pSession, "key-update-tls-server"
	);
	if ( Result != XTLS_OK ) {
		if ( (Result == XTLS_AGAIN) &&
			!__xrtTlsServerWait(pSession, true) ) {
			return __xrtTlsServerFailed(pSession);
		}
		return Result;
	}
	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTls13KeyUpdateSend(
		pState->Cipher,
		(xbytesview) {
			pState->ServerApplicationTraffic, pState->HashSize
		}, Request, &Next, "key-update-tls-server"
	) ) {
		return XTLS_ERROR;
	}
	Result = __xrtTlsSessionRecordProtect(
		pSession, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Next.Message, Next.MessageSize }, 0
	);
	if ( Result != XTLS_OK ) {
		__xrtTlsRecordKeyClear(&Next.WriteKey);
		xrtSecureZero(&Next, sizeof(Next));
		if ( (Result == XTLS_AGAIN) &&
			!__xrtTlsServerWait(pSession, true) ) {
			return __xrtTlsServerFailed(pSession);
		}
		return Result;
	}
	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	pSession->WriteKey = Next.WriteKey;
	xrtSecureZero(&Next.WriteKey, sizeof(Next.WriteKey));
	memcpy(
		pState->ServerApplicationTraffic,
		Next.WriteTraffic, pState->HashSize
	);
	xrtSecureZero(&Next, sizeof(Next));
	if ( !__xrtTlsServerWait(pSession, true) ) {
		return __xrtTlsServerFailed(pSession);
	}
	return XTLS_OK;
}



/* READY 状态只接受与记录边界严格对齐的 TLS 1.3 KeyUpdate。 */
static xtlsresult __xrtTlsServerPostHandshakeRecord(
	xtlssession* pSession,
	xtlsserverstate* pState,
	xbytesview Input,
	size_t* pConsumed,
	bool* pComplete
)
{
	xtlshandshake Message;
	xtlskeyupdate Request;
	xtlsresult Result;
	bool bContinued = (pState->Reader.Size != 0) ||
		(pState->Reader.HeaderSize != 0);

	*pConsumed = 0;
	*pComplete = false;
	if ( pState->Version != XTLS_VERSION_13 ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_VERSION, "drive-tls-server",
			"TLS 1.2 does not permit post-handshake messages"
		);
	}
	Result = xrtTlsHandshakeReaderRead(
		&pState->Reader, Input, pConsumed, &Message
	);
	if ( Result == XTLS_ERROR ) {
		return __xrtTlsServerFailed(pSession);
	}
	if ( Result == XTLS_AGAIN ) {
		if ( *pConsumed != Input.Size ) {
			return __xrtTlsServerProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls-server",
				"TLS post-handshake reader left an incomplete record suffix"
			);
		}
		return XTLS_OK;
	}
	*pComplete = true;
	if ( (Message.Type != XTLS_HANDSHAKE_KEY_UPDATE) || bContinued ||
		(pState->RecordOffset != 0) || (*pConsumed != Input.Size) ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_HANDSHAKE,
			"process-key-update",
			"TLS server requires one complete KeyUpdate per record"
		);
	}
	if ( !xrtTlsKeyUpdateParse(Message.Body, &Request) ) {
		return __xrtTlsServerFailed(pSession);
	}
	if ( Request == XTLS_KEY_UPDATE_REQUESTED ) {
		Result = __xrtTlsSessionKeyUpdateWritable(
			pSession, "process-key-update"
		);
		if ( Result != XTLS_OK ) {
			return Result == XTLS_AGAIN ?
				XTLS_AGAIN : __xrtTlsServerFailed(pSession);
		}
	}
	Result = __xrtTlsServerKeyUpdateCommit(
		pSession, pState, Request
	);
	if ( Result != XTLS_OK ) {
		return Result;
	}
	if ( !xrtTlsHandshakeReaderReset(&pState->Reader) ) {
		return __xrtTlsServerFailed(pSession);
	}
	return XTLS_OK;
}



/* 严格忽略 RFC 8446 兼容 CCS。 */
static xtlsresult __xrtTlsServerChangeCipherSpec(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlssessionrecord* pRecord
)
{
	bool bRetryWindow = pState->RetrySeen &&
		(pState->Step == XTLS_SERVER_WAIT_CLIENT_HELLO);

	if ( pState->Version == XTLS_VERSION_12 ) {
		return __xrtTlsServer12ChangeCipherSpec(
			pSession, pState, pRecord
		);
	}
	if ( (!bRetryWindow &&
		((pState->Version != XTLS_VERSION_13) ||
		 (pState->Step != XTLS_SERVER_WAIT_CLIENT_FINISHED))) ||
		pRecord->Protected ||
		(pRecord->Data.Size != 1u) ||
		(pRecord->Data.Data[0] != 1u) ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_HANDSHAKE, "drive-tls-server",
			"TLS compatibility ChangeCipherSpec is malformed"
		);
	}
	if ( __xrtTlsSessionRecordFinish(pSession, false) != XTLS_OK ) {
		return __xrtTlsServerFailed(pSession);
	}
	return XTLS_OK;
}



/* 处理一段握手记录，允许一条记录顺序承载多条完整握手消息。 */
static xtlsresult __xrtTlsServerHandshakeRecord(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlssessionrecord* pRecord,
	xbytesview Input,
	size_t* pConsumed,
	bool* pComplete
)
{
	xtlshandshake Message;
	xtlsresult Result;

	*pConsumed = 0;
	*pComplete = false;
	if ( (pState->Step == XTLS_SERVER_WAIT_CLIENT_HELLO) &&
		pRecord->Protected ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE, "drive-tls-server",
			"TLS ClientHello arrived in a protected record"
		);
	}
	if ( (pState->Step != XTLS_SERVER_WAIT_CLIENT_HELLO) &&
		(pState->Version == XTLS_VERSION_13) &&
		!pRecord->Protected ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE, "drive-tls-server",
			"TLS 1.3 client handshake arrived in a plaintext record"
		);
	}
	if ( (pState->Version == XTLS_VERSION_12) &&
		(pState->Step == XTLS_SERVER_WAIT_CLIENT_KEY_EXCHANGE) &&
		pRecord->Protected ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE, "drive-tls-server",
			"TLS 1.2 ClientKeyExchange arrived in a protected record"
		);
	}
	if ( (pState->Step == XTLS_SERVER_WAIT_CLIENT_FINISHED) &&
		!pRecord->Protected ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE, "drive-tls-server",
			"TLS client Finished arrived in a plaintext record"
		);
	}
	Result = xrtTlsHandshakeReaderRead(
		&pState->Reader, Input, pConsumed, &Message
	);
	if ( Result == XTLS_ERROR ) {
		return __xrtTlsServerFailed(pSession);
	}
	if ( Result == XTLS_AGAIN ) {
		if ( *pConsumed != Input.Size ) {
			return __xrtTlsServerProtocol(
				pSession, XTLS_ERROR_HANDSHAKE, "drive-tls-server",
				"TLS handshake reader left an incomplete record suffix"
			);
		}
		return XTLS_OK;
	}
	*pComplete = true;
	if ( pState->Step == XTLS_SERVER_WAIT_CLIENT_HELLO ) {
		Result = __xrtTlsServerFirstFlight(
			pSession, pState, &Message
		);
	} else if ( (pState->Version == XTLS_VERSION_12) &&
		((pState->Step == XTLS_SERVER_WAIT_CLIENT_KEY_EXCHANGE) ||
		 (pState->Step == XTLS_SERVER_WAIT_CLIENT_FINISHED)) ) {
		Result = __xrtTlsServer12Handshake(
			pSession, pState, &Message
		);
	} else if ( pState->Step == XTLS_SERVER_WAIT_CLIENT_FINISHED ) {
		Result = __xrtTlsServerFinished(pSession, pState, &Message);
	} else {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_INTERNAL, "drive-tls-server",
			"TLS server handshake step is invalid"
		);
	}
	if ( (Result == XTLS_OK) &&
		!xrtTlsHandshakeReaderReset(&pState->Reader) ) {
		return __xrtTlsServerFailed(pSession);
	}
	return Result;
}



/* 在上下文公平性预算内推进服务端握手和应用记录。 */
XRT_API xtlsresult xrtTlsServerDrive(xtlssession* pSession)
{
	xtlsserverstate* pState;
	const xtlslimits* pLimits;
	xtlsstate State;
	uint32 iRecords = 0;
	uint32 iHandshakes = 0;
	bool bProgress = false;

	if ( pSession == NULL ) {
		(void)__xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"drive-tls-server", "TLS server session is null"
		);
		return XTLS_ERROR;
	}
	if ( xrtTlsSessionRole(pSession) != XTLS_SERVER ) {
		(void)__xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"drive-tls-server", "TLS session is not a server"
		);
		return XTLS_ERROR;
	}
	State = xrtTlsSessionState(pSession);
	if ( State == XTLS_STATE_CLOSED ) {
		return XTLS_CLOSED;
	}
	if ( State == XTLS_STATE_FAILED ) {
		(void)__xrtTlsServerError(
			XERR_STATE, XTLS_ERROR_STATE,
			"drive-tls-server", "TLS server has failed"
		);
		return XTLS_ERROR;
	}
	if ( (State != XTLS_STATE_HANDSHAKE) &&
		(State != XTLS_STATE_READY) &&
		(State != XTLS_STATE_CLOSING) ) {
		(void)__xrtTlsServerError(
			XERR_STATE, XTLS_ERROR_STATE,
			"drive-tls-server",
			"TLS server cannot be driven in this state"
		);
		return XTLS_ERROR;
	}
	pState = (xtlsserverstate*)__xrtTlsSessionRoleData(pSession);
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( (pState == NULL) || (pLimits == NULL) ||
		(pLimits->RecordBudget == 0) ||
		(pLimits->HandshakeBudget == 0) ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_INTERNAL, "drive-tls-server",
			"TLS server role state is invalid"
		);
	}
	while ( iRecords < pLimits->RecordBudget ) {
		xtlssessionrecord Record;
		bool bHandshake = xrtTlsSessionState(pSession) ==
			XTLS_STATE_HANDSHAKE;
		xtlsresult Result = __xrtTlsSessionRecordNext(
			pSession, &Record
		);

		if ( Result == XTLS_AGAIN ) {
			if ( !__xrtTlsServerWait(pSession, true) ) {
				(void)__xrtTlsSessionSetState(
					pSession, XTLS_STATE_FAILED
				);
				return XTLS_ERROR;
			}
			return bProgress ? XTLS_OK : XTLS_AGAIN;
		}
		if ( Result != XTLS_OK ) {
			return __xrtTlsServerFailed(pSession);
		}
		if ( pState->RecordOffset > Record.Data.Size ) {
			return __xrtTlsServerProtocol(
				pSession, XTLS_ERROR_INTERNAL, "drive-tls-server",
				"TLS server record offset exceeds the pending record"
			);
		}
		if ( Record.Type == XTLS_RECORD_ALERT ) {
			if ( pState->RecordOffset != 0 ) {
				return __xrtTlsServerProtocol(
					pSession, XTLS_ERROR_INTERNAL, "drive-tls-server",
					"TLS server changed record type while a record was pending"
				);
			}
			Result = __xrtTlsSessionRecordAlert(pSession, &Record);
			if ( Result == XTLS_ERROR ) {
				return Result;
			}
			iRecords++;
			bProgress = true;
			if ( bHandshake ) {
				return __xrtTlsServerProtocol(
					pSession, XTLS_ERROR_CLOSED, "drive-tls-server",
					"TLS peer closed during the server handshake"
				);
			}
			if ( (Result == XTLS_CLOSED) ||
				(xrtTlsSessionState(pSession) == XTLS_STATE_CLOSED) ) {
				return XTLS_CLOSED;
			}
			continue;
		}
		if ( !bHandshake &&
			(Record.Type == XTLS_RECORD_APPLICATION_DATA) ) {
			if ( pState->RecordOffset != 0 ) {
				return __xrtTlsServerProtocol(
					pSession, XTLS_ERROR_INTERNAL, "drive-tls-server",
					"TLS server has a partial application record"
				);
			}
			Result = __xrtTlsSessionRecordFinish(
				pSession, !pSession->CloseReceived
			);
			if ( Result == XTLS_AGAIN ) {
				if ( !__xrtTlsServerWait(pSession, false) ) {
					(void)__xrtTlsSessionSetState(
						pSession, XTLS_STATE_FAILED
					);
					return XTLS_ERROR;
				}
				return bProgress ? XTLS_OK : XTLS_AGAIN;
			}
			if ( Result != XTLS_OK ) {
				(void)__xrtTlsSessionSetState(
					pSession, XTLS_STATE_FAILED
				);
				return XTLS_ERROR;
			}
			iRecords++;
			bProgress = true;
			continue;
		}
		if ( !bHandshake &&
			(Record.Type != XTLS_RECORD_HANDSHAKE) ) {
			return __xrtTlsServerProtocol(
				pSession, XTLS_ERROR_RECORD_TYPE, "drive-tls-server",
				"TLS server received an unexpected record after the handshake"
			);
		}
		if ( bHandshake &&
			(Record.Type == XTLS_RECORD_CHANGE_CIPHER_SPEC) ) {
			if ( pState->RecordOffset != 0 ) {
				return __xrtTlsServerProtocol(
					pSession, XTLS_ERROR_INTERNAL, "drive-tls-server",
					"TLS server has a partial ChangeCipherSpec record"
				);
			}
			Result = __xrtTlsServerChangeCipherSpec(
				pSession, pState, &Record
			);
			if ( Result != XTLS_OK ) {
				return Result;
			}
			iRecords++;
			bProgress = true;
			continue;
		}
		if ( Record.Type == XTLS_RECORD_HANDSHAKE ) {
			xbytesview Remaining;
			size_t iConsumed;
			bool bComplete;

			if ( iHandshakes >= pLimits->HandshakeBudget ) {
				break;
			}
			if ( bHandshake && (pState->Version == XTLS_VERSION_12) &&
				(pState->Step == XTLS_SERVER_WAIT_CLIENT_FINISHED) ) {
				Result = __xrtTlsServer12FinishedWritable(
					pSession, pState
				);
				if ( Result == XTLS_AGAIN ) {
					if ( !__xrtTlsServerWait(pSession, false) ) {
						return __xrtTlsServerFailed(pSession);
					}
					return bProgress ? XTLS_OK : XTLS_AGAIN;
				}
				if ( Result != XTLS_OK ) {
					return __xrtTlsServerFailed(pSession);
				}
			}
			Remaining.Data = Record.Data.Data + pState->RecordOffset;
			Remaining.Size = Record.Data.Size - pState->RecordOffset;
			if ( bHandshake ) {
				Result = __xrtTlsServerHandshakeRecord(
					pSession, pState, &Record, Remaining,
					&iConsumed, &bComplete
				);
			} else {
				Result = __xrtTlsServerPostHandshakeRecord(
					pSession, pState, Remaining,
					&iConsumed, &bComplete
				);
			}
			if ( !bHandshake && (Result == XTLS_AGAIN) ) {
				if ( !__xrtTlsServerWait(pSession, false) ) {
					return __xrtTlsServerFailed(pSession);
				}
				return bProgress ? XTLS_OK : XTLS_AGAIN;
			}
			if ( Result != XTLS_OK ) {
				return Result;
			}
			pState->RecordOffset += iConsumed;
			bProgress = bProgress || (iConsumed != 0);
			if ( bComplete ) {
				iHandshakes++;
			}
			if ( bHandshake &&
				(xrtTlsSessionState(pSession) == XTLS_STATE_READY) &&
				(pState->RecordOffset != Record.Data.Size) ) {
				return __xrtTlsServerProtocol(
					pSession, XTLS_ERROR_HANDSHAKE, "drive-tls-server",
					"TLS client Finished record contains trailing data"
				);
			}
			if ( pState->RecordOffset == Record.Data.Size ) {
				if ( __xrtTlsSessionRecordFinish(
					pSession, false
				) != XTLS_OK ) {
					(void)__xrtTlsSessionSetState(
						pSession, XTLS_STATE_FAILED
					);
					return XTLS_ERROR;
				}
				pState->RecordOffset = 0;
				iRecords++;
			} else if ( !bComplete ) {
				return __xrtTlsServerProtocol(
					pSession, XTLS_ERROR_INTERNAL, "drive-tls-server",
					"TLS server did not consume an incomplete record"
				);
			}
			if ( bHandshake &&
				(xrtTlsSessionState(pSession) == XTLS_STATE_READY) ) {
				return XTLS_OK;
			}
			continue;
		}
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE, "drive-tls-server",
			"TLS server received an unexpected handshake record type"
		);
	}
	if ( !__xrtTlsServerWait(pSession, true) ) {
		return __xrtTlsServerFailed(pSession);
	}
	return XTLS_OK;
}

#endif
