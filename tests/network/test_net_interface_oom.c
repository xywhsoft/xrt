#include "../test_allocator.h"



/* 接口快照的拥有存储分配失败时不得修改调用方输出。 */
int main(void)
{
	xnetinterfacelist List;
	xnetinterfacelist Saved;
	xnetaddr Address;
	xnetaddr AddressSaved;
	char sName[8] = "stable";
	char sNameSaved[8];

	List.Items = (const xnetinterface*)(uintptr_t)1;
	List.Count = 7;
	Saved = List;
	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");
	testRequire(!xrtNetInterfaces(&List),
		"interface snapshot unexpectedly survived OOM");
	testRequire((List.Items == Saved.Items) && (List.Count == Saved.Count),
		"failed interface snapshot modified output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"interface snapshot OOM error mismatch");

	memset(&Address, 0xa5, sizeof(Address));
	AddressSaved = Address;
	testRequire(!xrtNetLocalAddress(
		&Address, XNET_FAMILY_UNSPEC
	), "local address unexpectedly survived OOM");
	testRequire(memcmp(
		&Address, &AddressSaved, sizeof(Address)
	) == 0, "failed local address query modified output");

	memcpy(sNameSaved, sName, sizeof(sName));
	testRequire(xrtNetHostName(
		sName, sizeof(sName)
	) == XRT_NPOS, "host name unexpectedly survived OOM");
	testRequire(memcmp(sName, sNameSaved, sizeof(sName)) == 0,
		"failed host name query modified output");
	return 0;
}
