#include <xrt.h>

#include <stdio.h>



/*
 * 范例：x509/store_system —— 系统信任库一键装载
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509StoreCreateSystem   平台证书存储 → XRT 信任库快照
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/store_system/main.c -lws2_32 -liphlpapi
 * 预期输出（锚数随系统根证书数量变化）：
 *   system anchors=33
 *
 * 底层来源：Windows 证书存储 / macOS 钥匙串 / Linux 常见路径
 *   （/etc/ssl/certs 等）。HTTPS 客户端的默认信任基础——
 *   一次装载即得到独立的库快照（系统后续变更不影响已建库），
 *   PathBuild/验证全走同一接口，应用不感知平台差异。
 */


/* 创建当前平台的独立系统信任库快照。 */
int main(void)
{
	xx509store* pStore = xrtX509StoreSystem();

	if ( pStore == NULL ) {
		return 1;
	}
	printf("system anchors=%llu\n",
		(unsigned long long)xrtX509StoreCount(pStore));
	xrtX509StoreFree(pStore);
	return 0;
}
