#include "../test.h"



/* 验证接口快照的全部借用范围和地址元数据。 */
static const xnetinterface* testInterfaceSnapshot(
	const xnetinterfacelist* pList
)
{
	const xnetinterface* pScoped = NULL;
	size_t i;

	testRequire((pList->Count != 0) && (pList->Items != NULL),
		"network interface snapshot is empty");
	for ( i = 0; i < pList->Count; i++ ) {
		const xnetinterface* pInterface = &pList->Items[i];
		size_t j;

		testRequire((pInterface->Name.Data != NULL) &&
			(pInterface->Name.Size != 0) &&
			(pInterface->Name.Data[pInterface->Name.Size] == 0),
			"interface canonical name is invalid");
		testRequire((pInterface->DisplayName.Data != NULL) &&
			(pInterface->DisplayName.Size != 0) &&
			(pInterface->DisplayName.Data[pInterface->DisplayName.Size] == 0),
			"interface display name is invalid");
		testRequire((pInterface->IPv4Index != 0) ||
			(pInterface->IPv6Index != 0),
			"interface has no usable index");
		testRequire((pInterface->HardwareAddress.Size == 0) ||
			(pInterface->HardwareAddress.Data != NULL),
			"interface hardware address view is invalid");
		testRequire((pInterface->AddressCount == 0) ||
			(pInterface->Addresses != NULL),
			"interface address array is invalid");
		for ( j = 0; j < pInterface->AddressCount; j++ ) {
			const xnetinterfaceaddress* pAddress = &pInterface->Addresses[j];
			uint8 iMaximum = pAddress->Address.Family == XNET_FAMILY_IPV4 ?
				32u : 128u;

			testRequire((pAddress->Address.Family == XNET_FAMILY_IPV4) ||
				(pAddress->Address.Family == XNET_FAMILY_IPV6),
				"interface contains an unsupported address family");
			testRequire(pAddress->Address.Port == 0,
				"interface address unexpectedly contains a port");
			testRequire((pAddress->PrefixLength <= iMaximum) ||
				(pAddress->PrefixLength == XNET_INTERFACE_PREFIX_UNKNOWN),
				"interface prefix length is invalid");
		}
		if ( (pScoped == NULL) && (pInterface->IPv6Index != 0) &&
			(memchr(pInterface->Name.Data, '%', pInterface->Name.Size) == NULL) ) {
			pScoped = pInterface;
		}
	}
	return pScoped;
}



/* 名称与索引必须按地址族双向稳定转换并遵守文本缓冲契约。 */
static void testInterfaceNames(const xnetinterfacelist* pList)
{
	const xnetinterface* pInterface = &pList->Items[0];
	xnetfamily Family = pInterface->IPv6Index != 0 ?
		XNET_FAMILY_IPV6 : XNET_FAMILY_IPV4;
	uint32 iIndex = Family == XNET_FAMILY_IPV6 ?
		pInterface->IPv6Index : pInterface->IPv4Index;
	char sLocal[128];
	char* sName = sLocal;
	char sSmall[1];
	size_t iRequired;

	testRequire(xrtNetInterfaceIndex(pInterface->Name.Data, Family) == iIndex,
		"interface canonical name did not round trip");
	testRequire(xrtNetInterfaceIndex(
		pInterface->DisplayName.Data, Family
	) == iIndex, "interface display name did not resolve");
	iRequired = xrtNetInterfaceName(iIndex, Family, NULL, 0);
	testRequire(iRequired == pInterface->Name.Size,
		"interface name size query mismatch");
	if ( iRequired >= sizeof(sLocal) ) {
		sName = (char*)xrtMalloc(iRequired + 1u);
		testRequire(sName != NULL,
			"interface name test buffer allocation failed");
	}
	testRequire(xrtNetInterfaceName(
		iIndex, Family, sName, iRequired + 1u
	) == iRequired, "interface name output failed");
	testRequire(strcmp(sName, pInterface->Name.Data) == 0,
		"interface name output mismatch");
	if ( sName != sLocal ) {
		xrtFree(sName);
	}

	xrtClearError();
	sSmall[0] = 'x';
	testRequire(xrtNetInterfaceName(
		iIndex, Family, sSmall, sizeof(sSmall)
	) == iRequired, "small interface name buffer lost required length");
	testRequire((sSmall[0] == 0) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_BUFFER),
		"small interface name buffer contract mismatch");
}



/* IPv6 地址和端点解析必须复用接口名称 Scope。 */
static void testInterfaceScope(const xnetinterface* pInterface)
{
	char sAddress[1024];
	char sEndpoint[1024];
	xnetaddr Address;
	int iWritten;

	if ( pInterface == NULL ) {
		return;
	}
	iWritten = snprintf(sAddress, sizeof(sAddress),
		"fe80:0000:0000:0000:0000:0000:0000:0001%%%s",
		pInterface->Name.Data);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sAddress)),
		"scoped address test input overflowed");
	testRequire(xrtNetAddrParse(&Address, sAddress, 0),
		"named IPv6 scope was rejected");
	testRequire((Address.Family == XNET_FAMILY_IPV6) &&
		(Address.Scope == pInterface->IPv6Index),
		"named IPv6 scope resolved to the wrong index");

	iWritten = snprintf(sEndpoint, sizeof(sEndpoint), "[%s]:443", sAddress);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sEndpoint)),
		"scoped endpoint test input overflowed");
	testRequire(xrtNetAddrParseEndpoint(&Address, sEndpoint, 0),
		"named IPv6 endpoint scope was rejected");
	testRequire((Address.Scope == pInterface->IPv6Index) &&
		(Address.Port == 443),
		"named IPv6 endpoint fields mismatch");
}



/* 参数、缺失名称和缺失索引必须返回稳定结构化错误。 */
static void testInterfaceInvalid(void)
{
	xnetinterfacelist List;
	char sName[8];

	xrtClearError();
	testRequire(!xrtNetInterfaces(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null interface list output was accepted");
	testRequire(xrtNetInterfaceIndex(NULL, XNET_FAMILY_UNSPEC) == 0,
		"null interface name was accepted");
	testRequire(xrtNetInterfaceIndex(
		"xrt-interface-that-does-not-exist", XNET_FAMILY_UNSPEC
	) == 0, "missing interface name unexpectedly resolved");
	testRequire(xrtErrorCode(xrtGetError()) == XNET_ERROR_INTERFACE_INDEX,
		"missing interface name error mismatch");
	testRequire(xrtNetInterfaceName(
		0, XNET_FAMILY_UNSPEC, sName, sizeof(sName)
	) == XRT_NPOS, "zero interface index was accepted");

	memset(&List, 0, sizeof(List));
	xrtNetInterfacesFree(&List);
}



/* 便捷地址查询必须返回可格式化、端口为零的稳定本机单播地址。 */
static void testInterfaceLocalAddress(void)
{
	xnetaddr Address;
	char sText[96];

	testRequire(xrtNetLocalAddress(&Address, XNET_FAMILY_UNSPEC),
		"preferred local address query failed");
	testRequire(((Address.Family == XNET_FAMILY_IPV4) ||
		(Address.Family == XNET_FAMILY_IPV6)) && (Address.Port == 0),
		"preferred local address fields are invalid");
	testRequire(xrtNetAddrText(
		&Address, sText, sizeof(sText)
	) != XRT_NPOS, "preferred local address did not format");

	xrtClearError();
	testRequire(!xrtNetLocalAddress(NULL, XNET_FAMILY_UNSPEC) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null preferred local address output was accepted");
	xrtClearError();
	testRequire(!xrtNetLocalAddress(&Address, (xnetfamily)99) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid preferred local address family was accepted");
}



/* 主机名查询支持测量、完整输出、安全截断和分配自由的基础路径。 */
static void testInterfaceHostName(void)
{
	char sName[1024];
	char sSmall[1];
	size_t iRequired;

	iRequired = xrtNetHostName(NULL, 0);
	testRequire((iRequired != XRT_NPOS) && (iRequired != 0) &&
		(iRequired < sizeof(sName)), "host name size query failed");
	testRequire(xrtNetHostName(
		sName, sizeof(sName)
	) == iRequired, "host name output failed");
	testRequire((strlen(sName) == iRequired) && (sName[0] != 0),
		"host name output length mismatch");

	xrtClearError();
	sSmall[0] = 'x';
	testRequire(xrtNetHostName(
		sSmall, sizeof(sSmall)
	) == iRequired, "small host name buffer lost required length");
	testRequire((sSmall[0] == 0) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_BUFFER),
		"small host name buffer contract mismatch");
}



/* 硬件地址查询必须保持二进制大小语义并允许无硬件接口环境。 */
static void testInterfaceLocalHardware(void)
{
	uint8 sSmall[1] = { 0xa5u };
	size_t iRequired;

	xrtClearError();
	iRequired = xrtNetLocalHardware(NULL, 0);
	if ( iRequired == XRT_NPOS ) {
		testRequire(xrtErrorCode(xrtGetError()) ==
			XNET_ERROR_INTERFACE_HARDWARE,
			"missing local hardware address error mismatch");
		return;
	}
	testRequire(iRequired != 0,
		"local hardware address reported an empty value");
	xrtClearError();
	testRequire(xrtNetLocalHardware(
		sSmall, 0
	) == iRequired, "small hardware buffer lost required size");
	testRequire((sSmall[0] == 0xa5u) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_BUFFER),
		"small hardware buffer contract mismatch");
}



/* 接口模块只观察本机接口，不访问任何远端网络。 */
int main(void)
{
	xnetinterfacelist List;
	const xnetinterface* pScoped;

	memset(&List, 0, sizeof(List));
	testRequire(xrtNetInterfaces(&List),
		"network interface enumeration failed");
	pScoped = testInterfaceSnapshot(&List);
	testInterfaceNames(&List);
	testInterfaceScope(pScoped);
	xrtNetInterfacesFree(&List);
	testRequire((List.Items == NULL) && (List.Count == 0),
		"interface snapshot free did not clear the list");
	xrtNetInterfacesFree(&List);
	testInterfaceInvalid();
	testInterfaceLocalAddress();
	testInterfaceHostName();
	testInterfaceLocalHardware();
	return 0;
}
