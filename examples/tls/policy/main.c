#include <xrt/tls.h>

#include <stdio.h>



/*
 * 范例：tls/policy —— 策略对象：允许什么版本/套件/组的单一事实源
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsPolicyInit    默认策略（现役安全基线）
 *   Policy.KeySharePolicy = XTLS_KEY_SHARE_PREFER_GROUP
 *                        本地 key-share 选择模式覆盖
 *   xrtTlsPolicyValid   策略自洽校验（空集/矛盾直接拒绝）
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/policy/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   versions=2 ciphers=9 groups=4 signatures=14
 *
 * 默认基线（输出即证据）：2 个版本（1.2/1.3）、9 个套件、
 *   4 个命名组、14 个签名方案——全部现役、无弱算法。
 *   收紧（如只要 1.3）就改数组；策略一处定义，
 *   Context/客户端/服务端全部引用它。
 */


/* 展示默认策略和调用方覆盖本地 key-share 选择模式。 */
int main(void)
{
	xtlspolicy Policy;

	xrtTlsPolicyInit(&Policy);
	Policy.KeySharePolicy = XTLS_KEY_SHARE_PREFER_GROUP;
	if ( !xrtTlsPolicyValid(&Policy) ) {
		return 1;
	}
	printf(
		"versions=%zu ciphers=%zu groups=%zu signatures=%zu\n",
		Policy.VersionCount, Policy.CipherCount,
		Policy.GroupCount, Policy.SignatureCount
	);
	return 0;
}
