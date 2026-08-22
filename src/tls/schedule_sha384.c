#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)

/* 初始化 transcript 的 SHA-384 状态。 */
void __xrtTlsScheduleSha384Init(xtlstranscript* pTranscript)
{
	xrtSha384Init(&pTranscript->State.Sha384);
}



/* 向 transcript 的 SHA-384 状态追加消息。 */
bool __xrtTlsScheduleSha384Update(
	xtlstranscript* pTranscript,
	xbytesview Message
)
{
	return xrtSha384Update(
		&pTranscript->State.Sha384, Message.Data, Message.Size
	);
}



/* 从 transcript 的 SHA-384 状态输出快照。 */
bool __xrtTlsScheduleSha384Digest(
	const xtlstranscript* pTranscript,
	void* pDigest
)
{
	return xrtSha384Final(&pTranscript->State.Sha384, pDigest);
}



/* 使用 SHA-384 执行 TLS 1.3 HKDF-Extract。 */
bool __xrtTlsScheduleSha384Extract(
	xbytesview Salt,
	xbytesview Ikm,
	void* pSecret
)
{
	return xrtHkdfSha384Extract(
		Salt.Data, Salt.Size, Ikm.Data, Ikm.Size, pSecret
	);
}



/* 使用 SHA-384 执行 TLS 1.3 HKDF-Expand。 */
bool __xrtTlsScheduleSha384Expand(
	xbytesview Secret,
	xbytesview Info,
	void* pOutput,
	size_t iOutputSize
)
{
	return xrtHkdfSha384Expand(
		Secret.Data, Secret.Size, Info.Data, Info.Size,
		pOutput, iOutputSize
	);
}

#endif
