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



/*
 * 范例：tls/key_exchange —— 密钥交换原语：生成 / 派生 / 双向一致
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsGroupAvailable / GroupInfo   当前构建可用的命名组
 *   xrtTlsKeyShareGenerate   生成 (私钥, 公钥) 对
 *   xrtTlsKeyShareDerive     私钥 + 对端公钥 → 共享秘密
 *   xtlsgroupinfo            元数据：三种缓冲的精确尺寸
 * 模块宏：XRT_MODULE_TLS（依赖 CRYPTO）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/key_exchange/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   group=29 private=32 public=32 shared=32
 *
 * group=29 即 x25519（首选组，三尺寸全 32 字节）。
 *   元数据驱动缓冲：调用方按 Info 的精确尺寸开数组，
 *   不猜常数。双向 Derive 结果 memcmp 相等即 ECDH 语义
 *   成立——TLS 1.3 握手密钥就是这么算出来的。
 */


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
