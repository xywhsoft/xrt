#include "../internal/xrt_tls_server.h"



#if defined(XRT_FEATURE_TLS_SERVER)

/* 首航提交状态持有全部可失败步骤的输出。 */
typedef struct xtlsserverflight {
	xtlstranscript Transcript;
	xtlsrecordkey ReadKey;
	xtlsrecordkey WriteKey;
	const xtlsidentity* Identity;
	bytes ServerName;
	size_t ServerNameSize;
	bytes Output;
	size_t OutputSize;
	size_t Protocol;
	size_t HashSize;
	uint64 Cookie;
	xtlscipher Cipher;
	xtlssignature Signature;
	uint8 ClientHandshake[XTLS_SERVER_SECRET_MAX_SIZE];
	uint8 ClientApplication[XTLS_SERVER_SECRET_MAX_SIZE];
	uint8 ServerApplication[XTLS_SERVER_SECRET_MAX_SIZE];
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		uint8 Master[XTLS_SERVER_SECRET_MAX_SIZE];
		bool Resumed;
	#endif
} xtlsserverflight;



/* 设置服务端协议错误并进入失败终态。 */
xtlsresult __xrtTlsServerProtocol(
	xtlssession* pSession,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	(void)__xrtTlsServerError(
		XERR_PROTOCOL, Code, sOperation, sMessage
	);
	return __xrtTlsSessionFail(pSession);
}



/* 安全累加临时编码尺寸。 */
static bool __xrtTlsServerSize(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		return __xrtTlsServerError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "build-server-flight",
			"TLS server flight size overflows"
		);
	}
	*pSize += iAdd;
	return true;
}



/* 找到扩展时发布负载，缺失时发布空视图。 */
static bool __xrtTlsServerExtension(
	xbytesview Extensions,
	xtlsextensiontype Type,
	xbytesview* pData,
	bool bRequired
)
{
	xtlsextension Extension;
	xtlsitemresult Result = xrtTlsExtensionsFind(
		Extensions, Type, &Extension
	);

	memset(pData, 0, sizeof(*pData));
	if ( Result == XTLS_ITEM_VALUE ) {
		*pData = Extension.Data;
		return true;
	}
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( bRequired ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
			"select-server-flight",
			"TLS ClientHello is missing a required extension"
		);
	}
	return true;
}



/* 按服务器协议偏好选择客户端实际提供的第一项交集。 */
static size_t __xrtTlsServerProtocolDefault(
	const xtlsserverstate* pState,
	xbytesview Offered
)
{
	if ( Offered.Size == 0 ) {
		return XTLS_SERVER_PROTOCOL_NONE;
	}
	for ( size_t i = 0; i < pState->ProtocolCount; i++ ) {
		if ( xrtTlsProtocolFind(
			Offered, pState->Protocols[i]
		) == XTLS_ITEM_VALUE ) {
			return i;
		}
	}
	return XTLS_SERVER_PROTOCOL_NONE;
}



/* 验证选择器返回的协议仍属于配置和客户端 offer。 */
static bool __xrtTlsServerProtocolChoice(
	const xtlsserverstate* pState,
	xbytesview Offered,
	size_t iProtocol
)
{
	if ( iProtocol == XTLS_SERVER_PROTOCOL_NONE ) {
		return !pState->RequireProtocol || __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_NEGOTIATION,
			"select-server-flight",
			"TLS server requires an ALPN protocol"
		);
	}
	if ( (iProtocol >= pState->ProtocolCount) ||
		(Offered.Size == 0) ||
		(xrtTlsProtocolFind(
			Offered, pState->Protocols[iProtocol]
		) != XTLS_ITEM_VALUE) ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_NEGOTIATION,
			"select-server-flight",
			"TLS server selected an ALPN protocol that was not offered"
		);
	}
	return true;
}



/* 选择身份后按服务器偏好和签名器真实能力选择签名方案。 */
static bool __xrtTlsServerSignature(
	const xtlspolicy* pPolicy,
	const xtlsidentity* pIdentity,
	const xtlsids* pOffered,
	xtlsversion Version,
	xtlssignature* pSignature
)
{
	for ( size_t i = 0; i < pPolicy->SignatureCount; i++ ) {
		xtlssignature Signature = pPolicy->Signatures[i];

		if ( xrtTlsIdsContain(pOffered, (uint16)Signature) &&
			xrtTlsIdentityCanSign(
				pIdentity, Version, Signature
			) ) {
			*pSignature = Signature;
			return true;
		}
	}
	return __xrtTlsServerError(
		XERR_PROTOCOL, XTLS_ERROR_NEGOTIATION,
		"select-server-flight",
		"TLS client and server have no usable signature scheme"
	);
}



/* TLS 只支持 1.2/1.3，因此可直接判定 Fallback SCSV 是否不恰当。 */
static bool __xrtTlsServerFallback(
	const xtlspolicy* pPolicy,
	const xtlsclienthello* pHello
)
{
	xtlsextension Extension;
	xtlsids Versions;
	xtlsitemresult Result;
	bool bServer13 = false;
	bool bClient13 = false;

	if ( !xrtTlsIdsContain(
		&pHello->CipherSuites, XTLS_FALLBACK_SCSV
	) ) {
		return true;
	}
	for ( size_t i = 0; i < pPolicy->VersionCount; i++ ) {
		if ( pPolicy->Versions[i] == XTLS_VERSION_13 ) {
			bServer13 = true;
		}
	}
	Result = xrtTlsExtensionsFind(
		pHello->Extensions,
		XTLS_EXTENSION_SUPPORTED_VERSIONS,
		&Extension
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( Result == XTLS_ITEM_VALUE ) {
		if ( !xrtTlsClientVersions(Extension.Data, &Versions) ) {
			return false;
		}
		bClient13 = xrtTlsIdsContain(&Versions, XTLS_VERSION_13);
	}
	if ( bServer13 && !bClient13 ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_VERSION,
			"select-server-flight",
			"TLS client sent an inappropriate fallback signal"
		);
	}
	return true;
}



/* 0-RTT 必须在具备反重放策略前保持显式 fail-closed。 */
static bool __xrtTlsServerEarlyData(const xtlsclienthello* pHello)
{
	xtlsextension Extension;
	xtlsitemresult Result = xrtTlsExtensionsFind(
		pHello->Extensions, XTLS_EXTENSION_EARLY_DATA, &Extension
	);

	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( Result == XTLS_ITEM_VALUE ) {
		return __xrtTlsServerError(
			XERR_UNSUPPORTED, XTLS_ERROR_EXTENSION,
			"select-server-flight",
			"TLS early data is not enabled without an anti-replay policy"
		);
	}
	return true;
}



/* 比较两个借用字节视图。 */
static bool __xrtTlsServerViewEqual(xbytesview Left, xbytesview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



#if defined(XRT_FEATURE_TLS_SERVER_RESUME)

/* HRR 后 PSK 身份和 binder 布局必须稳定，年龄和 binder 内容允许更新。 */
static bool __xrtTlsServerRetryPsksEqual(
	xbytesview First,
	xbytesview Second
)
{
	xtlspskcursor Left;
	xtlspskcursor Right;
	xtlspsk LeftPsk;
	xtlspsk RightPsk;
	xtlsitemresult LeftResult;
	xtlsitemresult RightResult;

	if ( !xrtTlsClientPsks(First, &Left) ||
		!xrtTlsClientPsks(Second, &Right) ) {
		return false;
	}
	do {
		LeftResult = xrtTlsPsksRead(&Left, &LeftPsk);
		RightResult = xrtTlsPsksRead(&Right, &RightPsk);
		if ( LeftResult != RightResult ) {
			return false;
		}
		if ( LeftResult == XTLS_ITEM_VALUE ) {
			if ( !__xrtTlsServerViewEqual(
				LeftPsk.Identity, RightPsk.Identity
			) || (LeftPsk.Binder.Size != RightPsk.Binder.Size) ) {
				return false;
			}
		}
	} while ( LeftResult == XTLS_ITEM_VALUE );
	return LeftResult == XTLS_ITEM_DONE;
}

#endif



/* 校验第二个 ClientHello 只包含 RFC 8446 允许的 HRR 后变化。 */
static bool __xrtTlsServerRetryClientHello(
	const xtlsserverstate* pState,
	const xtlsclienthello* pSecond
)
{
	xtlshandshake Message;
	xtlsclienthello First;
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;

	if ( (pState->RetryClientHello == NULL) ||
		(xrtTlsHandshakeParse(
			(xbytesview) {
				pState->RetryClientHello,
				pState->RetryClientHelloSize
			}, &Message, NULL
		) != XTLS_OK) ||
		(Message.Type != XTLS_HANDSHAKE_CLIENT_HELLO) ||
		!xrtTlsClientHelloParse(Message.Body, &First) ) {
		return __xrtTlsServerError(
			XERR_STATE, XTLS_ERROR_HANDSHAKE,
			"select-server-flight",
			"TLS server retry ClientHello state is unavailable"
		);
	}
	if ( (First.LegacyVersion != pSecond->LegacyVersion) ||
		!__xrtTlsServerViewEqual(First.Random, pSecond->Random) ||
		!__xrtTlsServerViewEqual(First.SessionId, pSecond->SessionId) ||
		!__xrtTlsServerViewEqual(
			First.CipherSuites.Data, pSecond->CipherSuites.Data
		) || !__xrtTlsServerViewEqual(
			First.CompressionMethods, pSecond->CompressionMethods
		) ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"select-server-flight",
			"TLS second ClientHello changed a fixed field"
		);
	}
	if ( !xrtTlsExtensionsInit(&Cursor, First.Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		xtlsextension Current;
		xtlsitemresult Found;

		if ( (Extension.Type == XTLS_EXTENSION_KEY_SHARE) ||
			(Extension.Type == XTLS_EXTENSION_EARLY_DATA) ) {
			continue;
		}
		if ( Extension.Type == XTLS_EXTENSION_COOKIE ) {
			return __xrtTlsServerError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"select-server-flight",
				"TLS first ClientHello unexpectedly contains a cookie"
			);
		}
		Found = xrtTlsExtensionsFind(
			pSecond->Extensions, Extension.Type, &Current
		);
		if ( Found != XTLS_ITEM_VALUE ) {
			return __xrtTlsServerError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"select-server-flight",
				"TLS second ClientHello removed a stable extension"
			);
		}
		#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
			if ( Extension.Type == XTLS_EXTENSION_PRE_SHARED_KEY ) {
				if ( !__xrtTlsServerRetryPsksEqual(
					Extension.Data, Current.Data
				) ) {
					return __xrtTlsServerError(
						XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
						"select-server-flight",
						"TLS second ClientHello changed its PSK identities"
					);
				}
				continue;
			}
		#endif
		if ( !__xrtTlsServerViewEqual(
			Extension.Data, Current.Data
		) ) {
			return __xrtTlsServerError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"select-server-flight",
				"TLS second ClientHello changed a stable extension value"
			);
		}
	}
	if ( Result != XTLS_ITEM_DONE ) {
		return false;
	}
	if ( !xrtTlsExtensionsInit(&Cursor, pSecond->Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == XTLS_EXTENSION_KEY_SHARE ) {
			xtlskeysharecursor Shares;
			xtlskeyshare Share;

			if ( !xrtTlsClientKeyShares(Extension.Data, &Shares) ||
				(xrtTlsKeySharesRead(
					&Shares, &Share
				) != XTLS_ITEM_VALUE) ||
				(Share.Group != pState->RetryGroup) ||
				(xrtTlsKeySharesRead(
					&Shares, &Share
				) != XTLS_ITEM_DONE) ) {
				return __xrtTlsServerError(
					XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
					"select-server-flight",
					"TLS second ClientHello has an invalid retry key share"
				);
			}
			continue;
		}
		if ( (Extension.Type == XTLS_EXTENSION_COOKIE) ||
			(Extension.Type == XTLS_EXTENSION_EARLY_DATA) ) {
			return __xrtTlsServerError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"select-server-flight",
				"TLS second ClientHello added a forbidden extension"
			);
		}
		{
			xtlsextension Original;

			if ( xrtTlsExtensionsFind(
				First.Extensions, Extension.Type, &Original
			) != XTLS_ITEM_VALUE ) {
				return __xrtTlsServerError(
					XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
					"select-server-flight",
					"TLS second ClientHello added an extension"
				);
			}
		}
	}
	return Result == XTLS_ITEM_DONE;
}



/* 严格提取 ClientHello 并完成所有服务端策略选择。 */
static bool __xrtTlsServerSelect(
	const xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage,
	xtlsserverselection* pSelection
)
{
	const xtlspolicy* pPolicy = xrtTlsContextPolicy(pSession->Context);
	xtlsextension SniExtension;
	xtlsextension AlpnExtension;
	xtlsids Signatures;
	xtlsids Groups;
	xtlskeyshareselection KeyShare;
	xtlsserverrequest Request;
	xtlsserverchoice Choice;
	xtlsitemresult Result;
	xtlsversion Version;
	xbytesview SignatureData;
	xbytesview GroupData;
	xbytesview KeyShareData;
	xbytesview ExtendedMasterSecret;
	xtlsidentitytype IdentityType;

	memset(pSelection, 0, sizeof(*pSelection));
	pSelection->Protocol = XTLS_SERVER_PROTOCOL_NONE;
	if ( (pPolicy == NULL) || !xrtTlsClientHelloParse(
		pMessage->Body, &pSelection->Hello
	) ) {
		return false;
	}
	if ( !__xrtTlsServerFallback(
		pPolicy, &pSelection->Hello
	) || !__xrtTlsServerEarlyData(&pSelection->Hello) ) {
		return false;
	}
	if ( pState->RetrySeen && !__xrtTlsServerRetryClientHello(
		pState, &pSelection->Hello
	) ) {
		return false;
	}
	Result = xrtTlsClientVersionSelect(
		&pSelection->Hello,
		pPolicy->Versions, pPolicy->VersionCount, &Version
	);
	if ( (Result != XTLS_ITEM_VALUE) ||
		((Version != XTLS_VERSION_12) &&
		 (Version != XTLS_VERSION_13)) ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_VERSION,
			"select-server-flight",
			"TLS client and server have no supported protocol version"
		);
	}
	pSelection->Version = Version;

	/* SNI 和 ALPN 可缺失，但存在时必须通过各自的严格解析器。 */
	Result = xrtTlsExtensionsFind(
		pSelection->Hello.Extensions,
		XTLS_EXTENSION_SERVER_NAME, &SniExtension
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( Result == XTLS_ITEM_VALUE ) {
		Result = xrtTlsHostName(
			SniExtension.Data, &pSelection->ServerName
		);
		if ( Result == XTLS_ITEM_ERROR ) {
			return false;
		}
	}
	Result = xrtTlsExtensionsFind(
		pSelection->Hello.Extensions,
		XTLS_EXTENSION_ALPN, &AlpnExtension
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( Result == XTLS_ITEM_VALUE ) {
		xtlsprotocolcursor Cursor;

		if ( !xrtTlsProtocols(AlpnExtension.Data, &Cursor) ) {
			return false;
		}
		pSelection->Protocols = AlpnExtension.Data;
	}

	/* 先提供确定性的静态默认值，再让真实扩展点执行动态路由。 */
	Choice.Identity = pState->Identity;
	Choice.Protocol = __xrtTlsServerProtocolDefault(
		pState, pSelection->Protocols
	);
	Choice.Cookie = 0;
	Request.ServerName = pSelection->ServerName;
	Request.Protocols = pSelection->Protocols;
	if ( (pState->Select != NULL) && !pState->Select(
		pState->SelectContext, &Request, &Choice
	) ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_IDENTITY,
			"select-server-flight",
			"TLS server identity selector rejected the ClientHello"
		);
	}
	if ( Choice.Identity == NULL ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_IDENTITY,
			"select-server-flight",
			"TLS server did not select an identity"
		);
	}
	if ( !__xrtTlsServerProtocolChoice(
		pState, pSelection->Protocols, Choice.Protocol
	) ) {
		return false;
	}
	pSelection->Identity = xrtTlsIdentityRetain(Choice.Identity);
	if ( pSelection->Identity == NULL ) {
		return __xrtTlsServerCause(
			"select-server-flight",
			"TLS server could not retain the selected identity"
		);
	}
	pSelection->Cookie = Choice.Cookie;
	pSelection->Protocol = Choice.Protocol;
	IdentityType = xrtTlsIdentityType(pSelection->Identity);
	Result = xrtTlsCipherSelect(
		Version, &pSelection->Hello.CipherSuites,
		IdentityType, pPolicy->Ciphers, pPolicy->CipherCount,
		&pSelection->Cipher
	);
	if ( Result != XTLS_ITEM_VALUE ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_CIPHER,
			"select-server-flight",
			"TLS client and server have no usable cipher for the selected version"
		);
	}
	if ( pState->RetrySeen &&
		((Version != XTLS_VERSION_13) ||
		 (pSelection->Cipher != pState->RetryCipher)) ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_NEGOTIATION,
			"select-server-flight",
			"TLS second ClientHello changed the retry negotiation"
		);
	}
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		if ( (Version == XTLS_VERSION_13) &&
			!__xrtTlsServerResumeSelect(
			pMessage, &pSelection->Hello, pState,
			pSelection->ServerName, pSelection->Protocols,
			pSelection->Cipher, pSelection->Protocol,
			&pSelection->Resume
		) ) {
			return false;
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		if ( (Version == XTLS_VERSION_12) ||
			!pSelection->Resume.Resumed ) {
	#endif
	if ( !__xrtTlsServerExtension(
		pSelection->Hello.Extensions,
		XTLS_EXTENSION_SIGNATURE_ALGORITHMS,
		&SignatureData, true
	) || !xrtTlsSignatures(SignatureData, &Signatures) ||
		!__xrtTlsServerSignature(
			pPolicy, pSelection->Identity,
			&Signatures, Version, &pSelection->Signature
		) ) {
		return false;
	}
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		}
	#endif
	if ( !__xrtTlsServerExtension(
		pSelection->Hello.Extensions,
		XTLS_EXTENSION_SUPPORTED_GROUPS, &GroupData, true
	) || !xrtTlsGroups(GroupData, &Groups) ) {
		return false;
	}
	if ( Version == XTLS_VERSION_12 ) {
		if ( !__xrtTlsServerExtension(
			pSelection->Hello.Extensions,
			XTLS_EXTENSION_EXTENDED_MASTER_SECRET,
			&ExtendedMasterSecret, true
		) || (ExtendedMasterSecret.Size != 0) ) {
			return __xrtTlsServerError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"select-server-flight",
				"TLS 1.2 requires an empty extended_master_secret extension"
			);
		}
		for ( size_t i = 0; i < pPolicy->GroupCount; i++ ) {
			uint16 iGroup = pPolicy->Groups[i];

			if ( xrtTlsGroupAvailable(iGroup) &&
				xrtTlsIdsContain(&Groups, iGroup) ) {
				pSelection->Group = iGroup;
				return true;
			}
		}
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
			"select-server-flight",
			"TLS client and server have no common TLS 1.2 key exchange group"
		);
	}
	if ( !__xrtTlsServerExtension(
		pSelection->Hello.Extensions,
		XTLS_EXTENSION_KEY_SHARE, &KeyShareData, true
	) ) {
		return false;
	}
	Result = xrtTlsKeyShareSelect(
		&Groups, KeyShareData,
		pPolicy->Groups, pPolicy->GroupCount,
		pPolicy->KeySharePolicy, &KeyShare
	);
	if ( Result != XTLS_ITEM_VALUE ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
			"select-server-flight",
			"TLS client and server have no common key share"
		);
	}
	if ( KeyShare.Retry ) {
		if ( pState->RetrySeen ) {
			return __xrtTlsServerError(
				XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
				"select-server-flight",
				"TLS second ClientHello requested another retry"
			);
		}
		pSelection->Group = KeyShare.Share.Group;
		pSelection->Retry = true;
		return true;
	}
	pSelection->Share = KeyShare.Share;
	pSelection->Group = KeyShare.Share.Group;
	if ( pState->RetrySeen &&
		(pSelection->Group != pState->RetryGroup) ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
			"select-server-flight",
			"TLS second ClientHello used the wrong retry group"
		);
	}
	return true;
}



/* 清理尚未提交的选择结果。 */
static void __xrtTlsServerSelectionClear(
	xtlsserverselection* pSelection
)
{
	if ( pSelection == NULL ) {
		return;
	}
	xrtTlsIdentityRelease((xtlsidentity*)pSelection->Identity);
	memset(pSelection, 0, sizeof(*pSelection));
}



/* 清除未提交航次的全部密码材料和动态存储。 */
static void __xrtTlsServerFlightClear(xtlsserverflight* pFlight)
{
	if ( pFlight == NULL ) {
		return;
	}
	__xrtTlsRecordKeyClear(&pFlight->ReadKey);
	__xrtTlsRecordKeyClear(&pFlight->WriteKey);
	__xrtTlsTranscriptClear(&pFlight->Transcript);
	xrtTlsIdentityRelease((xtlsidentity*)pFlight->Identity);
	if ( pFlight->ServerName != NULL ) {
		xrtSecureZero(
			pFlight->ServerName, pFlight->ServerNameSize
		);
		xrtFree(pFlight->ServerName);
	}
	if ( pFlight->Output != NULL ) {
		xrtSecureZero(pFlight->Output, pFlight->OutputSize);
		xrtFree(pFlight->Output);
	}
	xrtSecureZero(pFlight, sizeof(*pFlight));
}



/* 验证每条输出消息都位于会话单消息硬上限内。 */
static bool __xrtTlsServerHandshakeLimit(
	const xtlssession* pSession,
	size_t iMessage,
	cstr sMessage
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(pSession->Context);

	if ( (pLimits != NULL) &&
		(iMessage >= XTLS_HANDSHAKE_HEADER_SIZE) &&
		((iMessage - XTLS_HANDSHAKE_HEADER_SIZE) <=
		 pLimits->HandshakeLimit) ) {
		return true;
	}
	return __xrtTlsServerError(
		XERR_RANGE, XTLS_ERROR_LIMIT,
		"build-server-flight", sMessage
	);
}



/* 编码正文已经位于头部之后的一条握手消息。 */
static bool __xrtTlsServerHandshake(
	xtlshandshaketype Type,
	bytes pMessage,
	size_t iMessage
)
{
	if ( (pMessage == NULL) ||
		(iMessage < XTLS_HANDSHAKE_HEADER_SIZE) ) {
		return __xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"build-server-flight",
			"TLS server handshake output layout is invalid"
		);
	}
	return xrtTlsHandshakeEncode(
		Type,
		(xbytesview) {
			pMessage + XTLS_HANDSHAKE_HEADER_SIZE,
			iMessage - XTLS_HANDSHAKE_HEADER_SIZE
		},
		pMessage, iMessage
	);
}



/* 复制可选 SNI，并为失败原子的首航保留独立生命周期。 */
static bool __xrtTlsServerNameCopy(
	const xtlsserverselection* pSelection,
	xtlsserverflight* pFlight
)
{
	if ( pSelection->ServerName.Size == 0 ) {
		return true;
	}
	pFlight->ServerName = (bytes)xrtMalloc(
		pSelection->ServerName.Size + 1u
	);
	if ( pFlight->ServerName == NULL ) {
		return __xrtTlsServerCause(
			"build-server-flight",
			"TLS server name allocation failed"
		);
	}
	memcpy(
		pFlight->ServerName,
		pSelection->ServerName.Data,
		pSelection->ServerName.Size
	);
	pFlight->ServerName[pSelection->ServerName.Size] = 0;
	pFlight->ServerNameSize = pSelection->ServerName.Size;
	return true;
}



/* 构建并提交一次无 cookie 的 HelloRetryRequest。 */
static bool __xrtTlsServerRetryFlight(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pClientHello,
	const xtlsserverselection* pSelection
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(
		pSelection->Cipher
	);
	xtlstranscript Transcript;
	xtlswriter Writer;
	xtlsserverhello Hello;
	bytes pExtensions = NULL;
	bytes pHello = NULL;
	bytes pSaved = NULL;
	xbytesview Encoded = {0};
	size_t iBodySize;
	size_t iMessageSize;
	bool bResult = false;

	memset(&Transcript, 0, sizeof(Transcript));
	memset(&Hello, 0, sizeof(Hello));
	if ( pState->RetrySeen || (pCipher == NULL) ||
		(pCipher->Version != XTLS_VERSION_13) ||
		!xrtTlsGroupAvailable(pSelection->Group) ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"build-hello-retry-request",
			"TLS server retry selection is invalid"
		);
	}
	pExtensions = (bytes)xrtTempAlloc(
		&pState->HandshakeArena, 12u
	);
	if ( pExtensions == NULL ) {
		(void)__xrtTlsServerCause(
			"build-hello-retry-request",
			"TLS HelloRetryRequest extension allocation failed"
		);
		goto cleanup;
	}
	if ( !xrtTlsWriterInit(&Writer, pExtensions, 12u) ||
		!xrtTlsWriterServerVersion(&Writer, XTLS_VERSION_13) ||
		!xrtTlsWriterRetryGroup(&Writer, pSelection->Group) ||
		(Writer.Size != 12u) ) {
		goto cleanup;
	}
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = __xrtTlsHelloRetryRandom();
	Hello.SessionId = pSelection->Hello.SessionId;
	Hello.CipherSuite = (uint16)pSelection->Cipher;
	Hello.Extensions = xrtTlsWriterData(&Writer);
	Hello.Retry = true;
	iBodySize = xrtTlsServerHelloSize(&Hello);
	iMessageSize = xrtTlsHandshakeSize(iBodySize);
	if ( (iBodySize == 0) || (iMessageSize == 0) ||
		!__xrtTlsServerHandshakeLimit(
			pSession, iMessageSize,
			"TLS HelloRetryRequest exceeds the configured handshake limit"
		) ) {
		goto cleanup;
	}
	pHello = (bytes)xrtTempAlloc(
		&pState->HandshakeArena, iMessageSize
	);
	if ( pHello == NULL ) {
		(void)__xrtTlsServerCause(
			"build-hello-retry-request",
			"TLS HelloRetryRequest allocation failed"
		);
		goto cleanup;
	}
	if ( !xrtTlsServerHelloEncode(
		&Hello, pHello + XTLS_HANDSHAKE_HEADER_SIZE, iBodySize
	) || !__xrtTlsServerHandshake(
		XTLS_HANDSHAKE_SERVER_HELLO, pHello, iMessageSize
	) ) {
		goto cleanup;
	}
	Encoded.Data = pClientHello->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pClientHello->EncodedSize;
	pSaved = (bytes)xrtMalloc(Encoded.Size);
	if ( pSaved == NULL ) {
		(void)__xrtTlsServerCause(
			"build-hello-retry-request",
			"TLS first ClientHello retention failed"
		);
		goto cleanup;
	}
	memcpy(pSaved, Encoded.Data, Encoded.Size);
	if ( !__xrtTlsTranscriptInit(
		&Transcript, __xrtTlsHash(pCipher->Hash)
	) || !__xrtTlsTranscriptUpdate(&Transcript, Encoded) ||
		!__xrtTlsTranscriptRetry(&Transcript) ||
		!__xrtTlsTranscriptUpdate(
			&Transcript, (xbytesview) { pHello, iMessageSize }
		) || (__xrtTlsSessionRecordPlain(
			pSession, XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
			(xbytesview) { pHello, iMessageSize }
		) != XTLS_OK) ) {
		goto cleanup;
	}
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Transcript;
	memset(&Transcript, 0, sizeof(Transcript));
	pState->RetryClientHello = pSaved;
	pState->RetryClientHelloSize = Encoded.Size;
	pSaved = NULL;
	pState->RetryCipher = pSelection->Cipher;
	pState->RetryGroup = pSelection->Group;
	pState->RetrySeen = true;
	pSession->Wait = XTLS_WAIT_INPUT | XTLS_WAIT_OUTPUT;
	bResult = true;

cleanup:
	__xrtTlsTranscriptClear(&Transcript);
	if ( pSaved != NULL ) {
		xrtSecureZero(pSaved, Encoded.Size);
		xrtFree(pSaved);
	}
	return bResult;
}



/* 构建普通 ServerHello 并派生双向握手 epoch。 */
static bool __xrtTlsServerHelloFlight(
	const xtlssession* pSession,
	const xtlsserverstate* pState,
	const xtlsserverselection* pSelection,
	const xtlshandshake* pClientHello,
	xtemparena* pArena,
	xtlsserverflight* pFlight,
	bytes* ppServerHello,
	size_t* pServerHelloSize,
	xtlsrecordkey* pHandshakeWrite,
	uint8* pHandshakeSecret,
	uint8* pServerHandshake
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(
		pSelection->Cipher
	);
	const xtlsgroupinfo* pGroup = xrtTlsGroupInfo(pSelection->Group);
	bytes pKeyBlock = NULL;
	bytes pPrivate;
	bytes pPublic;
	bytes pShared;
	bytes pExtensions = NULL;
	bytes pHello = NULL;
	xtlswriter Writer;
	xtlskeyshare Share;
	xtlsserverhello Hello;
	xcryptohash Hash;
	xbytesview Psk = { NULL, 0 };
	uint8 Random[XTLS_RANDOM_SIZE];
	uint8 TranscriptHash[XTLS_SERVER_SECRET_MAX_SIZE];
	size_t iKeyBytes = 0;
	size_t iExtensionSize;
	size_t iBodySize;
	size_t iMessageSize;
	bool bResult = false;

	memset(Random, 0, sizeof(Random));
	memset(TranscriptHash, 0, sizeof(TranscriptHash));
	if ( (pCipher == NULL) || (pGroup == NULL) ||
		(pCipher->HashSize > XTLS_SERVER_SECRET_MAX_SIZE) ||
		!__xrtTlsServerSize(&iKeyBytes, pGroup->PrivateSize) ||
		!__xrtTlsServerSize(&iKeyBytes, pGroup->PublicSize) ||
		!__xrtTlsServerSize(&iKeyBytes, pGroup->SharedSize) ) {
		goto cleanup;
	}
	pKeyBlock = (bytes)xrtTempAlloc(pArena, iKeyBytes);
	if ( pKeyBlock == NULL ) {
		(void)__xrtTlsServerCause(
			"build-server-flight",
			"TLS server key-share workspace allocation failed"
		);
		goto cleanup;
	}
	pPrivate = pKeyBlock;
	pPublic = pPrivate + pGroup->PrivateSize;
	pShared = pPublic + pGroup->PublicSize;
	if ( !xrtSecureRandom(Random, sizeof(Random)) ||
		!xrtTlsKeyShareGenerate(
			pSelection->Group,
			pPrivate, pGroup->PrivateSize,
			pPublic, pGroup->PublicSize
		) || !xrtTlsKeyShareDerive(
			pSelection->Group,
			(xbytesview) { pPrivate, pGroup->PrivateSize },
			pSelection->Share.Key,
			pShared, pGroup->SharedSize
		) ) {
		(void)__xrtTlsServerCause(
			"build-server-flight",
			"TLS server random or key-share operation failed"
		);
		goto cleanup;
	}
	iExtensionSize = 14u + pGroup->PublicSize;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		if ( pSelection->Resume.Resumed ) {
			iExtensionSize += XTLS_EXTENSION_HEADER_SIZE + 2u;
			Psk = (xbytesview) {
				pSelection->Resume.Secret, pCipher->HashSize
			};
		}
	#endif
	pExtensions = (bytes)xrtTempAlloc(pArena, iExtensionSize);
	if ( pExtensions == NULL ) {
		(void)__xrtTlsServerCause(
			"build-server-flight",
			"TLS ServerHello extension allocation failed"
		);
		goto cleanup;
	}
	Share.Group = pSelection->Group;
	Share.Key = (xbytesview) { pPublic, pGroup->PublicSize };
	if ( !xrtTlsWriterInit(
		&Writer, pExtensions, iExtensionSize
	) || !xrtTlsWriterServerVersion(
		&Writer, XTLS_VERSION_13
	) || !xrtTlsWriterServerKeyShare(&Writer, &Share) ||
		#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
			(pSelection->Resume.Resumed &&
			 !xrtTlsWriterServerPsk(
				&Writer, pSelection->Resume.Selected
			 )) ||
		#endif
		(Writer.Size != iExtensionSize) ) {
		goto cleanup;
	}
	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = (xbytesview) { Random, sizeof(Random) };
	Hello.SessionId = pSelection->Hello.SessionId;
	Hello.CipherSuite = (uint16)pSelection->Cipher;
	Hello.Extensions = xrtTlsWriterData(&Writer);
	iBodySize = xrtTlsServerHelloSize(&Hello);
	iMessageSize = xrtTlsHandshakeSize(iBodySize);
	if ( (iBodySize == 0) || (iMessageSize == 0) ||
		!__xrtTlsServerHandshakeLimit(
			pSession, iMessageSize,
			"TLS ServerHello exceeds the configured handshake limit"
		) ) {
		goto cleanup;
	}
	pHello = (bytes)xrtTempAlloc(pArena, iMessageSize);
	if ( pHello == NULL ) {
		(void)__xrtTlsServerCause(
			"build-server-flight",
			"TLS ServerHello allocation failed"
		);
		goto cleanup;
	}
	if ( !xrtTlsServerHelloEncode(
		&Hello, pHello + XTLS_HANDSHAKE_HEADER_SIZE, iBodySize
	) || !__xrtTlsServerHandshake(
		XTLS_HANDSHAKE_SERVER_HELLO, pHello, iMessageSize
	) ) {
		goto cleanup;
	}
	Hash = __xrtTlsHash(pCipher->Hash);
	if ( pState->RetrySeen ) {
		if ( !pState->Transcript.Ready ||
			(pState->Transcript.Hash != Hash) ) {
			(void)__xrtTlsServerError(
				XERR_STATE, XTLS_ERROR_TRANSCRIPT,
				"build-server-flight",
				"TLS server retry transcript is unavailable"
			);
			goto cleanup;
		}
		pFlight->Transcript = pState->Transcript;
	} else if ( !__xrtTlsTranscriptInit(
		&pFlight->Transcript, Hash
	) ) {
		goto cleanup;
	}
	if ( !__xrtTlsTranscriptUpdate(
			&pFlight->Transcript,
			(xbytesview) {
				pClientHello->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE,
				pClientHello->EncodedSize
			}
		) || !__xrtTlsTranscriptUpdate(
			&pFlight->Transcript,
			(xbytesview) { pHello, iMessageSize }
		) || !__xrtTlsTranscriptDigest(
			&pFlight->Transcript,
			TranscriptHash, pCipher->HashSize
		) || !__xrtTls13HandshakeSchedule(
			Hash, Psk,
			(xbytesview) { pShared, pGroup->SharedSize },
			(xbytesview) { TranscriptHash, pCipher->HashSize },
			pHandshakeSecret,
			pFlight->ClientHandshake,
			pServerHandshake,
			pCipher->HashSize
		) || !__xrtTls13RecordKey(
			pSelection->Cipher,
			(xbytesview) {
				pFlight->ClientHandshake, pCipher->HashSize
			}, &pFlight->ReadKey
		) || !__xrtTls13RecordKey(
			pSelection->Cipher,
			(xbytesview) { pServerHandshake, pCipher->HashSize },
			pHandshakeWrite
		) ) {
		goto cleanup;
	}
	pFlight->HashSize = pCipher->HashSize;
	pFlight->Cipher = pSelection->Cipher;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		pFlight->Resumed = pSelection->Resume.Resumed;
	#endif
	*ppServerHello = pHello;
	*pServerHelloSize = iMessageSize;
	bResult = true;

cleanup:
	xrtSecureZero(TranscriptHash, sizeof(TranscriptHash));
	xrtSecureZero(Random, sizeof(Random));
	return bResult;
}



/* 构建 EE、完整证书链、CertificateVerify 与 Finished 握手流。 */
static bool __xrtTlsServerAuthentication(
	const xtlssession* pSession,
	const xtlsserverstate* pState,
	const xtlsserverselection* pSelection,
	xtemparena* pArena,
	xtlsserverflight* pFlight,
	xbytesview HandshakeSecret,
	xbytesview ServerHandshake,
	bytes* ppStream,
	size_t* pStreamSize
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(
		pSelection->Cipher
	);
	xcryptohash Hash;
	xtlswriter Writer;
	xtlscertificateentry* pEntries = NULL;
	xtlscertificateverify Verify;
	bytes pExtensions = NULL;
	bytes pPrefix = NULL;
	bytes pSignature = NULL;
	bytes pStream = NULL;
	xbytesview Empty = { NULL, 0 };
	xbytesview Protocol;
	uint8 TranscriptHash[XTLS_SERVER_SECRET_MAX_SIZE];
	uint8 VerifyContent[
		64u + sizeof("TLS 1.3, server CertificateVerify")
		+ XTLS_SERVER_SECRET_MAX_SIZE
	];
	uint8 FinishedKey[XTLS_SERVER_SECRET_MAX_SIZE];
	uint8 Finished[XTLS_SERVER_SECRET_MAX_SIZE];
	uint8 Master[XTLS_SERVER_SECRET_MAX_SIZE];
	size_t iExtensionSize = 0;
	size_t iEeBody = 0;
	size_t iEeMessage = 0;
	size_t iCertificateCount = 0;
	size_t iCertificateBody = 0;
	size_t iCertificateMessage = 0;
	size_t iPrefixSize = 0;
	size_t iVerifyContent = 0;
	size_t iSignature = 0;
	size_t iVerifyBody = 0;
	size_t iVerifyMessage = 0;
	size_t iFinishedMessage = 0;
	size_t iStreamSize = 0;
	size_t iOffset = 0;
	bool bCertificate = true;
	bool bResult = false;

	memset(&Verify, 0, sizeof(Verify));
	memset(TranscriptHash, 0, sizeof(TranscriptHash));
	memset(VerifyContent, 0, sizeof(VerifyContent));
	memset(FinishedKey, 0, sizeof(FinishedKey));
	memset(Finished, 0, sizeof(Finished));
	memset(Master, 0, sizeof(Master));
	if ( pCipher == NULL ) {
		(void)__xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_CIPHER,
			"build-server-flight",
			"TLS server selected cipher metadata is unavailable"
		);
		goto cleanup;
	}
	Hash = __xrtTlsHash(pCipher->Hash);
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		bCertificate = !pSelection->Resume.Resumed;
	#endif
	if ( pSelection->ServerName.Size != 0 ) {
		iExtensionSize += XTLS_EXTENSION_HEADER_SIZE;
	}
	if ( pSelection->Protocol != XTLS_SERVER_PROTOCOL_NONE ) {
		Protocol = pState->Protocols[pSelection->Protocol];
		iExtensionSize += XTLS_EXTENSION_HEADER_SIZE + 3u + Protocol.Size;
	} else {
		memset(&Protocol, 0, sizeof(Protocol));
	}
	if ( iExtensionSize != 0 ) {
		pExtensions = (bytes)xrtTempAlloc(pArena, iExtensionSize);
		if ( pExtensions == NULL ) {
			(void)__xrtTlsServerCause(
				"build-server-flight",
				"TLS EncryptedExtensions allocation failed"
			);
			goto cleanup;
		}
	}
	if ( !xrtTlsWriterInit(
		&Writer, pExtensions, iExtensionSize
	) || ((pSelection->ServerName.Size != 0) &&
		 !xrtTlsWriterExtension(
			&Writer, XTLS_EXTENSION_SERVER_NAME, Empty
		 )) || ((Protocol.Size != 0) &&
		 !xrtTlsWriterProtocols(&Writer, &Protocol, 1u)) ||
		(Writer.Size != iExtensionSize) ) {
		goto cleanup;
	}
	iEeBody = xrtTlsEncryptedExtensionsSize(xrtTlsWriterData(&Writer));
	iEeMessage = xrtTlsHandshakeSize(iEeBody);
	if ( (iEeBody == 0) || (iEeMessage == 0) ||
		!__xrtTlsServerHandshakeLimit(
			pSession, iEeMessage,
			"TLS EncryptedExtensions exceeds the configured handshake limit"
		) ) {
		goto cleanup;
	}
	iPrefixSize = iEeMessage;
	if ( bCertificate ) {
		iCertificateCount = xrtTlsIdentityCertificateCount(
			pSelection->Identity
		);
		if ( (iCertificateCount == 0) ||
			(iCertificateCount > (SIZE_MAX / sizeof(*pEntries))) ) {
			goto cleanup;
		}
		pEntries = (xtlscertificateentry*)xrtTempAlloc(
			pArena,
			iCertificateCount * sizeof(*pEntries)
		);
		if ( pEntries == NULL ) {
			(void)__xrtTlsServerCause(
				"build-server-flight",
				"TLS Certificate entry allocation failed"
			);
			goto cleanup;
		}
		memset(
			pEntries,
			0,
			iCertificateCount * sizeof(*pEntries)
		);
		for ( size_t i = 0; i < iCertificateCount; i++ ) {
			if ( !xrtTlsIdentityCertificate(
				pSelection->Identity, i, &pEntries[i].Data
			) ) {
				goto cleanup;
			}
		}
		iCertificateBody = xrtTlsCertificateSize(
			XTLS_VERSION_13, Empty, pEntries, iCertificateCount
		);
		iCertificateMessage = xrtTlsHandshakeSize(iCertificateBody);
		if ( (iCertificateBody == 0) ||
			(iCertificateMessage == 0) ||
			!__xrtTlsServerHandshakeLimit(
				pSession, iCertificateMessage,
				"TLS Certificate exceeds the configured handshake limit"
			) || !__xrtTlsServerSize(
				&iPrefixSize, iCertificateMessage
			) ) {
			goto cleanup;
		}
	}
	pPrefix = (bytes)xrtTempAlloc(pArena, iPrefixSize);
	if ( pPrefix == NULL ) {
		(void)__xrtTlsServerCause(
			"build-server-flight",
			"TLS authentication prefix allocation failed"
		);
		goto cleanup;
	}
	if ( !xrtTlsEncryptedExtensionsEncode(
		xrtTlsWriterData(&Writer),
		pPrefix + XTLS_HANDSHAKE_HEADER_SIZE, iEeBody
	) || !__xrtTlsServerHandshake(
		XTLS_HANDSHAKE_ENCRYPTED_EXTENSIONS,
		pPrefix, iEeMessage
	) || !__xrtTlsTranscriptUpdate(
		&pFlight->Transcript,
		(xbytesview) { pPrefix, iEeMessage }
	) ) {
		goto cleanup;
	}
	if ( bCertificate ) {
		if ( !xrtTlsCertificateEncode(
			XTLS_VERSION_13, Empty, pEntries, iCertificateCount,
			pPrefix + iEeMessage + XTLS_HANDSHAKE_HEADER_SIZE,
			iCertificateBody
		) || !__xrtTlsServerHandshake(
			XTLS_HANDSHAKE_CERTIFICATE,
			pPrefix + iEeMessage, iCertificateMessage
		) || !__xrtTlsTranscriptUpdate(
			&pFlight->Transcript,
			(xbytesview) {
				pPrefix + iEeMessage, iCertificateMessage
			}
		) || !__xrtTlsTranscriptDigest(
			&pFlight->Transcript,
			TranscriptHash, pFlight->HashSize
		) ) {
			goto cleanup;
		}
		iVerifyContent = xrtTls13CertificateVerifyContentSize(
			XTLS_SERVER, pFlight->HashSize
		);
		if ( (iVerifyContent == 0) ||
			(iVerifyContent > sizeof(VerifyContent)) ||
			!xrtTls13CertificateVerifyContentEncode(
				XTLS_SERVER,
				(xbytesview) {
					TranscriptHash, pFlight->HashSize
				}, VerifyContent, sizeof(VerifyContent)
			) || !xrtTlsIdentitySign(
				pSelection->Identity, XTLS_VERSION_13,
				pSelection->Signature,
				(xbytesview) { VerifyContent, iVerifyContent },
				NULL, 0, &iSignature
			) ) {
			goto cleanup;
		}
		pSignature = (bytes)xrtTempAlloc(pArena, iSignature);
		if ( pSignature == NULL ) {
			(void)__xrtTlsServerCause(
				"build-server-flight",
				"TLS CertificateVerify signature allocation failed"
			);
			goto cleanup;
		}
		if ( !xrtTlsIdentitySign(
			pSelection->Identity, XTLS_VERSION_13,
			pSelection->Signature,
			(xbytesview) { VerifyContent, iVerifyContent },
			pSignature, iSignature, &iSignature
		) ) {
			goto cleanup;
		}
		Verify.Scheme = (uint16)pSelection->Signature;
		Verify.Signature = (xbytesview) {
			pSignature, iSignature
		};
		iVerifyBody = xrtTlsCertificateVerifySize(&Verify);
		iVerifyMessage = xrtTlsHandshakeSize(iVerifyBody);
		if ( (iVerifyBody == 0) || (iVerifyMessage == 0) ||
			!__xrtTlsServerHandshakeLimit(
				pSession, iVerifyMessage,
				"TLS CertificateVerify exceeds the configured handshake limit"
			) ) {
			goto cleanup;
		}
	}
	iFinishedMessage = xrtTlsHandshakeSize(pFlight->HashSize);
	if ( (iFinishedMessage == 0) || !__xrtTlsServerHandshakeLimit(
			pSession, iFinishedMessage,
			"TLS Finished exceeds the configured handshake limit"
		) ) {
		goto cleanup;
	}
	iStreamSize = iPrefixSize;
	if ( !__xrtTlsServerSize(&iStreamSize, iVerifyMessage) ||
		!__xrtTlsServerSize(&iStreamSize, iFinishedMessage) ) {
		goto cleanup;
	}
	pStream = (bytes)xrtTempAlloc(pArena, iStreamSize);
	if ( pStream == NULL ) {
		(void)__xrtTlsServerCause(
			"build-server-flight",
			"TLS authentication stream allocation failed"
		);
		goto cleanup;
	}
	memcpy(pStream, pPrefix, iPrefixSize);
	iOffset = iPrefixSize;
	if ( bCertificate ) {
		if ( !xrtTlsCertificateVerifyEncode(
			&Verify,
			pStream + iOffset + XTLS_HANDSHAKE_HEADER_SIZE,
			iVerifyBody
		) || !__xrtTlsServerHandshake(
			XTLS_HANDSHAKE_CERTIFICATE_VERIFY,
			pStream + iOffset, iVerifyMessage
		) || !__xrtTlsTranscriptUpdate(
			&pFlight->Transcript,
			(xbytesview) { pStream + iOffset, iVerifyMessage }
		) ) {
			goto cleanup;
		}
		iOffset += iVerifyMessage;
	}
	if ( !__xrtTlsTranscriptDigest(
		&pFlight->Transcript,
		TranscriptHash, pFlight->HashSize
	) || !__xrtTls13ExpandLabel(
		Hash, ServerHandshake, XRT_STR_LITERAL("finished"), Empty,
		FinishedKey, pFlight->HashSize
	) || !__xrtTls13Finished(
		Hash,
		(xbytesview) { FinishedKey, pFlight->HashSize },
		(xbytesview) { TranscriptHash, pFlight->HashSize },
		Finished, pFlight->HashSize
	) || !xrtTlsFinishedEncode(
		(xbytesview) { Finished, pFlight->HashSize },
		pStream + iOffset + XTLS_HANDSHAKE_HEADER_SIZE,
		pFlight->HashSize
	) || !__xrtTlsServerHandshake(
		XTLS_HANDSHAKE_FINISHED,
		pStream + iOffset, iFinishedMessage
	) || !__xrtTlsTranscriptUpdate(
		&pFlight->Transcript,
		(xbytesview) { pStream + iOffset, iFinishedMessage }
	) || !__xrtTlsTranscriptDigest(
		&pFlight->Transcript,
		TranscriptHash, pFlight->HashSize
	) || !__xrtTls13ApplicationSchedule(
		Hash, HandshakeSecret,
		(xbytesview) { TranscriptHash, pFlight->HashSize },
		Master,
		pFlight->ClientApplication,
		pFlight->ServerApplication,
		pFlight->HashSize
	) || !__xrtTls13RecordKey(
		pSelection->Cipher,
		(xbytesview) {
			pFlight->ServerApplication, pFlight->HashSize
		}, &pFlight->WriteKey
	) ) {
		goto cleanup;
	}
	pFlight->Signature = pSelection->Signature;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		memcpy(pFlight->Master, Master, pFlight->HashSize);
	#endif
	*ppStream = pStream;
	*pStreamSize = iStreamSize;
	bResult = true;

cleanup:
	xrtSecureZero(Master, sizeof(Master));
	xrtSecureZero(Finished, sizeof(Finished));
	xrtSecureZero(FinishedKey, sizeof(FinishedKey));
	xrtSecureZero(VerifyContent, sizeof(VerifyContent));
	xrtSecureZero(TranscriptHash, sizeof(TranscriptHash));
	return bResult;
}



/* 把 ServerHello 与受保护认证流编码成一次精确分配。 */
static bool __xrtTlsServerRecords(
	const xtlssession* pSession,
	xbytesview ServerHello,
	xbytesview Stream,
	xtlsrecordkey* pHandshakeWrite,
	xtlsserverflight* pFlight
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(pSession->Context);
	size_t iHelloRecord = xrtTlsRecordSize(ServerHello.Size);
	size_t iOutputSize = iHelloRecord;
	size_t iOffset = 0;
	size_t iWrite = 0;

	if ( (pLimits == NULL) || (iHelloRecord == 0) ||
		(ServerHello.Size > XTLS_RECORD_PLAINTEXT_MAX) ||
		((Stream.Data == NULL) && (Stream.Size != 0)) ) {
		return __xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"build-server-flight",
			"TLS server record layout is invalid"
		);
	}
	while ( iOffset < Stream.Size ) {
		size_t iChunk = Stream.Size - iOffset;
		size_t iRecord;

		if ( iChunk > XTLS_RECORD_PLAINTEXT_MAX ) {
			iChunk = XTLS_RECORD_PLAINTEXT_MAX;
		}
		iRecord = __xrtTlsRecordSealSize(
			pHandshakeWrite, iChunk, 0
		);
		if ( (iRecord == 0) ||
			!__xrtTlsServerSize(&iOutputSize, iRecord) ) {
			return false;
		}
		iOffset += iChunk;
	}
	if ( (xrtTlsSessionSendSize(pSession) != 0) ||
		(iOutputSize > pLimits->SendLimit) ) {
		return __xrtTlsServerError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			"build-server-flight",
			"TLS send limit cannot hold the mandatory server flight"
		);
	}
	pFlight->Output = (bytes)xrtMalloc(iOutputSize);
	if ( pFlight->Output == NULL ) {
		return __xrtTlsServerCause(
			"build-server-flight",
			"TLS server ciphertext flight allocation failed"
		);
	}
	pFlight->OutputSize = iOutputSize;
	if ( !xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12, ServerHello,
		pFlight->Output, iHelloRecord
	) ) {
		return false;
	}
	iOffset = 0;
	iWrite = iHelloRecord;
	while ( iOffset < Stream.Size ) {
		size_t iChunk = Stream.Size - iOffset;
		size_t iWritten = 0;

		if ( iChunk > XTLS_RECORD_PLAINTEXT_MAX ) {
			iChunk = XTLS_RECORD_PLAINTEXT_MAX;
		}
		if ( !__xrtTlsRecordSeal(
			pHandshakeWrite, XTLS_RECORD_HANDSHAKE,
			(xbytesview) { Stream.Data + iOffset, iChunk }, 0,
			pFlight->Output + iWrite, iOutputSize - iWrite,
			&iWritten
		) ) {
			return false;
		}
		iWrite += iWritten;
		iOffset += iChunk;
	}
	if ( iWrite != iOutputSize ) {
		return __xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"build-server-flight",
			"TLS server record encoder returned an inconsistent size"
		);
	}
	return true;
}



/* 成功接管密文后执行不再失败的首航状态提交。 */
static void __xrtTlsServerFlightCommit(
	xtlssession* pSession,
	xtlsserverstate* pState,
	xtlsserverflight* pFlight
)
{
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	pSession->ReadKey = pFlight->ReadKey;
	pSession->WriteKey = pFlight->WriteKey;
	xrtSecureZero(&pFlight->ReadKey, sizeof(pFlight->ReadKey));
	xrtSecureZero(&pFlight->WriteKey, sizeof(pFlight->WriteKey));

	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = pFlight->Transcript;
	memset(&pFlight->Transcript, 0, sizeof(pFlight->Transcript));
	memcpy(
		pState->ClientHandshakeTraffic,
		pFlight->ClientHandshake, pFlight->HashSize
	);
	memcpy(
		pState->ClientApplicationTraffic,
		pFlight->ClientApplication, pFlight->HashSize
	);
	memcpy(
		pState->ServerApplicationTraffic,
		pFlight->ServerApplication, pFlight->HashSize
	);
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		memcpy(
			pState->MasterSecret,
			pFlight->Master, pFlight->HashSize
		);
		pState->Resumed = pFlight->Resumed;
	#endif
	pState->HashSize = pFlight->HashSize;
	pState->Cookie = pFlight->Cookie;
	pState->Version = XTLS_VERSION_13;
	pState->Cipher = pFlight->Cipher;
	pState->Signature = pFlight->Signature;
	pState->Step = XTLS_SERVER_WAIT_CLIENT_FINISHED;
	if ( pState->RetryClientHello != NULL ) {
		xrtSecureZero(
			pState->RetryClientHello, pState->RetryClientHelloSize
		);
		xrtFree(pState->RetryClientHello);
		pState->RetryClientHello = NULL;
		pState->RetryClientHelloSize = 0;
	}
	pState->RetrySeen = false;
	pSession->Version = XTLS_VERSION_13;
	pSession->Cipher = pFlight->Cipher;

	xrtTlsIdentityRelease((xtlsidentity*)pState->Identity);
	pState->Identity = pFlight->Identity;
	pFlight->Identity = NULL;
	pState->ServerNameStorage = pFlight->ServerName;
	pState->ServerName.Data = pFlight->ServerName;
	pState->ServerName.Size = pFlight->ServerNameSize;
	pFlight->ServerName = NULL;
	if ( pFlight->Protocol != XTLS_SERVER_PROTOCOL_NONE ) {
		pSession->Protocol = pState->Protocols[pFlight->Protocol];
	}
	pState->Select = NULL;
	pState->SelectContext = NULL;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		pState->Resume = NULL;
		pState->ResumeContext = NULL;
	#endif
	pSession->Wait = XTLS_WAIT_INPUT | XTLS_WAIT_OUTPUT;
}



/* 处理 ClientHello，并在完整密文可接管后一次提交所有状态。 */
xtlsresult __xrtTlsServerFirstFlight(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage
)
{
	xtlsserverselection Selection;
	xtlsserverflight Flight;
	xtlsrecordkey HandshakeWrite;
	bytes pServerHello = NULL;
	bytes pStream = NULL;
	uint8 HandshakeSecret[XTLS_SERVER_SECRET_MAX_SIZE];
	uint8 ServerHandshake[XTLS_SERVER_SECRET_MAX_SIZE];
	size_t iServerHello = 0;
	size_t iStream = 0;
	xtlsresult Result = XTLS_ERROR;

	memset(&Selection, 0, sizeof(Selection));
	memset(&Flight, 0, sizeof(Flight));
	memset(&HandshakeWrite, 0, sizeof(HandshakeWrite));
	memset(HandshakeSecret, 0, sizeof(HandshakeSecret));
	memset(ServerHandshake, 0, sizeof(ServerHandshake));
	if ( (pMessage == NULL) ||
		(pMessage->Type != XTLS_HANDSHAKE_CLIENT_HELLO) ) {
		(void)__xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"build-server-flight",
			"TLS server expected ClientHello as the first message"
		);
		goto cleanup;
	}
	if ( !__xrtTlsServerSelect(
		pSession, pState, pMessage, &Selection
	) ) {
		goto cleanup;
	}
	if ( Selection.Version == XTLS_VERSION_12 ) {
		Result = __xrtTlsServer12FirstFlight(
			pSession, pState, pMessage, &Selection
		);
		goto cleanup;
	}
	if ( Selection.Retry ) {
		Result = __xrtTlsServerRetryFlight(
			pSession, pState, pMessage, &Selection
		) ? XTLS_OK : XTLS_ERROR;
		goto cleanup;
	}
	Flight.Protocol = Selection.Protocol;
	Flight.Cookie = Selection.Cookie;
	if ( !__xrtTlsServerNameCopy(&Selection, &Flight) ||
		!__xrtTlsServerHelloFlight(
			pSession, pState, &Selection, pMessage,
			&pState->HandshakeArena, &Flight,
			&pServerHello, &iServerHello, &HandshakeWrite,
			HandshakeSecret, ServerHandshake
		) || !__xrtTlsServerAuthentication(
			pSession, pState, &Selection,
			&pState->HandshakeArena, &Flight,
			(xbytesview) { HandshakeSecret, Flight.HashSize },
			(xbytesview) { ServerHandshake, Flight.HashSize },
			&pStream, &iStream
		) || !__xrtTlsServerRecords(
			pSession,
			(xbytesview) { pServerHello, iServerHello },
			(xbytesview) { pStream, iStream },
			&HandshakeWrite, &Flight
		) ) {
		goto cleanup;
	}
	Flight.Identity = Selection.Identity;
	Selection.Identity = NULL;
	Result = __xrtTlsSessionSendTake(
		pSession, Flight.Output, Flight.OutputSize
	);
	if ( Result != XTLS_OK ) {
		goto cleanup;
	}
	Flight.Output = NULL;
	__xrtTlsServerFlightCommit(pSession, pState, &Flight);
	Result = XTLS_OK;

cleanup:
	if ( (Result == XTLS_ERROR) || !pState->RetrySeen ) {
		pState->Select = NULL;
		pState->SelectContext = NULL;
		#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
			pState->Resume = NULL;
			pState->ResumeContext = NULL;
		#endif
	}
	__xrtTlsRecordKeyClear(&HandshakeWrite);
	__xrtTlsServerSelectionClear(&Selection);
	__xrtTlsServerFlightClear(&Flight);
	xrtSecureZero(ServerHandshake, sizeof(ServerHandshake));
	xrtSecureZero(HandshakeSecret, sizeof(HandshakeSecret));
	(void)xrtTempSecureReset(&pState->HandshakeArena);
	if ( Result == XTLS_ERROR ) {
		Result = __xrtTlsSessionFail(pSession);
	}
	return Result;
}



/* 验证客户端 Finished，派生接收应用密钥并发布 READY。 */
xtlsresult __xrtTlsServerFinished(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(pState->Cipher);
	xtlstranscript Next = pState->Transcript;
	xtlsrecordkey ReadKey;
	xbytesview VerifyData;
	xbytesview Encoded;
	xbytesview Empty = { NULL, 0 };
	uint8 TranscriptHash[XTLS_SERVER_SECRET_MAX_SIZE];
	uint8 FinishedKey[XTLS_SERVER_SECRET_MAX_SIZE];
	uint8 Expected[XTLS_SERVER_SECRET_MAX_SIZE];
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		uint8 ResumptionMaster[XTLS_SERVER_SECRET_MAX_SIZE];
	#endif
	xcryptohash Hash;
	bool bResult = false;

	memset(&ReadKey, 0, sizeof(ReadKey));
	memset(TranscriptHash, 0, sizeof(TranscriptHash));
	memset(FinishedKey, 0, sizeof(FinishedKey));
	memset(Expected, 0, sizeof(Expected));
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		memset(ResumptionMaster, 0, sizeof(ResumptionMaster));
	#endif
	if ( (pCipher == NULL) ||
		(pMessage->Type != XTLS_HANDSHAKE_FINISHED) ||
		(pCipher->HashSize != pState->HashSize) ) {
		(void)__xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"process-client-finished",
			"TLS server expected a valid client Finished"
		);
		goto cleanup;
	}
	Hash = __xrtTlsHash(pCipher->Hash);
	if ( !xrtTlsFinishedParse(
		pMessage->Body, pState->HashSize, &VerifyData
	) || !__xrtTlsTranscriptDigest(
		&pState->Transcript,
		TranscriptHash, pState->HashSize
	) || !__xrtTls13ExpandLabel(
		Hash,
		(xbytesview) {
			pState->ClientHandshakeTraffic, pState->HashSize
		}, XRT_STR_LITERAL("finished"), Empty,
		FinishedKey, pState->HashSize
	) || !__xrtTls13Finished(
		Hash,
		(xbytesview) { FinishedKey, pState->HashSize },
		(xbytesview) { TranscriptHash, pState->HashSize },
		Expected, pState->HashSize
	) ) {
		goto cleanup;
	}
	if ( !xrtConstTimeEqual(
		Expected, VerifyData.Data, pState->HashSize
	) ) {
		(void)__xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_VERIFY,
			"process-client-finished",
			"TLS client Finished verification failed"
		);
		goto cleanup;
	}
	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	if ( !__xrtTlsTranscriptUpdate(&Next, Encoded) ||
		#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
			!__xrtTlsTranscriptDigest(
				&Next, TranscriptHash, pState->HashSize
			) || !__xrtTls13DeriveSecret(
				Hash,
				(xbytesview) {
					pState->MasterSecret, pState->HashSize
				}, XRT_STR_LITERAL("res master"),
				(xbytesview) {
					TranscriptHash, pState->HashSize
				}, ResumptionMaster, pState->HashSize
			) ||
		#endif
		!__xrtTls13RecordKey(
			pState->Cipher,
			(xbytesview) {
				pState->ClientApplicationTraffic, pState->HashSize
			}, &ReadKey
		) ) {
		goto cleanup;
	}
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	pSession->ReadKey = ReadKey;
	xrtSecureZero(&ReadKey, sizeof(ReadKey));
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	memset(&Next, 0, sizeof(Next));
	xrtSecureZero(
		pState->ClientHandshakeTraffic, pState->SecretCapacity
	);
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		memcpy(
			pState->ResumptionMaster,
			ResumptionMaster, pState->HashSize
		);
		xrtSecureZero(
			pState->MasterSecret, pState->SecretCapacity
		);
		pState->ResumptionReady = true;
	#endif
	pState->Step = XTLS_SERVER_READY;
	if ( !__xrtTlsSessionSetState(
		pSession, XTLS_STATE_READY
	) ) {
		goto cleanup;
	}
	pSession->Wait = XTLS_WAIT_INPUT |
		(xrtTlsSessionSendSize(pSession) != 0 ? XTLS_WAIT_OUTPUT : 0);
	bResult = true;

cleanup:
	__xrtTlsRecordKeyClear(&ReadKey);
	__xrtTlsTranscriptClear(&Next);
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		xrtSecureZero(ResumptionMaster, sizeof(ResumptionMaster));
	#endif
	xrtSecureZero(Expected, sizeof(Expected));
	xrtSecureZero(FinishedKey, sizeof(FinishedKey));
	xrtSecureZero(TranscriptHash, sizeof(TranscriptHash));
	if ( !bResult ) {
		return __xrtTlsSessionFail(pSession);
	}
	return XTLS_OK;
}

#endif
