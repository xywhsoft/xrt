#include <stdio.h>
#include <xrt.h>



/*
 * 范例：crypto/ed25519 —— Ed25519 密钥对生成与一致性
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtEd25519KeyPair   随机种子 + 公钥一次生成
 *   xrtEd25519Public    由种子复算公钥（一致性自检）
 * 模块宏：XRT_MODULE_CRYPTO（ED25519 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/ed25519/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   consistent: yes
 *
 * 种子（32 字节）即私钥全部秘密：备份只需存种子，
 *   公钥随时可复算。比较用 ConstTimeEqual、
 *   用完 SecureZero 种子——密码学纪律贯穿。
 */


/* 生成密钥，并复算公钥检查密钥对一致性。 */
int main(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 Derived[XRT_ED25519_PUBLIC_SIZE];
	bool bEqual;

	if ( !xrtEd25519KeyPair(Seed, Public) ||
		 !xrtEd25519Public(Seed, Derived) ) {
		return 1;
	}
	bEqual = xrtConstTimeEqual(Public, Derived, sizeof(Public));
	xrtSecureZero(Seed, sizeof(Seed));
	printf("consistent: %s\n", bEqual ? "yes" : "no");
	return bEqual ? 0 : 1;
}
