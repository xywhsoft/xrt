#include <stdio.h>
#include <xrt.h>



/*
 * 范例：crypto/poly1305 —— Poly1305 消息认证码（RFC 8439 向量）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPoly1305Init / Update / Final   流式三段式
 *   xrtPoly1305                        一次性
 * 模块宏：XRT_MODULE_CRYPTO（POLY1305 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/poly1305/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   Poly1305 vector: valid
 *
 * 期望标签来自 RFC 8439 §2.5.2 标准测试向量——
 *   流式与一次性两条路径都命中同一向量。
 *   单次密钥只签一条消息（one-time MAC），
 *   组合用法是"ChaCha20 派生密钥 → 签本条消息"。
 */


/* 使用 RFC 8439 固定向量展示流式与一次性 Poly1305。 */
int main(void)
{
	static const uint8 Key[XRT_POLY1305_KEY_SIZE] = {
		0x85, 0xD6, 0xBE, 0x78, 0x57, 0x55, 0x6D, 0x33,
		0x7F, 0x44, 0x52, 0xFE, 0x42, 0xD5, 0x06, 0xA8,
		0x01, 0x03, 0x80, 0x8A, 0xFB, 0x0D, 0xB2, 0xFD,
		0x4A, 0xBF, 0xF6, 0xAF, 0x41, 0x49, 0xF5, 0x1B
	};
	static const uint8 Expected[XRT_POLY1305_TAG_SIZE] = {
		0xA8, 0x06, 0x1D, 0xC1, 0x30, 0x51, 0x36, 0xC6,
		0xC2, 0x2B, 0x8B, 0xAF, 0x0C, 0x01, 0x27, 0xA9
	};
	static const char Message[] = "Cryptographic Forum Research Group";
	uint8 StreamTag[XRT_POLY1305_TAG_SIZE];
	uint8 OneShotTag[XRT_POLY1305_TAG_SIZE];
	xpoly1305 State;
	bool bValid = false;

	/* Final 在状态快照上执行，流状态仍可由调用方继续使用。 */
	if ( xrtPoly1305Init(&State, Key) &&
		 xrtPoly1305Update(&State, Message, 13u) &&
		 xrtPoly1305Update(
			&State, Message + 13u, (sizeof(Message) - 1u) - 13u
		 ) &&
		 xrtPoly1305Final(&State, StreamTag) &&
		 xrtPoly1305(Key, Message, sizeof(Message) - 1u, OneShotTag) ) {
		bValid = xrtConstTimeEqual(
			StreamTag, Expected, sizeof(StreamTag)
		) && xrtConstTimeEqual(
			OneShotTag, Expected, sizeof(OneShotTag)
		);
	}
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(StreamTag, sizeof(StreamTag));
	xrtSecureZero(OneShotTag, sizeof(OneShotTag));

	printf("Poly1305 vector: %s\n", bValid ? "valid" : "invalid");
	return bValid ? 0 : 1;
}
