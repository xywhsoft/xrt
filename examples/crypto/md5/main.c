#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/md5 —— MD5（遗留互操作专用）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMd5   一次性摘要（16 字节）
 * 模块宏：XRT_MODULE_CRYPTO（MD5 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/md5/main.c -lws2_32 -liphlpapi
 * 预期输出（"abc" 标准摘要）：
 *   900150983cd24fb0d6963f7d28e17f72
 *
 * 破产算法：可任意碰撞，绝不能用于签名/完整性判断；
 *   仅在旧系统数据迁移、非安全缓存键等遗留场景使用。
 *   新代码的默认档位是 SHA-256。
 */


/* 为必须兼容 MD5 的历史协议计算摘要。 */
int main(void)
{
	static const char Hex[] = "0123456789abcdef";
	uint8 Digest[XRT_MD5_SIZE];

	if ( !xrtMd5("abc", 3u, Digest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(Digest); i++ ) {
		putchar(Hex[Digest[i] >> 4u]);
		putchar(Hex[Digest[i] & 0x0Fu]);
	}
	putchar('\n');
	return 0;
}
