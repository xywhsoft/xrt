#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/hmac_sha512 —— HMAC-SHA384 流式（复用密钥状态）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHmacSha384Init / Update / Final   流式 HMAC
 * 模块宏：XRT_MODULE_CRYPTO（HMAC 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/hmac_sha512/main.c -lws2_32 -liphlpapi
 * 预期输出（"secret"+"hello world" 的标准 MAC）：
 *   2da3bb177b92aae98c3ab22727d7f60c...
 *
 * Init 一次后的状态可连续算多条消息（密钥已展开）——
 *   高频校验（每请求验签）省去每轮密钥预处理。
 *   文件名虽为 sha512，范例演示同族 384 变体。
 */


/* 复用预计算密钥状态，流式计算 HMAC-SHA384。 */
int main(void)
{
	xhmacsha384 State;
	uint8 arrMac[XRT_SHA384_SIZE];

	if ( !xrtHmacSha384Init(&State, "secret", 6) ||
		 !xrtHmacSha384Update(&State, "hello ", 6) ||
		 !xrtHmacSha384Update(&State, "world", 5) ||
		 !xrtHmacSha384Final(&State, arrMac) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrMac); i++ ) {
		printf("%02x", (unsigned int)arrMac[i]);
	}
	printf("\n");
	return 0;
}
