#ifndef XHTTP_POLICY_HTTP_CLIENT_POLICY_H
#define XHTTP_POLICY_HTTP_CLIENT_POLICY_H

#include <xhttp-policy/export.h>
#include <xrt/http_auth_policy.h>

#include <xrt/http_client.h>
#include <xrt/crypto.h>
#include <xrt/codec.h>
#include <xrt/sync.h>
#include <xrt/random.h>

XRT_EXTERN_C_BEGIN


#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION))
	#error "XRT HTTP/1 Digest prepare support requires request prepare and Digest session support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT))
	#error "XRT HTTP client Digest selection requires response Digest and client protocol support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION))
	#error "XRT HTTP client Digest session response support requires Digest info and session support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION)

/*
	使用最终 request-target 生成源站 Digest 凭据并冻结请求计划。
	成功时发布由调用方释放的 Exchange；失败时请求对象不变且输出为空。
	ppExchange 允许未对齐，但不得覆盖请求或 Digest session 拥有的存储。
*/
XHTTP_POLICY_API xhttp1requestplan* xrtHttp1RequestPrepareDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
);



/* 使用同一准备契约生成 Proxy-Authorization Digest 字段。 */
XHTTP_POLICY_API xhttp1requestplan* xrtHttp1RequestPrepareProxyDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
);

#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE)

/*
	按线路顺序选择首个满足本地策略的源站 Digest challenge。
	查询模式完成全部候选验证并返回所选项的精确解码长度。
	全部写出区允许未对齐，但必须互不覆盖且不得覆盖响应拥有的存储。
*/
XHTTP_POLICY_API xhttpnext xrtHttpResponseDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
);



/* 选择首个满足本地策略的代理 Digest challenge；契约与源站入口一致。 */
XHTTP_POLICY_API xhttpnext xrtHttpResponseProxyDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
);

#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)

/*
	解析并验证源站 Digest 回执。
	END 表示字段缺失；ITEM 时 pCheck 区分无效、有效、更新与已取代状态。
	pCheck 允许未对齐，但不得覆盖响应、会话或 Exchange 拥有的存储。
*/
XHTTP_POLICY_API xhttpnext xrtHttpResponseDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
);



/* 解析并验证代理 Digest 回执；返回与状态契约和源站入口一致。 */
XHTTP_POLICY_API xhttpnext xrtHttpResponseProxyDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
);

#endif

XRT_EXTERN_C_END

#endif
