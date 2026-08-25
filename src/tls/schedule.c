#include "../internal/xrt_tls.h"
#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_TLS_SCHEDULE)

#define XTLS13_PREFIX "tls13 "
#define XTLS13_PREFIX_SIZE 6u
#define XTLS13_LABEL_MAX_SIZE 249u
#define XTLS13_HKDF_LABEL_MAX_SIZE 514u



/* 调度层 HMAC 状态只占当前构建中最大后端的空间。 */
typedef struct xtlsschedulehmac {
	union {
		#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
			xhmacsha256 Sha256;
		#endif
		#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
			xhmacsha384 Sha384;
		#endif
		uint64 Align;
	} State;
} xtlsschedulehmac;



/* 返回摘要算法在线路协议中的固定输出长度。 */
static size_t __xrtTlsScheduleKnownHashSize(xcryptohash Hash)
{
	if ( Hash == XCRYPTO_HASH_SHA256 ) {
		return 32u;
	}
	if ( Hash == XCRYPTO_HASH_SHA384 ) {
		return 48u;
	}
	return 0;
}



/* 把线路套件摘要统一映射到密码底座，供两端状态机共享。 */
xcryptohash __xrtTlsHash(xtlshash Hash)
{
	if ( Hash == XTLS_HASH_SHA256 ) {
		return XCRYPTO_HASH_SHA256;
	}
	if ( Hash == XTLS_HASH_SHA384 ) {
		return XCRYPTO_HASH_SHA384;
	}
	return (xcryptohash)0;
}



/* 检查借用视图的空指针与长度是否一致。 */
static bool __xrtTlsScheduleBytesValid(xbytesview Data)
{
	return (Data.Data != NULL) || (Data.Size == 0);
}



/* 检查借用文本视图的空指针与长度是否一致。 */
static bool __xrtTlsScheduleTextValid(xstrview Text)
{
	return (Text.Data != NULL) || (Text.Size == 0);
}



/* 设置调度层参数错误并返回 false，缩短各入口的失败路径。 */
static bool __xrtTlsScheduleArgument(cstr sOperation, cstr sMessage)
{
	__xrtTlsError(
		XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
		sOperation, sMessage, SIZE_MAX
	);
	return false;
}



/* 设置不支持的摘要错误并返回 false。 */
static bool __xrtTlsScheduleUnsupported(
	xcryptohash Hash,
	cstr sOperation
)
{
	(void)Hash;
	__xrtTlsError(
		XERR_UNSUPPORTED, XTLS_ERROR_KEY_DERIVATION,
		sOperation, "TLS hash schedule backend is not enabled", SIZE_MAX
	);
	return false;
}



/* 把底层摘要或 HKDF 失败包装为 TLS 原因链。 */
static bool __xrtTlsScheduleCause(
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	__xrtTlsErrorCause(
		XERR_PROTOCOL, Code, sOperation, sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 初始化所选摘要的 HMAC 状态。 */
static bool __xrtTlsScheduleHmacInit(
	xcryptohash Hash,
	xtlsschedulehmac* pState,
	xbytesview Key
)
{
	memset(pState, 0, sizeof(*pState));
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( Hash == XCRYPTO_HASH_SHA256 ) {
			return xrtHmacSha256Init(
				&pState->State.Sha256, Key.Data, Key.Size
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( Hash == XCRYPTO_HASH_SHA384 ) {
			return xrtHmacSha384Init(
				&pState->State.Sha384, Key.Data, Key.Size
			);
		}
	#endif
	(void)Hash;
	(void)Key;
	return false;
}



/* 向所选摘要的 HMAC 状态追加一段数据。 */
static bool __xrtTlsScheduleHmacUpdate(
	xcryptohash Hash,
	xtlsschedulehmac* pState,
	const void* pData,
	size_t iSize
)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( Hash == XCRYPTO_HASH_SHA256 ) {
			return xrtHmacSha256Update(
				&pState->State.Sha256, pData, iSize
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( Hash == XCRYPTO_HASH_SHA384 ) {
			return xrtHmacSha384Update(
				&pState->State.Sha384, pData, iSize
			);
		}
	#endif
	(void)Hash;
	(void)pState;
	(void)pData;
	(void)iSize;
	return false;
}



/* 从所选摘要的 HMAC 状态输出快照。 */
static bool __xrtTlsScheduleHmacFinal(
	xcryptohash Hash,
	const xtlsschedulehmac* pState,
	void* pOutput
)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( Hash == XCRYPTO_HASH_SHA256 ) {
			return xrtHmacSha256Final(&pState->State.Sha256, pOutput);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( Hash == XCRYPTO_HASH_SHA384 ) {
			return xrtHmacSha384Final(&pState->State.Sha384, pOutput);
		}
	#endif
	(void)Hash;
	(void)pState;
	(void)pOutput;
	return false;
}



/* 一次计算所选摘要的 HMAC，并清除中间状态。 */
static bool __xrtTlsScheduleHmac(
	xcryptohash Hash,
	xbytesview Key,
	xbytesview Message,
	void* pOutput
)
{
	xtlsschedulehmac State;
	bool bResult;

	bResult = __xrtTlsScheduleHmacInit(Hash, &State, Key) &&
		__xrtTlsScheduleHmacUpdate(
			Hash, &State, Message.Data, Message.Size
		) && __xrtTlsScheduleHmacFinal(Hash, &State, pOutput);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



/* 流式执行 TLS 1.2 P_hash，两个摘要后端共用一份控制流程。 */
static bool __xrtTlsSchedulePrf(
	xcryptohash Hash,
	xbytesview Secret,
	xstrview Label,
	xbytesview Seed,
	void* pOutput,
	size_t iOutputSize
)
{
	xtlsschedulehmac Base;
	xtlsschedulehmac State;
	uint8 A[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Block[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	size_t iHashSize = __xrtTlsScheduleKnownHashSize(Hash);
	size_t iOffset = 0;
	bool bResult = false;

	memset(&Base, 0, sizeof(Base));
	memset(&State, 0, sizeof(State));
	memset(A, 0, sizeof(A));
	memset(Block, 0, sizeof(Block));
	if ( !__xrtTlsScheduleHmacInit(Hash, &Base, Secret) ) {
		goto cleanup;
	}

	State = Base;
	if ( !__xrtTlsScheduleHmacUpdate(
		Hash, &State, Label.Data, Label.Size
	) || !__xrtTlsScheduleHmacUpdate(
		Hash, &State, Seed.Data, Seed.Size
	) || !__xrtTlsScheduleHmacFinal(Hash, &State, A) ) {
		goto cleanup;
	}

	while ( iOffset < iOutputSize ) {
		size_t iCopy = iOutputSize - iOffset;

		State = Base;
		if ( !__xrtTlsScheduleHmacUpdate(
			Hash, &State, A, iHashSize
		) || !__xrtTlsScheduleHmacUpdate(
			Hash, &State, Label.Data, Label.Size
		) || !__xrtTlsScheduleHmacUpdate(
			Hash, &State, Seed.Data, Seed.Size
		) || !__xrtTlsScheduleHmacFinal(Hash, &State, Block) ) {
			goto cleanup;
		}
		if ( iCopy > iHashSize ) {
			iCopy = iHashSize;
		}
		memcpy((uint8*)pOutput + iOffset, Block, iCopy);
		iOffset += iCopy;

		if ( iOffset < iOutputSize ) {
			State = Base;
			if ( !__xrtTlsScheduleHmacUpdate(
				Hash, &State, A, iHashSize
			) || !__xrtTlsScheduleHmacFinal(Hash, &State, A) ) {
				goto cleanup;
			}
		}
	}
	bResult = true;

cleanup:
	xrtSecureZero(Block, sizeof(Block));
	xrtSecureZero(A, sizeof(A));
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(&Base, sizeof(Base));
	return bResult;
}



/* 调用当前 transcript 绑定的摘要后端追加数据。 */
static bool __xrtTlsTranscriptUpdateBackend(
	xtlstranscript* pTranscript,
	xbytesview Message
)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( pTranscript->Hash == XCRYPTO_HASH_SHA256 ) {
			return __xrtTlsScheduleSha256Update(pTranscript, Message);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( pTranscript->Hash == XCRYPTO_HASH_SHA384 ) {
			return __xrtTlsScheduleSha384Update(pTranscript, Message);
		}
	#endif
	(void)pTranscript;
	(void)Message;
	return false;
}



/* 调用当前 transcript 绑定的摘要后端输出快照。 */
static bool __xrtTlsTranscriptDigestBackend(
	const xtlstranscript* pTranscript,
	void* pDigest
)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( pTranscript->Hash == XCRYPTO_HASH_SHA256 ) {
			return __xrtTlsScheduleSha256Digest(pTranscript, pDigest);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( pTranscript->Hash == XCRYPTO_HASH_SHA384 ) {
			return __xrtTlsScheduleSha384Digest(pTranscript, pDigest);
		}
	#endif
	(void)pTranscript;
	(void)pDigest;
	return false;
}



/* 返回当前构建是否提供指定 TLS 摘要后端。 */
bool __xrtTlsScheduleHashSupported(xcryptohash Hash)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( Hash == XCRYPTO_HASH_SHA256 ) {
			return true;
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( Hash == XCRYPTO_HASH_SHA384 ) {
			return true;
		}
	#endif
	(void)Hash;
	return false;
}



/* 返回当前构建中 TLS 调度支持的摘要长度。 */
size_t __xrtTlsScheduleHashSize(xcryptohash Hash)
{
	return __xrtTlsScheduleHashSupported(Hash) ?
		__xrtTlsScheduleKnownHashSize(Hash) : 0;
}



/* 初始化只跟踪协商摘要的 transcript。 */
bool __xrtTlsTranscriptInit(xtlstranscript* pTranscript, xcryptohash Hash)
{
	xtlstranscript Next;

	if ( pTranscript == NULL ) {
		return __xrtTlsScheduleArgument(
			"transcript-init", "TLS transcript destination is null"
		);
	}
	if ( !__xrtTlsScheduleHashSupported(Hash) ) {
		return __xrtTlsScheduleUnsupported(Hash, "transcript-init");
	}

	memset(&Next, 0, sizeof(Next));
	Next.Hash = Hash;
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( Hash == XCRYPTO_HASH_SHA256 ) {
			__xrtTlsScheduleSha256Init(&Next);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( Hash == XCRYPTO_HASH_SHA384 ) {
			__xrtTlsScheduleSha384Init(&Next);
		}
	#endif
	Next.Ready = true;
	xrtSecureZero(pTranscript, sizeof(*pTranscript));
	*pTranscript = Next;
	return true;
}



/* 清除 transcript 的全部中间状态。 */
void __xrtTlsTranscriptClear(xtlstranscript* pTranscript)
{
	if ( pTranscript != NULL ) {
		xrtSecureZero(pTranscript, sizeof(*pTranscript));
	}
}



/* 向 transcript 原子追加一段已编码握手消息。 */
bool __xrtTlsTranscriptUpdate(xtlstranscript* pTranscript, xbytesview Message)
{
	if ( (pTranscript == NULL) || !pTranscript->Ready ) {
		__xrtTlsError(
			XERR_STATE, XTLS_ERROR_TRANSCRIPT, "transcript-update",
			"TLS transcript is not initialized", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsScheduleBytesValid(Message) ) {
		return __xrtTlsScheduleArgument(
			"transcript-update", "TLS transcript message is invalid"
		);
	}
	if ( !__xrtTlsTranscriptUpdateBackend(pTranscript, Message) ) {
		return __xrtTlsScheduleCause(
			XTLS_ERROR_TRANSCRIPT, "transcript-update",
			"TLS transcript hash update failed"
		);
	}
	return true;
}



/* 从 transcript 快照输出摘要。 */
bool __xrtTlsTranscriptDigest(
	const xtlstranscript* pTranscript,
	void* pDigest,
	size_t iDigestSize
)
{
	size_t iHashSize;

	if ( (pTranscript == NULL) || !pTranscript->Ready ) {
		__xrtTlsError(
			XERR_STATE, XTLS_ERROR_TRANSCRIPT, "transcript-digest",
			"TLS transcript is not initialized", SIZE_MAX
		);
		return false;
	}
	iHashSize = __xrtTlsScheduleHashSize(pTranscript->Hash);
	if ( (pDigest == NULL) || (iDigestSize != iHashSize) ||
		 __xrtCryptoRangesOverlap(
			pDigest, iDigestSize, pTranscript, sizeof(*pTranscript)
		) ) {
		return __xrtTlsScheduleArgument(
			"transcript-digest", "TLS transcript digest buffer is invalid"
		);
	}
	if ( !__xrtTlsTranscriptDigestBackend(pTranscript, pDigest) ) {
		return __xrtTlsScheduleCause(
			XTLS_ERROR_TRANSCRIPT, "transcript-digest",
			"TLS transcript digest failed"
		);
	}
	return true;
}



/* 用 synthetic message_hash 原子替换 ClientHello1 transcript。 */
bool __xrtTlsTranscriptRetry(xtlstranscript* pTranscript)
{
	xtlstranscript Next;
	uint8 Digest[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Header[4];
	size_t iHashSize;
	bool bResult = false;

	if ( (pTranscript == NULL) || !pTranscript->Ready ) {
		__xrtTlsError(
			XERR_STATE, XTLS_ERROR_TRANSCRIPT, "transcript-retry",
			"TLS transcript is not initialized", SIZE_MAX
		);
		return false;
	}
	iHashSize = __xrtTlsScheduleHashSize(pTranscript->Hash);
	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTlsTranscriptDigest(
		pTranscript, Digest, iHashSize
	) || !__xrtTlsTranscriptInit(&Next, pTranscript->Hash) ) {
		goto cleanup;
	}

	Header[0] = 254u;
	Header[1] = 0;
	Header[2] = 0;
	Header[3] = (uint8)iHashSize;
	if ( !__xrtTlsTranscriptUpdate(
		&Next, (xbytesview) { Header, sizeof(Header) }
	) || !__xrtTlsTranscriptUpdate(
		&Next, (xbytesview) { Digest, iHashSize }
	) ) {
		goto cleanup;
	}

	xrtSecureZero(pTranscript, sizeof(*pTranscript));
	*pTranscript = Next;
	memset(&Next, 0, sizeof(Next));
	bResult = true;

cleanup:
	xrtSecureZero(Digest, sizeof(Digest));
	xrtSecureZero(&Next, sizeof(Next));
	return bResult;
}



/* 执行 TLS 1.3 HKDF-Extract。 */
bool __xrtTls13Extract(
	xcryptohash Hash,
	xbytesview Salt,
	xbytesview Ikm,
	void* pSecret,
	size_t iSecretSize
)
{
	size_t iHashSize = __xrtTlsScheduleHashSize(Hash);
	bool bResult = false;

	if ( iHashSize == 0 ) {
		return __xrtTlsScheduleUnsupported(Hash, "tls13-extract");
	}
	if ( !__xrtTlsScheduleBytesValid(Salt) ||
		 !__xrtTlsScheduleBytesValid(Ikm) || (pSecret == NULL) ||
		 (iSecretSize != iHashSize) ||
		 __xrtCryptoRangesOverlap(
			pSecret, iSecretSize, Salt.Data, Salt.Size
		) || __xrtCryptoRangesOverlap(
			pSecret, iSecretSize, Ikm.Data, Ikm.Size
		) ) {
		return __xrtTlsScheduleArgument(
			"tls13-extract", "TLS 1.3 extract arguments are invalid"
		);
	}

	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( Hash == XCRYPTO_HASH_SHA256 ) {
			bResult = __xrtTlsScheduleSha256Extract(Salt, Ikm, pSecret);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( Hash == XCRYPTO_HASH_SHA384 ) {
			bResult = __xrtTlsScheduleSha384Extract(Salt, Ikm, pSecret);
		}
	#endif
	if ( !bResult ) {
		return __xrtTlsScheduleCause(
			XTLS_ERROR_KEY_DERIVATION, "tls13-extract",
			"TLS 1.3 HKDF extract failed"
		);
	}
	return true;
}



/* 编码 HkdfLabel 并执行 TLS 1.3 HKDF-Expand-Label。 */
bool __xrtTls13ExpandLabel(
	xcryptohash Hash,
	xbytesview Secret,
	xstrview Label,
	xbytesview Context,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8 Info[XTLS13_HKDF_LABEL_MAX_SIZE];
	size_t iHashSize = __xrtTlsScheduleHashSize(Hash);
	size_t iInfoSize = 0;
	size_t iFullLabelSize;
	bool bResult = false;

	if ( iHashSize == 0 ) {
		return __xrtTlsScheduleUnsupported(Hash, "tls13-expand-label");
	}
	if ( !__xrtTlsScheduleBytesValid(Secret) ||
		 !__xrtTlsScheduleTextValid(Label) ||
		 !__xrtTlsScheduleBytesValid(Context) ||
		 ((pOutput == NULL) && (iOutputSize != 0)) ||
		 (Secret.Size != iHashSize) || (Label.Size == 0) ||
		 (Label.Size > XTLS13_LABEL_MAX_SIZE) ||
		 (Context.Size > UINT8_MAX) || (iOutputSize > UINT16_MAX) ||
		 (iOutputSize > (255u * iHashSize)) ||
		 __xrtCryptoRangesOverlap(
			pOutput, iOutputSize, Secret.Data, Secret.Size
		) || __xrtCryptoRangesOverlap(
			pOutput, iOutputSize, Label.Data, Label.Size
		) || __xrtCryptoRangesOverlap(
			pOutput, iOutputSize, Context.Data, Context.Size
		) ) {
		return __xrtTlsScheduleArgument(
			"tls13-expand-label", "TLS 1.3 HkdfLabel arguments are invalid"
		);
	}

	iFullLabelSize = XTLS13_PREFIX_SIZE + Label.Size;
	Info[iInfoSize++] = (uint8)(iOutputSize >> 8u);
	Info[iInfoSize++] = (uint8)iOutputSize;
	Info[iInfoSize++] = (uint8)iFullLabelSize;
	memcpy(Info + iInfoSize, XTLS13_PREFIX, XTLS13_PREFIX_SIZE);
	iInfoSize += XTLS13_PREFIX_SIZE;
	memcpy(Info + iInfoSize, Label.Data, Label.Size);
	iInfoSize += Label.Size;
	Info[iInfoSize++] = (uint8)Context.Size;
	if ( Context.Size != 0 ) {
		memcpy(Info + iInfoSize, Context.Data, Context.Size);
		iInfoSize += Context.Size;
	}

	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		if ( Hash == XCRYPTO_HASH_SHA256 ) {
			bResult = __xrtTlsScheduleSha256Expand(
				Secret, (xbytesview) { Info, iInfoSize },
				pOutput, iOutputSize
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		if ( Hash == XCRYPTO_HASH_SHA384 ) {
			bResult = __xrtTlsScheduleSha384Expand(
				Secret, (xbytesview) { Info, iInfoSize },
				pOutput, iOutputSize
			);
		}
	#endif
	xrtSecureZero(Info, sizeof(Info));
	if ( !bResult ) {
		return __xrtTlsScheduleCause(
			XTLS_ERROR_KEY_DERIVATION, "tls13-expand-label",
			"TLS 1.3 HKDF expand failed"
		);
	}
	return true;
}



/* 使用 transcript 摘要派生固定长度 TLS 1.3 secret。 */
bool __xrtTls13DeriveSecret(
	xcryptohash Hash,
	xbytesview Secret,
	xstrview Label,
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iOutputSize
)
{
	size_t iHashSize = __xrtTlsScheduleHashSize(Hash);

	if ( (iHashSize == 0) || (TranscriptHash.Size != iHashSize) ||
		 (iOutputSize != iHashSize) ) {
		if ( iHashSize == 0 ) {
			return __xrtTlsScheduleUnsupported(Hash, "tls13-derive-secret");
		}
		return __xrtTlsScheduleArgument(
			"tls13-derive-secret", "TLS 1.3 secret size is invalid"
		);
	}
	return __xrtTls13ExpandLabel(
		Hash, Secret, Label, TranscriptHash, pOutput, iOutputSize
	);
}



/* 通过空 transcript 快照输出协议规定的摘要常量。 */
bool __xrtTls13EmptyHash(
	xcryptohash Hash,
	void* pDigest,
	size_t iDigestSize
)
{
	xtlstranscript Empty;
	bool bResult;

	memset(&Empty, 0, sizeof(Empty));
	bResult = __xrtTlsTranscriptInit(&Empty, Hash) &&
		__xrtTlsTranscriptDigest(&Empty, pDigest, iDigestSize);
	__xrtTlsTranscriptClear(&Empty);
	return bResult;
}



/* 集中实现 TLS 1.3 early 到 handshake 的标准密钥调度。 */
bool __xrtTls13HandshakeSchedule(
	xcryptohash Hash,
	xbytesview Psk,
	xbytesview Shared,
	xbytesview TranscriptHash,
	void* pHandshake,
	void* pClientTraffic,
	void* pServerTraffic,
	size_t iSecretSize
)
{
	uint8 Zero[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 EmptyHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Early[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Derived[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	xbytesview Empty = { NULL, 0 };
	bool bResult = false;

	memset(Zero, 0, sizeof(Zero));
	memset(EmptyHash, 0, sizeof(EmptyHash));
	memset(Early, 0, sizeof(Early));
	memset(Derived, 0, sizeof(Derived));
	if ( (iSecretSize == 0) ||
		(iSecretSize != __xrtTlsScheduleHashSize(Hash)) ||
		((Psk.Data == NULL) && (Psk.Size != 0)) ||
		((Shared.Data == NULL) && (Shared.Size != 0)) ||
		(Shared.Size == 0) ||
		(TranscriptHash.Data == NULL) ||
		(TranscriptHash.Size != iSecretSize) ||
		(pHandshake == NULL) || (pClientTraffic == NULL) ||
		(pServerTraffic == NULL) ) {
		(void)__xrtTlsScheduleArgument(
			"tls13-handshake-schedule",
			"TLS 1.3 handshake schedule input is invalid"
		);
		goto cleanup;
	}
	if ( Psk.Size == 0 ) {
		Psk = (xbytesview) { Zero, iSecretSize };
	} else if ( Psk.Size != iSecretSize ) {
		(void)__xrtTlsScheduleArgument(
			"tls13-handshake-schedule",
			"TLS 1.3 PSK size does not match its hash"
		);
		goto cleanup;
	}
	bResult = __xrtTls13EmptyHash(
		Hash, EmptyHash, iSecretSize
	) && __xrtTls13Extract(
		Hash, Empty, Psk, Early, iSecretSize
	) && __xrtTls13DeriveSecret(
		Hash, (xbytesview) { Early, iSecretSize },
		XRT_STR_LITERAL("derived"),
		(xbytesview) { EmptyHash, iSecretSize },
		Derived, iSecretSize
	) && __xrtTls13Extract(
		Hash, (xbytesview) { Derived, iSecretSize }, Shared,
		pHandshake, iSecretSize
	) && __xrtTls13DeriveSecret(
		Hash, (xbytesview) { pHandshake, iSecretSize },
		XRT_STR_LITERAL("c hs traffic"), TranscriptHash,
		pClientTraffic, iSecretSize
	) && __xrtTls13DeriveSecret(
		Hash, (xbytesview) { pHandshake, iSecretSize },
		XRT_STR_LITERAL("s hs traffic"), TranscriptHash,
		pServerTraffic, iSecretSize
	);

cleanup:
	xrtSecureZero(Derived, sizeof(Derived));
	xrtSecureZero(Early, sizeof(Early));
	xrtSecureZero(EmptyHash, sizeof(EmptyHash));
	xrtSecureZero(Zero, sizeof(Zero));
	return bResult;
}



/* 集中实现 TLS 1.3 handshake 到 master 的标准密钥调度。 */
bool __xrtTls13ApplicationSchedule(
	xcryptohash Hash,
	xbytesview Handshake,
	xbytesview TranscriptHash,
	void* pMaster,
	void* pClientTraffic,
	void* pServerTraffic,
	size_t iSecretSize
)
{
	uint8 Zero[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 EmptyHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Derived[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	bool bResult = false;

	memset(Zero, 0, sizeof(Zero));
	memset(EmptyHash, 0, sizeof(EmptyHash));
	memset(Derived, 0, sizeof(Derived));
	if ( (iSecretSize == 0) ||
		(iSecretSize != __xrtTlsScheduleHashSize(Hash)) ||
		(Handshake.Data == NULL) ||
		(Handshake.Size != iSecretSize) ||
		(TranscriptHash.Data == NULL) ||
		(TranscriptHash.Size != iSecretSize) ||
		(pMaster == NULL) || (pClientTraffic == NULL) ||
		(pServerTraffic == NULL) ) {
		(void)__xrtTlsScheduleArgument(
			"tls13-application-schedule",
			"TLS 1.3 application schedule input is invalid"
		);
		goto cleanup;
	}
	bResult = __xrtTls13EmptyHash(
		Hash, EmptyHash, iSecretSize
	) && __xrtTls13DeriveSecret(
		Hash, Handshake, XRT_STR_LITERAL("derived"),
		(xbytesview) { EmptyHash, iSecretSize },
		Derived, iSecretSize
	) && __xrtTls13Extract(
		Hash, (xbytesview) { Derived, iSecretSize },
		(xbytesview) { Zero, iSecretSize },
		pMaster, iSecretSize
	) && __xrtTls13DeriveSecret(
		Hash, (xbytesview) { pMaster, iSecretSize },
		XRT_STR_LITERAL("c ap traffic"), TranscriptHash,
		pClientTraffic, iSecretSize
	) && __xrtTls13DeriveSecret(
		Hash, (xbytesview) { pMaster, iSecretSize },
		XRT_STR_LITERAL("s ap traffic"), TranscriptHash,
		pServerTraffic, iSecretSize
	);

cleanup:
	xrtSecureZero(Derived, sizeof(Derived));
	xrtSecureZero(EmptyHash, sizeof(EmptyHash));
	xrtSecureZero(Zero, sizeof(Zero));
	return bResult;
}



/* 在可选 HRR transcript 前缀之后计算会话票据 binder。 */
bool __xrtTls13ResumptionBinderTranscript(
	xcryptohash Hash,
	xbytesview Psk,
	const xtlstranscript* pPrefix,
	xbytesview ClientHelloPartial,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8 EmptyHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Early[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 BinderKey[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 FinishedKey[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 TranscriptHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	xtlstranscript Transcript;
	xbytesview Empty = { NULL, 0 };
	size_t iHashSize = __xrtTlsScheduleHashSize(Hash);
	bool bResult = false;

	memset(EmptyHash, 0, sizeof(EmptyHash));
	memset(Early, 0, sizeof(Early));
	memset(BinderKey, 0, sizeof(BinderKey));
	memset(FinishedKey, 0, sizeof(FinishedKey));
	memset(TranscriptHash, 0, sizeof(TranscriptHash));
	memset(&Transcript, 0, sizeof(Transcript));
	if ( (iHashSize == 0) || (Psk.Data == NULL) ||
		(Psk.Size != iHashSize) ||
		((ClientHelloPartial.Data == NULL) &&
		 (ClientHelloPartial.Size != 0)) || (pOutput == NULL) ||
		(iOutputSize != iHashSize) ) {
		(void)__xrtTlsScheduleArgument(
			"tls13-resumption-binder",
			"TLS 1.3 resumption binder input is invalid"
		);
		goto cleanup;
	}
	if ( pPrefix != NULL ) {
		if ( !pPrefix->Ready || (pPrefix->Hash != Hash) ) {
			(void)__xrtTlsScheduleArgument(
				"tls13-resumption-binder",
				"TLS 1.3 binder transcript prefix is invalid"
			);
			goto cleanup;
		}
		Transcript = *pPrefix;
	} else if ( !__xrtTlsTranscriptInit(&Transcript, Hash) ) {
		goto cleanup;
	}
	bResult = __xrtTls13EmptyHash(
		Hash, EmptyHash, iHashSize
	) && __xrtTlsTranscriptUpdate(
		&Transcript, ClientHelloPartial
	) && __xrtTlsTranscriptDigest(
		&Transcript, TranscriptHash, iHashSize
	) && __xrtTls13Extract(
		Hash, Empty, Psk, Early, iHashSize
	) && __xrtTls13DeriveSecret(
		Hash, (xbytesview) { Early, iHashSize },
		XRT_STR_LITERAL("res binder"),
		(xbytesview) { EmptyHash, iHashSize },
		BinderKey, iHashSize
	) && __xrtTls13ExpandLabel(
		Hash, (xbytesview) { BinderKey, iHashSize },
		XRT_STR_LITERAL("finished"), Empty,
		FinishedKey, iHashSize
	) && __xrtTls13Finished(
		Hash, (xbytesview) { FinishedKey, iHashSize },
		(xbytesview) { TranscriptHash, iHashSize },
		pOutput, iOutputSize
	);

cleanup:
	__xrtTlsTranscriptClear(&Transcript);
	xrtSecureZero(TranscriptHash, sizeof(TranscriptHash));
	xrtSecureZero(FinishedKey, sizeof(FinishedKey));
	xrtSecureZero(BinderKey, sizeof(BinderKey));
	xrtSecureZero(Early, sizeof(Early));
	xrtSecureZero(EmptyHash, sizeof(EmptyHash));
	return bResult;
}



/* 从首个 ClientHello transcript 计算会话票据 binder。 */
bool __xrtTls13ResumptionBinder(
	xcryptohash Hash,
	xbytesview Psk,
	xbytesview ClientHelloPartial,
	void* pOutput,
	size_t iOutputSize
)
{
	return __xrtTls13ResumptionBinderTranscript(
		Hash, Psk, NULL, ClientHelloPartial,
		pOutput, iOutputSize
	);
}



/* 计算 TLS 1.3 Finished.verify_data。 */
bool __xrtTls13Finished(
	xcryptohash Hash,
	xbytesview FinishedKey,
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iOutputSize
)
{
	size_t iHashSize = __xrtTlsScheduleHashSize(Hash);

	if ( iHashSize == 0 ) {
		return __xrtTlsScheduleUnsupported(Hash, "tls13-finished");
	}
	if ( !__xrtTlsScheduleBytesValid(FinishedKey) ||
		 !__xrtTlsScheduleBytesValid(TranscriptHash) ||
		 (FinishedKey.Size != iHashSize) ||
		 (TranscriptHash.Size != iHashSize) || (pOutput == NULL) ||
		 (iOutputSize != iHashSize) ||
		 __xrtCryptoRangesOverlap(
			pOutput, iOutputSize, FinishedKey.Data, FinishedKey.Size
		) || __xrtCryptoRangesOverlap(
			pOutput, iOutputSize, TranscriptHash.Data, TranscriptHash.Size
		) ) {
		return __xrtTlsScheduleArgument(
			"tls13-finished", "TLS 1.3 Finished arguments are invalid"
		);
	}

	if ( !__xrtTlsScheduleHmac(
		Hash, FinishedKey, TranscriptHash, pOutput
	) ) {
		return __xrtTlsScheduleCause(
			XTLS_ERROR_KEY_DERIVATION, "tls13-finished",
			"TLS 1.3 Finished HMAC failed"
		);
	}
	return true;
}



/* 流式执行 TLS 1.2 P_hash。 */
bool __xrtTls12Prf(
	xcryptohash Hash,
	xbytesview Secret,
	xstrview Label,
	xbytesview Seed,
	void* pOutput,
	size_t iOutputSize
)
{
	if ( !__xrtTlsScheduleHashSupported(Hash) ) {
		return __xrtTlsScheduleUnsupported(Hash, "tls12-prf");
	}
	if ( !__xrtTlsScheduleBytesValid(Secret) ||
		 !__xrtTlsScheduleTextValid(Label) ||
		 !__xrtTlsScheduleBytesValid(Seed) ||
		 ((pOutput == NULL) && (iOutputSize != 0)) ||
		 (Label.Size == 0) || (iOutputSize > XTLS12_PRF_MAX_SIZE) ||
		 __xrtCryptoRangesOverlap(
			pOutput, iOutputSize, Secret.Data, Secret.Size
		) || __xrtCryptoRangesOverlap(
			pOutput, iOutputSize, Label.Data, Label.Size
		) || __xrtCryptoRangesOverlap(
			pOutput, iOutputSize, Seed.Data, Seed.Size
		) ) {
		return __xrtTlsScheduleArgument(
			"tls12-prf", "TLS 1.2 PRF arguments are invalid"
		);
	}
	if ( iOutputSize == 0 ) {
		return true;
	}

	if ( !__xrtTlsSchedulePrf(
		Hash, Secret, Label, Seed, pOutput, iOutputSize
	) ) {
		return __xrtTlsScheduleCause(
			XTLS_ERROR_KEY_DERIVATION, "tls12-prf",
			"TLS 1.2 PRF failed"
		);
	}
	return true;
}



/* 使用 RFC 7627 扩展主密钥和 RFC 5246 key expansion 派生记录材料。 */
bool __xrtTls12KeyMaterial(
	xtlscipher Cipher,
	xbytesview Shared,
	xbytesview SessionHash,
	xbytesview ClientRandom,
	xbytesview ServerRandom,
	xtls12keymaterial* pMaterial
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(Cipher);
	xtls12keymaterial Next;
	uint8 Seed[XTLS12_RANDOM_SIZE * 2u];
	uint8 KeyBlock[(XTLS12_KEY_MAX_SIZE + XTLS12_IV_MAX_SIZE) * 2u];
	size_t iKeyBlock;
	size_t iOffset = 0;
	bool bResult = false;

	memset(&Next, 0, sizeof(Next));
	memset(Seed, 0, sizeof(Seed));
	memset(KeyBlock, 0, sizeof(KeyBlock));
	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_12) ||
		(pMaterial == NULL) || (Shared.Data == NULL) || (Shared.Size == 0) ||
		(SessionHash.Data == NULL) ||
		((SessionHash.Size != 32u) && (SessionHash.Size != 48u)) ||
		(ClientRandom.Data == NULL) ||
		(ClientRandom.Size != XTLS12_RANDOM_SIZE) ||
		(ServerRandom.Data == NULL) ||
		(ServerRandom.Size != XTLS12_RANDOM_SIZE) ||
		(pCipher->KeySize > XTLS12_KEY_MAX_SIZE) ||
		(pCipher->IvSize > XTLS12_IV_MAX_SIZE) ) {
		return __xrtTlsScheduleArgument(
			"tls12-key-material",
			"TLS 1.2 key material arguments are invalid"
		);
	}
	Next.Hash = __xrtTlsHash(pCipher->Hash);
	Next.HashSize = pCipher->HashSize;
	Next.KeySize = pCipher->KeySize;
	Next.IvSize = pCipher->IvSize;
	if ( !__xrtTlsScheduleHashSupported(Next.Hash) ||
		(SessionHash.Size != Next.HashSize) ) {
		return __xrtTlsScheduleUnsupported(
			Next.Hash, "tls12-key-material"
		);
	}
	iKeyBlock = (Next.KeySize + Next.IvSize) * 2u;
	memcpy(Seed, ServerRandom.Data, XTLS12_RANDOM_SIZE);
	memcpy(
		Seed + XTLS12_RANDOM_SIZE, ClientRandom.Data, XTLS12_RANDOM_SIZE
	);
	if ( !__xrtTls12Prf(
		Next.Hash, Shared, XRT_STR_LITERAL("extended master secret"),
		SessionHash, Next.Master, sizeof(Next.Master)
	) || !__xrtTls12Prf(
		Next.Hash,
		(xbytesview) { Next.Master, sizeof(Next.Master) },
		XRT_STR_LITERAL("key expansion"),
		(xbytesview) { Seed, sizeof(Seed) }, KeyBlock, iKeyBlock
	) ) {
		goto cleanup;
	}
	memcpy(Next.ClientKey, KeyBlock + iOffset, Next.KeySize);
	iOffset += Next.KeySize;
	memcpy(Next.ServerKey, KeyBlock + iOffset, Next.KeySize);
	iOffset += Next.KeySize;
	memcpy(Next.ClientIv, KeyBlock + iOffset, Next.IvSize);
	iOffset += Next.IvSize;
	memcpy(Next.ServerIv, KeyBlock + iOffset, Next.IvSize);
	*pMaterial = Next;
	memset(&Next, 0, sizeof(Next));
	bResult = true;

cleanup:
	xrtSecureZero(KeyBlock, sizeof(KeyBlock));
	xrtSecureZero(Seed, sizeof(Seed));
	xrtSecureZero(&Next, sizeof(Next));
	return bResult;
}



/* 计算 TLS 1.2 client/server Finished.verify_data。 */
bool __xrtTls12Finished(
	xcryptohash Hash,
	xbytesview Master,
	bool bServer,
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iOutputSize
)
{
	xstrview Label = bServer ?
		XRT_STR_LITERAL("server finished") :
		XRT_STR_LITERAL("client finished");
	size_t iHashSize = __xrtTlsScheduleHashSize(Hash);

	if ( (Master.Data == NULL) ||
		(Master.Size != XTLS12_MASTER_SECRET_SIZE) ||
		(TranscriptHash.Data == NULL) ||
		(TranscriptHash.Size != iHashSize) ||
		(pOutput == NULL) || (iOutputSize != XTLS12_FINISHED_SIZE) ) {
		return __xrtTlsScheduleArgument(
			"tls12-finished", "TLS 1.2 Finished arguments are invalid"
		);
	}
	return __xrtTls12Prf(
		Hash, Master, Label, TranscriptHash, pOutput, iOutputSize
	);
}



#undef XTLS13_HKDF_LABEL_MAX_SIZE
#undef XTLS13_LABEL_MAX_SIZE
#undef XTLS13_PREFIX_SIZE
#undef XTLS13_PREFIX

#endif
