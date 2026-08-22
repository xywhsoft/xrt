#include <stdio.h>

#include <xrt.h>



/* 列出本机接口以及可用于绑定、Scope 和多播的地址元数据。 */
int main(void)
{
	xnetinterfacelist List;
	size_t i;

	if ( !xrtNetInterfaces(&List) ) {
		return 1;
	}
	for ( i = 0; i < List.Count; i++ ) {
		const xnetinterface* pInterface = &List.Items[i];
		size_t j;

		printf("%.*s (%.*s) index4=%u index6=%u mtu=%u\n",
			(int)pInterface->Name.Size, pInterface->Name.Data,
			(int)pInterface->DisplayName.Size, pInterface->DisplayName.Data,
			(unsigned int)pInterface->IPv4Index,
			(unsigned int)pInterface->IPv6Index,
			(unsigned int)pInterface->Mtu);
		for ( j = 0; j < pInterface->AddressCount; j++ ) {
			const xnetinterfaceaddress* pAddress = &pInterface->Addresses[j];
			char sAddress[96];

			if ( xrtNetAddrText(
				&pAddress->Address, sAddress, sizeof(sAddress)
			) == XRT_NPOS ) {
				xrtNetInterfacesFree(&List);
				return 1;
			}
			printf("  %s/%u\n", sAddress,
				(unsigned int)pAddress->PrefixLength);
		}
	}
	xrtNetInterfacesFree(&List);
	return 0;
}
