/*
 * 范例：network/proxy_tour —— 代理对象/握手自省/Dial 补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【对象】    xrtNetProxyRetain / Info（六字段视图）
 *   【握手自省】 xrtNetProxyHandshakeState / Bound / Error / Code
 *   【Dial 补集】 xrtNetProxyDialRef / State / Error / Stats
 * 模块宏：XRT_MODULE_NET_PROXY（+ HANDSHAKE/DIAL）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/proxy_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   proxy: retain+info type=1 host=socks5.local port=1080
 *   proxy: handshake write-state output>0 bound/http=reject
 *   proxy: dial ref/state=4 stats error-path ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)
#define EXAMPLE_TIMEOUT_US	UINT64_C(5000000)

/* Dial 完成交接块。 */
typedef struct examplepdial {
	volatile xnetresult Result;
	volatile bool bDone;
} examplepdial;

static void exampleDialDone(xnetproxydial* pDial,
	xnetresult Result, xnetstream* pStream,
	const xerror* pError, ptr pData)
{
	examplepdial* pJob = (examplepdial*)pData;

	(void)pDial;
	(void)pStream;
	(void)pError;
	pJob->Result = Result;
	pJob->bDone = true;
}

static bool exampleSpinUntil(volatile bool* pFlag)
{
	xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_TIMEOUT_US);

	while ( !*pFlag ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

int main(void)
{
	xnetproxyconfig ProxyConfig;
	xnetproxyhandshakeconfig HsConfig;
	xnetproxydialconfig DialConfig;
	xnetengineconfig EngineConfig;
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xnetproxy* pProxy = NULL;
	xnetproxy* pRetained = NULL;
	xnetproxyhandshake* pHandshake = NULL;
	xnetproxydial* pDial = NULL;
	xnetproxydial* pDialRef = NULL;
	examplepdial DialJob;
	xnetproxyinfo Info;
	xnetproxyendpoint Endpoint;
	xnetproxydialstats DialStats;
	xnetspan Output;
	uint32 iCode = 0;
	int iResult = 1;

	memset(&DialJob, 0, sizeof(DialJob));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;

	/* ---- 对象族：Create + Retain + Info ---- */
	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_SOCKS5;
	ProxyConfig.Host = SV("socks5.local");
	ProxyConfig.Port = 1080u;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	if ( (pProxy == NULL) ||
		((pRetained = xrtNetProxyRetain(pProxy)) != pProxy) ) {
		goto Cleanup;
	}
	if ( !xrtNetProxyInfo(pProxy, &Info) ||
		(Info.Type != XNET_PROXY_SOCKS5) ||
		(Info.Host.Size != 12u) ||
		(memcmp(Info.Host.Data, "socks5.local", 12u) != 0) ||
		(Info.Port != 1080u) ) {
		goto Cleanup;
	}
	printf("proxy: retain+info type=1 host=socks5.local port=1080\n");

	/* ---- 握手自省：创建即 WRITE + 首段输出非空 ---- */
	pEngine = xrtNetEngineCreate(&EngineConfig);
	pResolver = xrtNetResolverCreate(NULL);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ||
		(pResolver == NULL) ) {
		goto Cleanup;
	}
	xrtNetProxyHandshakeConfigInit(&HsConfig);
	HsConfig.Proxy = pProxy;
	HsConfig.TargetHost = SV("example.org");
	HsConfig.TargetPort = 443u;
	pHandshake = xrtNetProxyHandshakeCreate(&HsConfig);
	if ( (pHandshake == NULL) ||
		(xrtNetProxyHandshakeState(pHandshake) !=
			XNET_PROXY_HANDSHAKE_WRITE) ||
		!xrtNetProxyHandshakeOutput(pHandshake, &Output) ||
		(Output.Size == 0u) ||
		(Output.Data == NULL) ) {
		goto Cleanup;
	}
	/* 确认首段输出推进状态。 */
	(void)xrtNetProxyHandshakeSent(pHandshake, Output.Size);
	/* 未完成握手：Bound 假 / Code 假 / Error 空。 */
	if ( xrtNetProxyHandshakeBound(pHandshake, &Endpoint) ||
		xrtNetProxyHandshakeCode(pHandshake, &iCode) ||
		(xrtNetProxyHandshakeError(pHandshake) != NULL) ) {
		goto Cleanup;
	}
	printf("proxy: handshake write-state output>0 bound/http=reject\n");
	xrtNetProxyHandshakeDestroy(pHandshake);
	pHandshake = NULL;

	/* ---- Dial 补集：Ref/State/Stats/Error ---- */
	xrtNetProxyDialConfigInit(&DialConfig);
	pDial = xrtNetProxyDial(pEngine, pResolver, pProxy,
		"example.org", 443u, &DialConfig, NULL, NULL,
		exampleDialDone, &DialJob);
	if ( pDial == NULL ) {
		goto Cleanup;
	}
	pDialRef = xrtNetProxyDialRef(pDial);
	if ( (pDialRef != pDial) ||
		!exampleSpinUntil(&DialJob.bDone) ||
		(DialJob.Result == XNET_RESULT_OK) ) {
		goto Cleanup;  /* socks5.local 不可解析：必然失败路径 */
	}
	{
		xnetproxydialstate State = xrtNetProxyDialState(pDial);

		if ( (State != XNET_PROXY_DIAL_FAILED) ||
			(xrtNetProxyDialError(pDial) == NULL) ||
			!xrtNetProxyDialStats(pDial, &DialStats) ||
			(DialStats.State != XNET_PROXY_DIAL_FAILED) ) {
			goto Cleanup;
		}
	}
	printf("proxy: dial ref/state=%d stats error-path ok\n",
		(int)XNET_PROXY_DIAL_FAILED);
	iResult = 0;

Cleanup:
	xrtNetProxyDialDestroy(pDialRef);
	xrtNetProxyDialDestroy(pDial);
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetResolverDestroy(pResolver);
	xrtNetEngineDestroy(pEngine);
	xrtNetProxyRelease(pRetained);
	xrtNetProxyRelease(pProxy);
	return iResult;
}
