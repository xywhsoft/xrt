#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供无固定数组的 TLS Hello writer。 */
int main(void)
{
	static const uint8 Ciphers[] = { 0x13, 0x01 };
	static const uint8 Compression[] = { 0 };
	static const uint16 Versions[] = {
		XTLS_VERSION_13, XTLS_VERSION_12
	};
	uint8 Random[32] = { 0 };
	uint8 Extensions[32];
	uint8 Body[96];
	xtlswriter Writer;
	xtlsclienthello Hello;
	xtlsclienthello Parsed;
	size_t iSize;

	if ( !xrtTlsWriterInit(&Writer, Extensions, sizeof(Extensions)) ||
		!xrtTlsWriterClientVersions(&Writer, Versions, 2u) ) {
		return 1;
	}
	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = (xbytesview) { Random, sizeof(Random) };
	Hello.CipherSuites.Data = (xbytesview) {
		Ciphers, sizeof(Ciphers)
	};
	Hello.CompressionMethods = (xbytesview) {
		Compression, sizeof(Compression)
	};
	Hello.Extensions = xrtTlsWriterData(&Writer);
	iSize = xrtTlsClientHelloSize(&Hello);
	return (iSize != 0) && xrtTlsClientHelloEncode(
		&Hello, Body, sizeof(Body)
	) && xrtTlsClientHelloParse(
		(xbytesview) { Body, iSize }, &Parsed
	) ? 0 : 1;
}
