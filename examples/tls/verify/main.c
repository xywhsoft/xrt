#include <xrt/tls_verify.h>

#include <stdio.h>



/*
 * 范例：tls/verify —— 自定义验证器：证书决策权交给应用
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsVerifierConfigInit / VerifierCreate / VerifierRelease
 *   Config.Verify = 回调        应用接管信任决策
 *   xtlspeer                     对端信息（证书链等）
 *   XTLS_VERIFY_ACCEPT / REJECT 决策枚举
 * 模块宏：XRT_MODULE_TLS_VERIFY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/verify/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   TLS verifier is ready
 *
 * 验证器只管"信任决策"：协议层签名、Finished 校验仍由
 *   XRT 完成——回调拿到的是已通过密码学校验的对端结构。
 *   证书固定（pinning）、内嵌私有 CA、审计日志都在这层做；
 *   本例演示最小决策（有证书即收）。验证器可被多个客户端共享。
 */


/* 示例回调以应用自有的证书固定策略接管信任决策。 */
static xtlsverifydecision exampleTlsVerify(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	return pPeer->CertificateCount != 0 ?
		XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}



/* 创建可供多个 TLS 客户端共享的自定义验证器。 */
int main(void)
{
	xtlsverifierconfig Config;
	xtlsverifier* pVerifier;

	xrtTlsVerifierConfigInit(&Config);
	Config.Verify = exampleTlsVerify;
	pVerifier = xrtTlsVerifierCreate(&Config);
	if ( pVerifier == NULL ) {
		return 1;
	}
	printf("TLS verifier is ready\n");
	xrtTlsVerifierRelease(pVerifier);
	return 0;
}
