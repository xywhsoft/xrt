#include <stdio.h>

#include <xrt.h>



/*
 * 范例：tls/negotiate —— 版本与套件协商：按本地策略选交集
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsVersionSelect   对端 offered ∩ 本地支持 → 最高版本
 *   xrtTlsCipherSelect    版本 + 身份类型 + 交集 → 套件
 *   xrtTlsVersionName / CipherName   枚举 → 展示名
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/negotiate/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   version=TLS 1.3 cipher=TLS_AES_128_GCM_SHA256
 *
 * 协商读法：对端给 1.3+1.2、本地只收 1.3 → 选 1.3；
 *   套件按"对端 offered 顺序 + 本地支持 + 身份兼容"
 *   三重过滤（RSA 身份排除纯 ECDSA 套件）。
 *   服务端每连接都跑这两步——本地策略数组就是安全基线。
 */


/* 按应用策略选择对端提供的 TLS 版本和密码套件。 */
int main(void)
{
	static const uint8 VersionData[] = {
		0x03, 0x04, 0x03, 0x03
	};
	static const uint8 CipherData[] = {
		0x13, 0x01, 0x13, 0x02, 0xC0, 0x2F
	};
	static const xtlsversion Versions[] = {
		XTLS_VERSION_13, XTLS_VERSION_12
	};
	static const xtlscipher Ciphers[] = {
		XTLS_CHACHA20_POLY1305_SHA256,
		XTLS_AES_128_GCM_SHA256,
		XTLS_AES_256_GCM_SHA384,
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256
	};
	xtlsids OfferedVersions = {
		{ VersionData, sizeof(VersionData) }
	};
	xtlsids OfferedCiphers = {
		{ CipherData, sizeof(CipherData) }
	};
	xtlsversion Version;
	xtlscipher Cipher;

	if ( xrtTlsVersionSelect(
		&OfferedVersions, Versions, 2, &Version
	) != XTLS_ITEM_VALUE ) {
		return 1;
	}
	if ( xrtTlsCipherSelect(
		Version, &OfferedCiphers, XTLS_IDENTITY_RSA,
		Ciphers, sizeof(Ciphers) / sizeof(Ciphers[0]), &Cipher
	) != XTLS_ITEM_VALUE ) {
		return 1;
	}
	printf(
		"version=%s cipher=%s\n",
		xrtTlsVersionName(Version), xrtTlsCipherName(Cipher)
	);
	return 0;
}
