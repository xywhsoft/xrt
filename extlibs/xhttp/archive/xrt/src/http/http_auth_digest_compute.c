#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2)

#define XRT_HTTP_DIGEST_MAX_MESSAGE (UINT64_MAX >> 3u)



/* 统一承载 SHA-256、SHA-512/256 与可选 MD5 的流状态。 */
typedef union xrt_http_auth_digest_hash_state {
	xsha256 Sha256;
	xsha512_256 Sha512_256;
	#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_MD5)
	xmd5 Md5;
	#endif
} xrt_http_auth_digest_hash_state;



/* 把 session 算法映射到共享同一摘要函数的基础算法。 */
static xhttpdigestalgorithm __xrtHttpDigestBaseAlgorithm(
	xhttpdigestalgorithm Algorithm
)
{
	switch ( Algorithm ) {
		case XHTTP_DIGEST_ALGORITHM_MD5_SESSION:
			return XHTTP_DIGEST_ALGORITHM_MD5;
		case XHTTP_DIGEST_ALGORITHM_SHA256_SESSION:
			return XHTTP_DIGEST_ALGORITHM_SHA256;
		case XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION:
			return XHTTP_DIGEST_ALGORITHM_SHA512_256;
		default:
			return Algorithm;
	}
}



/* 区分无效算法值与当前裁剪闭包未提供的已知算法。 */
static bool __xrtHttpDigestAlgorithmValid(
	xhttpdigestalgorithm Algorithm
)
{
	if ( xrtHttpDigestSize(Algorithm) == 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !xrtHttpDigestAlgorithmSupported(Algorithm) ) {
		__xrtErrorSetUnsupported();
		return false;
	}
	return true;
}



/* 初始化所选摘要后端。 */
static void __xrtHttpDigestStateInit(
	xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm
)
{
	memset(pState, 0, sizeof(*pState));
	switch ( __xrtHttpDigestBaseAlgorithm(Algorithm) ) {
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			xrtSha256Init(&pState->Sha256);
			break;
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			xrtSha512_256Init(&pState->Sha512_256);
			break;
		#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_MD5)
		case XHTTP_DIGEST_ALGORITHM_MD5:
			xrtMd5Init(&pState->Md5);
			break;
		#endif
		default:
			break;
	}
}



/* 向所选摘要后端追加一段连续字节。 */
static bool __xrtHttpDigestStateUpdate(
	xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm,
	const void* pData,
	size_t iSize
)
{
	switch ( __xrtHttpDigestBaseAlgorithm(Algorithm) ) {
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			return xrtSha256Update(&pState->Sha256, pData, iSize);
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			return xrtSha512_256Update(
				&pState->Sha512_256, pData, iSize
			);
		#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_MD5)
		case XHTTP_DIGEST_ALGORITHM_MD5:
			return xrtMd5Update(&pState->Md5, pData, iSize);
		#endif
		default:
			__xrtErrorSetUnsupported();
			return false;
	}
}



/* 从所选摘要后端的快照读取二进制摘要。 */
static bool __xrtHttpDigestStateFinal(
	const xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm,
	void* pDigest
)
{
	switch ( __xrtHttpDigestBaseAlgorithm(Algorithm) ) {
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			return xrtSha256Final(&pState->Sha256, pDigest);
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			return xrtSha512_256Final(
				&pState->Sha512_256, pDigest
			);
		#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_MD5)
		case XHTTP_DIGEST_ALGORITHM_MD5:
			return xrtMd5Final(&pState->Md5, pDigest);
		#endif
		default:
			__xrtErrorSetUnsupported();
			return false;
	}
}



/* 把二进制摘要转换为 RFC 7616 使用的小写十六进制文本。 */
static void __xrtHttpDigestHexWrite(
	const uint8* pDigest,
	size_t iSize,
	char* sOutput
)
{
	static const char Hex[] = "0123456789abcdef";

	for ( size_t i = 0; i < iSize; i++ ) {
		sOutput[i * 2u] = Hex[pDigest[i] >> 4u];
		sOutput[(i * 2u) + 1u] = Hex[pDigest[i] & 0x0Fu];
	}
}



/* 完成当前摘要，规范编码文本，并清理流状态和二进制副本。 */
static bool __xrtHttpDigestStateFinalText(
	xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm,
	char* sOutput
)
{
	uint8 Digest[XRT_HTTP_DIGEST_MAX_SIZE];
	size_t iSize = xrtHttpDigestSize(Algorithm);
	bool bResult;

	bResult = __xrtHttpDigestStateFinal(
		pState, Algorithm, Digest
	);
	if ( bResult ) {
		__xrtHttpDigestHexWrite(Digest, iSize, sOutput);
	}
	xrtSecureZero(Digest, sizeof(Digest));
	xrtSecureZero(pState, sizeof(*pState));
	return bResult;
}



/* 校验输出描述符、固定结构和所有借用输入之间没有危险别名。 */
static bool __xrtHttpDigestOutputValid(
	const void* pOwner,
	size_t iOwnerSize,
	const xstrview* pViews,
	size_t iViewCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !__xrtRangeValid(pSize, sizeof(size_t)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !__xrtRangeValid(
			pOutput, iCapacity
		)) || ((pOwner != NULL) && (!__xrtRangeValid(
			pOwner, iOwnerSize
		) || __xrtRangesOverlap(
			pOwner, iOwnerSize, pSize, sizeof(size_t)
		) || ((pOutput != NULL) && __xrtRangesOverlap(
			pOwner, iOwnerSize, pOutput, iCapacity
		)))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < iViewCount; i++ ) {
		if ( !__xrtHttpViewValid(pViews[i]) ||
			__xrtRangesOverlap(
				pViews[i].Data, pViews[i].Size,
				pSize, sizeof(size_t)
			) || ((pOwner != NULL) && __xrtRangesOverlap(
				pViews[i].Data, pViews[i].Size,
				pOwner, iOwnerSize
			)) || ((pOutput != NULL) && __xrtRangesOverlap(
				pViews[i].Data, pViews[i].Size,
				pOutput, iCapacity
			)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	return true;
}



/* 发布固定长度摘要的查询、短缓冲或实际写入决策。 */
static bool __xrtHttpDigestOutputPrepare(
	size_t iRequired,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pWrite
)
{
	*pWrite = false;
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	*pWrite = true;
	return true;
}



/* 保证拼接后的摘要消息落在所有当前后端都能表示的长度范围内。 */
static bool __xrtHttpDigestMessageSize(
	const size_t* pParts,
	size_t iCount
)
{
	size_t iTotal = 0;

	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpSizeAdd(&iTotal, pParts[i]) ) {
			return false;
		}
	}
	if ( (uint64)iTotal > XRT_HTTP_DIGEST_MAX_MESSAGE ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 向摘要状态追加一个已经验证的文本视图。 */
static bool __xrtHttpDigestStateText(
	xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm,
	xstrview Text
)
{
	return __xrtHttpDigestStateUpdate(
		pState, Algorithm, Text.Data, Text.Size
	);
}



/* 使用所选 Digest 算法散列任意连续数据。 */
XRT_API bool xrtHttpDigestHash(
	xhttpdigestalgorithm Algorithm,
	const void* pData,
	size_t iDataSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Data = { (cstr)pData, iDataSize };
	xrt_http_auth_digest_hash_state State;
	char Text[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iRequired;
	bool bWrite;

	if ( !__xrtHttpDigestOutputValid(
		NULL, 0, &Data, 1u,
		pOutput, iCapacity, pSize
	) || !__xrtHttpDigestAlgorithmValid(Algorithm) ||
		!__xrtHttpDigestMessageSize(&iDataSize, 1u) ) {
		return false;
	}
	iRequired = xrtHttpDigestSize(Algorithm) * 2u;
	if ( !__xrtHttpDigestOutputPrepare(
		iRequired, pOutput, iCapacity, pSize, &bWrite
	) || !bWrite ) {
		return pOutput == NULL;
	}
	__xrtHttpDigestStateInit(&State, Algorithm);
	if ( !__xrtHttpDigestStateUpdate(
		&State, Algorithm, pData, iDataSize
	) || !__xrtHttpDigestStateFinalText(
		&State, Algorithm, Text
	) ) {
		xrtSecureZero(&State, sizeof(State));
		xrtSecureZero(Text, sizeof(Text));
		return false;
	}
	memcpy(pOutput, Text, iRequired);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	xrtSecureZero(Text, sizeof(Text));
	return true;
}



/* 计算用户身份拼接值，Password 为空指针时生成 userhash。 */
static bool __xrtHttpDigestIdentity(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	const xstrview* pPassword,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Views[3] = { Username, Realm, { NULL, 0 } };
	xrt_http_auth_digest_hash_state State;
	char Text[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	static const char Colon = ':';
	size_t Parts[5];
	size_t iViewCount = 2u;
	size_t iPartCount = 3u;
	size_t iRequired;
	bool bWrite;

	Parts[0] = Username.Size;
	Parts[1] = 1u;
	Parts[2] = Realm.Size;
	if ( pPassword != NULL ) {
		Views[2] = *pPassword;
		iViewCount = 3u;
		Parts[3] = 1u;
		Parts[4] = pPassword->Size;
		iPartCount = 5u;
	}
	if ( !__xrtHttpDigestOutputValid(
		NULL, 0, Views, iViewCount,
		pOutput, iCapacity, pSize
	) || !__xrtHttpDigestAlgorithmValid(Algorithm) ||
		!__xrtHttpDigestMessageSize(Parts, iPartCount) ) {
		return false;
	}
	iRequired = xrtHttpDigestSize(Algorithm) * 2u;
	if ( !__xrtHttpDigestOutputPrepare(
		iRequired, pOutput, iCapacity, pSize, &bWrite
	) || !bWrite ) {
		return pOutput == NULL;
	}
	__xrtHttpDigestStateInit(&State, Algorithm);
	if ( !__xrtHttpDigestStateText(
		&State, Algorithm, Username
	) || !__xrtHttpDigestStateUpdate(
		&State, Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, Algorithm, Realm
	) || ((pPassword != NULL) && (!__xrtHttpDigestStateUpdate(
		&State, Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, Algorithm, *pPassword
	))) || !__xrtHttpDigestStateFinalText(
		&State, Algorithm, Text
	) ) {
		xrtSecureZero(&State, sizeof(State));
		xrtSecureZero(Text, sizeof(Text));
		return false;
	}
	memcpy(pOutput, Text, iRequired);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	xrtSecureZero(Text, sizeof(Text));
	return true;
}



/* 计算可持久化的基础 H(username:realm:password)。 */
XRT_API bool xrtHttpDigestSecret(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	xstrview Password,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestIdentity(
		Algorithm, Username, Realm, &Password,
		pOutput, iCapacity, pSize
	);
}



/* 计算 RFC 7616 的 H(username:realm) 用户名摘要。 */
XRT_API bool xrtHttpDigestUserHash(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestIdentity(
		Algorithm, Username, Realm, NULL,
		pOutput, iCapacity, pSize
	);
}



/* 把已经验证的十六进制文本规范化为小写。 */
static void __xrtHttpDigestHexLower(
	xstrview Text,
	char* sOutput
)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		sOutput[i] = (char)__xrtHttpAsciiLower(
			(uint8)Text.Data[i]
		);
	}
}



/* 校验证明上下文、借用视图、算法和 qop 关系。 */
static bool __xrtHttpDigestProofValid(
	const xhttpdigestproof* pInput,
	xstrview Method,
	bool bResponse,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestproof* pProof
)
{
	xstrview Views[6];
	size_t iViewCount = bResponse ? 5u : 6u;
	size_t iDigest;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pProof, pInput, sizeof(*pProof));
	Views[0] = pProof->Secret;
	Views[1] = pProof->Nonce;
	Views[2] = pProof->Cnonce;
	Views[3] = pProof->Uri;
	Views[4] = pProof->EntityHash;
	Views[5] = Method;
	if ( !__xrtHttpDigestOutputValid(
		pInput, sizeof(*pInput), Views, iViewCount,
		pOutput, iCapacity, pSize
	) || !__xrtHttpDigestAlgorithmValid(pProof->Algorithm) ) {
		return false;
	}
	iDigest = xrtHttpDigestSize(pProof->Algorithm) * 2u;
	if ( ((pProof->Qop != XHTTP_DIGEST_QOP_AUTH) &&
		 (pProof->Qop != XHTTP_DIGEST_QOP_AUTH_INT)) ||
		(pProof->NonceCount == 0) ||
		(pProof->Secret.Size != iDigest) ||
		!__xrtHttpDigestHexViewValid(pProof->Secret) ||
		(pProof->Nonce.Size == 0) ||
		(pProof->Cnonce.Size == 0) ||
		(pProof->Uri.Size == 0) ||
		(!bResponse && (Method.Size == 0)) ||
		((pProof->Qop == XHTTP_DIGEST_QOP_AUTH) &&
		 (pProof->EntityHash.Size != 0)) ||
		((pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		 ((pProof->EntityHash.Size != iDigest) ||
		  !__xrtHttpDigestHexViewValid(pProof->EntityHash))) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 预检 session A1、A2 和最终 KD 三段消息的累计长度。 */
static bool __xrtHttpDigestProofSizes(
	const xhttpdigestproof* pProof,
	xstrview Method,
	bool bResponse
)
{
	size_t iDigest = xrtHttpDigestSize(pProof->Algorithm) * 2u;
	size_t A1[] = {
		iDigest, 1u, pProof->Nonce.Size, 1u, pProof->Cnonce.Size
	};
	size_t A2[] = {
		bResponse ? 0u : Method.Size,
		1u,
		pProof->Uri.Size,
		(pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT) ? 1u : 0u,
		(pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT) ? iDigest : 0u
	};
	xstrview Qop = xrtHttpDigestQopName(pProof->Qop);
	size_t Final[] = {
		iDigest, 1u, pProof->Nonce.Size, 1u, 8u, 1u,
		pProof->Cnonce.Size, 1u, Qop.Size, 1u, iDigest
	};

	if ( xrtHttpDigestAlgorithmSession(pProof->Algorithm) &&
		!__xrtHttpDigestMessageSize(
			A1, sizeof(A1) / sizeof(A1[0])
		) ) {
		return false;
	}
	return __xrtHttpDigestMessageSize(
		A2, sizeof(A2) / sizeof(A2[0])
	) && __xrtHttpDigestMessageSize(
		Final, sizeof(Final) / sizeof(Final[0])
	);
}



/* 计算请求 response 或服务器 rspauth 的公共 RFC 7616 公式。 */
static bool __xrtHttpDigestProofCalculate(
	const xhttpdigestproof* pProof,
	xstrview Method,
	bool bResponse,
	char* sOutput
)
{
	xrt_http_auth_digest_hash_state State;
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char A1[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char A2[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char Entity[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char NonceCount[8];
	static const char Colon = ':';
	xstrview Qop = xrtHttpDigestQopName(pProof->Qop);
	size_t iDigest = xrtHttpDigestSize(pProof->Algorithm) * 2u;
	bool bResult = false;

	__xrtHttpDigestHexLower(pProof->Secret, Secret);
	if ( pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT ) {
		__xrtHttpDigestHexLower(pProof->EntityHash, Entity);
	}
	if ( xrtHttpDigestAlgorithmSession(pProof->Algorithm) ) {
		__xrtHttpDigestStateInit(&State, pProof->Algorithm);
		if ( !__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, Secret, iDigest
		) || !__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, &Colon, 1u
		) || !__xrtHttpDigestStateText(
			&State, pProof->Algorithm, pProof->Nonce
		) || !__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, &Colon, 1u
		) || !__xrtHttpDigestStateText(
			&State, pProof->Algorithm, pProof->Cnonce
		) || !__xrtHttpDigestStateFinalText(
			&State, pProof->Algorithm, A1
		) ) {
			goto cleanup;
		}
	} else {
		memcpy(A1, Secret, iDigest);
	}

	__xrtHttpDigestStateInit(&State, pProof->Algorithm);
	if ( (!bResponse && !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, Method
	)) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, pProof->Uri
	) || ((pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		(!__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, &Colon, 1u
		) || !__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, Entity, iDigest
		))) || !__xrtHttpDigestStateFinalText(
		&State, pProof->Algorithm, A2
	) ) {
		goto cleanup;
	}

	__xrtHttpDigestNonceCountWrite(
		pProof->NonceCount, NonceCount
	);
	__xrtHttpDigestStateInit(&State, pProof->Algorithm);
	if ( !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, A1, iDigest
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, pProof->Nonce
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, NonceCount, sizeof(NonceCount)
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, pProof->Cnonce
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, Qop
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, A2, iDigest
	) || !__xrtHttpDigestStateFinalText(
		&State, pProof->Algorithm, sOutput
	) ) {
		goto cleanup;
	}
	bResult = true;

cleanup:
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(Secret, sizeof(Secret));
	xrtSecureZero(A1, sizeof(A1));
	xrtSecureZero(A2, sizeof(A2));
	xrtSecureZero(Entity, sizeof(Entity));
	xrtSecureZero(NonceCount, sizeof(NonceCount));
	return bResult;
}



/* 执行请求 response 与服务器 rspauth 的公共入口契约。 */
static bool __xrtHttpDigestProof(
	const xhttpdigestproof* pInput,
	xstrview Method,
	bool bResponse,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpdigestproof Proof;
	char Text[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iRequired;
	bool bWrite;

	if ( !__xrtHttpDigestProofValid(
		pInput, Method, bResponse,
		pOutput, iCapacity, pSize, &Proof
	) || !__xrtHttpDigestProofSizes(
		&Proof, Method, bResponse
	) ) {
		return false;
	}
	iRequired = xrtHttpDigestSize(Proof.Algorithm) * 2u;
	if ( !__xrtHttpDigestOutputPrepare(
		iRequired, pOutput, iCapacity, pSize, &bWrite
	) || !bWrite ) {
		return pOutput == NULL;
	}
	if ( !__xrtHttpDigestProofCalculate(
		&Proof, Method, bResponse, Text
	) ) {
		xrtSecureZero(Text, sizeof(Text));
		return false;
	}
	memcpy(pOutput, Text, iRequired);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	xrtSecureZero(Text, sizeof(Text));
	return true;
}



/* 计算 Authorization 中的 request-digest。 */
XRT_API bool xrtHttpDigestRequest(
	const xhttpdigestproof* pProof,
	xstrview Method,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestProof(
		pProof, Method, false,
		pOutput, iCapacity, pSize
	);
}



/* 计算 Authentication-Info 中的 rspauth。 */
XRT_API bool xrtHttpDigestRspAuth(
	const xhttpdigestproof* pProof,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestProof(
		pProof, (xstrview){ NULL, 0 }, true,
		pOutput, iCapacity, pSize
	);
}



/* 常量时间比较大小写不敏感的十六进制摘要。 */
XRT_API bool xrtHttpDigestEqual(xstrview Left, xstrview Right)
{
	uint32 iDifference = 0;
	uint32 iInvalid = 0;

	if ( !__xrtRangeValid(Left.Data, Left.Size) ||
		!__xrtRangeValid(Right.Data, Right.Size) ||
		(Left.Size == 0) ||
		(Left.Size != Right.Size) ||
		((Left.Size & 1u) != 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		int iLeft = __xrtHttpDigestHexValue((uint8)Left.Data[i]);
		int iRight = __xrtHttpDigestHexValue((uint8)Right.Data[i]);

		if ( (iLeft < 0) || (iRight < 0) ) {
			iInvalid = 1u;
			iLeft = 0;
			iRight = 0;
		}
		iDifference |= (uint32)(iLeft ^ iRight);
	}
	return (iDifference | iInvalid) == 0;
}



#undef XRT_HTTP_DIGEST_MAX_MESSAGE

#endif
