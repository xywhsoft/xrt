#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)

/* 初始化 transcript 的 SHA-256 状态。 */
void __xrtTlsScheduleSha256Init(xtlstranscript* pTranscript)
{
	xrtSha256Init(&pTranscript->State.Sha256);
}



/* 向 transcript 的 SHA-256 状态追加消息。 */
bool __xrtTlsScheduleSha256Update(
	xtlstranscript* pTranscript,
	xbytesview Message
)
{
	return xrtSha256Update(
		&pTranscript->State.Sha256, Message.Data, Message.Size
	);
}



/* 从 transcript 的 SHA-256 状态输出快照。 */
bool __xrtTlsScheduleSha256Digest(
	const xtlstranscript* pTranscript,
	void* pDigest
)
{
	return xrtSha256Final(&pTranscript->State.Sha256, pDigest);
}



/* 使用 SHA-256 执行 TLS 1.3 HKDF-Extract。 */
bool __xrtTlsScheduleSha256Extract(
	xbytesview Salt,
	xbytesview Ikm,
	void* pSecret
)
{
	return xrtHkdfSha256Extract(
		Salt.Data, Salt.Size, Ikm.Data, Ikm.Size, pSecret
	);
}



/* 使用 SHA-256 执行 TLS 1.3 HKDF-Expand。 */
bool __xrtTlsScheduleSha256Expand(
	xbytesview Secret,
	xbytesview Info,
	void* pOutput,
	size_t iOutputSize
)
{
	return xrtHkdfSha256Expand(
		Secret.Data, Secret.Size, Info.Data, Info.Size,
		pOutput, iOutputSize
	);
}

#endif
