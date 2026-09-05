/*
 * 范例：math/random_secure_text —— URL 安全令牌：安全随机 + 友好字母表
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSecureString  n 字符密码安全随机串（URL-safe 64 字符表）
 * 模块宏：XRT_MODULE_RANDOM
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/math/random_secure_text/main.c -lws2_32 -liphlpapi
 * 预期输出（每次不同；32 字符，仅 A-Za-z0-9_- ）：
 *   token: 8lNf4T4ncbN4Um_d1OU4RkVaW3aaH3h_
 *
 * 为什么不用十六进制：同等熵值长度减半——
 *   64 字符表每字符 log2(64)=6 位，32 字符 = 192 位熵；
 *   十六进制每字符 4 位，同样长度只有 128 位。
 * URL-safe 字母表（含 _ - 不含 + /）：可直接进 URL、Cookie、
 *   HTTP 头，不需要任何二次编码。
 * 会话令牌/CSRF token/重置码的标准生成方式。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	str sToken = xrtSecureString(32);   /* 32 字符 ≈ 192 位熵 */

	if ( sToken == NULL ) {
		return 1;    /* 熵源失败必须处理 */
	}
	printf("token: %s\n", sToken);

	/* 令牌等同凭据：清零（33 = 32 字符 + 结尾零）再释放。 */
	xrtSecureZero(sToken, 33);
	xrtFree(sToken);
	return 0;
}
