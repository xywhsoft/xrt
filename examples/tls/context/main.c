#include <xrt/tls.h>

#include <stdio.h>



/*
 * 范例：tls/context —— 共享上下文：策略快照 + 资源限制
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsPolicyInit + 自定义版本/套件/签名数组
 *   xrtTlsContextConfigInit + Config.Policy / Limits.PlainLimit
 *   xrtTlsContextCreate / Release   创建共享上下文（引用计数）
 *   xrtTlsContextPolicy / ContextLimits   只读回查
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/context/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   versions=1 plain-limit=524288
 *
 * Context 的意义：Create 时对策略做一次"自有快照"——
 *   之后调用方改栈上数组不影响已建上下文；N 个连接共享
 *   一个 Context，握手期零重复初始化。
 *   PlainLimit（明文记录上限）是资源防线：恶意对端
 *   超限直接断连（输出 512KB 即本例设置值）。
 */


/* 创建一份可由多个连接共享的自有策略与限制快照。 */
int main(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_13 };
	static const xtlscipher Ciphers[] = {
		XTLS_AES_128_GCM_SHA256,
		XTLS_CHACHA20_POLY1305_SHA256
	};
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_ED25519,
		XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	};
	xtlspolicy Policy;
	xtlscontextconfig Config;
	xtlscontext* pContext;

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions;
	Policy.VersionCount = 1u;
	Policy.Ciphers = Ciphers;
	Policy.CipherCount = 2u;
	Policy.Signatures = Signatures;
	Policy.SignatureCount = 3u;

	xrtTlsContextConfigInit(&Config);
	Config.Policy = &Policy;
	Config.Limits.PlainLimit = 512u * 1024u;
	pContext = xrtTlsContextCreate(&Config);
	if ( pContext == NULL ) {
		return 1;
	}
	printf(
		"versions=%zu plain-limit=%zu\n",
		xrtTlsContextPolicy(pContext)->VersionCount,
		xrtTlsContextLimits(pContext)->PlainLimit
	);
	xrtTlsContextRelease(pContext);
	return 0;
}
