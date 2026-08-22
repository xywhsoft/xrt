#include "../test.h"

#include <xrt/http_upgrade.h>



/* 验证规范写出、短缓冲原子性、Build 和输入别名拒绝。 */
int main(void)
{
	static const xhttpupgradeitem Upgrades[] = {
		{ XRT_STR_INIT("websocket"), { NULL, 0 } },
		{ XRT_STR_INIT("HTTP"), XRT_STR_INIT("2.0") }
	};
	static const xhttpupgradeitem EmptyProtocol = {
		{ NULL, 0 }, { NULL, 0 }
	};
	static const xhttpupgradeitem InvalidVersion = {
		XRT_STR_INIT("HTTP"), XRT_STR_INIT("2 0")
	};
	static const xhttpupgradeitem WrappedProtocol = {
		{ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u },
		{ NULL, 0 }
	};
	uint8 DescriptorStorage[sizeof(Upgrades) + 1u];
	const xhttpupgradeitem* pUnaligned;
	char sOutput[32];
	str sBuilt;
	size_t iSize;

	testRequire(xrtHttpUpgradeWrite(
		Upgrades, 2u, NULL, 0, &iSize
	) && (iSize == 19u),
		"HTTP Upgrade write size mismatch");
	memset(sOutput, '#', sizeof(sOutput));
	testRequire(!xrtHttpUpgradeWrite(
		Upgrades, 2u, sOutput, 18u, &iSize
	) && (iSize == 19u) &&
		(sOutput[0] == '#') && (sOutput[17] == '#'),
		"HTTP Upgrade short write was not atomic");
	xrtClearError();
	testRequire(xrtHttpUpgradeWrite(
		Upgrades, 2u, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 19u) &&
		(memcmp(sOutput, "websocket, HTTP/2.0", iSize) == 0),
		"HTTP Upgrade canonical write mismatch");
	testRequire(!xrtHttpUpgradeElementWrite(
		Upgrades,
		(void*)Upgrades[0].Protocol.Data,
		Upgrades[0].Protocol.Size,
		&iSize
	), "HTTP Upgrade writer accepted overlapping output");
	xrtClearError();
	memset(sOutput, '#', sizeof(sOutput));
	iSize = 47u;
	testRequire(!xrtHttpUpgradeElementWrite(
		&EmptyProtocol, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 47u) && (sOutput[0] == '#'),
		"HTTP Upgrade writer accepted an empty protocol");
	xrtClearError();
	testRequire(!xrtHttpUpgradeElementWrite(
		&InvalidVersion, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 47u) && (sOutput[0] == '#'),
		"HTTP Upgrade writer accepted an invalid version");
	xrtClearError();
	testRequire(!xrtHttpUpgradeElementWrite(
		&WrappedProtocol, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 47u) && (sOutput[0] == '#') &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Upgrade writer accepted a wrapped protocol range");
	xrtClearError();
	memcpy(DescriptorStorage + 1u, Upgrades, sizeof(Upgrades));
	pUnaligned = (const xhttpupgradeitem*)(const void*)(
		DescriptorStorage + 1u
	);
	testRequire(xrtHttpUpgradeWrite(
		pUnaligned, 2u, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 19u) &&
		(memcmp(sOutput, "websocket, HTTP/2.0", iSize) == 0),
		"HTTP Upgrade writer rejected unaligned descriptors");
	xrtClearError();
	sBuilt = xrtHttpUpgradeBuild(Upgrades, 2u, &iSize);
	testRequire((sBuilt != NULL) && (iSize == 19u) &&
		(strcmp(sBuilt, "websocket, HTTP/2.0") == 0),
		"HTTP Upgrade Build mismatch");
	xrtFree(sBuilt);
	puts("[PASS] http_upgrade_write");
	return 0;
}
