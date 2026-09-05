#include <stdio.h>
#include <xrt.h>



/*
 * 范例：crypto/ed25519_sign —— Ed25519 签名（展开密钥复用）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtEd25519KeyInit / SignKey / KeyClear   带状态签名
 * 模块宏：XRT_MODULE_CRYPTO（ED25519 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/ed25519_sign/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   signed: yes
 *
 * Key 状态预展开签名中间量：同钥多签（证书签发、
 *   批量授权）比逐次入口快；KeyClear 抹除敏感中间态。
 *   64 字节签名、无随机性需求（确定性签名）。
 */


/* 使用可复用展开密钥签署一条消息。 */
int main(void)
{
	static const char Message[] = "xrt ed25519";
	uint8 Seed[XRT_ED25519_SEED_SIZE] = { 0 };
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	xed25519key Key;
	bool bSigned;

	if ( !xrtEd25519KeyInit(&Key, Seed) ) {
		return 1;
	}
	bSigned = xrtEd25519SignKey(
		&Key, Message, sizeof(Message) - 1u, Signature
	);
	xrtEd25519KeyClear(&Key);
	printf("signed: %s\n", bSigned ? "yes" : "no");
	return bSigned ? 0 : 1;
}
