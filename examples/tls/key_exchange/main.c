#include <xrt.h>

#include <stdio.h>
#include <string.h>



/* 选择当前构建中第一个可用命名组。 */
static const xtlsgroupinfo* exampleGroup(void)
{
	static const uint16 Groups[] = {
		XTLS_GROUP_X25519,
		XTLS_GROUP_SECP256R1,
		XTLS_GROUP_X448,
		XTLS_GROUP_SECP384R1
	};

	for ( size_t i = 0; i < (sizeof(Groups) / sizeof(Groups[0])); i++ ) {
		if ( xrtTlsGroupAvailable(Groups[i]) ) {
			return xrtTlsGroupInfo(Groups[i]);
		}
	}
	return NULL;
}



/* 演示调用方按元数据提供缓冲，并完成双向共享秘密校验。 */
int main(void)
{
	const xtlsgroupinfo* pInfo = exampleGroup();
	uint8 ClientPrivate[56];
	uint8 ClientPublic[97];
	uint8 ServerPrivate[56];
	uint8 ServerPublic[97];
	uint8 ClientShared[56];
	uint8 ServerShared[56];

	if ( pInfo == NULL ) {
		return 1;
	}
	if ( !xrtTlsKeyShareGenerate(
		pInfo->Group,
		ClientPrivate, sizeof(ClientPrivate),
		ClientPublic, sizeof(ClientPublic)
	) || !xrtTlsKeyShareGenerate(
		pInfo->Group,
		ServerPrivate, sizeof(ServerPrivate),
		ServerPublic, sizeof(ServerPublic)
	) || !xrtTlsKeyShareDerive(
		pInfo->Group,
		(xbytesview) { ClientPrivate, pInfo->PrivateSize },
		(xbytesview) { ServerPublic, pInfo->PublicSize },
		ClientShared, sizeof(ClientShared)
	) || !xrtTlsKeyShareDerive(
		pInfo->Group,
		(xbytesview) { ServerPrivate, pInfo->PrivateSize },
		(xbytesview) { ClientPublic, pInfo->PublicSize },
		ServerShared, sizeof(ServerShared)
	) ) {
		return 1;
	}
	if ( memcmp(ClientShared, ServerShared, pInfo->SharedSize) != 0 ) {
		return 1;
	}
	printf("group=%u private=%u public=%u shared=%u\n",
		(unsigned)pInfo->Group,
		(unsigned)pInfo->PrivateSize,
		(unsigned)pInfo->PublicSize,
		(unsigned)pInfo->SharedSize);
	return 0;
}
