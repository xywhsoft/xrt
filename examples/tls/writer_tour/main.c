/*
 * 范例：tls/writer_tour —— TLS Writer 全接口 + Hello 编解码闭环
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Writer 核心】 xrtTlsWriterInit / Reset / Data
 *   【扩展写出】  xrtTlsWriterExtension / HostName / Protocols /
 *                Ids / ClientVersions / ServerVersion /
 *                ClientKeyShares / ServerKeyShare / RetryGroup /
 *                RetryCookie / PskModes / ClientPsks / ServerPsk
 *   【Hello 闭环】 ClientHelloSize / ClientHelloEncode /
 *                ClientHelloParse（写出→读回）
 *                ServerHelloSize / ServerHelloEncode /
 *                ServerHelloParse
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/writer_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   tls: ext+hostname+protocols+ids written=46 ok
 *   tls: versions+keyshares+retry+psk written=6 ok
 *   tls: client-hello encode/parse roundtrip ok
 *   tls: server-hello encode/parse roundtrip ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

static void examplePut16(uint8* p, uint16 v)
{
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)(v & 0xFFu);
}

int main(void)
{
	uint8 arrExtBuf[256];
	uint8 arrHelloBuf[512];
	uint8 arrRandom[32];
	uint8 arrSession[4] = { 1u, 2u, 3u, 4u };
	uint8 arrCiphers[4];
	uint8 arrComp[2] = { 0u, 1u };
	uint8 arrKey1[4] = { 0xA1u, 0xA2u, 0xA3u, 0xA4u };
	xtlswriter Writer;
	xbytesview Written;
	xtlsclienthello ClientHello;
	xtlsclienthello Parsed;
	xtlsserverhello ServerHello;
	xtlsserverhello ServerParsed;
	xtlskeyshare Share;
	xtlspsk Psks[1];
	static const xbytesview arrProtocols[2] = {
		{ (cbytes)"h2", 2u },
		{ (cbytes)"http/1.1", 9u }
	};
	static const uint16 arrGroups[2] = { 0x001Du, 0x0017u };
	size_t iSize;
	size_t iExtLen;
	int iResult = 1;

	memset(arrRandom, 0x5Au, sizeof(arrRandom));
	examplePut16(arrCiphers, 0x1301u);
	examplePut16(arrCiphers + 2, 0x1302u);

	/* ---- Writer 核心：Init / Data / Reset。 ---- */
	if ( !xrtTlsWriterInit(&Writer, arrExtBuf, sizeof(arrExtBuf)) ) {
		goto Cleanup;
	}
	Written = xrtTlsWriterData(&Writer);
	if ( (Written.Data == NULL) || (Written.Size != 0u) ) {
		goto Cleanup;
	}

	/* ---- 基础扩展族 ---- */
	/* Extension：原始负载扩展（status_request 占位负载——
	 * Writer 拒绝重复扩展类型，ALPN 交给后面的 Protocols）。 */
	{
		static const uint8 arrStatus[1] = { 0u };

		if ( !xrtTlsWriterExtension(&Writer,
				XTLS_EXTENSION_STATUS_REQUEST,
				(xbytesview) { arrStatus, 1u }) ) {
			goto Cleanup;
		}
	}
	/* HostName：SNI 单 host_name 快捷写出。 */
	if ( !xrtTlsWriterHostName(&Writer,
			(xbytesview) { (cbytes)"api", 3u }) ) {
		goto Cleanup;
	}
	/* Protocols：完整 ALPN 列表。 */
	if ( !xrtTlsWriterProtocols(&Writer, arrProtocols, 2u) ) {
		goto Cleanup;
	}
	/* Ids：组列表扩展（16 位向量长前缀自动补）。 */
	if ( !xrtTlsWriterIds(&Writer,
			XTLS_EXTENSION_SUPPORTED_GROUPS, arrGroups, 2u) ) {
		goto Cleanup;
	}
	Written = xrtTlsWriterData(&Writer);
	iExtLen = Written.Size;
	if ( (iExtLen == 0u) || (iExtLen >= sizeof(arrExtBuf)) ) {
		goto Cleanup;
	}
	/* 写出的扩展向量必须能被解析端 Validate。 */
	if ( !xrtTlsExtensionsValidate(Written) ) {
		goto Cleanup;
	}
	printf("tls: ext+hostname+protocols+ids written=%zu ok\n",
		iExtLen);

	/* ---- 版本/keyshare/PSK 族：Reset 后重写。 ---- */
	if ( !xrtTlsWriterReset(&Writer) ) {
		goto Cleanup;
	}
	Written = xrtTlsWriterData(&Writer);
	if ( Written.Size != 0u ) {
		goto Cleanup;
	}
	/* ClientVersions：supported_versions 客户端列表。 */
	if ( !xrtTlsWriterClientVersions(&Writer, arrGroups, 2u) ) {
		goto Cleanup;
	}
	/* ServerVersion 与 ClientVersions 同写 supported_versions 类型——
	 * 单独一次写出验证。 */
	(void)xrtTlsWriterReset(&Writer);
	if ( !xrtTlsWriterServerVersion(&Writer, 0x0304u) ) {
		goto Cleanup;
	}
	/* ClientKeyShares：P-256 共享（key_share 类型同写一次，
	 * ServerKeyShare/RetryGroup 同类型单独验证）。 */
	Share.Group = 0x001Du;
	Share.Key = (xbytesview) { arrKey1, 4u };
	(void)xrtTlsWriterReset(&Writer);
	if ( !xrtTlsWriterClientKeyShares(&Writer, &Share, 1u) ) {
		goto Cleanup;
	}
	(void)xrtTlsWriterReset(&Writer);
	if ( !xrtTlsWriterServerKeyShare(&Writer, &Share) ) {
		goto Cleanup;
	}
	(void)xrtTlsWriterReset(&Writer);
	if ( !xrtTlsWriterRetryGroup(&Writer, 0x001Du) ||
		!xrtTlsWriterRetryCookie(&Writer,
			(xbytesview) { arrKey1, 4u }) ) {
		goto Cleanup;
	}
	/* 汇总缓冲：版本 + keyshare + cookie + psk_modes 不同类型共存。 */
	(void)xrtTlsWriterReset(&Writer);
	if ( !xrtTlsWriterClientVersions(&Writer, arrGroups, 2u) ||
		!xrtTlsWriterClientKeyShares(&Writer, &Share, 1u) ||
		!xrtTlsWriterRetryCookie(&Writer,
			(xbytesview) { arrKey1, 4u }) ) {
		goto Cleanup;
	}
	/* PskModes：[1] 单模式（汇总缓冲内追加，类型独立）。 */
	{
		static const uint8 arrModes[1] = { 1u };

		if ( !xrtTlsWriterPskModes(&Writer, arrModes, 1u) ) {
			goto Cleanup;
		}
	}
	/* ClientPsks：一条 identity+binder（必须末项）。 */
	{
		static uint8 arrIdentity[7] = { 9u, 8u, 7u, 6u, 5u, 4u, 3u };
		static uint8 arrBinder[32];

		memset(arrBinder, 0xB7u, sizeof(arrBinder));
		Psks[0].Identity = (xbytesview) { arrIdentity, 7u };
		Psks[0].ObfuscatedAge = 1000u;
		Psks[0].Binder = (xbytesview) { arrBinder, 32u };
		if ( !xrtTlsWriterClientPsks(&Writer, Psks, 1u) ) {
			goto Cleanup;
		}
		/* ServerPsk 与 ClientPsks 同写 pre_shared_key——单独验证。 */
		(void)xrtTlsWriterReset(&Writer);
		if ( !xrtTlsWriterServerPsk(&Writer, 0u) ) {
			goto Cleanup;
		}
	}
	Written = xrtTlsWriterData(&Writer);
	iExtLen = Written.Size;
	if ( (iExtLen == 0u) || (iExtLen >= sizeof(arrExtBuf)) ||
		!xrtTlsExtensionsValidate(Written) ) {
		goto Cleanup;
	}
	printf("tls: versions+keyshares+retry+psk written=%zu ok\n",
		iExtLen);

	/* ---- ClientHello：写出→Size/Encode→Parse 闭环。 ---- */
	/* 重新只写 SNI + 组两个扩展作 ClientHello 载荷。 */
	(void)xrtTlsWriterReset(&Writer);
	if ( !xrtTlsWriterHostName(&Writer,
			(xbytesview) { (cbytes)"api", 3u }) ||
		!xrtTlsWriterIds(&Writer,
			XTLS_EXTENSION_SUPPORTED_GROUPS, arrGroups, 2u) ) {
		goto Cleanup;
	}
	Written = xrtTlsWriterData(&Writer);
	memset(&ClientHello, 0, sizeof(ClientHello));
	ClientHello.LegacyVersion = 0x0303u;
	ClientHello.Random = (xbytesview) { arrRandom, 32u };
	ClientHello.SessionId = (xbytesview) { arrSession, 4u };
	/* 用 Writer 写出的扩展向量 + 4 字节套件数据。 */
	ClientHello.Extensions = Written;
	ClientHello.CipherSuites.Data.Data = arrCiphers;
	ClientHello.CipherSuites.Data.Size = 4u;
	ClientHello.CompressionMethods =
		(xbytesview) { arrComp, 2u };
	iSize = xrtTlsClientHelloSize(&ClientHello);
	if ( (iSize == 0u) || (iSize >= sizeof(arrHelloBuf)) ) {
		goto Cleanup;
	}
	if ( !xrtTlsClientHelloEncode(&ClientHello, arrHelloBuf,
			iSize) ||
		!xrtTlsClientHelloParse(
			(xbytesview) { arrHelloBuf, iSize }, &Parsed) ||
		(Parsed.LegacyVersion != 0x0303u) ||
		(Parsed.Random.Size != 32u) ||
		(Parsed.SessionId.Size != 4u) ||
		(Parsed.Extensions.Size == 0u) ) {
		goto Cleanup;
	}
	printf("tls: client-hello encode/parse roundtrip ok\n");

	/* ---- ServerHello：服务端不写 SNI（必须为空的确认性扩展）。 */
	(void)xrtTlsWriterReset(&Writer);
	if ( !xrtTlsWriterIds(&Writer,
			XTLS_EXTENSION_SUPPORTED_GROUPS, arrGroups, 2u) ) {
		goto Cleanup;
	}
	memset(&ServerHello, 0, sizeof(ServerHello));
	ServerHello.LegacyVersion = 0x0303u;
	ServerHello.Random = (xbytesview) { arrRandom, 32u };
	ServerHello.SessionId = (xbytesview) { arrSession, 4u };
	ServerHello.CipherSuite = 0x1301u;
	ServerHello.CompressionMethod = 0u;
	ServerHello.Extensions = xrtTlsWriterData(&Writer);
	ServerHello.Retry = false;
	iSize = xrtTlsServerHelloSize(&ServerHello);
	if ( (iSize == 0u) || (iSize >= sizeof(arrHelloBuf)) ) {
		goto Cleanup;
	}
	if ( !xrtTlsServerHelloEncode(&ServerHello, arrHelloBuf,
			iSize) ||
		!xrtTlsServerHelloParse(
			(xbytesview) { arrHelloBuf, iSize },
			&ServerParsed) ||
		(ServerParsed.CipherSuite != 0x1301u) ||
		(ServerParsed.Retry) ||
		(ServerParsed.Extensions.Size == 0u) ) {
		goto Cleanup;
	}
	printf("tls: server-hello encode/parse roundtrip ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
