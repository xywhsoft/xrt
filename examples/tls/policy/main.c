#include <xrt/tls.h>

#include <stdio.h>



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
